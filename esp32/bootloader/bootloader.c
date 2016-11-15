// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "bootloader.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"

#include "rom/cache.h"
#include "rom/ets_sys.h"
#include "rom/spi_flash.h"
#include "rom/crc.h"
#include "rom/rtc.h"
#include "rom/md5_hash.h"

#include "soc/soc.h"
#include "soc/cpu.h"
#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/timer_group_reg.h"

#include "sdkconfig.h"
#include "mpconfigboard.h"
#include "bootmgr.h"
#include "mperror.h"

extern int _bss_start;
extern int _bss_end;

static const char* TAG = "boot";

static const uint8_t empty_signature[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/*
We arrive here after the bootloader finished loading the program from flash. The hardware is mostly uninitialized,
flash cache is down and the app CPU is in reset. We do have a stack, so we can do the initialization in C.
*/

// TODO: make a nice header file for ROM functions instead of adding externs all over the place
extern void Cache_Flush(int);

extern __attribute__((noreturn)) void mperror_fatal_error (void);

void bootloader_main();
void unpack_load_app(const partition_pos_t *app_node);
void print_flash_info(struct flash_hdr* pfhdr);
void IRAM_ATTR set_cache_and_start_app(uint32_t drom_addr,
    uint32_t drom_load_addr,
    uint32_t drom_size,
    uint32_t irom_addr,
    uint32_t irom_load_addr,
    uint32_t irom_size,
    uint32_t entry_addr);


void IRAM_ATTR call_start_cpu0()
{
    cpu_configure_region_protection();

    //Clear bss
    memset(&_bss_start, 0, (&_bss_end - &_bss_start) * sizeof(_bss_start));

    /* completely reset MMU for both CPUs
       (in case serial bootloader was running) */
    Cache_Read_Disable(0);
    Cache_Read_Disable(1);
    Cache_Flush(0);
    Cache_Flush(1);
    mmu_init(0);
    REG_SET_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MMU_IA_CLR);
    mmu_init(1);
    REG_CLR_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MMU_IA_CLR);
    /* (above steps probably unnecessary for most serial bootloader
       usage, all that's absolutely needed is that we unmask DROM0
       cache on the following two lines - normal ROM boot exits with
       DROM0 cache unmasked, but serial bootloader exits with it
       masked. However can't hurt to be thorough and reset
       everything.)

       The lines which manipulate DPORT_APP_CACHE_MMU_IA_CLR bit are
       necessary to work around a hardware bug.
    */
    REG_CLR_BIT(DPORT_PRO_CACHE_CTRL1_REG, DPORT_PRO_CACHE_MASK_DROM0);
    REG_CLR_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MASK_DROM0);

    // workaround: configure the SPI flash size manually (2nd argument)
    SPIParamCfg(0x1540ef, MICROPY_HW_FLASH_SIZE, 64 * 1024, 4096, 256, 0xffff);

    bootloader_main();
}

/** 
 *  @function :     boot_cache_redirect
 *  @description:   Configure several pages in flash map so that `size` bytes 
 *                  starting at `pos` are mapped to 0x3f400000.
 *                  This sets up mapping only for PRO CPU.
 *
 *  @inputs:        pos     address in flash
 *                  size    size of the area to map, in bytes
 */
void boot_cache_redirect( uint32_t pos, size_t size )
{
    uint32_t pos_aligned = pos & 0xffff0000;
    uint32_t count = (size + 0xffff) / 0x10000;
    Cache_Read_Disable( 0 );
    Cache_Flush( 0 );
    ESP_LOGD(TAG, "mmu set paddr=%08x count=%d", pos_aligned, count );
    cache_flash_mmu_set( 0, 0, 0x3f400000, pos_aligned, 64, count );
    Cache_Read_Enable( 0 );
}

/**
 *  @function :     load_partition_table
 *  @description:   Parse partition table, get useful data such as location of 
 *                  OTA info sector, factory app sector, and test app sector.
 *
 *  @inputs:        bs     bootloader state structure used to save the data
 *                  addr   address of partition table in flash
 *  @return:        return true, if the partition table is loaded (and MD5 checksum is valid)
 *
 */
bool load_partition_table(bootloader_state_t* bs, uint32_t addr)
{
    partition_info_t partition;
    uint32_t end = addr + 0x1000;
    int index = 0;
    char *partition_usage;

    ESP_LOGI(TAG, "Partition Table:");
    ESP_LOGI(TAG, "## Label            Usage          Type ST Offset   Length");

    while (addr < end) {
        ESP_LOGD(TAG, "load partition table entry from %x(%08x)", addr, (uint32_t)MEM_CACHE(addr));
        memcpy(&partition, MEM_CACHE(addr), sizeof(partition));
        ESP_LOGD(TAG, "type=%x subtype=%x", partition.type, partition.subtype);
        partition_usage = "unknown";

        if (partition.magic == PARTITION_MAGIC) { /* valid partition definition */
            switch(partition.type) {
            case PART_TYPE_APP: /* app partition */
                switch(partition.subtype) {
                case PART_SUBTYPE_FACTORY: /* factory binary */
                    bs->image[0] = partition.pos;
                    partition_usage = "factory app";
                    bs->image_count = 1;
                    break;
                default:
                    /* OTA binary */
                    if ((partition.subtype & ~PART_SUBTYPE_OTA_MASK) == PART_SUBTYPE_OTA_FLAG) {
                        bs->image[bs->image_count++] = partition.pos;
                        partition_usage = "OTA app";
                    } else {
                        partition_usage = "Unknown app";
                    }
                    break;
                }
                break; /* PART_TYPE_APP */
            case PART_TYPE_DATA: /* data partition */
                switch(partition.subtype) {
                case PART_SUBTYPE_DATA_OTA: /* ota data */
                    bs->ota_info = partition.pos;
                    partition_usage = "OTA data";
                    break;
                case PART_SUBTYPE_DATA_RF:
                    partition_usage = "RF data";
                    break;
                case PART_SUBTYPE_DATA_WIFI:
                    partition_usage = "WiFi data";
                    break;
                default:
                    partition_usage = "Unknown data";
                    break;
                }
                break; /* PARTITION_USAGE_DATA */
            default: /* other partition type */
                break;
            }
        }
        /* invalid partition magic number */
        else {
            break; /* todo: validate md5 */
        }

        /* print partition type info */
        ESP_LOGI(TAG, "%2d %-16s %-16s %02x %02x %08x %08x", index, partition.label, partition_usage,
                 partition.type, partition.subtype,
                 partition.pos.offset, partition.pos.size);
        index++;
        addr += sizeof(partition);
    }

    ESP_LOGI(TAG,"End of partition table");
    return true;
}

static uint32_t ota_select_crc(const boot_info_t *s)
{
  return crc32_le(UINT32_MAX, (uint8_t*)&s->ActiveImg, sizeof(boot_info_t) - sizeof(s->crc));
}

static bool ota_select_valid(const boot_info_t *s)
{
    uint32_t _crc = ota_select_crc(s);
    ESP_LOGI(TAG, "Cal crc=%x, saved crc=%x", _crc, s->crc);
    return s->Status != UINT32_MAX && s->crc == _crc;
}

void md5_to_ascii(unsigned char *md5, unsigned char *hex) {
    #define nibble2ascii(x) ((x) < 10 ? (x) + '0' : (x) - 10 + 'a')

    for (int i = 0; i < 16; i++) {
        hex[(i * 2)] = nibble2ascii(md5[i] >> 4);
        hex[(i * 2) + 1] = nibble2ascii(md5[i] & 0xF);
    }
}

// must be 32-bit aligned
static uint32_t bootloader_buf[1024];

static IRAM_ATTR void calculate_signature (uint8_t *signature) {
    uint32_t total_len = 0;
    uint8_t mac[6];
    system_efuse_read_mac(mac);

    struct MD5Context md5_context;

    ESP_LOGI(TAG, "Starting signature calculation");

    MD5Init(&md5_context);
    ESP_LOGI(TAG, "md5 init sig");
    while (total_len < 0x3000) {
        Cache_Read_Disable(0);
        if (SPI_FLASH_RESULT_OK != SPIRead(0x3000 + total_len, (void *)bootloader_buf, SPI_SEC_SIZE)) {
            ESP_LOGE(TAG, SPI_ERROR_LOG);
            Cache_Read_Enable(0);
            return;
        }
        Cache_Read_Enable(0);
        total_len += SPI_SEC_SIZE;
        MD5Update(&md5_context, (void *)bootloader_buf, SPI_SEC_SIZE);
    }
    // add the mac address
    MD5Update(&md5_context, (void *)mac, sizeof(mac));
    MD5Final(signature, &md5_context);
}

static IRAM_ATTR bool bootloader_verify (const partition_pos_t *pos, uint32_t size) {
    uint32_t total_len = 0, read_len;
    uint8_t hash[16];
    uint8_t hash_hex[33];
    struct MD5Context md5_context;

    ESP_LOGI(TAG, "Starting image verification %x %d", pos->offset, size);

    size -= 32;

    MD5Init(&md5_context);
    ESP_LOGI(TAG, "md5 init");
    while (total_len < size) {
        read_len = (size - total_len) > SPI_SEC_SIZE ? SPI_SEC_SIZE : (size - total_len);
        Cache_Read_Disable(0);
        if (SPI_FLASH_RESULT_OK != SPIRead(pos->offset + total_len, (void *)bootloader_buf, SPI_SEC_SIZE)) {
            ESP_LOGE(TAG, SPI_ERROR_LOG);
            Cache_Read_Enable(0);
            return false;
        }
        Cache_Read_Enable(0);
        total_len += read_len;
        MD5Update(&md5_context, (void *)bootloader_buf, read_len);
    }
    ESP_LOGI(TAG, "Reading done total len=%d", total_len);
    MD5Final(hash, &md5_context);

    ESP_LOGI(TAG, "Hash calculated");
    md5_to_ascii(hash, hash_hex);
    ESP_LOGI(TAG, "Converted to hex");

    Cache_Read_Disable(0);
    if (SPI_FLASH_RESULT_OK != SPIRead(pos->offset + total_len, (void *)bootloader_buf, SPI_SEC_SIZE)) {
        ESP_LOGE(TAG, SPI_ERROR_LOG);
        Cache_Read_Enable(0);
        return false;
    }
    Cache_Read_Enable(0);

    hash_hex[32] = '\0';
    // this one is uint32_t type, remember?
    bootloader_buf[32 / sizeof(uint32_t)] = '\0';
    // compare both hashes
    if (!strcmp((const char *)hash_hex, (const char *)bootloader_buf)) {
        ESP_LOGI(TAG, "MD5 hash OK! :-)");
        // it's a match
        return true;
    }

    ESP_LOGI(TAG, "MD5 hash failed %s : %s", hash_hex, bootloader_buf);
    return false;
}

static IRAM_ATTR bool ota_write_boot_info (boot_info_t *boot_info, uint32_t offset) {
    boot_info->crc = ota_select_crc(boot_info);
    Cache_Read_Disable(0);
    if (SPI_FLASH_RESULT_OK != SPIEraseSector(offset / 0x1000)) {
        ESP_LOGE(TAG, SPI_ERROR_LOG);
        return false;
    }

    if (SPI_FLASH_RESULT_OK != SPIWrite(offset, (void *)boot_info, sizeof(boot_info_t))) {
        ESP_LOGE(TAG, SPI_ERROR_LOG);
        return false;
    }
    Cache_Read_Enable(0);
    return true;
}

/**
 *  @function :     bootloader_main
 *  @description:   entry function of 2nd bootloader
 *
 *  @inputs:        void
 */

void bootloader_main()
{
    ESP_LOGI(TAG, "Espressif ESP32 2nd stage bootloader v. %s", BOOT_VERSION);

    struct flash_hdr fhdr;
    bootloader_state_t bs;
    memset(&bs, 0, sizeof(bs));

    ESP_LOGI(TAG, "compile time " __TIME__ );
    /* close watch dog here */
    REG_CLR_BIT( RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_FLASHBOOT_MOD_EN );
    REG_CLR_BIT( TIMG_WDTCONFIG0_REG(0), TIMG_WDT_FLASHBOOT_MOD_EN );
    SPIUnlock();
    /*register first sector in drom0 page 0 */
    boot_cache_redirect( 0, 0x5000 );

    memcpy((unsigned int *) &fhdr, MEM_CACHE(0x1000), sizeof(struct flash_hdr) );

    print_flash_info(&fhdr);

    if (!load_partition_table(&bs, PARTITION_ADD)) {
        ESP_LOGE(TAG, "load partition table error!");
        return;
    }

    partition_pos_t load_part_pos;

    boot_info_t boot_info;

    if (bs.ota_info.offset != 0) {              // check if partition table has OTA info partition
        boot_cache_redirect(bs.ota_info.offset, bs.ota_info.size);
        memcpy(&boot_info, MEM_CACHE(bs.ota_info.offset & 0x0000ffff), sizeof(boot_info_t));

        if (!ota_select_valid(&boot_info)) {
            ESP_LOGI(TAG, "Initializing OTA partition info");
            // init status flash
            load_part_pos = bs.image[0];
            boot_info.ActiveImg = IMG_ACT_FACTORY;
            boot_info.Status = IMG_STATUS_READY;
            boot_info.PrevImg = IMG_ACT_FACTORY;
            boot_info.safeboot = false;
            if (!ota_write_boot_info (&boot_info, bs.ota_info.offset)) {
                ESP_LOGE(TAG, "Error writing boot info");
                mperror_fatal_error();
                return;
            }
        } else {
            // CRC is fine, check here the image that we need to load based on the status (ready or check)
            // if image is in status check then we must verify the MD5, and set the new status
            // if the MD5 fails, then we roll back to the previous image

            // do we have a new image that needs to be verified?
            if ((boot_info.ActiveImg != IMG_ACT_FACTORY) && (boot_info.Status == IMG_STATUS_CHECK)) {
                if (!bootloader_verify(&bs.image[boot_info.ActiveImg], boot_info.size)) {
                    // switch to the previous image
                    boot_info.ActiveImg = boot_info.PrevImg;
                    boot_info.PrevImg = IMG_ACT_FACTORY;
                }
                // in any case, change the status to "READY"
                boot_info.Status = IMG_STATUS_READY;
                // write the new boot info
                if (!ota_write_boot_info (&boot_info, bs.ota_info.offset)) {
                    ESP_LOGE(TAG, "Error writing boot info");
                    mperror_fatal_error();
                    return;
                }
            }

            // this one might modify the boot info hence it MUST be called after
            // bootmgr_verify! (so that the changes are not saved to flash)
            ets_update_cpu_frequency(40);
            mperror_init0();
            ESP_LOGI(TAG, "Checking safe boot pin");
            uint32_t ActiveImg = boot_info.ActiveImg;
            uint32_t safeboot = wait_for_safe_boot (&boot_info, &ActiveImg);
            if (safeboot) {
                ESP_LOGI(TAG, "Safe boot requested!");
            }
            if (safeboot != boot_info.safeboot) {
                boot_info.safeboot = safeboot;
                // write the new boot info
                if (!ota_write_boot_info (&boot_info, bs.ota_info.offset)) {
                    ESP_LOGE(TAG, "Error writing boot info");
                    mperror_fatal_error();
                    return;
                }
            }

            // load the selected active image
            load_part_pos = bs.image[ActiveImg];
        }
    } else if (bs.image[0].offset != 0) {       // otherwise, look for factory app partition
        load_part_pos = bs.image[0];
    } else {                                    // nothing to load, bail out
        ESP_LOGE(TAG, "nothing to load");
        mperror_fatal_error();
        return;
    }

    // check the signature
    uint8_t signature[16];
    calculate_signature(signature);
    if (!memcmp(boot_info.signature, empty_signature, sizeof(boot_info.signature))) {
        ESP_LOGI(TAG, "Writing the signature");
        // write the signature
        memcpy(boot_info.signature, signature, sizeof(boot_info.signature));
        if (!ota_write_boot_info (&boot_info, bs.ota_info.offset)) {
            ESP_LOGE(TAG, "Error writing boot info");
            mperror_fatal_error();
            return;
        }
    } else {
        ESP_LOGI(TAG, "Comparing the signature");
        // compare the signatures
        if (memcmp(boot_info.signature, signature, sizeof(boot_info.signature))) {
            // signature check failed, don't load the app!
            mperror_fatal_error();
            return;
        }
    }

    ESP_LOGI(TAG, "Loading app partition at offset %08x", load_part_pos.offset);

    // copy sections to RAM, set up caches, and start application
    unpack_load_app(&load_part_pos);
}


void unpack_load_app(const partition_pos_t* partition)
{
    boot_cache_redirect(partition->offset, partition->size);

    uint32_t pos = 0;
    struct flash_hdr image_header;
    memcpy(&image_header, MEM_CACHE(pos), sizeof(image_header));
    pos += sizeof(image_header);

    uint32_t drom_addr = 0;
    uint32_t drom_load_addr = 0;
    uint32_t drom_size = 0;
    uint32_t irom_addr = 0;
    uint32_t irom_load_addr = 0;
    uint32_t irom_size = 0;

    /* Reload the RTC memory sections whenever a non-deepsleep reset
       is occuring */
    bool load_rtc_memory = rtc_get_reset_reason(0) != DEEPSLEEP_RESET;

    ESP_LOGD(TAG, "bin_header: %u %u %u %u %08x", image_header.magic,
             image_header.blocks,
             image_header.spi_mode,
             image_header.spi_size,
             (unsigned)image_header.entry_addr);

    for (uint32_t section_index = 0;
            section_index < image_header.blocks;
            ++section_index) {
        struct block_hdr section_header = {0};
        memcpy(&section_header, MEM_CACHE(pos), sizeof(section_header));
        pos += sizeof(section_header);

        const uint32_t address = section_header.load_addr;
        bool load = true;
        bool map = false;
        if (address == 0x00000000) {        // padding, ignore block
            load = false;
        }
        if (address == 0x00000004) {
            load = false;                   // md5 checksum block
            // TODO: actually check md5
        }

        if (address >= DROM_LOW && address < DROM_HIGH) {
            ESP_LOGD(TAG, "found drom section, map from %08x to %08x", pos,
                      section_header.load_addr);
            drom_addr = partition->offset + pos - sizeof(section_header);
            drom_load_addr = section_header.load_addr;
            drom_size = section_header.data_len + sizeof(section_header);
            load = false;
            map = true;
        }

        if (address >= IROM_LOW && address < IROM_HIGH) {
            ESP_LOGD(TAG, "found irom section, map from %08x to %08x", pos,
                      section_header.load_addr);
            irom_addr = partition->offset + pos - sizeof(section_header);
            irom_load_addr = section_header.load_addr;
            irom_size = section_header.data_len + sizeof(section_header);
            load = false;
            map = true;
        }

		if(!load_rtc_memory && address >= RTC_IRAM_LOW && address < RTC_IRAM_HIGH) {
			ESP_LOGD(TAG, "Skipping RTC code section at %08x\n", pos);
			load = false;
		}

		if(!load_rtc_memory && address >= RTC_DATA_LOW && address < RTC_DATA_HIGH) {
			ESP_LOGD(TAG, "Skipping RTC data section at %08x\n", pos);
			load = false;
		}

        ESP_LOGI(TAG, "section %d: paddr=0x%08x vaddr=0x%08x size=0x%05x (%6d) %s", section_index, pos,
                 section_header.load_addr, section_header.data_len, section_header.data_len, (load)?"load":(map)?"map":"");

        if (!load) {
            pos += section_header.data_len;
            continue;
        }

        ESP_LOGI(TAG, "Copying section from=%d, to=%d, len=%d\n", (uint32_t)MEM_CACHE(pos), section_header.load_addr, section_header.data_len);

        memcpy((void*) section_header.load_addr, MEM_CACHE(pos), section_header.data_len);
        pos += section_header.data_len;
    }

    ESP_LOGI(TAG, "about to configure cache and start app\n");

    set_cache_and_start_app(drom_addr,
        drom_load_addr,
        drom_size,
        irom_addr,
        irom_load_addr,
        irom_size,
        image_header.entry_addr);
}

void IRAM_ATTR set_cache_and_start_app(
    uint32_t drom_addr,
    uint32_t drom_load_addr,
    uint32_t drom_size,
    uint32_t irom_addr,
    uint32_t irom_load_addr,
    uint32_t irom_size, 
    uint32_t entry_addr)
{
    ESP_LOGD(TAG, "configure drom and irom and start");
    Cache_Read_Disable( 0 );
    Cache_Read_Disable( 1 );
    Cache_Flush( 0 );
    Cache_Flush( 1 );
    uint32_t drom_page_count = (drom_size + 64*1024 - 1) / (64*1024); // round up to 64k
    ESP_LOGV(TAG, "d mmu set paddr=%08x vaddr=%08x size=%d n=%d", drom_addr & 0xffff0000, drom_load_addr & 0xffff0000, drom_size, drom_page_count );
    int rc = cache_flash_mmu_set( 0, 0, drom_load_addr & 0xffff0000, drom_addr & 0xffff0000, 64, drom_page_count );
    ESP_LOGV(TAG, "rc=%d", rc );
    rc = cache_flash_mmu_set( 1, 0, drom_load_addr & 0xffff0000, drom_addr & 0xffff0000, 64, drom_page_count );
    ESP_LOGV(TAG, "rc=%d", rc );
    uint32_t irom_page_count = (irom_size + 64*1024 - 1) / (64*1024); // round up to 64k
    ESP_LOGV(TAG, "i mmu set paddr=%08x vaddr=%08x size=%d n=%d", irom_addr & 0xffff0000, irom_load_addr & 0xffff0000, irom_size, irom_page_count );
    rc = cache_flash_mmu_set( 0, 0, irom_load_addr & 0xffff0000, irom_addr & 0xffff0000, 64, irom_page_count );
    ESP_LOGV(TAG, "rc=%d", rc );
    rc = cache_flash_mmu_set( 1, 0, irom_load_addr & 0xffff0000, irom_addr & 0xffff0000, 64, irom_page_count );
    ESP_LOGV(TAG, "rc=%d", rc );
    REG_CLR_BIT( DPORT_PRO_CACHE_CTRL1_REG, (DPORT_PRO_CACHE_MASK_IRAM0) | (DPORT_PRO_CACHE_MASK_IRAM1 & 0) | (DPORT_PRO_CACHE_MASK_IROM0 & 0) | DPORT_PRO_CACHE_MASK_DROM0 | DPORT_PRO_CACHE_MASK_DRAM1 );
    REG_CLR_BIT( DPORT_APP_CACHE_CTRL1_REG, (DPORT_APP_CACHE_MASK_IRAM0) | (DPORT_APP_CACHE_MASK_IRAM1 & 0) | (DPORT_APP_CACHE_MASK_IROM0 & 0) | DPORT_APP_CACHE_MASK_DROM0 | DPORT_APP_CACHE_MASK_DRAM1 );
    Cache_Read_Enable( 0 );
    Cache_Read_Enable( 1 );

    ESP_LOGD(TAG, "start: 0x%08x", entry_addr);
    typedef void (*entry_t)(void);
    entry_t entry = ((entry_t) entry_addr);

    // TODO: we have used quite a bit of stack at this point.
    // use "movsp" instruction to reset stack back to where ROM stack starts.
    (*entry)();
}


void print_flash_info(struct flash_hdr* pfhdr)
{
#if (BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_NOTICE)

    struct flash_hdr fhdr = *pfhdr;

    ESP_LOGD(TAG, "magic %02x", fhdr.magic );
    ESP_LOGD(TAG, "blocks %02x", fhdr.blocks );
    ESP_LOGD(TAG, "spi_mode %02x", fhdr.spi_mode );
    ESP_LOGD(TAG, "spi_speed %02x", fhdr.spi_speed );
    ESP_LOGD(TAG, "spi_size %02x", fhdr.spi_size );

    const char* str;
    switch ( fhdr.spi_speed ) {
    case SPI_SPEED_40M:
        str = "40MHz";
        break;

    case SPI_SPEED_26M:
        str = "26.7MHz";
        break;

    case SPI_SPEED_20M:
        str = "20MHz";
        break;

    case SPI_SPEED_80M:
        str = "80MHz";
        break;

    default:
        str = "20MHz";
        break;
    }
    ESP_LOGI(TAG, "SPI Speed      : %s", str );

    

    switch ( fhdr.spi_mode ) {
    case SPI_MODE_QIO:
        str = "QIO";
        break;

    case SPI_MODE_QOUT:
        str = "QOUT";
        break;

    case SPI_MODE_DIO:
        str = "DIO";
        break;

    case SPI_MODE_DOUT:
        str = "DOUT";
        break;

    case SPI_MODE_FAST_READ:
        str = "FAST READ";
        break;

    case SPI_MODE_SLOW_READ:
        str = "SLOW READ";
        break;
    default:
        str = "DIO";
        break;
    }
    ESP_LOGI(TAG, "SPI Mode       : %s", str );

    

    switch ( fhdr.spi_size ) {
    case SPI_SIZE_1MB:
        str = "1MB";
        break;

    case SPI_SIZE_2MB:
        str = "2MB";
        break;

    case SPI_SIZE_4MB:
        str = "4MB";
        break;

    case SPI_SIZE_8MB:
        str = "8MB";
        break;

    case SPI_SIZE_16MB:
        str = "16MB";
        break;

    default:
        str = "1MB";
        break;
    }
    ESP_LOGI(TAG, "SPI Flash Size : %s", str );
#endif
}

int ets_printf_dummy(const char *fmt, ...) {
    return 0;
}

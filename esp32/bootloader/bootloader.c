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
#include "esp_image_format.h"
#include "bootloader_flash.h"
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

static void bootloader_main();
static void unpack_load_app(const esp_partition_pos_t *app_node);
static void print_flash_info(const esp_image_header_t* pfhdr);
static void set_cache_and_start_app(uint32_t drom_addr,
    uint32_t drom_load_addr,
    uint32_t drom_size,
    uint32_t irom_addr,
    uint32_t irom_load_addr,
    uint32_t irom_size,
    uint32_t entry_addr);
static void update_flash_config(const esp_image_header_t* pfhdr);

static void read_mac(uint8_t* mac)
{
    uint32_t mac_low = REG_READ(EFUSE_BLK0_RDATA1_REG);
    uint32_t mac_high = REG_READ(EFUSE_BLK0_RDATA2_REG);

    mac[0] = mac_high >> 8;
    mac[1] = mac_high;
    mac[2] = mac_low >> 24;
    mac[3] = mac_low >> 16;
    mac[4] = mac_low >> 8;
    mac[5] = mac_low;
}

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

    bootloader_main();
}

/**
 *  @function :     load_partition_table
 *  @description:   Parse partition table, get useful data such as location of
 *                  OTA info sector, factory app sector, and test app sector.
 *
 *  @inputs:        bs     bootloader state structure used to save the data
 *  @return:        return true, if the partition table is loaded (and MD5 checksum is valid)
 *
 */
bool load_partition_table(bootloader_state_t* bs)
{
    const esp_partition_info_t *partitions;
    const int ESP_PARTITION_TABLE_DATA_LEN = 0xC00; /* length of actual data (signature is appended to this) */
    const int MAX_PARTITIONS = ESP_PARTITION_TABLE_DATA_LEN / sizeof(esp_partition_info_t);
    char *partition_usage;

    ESP_LOGI(TAG, "Partition Table:");
    ESP_LOGI(TAG, "## Label            Usage          Type ST Offset   Length");

    partitions = bootloader_mmap(ESP_PARTITION_TABLE_ADDR, ESP_PARTITION_TABLE_DATA_LEN);
    if (!partitions) {
            ESP_LOGE(TAG, "bootloader_mmap(0x%x, 0x%x) failed", ESP_PARTITION_TABLE_ADDR, ESP_PARTITION_TABLE_DATA_LEN);
            return false;
    }
    ESP_LOGD(TAG, "mapped partition table 0x%x at 0x%x", ESP_PARTITION_TABLE_ADDR, (intptr_t)partitions);

    for(int i = 0; i < MAX_PARTITIONS; i++) {
        const esp_partition_info_t *partition = &partitions[i];
        ESP_LOGD(TAG, "load partition table entry 0x%x", (intptr_t)partition);
        ESP_LOGD(TAG, "type=%x subtype=%x", partition->type, partition->subtype);
        partition_usage = "unknown";

        if (partition->magic == ESP_PARTITION_MAGIC) { /* valid partition definition */
            switch(partition->type) {
            case PART_TYPE_APP: /* app partition */
                switch(partition->subtype) {
                case PART_SUBTYPE_FACTORY: /* factory binary */
                    bs->image[0] = partition->pos;
                    partition_usage = "factory app";
                    bs->image_count = 1;
                    break;
                default:
                    /* OTA binary */
                    if ((partition->subtype & ~PART_SUBTYPE_OTA_MASK) == PART_SUBTYPE_OTA_FLAG) {
                        bs->image[bs->image_count++] = partition->pos;
                        partition_usage = "OTA app";
                    } else {
                        partition_usage = "Unknown app";
                    }
                    break;
                }
                break; /* PART_TYPE_APP */
            case PART_TYPE_DATA: /* data partition */
                switch(partition->subtype) {
                case PART_SUBTYPE_DATA_OTA: /* ota data */
                    bs->ota_info = partition->pos;
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
            break;
        }

        /* print partition type info */
        ESP_LOGI(TAG, "%2d %-16s %-16s %02x %02x %08x %08x", i, partition->label, partition_usage,
                 partition->type, partition->subtype,
                 partition->pos.offset, partition->pos.size);
    }

    bootloader_munmap(partitions);

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
    read_mac(mac);

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

static IRAM_ATTR bool bootloader_verify (const esp_partition_pos_t *pos, uint32_t size) {
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

static void bootloader_main()
{
    ESP_LOGI(TAG, "Espressif ESP32 2nd stage bootloader v. %s", BOOT_VERSION);

    esp_image_header_t fhdr;
    bootloader_state_t bs;
    memset(&bs, 0, sizeof(bs));

    ESP_LOGI(TAG, "compile time " __TIME__ );
    /* disable the watchdog here */
    REG_CLR_BIT( RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_FLASHBOOT_MOD_EN );
    REG_CLR_BIT( TIMG_WDTCONFIG0_REG(0), TIMG_WDT_FLASHBOOT_MOD_EN );
    SPIUnlock();

    ESP_LOGI(TAG, "loading bootloader header");
    if(esp_image_load_header(0x1000, true, &fhdr) != ESP_OK) {
        ESP_LOGE(TAG, "failed to load bootloader header!");
        return;
    }

    print_flash_info(&fhdr);

    update_flash_config(&fhdr);

    if (!load_partition_table(&bs)) {
        ESP_LOGE(TAG, "load partition table error!");
        return;
    }

    esp_partition_pos_t load_part_pos;

    boot_info_t *boot_info;
    boot_info_t _boot_info;

    if (bs.ota_info.offset != 0) {              // check if partition table has OTA info partition
        if (bs.ota_info.size < 2 * sizeof(esp_ota_select_entry_t)) {
            ESP_LOGE(TAG, "ERROR: ota_info partition size %d is too small (minimum %d bytes)", bs.ota_info.size, sizeof(esp_ota_select_entry_t));
            return;
        }
        ESP_LOGI(TAG, "Loading boot info");
        boot_info = (boot_info_t *)bootloader_mmap(bs.ota_info.offset, bs.ota_info.size);
        if (!boot_info) {
            ESP_LOGE(TAG, "bootloader_mmap(0x%x, 0x%x) failed", bs.ota_info.offset, bs.ota_info.size);
            return;
        }
        memcpy(&_boot_info, boot_info, sizeof(boot_info_t));
        bootloader_munmap(boot_info);
        boot_info = &_boot_info;
        if (!ota_select_valid(boot_info)) {
            ESP_LOGI(TAG, "Initializing OTA partition info");
            // init status flash
            load_part_pos = bs.image[0];
            boot_info->ActiveImg = IMG_ACT_FACTORY;
            boot_info->Status = IMG_STATUS_READY;
            boot_info->PrevImg = IMG_ACT_FACTORY;
            boot_info->safeboot = false;
            if (!ota_write_boot_info (boot_info, bs.ota_info.offset)) {
                ESP_LOGE(TAG, "Error writing boot info");
                mperror_fatal_error();
                return;
            }
        } else {
            // CRC is fine, check here the image that we need to load based on the status (ready or check)
            // if image is in status check then we must verify the MD5, and set the new status
            // if the MD5 fails, then we roll back to the previous image

            // do we have a new image that needs to be verified?
            if ((boot_info->ActiveImg != IMG_ACT_FACTORY) && (boot_info->Status == IMG_STATUS_CHECK)) {
                if (!bootloader_verify(&bs.image[boot_info->ActiveImg], boot_info->size)) {
                    // switch to the previous image
                    boot_info->ActiveImg = boot_info->PrevImg;
                    boot_info->PrevImg = IMG_ACT_FACTORY;
                }
                // in any case, change the status to "READY"
                boot_info->Status = IMG_STATUS_READY;
                // write the new boot info
                if (!ota_write_boot_info (boot_info, bs.ota_info.offset)) {
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
            uint32_t ActiveImg = boot_info->ActiveImg;
            uint32_t safeboot = wait_for_safe_boot (boot_info, &ActiveImg);
            if (safeboot) {
                ESP_LOGI(TAG, "Safe boot requested!");
            }
            if (safeboot != boot_info->safeboot) {
                boot_info->safeboot = safeboot;
                // write the new boot info
                if (!ota_write_boot_info (boot_info, bs.ota_info.offset)) {
                    ESP_LOGE(TAG, "Error writing boot info");
                    mperror_fatal_error();
                    return;
                }
            }

            // load the selected active image
            load_part_pos = bs.image[ActiveImg];
        }
    } else {                                    // nothing to load, bail out
        ESP_LOGE(TAG, "nothing to load");
        mperror_fatal_error();
        return;
    }

    // check the signature
    uint8_t signature[16];
    calculate_signature(signature);
    if (!memcmp(boot_info->signature, empty_signature, sizeof(boot_info->signature))) {
        ESP_LOGI(TAG, "Writing the signature");
        // write the signature
        memcpy(boot_info->signature, signature, sizeof(boot_info->signature));
        if (!ota_write_boot_info (boot_info, bs.ota_info.offset)) {
            ESP_LOGE(TAG, "Error writing boot info");
            mperror_fatal_error();
            return;
        }
    } else {
        ESP_LOGI(TAG, "Comparing the signature");
        // compare the signatures
        if (memcmp(boot_info->signature, signature, sizeof(boot_info->signature))) {
            // signature check failed, don't load the app!
            mperror_fatal_error();
            return;
        }
    }

    ESP_LOGI(TAG, "Loading app partition at offset %08x size %08x", load_part_pos.offset, load_part_pos.size);

    // copy sections to RAM, set up caches, and start application
    unpack_load_app(&load_part_pos);
}


static void unpack_load_app(const esp_partition_pos_t* partition)
{
    esp_err_t err;
    esp_image_header_t image_header;
    uint32_t image_length;

    /* TODO: verify the app image as part of OTA boot decision, so can have fallbacks */
    err = esp_image_basic_verify(partition->offset, true, &image_length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to verify app image @ 0x%x (%d)", partition->offset, err);
        return;
    }

    if (esp_image_load_header(partition->offset, true, &image_header) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load app image header @ 0x%x", partition->offset);
        return;
    }

    uint32_t drom_addr = 0;
    uint32_t drom_load_addr = 0;
    uint32_t drom_size = 0;
    uint32_t irom_addr = 0;
    uint32_t irom_load_addr = 0;
    uint32_t irom_size = 0;

    /* Reload the RTC memory segments whenever a non-deepsleep reset
       is occurring */
    bool load_rtc_memory = rtc_get_reset_reason(0) != DEEPSLEEP_RESET;

    ESP_LOGD(TAG, "bin_header: %u %u %u %u %08x", image_header.magic,
             image_header.segment_count,
             image_header.spi_mode,
             image_header.spi_size,
             (unsigned)image_header.entry_addr);

    for (int segment = 0; segment < image_header.segment_count; segment++) {
        esp_image_segment_header_t segment_header;
        uint32_t data_offs;
        if(esp_image_load_segment_header(segment, partition->offset,
                                         &image_header, true, &segment_header,
                                         &data_offs) != ESP_OK) {
            ESP_LOGE(TAG, "failed to load segment header #%d", segment);
            return;
        }

        const uint32_t address = segment_header.load_addr;
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
            ESP_LOGD(TAG, "found drom segment, map from %08x to %08x", data_offs,
                      segment_header.load_addr);
            drom_addr = data_offs;
            drom_load_addr = segment_header.load_addr;
            drom_size = segment_header.data_len + sizeof(segment_header);
            load = false;
            map = true;
        }

        if (address >= IROM_LOW && address < IROM_HIGH) {
            ESP_LOGD(TAG, "found irom segment, map from %08x to %08x", data_offs,
                      segment_header.load_addr);
            irom_addr = data_offs;
            irom_load_addr = segment_header.load_addr;
            irom_size = segment_header.data_len + sizeof(segment_header);
            load = false;
            map = true;
        }

        if (!load_rtc_memory && address >= RTC_IRAM_LOW && address < RTC_IRAM_HIGH) {
            ESP_LOGD(TAG, "Skipping RTC code segment at %08x\n", data_offs);
            load = false;
        }

        if (!load_rtc_memory && address >= RTC_DATA_LOW && address < RTC_DATA_HIGH) {
            ESP_LOGD(TAG, "Skipping RTC data segment at %08x\n", data_offs);
            load = false;
        }

        ESP_LOGI(TAG, "segment %d: paddr=0x%08x vaddr=0x%08x size=0x%05x (%6d) %s", segment, data_offs - sizeof(esp_image_segment_header_t),
                 segment_header.load_addr, segment_header.data_len, segment_header.data_len, (load)?"load":(map)?"map":"");

        if (load) {

            intptr_t sp, start_addr, end_addr;
            ESP_LOGV(TAG, "bootloader_mmap data_offs=%08x data_len=%08x", data_offs, segment_header.data_len);

            start_addr = segment_header.load_addr;
            end_addr = start_addr + segment_header.data_len;

            /* Before loading segment, check it doesn't clobber
               bootloader RAM... */

            if (end_addr < 0x40000000) {
                sp = (intptr_t)get_sp();
                if (end_addr > sp) {
                    ESP_LOGE(TAG, "Segment %d end address %08x overlaps bootloader stack %08x - can't load",
                         segment, end_addr, sp);
                    return;
                }
                if (end_addr > sp - 256) {
                    /* We don't know for sure this is the stack high water mark, so warn if
                       it seems like we may overflow.
                    */
                    ESP_LOGW(TAG, "Segment %d end address %08x close to stack pointer %08x",
                             segment, end_addr, sp);
                }
            }

            const void *data = bootloader_mmap(data_offs, segment_header.data_len);
            if(!data) {
                ESP_LOGE(TAG, "bootloader_mmap(0x%xc, 0x%x) failed",
                         data_offs, segment_header.data_len);
                return;
            }
            memcpy((void *)segment_header.load_addr, data, segment_header.data_len);
            bootloader_munmap(data);
        }
    }

    set_cache_and_start_app(drom_addr,
        drom_load_addr,
        drom_size,
        irom_addr,
        irom_load_addr,
        irom_size,
        image_header.entry_addr);
}

static void set_cache_and_start_app(
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
    Cache_Flush( 0 );
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

    // Application will need to do Cache_Flush(1) and Cache_Read_Enable(1)

    ESP_LOGD(TAG, "start: 0x%08x", entry_addr);
    typedef void (*entry_t)(void);
    entry_t entry = ((entry_t) entry_addr);

    // TODO: we have used quite a bit of stack at this point.
    // use "movsp" instruction to reset stack back to where ROM stack starts.
    (*entry)();
}


static void update_flash_config(const esp_image_header_t* pfhdr)
{
    uint32_t size;
    switch(pfhdr->spi_size) {
        case ESP_IMAGE_FLASH_SIZE_1MB:
            size = 1;
            break;
        case ESP_IMAGE_FLASH_SIZE_2MB:
            size = 2;
            break;
        case ESP_IMAGE_FLASH_SIZE_4MB:
            size = 4;
            break;
        case ESP_IMAGE_FLASH_SIZE_8MB:
            size = 8;
            break;
        case ESP_IMAGE_FLASH_SIZE_16MB:
            size = 16;
            break;
        default:
            size = 2;
    }
    ESP_LOGD(TAG, "Flash size=%d\n", size);
    Cache_Read_Disable( 0 );
    // Set flash chip size
    SPIParamCfg(g_rom_flashchip.deviceId, size * 0x100000, 0x10000, 0x1000, 0x100, 0xffff);
    // TODO: set mode
    // TODO: set frequency
    Cache_Flush(0);
    Cache_Read_Enable( 0 );
}

static void print_flash_info(const esp_image_header_t* phdr)
{
#if (BOOT_LOG_LEVEL >= BOOT_LOG_LEVEL_NOTICE)

    ESP_LOGD(TAG, "magic %02x", phdr->magic );
    ESP_LOGD(TAG, "segments %02x", phdr->segment_count );
    ESP_LOGD(TAG, "spi_mode %02x", phdr->spi_mode );
    ESP_LOGD(TAG, "spi_speed %02x", phdr->spi_speed );
    ESP_LOGD(TAG, "spi_size %02x", phdr->spi_size );

    const char* str;
    switch ( phdr->spi_speed ) {
    case ESP_IMAGE_SPI_SPEED_40M:
        str = "40MHz";
        break;
    case ESP_IMAGE_SPI_SPEED_26M:
        str = "26.7MHz";
        break;
    case ESP_IMAGE_SPI_SPEED_20M:
        str = "20MHz";
        break;
    case ESP_IMAGE_SPI_SPEED_80M:
        str = "80MHz";
        break;
    default:
        str = "20MHz";
        break;
    }
    ESP_LOGI(TAG, "SPI Speed      : %s", str );

    switch ( phdr->spi_mode ) {
    case ESP_IMAGE_SPI_MODE_QIO:
        str = "QIO";
        break;
    case ESP_IMAGE_SPI_MODE_QOUT:
        str = "QOUT";
        break;
    case ESP_IMAGE_SPI_MODE_DIO:
        str = "DIO";
        break;
    case ESP_IMAGE_SPI_MODE_DOUT:
        str = "DOUT";
        break;
    case ESP_IMAGE_SPI_MODE_FAST_READ:
        str = "FAST READ";
        break;
    case ESP_IMAGE_SPI_MODE_SLOW_READ:
        str = "SLOW READ";
        break;
    default:
        str = "DIO";
        break;
    }
    ESP_LOGI(TAG, "SPI Mode       : %s", str );

    switch ( phdr->spi_size ) {
    case ESP_IMAGE_FLASH_SIZE_1MB:
        str = "1MB";
        break;
    case ESP_IMAGE_FLASH_SIZE_2MB:
        str = "2MB";
        break;
    case ESP_IMAGE_FLASH_SIZE_4MB:
        str = "4MB";
        break;
    case ESP_IMAGE_FLASH_SIZE_8MB:
        str = "8MB";
        break;
    case ESP_IMAGE_FLASH_SIZE_16MB:
        str = "16MB";
        break;
    default:
        str = "2MB";
        break;
    }
    ESP_LOGI(TAG, "SPI Flash Size : %s", str );
#endif
}

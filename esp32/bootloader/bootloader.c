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
#include <sys/param.h>

#include "bootloader.h"
#include "esp_attr.h"
//#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_system.h"

#include "rom/cache.h"
#include "rom/efuse.h"
#include "rom/ets_sys.h"
#include "rom/spi_flash.h"
#include "rom/crc.h"
#include "rom/rtc.h"
#include "rom/md5_hash.h"
#include "rom/uart.h"
#include "rom/gpio.h"

#include "soc/soc.h"
#include "soc/cpu.h"
#include "soc/rtc.h"
#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/timer_group_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"

#include "sdkconfig.h"
#include "mpconfigboard.h"
#include "esp_image_format.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "esp_flash_partitions.h"
#include "bootloader_flash.h"
#include "bootmgr.h"
#include "bootloader_random.h"
#include "bootloader_clock.h"

#include "flash_qio_mode.h"
#include "mperror.h"


#define MAP_ERR_MSG "Image contains multiple %s segments. Only the last one will be mapped."

extern int _bss_start;
extern int _bss_end;
extern int _data_start;
extern int _data_end;

static const char* TAG = "boot";

static const uint8_t empty_signature[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/*
We arrive here after the bootloader finished loading the program from flash. The hardware is mostly uninitialized,
flash cache is down and the app CPU is in reset. We do have a stack, so we can do the initialization in C.
*/

extern __attribute__((noreturn)) void mperror_fatal_error (void);

static void bootloader_main();
static void unpack_load_app(const esp_image_metadata_t* data);
static void print_flash_info(const esp_image_header_t* pfhdr);
static void set_cache_and_start_app(uint32_t drom_addr,
    uint32_t drom_load_addr,
    uint32_t drom_size,
    uint32_t irom_addr,
    uint32_t irom_load_addr,
    uint32_t irom_size,
    uint32_t entry_addr);
static void update_flash_config(const esp_image_header_t* pfhdr);
static void vddsdio_configure();
static void flash_gpio_configure();
static void uart_console_configure(void);
static void wdt_reset_check(void);

// static void read_mac(uint8_t* mac)
// {
//     uint32_t mac_low = REG_READ(EFUSE_BLK0_RDATA1_REG);
//     uint32_t mac_high = REG_READ(EFUSE_BLK0_RDATA2_REG);

//     mac[0] = mac_high >> 8;
//     mac[1] = mac_high;
//     mac[2] = mac_low >> 24;
//     mac[3] = mac_low >> 16;
//     mac[4] = mac_low >> 8;
//     mac[5] = mac_low;
// }

/*
 * We arrive here after the ROM bootloader finished loading this second stage bootloader from flash.
 * The hardware is mostly uninitialized, flash cache is down and the app CPU is in reset.
 * We do have a stack, so we can do the initialization in C.
 */
void call_start_cpu0()
{
    cpu_configure_region_protection();

    /* Sanity check that static RAM is after the stack */
#ifndef NDEBUG
    {
        int *sp = get_sp();
        assert(&_bss_start <= &_bss_end);
        assert(&_data_start <= &_data_end);
        assert(sp < &_bss_start);
        assert(sp < &_data_start);
    }
#endif

    //Clear bss
    memset(&_bss_start, 0, (&_bss_end - &_bss_start) * sizeof(_bss_start));

    /* completely reset MMU for both CPUs
       (in case serial bootloader was running) */
    Cache_Read_Disable(0);
    Cache_Read_Disable(1);
    Cache_Flush(0);
    Cache_Flush(1);
    mmu_init(0);
    DPORT_REG_SET_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MMU_IA_CLR);
    mmu_init(1);
    DPORT_REG_CLR_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MMU_IA_CLR);
    /* (above steps probably unnecessary for most serial bootloader
       usage, all that's absolutely needed is that we unmask DROM0
       cache on the following two lines - normal ROM boot exits with
       DROM0 cache unmasked, but serial bootloader exits with it
       masked. However can't hurt to be thorough and reset
       everything.)

       The lines which manipulate DPORT_APP_CACHE_MMU_IA_CLR bit are
       necessary to work around a hardware bug.
    */
    DPORT_REG_CLR_BIT(DPORT_PRO_CACHE_CTRL1_REG, DPORT_PRO_CACHE_MASK_DROM0);
    DPORT_REG_CLR_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MASK_DROM0);

    bootloader_main();
}


/** @brief Load partition table
 *
 *  Parse partition table, get useful data such as location of
 *  OTA data partition, factory app partition, and test app partition.
 *
 *  @param         bs     bootloader state structure used to save read data
 *  @return        return true if the partition table was succesfully loaded and MD5 checksum is valid.
 */
bool load_partition_table(bootloader_state_t* bs)
{
    const esp_partition_info_t *partitions;
    const int ESP_PARTITION_TABLE_DATA_LEN = 0xC00; /* length of actual data (signature is appended to this) */
    char *partition_usage;
    esp_err_t err;
    int num_partitions;

#ifdef CONFIG_SECURE_BOOT_ENABLED
    if(esp_secure_boot_enabled()) {
        ESP_LOGI(TAG, "Verifying partition table signature...");
        err = esp_secure_boot_verify_signature(CONFIG_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_DATA_LEN);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to verify partition table signature.");
            return false;
        }
        ESP_LOGD(TAG, "Partition table signature verified");
    }
#endif

    partitions = bootloader_mmap(CONFIG_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_DATA_LEN);
    if (!partitions) {
            ESP_LOGE(TAG, "bootloader_mmap(0x%x, 0x%x) failed", CONFIG_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_DATA_LEN);
            return false;
    }
    ESP_LOGD(TAG, "mapped partition table 0x%x at 0x%x", CONFIG_PARTITION_TABLE_OFFSET, (intptr_t)partitions);

    err = esp_partition_table_verify(partitions, true, &num_partitions);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to verify partition table");
        return false;
    }

    ESP_LOGI(TAG, "Partition Table:");
    ESP_LOGI(TAG, "## Label            Usage          Type ST Offset   Length");

    for(int i = 0; i < num_partitions; i++) {
        const esp_partition_info_t *partition = &partitions[i];
        ESP_LOGD(TAG, "load partition table entry 0x%x", (intptr_t)partition);
        ESP_LOGD(TAG, "type=%x subtype=%x", partition->type, partition->subtype);
        partition_usage = "unknown";

        /* valid partition table */
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

static IRAM_ATTR bool ota_write_boot_info (boot_info_t *boot_info, uint32_t offset) {
    esp_rom_spiflash_result_t write_result;

    boot_info->crc = ota_select_crc(boot_info);
    Cache_Read_Disable(0);
    if (ESP_ROM_SPIFLASH_RESULT_OK != esp_rom_spiflash_erase_sector(offset / 0x1000)) {
        ESP_LOGE(TAG, SPI_ERROR_LOG);
        Cache_Read_Enable(0);
        return false;
    }

    if (esp_flash_encryption_enabled()) {
            // if flash is encrypted, then Write is done 32B chunks
        uint8_t buff[64] __attribute__((aligned (32)));
        memcpy(buff, (void *)boot_info, sizeof(boot_info_t));
        write_result = esp_rom_spiflash_write_encrypted(offset, (void *)boot_info, 64);
    }
    else {
            write_result = esp_rom_spiflash_write(offset, (void *)boot_info, sizeof(boot_info_t));
    }

    if (ESP_ROM_SPIFLASH_RESULT_OK != write_result) {
        ESP_LOGE(TAG, SPI_ERROR_LOG);
        Cache_Read_Enable(0);
        return false;
    }
    Cache_Read_Enable(0);
    return true;
}


/* Return true if a partition has a valid app image that was successfully loaded */
static bool get_image_from_partition(const esp_partition_pos_t *partition, esp_image_metadata_t *data)
{
    if (partition->size == 0) {
        ESP_LOGD(TAG, "Can't boot from zero-length partition");
        return false;
    }

    if (bootloader_load_image(partition, data) == ESP_OK) {
        ESP_LOGI(TAG, "Loaded app from partition at offset 0x%x",
                 partition->offset);
        return true;
    }

    return false;
}

static bool find_active_image(bootloader_state_t *bs, esp_partition_pos_t *partition)
{
    boot_info_t *boot_info;
    boot_info_t _boot_info;

    if (bs->ota_info.size < 2 * sizeof(esp_ota_select_entry_t)) {
        ESP_LOGE(TAG, "ERROR: ota_info partition size %d is too small (minimum %d bytes)", bs->ota_info.size, sizeof(esp_ota_select_entry_t));
        return false;
    }
    ESP_LOGI(TAG, "Loading boot info");
    boot_info = (boot_info_t *)bootloader_mmap(bs->ota_info.offset, bs->ota_info.size);
    if (!boot_info) {
        ESP_LOGE(TAG, "bootloader_mmap(0x%x, 0x%x) failed", bs->ota_info.offset, bs->ota_info.size);
        return false;
    }
    memcpy(&_boot_info, boot_info, sizeof(boot_info_t));
    bootloader_munmap(boot_info);
    boot_info = &_boot_info;
#ifndef RGB_LED_DISABLE
    mperror_init0();
#endif

    // // check the signature fot he bootloader first
    // uint8_t signature[16];
    // calculate_signature(signature);
    // if (!memcmp(boot_info->signature, empty_signature, sizeof(boot_info->signature))) {
    //     ESP_LOGI(TAG, "Writing the signature");
    //     // write the signature
    //     memcpy(boot_info->signature, signature, sizeof(boot_info->signature));
    //     if (!ota_write_boot_info (boot_info, bs->ota_info.offset)) {
    //         ESP_LOGE(TAG, "Error writing boot info");
    //         mperror_fatal_error();
    //         return false;
    //     }
    // } else {
    //     ESP_LOGI(TAG, "Comparing the signature");
    //     // compare the signatures
    //     if (memcmp(boot_info->signature, signature, sizeof(boot_info->signature))) {
    //         // signature check failed, don't load the app!
    //         mperror_fatal_error();
    //         return false;
    //     }
    // }

    if (!ota_select_valid(boot_info)) {
        ESP_LOGI(TAG, "Initializing OTA partition info");
        // init status flash
        memcpy(partition, &bs->image[0], sizeof(esp_partition_pos_t));
        boot_info->ActiveImg = IMG_ACT_FACTORY;
        boot_info->Status = IMG_STATUS_READY;
        boot_info->PrevImg = IMG_ACT_FACTORY;
        boot_info->safeboot = false;
        if (!ota_write_boot_info (boot_info, bs->ota_info.offset)) {
            ESP_LOGE(TAG, "Error writing boot info");
#ifndef RGB_LED_DISABLE
            mperror_fatal_error();
#endif
            return false;
        }
        return true;
    } else {
        // CRC is fine, check here the image that we need to load based on the status (ready or check)
        // if image is in status check then we must verify the MD5, and set the new status
        // if the MD5 fails, then we roll back to the previous image

        // do we have a new image that needs to be verified?
        if (boot_info->Status == IMG_STATUS_CHECK) {
            if (boot_info->ActiveImg == IMG_ACT_UPDATE2) {
                boot_info->ActiveImg = IMG_ACT_FACTORY;    // we only have space for 1 OTA image
            }

            // verify the active image (ota partition)
            esp_image_metadata_t data;
            if (ESP_OK != esp_image_verify(ESP_IMAGE_VERIFY, &bs->image[boot_info->ActiveImg], &data)) {
                ets_printf("Cannot load Firmware img in the active partition! .. Defaulting back to previous partition\n");
                // switch to the previous image
                uint32_t tempimg = boot_info->ActiveImg;
                boot_info->ActiveImg = boot_info->PrevImg;
                boot_info->PrevImg = tempimg;
            }

            // in any case, change the status to "READY"
            boot_info->Status = IMG_STATUS_READY;
            // write the new boot info
            if (!ota_write_boot_info (boot_info, bs->ota_info.offset)) {
                ESP_LOGE(TAG, "Error writing boot info");
                mperror_fatal_error();
                return false;
            }
        }

        // this one might modify the boot info hence it MUST be called after
        // bootmgr_verify! (so that the changes are not saved to flash)
        ESP_LOGI(TAG, "Checking safe boot pin");
        uint32_t ActiveImg = boot_info->ActiveImg;
        uint32_t safeboot = wait_for_safe_boot (boot_info, &ActiveImg);
        if (safeboot > 0) {
            ESP_LOGI(TAG, "Safe boot requested!");
        }
        if (safeboot != boot_info->safeboot) {
            if (boot_info->safeboot == SAFE_BOOT_SW) {
                boot_info->safeboot = SAFE_BOOT_HW;
            } else {
                boot_info->safeboot = safeboot;
            }
            // write the new boot info
            if (!ota_write_boot_info (boot_info, bs->ota_info.offset)) {
                ESP_LOGE(TAG, "Error writing boot info");
                mperror_fatal_error();
                return false;
            }
        }

        // load the selected active image
        memcpy(partition, &bs->image[ActiveImg], sizeof(esp_partition_pos_t));
        return true;
    }
}


/**
 *  @function :     bootloader_main
 *  @description:   entry function of 2nd bootloader
 *
 *  @inputs:        void
 */

static void bootloader_main()
{
    vddsdio_configure();
    flash_gpio_configure();
    bootloader_clock_configure();
    uart_console_configure();
    ESP_LOGI(TAG, "ESP-IDF 2nd stage bootloader");
    wdt_reset_check();
#if defined(CONFIG_SECURE_BOOT_ENABLED) || defined(CONFIG_FLASH_ENCRYPTION_ENABLED)
    esp_err_t err;
#endif
    esp_image_header_t fhdr __attribute__((aligned (4)));
    bootloader_state_t bootloader_state __attribute__((aligned (4)));
    esp_partition_pos_t partition __attribute__((aligned (4)));
    esp_image_metadata_t image_data __attribute__((aligned (4)));

    memset(&bootloader_state, 0, sizeof(bootloader_state));
    ets_set_appcpu_boot_addr(0);

    /* disable watch dog here */
    REG_CLR_BIT( RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_FLASHBOOT_MOD_EN );
    REG_CLR_BIT( TIMG_WDTCONFIG0_REG(0), TIMG_WDT_FLASHBOOT_MOD_EN );

#ifndef CONFIG_SPI_FLASH_ROM_DRIVER_PATCH
    const uint32_t spiconfig = ets_efuse_get_spiconfig();
    if(spiconfig != EFUSE_SPICONFIG_SPI_DEFAULTS && spiconfig != EFUSE_SPICONFIG_HSPI_DEFAULTS) {
        ESP_LOGE(TAG, "SPI flash pins are overridden. \"Enable SPI flash ROM driver patched functions\" must be enabled in menuconfig");
        return;
    }
#endif

    esp_rom_spiflash_unlock();

    ESP_LOGI(TAG, "Enabling RNG early entropy source...");
    bootloader_random_enable();

#if CONFIG_FLASHMODE_QIO || CONFIG_FLASHMODE_QOUT
    bootloader_enable_qio_mode();
#endif

    if (bootloader_flash_read(ESP_BOOTLOADER_OFFSET, &fhdr,
                              sizeof(esp_image_header_t), true) != ESP_OK) {
        ESP_LOGE(TAG, "failed to load bootloader header!");
        return;
    }

    // force 4MB flash size for all boards
    fhdr.spi_size = ESP_IMAGE_FLASH_SIZE_4MB;

    print_flash_info(&fhdr);
    update_flash_config(&fhdr);

    if (!load_partition_table(&bootloader_state)) {
        ESP_LOGE(TAG, "load partition table error!");
        return;
    }

    // check if the partition table has OTA info partition
    if (bootloader_state.ota_info.offset == 0 || !find_active_image(&bootloader_state, &partition)) {
        // nothing to load, bail out
        ESP_LOGE(TAG, "nothing to load");
#ifndef RGB_LED_DISABLE
        mperror_fatal_error();
#endif
        return;
    }

    get_image_from_partition(&partition, &image_data);

#ifdef CONFIG_SECURE_BOOT_ENABLED
    // Generate secure digest from this bootloader to protect future modifications
    ESP_LOGI(TAG, "Checking secure boot...");
    err = esp_secure_boot_permanently_enable();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bootloader digest generation failed (%d). SECURE BOOT IS NOT ENABLED.", err);
        // Stop booting, as this could next Encrypt the whole Flash
        return;
    }
#endif

#ifdef CONFIG_FLASH_ENCRYPTION_ENABLED
    // encrypt flash
    ESP_LOGI(TAG, "Checking flash encryption...");
    bool flash_encryption_enabled = esp_flash_encryption_enabled();
    err = esp_flash_encrypt_check_and_update();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Flash encryption check failed (%d).", err);
        return;
    }

    if (!flash_encryption_enabled && esp_flash_encryption_enabled()) {
        /* Flash encryption was just enabled for the first time,
           so issue a system reset to ensure flash encryption
           cache resets properly */
        ESP_LOGI(TAG, "Resetting with flash encryption enabled...");
        REG_WRITE(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_SW_SYS_RST);
        return;
    }
#endif

    ESP_LOGI(TAG, "Disabling RNG early entropy source...");
    bootloader_random_disable();

    // copy loaded segments to RAM, set up caches for mapped segments, and start application
    ESP_LOGI(TAG, "Loading app partition at offset %08x", partition.offset);
    unpack_load_app(&image_data);
}

static void unpack_load_app(const esp_image_metadata_t* data)
{
    uint32_t drom_addr = 0;
    uint32_t drom_load_addr = 0;
    uint32_t drom_size = 0;
    uint32_t irom_addr = 0;
    uint32_t irom_load_addr = 0;
    uint32_t irom_size = 0;

    // Find DROM & IROM addresses, to configure cache mappings
    for (int i = 0; i < data->image.segment_count; i++) {
        const esp_image_segment_header_t *header = &data->segments[i];
        if (header->load_addr >= SOC_IROM_LOW && header->load_addr < SOC_IROM_HIGH) {
            if (drom_addr != 0) {
                ESP_LOGE(TAG, MAP_ERR_MSG, "DROM");
            } else {
                ESP_LOGD(TAG, "Mapping segment %d as %s", i, "DROM");
            }
            drom_addr = data->segment_data[i];
            drom_load_addr = header->load_addr;
            drom_size = header->data_len;
        }
        if (header->load_addr >= SOC_DROM_LOW && header->load_addr < SOC_DROM_HIGH) {
            if (irom_addr != 0) {
                ESP_LOGE(TAG, MAP_ERR_MSG, "IROM");
            } else {
                ESP_LOGD(TAG, "Mapping segment %d as %s", i, "IROM");
            }
            irom_addr = data->segment_data[i];
            irom_load_addr = header->load_addr;
            irom_size = header->data_len;
        }
    }

    ESP_LOGD(TAG, "calling set_cache_and_start_app");
    set_cache_and_start_app(drom_addr,
        drom_load_addr,
        drom_size,
        irom_addr,
        irom_load_addr,
        irom_size,
        data->image.entry_addr);
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

    /* Clear the MMU entries that are already set up,
       so the new app only has the mappings it creates.
    */
    for (int i = 0; i < DPORT_FLASH_MMU_TABLE_SIZE; i++) {
        DPORT_PRO_FLASH_MMU_TABLE[i] = DPORT_FLASH_MMU_TABLE_INVALID_VAL;
    }

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
    DPORT_REG_CLR_BIT( DPORT_PRO_CACHE_CTRL1_REG, (DPORT_PRO_CACHE_MASK_IRAM0) | (DPORT_PRO_CACHE_MASK_IRAM1 & 0) | (DPORT_PRO_CACHE_MASK_IROM0 & 0) | DPORT_PRO_CACHE_MASK_DROM0 | DPORT_PRO_CACHE_MASK_DRAM1 );
    DPORT_REG_CLR_BIT( DPORT_APP_CACHE_CTRL1_REG, (DPORT_APP_CACHE_MASK_IRAM0) | (DPORT_APP_CACHE_MASK_IRAM1 & 0) | (DPORT_APP_CACHE_MASK_IROM0 & 0) | DPORT_APP_CACHE_MASK_DROM0 | DPORT_APP_CACHE_MASK_DRAM1 );
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
    Cache_Read_Disable( 0 );
    // Set flash chip size
    esp_rom_spiflash_config_param(g_rom_flashchip.device_id, size * 0x100000, 0x10000, 0x1000, 0x100, 0xffff);
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

    /* SPI mode could have been set to QIO during boot already,
       so test the SPI registers not the flash header */
    uint32_t spi_ctrl = REG_READ(SPI_CTRL_REG(0));
    if (spi_ctrl & SPI_FREAD_QIO) {
        str = "QIO";
    } else if (spi_ctrl & SPI_FREAD_QUAD) {
        str = "QOUT";
    } else if (spi_ctrl & SPI_FREAD_DIO) {
        str = "DIO";
    } else if (spi_ctrl & SPI_FREAD_DUAL) {
        str = "DOUT";
    } else if (spi_ctrl & SPI_FASTRD_MODE) {
        str = "FAST READ";
    } else {
        str = "SLOW READ";
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


static void vddsdio_configure()
{
#if CONFIG_BOOTLOADER_VDDSDIO_BOOST_1_9V
    rtc_vddsdio_config_t cfg = rtc_vddsdio_get_config();
    if (cfg.enable == 1 && cfg.tieh == 0) {    // VDDSDIO regulator is enabled @ 1.8V
        cfg.drefh = 3;
        cfg.drefm = 3;
        cfg.drefl = 3;
        cfg.force = 1;
        rtc_vddsdio_set_config(cfg);
        ets_delay_us(10); // wait for regulator to become stable
    }
#endif // CONFIG_BOOTLOADER_VDDSDIO_BOOST_1_9V
}


#define FLASH_CLK_IO      6
#define FLASH_CS_IO       11
#define FLASH_SPIQ_IO     7
#define FLASH_SPID_IO     8
#define FLASH_SPIWP_IO    10
#define FLASH_SPIHD_IO    9
#define FLASH_IO_MATRIX_DUMMY_40M   1
#define FLASH_IO_MATRIX_DUMMY_80M   2
static void IRAM_ATTR flash_gpio_configure()
{
    int spi_cache_dummy = 0;
    int drv = 2;
#if CONFIG_FLASHMODE_QIO
    spi_cache_dummy = SPI0_R_QIO_DUMMY_CYCLELEN;   //qio 3
#elif CONFIG_FLASHMODE_QOUT
    spi_cache_dummy = SPI0_R_FAST_DUMMY_CYCLELEN;  //qout 7
#elif CONFIG_FLASHMODE_DIO
    spi_cache_dummy = SPI0_R_DIO_DUMMY_CYCLELEN;   //dio 3
#elif CONFIG_FLASHMODE_DOUT
    spi_cache_dummy = SPI0_R_FAST_DUMMY_CYCLELEN;  //dout 7
#endif
    /* dummy_len_plus values defined in ROM for SPI flash configuration */
    extern uint8_t g_rom_spiflash_dummy_len_plus[];
#if CONFIG_ESPTOOLPY_FLASHFREQ_40M
    g_rom_spiflash_dummy_len_plus[0] = FLASH_IO_MATRIX_DUMMY_40M;
    g_rom_spiflash_dummy_len_plus[1] = FLASH_IO_MATRIX_DUMMY_40M;
    SET_PERI_REG_BITS(SPI_USER1_REG(0), SPI_USR_DUMMY_CYCLELEN_V, spi_cache_dummy + FLASH_IO_MATRIX_DUMMY_40M, SPI_USR_DUMMY_CYCLELEN_S);  //DUMMY
#elif CONFIG_ESPTOOLPY_FLASHFREQ_80M
    g_rom_spiflash_dummy_len_plus[0] = FLASH_IO_MATRIX_DUMMY_80M;
    g_rom_spiflash_dummy_len_plus[1] = FLASH_IO_MATRIX_DUMMY_80M;
    SET_PERI_REG_BITS(SPI_USER1_REG(0), SPI_USR_DUMMY_CYCLELEN_V, spi_cache_dummy + FLASH_IO_MATRIX_DUMMY_80M, SPI_USR_DUMMY_CYCLELEN_S);  //DUMMY
    drv = 3;
#endif

    uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
    uint32_t pkg_ver = chip_ver & 0x7;

    if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5) {
        // For ESP32D2WD the SPI pins are already configured
        // flash clock signal should come from IO MUX.
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CLK_U, FUNC_SD_CLK_SPICLK);
        SET_PERI_REG_BITS(PERIPHS_IO_MUX_SD_CLK_U, FUN_DRV, drv, FUN_DRV_S);
    } else if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2) {
        // For ESP32PICOD2 the SPI pins are already configured
        // flash clock signal should come from IO MUX.
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CLK_U, FUNC_SD_CLK_SPICLK);
        SET_PERI_REG_BITS(PERIPHS_IO_MUX_SD_CLK_U, FUN_DRV, drv, FUN_DRV_S);
    } else if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4) {
        // For ESP32PICOD4 the SPI pins are already configured
        // flash clock signal should come from IO MUX.
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CLK_U, FUNC_SD_CLK_SPICLK);
        SET_PERI_REG_BITS(PERIPHS_IO_MUX_SD_CLK_U, FUN_DRV, drv, FUN_DRV_S);
    } else {
        const uint32_t spiconfig = ets_efuse_get_spiconfig();
        if (spiconfig == EFUSE_SPICONFIG_SPI_DEFAULTS) {
            gpio_matrix_out(FLASH_CS_IO, SPICS0_OUT_IDX, 0, 0);
            gpio_matrix_out(FLASH_SPIQ_IO, SPIQ_OUT_IDX, 0, 0);
            gpio_matrix_in(FLASH_SPIQ_IO, SPIQ_IN_IDX, 0);
            gpio_matrix_out(FLASH_SPID_IO, SPID_OUT_IDX, 0, 0);
            gpio_matrix_in(FLASH_SPID_IO, SPID_IN_IDX, 0);
            gpio_matrix_out(FLASH_SPIWP_IO, SPIWP_OUT_IDX, 0, 0);
            gpio_matrix_in(FLASH_SPIWP_IO, SPIWP_IN_IDX, 0);
            gpio_matrix_out(FLASH_SPIHD_IO, SPIHD_OUT_IDX, 0, 0);
            gpio_matrix_in(FLASH_SPIHD_IO, SPIHD_IN_IDX, 0);
            //select pin function gpio
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA0_U, PIN_FUNC_GPIO);
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA1_U, PIN_FUNC_GPIO);
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA2_U, PIN_FUNC_GPIO);
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA3_U, PIN_FUNC_GPIO);
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CMD_U, PIN_FUNC_GPIO);
            // flash clock signal should come from IO MUX.
            // set drive ability for clock
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CLK_U, FUNC_SD_CLK_SPICLK);
            SET_PERI_REG_BITS(PERIPHS_IO_MUX_SD_CLK_U, FUN_DRV, drv, FUN_DRV_S);
        }
    }
}


static void uart_console_configure(void)
{
#if CONFIG_CONSOLE_UART_NONE
    ets_install_putc1(NULL);
    ets_install_putc2(NULL);
#else // CONFIG_CONSOLE_UART_NONE
    const int uart_num = CONFIG_CONSOLE_UART_NUM;

    uartAttach();
    ets_install_uart_printf();

    // Wait for UART FIFO to be empty.
    uart_tx_wait_idle(0);

#if CONFIG_CONSOLE_UART_CUSTOM
    // Some constants to make the following code less upper-case
    const int uart_tx_gpio = CONFIG_CONSOLE_UART_TX_GPIO;
    const int uart_rx_gpio = CONFIG_CONSOLE_UART_RX_GPIO;
    // Switch to the new UART (this just changes UART number used for
    // ets_printf in ROM code).
    uart_tx_switch(uart_num);
    // If console is attached to UART1 or if non-default pins are used,
    // need to reconfigure pins using GPIO matrix
    if (uart_num != 0 || uart_tx_gpio != 1 || uart_rx_gpio != 3) {
        // Change pin mode for GPIO1/3 from UART to GPIO
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_GPIO3);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_GPIO1);
        // Route GPIO signals to/from pins
        // (arrays should be optimized away by the compiler)
        const uint32_t tx_idx_list[3] = { U0TXD_OUT_IDX, U1TXD_OUT_IDX, U2TXD_OUT_IDX };
        const uint32_t rx_idx_list[3] = { U0RXD_IN_IDX, U1RXD_IN_IDX, U2RXD_IN_IDX };
        const uint32_t tx_idx = tx_idx_list[uart_num];
        const uint32_t rx_idx = rx_idx_list[uart_num];
        gpio_matrix_out(uart_tx_gpio, tx_idx, 0, 0);
        gpio_matrix_in(uart_rx_gpio, rx_idx, 0);
    }
#endif // CONFIG_CONSOLE_UART_CUSTOM

    // Set configured UART console baud rate
    const int uart_baud = CONFIG_CONSOLE_UART_BAUDRATE;
    uart_div_modify(uart_num, (rtc_clk_apb_freq_get() << 4) / uart_baud);

#endif // CONFIG_CONSOLE_UART_NONE
}

static void wdt_reset_cpu0_info_enable(void)
{
    // We do not reset core1 info here because it didn't work before cpu1 was up. So we put it into call_start_cpu1.
    DPORT_REG_SET_BIT(DPORT_PRO_CPU_RECORD_CTRL_REG, DPORT_PRO_CPU_PDEBUG_ENABLE | DPORT_PRO_CPU_RECORD_ENABLE);
    DPORT_REG_CLR_BIT(DPORT_PRO_CPU_RECORD_CTRL_REG, DPORT_PRO_CPU_RECORD_ENABLE);
}

static void wdt_reset_info_dump(int cpu)
{
    uint32_t inst = 0, pid = 0, stat = 0, data = 0, pc = 0,
             lsstat = 0, lsaddr = 0, lsdata = 0, dstat = 0;
    char *cpu_name = cpu ? "APP" : "PRO";

    if (cpu == 0) {
        stat    = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_STATUS_REG);
        pid     = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PID_REG);
        inst    = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGINST_REG);
        dstat   = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGSTATUS_REG);
        data    = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGDATA_REG);
        pc      = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGPC_REG);
        lsstat  = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGLS0STAT_REG);
        lsaddr  = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGLS0ADDR_REG);
        lsdata  = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGLS0DATA_REG);

    } else {
        stat    = DPORT_REG_READ(DPORT_APP_CPU_RECORD_STATUS_REG);
        pid     = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PID_REG);
        inst    = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGINST_REG);
        dstat   = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGSTATUS_REG);
        data    = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGDATA_REG);
        pc      = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGPC_REG);
        lsstat  = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGLS0STAT_REG);
        lsaddr  = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGLS0ADDR_REG);
        lsdata  = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGLS0DATA_REG);
    }
    if (DPORT_RECORD_PDEBUGINST_SZ(inst) == 0 &&
        DPORT_RECORD_PDEBUGSTATUS_BBCAUSE(dstat) == DPORT_RECORD_PDEBUGSTATUS_BBCAUSE_WAITI) {
        ESP_LOGW(TAG, "WDT reset info: %s CPU PC=0x%x (waiti mode)", cpu_name, pc);
    } else {
        ESP_LOGW(TAG, "WDT reset info: %s CPU PC=0x%x", cpu_name, pc);
    }
    ESP_LOGD(TAG, "WDT reset info: %s CPU STATUS        0x%08x", cpu_name, stat);
    ESP_LOGD(TAG, "WDT reset info: %s CPU PID           0x%08x", cpu_name, pid);
    ESP_LOGD(TAG, "WDT reset info: %s CPU PDEBUGINST    0x%08x", cpu_name, inst);
    ESP_LOGD(TAG, "WDT reset info: %s CPU PDEBUGSTATUS  0x%08x", cpu_name, dstat);
    ESP_LOGD(TAG, "WDT reset info: %s CPU PDEBUGDATA    0x%08x", cpu_name, data);
    ESP_LOGD(TAG, "WDT reset info: %s CPU PDEBUGPC      0x%08x", cpu_name, pc);
    ESP_LOGD(TAG, "WDT reset info: %s CPU PDEBUGLS0STAT 0x%08x", cpu_name, lsstat);
    ESP_LOGD(TAG, "WDT reset info: %s CPU PDEBUGLS0ADDR 0x%08x", cpu_name, lsaddr);
    ESP_LOGD(TAG, "WDT reset info: %s CPU PDEBUGLS0DATA 0x%08x", cpu_name, lsdata);
}

static void wdt_reset_check(void)
{
    int wdt_rst = 0;
    RESET_REASON rst_reas[2];

    rst_reas[0] = rtc_get_reset_reason(0);
    rst_reas[1] = rtc_get_reset_reason(1);
    if (rst_reas[0] == RTCWDT_SYS_RESET || rst_reas[0] == TG0WDT_SYS_RESET || rst_reas[0] == TG1WDT_SYS_RESET ||
        rst_reas[0] == TGWDT_CPU_RESET  || rst_reas[0] == RTCWDT_CPU_RESET) {
        ESP_LOGW(TAG, "PRO CPU has been reset by WDT.");
        wdt_rst = 1;
    }
    if (rst_reas[1] == RTCWDT_SYS_RESET || rst_reas[1] == TG0WDT_SYS_RESET || rst_reas[1] == TG1WDT_SYS_RESET ||
        rst_reas[1] == TGWDT_CPU_RESET  || rst_reas[1] == RTCWDT_CPU_RESET) {
        ESP_LOGW(TAG, "APP CPU has been reset by WDT.");
        wdt_rst = 1;
    }
    if (wdt_rst) {
        // if reset by WDT dump info from trace port
        wdt_reset_info_dump(0);
        wdt_reset_info_dump(1);
    }
    wdt_reset_cpu0_info_enable();
}


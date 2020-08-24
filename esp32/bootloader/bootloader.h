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
#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

#include <stdint.h>
#include "esp_flash_data_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* OTA selection structure (two copies in the OTA data partition).
   Size of 32 bytes is friendly to flash encryption */
typedef struct {
    uint32_t ota_seq;
    uint8_t  seq_label[24];
    uint32_t crc; /* CRC32 of ota_seq field only */
} ota_select;

typedef struct _boot_info_t
{
  uint32_t  ActiveImg;
  uint32_t  Status;
  uint32_t  PrevImg;
  uint32_t  size;
  uint32_t  safeboot;
  uint8_t   signature[16];
  uint32_t  crc;
} boot_info_t;

#define IMG_SIZE_8MB                            (1980 * 1024)
#define IMG_UPDATE1_OFFSET_8MB                  (2112 * 1024)  // taken from the partitions table

#define IMG_SIZE_4MB                            (1720 * 1024)
#define IMG_UPDATE1_OFFSET_4MB                  (1792 * 1024)  // taken from the partitions table

#define OTAA_DATA_SIZE                      (4 * 1024)
#define OTA_DATA_INDEX                      2
#define IMG_FACTORY_OFFSET                  (64 * 1024)


#define IMG_UPDATE2_OFFSET                  (IMG_FACTORY_OFFSET)

#define IMG_STATUS_CHECK                    0
#define IMG_STATUS_READY                    1
#define IMG_STATUS_PATCH                    2

#define IMG_ACT_FACTORY                     0
#define IMG_ACT_UPDATE1                     1
#define IMG_ACT_UPDATE2                     2

#define BOOT_VERSION                        "V0.3"
#define SPI_SEC_SIZE                        0x1000

#define PARTITIONS_COUNT_8MB                    5
#define PARTITIONS_COUNT_4MB                    7

#define PART_TYPE_APP                       0x00
#define PART_SUBTYPE_FACTORY                0x00
#define PART_SUBTYPE_OTA_FLAG               0x10
#define PART_SUBTYPE_OTA_MASK               0x0f
#define PART_SUBTYPE_TEST                   0x20

#define PART_TYPE_DATA                      0x01
#define PART_SUBTYPE_DATA_OTA               0x00
#define PART_SUBTYPE_DATA_RF                0x01
#define PART_SUBTYPE_DATA_WIFI              0x02

#define SAFE_BOOT_HW                        0x01
#define SAFE_BOOT_SW                        0x02

#define SPI_ERROR_LOG "spi flash error"

typedef struct {
    esp_partition_pos_t ota_info;
    esp_partition_pos_t image[3];
    uint32_t image_count;
    uint32_t selected_subtype;
} bootloader_state_t;

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H__ */

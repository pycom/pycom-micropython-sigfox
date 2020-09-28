/*
 * Copyright (c) 2020, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "pycom_config.h"
#include "wifi_conn.h"
#include "cJSON.h"

static const char *TAG = "FactoryFWMain";

extern const uint8_t pycom_server_cert_pem_start[] asm("_binary_pycom_ca_cert_pem_start");

static bool ota_pending() {
    uint8_t status = config_get_pybytes_ota_status();
    ESP_LOGI(TAG, "Pybytes OTA Status: %u", status);
    if (status == OTA_STATUS_PENDING) {
        return true;
    } else {
        return false;
    }
}

/* 
  This function requests the manifest.json file from the Pycom server and returns the
  URL of the firmware binary if available
*/
static bool get_fw_url(char *url) {
    const int MAX_HTTP_RECV_BUFFER = 512;

    char manifest_req_url[200];
    char res_buffer[MAX_HTTP_RECV_BUFFER]; // Buffer for the  server response (manifest.json)
    uint8_t wmac[6];
    uint8_t sw_version[12];
    uint8_t sysname[6];

    ESP_ERROR_CHECK(esp_efuse_mac_get_default(wmac));
    config_get_sw_version(sw_version);
    config_get_pybytes_sysname(sysname);

    if (config_get_pybytes_fwtype() == FW_TYPE_PYMESH) {
        // We need to request Pymesh firmware which requires additional parameters: token and fwtype
        uint8_t token[40];
        config_get_pybytes_device_token(token);

        snprintf(manifest_req_url, 200, "https://%s:%s/manifest.json?current_ver=%s&sysname=%s&token=%s&ota_slot=0x110000&wmac=%X%X%X%X%X%X&fwtype=pymesh",
                 CONFIG_PYCOM_SW_SERVER, CONFIG_PYCOM_SW_SERVER_PORT, sw_version, sysname, token, wmac[0], wmac[1], wmac[2], wmac[3], wmac[4], wmac[5]);
    } else {
        // Requesting Non-Pymesh firmware
        snprintf(manifest_req_url, 200, "https://%s:%s/manifest.json?current_ver=%s&sysname=%s&ota_slot=0x110000&wmac=%X%X%X%X%X%X",
                 CONFIG_PYCOM_SW_SERVER, CONFIG_PYCOM_SW_SERVER_PORT, sw_version, sysname, wmac[0], wmac[1], wmac[2], wmac[3], wmac[4], wmac[5]);
    }

    ESP_LOGD(TAG, "MANIFEST REQ URL: %s", manifest_req_url);

    esp_http_client_config_t config = {
        .url = manifest_req_url,
        .cert_pem = (char *)pycom_server_cert_pem_start,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);

    ESP_LOGD(TAG, "content_length: %d", content_length);

    int total_read_len = 0, read_len;
    if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER) {
        read_len = esp_http_client_read(client, res_buffer, content_length);
        if (read_len <= 0) {
            printf("Error read data, read_len: %d\n", read_len);
        } else {
            res_buffer[read_len] = 0;
            ESP_LOGI(TAG, "read_len = %d", read_len);
        }
    } else {
        printf("Invalid Content Lenght: %d\n", content_length);
    }
    ESP_LOGD(TAG, "Received buffer: %s", res_buffer);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (res_buffer[0] == 0) {
        ESP_LOGE(TAG, "Failed to get the manifest file");
        return false;
    }

    // Parsing the received manifest.json
    cJSON *manifest_json = cJSON_Parse(res_buffer);
    const cJSON *fw = NULL, *fw_url = NULL;

    if (manifest_json != NULL) {
        fw = cJSON_GetObjectItemCaseSensitive(manifest_json, "firmware");
        if (fw != NULL) {
            fw_url = cJSON_GetObjectItemCaseSensitive(fw, "URL");
            if (cJSON_IsString(fw_url) && (fw_url->valuestring != NULL)) {
                ESP_LOGD(TAG, "Need to fetch the binary from: \"%s\"\n", fw_url->valuestring);

                strncpy(url, fw_url->valuestring, 100);
            }
        } else {
            printf("No \"firmware\" URL in the manifest: %s\n", res_buffer);
            printf("Manifest Request URL: %s\n", manifest_req_url);
        }
    } else {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("Error before: %s\n", error_ptr);
        }
    }

    cJSON_Delete(manifest_json);

    if (url[0] == 0)
        return false;
    else
        return true;
}

void factory_fw_task(void *pvParameter) {
    char url[100] = {0};

    printf("Factory FW Task Started\n");

    if (!ota_pending()) {
        printf("No pending OTA update. Rebooting into the OTA partition.\n");

        // Setting the boot partition to OTA
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition));

        esp_restart();
    }

    if (get_fw_url(url)) {
        // If a firmware URL was received in the manifest, we use the URL to download the firmware
        esp_err_t ota_finish_err = ESP_OK;

        esp_http_client_config_t config = {
            .url = url,
            .cert_pem = (char *)pycom_server_cert_pem_start,
        };

        esp_https_ota_config_t ota_config = {
            .http_config = &config,
        };

        esp_https_ota_handle_t https_ota_handle = NULL;
        esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
        if (err != ESP_OK) {
            printf("esp_https_ota_begin failed. Error: %d, URL: %s\n", err, url);

            goto ota_fail;
        }

        esp_app_desc_t app_desc;
        err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
        if (err != ESP_OK) {
            printf("esp_https_ota_read_img_desc failed\n");
            goto ota_end;
        }

        // TODO: Add any checks (if required) on the app description or we can probably skip reading this individually
        printf("OTA Update started\n");
        while (1) {
            err = esp_https_ota_perform(https_ota_handle);
            if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                break;
            }
            // esp_https_ota_perform returns after every read operation which gives user the ability to
            // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
            // data read so far.
            ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
        }

    ota_end:
        ota_finish_err = esp_https_ota_finish(https_ota_handle);

        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            if (!config_set_pybytes_ota_status(OTA_STATUS_SUCCESS)) {
                printf("Failed to update the Pybytes OTA Status\n");
            }

            printf("ESP_HTTPS_OTA upgrade successful. Rebooting ...\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            printf("ESP_HTTPS_OTA upgrade failed. The OTA Partition could be corrupted. Reboot the device to try again.\n");

            while (1) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
    } else {
        printf("Failed to get the firmware URL.\n");
        goto ota_fail;
    }

ota_fail:
    config_set_pybytes_ota_status(OTA_STATUS_FAILURE);

    // Setting the boot partition to OTA
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition));
    esp_restart();
}

void app_main() {
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initializing Pycom configuration from the flash
    config_init0();

    /* Connecting to WiFi. The credentials for the WiFi are read from the Pycom configurations written in config partition */
    if (ESP_OK != wifi_connect()) {
        // Setting the boot partition to OTA
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition));
        esp_restart();
    }

    xTaskCreate(&factory_fw_task, "Factory FW Task", 1024 * 9, NULL, 5, NULL);
}

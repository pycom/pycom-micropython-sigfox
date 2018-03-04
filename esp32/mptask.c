/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_spi_flash.h"
#include "soc/cpu.h"

#include "py/mpconfig.h"
#include "py/stackctrl.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "lib/utils/pyexec.h"
#include "readline.h"
#include "esp32_mphal.h"
#include "machuart.h"
#include "machpin.h"
#include "mpexception.h"
#include "moduos.h"
#include "mperror.h"
#include "mpirq.h"
#include "serverstask.h"
#include "modnetwork.h"
#include "modwlan.h"
#include "antenna.h"
#include "modled.h"
#include "esp_log.h"

#if defined (LOPY) || defined (LOPY4) || defined (FIPY)
#include "modlora.h"
#endif
#if defined (SIPY) || defined(LOPY4) || defined (FIPY)
#include "sigfox/modsigfox.h"
#endif
#if defined (GPY) || defined (FIPY)
#include "modlte.h"
#endif

#include "random.h"
#include "bootmgr.h"
#include "updater.h"
#include "pycom_config.h"
#include "mpsleep.h"
#include "machrtc.h"
#include "modbt.h"
#include "machtimer.h"
#include "machtimer_alarm.h"
#include "mptask.h"

#include "ff.h"
#include "diskio.h"
#include "sflash_diskio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"


/******************************************************************************
 DECLARE EXTERNAL FUNCTIONS
 ******************************************************************************/
extern void modpycom_init0(void);

/******************************************************************************
 DECLARE PRIVATE CONSTANTS
 ******************************************************************************/
#define GC_POOL_SIZE_BYTES                                          (67 * 1024)
#define GC_POOL_SIZE_BYTES_PSRAM                                    ((2048 + 512) * 1024)

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void mptask_preinit (void);
STATIC void mptask_init_sflash_filesystem (void);
#if defined (LOPY) || defined (SIPY) || defined (LOPY4) || defined (FIPY)
STATIC void mptask_update_lpwan_mac_address (void);
#endif
STATIC void mptask_enable_wifi_ap (void);
STATIC void mptask_create_main_py (void);

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
extern StackType_t *mpTaskStack;
extern TaskHandle_t svTaskHandle;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static FATFS sflash_fatfs;
static uint8_t *gc_pool_upy;

static char fresh_main_py[] = "# main.py -- put your code here!\r\n";
static char fresh_boot_py[] = "# boot.py -- run on boot-up\r\n";

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void TASK_Micropython (void *pvParameters) {
    // initialize the garbage collector with the top of our stack
    volatile uint32_t sp = (uint32_t)get_sp();
    uint32_t gc_pool_size;
    bool soft_reset = false;
    bool wifi_on_boot;
    esp_chip_info_t chip_info;
    uint32_t stack_len;

    esp_chip_info(&chip_info);
    if (chip_info.revision > 0) {
        stack_len = (MICROPY_TASK_STACK_SIZE_PSRAM / sizeof(StackType_t));
    } else {
        stack_len = (MICROPY_TASK_STACK_SIZE / sizeof(StackType_t));
    }

    // configure the antenna select switch here
    antenna_init0();
    config_init0();
    mpsleep_init0();
    if (mpsleep_get_reset_cause() != MPSLEEP_DEEPSLEEP_RESET) {
        rtc_init0();
    }

    // initialization that must not be repeted after a soft reset
    mptask_preinit();
#if MICROPY_PY_THREAD
    mp_thread_preinit(mpTaskStack, stack_len);
    mp_irq_preinit();
#endif

    // initialise the stack pointer for the main thread (must be done after mp_thread_preinit)
    mp_stack_set_top((void *)sp);

    // the stack limit should be less than real stack size, so we have a chance
    // to recover from hiting the limit (the limit is measured in bytes)
    mp_stack_set_limit(stack_len - 1024);

    if (esp_get_revision() > 0) {
        gc_pool_size = GC_POOL_SIZE_BYTES_PSRAM;
        gc_pool_upy = heap_caps_malloc(GC_POOL_SIZE_BYTES_PSRAM, MALLOC_CAP_SPIRAM);
    } else {
        gc_pool_size = GC_POOL_SIZE_BYTES;
        gc_pool_upy = heap_caps_malloc(GC_POOL_SIZE_BYTES, MALLOC_CAP_INTERNAL);
    }

    if (NULL == gc_pool_upy) {
        printf("GC pool malloc failed!\n");
        for ( ; ; );
    }

    mach_timer_alarm_preinit();
    pin_preinit();

    wifi_on_boot = config_get_wifi_on_boot();

soft_reset:

    // thread init
#if MICROPY_PY_THREAD
    mp_thread_init();
#endif

    // GC init
    gc_init((void *)gc_pool_upy, (void *)(gc_pool_upy + gc_pool_size));

    // MicroPython init
    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_init(mp_sys_argv, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_)); // current dir (or base dir of the script)

    // execute all basic initializations
    pin_init0();    // always before the rest of the peripherals
    mpexception_init0();
#if MICROPY_PY_THREAD
    mp_irq_init0();
#endif
    moduos_init0();
    uart_init0();
    mperror_init0();
    rng_init0();
    mp_hal_init(soft_reset);
    readline_init0();
    mod_network_init0();
    modbt_init0();
    machtimer_init0();
    modpycom_init0();
    bool safeboot = false;
    boot_info_t boot_info;
    uint32_t boot_info_offset;
    if (updater_read_boot_info (&boot_info, &boot_info_offset)) {
        safeboot = boot_info.safeboot;
    }
    if (!soft_reset) {
        if (wifi_on_boot) {
            mptask_enable_wifi_ap();
        }
        // these ones are special because they need uPy running and they launch tasks
#if defined(LOPY) || defined (LOPY4) || defined (FIPY)
        modlora_init0();
#endif
#if defined(SIPY) || defined(LOPY4) || defined (FIPY)
        modsigfox_init0();
#endif
    }

    // initialize the serial flash file system
    mptask_init_sflash_filesystem();

#if defined(LOPY) || defined(SIPY) || defined (LOPY4) || defined(FIPY)
    // must be done after initializing the file system
    mptask_update_lpwan_mac_address();
#endif

#if defined(SIPY) || defined(LOPY4) || defined(FIPY)
    sigfox_update_id();
    sigfox_update_pac();
    sigfox_update_private_key();
    sigfox_update_public_key();
#endif

    // append the flash paths to the system path
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_flash));
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_flash_slash_lib));

    // reset config variables; they should be set by boot.py
    MP_STATE_PORT(machine_config_main) = MP_OBJ_NULL;

    // enable telnet and ftp
    if (wifi_on_boot) {
        servers_start();
    }

    pyexec_frozen_module("_boot.py");

    if (!safeboot) {
    #if defined(GPY) || defined (FIPY)
        modlte_init0();
    #endif
        // run boot.py
        int ret = pyexec_file("boot.py");
        if (ret & PYEXEC_FORCED_EXIT) {
            goto soft_reset_exit;
        }
        if (!ret) {
            // flash the system led
            mperror_signal_error();
        }
    }

    if (!safeboot) {
        // execute the frozen main first
        pyexec_frozen_module("_main.py");

        // run the main script from the current directory.
        if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
            const char *main_py;
            if (MP_STATE_PORT(machine_config_main) == MP_OBJ_NULL) {
                main_py = "main.py";
            } else {
                main_py = mp_obj_str_get_str(MP_STATE_PORT(machine_config_main));
            }
            int ret = pyexec_file(main_py);
            if (ret & PYEXEC_FORCED_EXIT) {
                goto soft_reset_exit;
            }
            if (!ret) {
                // flash the system led
                mperror_signal_error();
            }
        }
    }

    // main script is finished, so now go into REPL mode.
    // the REPL mode can change, or it can request a soft reset.
    for ( ; ; ) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            if (pyexec_raw_repl() != 0) {
                break;
            }
        } else {
            if (pyexec_friendly_repl() != 0) {
                break;
            }
        }
    }

soft_reset_exit:

    machtimer_deinit();
#if MICROPY_PY_THREAD
    mp_irq_kill();
    mp_thread_deinit();
#endif
    mpsleep_signal_soft_reset();
    mp_printf(&mp_plat_print, "PYB: soft reboot\n");
    // it needs to be this one in order to not mess with the GIL
    ets_delay_us(5000);

    uart_deinit_all();
    // TODO: rmt_deinit_all();
    rmt_deinit_rgb();

    soft_reset = true;
    goto soft_reset;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void mptask_preinit (void) {
    wlan_pre_init();
    xTaskCreatePinnedToCore(TASK_Servers, "Servers", SERVERS_STACK_LEN, NULL, SERVERS_PRIORITY, &svTaskHandle, 1);
}

STATIC void mptask_init_sflash_filesystem (void) {
    FILINFO fno;

    // Initialise the local flash filesystem.
    // Create it if needed, and mount in on /flash.
    FRESULT res = f_mount(&sflash_fatfs, "/flash", 1);
    if (res == FR_NO_FILESYSTEM) {
        // no filesystem, so create a fresh one
        res = f_mkfs("/flash", FM_SFD | FM_FAT, 0, NULL, 0);
        if (res != FR_OK) {
            __fatal_error("failed to create /flash");
        }
        // create empty main.py
        mptask_create_main_py();
    }
    else if (res == FR_OK) {
        // mount sucessful
        if (FR_OK != f_stat("/flash/main.py", &fno)) {
            // create empty main.py
            mptask_create_main_py();
        }
    } else {
        __fatal_error("failed to create /flash");
    }

    // The current directory is used as the boot up directory.
    // It is set to the internal flash filesystem by default.
    f_chdrive("/flash");

    // create /flash/sys, /flash/lib and /flash/cert if they don't exist
    if (FR_OK != f_chdir ("/flash/sys")) {
        f_mkdir("/flash/sys");
    }
    if (FR_OK != f_chdir ("/flash/lib")) {
        f_mkdir("/flash/lib");
    }
    if (FR_OK != f_chdir ("/flash/cert")) {
        f_mkdir("/flash/cert");
    }

    f_chdir ("/flash");

    // make sure we have a /flash/boot.py. Create it if needed.
    res = f_stat("/flash/boot.py", &fno);
    if (res == FR_OK) {
        if (fno.fattrib & AM_DIR) {
            // exists as a directory
            // TODO handle this case
            // see http://elm-chan.org/fsw/ff/img/app2.c for a "rm -rf" implementation
        } else {
            // exists as a file, good!
        }
    } else {
        // doesn't exist, create fresh file
        FIL fp;
        f_open(&fp, "/flash/boot.py", FA_WRITE | FA_CREATE_ALWAYS);
        UINT n;
        f_write(&fp, fresh_boot_py, sizeof(fresh_boot_py) - 1 /* don't count null terminator */, &n);
        // TODO check we could write n bytes
        f_close(&fp);
    }
}

#if defined(LOPY) || defined(SIPY) || defined (LOPY4) || defined(FIPY)
STATIC void mptask_update_lpwan_mac_address (void) {
    #define LPWAN_MAC_ADDR_PATH          "/flash/sys/lpwan.mac"

    FILINFO fno;

    if (FR_OK == f_stat(LPWAN_MAC_ADDR_PATH, &fno)) {
        FIL fp;
        f_open(&fp, LPWAN_MAC_ADDR_PATH, FA_READ);
        UINT sz_out;
        uint8_t mac[8];
        FRESULT res = f_read(&fp, mac, sizeof(mac), &sz_out);
        if (res == FR_OK) {
            // file found, update the MAC address
            if (config_set_lpwan_mac(mac)) {
                mp_hal_delay_ms(250);
                ets_printf("LPWAN MAC write OK\n");
            } else {
                res = FR_DENIED;    // just anything different than FR_OK
            }
        }
        f_close(&fp);
        if (res == FR_OK) {
            // delete the mac address file
            f_unlink(LPWAN_MAC_ADDR_PATH);
        }
    }
}
#endif

STATIC void mptask_enable_wifi_ap (void) {
	uint8_t wifi_ssid[32];
	config_get_wifi_ssid(wifi_ssid);
	uint8_t wifi_pwd[64];
	config_get_wifi_pwd(wifi_pwd);
    wlan_setup (WIFI_MODE_AP, (wifi_ssid[0]==0x00) ? DEFAULT_AP_SSID : (const char*) wifi_ssid , WIFI_AUTH_WPA2_PSK, (wifi_pwd[0]==0x00) ? DEFAULT_AP_PASSWORD : (const char*) wifi_pwd ,
                DEFAULT_AP_CHANNEL, ANTENNA_TYPE_INTERNAL, (wifi_ssid[0]==0x00) ? true:false);
    mod_network_register_nic(&wlan_obj);
}

STATIC void mptask_create_main_py (void) {
    // create empty main.py
    FIL fp;
    f_open(&fp, "/flash/main.py", FA_WRITE | FA_CREATE_ALWAYS);
    UINT n;
    f_write(&fp, fresh_main_py, sizeof(fresh_main_py) - 1 /* don't count null terminator */, &n);
    f_close(&fp);
}

void stoupper (char *str) {
    while (str && *str != '\0') {
        *str = (char)toupper((int)(*str));
        str++;
    }
}

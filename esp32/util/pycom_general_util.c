/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>

#include "freertos/FreeRTOS.h"

#include "py/mpconfig.h"
#include "py/runtime.h"
#include "mptask.h"
#include "pycom_general_util.h"


/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define FILE_READ_SIZE                              256

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
char *pycom_util_read_file (const char *file_path, vstr_t *vstr) {
    vstr_init(vstr, FILE_READ_SIZE);
    char *filebuf = vstr->buf;
    mp_uint_t actualsize;
    mp_uint_t totalsize = 0;
    static const TCHAR *path_relative;
    // THis is needed to be able to clean the VSTR if any error occurred
    bool error = false;

    if(isLittleFs(file_path)) {

        vfs_lfs_struct_t* littlefs = lookup_path_littlefs(file_path, &path_relative);
        if (littlefs == NULL) {
            error = true;
        }
        else {

            lfs_file_t fp;

            xSemaphoreTake(littlefs->mutex, portMAX_DELAY);

            int res = lfs_file_open(&littlefs->lfs, &fp, path_relative, LFS_O_RDONLY);
            if(res < LFS_ERR_OK) {
                error = true;
            }
            else {
                while (error == false) {
                    actualsize = lfs_file_read(&littlefs->lfs, &fp, filebuf, FILE_READ_SIZE);
                    if (actualsize < LFS_ERR_OK) {
                        error = true;
                    }
                    else {
                        totalsize += actualsize;
                        if (actualsize < FILE_READ_SIZE) {
                            break;
                        } else {
                            filebuf = vstr_extend(vstr, FILE_READ_SIZE);
                        }
                    }
                }

                lfs_file_close(&littlefs->lfs, &fp);
            }

            xSemaphoreGive(littlefs->mutex);
        }
    }
    else
    {
        FATFS *fs = lookup_path_fatfs(file_path, &path_relative);
        if (fs == NULL) {
            error = true;
        }
        else {
            FIL fp;
            FRESULT res = f_open(fs, &fp, path_relative, FA_READ);
            if (res != FR_OK) {
                error = true;
            }
            else {
                while (error == false) {
                    FRESULT res = f_read(&fp, filebuf, FILE_READ_SIZE, (UINT *)&actualsize);
                    if (res != FR_OK) {
                        f_close(&fp);
                        error = true;
                    }
                    else {
                        totalsize += actualsize;
                        if (actualsize < FILE_READ_SIZE) {
                            break;
                        } else {
                            filebuf = vstr_extend(vstr, FILE_READ_SIZE);
                        }
                    }
                }
                f_close(&fp);
            }
        }
    }

    if(error == true) {
        vstr_clear(vstr);
        return NULL;
    }
    else {
        vstr->len = totalsize;
        vstr_null_terminated_str(vstr);
        return vstr->buf;
    }
}

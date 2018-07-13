import os
import sys
import argparse
import shutil



def main():
    cmd_parser = argparse.ArgumentParser(description='Get the precompiled libs from the IDF')
    cmd_parser.add_argument('--idflibs', default=None, help='the path to the idf libraries')
    cmd_args = cmd_parser.parse_args()

    src = cmd_args.idflibs

    # copy the bootloader libraries
    dst = os.getcwd() + '/bootloader/lib'

    shutil.copy(src + '/bootloader/bootloader_support/libbootloader_support.a', dst)
    shutil.copy(src + '/bootloader/log/liblog.a', dst)
    shutil.copy(src + '/bootloader/micro-ecc/libmicro-ecc.a', dst)
    shutil.copy(src + '/bootloader/soc/libsoc.a', dst)
    shutil.copy(src + '/bootloader/spi_flash/libspi_flash.a', dst)


    # copy the application libraries
    dst = os.getcwd() + '/lib'

    shutil.copy(src + '/bootloader_support/libbootloader_support.a', dst)
    shutil.copy(src + '/bt/libbt.a', dst)
    shutil.copy(src + '/cxx/libcxx.a', dst)
    shutil.copy(src + '/driver/libdriver.a', dst)
    shutil.copy(src + '/esp_adc_cal/libesp_adc_cal.a', dst)
    shutil.copy(src + '/esp32/libesp32.a', dst)
    shutil.copy(src + '/expat/libexpat.a', dst)
    shutil.copy(src + '/freertos/libfreertos.a', dst)
    shutil.copy(src + '/heap/libheap.a', dst)
    shutil.copy(src + '/jsmn/libjsmn.a', dst)
    shutil.copy(src + '/json/libjson.a', dst)
    shutil.copy(src + '/log/liblog.a', dst)
    shutil.copy(src + '/lwip/liblwip.a', dst)
    shutil.copy(src + '/mbedtls/libmbedtls.a', dst)
    shutil.copy(src + '/micro-ecc/libmicro-ecc.a', dst)
    shutil.copy(src + '/newlib/libnewlib.a', dst)
    shutil.copy(src + '/nghttp/libnghttp.a', dst)
    shutil.copy(src + '/nvs_flash/libnvs_flash.a', dst)
    shutil.copy(src + '/openssl/libopenssl.a', dst)
    shutil.copy(src + '/pthread/libpthread.a', dst)
    shutil.copy(src + '/sdmmc/libsdmmc.a', dst)
    shutil.copy(src + '/soc/libsoc.a', dst)
    shutil.copy(src + '/spi_flash/libspi_flash.a', dst)
    shutil.copy(src + '/tcpip_adapter/libtcpip_adapter.a', dst)
    shutil.copy(src + '/vfs/libvfs.a', dst)
    shutil.copy(src + '/wpa_supplicant/libwpa_supplicant.a', dst)
    shutil.copy(src + '/xtensa-debug-module/libxtensa-debug-module.a', dst)
    shutil.copy(src + '/openthread/libopenthread.a', dst)

if __name__ == "__main__":
    main()

import os
import sys
import argparse
import shutil
import traceback


def main():
    cmd_parser = argparse.ArgumentParser(description='Get the precompiled libs from the IDF')
    cmd_parser.add_argument('--idflibs', default=None, help='the path to the idf libraries')
    cmd_args = cmd_parser.parse_args()

    src = cmd_args.idflibs

    dsttmpbl = os.getcwd() + '/bootloader/lib/tmp'
    dstbl = os.getcwd() + '/bootloader/lib'
    dsttmpapp = os.getcwd() + '/lib/tmp'
    dstapp = os.getcwd() + '/lib'
    
    try:
        # copy the bootloader libraries
        
        os.mkdir(dsttmpbl)
        os.mkdir(dsttmpapp)
        
        shutil.copy(src + '/bootloader/bootloader_support/libbootloader_support.a', dsttmpbl)
        shutil.copy(src + '/bootloader/log/liblog.a', dsttmpbl)
        shutil.copy(src + '/bootloader/micro-ecc/libmicro-ecc.a', dsttmpbl)
        shutil.copy(src + '/bootloader/soc/libsoc.a', dsttmpbl)
        shutil.copy(src + '/bootloader/spi_flash/libspi_flash.a', dsttmpbl)
        
        # copy the application libraries
        
        shutil.copy(src + '/bootloader_support/libbootloader_support.a', dsttmpapp)
        shutil.copy(src + '/bt/libbt.a', dsttmpapp)
        shutil.copy(src + '/cxx/libcxx.a', dsttmpapp)
        shutil.copy(src + '/driver/libdriver.a', dsttmpapp)
        shutil.copy(src + '/esp_adc_cal/libesp_adc_cal.a', dsttmpapp)
        shutil.copy(src + '/esp32/libesp32.a', dsttmpapp)
        shutil.copy(src + '/smartconfig_ack/libsmartconfig_ack.a', dsttmpapp)
        shutil.copy(src + '/expat/libexpat.a', dsttmpapp)
        shutil.copy(src + '/freertos/libfreertos.a', dsttmpapp)
        shutil.copy(src + '/heap/libheap.a', dsttmpapp)
        shutil.copy(src + '/jsmn/libjsmn.a', dsttmpapp)
        shutil.copy(src + '/json/libjson.a', dsttmpapp)
        shutil.copy(src + '/log/liblog.a', dsttmpapp)
        shutil.copy(src + '/lwip/liblwip.a', dsttmpapp)
        shutil.copy(src + '/mbedtls/libmbedtls.a', dsttmpapp)
        shutil.copy(src + '/micro-ecc/libmicro-ecc.a', dsttmpapp)
        shutil.copy(src + '/newlib/libnewlib.a', dsttmpapp)
        shutil.copy(src + '/nghttp/libnghttp.a', dsttmpapp)
        shutil.copy(src + '/nvs_flash/libnvs_flash.a', dsttmpapp)
        shutil.copy(src + '/openssl/libopenssl.a', dsttmpapp)
        shutil.copy(src + '/pthread/libpthread.a', dsttmpapp)
        shutil.copy(src + '/sdmmc/libsdmmc.a', dsttmpapp)
        shutil.copy(src + '/soc/libsoc.a', dsttmpapp)
        shutil.copy(src + '/spi_flash/libspi_flash.a', dsttmpapp)
        shutil.copy(src + '/tcpip_adapter/libtcpip_adapter.a', dsttmpapp)
        shutil.copy(src + '/vfs/libvfs.a', dsttmpapp)
        shutil.copy(src + '/wpa_supplicant/libwpa_supplicant.a', dsttmpapp)
        shutil.copy(src + '/xtensa-debug-module/libxtensa-debug-module.a', dsttmpapp)
        shutil.copy(src + '/openthread/libopenthread.a', dsttmpapp)
        shutil.copy(src + '/esp_ringbuf/libesp_ringbuf.a', dsttmpapp)
    except:
        print("Couldn't Copy IDF libs defaulting to Local Lib Folders!")
        traceback.print_exc()
        shutil.rmtree(dsttmpbl)
        shutil.rmtree(dsttmpapp)
        return
    
    for item in os.listdir(dsttmpbl):
        shutil.copy(dsttmpbl+ '/' + item, dstbl + '/' + item)
        
    for item in os.listdir(dsttmpapp):
        shutil.copy(dsttmpapp + '/' + item, dstapp + '/' + item)
        
    shutil.rmtree(dsttmpbl)
    shutil.rmtree(dsttmpapp)
    
    print("IDF Libs copied Successfully!")
        

if __name__ == "__main__":
    main()

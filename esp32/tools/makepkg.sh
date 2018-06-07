#!/bin/bash
set -e
if [ -z $1 ]; then echo "Invalid board name!"; exit 1; fi
BOARD_NAME=$(echo $1 | tr '[IOY]' '[ioy]')
BOARD_NAME_L=$(echo ${BOARD_NAME} | tr '[A-Z]' '[a-z]')
VERSION=$(cat pycom_version.h |grep SW_VERSION_NUMBER | cut -d'"' -f2)
FILE_NAME="$BOARD_NAME-$VERSION.tar.gz"
cd build/$1/release
mkdir -p firmware_package && cd firmware_package
cp ../bootloader/bootloader.bin .
cp ../lib/partitions.bin .
cp ../${BOARD_NAME_L}.bin .
cat ../../../../tools/script | sed s/"appimg.bin"/"${BOARD_NAME_L}.bin"/g > ./script
tar -cvzf ../../../${FILE_NAME} bootloader.bin partitions.bin ${BOARD_NAME_L}.bin script
cd ..
rm -rf release_package

#!/bin/bash
REALPATH="realpath"
#set -x
set -e
if [ -z $1 ]; then echo >&2 "Invalid board name!"; exit 1; fi
if ! [ $0 = "tools/makepkg.sh" ]; then echo >&2 "Need to run as tools/makepkg.sh!"; exit 1; fi
if ! [ -r "pycom_version.h" ]; then echo >&2 "Cannot read pycom_version.h! Aborting."; exit 1; fi
${REALPATH} --help >/dev/null 2>&1 || { REALPATH="echo"; }
if [ -z $2 ]; then
  RELEASE_DIR="$(pwd)/build"
else
  RELEASE_DIR=$($REALPATH $2)
  if ! [ -d $RELEASE_DIR ]; then
    mkdir -p $RELEASE_DIR >/dev/null 2>&1 || { echo >&2 "Cannot create release directory! Aborting."; exit 1; }
  fi
  if ! [ -w $RELEASE_DIR ]; then
    echo >&2 "Cannot write to ${RELEASE_DIR}! Aborting."
    exit 1
  fi
fi
echo "Creating release package in ${RELEASE_DIR}"
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
tar -czf ${RELEASE_DIR}/${FILE_NAME} bootloader.bin partitions.bin ${BOARD_NAME_L}.bin script || { echo >&2 "Cannot create ${RELEASE_DIR}/${FILE_NAME}! Aborting."; exit 1; }
cd ..
echo "Release package ${FILE_NAME} created successfully."
rm -rf firmware_package

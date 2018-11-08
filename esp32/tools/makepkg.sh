#!/bin/bash
set -e
#set -x
if [ -z $1 ]; then echo >&2 "Invalid board name!"; exit 1; fi
if ! [ $0 = "tools/makepkg.sh" ]; then echo >&2 "Need to run as tools/makepkg.sh!"; exit 1; fi
if ! [ -d boards/$1 ]; then echo >&2 "Unknown board type!"; exit 1; fi
if ! [ -d build/$1 ]; then echo >&2 "Build directory for $1 not found! Run make BOARD=$1 first!"; exit 1; fi
if ! [ -r "pycom_version.h" ]; then echo >&2 "Cannot read pycom_version.h! Aborting."; exit 1; fi
if ! [ -r "tools/script2" ]; then echo >&2 "Cannot read tools/script2! Aborting."; exit 1; fi
if ! [ -r "tools/script" ]; then echo >&2 "Cannot read legacy tools/script! Aborting."; exit 1; fi
if [ -z $2 ]; then
  RELEASE_DIR="$(pwd)/build"
else
  RELEASE_DIR=$(realpath $2 2>/dev/null || echo $2)
  if ! [ -d $RELEASE_DIR ]; then
    mkdir -p $RELEASE_DIR >/dev/null 2>&1 || { echo >&2 "Cannot create release directory! Aborting."; exit 1; }
    RELEASE_DIR=$(realpath $2 || echo $2)
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
BUILD_DIR="build/$1/release"
PKG_TMP_DIR="${BUILD_DIR}/firmware_package"
mkdir -p ${PKG_TMP_DIR}
cp ${BUILD_DIR}/bootloader/bootloader.bin ${PKG_TMP_DIR}
cp ${BUILD_DIR}/lib/partitions.bin ${PKG_TMP_DIR}
cp ${BUILD_DIR}/${BOARD_NAME_L}.bin ${PKG_TMP_DIR}
cat tools/script2 | sed s/"appimg.bin"/"${BOARD_NAME_L}.bin"/g > ${PKG_TMP_DIR}/script2 || { echo >&2 "Cannot create script2 file! Aborting."; exit 1; }
cat tools/script | sed s/"appimg.bin"/"${BOARD_NAME_L}.bin"/g > ${PKG_TMP_DIR}/script || { echo >&2 "Cannot create legacy script file! Aborting."; exit 1; }
tar -czf ${RELEASE_DIR}/${FILE_NAME} -C ${PKG_TMP_DIR} bootloader.bin partitions.bin ${BOARD_NAME_L}.bin script2 script || { echo >&2 "Cannot create ${RELEASE_DIR}/${FILE_NAME}! Aborting."; exit 1; }
echo "Release package ${FILE_NAME} created successfully."
rm -rf ${PKG_TMP_DIR}/

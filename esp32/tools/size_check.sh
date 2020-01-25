#!/bin/bash

BOARD="$1"
RELEASE_TYP="$2"
VARIANT="$3"
BUILD_DIR="build"
IMG_MAX_SIZE_8MB=2027520
IMG_MAX_SIZE_4MB=1761280
OS="$(uname)"

#Script Has to be called from esp32 Dir
if ! [ $0 = "tools/size_check.sh" ]; then
  echo "Need to run as tools/size_check.sh!" >&2
  exit 1
fi

#Check Board Type
if [ "${BOARD}" != "WIPY" -a "${BOARD}" != "SIPY" -a "${BOARD}" != "LOPY" -a "${BOARD}" != "LOPY4" -a "${BOARD}" != "GPY" -a "${BOARD}" != "FIPY" -a "${BOARD}" != "TBEAMv1" ] ; then
  echo "Invalid Board name for MPY build!" >&2
  exit 1
fi

if [ "${VARIANT}" != "BASE" ] ; then
    BUILD_DIR="build-${VARIANT}"
fi

APP_BIN_PATH=./"${BUILD_DIR}"/"${BOARD}"/"${RELEASE_TYP}"/$(tr '[:upper:]' '[:lower:]'<<<${BOARD})".bin"
BOOT_BIN_PATH=./"${BUILD_DIR}"/"${BOARD}"/"${RELEASE_TYP}"/bootloader/bootloader.bin

if ! [ -f ${APP_BIN_PATH} ] ; then
  #Build Directory not created yet
  exit 0
fi

if ! [ -f ${BOOT_BIN_PATH} ] ; then
  #Build Directory not created yet
  exit 0
fi

if [ ${OS} = "Darwin" ] ; then
  size_app=$(stat -f%z ${APP_BIN_PATH})
  size_boot=$(stat -f%z ${BOOT_BIN_PATH})
 else
  size_app=$(stat -c%s ${APP_BIN_PATH})
  size_boot=$(stat -c%s ${BOOT_BIN_PATH})
fi

total_size=$((${size_app} + ${size_boot}))

if [ "${BOARD}" != "LOPY4" -a "${BOARD}" != "GPY" -a "${BOARD}" != "FIPY" ] ; then
  IMG_MAX_SIZE=${IMG_MAX_SIZE_4MB}
 else
  IMG_MAX_SIZE=${IMG_MAX_SIZE_8MB}
fi

if [ ${total_size} -gt ${IMG_MAX_SIZE} ] ; then
  echo "${total_size} bytes => Firmware image size exceeds avialable space on board!" >&2
  exit 1
else
  echo "${total_size} bytes => Size OK" >&2
fi
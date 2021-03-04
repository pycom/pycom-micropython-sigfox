#!/bin/bash

set -e

BOARD="$1"
RELEASE_TYP="$2"
VARIANT="$3"
if [ ${VARIANT} != "BASE" ] ; then
  BUILD_DIR="build-${VARIANT}"
else
  BUILD_DIR="build"
fi

if [ $4 -eq 1 ]; then
    IMG_MAX_SIZE_8MB=3076096
else
    IMG_MAX_SIZE_8MB=2027520
fi
# 4MB boards are deprecated
# IMG_MAX_SIZE_4MB=1761280

OS="$(uname)"

#Script Has to be called from esp32 Dir
if ! [ $0 = "tools/size_check.sh" ]; then
  echo "Need to run as tools/size_check.sh!" >&2
  exit 1
fi

#Check Board Type
if [ "${BOARD}" != "WIPY" -a "${BOARD}" != "SIPY" -a "${BOARD}" != "LOPY" -a "${BOARD}" != "LOPY4" -a "${BOARD}" != "GPY" -a "${BOARD}" != "FIPY" ] ; then
  echo "Invalid Board name for MPY build!" >&2
  exit 1
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


IMG_MAX_SIZE=${IMG_MAX_SIZE_8MB}

if [ ${total_size} -gt ${IMG_MAX_SIZE} ] ; then
  echo "${total_size} bytes => Firmware image size is NOT ok. It exceeds the available space ${IMG_MAX_SIZE} by $[ $total_size - $IMG_MAX_SIZE ]!" >&2
  exit 1
else
  echo "${total_size} bytes => Firmware image size is OK. It does not exceed the available space ${IMG_MAX_SIZE}. Still $[ $IMG_MAX_SIZE - $total_size ] left" >&2
fi

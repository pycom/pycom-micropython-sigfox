#!/bin/bash
set -e
#set -x
if [ -z $3 ]; then
  BUILD_DIR="$(pwd)/build/$1/release"
else
  BUILD_DIR="$(pwd)/$3"
fi
BOARD=$(echo $1)

if [ $4 ]; then
    SCRIPT_NAME_4MB="script_4MB_enc"
    SCRIPT_NAME_8MB="script_8MB_enc"
else
    SCRIPT_NAME_4MB="script_4MB"
    SCRIPT_NAME_8MB="script_8MB"
fi
if [ -z $1 ]; then echo >&2 "Invalid board name!"; exit 1; fi
if ! [ $0 = "tools/makepkg.sh" ]; then echo >&2 "Need to run as tools/makepkg.sh!"; exit 1; fi
if ! [ -d boards/$1 ]; then echo >&2 "Unknown board type!"; exit 1; fi
if ! [ -d ${BUILD_DIR} ]; then echo >&2 "Build directory for $1 not found! Run make BOARD=$1 first!"; exit 1; fi
if ! [ -r "pycom_version.h" ]; then echo >&2 "Cannot read pycom_version.h! Aborting."; exit 1; fi
if [ "${BOARD}" = "GPY" -o  "${BOARD}" = "LOPY4" -o "${BOARD}" = "FIPY" ]; then
    if ! [ -r "boards/$1/${SCRIPT_NAME_8MB}" ]; then echo >&2 "Cannot read boards/$1/${SCRIPT_NAME_8MB}! Aborting."; exit 1; fi
elif [ "${BOARD}" = "LOPY" -o "${BOARD}" = "WIPY" ]; then
    if ! [ -r "boards/$1/${SCRIPT_NAME_4MB}" ]; then echo >&2 "Cannot read boards/$1/${SCRIPT_NAME_4MB}! Aborting."; exit 1; fi
    if ! [ -r "boards/$1/${SCRIPT_NAME_8MB}" ]; then echo >&2 "Cannot read boards/$1/${SCRIPT_NAME_8MB}! Aborting."; exit 1; fi
else
    if ! [ -r "boards/$1/${SCRIPT_NAME_4MB}" ]; then echo >&2 "Cannot read boards/$1/${SCRIPT_NAME_4MB}! Aborting."; exit 1; fi
fi
if ! [ -r "boards/$1/script" ]; then echo >&2 "Cannot read legacy boards/$1/script! Aborting."; exit 1; fi
if ! [ -r "boards/$1/script2" ]; then echo >&2 "Cannot read legacy boards/$1/script2! Aborting."; exit 1; fi

if [ -z $2 ]; then
  RELEASE_DIR="$(pwd)/${BUILD_DIR}"
else
  RELEASE_DIR=$(realpath $2 2>/dev/null || echo $2)
      if ! [ -d $RELEASE_DIR ]; then
    mkdir -p $RELEASE_DIR >/dev/null 2>&1 || { echo >&2 "Cannot create release directory! Aborting."; exit 1; }
    RELEASE_DIR=$(realpath $2 2>/dev/null || echo $2)
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
PKG_TMP_DIR="${BUILD_DIR}/firmware_package"
mkdir -p ${PKG_TMP_DIR}

PART_FILE_4MB=''
PART_FILE_8MB=''
SCRIPT_FILE_4MB=''
SCRIPT_FILE_8MB=''
 
if [ $4 ]; then
    BOOT_FILE='bootloader-reflash-digest.bin_enc'
    APP_FILE="${BOARD_NAME_L}.bin_enc"
    FILE_NAME="${BOARD_NAME}-${VERSION}_ENC.tar.gz"
else
    BOOT_FILE='bootloader.bin'
    APP_FILE="${BOARD_NAME_L}.bin"
    FILE_NAME="$BOARD_NAME-$VERSION.tar.gz"
fi

cp ${BUILD_DIR}/bootloader/${BOOT_FILE} ${PKG_TMP_DIR}
cp ${BUILD_DIR}/${APP_FILE} ${PKG_TMP_DIR}

if [ "${BOARD}" != "SIPY" ]; then
    if [ $4 ]; then
        PART_FILE_8MB='partitions_8MB.bin_enc'
        SCRIPT_FILE_8MB='script_8MB_enc'
    else
        PART_FILE_8MB='partitions_8MB.bin'
        SCRIPT_FILE_8MB='script_8MB'
    fi
    
    cp ${BUILD_DIR}/lib/${PART_FILE_8MB} ${PKG_TMP_DIR}
    cat boards/$1/${SCRIPT_FILE_8MB} > ${PKG_TMP_DIR}/${SCRIPT_FILE_8MB} || { echo >&2 "Cannot create ${SCRIPT_FILE_8MB} file! Aborting."; exit 1; }

    if [ "${BOARD}" = "LOPY" -o "${BOARD}" = "WIPY" ]; then
       if [ $4 ]; then
            PART_FILE_4MB='partitions_4MB.bin_enc'
            SCRIPT_FILE_4MB='script_4MB_enc'
        else
            PART_FILE_4MB='partitions_4MB.bin'
            SCRIPT_FILE_4MB='script_4MB'
        fi
        
        cp ${BUILD_DIR}/lib/${PART_FILE_4MB} ${PKG_TMP_DIR}
        cat boards/$1/${SCRIPT_FILE_4MB} > ${PKG_TMP_DIR}/${SCRIPT_FILE_4MB} || { echo >&2 "Cannot create ${SCRIPT_FILE_4MB} file! Aborting."; exit 1; }
    fi
else
   if [ $4 ]; then
        PART_FILE_4MB='partitions_4MB.bin_enc'
        SCRIPT_FILE_4MB='script_4MB_enc'
    else
        PART_FILE_4MB='partitions_4MB.bin'
        SCRIPT_FILE_4MB='script_4MB'
    fi
    
    cp ${BUILD_DIR}/lib/${PART_FILE_4MB} ${PKG_TMP_DIR}
    cat boards/$1/${SCRIPT_FILE_4MB} > ${PKG_TMP_DIR}/${SCRIPT_FILE_4MB} || { echo >&2 "Cannot create ${SCRIPT_FILE_4MB} file! Aborting."; exit 1; }

fi
cat boards/$1/script  > ${PKG_TMP_DIR}/script  || { echo >&2 "Cannot create legacy script file! Aborting."; exit 1; }
cat boards/$1/script2  > ${PKG_TMP_DIR}/script2  || { echo >&2 "Cannot create legacy script2 file! Aborting."; exit 1; }
tar -czf ${RELEASE_DIR}/${FILE_NAME} -C ${PKG_TMP_DIR} ${BOOT_FILE} ${PART_FILE_4MB} ${PART_FILE_8MB} ${APP_FILE} ${SCRIPT_FILE_4MB} ${SCRIPT_FILE_8MB} script script2 || { echo >&2 "Cannot create ${RELEASE_DIR}/${FILE_NAME}! Aborting."; exit 1; }
echo "Release package ${FILE_NAME} created successfully."

rm -rf ${PKG_TMP_DIR}/

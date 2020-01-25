#!/bin/bash

BOARD="$1"
RELEASE_TYP="$2"
VARIANT="$3"
PY_PATH="./frozen"
PY_DIRS="$(ls ${PY_PATH})"
OS="$(uname)"
if [ ${VARIANT} != "BASE" ] ; then
    BUILD_DIR="build-${VARIANT}"
else
    BUILD_DIR="build"
fi

#Script Has to be called from esp32 Dir
if ! [ $0 = "tools/mpy-build-check.sh" ]; then
  echo "Need to run as tools/mpy-build-check.sh!" >&2
  exit 1
fi

#Check Board Type
if [ "${BOARD}" != "WIPY" -a "${BOARD}" != "SIPY" -a "${BOARD}" != "LOPY" -a "${BOARD}" != "LOPY4" -a "${BOARD}" != "GPY" -a "${BOARD}" != "FIPY" -a "${BOARD}" != "TBEAMv1" ] ; then
  echo "Invalid Board name for MPY build!" >&2
  exit 1
fi

BUILD_TIMESTAMP=./"${BUILD_DIR}"/${BOARD}"/"${RELEASE_TYP}"/"mpy_last_build_timestamp.TS

#If Last mpy Build Timestamp Not avialable create it
if [ ! -d ${BUILD_DIR}/${BOARD}/${RELEASE_TYP} ] ; then
    exit 0
else
    if [ ! -f  ${BUILD_TIMESTAMP} ] ; then
        $(touch ${BUILD_TIMESTAMP})
    fi
fi

#Get Current Timestamp
CURR_TS="$(date +"%s")"

MPY_PATH=./"${BUILD_DIR}"/"${BOARD}"/"${RELEASE_TYP}"/frozen_mpy

if ! [ -d ${MPY_PATH} ] ; then
  #Build Directory not created yet
  #Update Last Build Timestamp
  $(echo ${CURR_TS} > ${BUILD_TIMESTAMP})
  exit 0
fi

LAST_BUILD=$(<${BUILD_TIMESTAMP})

#Check if any of Frozen Directorys has been updated.. Rebuild out Mpy files
for dir in ${PY_DIRS}
do
  if [[ "${dir}" =~ ^\\.* ]] ; then
    continue
  fi

  if [ ${OS} = "Darwin" ] ; then
    LAST_MOD="$(stat -f "%Sm" ${PY_PATH}"/"${dir})"
    TS="$(date -j -f "%b %d %T %Y" "${LAST_MOD}" +"%s")"
  else
    TS=$(stat -c %Y ${PY_PATH}"/"${dir})
  fi

  if [[ ${TS} -gt ${LAST_BUILD} ]] ; then
    echo "Rebuilding frozen Code!" >&2
    #Remove all MPY out files to be rubuild again by Makefile
    $(rm -rf ${MPY_PATH})
    #Update Last Build Timestamp
    $(echo ${CURR_TS} > ${BUILD_TIMESTAMP})
    exit 0
  fi
done

#Update Last Build Timestamp
$(echo ${CURR_TS} > ${BUILD_TIMESTAMP})
exit 0

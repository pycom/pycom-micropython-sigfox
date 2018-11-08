#!/bin/bash

BOARD="$1"
RELEASE_TYP="$2"
PY_PATH="./frozen"
PY_DIRS="$(ls ${PY_PATH})"
OS="$(uname)"

#Script Has to be called from esp32 Dir
if ! [ $0 = "tools/mpy-build-check.sh" ]; then
  echo "Need to run as tools/mpy-build-check.sh!" >&2
  exit 1
fi

#Check Board Type
if [ "${BOARD}" != "WIPY" -a "${BOARD}" != "SIPY" -a "${BOARD}" != "LOPY" -a "${BOARD}" != "LOPY4" -a "${BOARD}" != "GPY" -a "${BOARD}" != "FIPY" ] ; then
  echo "Invalid Board name for MPY build!" >&2
  exit 1
fi

MPY_PATH=./build/"${BOARD}"/"${RELEASE_TYP}"/frozen_mpy

if ! [ -d ${MPY_PATH} ] ; then
  #Build Directory not created yet
  exit 0
fi

BUILD_TIMESTAMP=./build/${BOARD}"/"${RELEASE_TYP}"/"mpy_last_build_timestamp.TS

#If Last mpy Build Timestamp Not avialable create it
if [ ! -f  ${BUILD_TIMESTAMP} ] ; then
  $(touch ${BUILD_TIMESTAMP})
fi

LAST_BUILD=$(<${BUILD_TIMESTAMP})
#Get Current Timestamp
CURR_TS="$(date +"%s")"

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
    echo "Number of Frozen Code files changed!" >&2
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

#!/bin/bash
# Check whether we should rebuild the frozen Micro Python code

set -e

BOARD="$1"
RELEASE_TYP="$2"
VARIANT="$3"
PY_PATH="./frozen"
PY_DIRS="$(ls ${PY_PATH})"
OS="$(uname)"
if [ ${VARIANT} != "BASE" ] ; then
  BUILD_DIR="build-${VARIANT}/${BOARD}/${RELEASE_TYP}"
else
  BUILD_DIR="build/${BOARD}/${RELEASE_TYP}"
fi

# Script has to be called from esp32 dir
if ! [ $0 = "tools/mpy-build-check.sh" ]; then
  echo "Need to run as tools/mpy-build-check.sh!" >&2
  exit 1
fi

# Check board type
if [ "${BOARD}" != "WIPY" -a "${BOARD}" != "SIPY" -a "${BOARD}" != "LOPY" -a "${BOARD}" != "LOPY4" -a "${BOARD}" != "GPY" -a "${BOARD}" != "FIPY" ] ; then
  echo "Invalid Board name for MPY build!" >&2
  exit 1
fi


# Get current timestamp
CURR_TS="$(date +"%s")"


BUILD_TIMESTAMP="${BUILD_DIR}/mpy_last_build_timestamp.TS"


MPY_PATH="${BUILD_DIR}/frozen_mpy"
if ! [ -d ${MPY_PATH} ] ; then
  # Build directory does not exist
  # Update last build timestamp
  mkdir -p ${BUILD_DIR}
  echo ${CURR_TS} > ${BUILD_TIMESTAMP}
  exit 0
fi


# Get last build timestamp
if [ ! -f  ${BUILD_TIMESTAMP} ] ; then
    LAST_BUILD=0
else
    LAST_BUILD=$(<${BUILD_TIMESTAMP})
fi


# Remove Mpy build directory if any of the frozen directories have been updated
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
    # Remove the build directory to trigger a rebuild from the Makefile
    rm -rf ${MPY_PATH}
    # Update last build timestamp
    echo ${CURR_TS} > ${BUILD_TIMESTAMP}
    exit 0
  fi
done

# Update last build timestamp
echo ${CURR_TS} > ${BUILD_TIMESTAMP}
exit 0

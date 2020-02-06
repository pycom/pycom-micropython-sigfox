#!/bin/bash
#
# Copyright (c) 2020, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

IDF_VER="idf_v"$2
CURR_VER="$(git --git-dir=$1/.git branch | grep \* | cut -d ' ' -f2)"

if [ "${CURR_VER}" = "${IDF_VER}" ]; then
    echo "IDF Version OK!"
    exit 0
else
    echo "Incompatible IDF version...Checking out IDF version $2!"
    if ! git --git-dir=$1/.git --work-tree=$1 checkout ${IDF_VER} ; then
        echo "Cannot checkout IDF version ${IDF_VER}!...Please make sure latest idf_v${IDF_VER} branch is fetched" >&2
        exit 1
    fi
    cd ${IDF_PATH}
    if ! git submodule sync ; then
        echo "Cannot checkout IDF version ${IDF_VER}!...Please make sure latest idf_v${IDF_VER} branch is fetched" >&2
        exit 1
    fi
    if ! git submodule update --init ; then
        echo "Cannot checkout IDF version ${IDF_VER}!...Please make sure latest idf_v${IDF_VER} branch is fetched" >&2
        exit 1
    fi
    exit 0
fi
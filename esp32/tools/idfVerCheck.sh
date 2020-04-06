#!/bin/bash
#
# Copyright (c) 2020, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

IDF_VER=$2
IDF_PATH=$1
CURR_VER=$(git --git-dir=$IDF_PATH/.git rev-parse HEAD)

if [ "${CURR_VER}" = "${IDF_VER}" ]; then
    echo "IDF Version OK!"
    exit 0
else
    echo "Incompatible IDF version... Expected $IDF_VER, but $IDF_PATH is pointing at $CURR_VER"
        exit 1
fi

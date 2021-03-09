#!/bin/bash
#
# Copyright (c) 2021, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

IDF_HASH=$2
IDF_PATH=$1
CURR_HASH=$(git -c core.abbrev=7 --git-dir=$IDF_PATH/.git rev-parse --short HEAD)

if [ "${CURR_HASH}" = "${IDF_HASH}" ]; then
    echo "IDF Version OK! $IDF_HASH"
    exit 0
else
    echo "
Incompatible IDF git hash:

$IDF_HASH is expected from IDF_HASH from Makefile, but
$CURR_HASH is what IDF_PATH=$IDF_PATH is pointing at.

You should probably update one (or multiple) of:
  * IDF_PATH environment variable
  * IDF_HASH variable in esp32/Makefile
  * IDF commit, e.g.
cd \$IDF_PATH && git checkout $IDF_HASH && git submodule sync && git submodule update --init --recursive && cd -

"
    exit 1
fi

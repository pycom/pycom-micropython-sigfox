#!/bin/bash
#
# Copyright (c) 2021, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

if [ "$#" -ne 2 ]; then
    echo "Usage: appsign.sh *app bin* *build dir*"
    exit 1
fi

# Build location
APP_BIN=$1

# Build location
BUILD=$2

# Generate the MD5 hash
# md5 on Darwin, md5sum on Unix
if [ `uname -s` = "Darwin" ]; then
echo -n `md5 -q $APP_BIN` > $BUILD/__md5hash.bin
else
echo -n `md5sum --binary $APP_BIN | awk '{ print $1 }'` > $BUILD/__md5hash.bin
fi

# Concatenate it with the application binary
cat $APP_BIN $BUILD/__md5hash.bin > $BUILD/appimg.bin
RET=$?

# Remove the tmp files
rm -f $BUILD/__md5hash.bin

exit $RET

#!/bin/bash

CUD=$PWD
SRC=$CUD/..
DST=~/DABstarAppImage/build
CMFLAG=-DCONV_IN_FILES=OFF #switch off .in-files conversion as repo access could only be read-only
QtPath=-DCMAKE_PREFIX_PATH=/home/workvm/fs_Qt/6.7.3/gcc_64/
echo "Source folder is $SRC"
echo "Destination folder is $DST"
rm -rf $DST
mkdir -p $DST
cd $DST || exit

cmake $SRC $CMFLAG $QtPath -DUSE_LTO=ON -DAIRSPY=ON -DSDRPLAY_V2=ON -DSDRPLAY_V3=ON -DHACKRF=ON -DLIMESDR=ON -DRTL_TCP=ON -DPLUTO=ON -DUHD=ON -DRTLSDR=ON -DUSE_HBF=OFF -DDATA_STREAMER=OFF -DVITERBI_SSE=ON -DVITERBI_NEON=OFF -DFDK_AAC=ON

if [ $? -eq 0 ]; then
    echo "cmake runs successfully, now we begin to build the application"
    $CUD/build_only.sh
else
    echo "cmake failed with $?"
fi
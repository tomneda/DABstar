#!/bin/bash

QT_VERSION=6.8.3
CUD=$PWD
SRC=$CUD/..
DST=~/DABstarAppImage/build
CMFLAG=-DCONV_IN_FILES=OFF #switch off .in-files conversion as repo access could only be read-only
QtPath=-DCMAKE_PREFIX_PATH=/home/work/fs_Qt/$QT_VERSION/gcc_64/
echo "Source folder is $SRC"
echo "Destination folder is $DST"
rm -rf $DST
mkdir -p $DST
cd $DST || exit

cmake $SRC $CMFLAG $QtPath -DSSE_OR_AVX=ON -DUSE_LTO=ON -DAIRSPY=ON -DSDRPLAY_V2=ON -DSDRPLAY_V3=ON -DHACKRF=ON -DLIMESDR=ON -DRTL_TCP=ON -DPLUTO=ON -DUHD=ON -DRTLSDR=ON -DSPYSERVER=ON -DUSE_LIQUID=ON -DDATA_STREAMER=OFF -DVITERBI_SSE2=ON -DVITERBI_NEON=OFF -DFDK_AAC=ON -DSOAPY=ON

if [ $? -eq 0 ]; then
    echo "cmake runs successfully, now we begin to build the application"
    $CUD/build_only.sh
else
    echo "cmake failed with $?"
fi
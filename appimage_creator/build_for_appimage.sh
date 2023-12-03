#!/bin/bash

SRC=$PWD/..
DST=~/DABstarAppImage/build
echo "Source folder is $SRC"
echo "Destination folder is $DST"
rm -rf $DST
mkdir -p $DST
cd $DST || exit

cmake $SRC -DAIRSPY=ON -DSDRPLAY_V2=ON -DSDRPLAY_V3=ON -DHACKRF=ON -DLIMESDR=ON -DRTL_TCP=ON -DPLUTO=ON -DUHD=ON -DRTLSDR_LINUX=ON -DUSE_HBF=OFF -DDATA_STREAMER=OFF -DVITERBI_SSE=ON -DVITERBI_NEON=OFF -DFDK_AAC=OFF

if [ $? -eq 0 ]; then
    echo "cmake runs successfully, now we begin to build the application"
    cmake --build . -- -j4
else
    echo "cmake failed with $?"
fi
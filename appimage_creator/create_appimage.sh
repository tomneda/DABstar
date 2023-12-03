#!/bin/bash

DST=~/DABstarAppImage

mkdir -p $DST

mkdir -p $DST/appdir/usr/bin
mkdir -p $DST/appdir/usr/share/applications
mkdir -p $DST/appdir/usr/share/icons/hicolor/128x128/apps/
mkdir -p $DST/appdir/usr/share/icons/hicolor/256x256/apps/

cp dabstar.desktop            $DST/appdir/usr/share/applications
cp ../res/dabstar128x128.png  $DST/appdir/
cp ../res/dabstar256x256.png  $DST/appdir/
cp ../res/dabstar128x128.png  $DST/appdir/usr/share/icons/hicolor/128x128/apps/
cp ../res/dabstar256x256.png  $DST/appdir/usr/share/icons/hicolor/256x256/apps/

cd $DST || exit

# from here the build folder with the build has to be exist in the folder $DST
cp build/dabstar appdir/usr/bin/

wget -q --show-progress -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt*.AppImage

./linuxdeployqt*.AppImage ./appdir/usr/share/applications/* -appimage -no-translations


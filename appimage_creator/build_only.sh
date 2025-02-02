#!/bin/bash

DST=~/DABstarAppImage/build
cd $DST || exit

cmake --build . -- -j6

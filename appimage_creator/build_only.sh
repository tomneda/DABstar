#!/bin/bash
set -euo pipefail

DST=~/DABstarAppImage/build
cd "$DST"

cmake --build . -- -j$(nproc)

#!/bin/sh
set -x -e

mkdir /deps
cd /deps
git clone https://github.com/PJK/libcbor
sed -e 's/-flto//' -i libcbor/CMakeLists.txt
cmake -DCMAKE_BUILD_TYPE=Release -DCBOR_CUSTOM_ALLOC=ON libcbor
make
make install

# Build distribution

/edgex-c-sdk/scripts/build.sh $*

# Set ownership of generated files

chown -R $UID:$GID /edgex-c-sdk/build

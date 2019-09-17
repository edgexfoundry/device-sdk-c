#!/bin/sh
set -x -e

CBOR_VERSION=0.5.0

mkdir /deps
cd /deps
wget -O - https://github.com/PJK/libcbor/archive/v${CBOR_VERSION}.tar.gz | tar -z -x -f -
sed -e 's/-flto//' -i libcbor-${CBOR_VERSION}/CMakeLists.txt
cmake -DCMAKE_BUILD_TYPE=Release -DCBOR_CUSTOM_ALLOC=ON libcbor-${CBOR_VERSION}
make
make install

# Build distribution

/edgex-c-sdk/scripts/build.sh $*

mkdir -p /edgex-c-sdk/results/debug
cp /edgex-c-sdk/build/debug/c/libcsdk.so /edgex-c-sdk/results/debug
cp /edgex-c-sdk/build/release/csdk-*.tar.gz /edgex-c-sdk/results
cp /edgex-c-sdk/build/release/release.log /edgex-c-sdk/results

# Set ownership of generated files

chown -R $UID:$GID /edgex-c-sdk/results

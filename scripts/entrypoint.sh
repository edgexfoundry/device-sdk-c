#!/bin/sh
set -x -e

# Build distribution

/edgex-c-sdk/scripts/build.sh $*

mkdir -p /edgex-c-sdk/results/debug
cp /edgex-c-sdk/build/debug/c/libcsdk.so /edgex-c-sdk/results/debug
cp /edgex-c-sdk/build/release/csdk-*.tar.gz /edgex-c-sdk/results
cp /edgex-c-sdk/build/release/release.log /edgex-c-sdk/results

# Set ownership of generated files

chown -R $UID:$GID /edgex-c-sdk/results

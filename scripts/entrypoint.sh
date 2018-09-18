#!/bin/sh
set -x -e

# Build distribution

/edgex-c-sdk/scripts/build.sh $*

# Set ownership of generated files

chown -R $UID:$GID /edgex-c-sdk/build

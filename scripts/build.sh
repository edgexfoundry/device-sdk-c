#!/bin/sh
set -e -x

CPPCHECK=false
DOCGEN=false

TOMLVER=SDK-0.2
CUTILVER=1.1.4

# Process arguments

while [ $# -gt 0 ]
do
  case $1 in
    -cppcheck)
      CPPCHECK=true
      shift 1
    ;;
    -doxygen)
      DOCGEN=true
      shift 1
    ;;
    *)
      shift 1
    ;;
  esac
done

# Find root directory and system type

ROOT=$(dirname $(dirname $(readlink -f $0)))
cd $ROOT

# Dependencies

if [ ! -d deps ]
then
  mkdir deps

  # TOML Parser

  wget -O - https://github.com/IOTechSystems/tomlc99/archive/$TOMLVER.tar.gz | tar -C deps -z -x -f -
  cp deps/tomlc99-$TOMLVER/toml.* src/c

  # C Utils
  
  wget -O - https://github.com/IOTechSystems/iotech-c-utils/archive/v$CUTILVER.tar.gz | tar -C deps -z -x -f -
  mkdir -p src/c/iot
  cp deps/iotech-c-utils-$CUTILVER/src/c/*.c src/c/iot
  cp deps/iotech-c-utils-$CUTILVER/src/c/defs.h.in src/c/iot
  cp deps/iotech-c-utils-$CUTILVER/src/c/yxml.h src/c/iot
  cp deps/iotech-c-utils-$CUTILVER/VERSION src/c/iot
  mkdir -p include/iot
  cp deps/iotech-c-utils-$CUTILVER/include/iot/*.h include/iot
  mkdir -p include/iot/os
  cp deps/iotech-c-utils-$CUTILVER/include/iot/os/* include/iot/os

fi

# Cmake release build

mkdir -p $ROOT/build/release
cd $ROOT/build/release
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release $ROOT/src
make all package 2>&1 | tee release.log

# Run cppcheck if configured

if [ "$CPPCHECK" = "true" ]
then
  echo cppcheck --project=compile_commands.json --xml-version=2 --enable=style --output-file=cppcheck.xml
fi

# Run doxygen if configured

if [ "$DOCGEN" = "true" ]
then
  cd $ROOT
  doxygen
fi

# Cmake debug build

mkdir -p $ROOT/build/debug
cd $ROOT/build/debug
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug $ROOT/src
make 2>&1 | tee debug.log

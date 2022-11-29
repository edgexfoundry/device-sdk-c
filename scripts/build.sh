#!/bin/sh
set -e -x

CPPCHECK=false
DOCGEN=false
CMAKEOPTS=-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

TOMLVER=SDK-0.2
CUTILVER=1.4

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
    -legacyv2)
      CMAKEOPTS="$CMAKEOPTS -DCSDK_LEGACY_ARRAYS=ON"
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

fi

# Cmake release build

mkdir -p $ROOT/build/release
cd $ROOT/build/release
cmake $CMAKEOPTS -DCMAKE_BUILD_TYPE=Release $ROOT/src
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
cmake $CMAKEOPTS -DCMAKE_BUILD_TYPE=Debug $ROOT/src
make 2>&1 | tee debug.log

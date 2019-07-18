#!/bin/sh
set -e -x

CPPCHECK=false
DOCGEN=false

IOTECH=IOTechSystems
CUTILNAME=iotech-c-utils
CUTILREF=1a0be9f
CUTILDIR=$IOTECH-$CUTILNAME-$CUTILREF

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

  wget -O - https://github.com/IOTechSystems/tomlc99/archive/SDK-0.2.tar.gz | tar -C deps -z -x -f -
  cp deps/tomlc99-SDK-0.2/toml.* src/c

  # C Utils
  
  wget -O - https://github.com/$IOTECH/$CUTILNAME/tarball/$CUTILREF | tar -C deps -z -x -f -
  mkdir -p src/c/iot
  cp deps/$CUTILDIR/src/c/scheduler.c src/c/iot
  cp deps/$CUTILDIR/src/c/logger.c src/c/iot
  cp deps/$CUTILDIR/src/c/threadpool.c src/c/iot
  cp deps/$CUTILDIR/src/c/thread.c src/c/iot
  cp deps/$CUTILDIR/src/c/data.c src/c/iot
  cp deps/$CUTILDIR/src/c/json.c src/c/iot
  cp deps/$CUTILDIR/src/c/container.c src/c/iot
  mkdir -p include/iot
  cat deps/$CUTILDIR/include/iot/scheduler.h | sed -e 's/1000000000L/1000000000ULL/' > include/iot/scheduler.h
  cp deps/$CUTILDIR/include/iot/logger.h include/iot
  cp deps/$CUTILDIR/include/iot/threadpool.h include/iot
  cp deps/$CUTILDIR/include/iot/component.h include/iot
  cp deps/$CUTILDIR/include/iot/data.h include/iot
  cp deps/$CUTILDIR/include/iot/thread.h include/iot
  cp deps/$CUTILDIR/include/iot/container.h include/iot
  cp deps/$CUTILDIR/include/iot/json.h include/iot
  cp deps/$CUTILDIR/include/iot/os.h include/iot
  mkdir -p include/iot/os
  cp deps/$CUTILDIR/include/iot/os/* include/iot/os

fi

# Cmake release build

mkdir -p $ROOT/build/release
cd $ROOT/build/release
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release $ROOT/src
make 2>&1 | tee release.log
make package>&1 | tee -a release.log

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
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCSDK_BUILD_DEBUG=ON -DCMAKE_BUILD_TYPE=Debug $ROOT/src
make 2>&1 | tee debug.log

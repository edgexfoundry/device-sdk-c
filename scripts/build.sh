#!/bin/sh
set -e -x

CPPCHECK=false

# Process arguments

while [ $# -gt 0 ]
do
  case $1 in
    -cppcheck)
      CPPCHECK=true
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

  wget -O - https://github.com/IOTechSystems/tomlc99/archive/SDK-0.1.tar.gz | tar -C deps -z -x -f -
  cp deps/tomlc99-SDK-0.1/toml.c src/c
  cp deps/tomlc99-SDK-0.1/toml.h include/edgex
  
  # Thread Pool
  
  wget -O - https://github.com/IOTechSystems/C-Thread-Pool/archive/SDK-0.1.tar.gz | tar -C deps -z -x -f -
  cp deps/C-Thread-Pool-SDK-0.1/thpool.c src/c
  cp deps/C-Thread-Pool-SDK-0.1/thpool.h include/
  
  # C Utils
  
  wget -O - https://github.com/IOTechSystems/iotech-c-utils/archive/v0.1.2.tar.gz | tar -C deps -z -x -f -
  cp deps/iotech-c-utils-0.1.2/src/c/scheduler.c src/c/scheduler.c
  cp deps/iotech-c-utils-0.1.2/src/c/logging.c src/c/logging.c
  mkdir -p include/iot
  cp deps/iotech-c-utils-0.1.2/include/iot/scheduler.h include/iot/scheduler.h
  cp deps/iotech-c-utils-0.1.2/include/iot/logging.h include/iot/logging.h
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

# Cmake debug build

mkdir -p $ROOT/build/debug
cd $ROOT/build/debug
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCSDK_BUILD_DEBUG=ON -DCMAKE_BUILD_TYPE=Debug $ROOT/src
make 2>&1 | tee debug.log

#!/bin/bash
set -e

CLEAN=false
BUILD_TYPE="Release"
BUILD_ROOT="build"

while getopts "cdh" opt; do
  case ${opt} in
	c )
  	    CLEAN=true
  	    ;;
	d)
  	    BUILD_TYPE="Debug"
  	    ;;      
    h )
        echo "Usage: $0 [-c] [-d]"
        echo "  -c   Clean build directory"
        echo "  -d   Debug build"
        exit 0
        ;;
    \? )
        echo "Invalid option: $OPTARG" 1>&2
        exit 1
        ;;
  esac
done

BUILD_DIR=$BUILD_ROOT/$BUILD_TYPE

if [ "$CLEAN" = true ]; then
    echo "Removing $BUILD_ROOT"
    rm -rf "$BUILD_ROOT"
fi

echo "Configuring $BUILD_DIR..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "Building $BUILD_DIR..."
cmake --build "$BUILD_DIR" -j

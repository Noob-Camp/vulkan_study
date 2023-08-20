#!/bin/bash

if [ -d "./build" ]; then
    rm -rf ./build
    echo "the build directory is deleted successfully."
fi

cmake -B build -DCMAKE_BUILD_TYPE=Debug
# cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

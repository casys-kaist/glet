#!/bin/bash

cp ../CMakeLists.txt ../CMakeLists-temp.txt
cp ../CMakeLists-scheduler-only.txt ../CMakeLists.txt

BUILD_DIR=$PWD/../bin

mkdir -p $BUILD_DIR

cd $BUILD_DIR

cmake  -DCMAKE_BUILD_TYPE=Debug ..

make -j 32

cp ../CMakeLists-temp.txt ../CMakeLists.txt

#!/bin/bash

LIB_TORCH_DIR=$PWD/../libtorch

BUILD_DIR=$PWD/../bin

mkdir -p $BUILD_DIR
cd $BUILD_DIR

cmake -DCMAKE_PREFIX_PATH=$LIB_TORCH_DIR -DCMAKE_BUILD_TYPE=Debug ..

make -j 32

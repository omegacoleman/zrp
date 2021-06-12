#!/bin/bash

# use homebrew toolchain
brew update
brew install llvm

# build & install the project
mkdir -p build install
cmake -B ./build -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_TOOLCHAIN_FILE=$(pwd)/ci/brew-llvm.toolchain.cmake
cmake --build ./build --config $BUILD_TYPE
cmake --install ./build --config $BUILD_TYPE

# package the binary
mkdir -p output
tar -C ./install/bin -czvf ./output/zrp-darwin-$BUILD_TYPE-$(git rev-parse --short HEAD).tar.gz zclient zserver


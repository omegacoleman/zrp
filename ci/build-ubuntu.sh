#!/bin/bash

# setup cmake & g++-11
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update
sudo apt-get install -y build-essential software-properties-common git
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get install -y g++-11 cmake
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 --slave /usr/bin/g++ g++ /usr/bin/g++-11 --slave /usr/bin/gcov gcov /usr/bin/gcov-11

# build & install the project
mkdir -p build install
cmake -B ./build -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_TOOLCHAIN_FILE=$(pwd)/ci/gcc-static.toolchain.cmake
cmake --build ./build --config $BUILD_TYPE
cmake --install ./build --config $BUILD_TYPE

# package the binary
mkdir -p output
tar -C ./install/bin -czvf ./output/zrp-linux-$BUILD_TYPE-$(git rev-parse --short HEAD).tar.gz zclient zserver


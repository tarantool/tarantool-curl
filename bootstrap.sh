#!/bin/bash

## Curl
cd third_party/curl
git checkout curl-7_52_1
cd -

mkdir third_party/curl/build
cd third_party/curl/build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCURL_STATICLIB=ON \
      -DENABLE_ARES=OFF ../
make libcurl -j2
cd -

## Self
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DTP_CURL=ON \
      ../
make -j2
cd -

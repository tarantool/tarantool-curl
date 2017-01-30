#!/bin/bash

set -e -x

cp -f build/curl/driver.so curl/driver.so
tarantool tests/example.lua
tarantool tests/bugs.lua

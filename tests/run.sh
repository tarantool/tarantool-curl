#!/bin/bash

set -e -x

# Don't copy since we build into the root [[[
#cp -f build/curl/driver.so curl/driver.so
# ]]]

tarantool tests/example.lua
tarantool tests/bugs.lua
tarantool tests/async.lua
tarantool tests/func_tests.lua
./tests/server.js &
tarantool tests/load.lua
tarantool tests/new_func.lua
kill -s TERM %1

echo '[+] OK'

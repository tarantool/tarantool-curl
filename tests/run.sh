#!/bin/bash

set -e -x

# Don't copy since we build into the root [[[
#cp -f build/curl/driver.so curl/driver.so
# ]]]

tarantool tests/example.lua
tarantool tests/bugs.lua
tarantool tests/async.lua
./tests/server.js &
tarantool tests/load.lua
kill -s TERM %1

echo '[+] OK'

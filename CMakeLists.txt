cmake_minimum_required(VERSION 2.8)

project(tarantool-curl C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})


# Set CFLAGS
set(MY_C_FLAGS "-Wall -Wextra -Werror -pedantic -std=gnu99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${MY_C_FLAGS}")

find_package(Tarantool REQUIRED)
find_package(CURL REQUIRED)
find_package(Lua)

include_directories(${CURL_INCLUDE_DIRS}
                    ${TARANTOOL_INCLUDE_DIRS}
                    /usr/local/include)

message(STATUS "curl: ${CURL_INCLUDE_DIRS} ${CURL_LIBRARIES}")
message(STATUS "tarantool: ${TARANTOOL_INCLUDE_DIRS}")
message(STATUS "lua: ${LUA_INCLUDE_DIR} ${LUA_LIBRARIES}")

# Build module
add_subdirectory(curl)

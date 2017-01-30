cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include(CMakeDependentOption)

project(tarantool-curl C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Set CFLAGS
set(MY_C_FLAGS "-Wall -Wextra -Werror -std=gnu11 -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${MY_C_FLAGS}")

find_package(Tarantool REQUIRED)
find_package(CURL REQUIRED)
find_package(LibEV REQUIRED)
find_package(Lua)

message(STATUS "curl: ${CURL_INCLUDE_DIRS} ${CURL_LIBRARIES}")
message(STATUS "tarantool: ${TARANTOOL_INCLUDE_DIRS}")
message(STATUS "lua: ${LUA_INCLUDE_DIR} ${LUA_LIBRARIES}")
message(STATUS "libev: ${LIBEV_INCLUDE_DIR} ${LIBEV_LIBRARIES}")

include_directories(${CURL_INCLUDE_DIRS}
                    ${TARANTOOL_INCLUDE_DIRS}
                    ${LIBEV_INCLUDE_DIR} )

# Build module
add_subdirectory(curl)

add_custom_target(test
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh
                  DEPENDS driver)

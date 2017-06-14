cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include(CMakeDependentOption)
include(ExternalProject)

project(tarantool-curl C)

set(CMAKE_MODULE_PATH  "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Set CFLAGS
set(MY_C_FLAGS "-Wall -Wextra -Werror -std=gnu11 -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${MY_C_FLAGS}")

find_package(Tarantool REQUIRED)
find_package(LibEV REQUIRED)

option(WITH_SYSTEM_CURL "Use system curl, if it's available" ON)
include(BuildLibCURL)
build_libcurl_if_needed()

message(STATUS "tarantool:       ${TARANTOOL_INCLUDE_DIRS}")
message(STATUS "libev includes:  ${LIBEV_INCLUDE_DIR} ")
message(STATUS "libev libraries: ${LIBEV_LIBRARIES} ")
message(STATUS "curl includes:   ${CURL_INCLUDE_DIRS} ")
message(STATUS "curl libraries:  ${CURL_LIBRARIES} ")

include_directories(${CURL_INCLUDE_DIRS}
                    ${TARANTOOL_INCLUDE_DIRS}
                    ${LIBEV_INCLUDE_DIR} )

add_custom_target(test
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh
                  DEPENDS driver)

# Build module
add_subdirectory(curl)

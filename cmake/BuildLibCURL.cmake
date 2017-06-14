macro(build_libcurl)
    set(OPENSSL_ARG "")
    # Latest versions of Homebrew wont 'link --force' for libraries, that were
    # preinstalled in system. So we'll use this dirty hack
    if(APPLE)
        find_program(HOMEBREW_EXECUTABLE brew)
        if(EXISTS ${HOMEBREW_EXECUTABLE})
            execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix
                            OUTPUT_VARIABLE HOMEBREW_PREFIX
                            OUTPUT_STRIP_TRAILING_WHITESPACE)
            message(STATUS "Detected Homebrew install at ${HOMEBREW_PREFIX}")

            # Detecting OpenSSL
            execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix openssl
                            OUTPUT_VARIABLE HOMEBREW_OPENSSL
                            OUTPUT_STRIP_TRAILING_WHITESPACE)
            if (DEFINED HOMEBREW_OPENSSL)
                if (NOT DEFINED OPENSSL_ROOT_DIR)
                    message(STATUS "Setting OpenSSL root to ${HOMEBREW_OPENSSL}")
                    set(OPENSSL_ROOT_DIR "${HOMEBREW_OPENSSL}")
                endif()
                # set(OPENSSL_ARG " -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}")
            elseif(NOT DEFINED OPENSSL_ROOT_DIR)
                message(WARNING "Homebrew's OpenSSL isn't installed. Work isn't "
                                "guarenteed if built with system OpenSSL")
            endif()
        endif()
    endif()

    set(LIBCURL "${CMAKE_CURRENT_BINARY_DIR}/third_party/curl-out")

    ExternalProject_Add(libcurl_project
        PREFIX     "${CMAKE_CURRENT_BINARY_DIR}/third_party/.curl.tmp"
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/curl"
        CMAKE_ARGS -DCURL_STATICLIB=ON
                   -DHTTP_ONLY=TRUE
                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                   -DCMAKE_INSTALL_PREFIX=${LIBCURL}
                   -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
                   -DBUILD_CURL_EXE=OFF
                   -DBUILD_TESTING=OFF
                   -DENABLE_ARES=ON
                   -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

    add_library(libcurl STATIC IMPORTED)
    set_target_properties(libcurl PROPERTIES IMPORTED_LOCATION "${LIBCURL}/lib/libcurl.a")
    add_dependencies(libcurl libcurl_project)

    find_package(CARES REQUIRED)

    # finally, set paths
    set(CURL_LIBRARIES    libcurl ${CARES_LIBRARY})
    set(CURL_INCLUDE_DIRS "${LIBCURL}/include")
endmacro()

macro(build_libcurl_if_needed)
    if(WITH_SYSTEM_CURL)
        find_package(CURL)
        if (NOT CURL_FOUND)
            message(STATUS "Failed to find pre-installed libCURL")
        endif()
    endif()

    if (NOT CURL_FOUND)
        message(STATUS "Using bundled libCURL")
        build_libcurl()
    endif()
endmacro()

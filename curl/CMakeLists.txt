# Driver

add_library(driver SHARED curl_wrapper.c
                          request_pool.c
                          driver.c )

if (APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined suppress -flat_namespace -rdynamic")
endif(APPLE)

target_link_libraries(driver ${CURL_LIBRARIES} ${LIBEV_LIBRARIES})

set_target_properties(driver PROPERTIES PREFIX "" OUTPUT_NAME "driver")

install(TARGETS driver LIBRARY DESTINATION ${TARANTOOL_INSTALL_LIBDIR}/curl)
install(FILES init.lua DESTINATION ${TARANTOOL_INSTALL_LUADIR}/curl)

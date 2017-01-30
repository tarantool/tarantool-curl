# Driver
add_library(driver SHARED curl_wrapper.c
                          driver.c )
target_link_libraries(driver ${CURL_LIBRARIES}
                             ${LIBEV_LIBRARIES}
                             -rdynamic )
set_target_properties(driver PROPERTIES PREFIX "" OUTPUT_NAME "driver")

install(TARGETS driver LIBRARY DESTINATION ${TARANTOOL_INSTALL_LIBDIR}/curl)
install(FILES init.lua DESTINATION ${TARANTOOL_INSTALL_LUADIR}/curl)

## My test!
#if (LUA_FOUND)
#  include_directories(${LUA_INCLUDE_DIR})
#
#  add_executable(unit_tests curl_wrapper.c
#                            unit_tests.c )
#
#  target_link_libraries(unit_tests ${CURL_LIBRARIES}
#                                   ${LIBEV_LIBRARIES}
#                                   ${LUA_LIBRARIES} )
#endif (LUA_FOUND)

add_definitions(-DSOURCE_VERSION="${SOURCE_VERSION}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${ALFREDJSON_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR} ${ZLIB_INCLUDE_DIRS} ${JANSSON_INCLUDE_DIRS})
link_directories(${JANSSON_LIBRARY_DIRS} ${ZLIB_LIBRARY_DIRS})

add_executable(alfred-json main.c output_string.c output_json.c output_binary.c zcat.c)

set_target_properties(alfred-json PROPERTIES COMPILE_FLAGS "-std=gnu99 -Wall -pedantic" LINK_FLAGS "")
target_link_libraries(alfred-json ${ZLIB_LIBRARIES} ${JANSSON_LIBRARIES})

install(TARGETS alfred-json RUNTIME DESTINATION bin)

cmake_minimum_required(VERSION 2.8.3)
project(ALFREDJSON C)
set(SOURCE_VERSION "v0.3.1")
execute_process(COMMAND sh -c "git describe --always --dirty --tags || echo ${SOURCE_VERSION}"
		OUTPUT_VARIABLE SOURCE_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED zlib)
pkg_check_modules(JANSSON REQUIRED jansson)

set(CMAKE_MODULE_PATH ${ALFREDJSON_SOURCE_DIR})

add_subdirectory(src)

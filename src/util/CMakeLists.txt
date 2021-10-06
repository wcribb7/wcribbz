list(APPEND UTIL_INCLUDES
	"${COMMON_INCLUDES}"
	"${libgit2_BINARY_DIR}/src"
	"${libgit2_SOURCE_DIR}/src/util")

# Collect sourcefiles
file(GLOB SRC_UTIL_H
	"${libgit2_SOURCE_DIR}/include/git2.h"
	"${libgit2_SOURCE_DIR}/include/git2/*.h"
	"${libgit2_SOURCE_DIR}/include/git2/sys/*.h")
list(SORT SRC_UTIL_H)

file(GLOB SRC_UTIL alloc.c futils.c hash.c net.c pool.c posix.c runtime.c str.c strmap.c tsort.c util.c varint.c vector.c allocators/*.c allocators/*.h)

#file(GLOB SRC_UTIL *.c *.h allocators/*.c allocators/*.h)
list(SORT SRC_UTIL)

if(WIN32 AND NOT CYGWIN)
	file(GLOB SRC_UTIL_OS win32/*.c win32/*.h)
	list(SORT SRC_UTIL_OS)
else()
	file(GLOB SRC_UTIL_OS unix/*.c unix/*.h)
	list(SORT SRC_UTIL_OS)
endif()

# Utility functions shared with other parts of the libgit2
set(UTIL_SOURCES ${SRC_UTIL} ${SRC_UTIL_OS})
add_library(util OBJECT ${UTIL_SOURCES})
list(APPEND UTIL_OBJECTS ${COMMON_OBJECTS})
list(APPEND UTIL_OBJECTS $<TARGET_OBJECTS:util>)

get_property(LIBGIT2_INCLUDES GLOBAL PROPERTY libgit2_includes)
get_property(LIBGIT2_SYSTEM_INCLUDES GLOBAL PROPERTY libgit2_system_includes)

include_directories(${LIBGIT2_INCLUDES})
include_directories(SYSTEM ${LIBGIT2_SYSTEM_INCLUDES})

target_include_directories(util PRIVATE ${UTIL_INCLUDES} PUBLIC ${libgit2_SOURCE_DIR}/include)
target_include_directories(util SYSTEM PRIVATE ${UTIL_SYSTEM_INCLUDES})

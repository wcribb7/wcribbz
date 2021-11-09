# util: a shared library for common utility functions for libgit2 projects

add_library(util OBJECT)
set_target_properties(util PROPERTIES C_STANDARD 90)

set(UTIL_INCLUDES
	"${CMAKE_BINARY_DIR}/src"
	"${libgit2_SOURCE_DIR}/src/util"
	"${libgit2_SOURCE_DIR}/include")

file(GLOB SRC_UTIL *.c *.h allocators/*.c allocators/*.h)
list(SORT SRC_UTIL)
target_sources(util PRIVATE ${SRC_UTIL})

#
# Platform specific sources
#

if(WIN32 AND NOT CYGWIN)
	set(WIN_RC "win32/git2.rc")

	file(GLOB SRC_OS win32/*.c win32/*.h)
	list(SORT SRC_OS)
	target_sources(util PRIVATE ${SRC_OS})
elseif(NOT AMIGA)
	file(GLOB SRC_OS unix/*.c unix/*.h)
	list(SORT SRC_OS)
	target_sources(util PRIVATE ${SRC_OS})
endif()

if(AMIGA)
	target_compile_definitions(util PRIVATE NO_ADDRINFO NO_READDIR_R NO_MMAP)
endif()

#
# Hash backend selection
#

if(USE_SHA1 STREQUAL "CollisionDetection")
	file(GLOB SRC_HASH hash/sha1/collisiondetect.* hash/sha1/sha1dc/*)
elseif(USE_SHA1 STREQUAL "OpenSSL")
	file(GLOB SRC_HASH hash/sha1/openssl.*)
elseif(USE_SHA1 STREQUAL "CommonCrypto")
	file(GLOB SRC_HASH hash/sha1/common_crypto.*)
elseif(USE_SHA1 STREQUAL "mbedTLS")
	file(GLOB SRC_HASH hash/sha1/mbedtls.*)
elseif(USE_SHA1 STREQUAL "Win32")
	file(GLOB SRC_HASH hash/sha1/win32.*)
elseif(USE_SHA1 STREQUAL "Generic")
	file(GLOB SRC_HASH hash/sha1/generic.*)
else()
	message(FATAL_ERROR "Asked for unknown SHA1 backend: ${USE_SHA1}")
endif()

list(APPEND SRC_HASH "hash/sha1.h")
list(SORT SRC_HASH)

#
# Include builtin dependencies
#

if(NOT (USE_HTTP_PARSER STREQUAL "system"))
	list(APPEND UTIL_INCLUDES "${libgit2_SOURCE_DIR}/deps/http-parser")
	list(APPEND UTIL_OBJECTS "$<TARGET_OBJECTS:http-parser>")
endif()

if(REGEX_BACKEND STREQUAL "builtin")
	list(APPEND UTIL_INCLUDES "${libgit2_SOURCE_DIR}/deps/pcre")
	list(APPEND UTIL_OBJECTS $<TARGET_OBJECTS:pcre>)
endif()

if(USE_BUNDLED_ZLIB STREQUAL "Chromium")
	list(APPEND UTIL_INCLUDES "${libgit2_SOURCE_DIR}/deps/chromium-zlib")
	list(APPEND UTIL_OBJECTS $<TARGET_OBJECTS:chromium_zlib>)
elseif(USE_BUNDLED_ZLIB)
	list(APPEND UTIL_INCLUDES "${libgit2_SOURCE_DIR}/deps/zlib")
	list(APPEND UTIL_OBJECTS $<TARGET_OBJECTS:zlib>)
endif()

#
# Preprocessor definitions
#

if(DEPRECATE_HARD)
	add_definitions(-DGIT_DEPRECATE_HARD)
endif()

if(USE_LEAK_CHECKER STREQUAL "valgrind")
	target_compile_definitions(util PRIVATE VALGRIND)
endif()

target_sources(util PRIVATE ${SRC_UTIL} ${SRC_OS} ${SRC_HASH})

target_include_directories(util PRIVATE ${UTIL_INCLUDES} PUBLIC ${libgit2_SOURCE_DIR}/include)
target_include_directories(util SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

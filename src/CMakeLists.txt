include(PkgBuildConfig)
include(SanitizeBool)


# This variable will contain the libraries we need to put into
# libgit2.pc's Requires.private. That is, what we're linking to or
# what someone who's statically linking us needs to link to.
set(LIBGIT2_PC_REQUIRES "")
# This will be set later if we use the system's http-parser library or
# use iconv (OSX) and will be written to the Libs.private field in the
# pc file.
set(LIBGIT2_PC_LIBS "")

set(LIBGIT2_INCLUDES
        "${libgit2_BINARY_DIR}/src"
	"${libgit2_SOURCE_DIR}/src/util"
	"${libgit2_SOURCE_DIR}/src/libgit2"
	"${libgit2_SOURCE_DIR}/include")
set(LIBGIT2_SYSTEM_INCLUDES "")
set(LIBGIT2_LIBS "")


# Development / debugging options

if(DEBUG_POOL)
	set(GIT_DEBUG_POOL 1)
endif()
add_feature_info(debugpool GIT_DEBUG_POOL "debug pool allocator")

if(DEBUG_STRICT_ALLOC)
	set(GIT_DEBUG_STRICT_ALLOC 1)
endif()
add_feature_info(debugalloc GIT_DEBUG_STRICT_ALLOC "debug strict allocators")

if(DEBUG_STRICT_OPEN)
	set(GIT_DEBUG_STRICT_OPEN 1)
endif()
add_feature_info(debugopen GIT_DEBUG_STRICT_OPEN "path validation in open")

# Platform support

if(HAVE_FUTIMENS)
	set(GIT_USE_FUTIMENS 1)
endif ()
add_feature_info(futimens GIT_USE_FUTIMENS "futimens support")

check_prototype_definition(qsort_r
	"void qsort_r(void *base, size_t nmemb, size_t size, void *thunk, int (*compar)(void *, const void *, const void *))"
	"" "stdlib.h" HAVE_QSORT_R_BSD)
if(HAVE_QSORT_R_BSD)
	set(GIT_USE_QSORT_R_BSD 1)
endif()

check_prototype_definition(qsort_r
	"void qsort_r(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg)"
	"" "stdlib.h" HAVE_QSORT_R_GNU)
if(HAVE_QSORT_R_GNU)
	set(GIT_USE_QSORT_R_GNU 1)
endif()

check_function_exists(qsort_s HAVE_QSORT_S)
if(HAVE_QSORT_S)
	set(GIT_USE_QSORT_S 1)
endif()

# Add system dependendencies

if(WIN32)
	list(APPEND LIBGIT2_LIBS ws2_32)
elseif(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
	list(APPEND LIBGIT2_LIBS socket nsl)
	list(APPEND LIBGIT2_PC_LIBS "-lsocket" "-lnsl")
elseif(CMAKE_SYSTEM_NAME MATCHES "Haiku")
	list(APPEND LIBGIT2_LIBS network)
	list(APPEND LIBGIT2_PC_LIBS "-lnetwork")
endif()

check_library_exists(rt clock_gettime "time.h" NEED_LIBRT)
if(NEED_LIBRT)
	list(APPEND LIBGIT2_LIBS rt)
	list(APPEND LIBGIT2_PC_LIBS "-lrt")
endif()

if(THREADSAFE)
	list(APPEND LIBGIT2_LIBS ${CMAKE_THREAD_LIBS_INIT})
	list(APPEND LIBGIT2_PC_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()
add_feature_info(threadsafe THREADSAFE "threadsafe support")


if(WIN32 AND EMBED_SSH_PATH)
	file(GLOB SRC_SSH "${EMBED_SSH_PATH}/src/*.c")
	list(SORT SRC_SSH)
	target_sources(libgit2 PRIVATE ${SRC_SSH})

	list(APPEND LIBGIT2_SYSTEM_INCLUDES "${EMBED_SSH_PATH}/include")
	file(WRITE "${EMBED_SSH_PATH}/src/libssh2_config.h" "#define HAVE_WINCNG\n#define LIBSSH2_WINCNG\n#include \"../win32/libssh2_config.h\"")
	set(GIT_SSH 1)
endif()

if(WIN32 AND WINHTTP)
	set(GIT_WINHTTP 1)

	# Since MinGW does not come with headers or an import library for winhttp,
	# we have to include a private header and generate our own import library
	if(MINGW)
		add_subdirectory("${libgit2_SOURCE_DIR}/deps/winhttp" "${libgit2_BINARY_DIR}/deps/winhttp")
		list(APPEND LIBGIT2_LIBS winhttp)
		list(APPEND LIBGIT2_INCLUDES "${libgit2_SOURCE_DIR}/deps/winhttp")
	else()
		list(APPEND LIBGIT2_LIBS "winhttp")
		list(APPEND LIBGIT2_PC_LIBS "-lwinhttp")
	endif()

	list(APPEND LIBGIT2_LIBS "rpcrt4" "crypt32" "ole32")
	list(APPEND LIBGIT2_PC_LIBS "-lrpcrt4" "-lcrypt32" "-lole32")
endif()

include(SelectHTTPSBackend)

# Specify regular expression implementation

find_package(PCRE)

if(REGEX_BACKEND STREQUAL "")
	check_symbol_exists(regcomp_l "regex.h;xlocale.h" HAVE_REGCOMP_L)

	if(HAVE_REGCOMP_L)
		set(REGEX_BACKEND "regcomp_l")
	elseif(PCRE_FOUND)
		set(REGEX_BACKEND "pcre")
	else()
		set(REGEX_BACKEND "builtin")
	endif()
endif()

if(REGEX_BACKEND STREQUAL "regcomp_l")
	add_feature_info(regex ON "using system regcomp_l")
	set(GIT_REGEX_REGCOMP_L 1)
elseif(REGEX_BACKEND STREQUAL "pcre2")
	find_package(PCRE2)

	if(NOT PCRE2_FOUND)
		MESSAGE(FATAL_ERROR "PCRE2 support was requested but not found")
	endif()

	add_feature_info(regex ON "using system PCRE2")
	set(GIT_REGEX_PCRE2 1)

	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${PCRE2_INCLUDE_DIRS})
	list(APPEND LIBGIT2_LIBS ${PCRE2_LIBRARIES})
	list(APPEND LIBGIT2_PC_REQUIRES "libpcre2-8")
elseif(REGEX_BACKEND STREQUAL "pcre")
	add_feature_info(regex ON "using system PCRE")
	set(GIT_REGEX_PCRE 1)

	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${PCRE_INCLUDE_DIRS})
	list(APPEND LIBGIT2_LIBS ${PCRE_LIBRARIES})
	list(APPEND LIBGIT2_PC_REQUIRES "libpcre")
elseif(REGEX_BACKEND STREQUAL "regcomp")
	add_feature_info(regex ON "using system regcomp")
	set(GIT_REGEX_REGCOMP 1)
elseif(REGEX_BACKEND STREQUAL "builtin")
	add_feature_info(regex ON "using bundled PCRE")
	set(GIT_REGEX_BUILTIN 1)

	add_subdirectory("${libgit2_SOURCE_DIR}/deps/pcre" "${libgit2_BINARY_DIR}/deps/pcre")
	list(APPEND LIBGIT2_INCLUDES "${libgit2_SOURCE_DIR}/deps/pcre")
	list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:pcre>)
else()
	message(FATAL_ERROR "The REGEX_BACKEND option provided is not supported")
endif()

# Optional external dependency: http-parser
if(USE_HTTP_PARSER STREQUAL "system")
	find_package(HTTP_Parser)

	if(HTTP_PARSER_FOUND AND HTTP_PARSER_VERSION_MAJOR EQUAL 2)
		list(APPEND LIBGIT2_SYSTEM_INCLUDES ${HTTP_PARSER_INCLUDE_DIRS})
		list(APPEND LIBGIT2_LIBS ${HTTP_PARSER_LIBRARIES})
		list(APPEND LIBGIT2_PC_LIBS "-lhttp_parser")
		add_feature_info(http-parser ON "http-parser support (system)")
	else()
		message(FATAL_ERROR "http-parser support was requested but not found")
	endif()
else()
	message(STATUS "http-parser version 2 was not found or disabled; using bundled 3rd-party sources.")
	add_subdirectory("${libgit2_SOURCE_DIR}/deps/http-parser" "${libgit2_BINARY_DIR}/deps/http-parser")
	list(APPEND LIBGIT2_INCLUDES "${libgit2_SOURCE_DIR}/deps/http-parser")
	list(APPEND LIBGIT2_OBJECTS "$<TARGET_OBJECTS:http-parser>")
	add_feature_info(http-parser ON "http-parser support (bundled)")
endif()

# Optional external dependency: zlib
SanitizeBool(USE_BUNDLED_ZLIB)
if(USE_BUNDLED_ZLIB STREQUAL ON)
	set(USE_BUNDLED_ZLIB "Bundled")
endif()

if(USE_BUNDLED_ZLIB STREQUAL "OFF")
	find_package(ZLIB)
	if(ZLIB_FOUND)
		list(APPEND LIBGIT2_SYSTEM_INCLUDES ${ZLIB_INCLUDE_DIRS})
		list(APPEND LIBGIT2_LIBS ${ZLIB_LIBRARIES})
		if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
			list(APPEND LIBGIT2_PC_LIBS "-lz")
		else()
			list(APPEND LIBGIT2_PC_REQUIRES "zlib")
		endif()
		add_feature_info(zlib ON "using system zlib")
	else()
		message(STATUS "zlib was not found; using bundled 3rd-party sources." )
	endif()
endif()

if(USE_BUNDLED_ZLIB STREQUAL "Chromium")
	add_subdirectory("${libgit2_SOURCE_DIR}/deps/chromium-zlib" "${libgit2_BINARY_DIR}/deps/chromium-zlib")
	list(APPEND LIBGIT2_INCLUDES "${libgit2_SOURCE_DIR}/deps/chromium-zlib")
	list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:chromium_zlib>)
	add_feature_info(zlib ON "using (Chromium) bundled zlib")
elseif(USE_BUNDLED_ZLIB OR NOT ZLIB_FOUND)
	add_subdirectory("${libgit2_SOURCE_DIR}/deps/zlib" "${libgit2_BINARY_DIR}/deps/zlib")
	list(APPEND LIBGIT2_INCLUDES "${libgit2_SOURCE_DIR}/deps/zlib")
	list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:zlib>)
	add_feature_info(zlib ON "using bundled zlib")
endif()

# Optional external dependency: libssh2
if(USE_SSH)
	find_pkglibraries(LIBSSH2 libssh2)
	if(NOT LIBSSH2_FOUND)
		find_package(LibSSH2)
		set(LIBSSH2_INCLUDE_DIRS ${LIBSSH2_INCLUDE_DIR})
		get_filename_component(LIBSSH2_LIBRARY_DIRS "${LIBSSH2_LIBRARY}" DIRECTORY)
		set(LIBSSH2_LIBRARIES ${LIBSSH2_LIBRARY})
		set(LIBSSH2_LDFLAGS "-lssh2")
	endif()
endif()
if(LIBSSH2_FOUND)
	set(GIT_SSH 1)
	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${LIBSSH2_INCLUDE_DIRS})
	list(APPEND LIBGIT2_LIBS ${LIBSSH2_LIBRARIES})
	list(APPEND LIBGIT2_PC_LIBS ${LIBSSH2_LDFLAGS})

	check_library_exists("${LIBSSH2_LIBRARIES}" libssh2_userauth_publickey_frommemory "${LIBSSH2_LIBRARY_DIRS}" HAVE_LIBSSH2_MEMORY_CREDENTIALS)
	if(HAVE_LIBSSH2_MEMORY_CREDENTIALS)
		set(GIT_SSH_MEMORY_CREDENTIALS 1)
	endif()
else()
	message(STATUS "LIBSSH2 not found. Set CMAKE_PREFIX_PATH if it is installed outside of the default search path.")
endif()
add_feature_info(SSH GIT_SSH "SSH transport support")

# Optional external dependency: ntlmclient
if(USE_NTLMCLIENT)
	set(GIT_NTLM 1)
	add_subdirectory("${libgit2_SOURCE_DIR}/deps/ntlmclient" "${libgit2_BINARY_DIR}/deps/ntlmclient")
	list(APPEND LIBGIT2_INCLUDES "${libgit2_SOURCE_DIR}/deps/ntlmclient")
	list(APPEND LIBGIT2_OBJECTS "$<TARGET_OBJECTS:ntlmclient>")
endif()
add_feature_info(ntlmclient GIT_NTLM "NTLM authentication support for Unix")

# Optional external dependency: GSSAPI
include(SelectGSSAPI)

# Optional external dependency: iconv
if(USE_ICONV)
	find_package(Iconv)
endif()
if(ICONV_FOUND)
	set(GIT_USE_ICONV 1)
	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${ICONV_INCLUDE_DIR})
	list(APPEND LIBGIT2_LIBS ${ICONV_LIBRARIES})
	list(APPEND LIBGIT2_PC_LIBS ${ICONV_LIBRARIES})
endif()
add_feature_info(iconv GIT_USE_ICONV "iconv encoding conversion support")

set_property(GLOBAL PROPERTY libgit2_includes ${LIBGIT2_INCLUDES})
set_property(GLOBAL PROPERTY libgit2_system_includes ${LIBGIT2_SYSTEM_INCLUDES})
set_property(GLOBAL PROPERTY libgit2_libs ${LIBGIT2_LIBS})

configure_file(features.h.in generated/features.h)

add_subdirectory(util)
add_subdirectory(libgit2)

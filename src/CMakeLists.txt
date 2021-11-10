#
# Optional debugging functionality
#

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

#
# Optional feature enablement
#

include(SelectGSSAPI)
include(SelectHTTPSBackend)
include(SelectRegex)
include(SelectZlib)
include(SelectSsh)
include(SelectHttpParser)
include(SelectWinHttp)

# nanosecond support in index

if(USE_NSEC)
	set(GIT_USE_NSEC 1)
	add_feature_info(nsec GIT_USE_NSEC "nanosecond resolution in file times")
endif()

# optional external dependency: pthreads

if(USE_THREADS)
	if(NOT WIN32)
		find_package(Threads REQUIRED)
		list(APPEND LIBGIT2_SYSTEM_LIBS ${CMAKE_THREAD_LIBS_INIT})
		list(APPEND LIBGIT2_PC_LIBS ${CMAKE_THREAD_LIBS_INIT})
	endif()

	set(GIT_THREADS 1)
endif()
add_feature_info(threadsafe GIT_THREADS "threadsafe support")

# optional external dependency: iconv

if(USE_ICONV)
	find_package(Iconv REQUIRED)
	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${ICONV_INCLUDE_DIR})
	list(APPEND LIBGIT2_SYSTEM_LIBS ${ICONV_LIBRARIES})
	list(APPEND LIBGIT2_PC_LIBS ${ICONV_LIBRARIES})
	set(GIT_USE_ICONV 1)
endif()
add_feature_info(iconv GIT_USE_ICONV "iconv encoding conversion support")

# optional bundled dependency: ntlmclient

if(USE_NTLMCLIENT)
	set(GIT_NTLM 1)
	add_subdirectory("${libgit2_SOURCE_DIR}/deps/ntlmclient" "${libgit2_BINARY_DIR}/deps/ntlmclient")
	list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${libgit2_SOURCE_DIR}/deps/ntlmclient")
	list(APPEND LIBGIT2_DEPENDENCY_OBJECTS "$<TARGET_OBJECTS:ntlmclient>")
endif()
add_feature_info(ntlmclient GIT_NTLM "NTLM authentication support for Unix")

#
# Platform support identification
#

# futimes/futimens

if(HAVE_FUTIMENS)
	set(GIT_USE_FUTIMENS 1)
endif()

# qsort

check_prototype_definition(qsort_r
	"void qsort_r(void *base, size_t nmemb, size_t size, void *thunk, int (*compar)(void *, const void *, const void *))"
	"" "stdlib.h" GIT_QSORT_R_BSD)

check_prototype_definition(qsort_r
	"void qsort_r(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg)"
	"" "stdlib.h" GIT_QSORT_R_GNU)

check_function_exists(qsort_s GIT_QSORT_S)

# Determine architecture of the machine

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(GIT_ARCH_64 1)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(GIT_ARCH_32 1)
elseif(CMAKE_SIZEOF_VOID_P)
	message(FATAL_ERROR "Unsupported architecture (pointer size is ${CMAKE_SIZEOF_VOID_P} bytes)")
else()
	message(FATAL_ERROR "Unsupported architecture (CMAKE_SIZEOF_VOID_P is unset)")
endif()

# High-resolution stat support

if(HAVE_STRUCT_STAT_ST_MTIM)
	set(GIT_USE_STAT_MTIM 1)
elseif(HAVE_STRUCT_STAT_ST_MTIMESPEC)
	set(GIT_USE_STAT_MTIMESPEC 1)
elseif(HAVE_STRUCT_STAT_ST_MTIME_NSEC)
	set(GIT_USE_STAT_MTIME_NSEC 1)
endif()

# realtime support

check_library_exists(rt clock_gettime "time.h" NEED_LIBRT)
if(NEED_LIBRT)
	list(APPEND LIBGIT2_SYSTEM_LIBS rt)
	list(APPEND LIBGIT2_PC_LIBS "-lrt")
endif()

# platform libraries

if(WIN32)
	list(APPEND LIBGIT2_SYSTEM_LIBS ws2_32)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
	list(APPEND LIBGIT2_SYSTEM_LIBS socket nsl)
	list(APPEND LIBGIT2_PC_LIBS "-lsocket" "-lnsl")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Haiku")
	list(APPEND LIBGIT2_SYSTEM_LIBS network)
	list(APPEND LIBGIT2_PC_LIBS "-lnetwork")
endif()

#
# Configuration summary
#

configure_file(libgit2/features.h.in git2/sys/features.h)

#
# Exports for peer projects (tests)
#

set_property(GLOBAL PROPERTY libgit2_dependency_includes ${LIBGIT2_DEPENDENCY_INCLUDES})
set_property(GLOBAL PROPERTY libgit2_dependency_objects ${LIBGIT2_DEPENDENCY_OBJECTS})
set_property(GLOBAL PROPERTY libgit2_system_includes ${LIBGIT2_SYSTEM_INCLUDES})
set_property(GLOBAL PROPERTY libgit2_system_libs ${LIBGIT2_SYSTEM_LIBS})

add_subdirectory(libgit2)
add_subdirectory(util)

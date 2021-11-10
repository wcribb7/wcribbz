# libgit2: the git library

#
# An object library so that our tests can link to the objects directly and
# test internal state, instead of linking with libgit2.so and testing only
# the public interfaces.
#

add_library(libgit2 OBJECT)
set_target_properties(libgit2 PROPERTIES C_STANDARD 90)

set(LIBGIT2_INCLUDES
	"${CMAKE_BINARY_DIR}/src"
	"${libgit2_SOURCE_DIR}/src/libgit2"
	"${libgit2_SOURCE_DIR}/src/util"
	"${libgit2_SOURCE_DIR}/include")

file(GLOB LIBGIT2_SRC *.c *.h
	streams/*.c streams/*.h
	transports/*.c transports/*.h
	xdiff/*.c xdiff/*.h)
list(SORT LIBGIT2_SRC)

#
# Platform specific options
#

target_compile_definitions(libgit2 PRIVATE _FILE_OFFSET_BITS=64)

if(AMIGA)
	target_compile_definitions(libgit2 PRIVATE NO_ADDRINFO NO_READDIR_R NO_MMAP)
endif()

if(MSVC)
	# the xdiff dependency is not (yet) warning-free, disable warnings as
	# errors for the xdiff sources until we've sorted them out
        set_source_files_properties(xdiff/xdiffi.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(xdiff/xutils.c PROPERTIES COMPILE_FLAGS -WX-)
endif()

if(MSVC_IDE)
	# precompiled headers
	set_target_properties(git2 PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(libgit2/win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

if(APPLE)
	# the old Secure Transport API has been deprecated in macOS 10.15.
	set_source_files_properties(streams/stransport.c PROPERTIES COMPILE_FLAGS -Wno-deprecated)
endif()

#TODO: still necessary?
if(MSVC AND GIT_ARCH_64 AND NOT BUILD_SHARED_LIBS)
	# workaround for Cmake bug #0011240 (see http://public.kitware.com/Bug/view.php?id=11240)
	set_target_properties(git2 PROPERTIES STATIC_LIBRARY_FLAGS "/MACHINE:x64")
endif()

#
# Preprocessor definitions
#

if(DEPRECATE_HARD)
	target_compile_definitions(libgit2 PRIVATE GIT_DEPRECATE_HARD)
endif()

if(USE_LEAK_CHECKER STREQUAL "valgrind")
	target_compile_definitions(util PRIVATE VALGRIND)
endif()

target_sources(libgit2 PRIVATE ${LIBGIT2_SRC})

target_include_directories(libgit2 PRIVATE ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES} PUBLIC ${libgit2_SOURCE_DIR}/include)
target_include_directories(libgit2 SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

#
# The actual libgit2 public consumable (libgit2.so/git2.dll)
#

add_library(libgit2public $<TARGET_OBJECTS:libgit2> $<TARGET_OBJECTS:util> ${LIBGIT2_DEPENDENCY_OBJECTS})
target_link_libraries(libgit2public ${LIBGIT2_SYSTEM_LIBS})

set_target_properties(libgit2public PROPERTIES OUTPUT_NAME ${LIBGIT2_FILENAME})

set_target_properties(libgit2public PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
set_target_properties(libgit2public PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
set_target_properties(libgit2public PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

if(SONAME)
	set_target_properties(libgit2public PROPERTIES VERSION ${libgit2_VERSION})
	set_target_properties(libgit2public PROPERTIES SOVERSION "${libgit2_VERSION_MAJOR}.${libgit2_VERSION_MINOR}")
endif()

pkg_build_config(NAME libgit2
	VERSION ${libgit2_VERSION}
	DESCRIPTION "A cross-platform linkable library implementation of Git"
	LIBS_SELF ${LIBGIT2_FILENAME}
	PRIVATE_LIBS ${LIBGIT2_PC_LIBS}
	REQUIRES ${LIBGIT2_PC_REQUIRES})

#
# Installation
#

install(TARGETS libgit2public
       RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
       LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
       ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${libgit2_SOURCE_DIR}/include/git2 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${libgit2_SOURCE_DIR}/include/git2.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

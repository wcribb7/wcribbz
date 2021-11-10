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
# Load dependency information
#

get_property(LIBGIT2_DEPENDENCY_INCLUDES GLOBAL PROPERTY libgit2_dependency_includes)
get_property(LIBGIT2_DEPENDENCY_OBJECTS GLOBAL PROPERTY libgit2_dependency_objects)
get_property(LIBGIT2_SYSTEM_INCLUDES GLOBAL PROPERTY libgit2_system_includes)
get_property(LIBGIT2_SYSTEM_LIBS GLOBAL PROPERTY libgit2_system_libs)

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

if(APPLE)
	# the old Secure Transport API has been deprecated in macOS 10.15.
	set_source_files_properties(streams/stransport.c PROPERTIES COMPILE_FLAGS -Wno-deprecated)
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

add_library(libgit2 OBJECT)
set_target_properties(libgit2 PROPERTIES C_STANDARD 90)

# Collect sourcefiles

file(GLOB SRC_H
	"${libgit2_SOURCE_DIR}/include/git2.h"
	"${libgit2_SOURCE_DIR}/include/git2/*.h"
	"${libgit2_SOURCE_DIR}/include/git2/sys/*.h")
list(SORT SRC_H)
target_sources(libgit2 PRIVATE ${SRC_H})

# On Windows use specific platform sources
if(WIN32 AND NOT CYGWIN)
	set(WIN_RC "win32/git2.rc")

	file(GLOB SRC_OS win32/*.c win32/*.h)
	list(SORT SRC_OS)
	target_sources(libgit2 PRIVATE ${SRC_OS})
elseif(AMIGA)
	target_compile_definitions(libgit2 PRIVATE NO_ADDRINFO NO_READDIR_R NO_MMAP)
else()
	file(GLOB SRC_OS unix/*.c unix/*.h)
	list(SORT SRC_OS)
	target_sources(libgit2 PRIVATE ${SRC_OS})
endif()

file(GLOB SRC_GIT2 *.c *.h
	allocators/*.c allocators/*.h
	streams/*.c streams/*.h
	transports/*.c transports/*.h
	xdiff/*.c xdiff/*.h)
list(SORT SRC_GIT2)
target_sources(libgit2 PRIVATE ${SRC_GIT2})

target_sources(libgit2 PRIVATE ${SRC_SHA1})

if(APPLE)
	# The old Secure Transport API has been deprecated in macOS 10.15.
	set_source_files_properties(streams/stransport.c PROPERTIES COMPILE_FLAGS -Wno-deprecated)
endif()

# the xdiff dependency is not (yet) warning-free, disable warnings as
# errors for the xdiff sources until we've sorted them out
if(MSVC)
	set_source_files_properties(xdiff/xdiffi.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xutils.c PROPERTIES COMPILE_FLAGS -WX-)
endif()

ide_split_sources(libgit2)
list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:libgit2>)

target_include_directories(libgit2 PRIVATE ${LIBGIT2_INCLUDES} PUBLIC ${libgit2_SOURCE_DIR}/include)
target_include_directories(libgit2 SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

set(LIBGIT2_OBJECTS ${LIBGIT2_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_INCLUDES ${LIBGIT2_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_INCLUDES ${LIBGIT2_SYSTEM_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_LIBS ${LIBGIT2_LIBS} PARENT_SCOPE)

if(XCODE_VERSION)
	# This is required for Xcode to actually link the libgit2 library
	# when using only object libraries.
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dummy.c "")
	list(APPEND LIBGIT2_OBJECTS ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)
endif()

# Compile and link libgit2
add_library(git2 ${WIN_RC} ${LIBGIT2_OBJECTS})
target_link_libraries(git2 ${LIBGIT2_LIBS})

set_target_properties(git2 PROPERTIES C_STANDARD 90)
set_target_properties(git2 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
set_target_properties(git2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
set_target_properties(git2 PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

# Workaround for Cmake bug #0011240 (see http://public.kitware.com/Bug/view.php?id=11240)
# Win64+MSVC+static libs = linker error
if(MSVC AND GIT_ARCH_64 AND NOT BUILD_SHARED_LIBS)
	set_target_properties(git2 PROPERTIES STATIC_LIBRARY_FLAGS "/MACHINE:x64")
endif()

ide_split_sources(git2)

if(SONAME)
	set_target_properties(git2 PROPERTIES VERSION ${libgit2_VERSION})
	set_target_properties(git2 PROPERTIES SOVERSION "${libgit2_VERSION_MAJOR}.${libgit2_VERSION_MINOR}")
	if(LIBGIT2_FILENAME)
		target_compile_definitions(git2 PRIVATE LIBGIT2_FILENAME=\"${LIBGIT2_FILENAME}\")
		set_target_properties(git2 PROPERTIES OUTPUT_NAME ${LIBGIT2_FILENAME})
	elseif(DEFINED LIBGIT2_PREFIX)
		set_target_properties(git2 PROPERTIES PREFIX "${LIBGIT2_PREFIX}")
	endif()
endif()

pkg_build_config(NAME libgit2
	VERSION ${libgit2_VERSION}
	DESCRIPTION "The git library, take 2"
	LIBS_SELF git2
	PRIVATE_LIBS ${LIBGIT2_PC_LIBS}
	REQUIRES ${LIBGIT2_PC_REQUIRES}
)

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(git2 PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

# Install
install(TARGETS git2
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${libgit2_SOURCE_DIR}/include/git2 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${libgit2_SOURCE_DIR}/include/git2.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# export these variables for the test framework; it links our object
# files directly (instead of linking to libgit2.so) so that it can
# test private and internal functions.
set_property(GLOBAL PROPERTY libgit2_objects ${LIBGIT2_OBJECTS})
set_property(GLOBAL PROPERTY libgit2_includes ${LIBGIT2_INCLUDES})
set_property(GLOBAL PROPERTY libgit2_system_includes ${LIBGIT2_SYSTEM_INCLUDES})
set_property(GLOBAL PROPERTY libgit2_libs ${LIBGIT2_LIBS})

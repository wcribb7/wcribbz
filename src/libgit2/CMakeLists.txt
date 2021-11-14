add_library(git2internal OBJECT)
set_target_properties(git2internal PROPERTIES C_STANDARD 90)

include(PkgBuildConfig)

set(LIBGIT2_INCLUDES
	"${CMAKE_BINARY_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/libgit2"
	"${CMAKE_SOURCE_DIR}/src/util"
	"${CMAKE_SOURCE_DIR}/include")

if(WIN32 AND EMBED_SSH_PATH)
	file(GLOB SRC_SSH "${EMBED_SSH_PATH}/src/*.c")
	list(SORT SRC_SSH)
	target_sources(git2internal PRIVATE ${SRC_SSH})

	list(APPEND LIBGIT2_SYSTEM_INCLUDES "${EMBED_SSH_PATH}/include")
	file(WRITE "${EMBED_SSH_PATH}/src/libssh2_config.h" "#define HAVE_WINCNG\n#define LIBSSH2_WINCNG\n#include \"../win32/libssh2_config.h\"")
	set(GIT_SSH 1)
endif()

# Collect sourcefiles
file(GLOB SRC_H
	"${CMAKE_SOURCE_DIR}/include/git2.h"
	"${CMAKE_SOURCE_DIR}/include/git2/*.h"
	"${CMAKE_SOURCE_DIR}/include/git2/sys/*.h")
list(SORT SRC_H)
target_sources(git2internal PRIVATE ${SRC_H})

file(GLOB SRC_GIT2 *.c *.h
	streams/*.c streams/*.h
	transports/*.c transports/*.h
	xdiff/*.c xdiff/*.h)
list(SORT SRC_GIT2)
target_sources(git2internal PRIVATE ${SRC_GIT2})

if(WIN32 AND NOT CYGWIN)
	# Add resource information on Windows
	set(SRC_RC "git2.rc")
endif()

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

ide_split_sources(git2internal)
list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:util> $<TARGET_OBJECTS:git2internal> ${LIBGIT2_DEPENDENCY_OBJECTS})

target_include_directories(git2internal PRIVATE ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES} PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(git2internal SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

set(LIBGIT2_INCLUDES ${LIBGIT2_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_OBJECTS ${LIBGIT2_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_INCLUDES ${LIBGIT2_DEPENDENCY_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_OBJECTS ${LIBGIT2_DEPENDENCY_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_INCLUDES ${LIBGIT2_SYSTEM_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_LIBS ${LIBGIT2_SYSTEM_LIBS} PARENT_SCOPE)

if(XCODE_VERSION)
	# This is required for Xcode to actually link the libgit2 library
	# when using only object libraries.
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dummy.c "")
	list(APPEND LIBGIT2_OBJECTS ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)
endif()

# Compile and link libgit2
add_library(git2 ${SRC_RC} ${LIBGIT2_OBJECTS})
target_link_libraries(git2 ${LIBGIT2_SYSTEM_LIBS})

set_target_properties(git2 PROPERTIES C_STANDARD 90)
set_target_properties(git2 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set_target_properties(git2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set_target_properties(git2 PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

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
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/git2 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_SOURCE_DIR}/include/git2.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

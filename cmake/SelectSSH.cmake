# find libssh2
if(USE_SSH STREQUAL ON OR USE_SSH STREQUAL "libssh2")
	find_pkglibraries(LIBSSH2 libssh2)

	if(NOT LIBSSH2_FOUND)
		find_package(LibSSH2)
		set(LIBSSH2_INCLUDE_DIRS ${LIBSSH2_INCLUDE_DIR})
		get_filename_component(LIBSSH2_LIBRARY_DIRS "${LIBSSH2_LIBRARY}" DIRECTORY)
		set(LIBSSH2_LIBRARIES ${LIBSSH2_LIBRARY})
		set(LIBSSH2_LDFLAGS "-lssh2")
	endif()

	if(NOT LIBSSH2_FOUND)
		message(FATAL_ERROR "LIBSSH2 not found. Set CMAKE_PREFIX_PATH if it is installed outside of the default search path.")
	endif()

	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${LIBSSH2_INCLUDE_DIRS})
	list(APPEND LIBGIT2_SYSTEM_LIBS ${LIBSSH2_LIBRARIES})
	list(APPEND LIBGIT2_PC_LIBS ${LIBSSH2_LDFLAGS})

	check_library_exists("${LIBSSH2_LIBRARIES}" libssh2_userauth_publickey_frommemory "${LIBSSH2_LIBRARY_DIRS}" HAVE_LIBSSH2_MEMORY_CREDENTIALS)
	if(HAVE_LIBSSH2_MEMORY_CREDENTIALS)
		set(GIT_SSH_LIBSSH2_MEMORY_CREDENTIALS 1)
	endif()

	if(WIN32 AND EMBED_SSH_PATH)
		file(GLOB SSH_SRC "${EMBED_SSH_PATH}/src/*.c")
		list(SORT SSH_SRC)
		list(APPEND LIBGIT2_DEPENDENCY_OBJECTS ${SSH_SRC})

		list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${EMBED_SSH_PATH}/include")
		file(WRITE "${EMBED_SSH_PATH}/src/libssh2_config.h" "#define HAVE_WINCNG\n#define LIBSSH2_WINCNG\n#include \"../win32/libssh2_config.h\"")
	endif()

	set(GIT_SSH 1)
	set(GIT_SSH_LIBSSH2 1)
	add_feature_info(SSH ON "using libssh2")
else()
	add_feature_info(SSH OFF "SSH transport support")
endif()


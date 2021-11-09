# libgit2_tests

add_executable(libgit2_tests)

set(TEST_INCLUDES
	"${CMAKE_BINARY_DIR}/src"
	"${libgit2_SOURCE_DIR}/src/libgit2"
	"${libgit2_SOURCE_DIR}/src/util"
	"${libgit2_SOURCE_DIR}/include")

file(GLOB_RECURSE TEST_SRC */*.c */*.h)

#
# Locate python so that we can generate the clar test data
#

set(Python_ADDITIONAL_VERSIONS 3 2.7)
find_package(PythonInterp)

if(NOT PYTHONINTERP_FOUND)
	message(FATAL_ERROR "Could not find a python interpeter, which is needed to build the tests. "
	                     "Make sure python is available, or pass -DBUILD_TESTS=OFF to skip building the tests.")
endif()

#
# Load dependency information
#

get_property(LIBGIT2_DEPENDENCY_INCLUDES GLOBAL PROPERTY libgit2_dependency_includes)
get_property(LIBGIT2_DEPENDENCY_OBJECTS GLOBAL PROPERTY libgit2_dependency_objects)
get_property(LIBGIT2_SYSTEM_INCLUDES GLOBAL PROPERTY libgit2_system_includes)
get_property(LIBGIT2_SYSTEM_LIBS GLOBAL PROPERTY libgit2_system_libs)

#
# Include clar
#

set(CLAR_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(CLAR_FIXTURES "${CLAR_PATH}/resources")
set(CLAR_SUITE_PATH "${CMAKE_CURRENT_BINARY_DIR}")

include_directories(${CLAR_PATH})
set(TEST_SRC_CLAR "main.c" "clar.c" "clar_libgit2.c" "clar_libgit2_trace.c" "clar_libgit2_timer.c")

add_custom_command(
	OUTPUT ${CLAR_SUITE_PATH}/clar.suite ${CLAR_SUITE_PATH}/clar_suite.h
	COMMAND ${PYTHON_EXECUTABLE} generate.py -o "${CLAR_SUITE_PATH}" -f -xonline -xstress -xperf .
	DEPENDS ${TEST_SRC}
	WORKING_DIRECTORY ${CLAR_PATH}
)

set_source_files_properties(
	${CLAR_PATH}/clar.c
	PROPERTIES OBJECT_DEPENDS ${CLAR_SUITE_PATH}/clar.suite)

#
# Preprocessor definitions
#

if(USE_LEAK_CHECKER STREQUAL "valgrind")
        target_compile_definitions(libgit2_tests PRIVATE VALGRIND)
endif()

target_compile_definitions(libgit2_tests PRIVATE CLAR_FIXTURE_PATH=\"${CLAR_FIXTURES}\")
target_compile_definitions(libgit2_tests PRIVATE CLAR_TMPDIR=\"libgit2_tests\")
target_compile_definitions(libgit2_tests PRIVATE CLAR_WIN32_LONGPATHS)
target_compile_definitions(libgit2_tests PRIVATE _FILE_OFFSET_BITS=64)

# ensure that we do not use deprecated functions internally
target_compile_definitions(libgit2_tests PRIVATE GIT_DEPRECATE_HARD)

#
# Platform specific options
#

#
# Old versions of gcc require us to declare our test functions; don't do
# this on newer compilers to avoid unnecessary recompilation.
#
if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
	add_definitions(-include \"clar_suite.h\")
endif()

if(MSVC_IDE)
	list(APPEND TEST_SRC_CLAR "precompiled.c")
	set_target_properties(libgit2_tests PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties("precompiled.c" COMPILE_FLAGS "/Ycprecompiled.h")
endif()

#
# The test runner binary
#

target_sources(libgit2_tests PRIVATE ${TEST_SRC_CLAR} ${TEST_SRC} $<TARGET_OBJECTS:libgit2> $<TARGET_OBJECTS:util> ${LIBGIT2_DEPENDENCY_OBJECTS})

target_include_directories(libgit2_tests PRIVATE ${CLAR_SUITE_PATH} ${TEST_INCLUDES})
target_link_libraries(libgit2_tests ${LIBGIT2_SYSTEM_LIBS})

set_target_properties(libgit2_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

#
# Add tests
#

function(ADD_CLAR_TEST name)
	if(NOT USE_LEAK_CHECKER STREQUAL "OFF")
		add_test(${name} "${libgit2_SOURCE_DIR}/script/${USE_LEAK_CHECKER}.sh" "${libgit2_BINARY_DIR}/libgit2_tests" ${ARGN})
	else()
		add_test(${name} "${libgit2_BINARY_DIR}/libgit2_tests" ${ARGN})
	endif()
endfunction(ADD_CLAR_TEST)

enable_testing()

add_clar_test(offline             -v -xonline)
add_clar_test(invasive            -v -score::ftruncate -sfilter::stream::bigfile -sodb::largefiles -siterator::workdir::filesystem_gunk -srepo::init -srepo::init::at_filesystem_root)
add_clar_test(online              -v -sonline -xonline::customcert)
add_clar_test(online_customcert   -v -sonline::customcert)
add_clar_test(gitdaemon           -v -sonline::push)
add_clar_test(ssh                 -v -sonline::push -sonline::clone::ssh_cert -sonline::clone::ssh_with_paths -sonline::clone::path_whitespace_ssh)
add_clar_test(proxy               -v -sonline::clone::proxy)
add_clar_test(auth_clone          -v -sonline::clone::cred)
add_clar_test(auth_clone_and_push -v -sonline::clone::push -sonline::push)

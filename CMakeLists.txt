cmake_minimum_required(VERSION 3.19)
project(dencla-sailfish-common CXX)

# Remembers whether THIS repo is the top-level CMake invocation (its own standalone build/test
# run, e.g. CI for this repo) as opposed to being pulled in via a consuming app's
# add_subdirectory() call - the latter already configures CMAKE_CXX_STANDARD/finds Qt5 itself,
# and shouldn't have this repo's own unit tests silently added to its `ctest` run every time it
# links against a module here.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DENCLA_COMMON_IS_TOP_LEVEL ON)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
    set(DENCLA_COMMON_IS_TOP_LEVEL OFF)
endif()

# Used by each module's own CMakeLists.txt to find tests/test_*.cpp regardless of whether this
# repo is the top-level source dir or nested under a consuming app's own CMAKE_SOURCE_DIR (that
# variable refers to the OUTERMOST project once add_subdirectory()'d, not to this repo's own
# root - this one always does). Also promoted to PARENT_SCOPE so a consuming app's own top-level
# CMakeLists.txt (one level up from this add_subdirectory() call) can use it too - e.g. to
# install shared qml/ files straight from here instead of keeping a local copy.
set(DENCLA_COMMON_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DENCLA_COMMON_IS_TOP_LEVEL)
    set(DENCLA_COMMON_SOURCE_DIR ${DENCLA_COMMON_SOURCE_DIR} PARENT_SCOPE)
endif()

option(DENCLA_COMMON_BUILD_TESTS "Build dencla-sailfish-common's own unit tests" ON)
if(DENCLA_COMMON_IS_TOP_LEVEL AND DENCLA_COMMON_BUILD_TESTS)
    find_package(Qt5Test QUIET)
    if(Qt5Test_FOUND)
        enable_testing()
    else()
        message(STATUS "Qt5Test not found - skipping dencla-sailfish-common's own unit tests")
    endif()
endif()

find_package(Qt5 COMPONENTS Core REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(cmake/QtUnitTest.cmake)

# Each module is opt-in per consuming app via target_link_libraries() - e.g.
# harbour-wetterstein (no Secrets need) only links dencla_common_cache/dencla_common_net. cache/
# util/net only need Qt5, so they're always added - but secrets/ additionally does its own
# pkg_search_module(... REQUIRED sailfishsecrets), which would hard-fail *configure* (not just
# link) for a consuming app that doesn't even want it, in a build environment that lacks the
# sailfishsecrets dev package (wetterstein's real device/CI environment, for instance). A
# consuming app that needs it sets `set(DENCLA_COMMON_ENABLE_SECRETS ON)` BEFORE its
# add_subdirectory() call on this repo (see harbour-sailreader's/harbour-audiobookfish's own
# CMakeLists.txt) - a normal variable set by the parent scope shadows this option()'s own
# default for every directory nested under it, including this one.
add_subdirectory(cache)
add_subdirectory(util)
add_subdirectory(net)
option(DENCLA_COMMON_ENABLE_SECRETS "Build the secrets module (requires sailfishsecrets)" OFF)
if(DENCLA_COMMON_ENABLE_SECRETS)
    add_subdirectory(secrets)
endif()

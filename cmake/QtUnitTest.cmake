# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

# add_qt_unit_test(<name> SOURCES <files...> LIBS <targets...> [INCLUDE_DIRS <dirs...>])
#
# Registers a desktop-buildable Qt5Test executable - the same pattern
# harbour-wetterstein/harbour-audiobookfish/harbour-sailreader's own CMakeLists.txt each
# hand-rolled per test file: skipped entirely if Qt5Test isn't available (checked once by the
# caller before including this file - see each module's own CMakeLists.txt), links only the
# given libs plus Qt5::Test (never sailfishapp/sailfishsecrets, so tests build and run on a
# plain desktop), one add_test() per call.
function(add_qt_unit_test NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS;INCLUDE_DIRS" ${ARGN})
    add_executable(${NAME} ${ARG_SOURCES})
    if(ARG_INCLUDE_DIRS)
        target_include_directories(${NAME} PRIVATE ${ARG_INCLUDE_DIRS})
    endif()
    target_link_libraries(${NAME} ${ARG_LIBS} Qt5::Test)
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

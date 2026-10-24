# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dencla_common_util STATIC
    fileutils.h
    fileutils.cpp
    qtsignalonce.h
    httputils.h
)
target_include_directories(dencla_common_util PUBLIC ${DENCLA_COMMON_SOURCE_DIR})
target_link_libraries(dencla_common_util PUBLIC Qt5::Core)

if(DENCLA_COMMON_IS_TOP_LEVEL AND Qt5Test_FOUND)
    add_qt_unit_test(test_fileutils
        SOURCES ${DENCLA_COMMON_SOURCE_DIR}/tests/test_fileutils.cpp
        LIBS dencla_common_util
    )
endif()

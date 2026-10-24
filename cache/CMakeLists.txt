# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dencla_common_cache STATIC
    diskcache.h
    diskcache.cpp
)
target_include_directories(dencla_common_cache PUBLIC ${DENCLA_COMMON_SOURCE_DIR})
target_link_libraries(dencla_common_cache PUBLIC Qt5::Core)

if(DENCLA_COMMON_IS_TOP_LEVEL AND Qt5Test_FOUND)
    add_qt_unit_test(test_diskcache
        SOURCES ${DENCLA_COMMON_SOURCE_DIR}/tests/test_diskcache.cpp
        LIBS dencla_common_cache
    )
endif()

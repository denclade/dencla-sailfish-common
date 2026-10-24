# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt5 COMPONENTS Network REQUIRED)

add_library(dencla_common_net STATIC
    httpapiclient.h
    httpapiclient.cpp
    downloadqueue.h
    downloadqueue.cpp
)
target_include_directories(dencla_common_net PUBLIC ${DENCLA_COMMON_SOURCE_DIR})
target_link_libraries(dencla_common_net PUBLIC Qt5::Core Qt5::Network dencla_common_cache)

if(DENCLA_COMMON_IS_TOP_LEVEL AND Qt5Test_FOUND)
    add_qt_unit_test(test_httpapiclient
        SOURCES ${DENCLA_COMMON_SOURCE_DIR}/tests/test_httpapiclient.cpp
        LIBS dencla_common_net
    )
endif()

# downloadqueue.cpp is deliberately untested at the unit level - its behavior (concurrency
# limiting, integrity checking, retry) only means anything against a real
# QNetworkAccessManager talking to a real server, same as every source app's own equivalent
# (harbour-sailreader's MediaCacheManager, harbour-wetterstein's RadarManager) being absent
# from their own BUILD_TESTS blocks. See tests/test_httpapiclient.cpp's own header comment.

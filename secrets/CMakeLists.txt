# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

include(FindPkgConfig)
pkg_search_module(SAILFISHSECRETS REQUIRED sailfishsecrets)

add_library(dencla_common_secrets STATIC
    secretsstore.h
    secretsstore.cpp
)
# SAILFISHSECRETS_INCLUDE_DIRS must be PUBLIC, not PRIVATE: secretsstore.h itself (a public
# header, included by every consumer) directly includes <Secrets/secretmanager.h> - a
# consumer's own translation units (e.g. an app's apiclient.cpp including secretsstore.h) need
# this include path too, and PRIVATE would only apply it while compiling secretsstore.cpp
# itself inside this library, not to anything that merely links against it.
target_include_directories(dencla_common_secrets
    PUBLIC ${DENCLA_COMMON_SOURCE_DIR} ${SAILFISHSECRETS_INCLUDE_DIRS}
)
target_link_libraries(dencla_common_secrets PUBLIC Qt5::Core ${SAILFISHSECRETS_LDFLAGS})

# No unit tests here - SecretsStore only means anything against the real Sailfish Secrets
# daemon (DeviceLock unlock state, an actual encrypted collection), which isn't present in a
# desktop Qt5Test run at all. Matches harbour-sailreader's and harbour-audiobookfish's own
# SecretsManager, neither of which is in their app's BUILD_TESTS block either.

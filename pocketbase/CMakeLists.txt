find_package(Qt5 COMPONENTS Network REQUIRED)

# PocketBase's REST API (see pocketbaseclient.h). Tested against a real
# server by the consuming app (harbour-fitfish: just cloud-test), since
# nothing short of one says whether requests are shaped right.
add_library(dencla_common_pocketbase STATIC
    pocketbaseclient.h
    pocketbaseclient.cpp
)
target_include_directories(dencla_common_pocketbase PUBLIC ${DENCLA_COMMON_SOURCE_DIR})
target_link_libraries(dencla_common_pocketbase PUBLIC Qt5::Core Qt5::Network)

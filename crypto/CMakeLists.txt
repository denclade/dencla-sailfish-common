# End-to-end encryption primitives (see vaultcrypto.h). The two C libraries
# are vendored as upstream released them (vendor/*/, with their licences):
# Monocypher 4.0.2 and the BLAKE3 1.5.4 reference implementation, the
# latter in its portable form only -- no SIMD assembly, which would need a
# per-architecture build and gains nothing at the sizes synced here.
enable_language(C)

add_library(dencla_common_crypto STATIC
    vaultcrypto.h
    vaultcrypto.cpp
    vendor/monocypher/monocypher.c
    vendor/monocypher/monocypher.h
    vendor/blake3/blake3.c
    vendor/blake3/blake3_dispatch.c
    vendor/blake3/blake3_portable.c
    vendor/blake3/blake3.h
    vendor/blake3/blake3_impl.h
)
target_include_directories(dencla_common_crypto PUBLIC ${DENCLA_COMMON_SOURCE_DIR})
target_compile_definitions(dencla_common_crypto PRIVATE
    BLAKE3_NO_SSE2 BLAKE3_NO_SSE41 BLAKE3_NO_AVX2 BLAKE3_NO_AVX512 BLAKE3_USE_NEON=0)
set_source_files_properties(
    vendor/monocypher/monocypher.c vendor/blake3/blake3.c vendor/blake3/blake3_dispatch.c
    vendor/blake3/blake3_portable.c
    PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(dencla_common_crypto PUBLIC Qt5::Core)

if(DENCLA_COMMON_IS_TOP_LEVEL AND Qt5Test_FOUND)
    add_qt_unit_test(test_vaultcrypto
        SOURCES ${DENCLA_COMMON_SOURCE_DIR}/tests/test_vaultcrypto.cpp
        LIBS dencla_common_crypto
    )
endif()

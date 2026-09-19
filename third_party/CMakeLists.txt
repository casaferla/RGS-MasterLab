add_library(rgsml_vendor_json INTERFACE)
target_include_directories(rgsml_vendor_json SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/single_include")

# Directory-local normal variables: never change project-wide BUILD_SHARED_LIBS
# or cache options. The pinned upstream subproject sees an offline static profile.
set(BUILD_SHARED_LIBS OFF)
foreach(mz_option IN ITEMS
    MZ_COMPAT MZ_ZLIB MZ_BZIP2 MZ_LZMA MZ_PPMD MZ_ZSTD MZ_LIBCOMP
    MZ_FETCH_LIBS MZ_FORCE_FETCH_LIBS MZ_PKCRYPT MZ_WZAES MZ_OPENSSL
    MZ_LIBBSD MZ_ICONV MZ_COMPRESS_ONLY MZ_DECOMPRESS_ONLY
    MZ_BUILD_TESTS MZ_BUILD_UNIT_TESTS MZ_BUILD_FUZZ_TESTS
    MZ_CODE_COVERAGE MZ_SANITIZER)
    set(${mz_option} OFF)
endforeach()
set(SKIP_INSTALL_ALL ON)
add_subdirectory(minizip-ng)
if(NOT TARGET MINIZIP::minizip)
    message(FATAL_ERROR "Pinned minizip-ng did not expose MINIZIP::minizip")
endif()

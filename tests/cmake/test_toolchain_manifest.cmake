include("${RGSML_SOURCE_DIR}/cmake/RgsmlToolchainManifest.cmake")

set(first "${RGSML_TEST_BINARY_DIR}/synthetic/first.json")
set(second "${RGSML_TEST_BINARY_DIR}/synthetic/second.json")

foreach(output IN ITEMS "${first}" "${second}")
    rgsml_write_toolchain_manifest(
        OUTPUT "${output}"
        SYSTEM_NAME "SyntheticOS"
        ARCHITECTURE "synthetic64"
        WINDOWS_SDK_VERSION "10.0.synthetic"
        CMAKE_VERSION "3.31.6"
        GENERATOR "Synthetic \"Generator\"\\Path"
        GENERATOR_PLATFORM "synthetic64"
        COMPILER_ID "SyntheticCompiler"
        COMPILER_VERSION "1.2.3"
        TOOLSET "toolset"
        QT_VERSION "6.8.3"
        GIT_VERSION "2.53.0.windows.3"
    )
endforeach()

file(READ "${first}" actual)
set(expected [=[{
  "artifactId": "rgsml.toolchain_manifest",
  "schemaVersion": 1,
  "productName": "RGS MasterLab",
  "target": {
    "systemName": "SyntheticOS",
    "architecture": "synthetic64",
    "windowsSdkVersion": "10.0.synthetic"
  },
  "buildSystem": {
    "cmakeVersion": "3.31.6",
    "generator": "Synthetic \"Generator\"\\Path",
    "generatorPlatform": "synthetic64"
  },
  "compiler": {
    "id": "SyntheticCompiler",
    "version": "1.2.3",
    "toolset": "toolset"
  },
  "qt": {
    "version": "6.8.3",
    "minimumVersion": "6.8"
  },
  "sourceControl": {
    "gitVersion": "2.53.0.windows.3"
  },
  "language": {
    "cxxStandard": 20,
    "extensions": false
  }
}]=])
string(APPEND expected "\n")

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Synthetic manifest differs from the canonical byte format")
endif()
string(JSON artifact_id GET "${actual}" artifactId)
if(NOT artifact_id STREQUAL "rgsml.toolchain_manifest")
    message(FATAL_ERROR "Synthetic manifest JSON content is invalid")
endif()
file(SHA256 "${first}" first_hash)
file(SHA256 "${second}" second_hash)
if(NOT first_hash STREQUAL second_hash)
    message(FATAL_ERROR "Synthetic manifest generation is not deterministic")
endif()

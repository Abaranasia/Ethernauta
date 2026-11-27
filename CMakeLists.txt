cmake_minimum_required(VERSION 3.25)
project(Ethernauta VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(EthernautaWarnings)

option(ETHERNAUTA_SANITIZE "Build everything with AddressSanitizer + UBSan (clang/gcc only)" OFF)
if(ETHERNAUTA_SANITIZE)
  if(MSVC)
    message(FATAL_ERROR "ETHERNAUTA_SANITIZE is not supported with MSVC (use a clang/gcc toolchain)")
  endif()
  # Directory-scoped so Catch2 and ethernauta_core are instrumented together. Any report aborts the test.
  add_compile_options(-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all)
endif()

enable_testing()
add_subdirectory(Source/core)
add_subdirectory(tests)

# Opt-in JUCE plugin (VST3 + Standalone). OFF by default: the default build stays CXX-only and
# JUCE-free. Use a separate build directory (e.g. build-plugin) when turning this on.
option(ETHERNAUTA_BUILD_PLUGIN "Build the JUCE VST3 + Standalone plugin (local JUCE or pinned fetch)" OFF)
set(ETHERNAUTA_JUCE_DIR "${CMAKE_SOURCE_DIR}/../../JUCE" CACHE PATH
  "Path to a JUCE checkout containing CMakeLists.txt (used when ETHERNAUTA_BUILD_PLUGIN=ON)")
include(JucePin) # ETHERNAUTA_JUCE_VERSION, ETHERNAUTA_JUCE_SHA, ETHERNAUTA_JUCE_SHA256
option(ETHERNAUTA_JUCE_FETCH "Download pinned JUCE ${ETHERNAUTA_JUCE_VERSION} when ETHERNAUTA_JUCE_DIR has no JUCE" ON)
if(ETHERNAUTA_BUILD_PLUGIN)
  if(EXISTS "${ETHERNAUTA_JUCE_DIR}/CMakeLists.txt")
    set(_ethernauta_juce_src "${ETHERNAUTA_JUCE_DIR}")
    message(STATUS "Ethernauta: using local JUCE at '${ETHERNAUTA_JUCE_DIR}'")
  elseif(ETHERNAUTA_JUCE_FETCH)
    message(STATUS "Ethernauta: no JUCE at '${ETHERNAUTA_JUCE_DIR}'; fetching JUCE ${ETHERNAUTA_JUCE_VERSION} "
      "(${ETHERNAUTA_JUCE_SHA}). On failure set -DETHERNAUTA_JUCE_DIR=<JUCE checkout> "
      "or -DETHERNAUTA_JUCE_FETCH=OFF.")
    include(FetchContent)
    FetchContent_Declare(juce
      URL https://github.com/juce-framework/JUCE/archive/${ETHERNAUTA_JUCE_SHA}.tar.gz
      URL_HASH SHA256=${ETHERNAUTA_JUCE_SHA256}
      SOURCE_SUBDIR ethernauta-download-only) # no CMakeLists.txt there: download only, no add_subdirectory
    FetchContent_MakeAvailable(juce)
    if(NOT EXISTS "${juce_SOURCE_DIR}/CMakeLists.txt")
      message(FATAL_ERROR "Ethernauta: fetched JUCE not available at '${juce_SOURCE_DIR}' (offline, "
        "FETCHCONTENT_FULLY_DISCONNECTED, or FETCHCONTENT_* override?). "
        "Set -DETHERNAUTA_JUCE_DIR=<JUCE checkout> or -DETHERNAUTA_JUCE_FETCH=OFF.")
    endif()
    set(_ethernauta_juce_src "${juce_SOURCE_DIR}")
  else()
    message(FATAL_ERROR
      "ETHERNAUTA_JUCE_DIR='${ETHERNAUTA_JUCE_DIR}' does not contain a JUCE CMakeLists.txt and "
      "ETHERNAUTA_JUCE_FETCH=OFF. Set -DETHERNAUTA_JUCE_DIR=<path to a JUCE checkout>, "
      "or set ETHERNAUTA_JUCE_FETCH=ON, or leave ETHERNAUTA_BUILD_PLUGIN=OFF.")
  endif()
  enable_language(C) # JUCE's CMake requires the C language to be enabled
  add_subdirectory("${_ethernauta_juce_src}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
  add_subdirectory(Source/plugin EXCLUDE_FROM_ALL)
  # Not part of `all`. Also makes the excluded plugin targets visible to Visual Studio solutions,
  # which otherwise drop them and break `cmake --build --target EthernautaPlugin_VST3`.
  add_custom_target(ethernauta_plugin DEPENDS EthernautaPlugin_VST3 EthernautaPlugin_Standalone)
endif()

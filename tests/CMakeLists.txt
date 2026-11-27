# Catch2 is fetched at a pinned commit by default so every build uses the same bytes. An
# installed Catch2 is never picked up implicitly (no FIND_PACKAGE_ARGS). Offline options:
#   -DFETCHCONTENT_SOURCE_DIR_CATCH2=<path>      use a local checkout (honoured by FetchContent)
#   -DETHERNAUTA_USE_SYSTEM_CATCH2=ON            explicitly opt in to an installed Catch2 3.x
option(ETHERNAUTA_USE_SYSTEM_CATCH2 "Use an installed Catch2 3.x instead of the pinned fetch" OFF)

if(ETHERNAUTA_USE_SYSTEM_CATCH2)
  find_package(Catch2 3 REQUIRED)
else()
  include(FetchContent)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    # v3.8.1, pinned to the commit SHA so a moved tag cannot change the build.
    GIT_TAG        2b60af89e23d28eefc081bc930831ee9d45ea58b
    SYSTEM)
  FetchContent_MakeAvailable(Catch2)
endif()

# catch_discover_tests lives in Catch.cmake: shipped in extras/ of a source tree; an installed
# package already makes it available through find_package.
if(catch2_SOURCE_DIR)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
include(Catch)

add_executable(ethernauta_tests
  test_oscillator.cpp
  test_param_smoother.cpp
  test_latch_gate.cpp
  test_drone_voice.cpp
  test_master_stage.cpp)
target_include_directories(ethernauta_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ethernauta_tests PRIVATE ethernauta_core Catch2::Catch2WithMain)
ethernauta_set_warnings(ethernauta_tests)
catch_discover_tests(ethernauta_tests)

# Guard: the core must link no JUCE target. Configure-time check lives in Source/core/CMakeLists.txt;
# this ctest re-checks the final link line from the generated build.
add_test(NAME core_links_no_juce
  COMMAND ${CMAKE_COMMAND}
    "-DLIBS=$<TARGET_PROPERTY:ethernauta_core,LINK_LIBRARIES>;$<TARGET_PROPERTY:ethernauta_core,INTERFACE_LINK_LIBRARIES>"
    -P ${CMAKE_SOURCE_DIR}/cmake/CheckNoJuceLinks.cmake)

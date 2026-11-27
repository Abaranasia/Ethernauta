# Plugin-layer tests (opt-in: only reached when ETHERNAUTA_BUILD_PLUGIN=ON). Catch2 (already used by
# the core tests) drives them; the test exe links the plugin's shared-code target, so it exercises the
# real AudioProcessor, APVTS layout and editor. A listener in plugin_test_main.cpp initialises JUCE.
add_executable(EthernautaPluginTests
  plugin_test_main.cpp
  test_plugin_params.cpp
  test_plugin_state.cpp
  test_plugin_audio.cpp)
target_include_directories(EthernautaPluginTests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/Source/plugin)
# First-party warning flags (/WX) are not applied: JUCE headers are included here. JUCE's recommended
# warning flags come in through EthernautaPlugin.
# EthernautaPlugin links the JUCE modules PRIVATE, so the test exe links them itself for the headers.
target_link_libraries(EthernautaPluginTests PRIVATE
  EthernautaPlugin
  ethernauta_core
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  Catch2::Catch2WithMain)
catch_discover_tests(EthernautaPluginTests TEST_PREFIX "plugin: ")

# VST3 + Standalone shell around ethernauta_core. Metadata below is placeholder until the final
# plugin identity is decided. JUCE comes from the root CMakeLists (local ETHERNAUTA_JUCE_DIR or pinned fetch).
juce_add_plugin(EthernautaPlugin
  COMPANY_NAME "Ethernauta"
  PLUGIN_MANUFACTURER_CODE Ethn
  PLUGIN_CODE Eth1
  PRODUCT_NAME "Ethernauta"
  BUNDLE_ID com.ethernauta.ethernauta
  FORMATS VST3 Standalone
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(EthernautaPlugin PRIVATE
  ParameterLayout.cpp
  ParameterLayout.h
  PluginEditor.cpp
  PluginEditor.h
  PluginProcessor.cpp
  PluginProcessor.h
  VoicePanel.cpp
  VoicePanel.h)

target_compile_definitions(EthernautaPlugin PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

# Warnings: JUCE's recommended flags only. ethernauta_set_warnings (/WX, -Werror) is for first-party
# code; JUCE module sources compile inside this target.
target_link_libraries(EthernautaPlugin
  PRIVATE
    ethernauta_core
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

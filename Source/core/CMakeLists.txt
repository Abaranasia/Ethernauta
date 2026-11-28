add_library(ethernauta_core STATIC
  src/Oscillator.cpp
  src/LatchGate.cpp
  src/DroneVoice.cpp
  src/JustSnap.cpp
  src/TuningEngine.cpp)
target_include_directories(ethernauta_core PUBLIC include)
target_compile_features(ethernauta_core PUBLIC cxx_std_17)
ethernauta_set_warnings(ethernauta_core)

# The DSP core must stay JUCE-free. The check is deferred to the end of the top-level configure so
# links added after this point are seen too, and it inspects both LINK_LIBRARIES and
# INTERFACE_LINK_LIBRARIES (recursing through non-JUCE targets).
function(ethernauta_assert_juce_free target)
  foreach(_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(_libs ${target} ${_prop})
    if(NOT _libs)
      continue()
    endif()
    foreach(_lib IN LISTS _libs)
      if(_lib MATCHES "(^|[:>])juce")
        message(FATAL_ERROR "ethernauta_core must not link JUCE targets (found: ${_lib})")
      endif()
      if(TARGET ${_lib})
        get_property(_seen GLOBAL PROPERTY ETHERNAUTA_JUCE_SCAN_${_lib})
        if(NOT _seen)
          set_property(GLOBAL PROPERTY ETHERNAUTA_JUCE_SCAN_${_lib} TRUE)
          ethernauta_assert_juce_free(${_lib})
        endif()
      endif()
    endforeach()
  endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL ethernauta_assert_juce_free ethernauta_core)

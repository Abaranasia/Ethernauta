# Run as a ctest: cmake -DLIBS=<link libraries of ethernauta_core> -P CheckNoJuceLinks.cmake
# Fails if any linked library looks like a JUCE target (guards the JUCE-free core at test time;
# Source/core/CMakeLists.txt guards it at configure time).
if(NOT DEFINED LIBS)
  message(FATAL_ERROR "LIBS not set")
endif()
foreach(_lib IN LISTS LIBS)
  if(_lib MATCHES "(^|[:>])juce|juce_")
    message(FATAL_ERROR "ethernauta_core links a JUCE target: ${_lib}")
  endif()
endforeach()
message(STATUS "ethernauta_core links no JUCE targets (libs: '${LIBS}')")

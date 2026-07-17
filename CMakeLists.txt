cmake_minimum_required(VERSION 3.25)

project(VaporwaveSuite VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MINGW)
  add_compile_definitions(
    _USE_MATH_DEFINES
    M_PI=3.14159265358979323846
    M_PI_2=1.57079632679489661923
  )
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
endif()

option(VAPORWAVE_BUILD_TESTS "Build DSP tests" ON)
option(VAPORWAVE_BUILD_PLUGIN "Build the iPlug2 VST3 target" OFF)
option(VAPORWAVE_WITH_RNNOISE "Enable the optional RNNoise adapter" OFF)
option(VAPORWAVE_WITH_DEEPFILTERNET "Enable the optional DeepFilterNet adapter" OFF)

add_library(vaporwave_dsp STATIC
  src/dsp/Processor.cpp
  src/dsp/SpectralDenoiser.cpp
)
target_include_directories(vaporwave_dsp PUBLIC include)
target_compile_features(vaporwave_dsp PUBLIC cxx_std_20)
target_compile_definitions(vaporwave_dsp PUBLIC
  VAPORWAVE_WITH_RNNOISE=$<BOOL:${VAPORWAVE_WITH_RNNOISE}>
  VAPORWAVE_WITH_DEEPFILTERNET=$<BOOL:${VAPORWAVE_WITH_DEEPFILTERNET}>
)

if(MSVC)
  target_compile_options(vaporwave_dsp PRIVATE /W4 /permissive-)
else()
  target_compile_options(vaporwave_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(VAPORWAVE_BUILD_TESTS)
  enable_testing()
  add_executable(vaporwave_dsp_tests tests/dsp_tests.cpp)
  target_link_libraries(vaporwave_dsp_tests PRIVATE vaporwave_dsp)
  add_test(NAME vaporwave_dsp_tests COMMAND vaporwave_dsp_tests)
endif()

if(VAPORWAVE_BUILD_PLUGIN)
  if(NOT DEFINED IPLUG2_DIR)
    message(FATAL_ERROR "Set IPLUG2_DIR to a pinned iPlug2 checkout.")
  endif()
  include(${IPLUG2_DIR}/iPlug2.cmake)
  find_package(iPlug2 REQUIRED)
  add_subdirectory(plugin)
endif()

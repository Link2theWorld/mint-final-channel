cmake_minimum_required(VERSION 3.25)
project(VaporwaveSuitePlugin VERSION 0.1.2)

set(MINT_UI_FONT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/VarelaRound.ttf")
set(MINT_UI_FONT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/MintUIFontData.h")
set(MINT_CJK_FONT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/GenSenRoundedSubset.ttf")
set(MINT_CJK_FONT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/MintCJKFontData.h")
file(READ "${MINT_UI_FONT_FILE}" MINT_UI_FONT_HEX HEX)
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," MINT_UI_FONT_BYTES "${MINT_UI_FONT_HEX}")
file(READ "${MINT_CJK_FONT_FILE}" MINT_CJK_FONT_HEX HEX)
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," MINT_CJK_FONT_BYTES "${MINT_CJK_FONT_HEX}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${MINT_UI_FONT_HEADER}"
  "#pragma once\ninline constexpr unsigned char kMintUIFontData[] = {${MINT_UI_FONT_BYTES}};\ninline constexpr int kMintUIFontDataSize = sizeof(kMintUIFontData);\n"
)
file(WRITE "${MINT_CJK_FONT_HEADER}"
  "#pragma once\ninline constexpr unsigned char kMintCJKFontData[] = {${MINT_CJK_FONT_BYTES}};\ninline constexpr int kMintCJKFontDataSize = sizeof(kMintCJKFontData);\n"
)

iplug_add_plugin(VaporwaveSuite
  SOURCES
    VaporwaveSuite.cpp
    VaporwaveSuite.h
    VaporwaveEditor.h
    config.h
  RESOURCES
    resources/fonts/VarelaRound.ttf
    resources/fonts/OFL-VarelaRound.txt
    resources/fonts/GenSenRoundedSubset.ttf
    resources/fonts/OFL-GenSenRounded.txt
  FORMATS
    VST3
  LINK
    vaporwave_dsp
)
target_include_directories(VaporwaveSuite-vst3 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

if(MSVC)
  target_compile_options(VaporwaveSuite-vst3 PRIVATE /utf-8)
  add_custom_command(TARGET VaporwaveSuite-vst3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/VarelaRound.ttf"
      "$<TARGET_FILE_DIR:VaporwaveSuite-vst3>/../Resources/VarelaRound.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/OFL-VarelaRound.txt"
      "$<TARGET_FILE_DIR:VaporwaveSuite-vst3>/../Resources/OFL-VarelaRound.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/GenSenRoundedSubset.ttf"
      "$<TARGET_FILE_DIR:VaporwaveSuite-vst3>/../Resources/GenSenRoundedSubset.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/OFL-GenSenRounded.txt"
      "$<TARGET_FILE_DIR:VaporwaveSuite-vst3>/../Resources/OFL-GenSenRounded.txt"
    COMMENT "Bundling the Mint UI font"
  )
  if(IGRAPHICS_BACKEND STREQUAL "SKIA")
    target_link_directories(VaporwaveSuite-vst3 PRIVATE
      "${IPLUG2_DIR}/Dependencies/Build/win/x64/$<CONFIG>")
  endif()
endif()

if(MINGW)
  target_link_libraries(VaporwaveSuite-vst3 PRIVATE opengl32)
  set_target_properties(VaporwaveSuite-vst3 PROPERTIES PREFIX "")
endif()

cmake_minimum_required(VERSION 3.16)

project(sndplayer_main CXX C)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(SNDPLAYER_DIR ${REPO_ROOT})
set(SRC_DIR ${SNDPLAYER_DIR}/src)

set(ENGINE_SOURCES
  ${SRC_DIR}/sound/989snd/player.cpp
  ${SRC_DIR}/sound/989snd/midi_handler.cpp
  ${SRC_DIR}/sound/989snd/ame_handler.cpp
  ${SRC_DIR}/sound/989snd/blocksound_handler.cpp
  ${SRC_DIR}/sound/989snd/musicbank.cpp
  ${SRC_DIR}/sound/989snd/sfxblock.cpp
  ${SRC_DIR}/sound/989snd/sfxgrain.cpp
  ${SRC_DIR}/sound/989snd/plugin.cpp
  ${SRC_DIR}/sound/989snd/loader.cpp
  ${SRC_DIR}/sound/989snd/vagvoice.cpp
  ${SRC_DIR}/sound/989snd/lfo.cpp
  ${SRC_DIR}/sound/989snd/util.cpp
  ${SRC_DIR}/sound/common/synth.cpp
  ${SRC_DIR}/sound/common/voice.cpp
  ${SRC_DIR}/sound/common/envelope.cpp
  ${SRC_DIR}/sound/sndshim.cpp
  ${SRC_DIR}/sound/sdshim.cpp
)

set(COMMON_SOURCES
  ${SRC_DIR}/common/log/log.cpp
  ${SRC_DIR}/common/util/Assert.cpp
)

add_library(main SHARED
  ${SRC_DIR}/player/main.cpp
  ${SRC_DIR}/player/sndplayer.cpp
  ${SRC_DIR}/player/android_platform.cpp
  ${ENGINE_SOURCES}
  ${COMMON_SOURCES}
)

target_compile_options(main PRIVATE
  -Wno-unknown-warning-option
  -Wno-unused-parameter
  -Wno-unused-private-field
  -Wno-shadow
)

target_link_libraries(main PRIVATE
  SDL3::SDL3
  imgui
  fmt
  cubeb
  android
  log
  EGL
  GLESv3
)

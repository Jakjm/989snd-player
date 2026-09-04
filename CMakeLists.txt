cmake_minimum_required(VERSION 3.20)

project(sndplayer LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
add_subdirectory(third-party/SDL      EXCLUDE_FROM_ALL)
add_subdirectory(third-party/fmt      EXCLUDE_FROM_ALL)
add_subdirectory(third-party/cubeb    EXCLUDE_FROM_ALL)
add_subdirectory(third-party/imgui    EXCLUDE_FROM_ALL)

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/third-party/fmt/include
  ${CMAKE_SOURCE_DIR}/third-party/SDL/include
  ${CMAKE_SOURCE_DIR}/third-party/glad/include
)

set(SNDPLAYER_ENGINE_SOURCES
  src/sound/989snd/player.cpp
  src/sound/989snd/midi_handler.cpp
  src/sound/989snd/ame_handler.cpp
  src/sound/989snd/blocksound_handler.cpp
  src/sound/989snd/musicbank.cpp
  src/sound/989snd/sfxblock.cpp
  src/sound/989snd/sfxgrain.cpp
  src/sound/989snd/plugin.cpp
  src/sound/989snd/loader.cpp
  src/sound/989snd/vagvoice.cpp
  src/sound/989snd/lfo.cpp
  src/sound/989snd/util.cpp
  src/sound/common/synth.cpp
  src/sound/common/voice.cpp
  src/sound/common/envelope.cpp
  src/sound/sndshim.cpp
  src/sound/sdshim.cpp
)

set(SNDPLAYER_COMMON_SOURCES
  src/common/log/log.cpp
  src/common/util/Assert.cpp
)

add_executable(sndplayer
  src/player/main.cpp
  src/player/midi_timeline.cpp
  src/player/sndplayer.cpp
  ${SNDPLAYER_ENGINE_SOURCES}
  ${SNDPLAYER_COMMON_SOURCES}
  third-party/glad/src/glad.c
)

target_include_directories(sndplayer BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/player
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sndplayer PRIVATE
  SDL3::SDL3
  imgui
  fmt
  cubeb
)

if(NOT WIN32 AND NOT APPLE)
  target_link_libraries(sndplayer PRIVATE stdc++fs)
endif()

if(NOT WIN32)
  target_compile_options(sndplayer PRIVATE
    -Wno-unknown-warning-option
    -Wno-unused-parameter
    -Wno-unused-private-field
    -Wno-shadow
  )
endif()

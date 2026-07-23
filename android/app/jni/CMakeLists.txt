cmake_minimum_required(VERSION 3.16)

project(sndplayer_android CXX C)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(SNDPLAYER_DIR ${REPO_ROOT})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
  ${SNDPLAYER_DIR}/src
  ${SNDPLAYER_DIR}/src/player
  ${SNDPLAYER_DIR}
  ${REPO_ROOT}/third-party
  ${REPO_ROOT}/third-party/fmt/include
  ${REPO_ROOT}/third-party/SDL/include
)

set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
add_subdirectory(${REPO_ROOT}/third-party/SDL SDL EXCLUDE_FROM_ALL)
add_subdirectory(${REPO_ROOT}/third-party/fmt fmt EXCLUDE_FROM_ALL)
add_subdirectory(${REPO_ROOT}/third-party/cubeb cubeb EXCLUDE_FROM_ALL)
add_subdirectory(${REPO_ROOT}/third-party/imgui imgui EXCLUDE_FROM_ALL)
add_subdirectory(src)

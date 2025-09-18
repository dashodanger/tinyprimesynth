##########################################
# tpsplayer
##########################################
cmake_minimum_required(VERSION 3.25)

project(
  tpsplayer
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(
  tpsplayer
  tpsplayer-sapp.cc
)

# sokol_app/sokol_audio linked libraries
if (WIN32 OR MINGW)
  target_link_libraries(tpsplayer winmm kernel32)
elseif (CMAKE_HOST_LINUX)
  target_link_libraries(tpsplayer asound)
endif()

set(COPY_FILES "")
set (DEST_DIR "${CMAKE_SOURCE_DIR}")
list(APPEND COPY_FILES "$<TARGET_FILE:tpsplayer>")
list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/csound.sf2")
list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/ant_farm_melee.mid")
add_custom_command( TARGET tpsplayer POST_BUILD COMMAND_EXPAND_LISTS COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COPY_FILES} ${DEST_DIR})
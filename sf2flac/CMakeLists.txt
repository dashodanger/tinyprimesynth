##########################################
# sf2flac
##########################################
cmake_minimum_required(VERSION 3.5)

project(
  sf2flac
  LANGUAGES C
)

add_executable(
  sf2flac
  sf2flac.c
)

add_custom_command( TARGET sf2flac POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:sf2flac>" ${CMAKE_SOURCE_DIR})
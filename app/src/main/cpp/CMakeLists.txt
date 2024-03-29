cmake_minimum_required(VERSION 3.16)

project(zen_mirror C CXX)
message(STATUS "Using CMake version: ${CMAKE_VERSION}")

set(CMAKE_CXX_STANDARD 17)
add_definitions(-DXR_USE_PLATFORM_ANDROID)
add_definitions(-DXR_USE_GRAPHICS_API_OPENGL_ES)

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/config.h.in 
  ${CMAKE_CURRENT_BINARY_DIR}/include/config.h
)

# zen-remote
set(ZEN_REMOTE_REQUIRED_VERSION 0.1.2)
set(ZEN_REMOTE_SERVER OFF)
set(ZEN_REMOTE_CLIENT ON)
set(ZEN_REMOTE_GRAPHICS_API GLESv3)
add_subdirectory(
  ${PROJECT_DIR}/3rdParty/zen-remote
  zen-remote
)

# Don't call FetchContent_MakeAvailable so that add_subdirectories is not called
include(FetchContent)
FetchContent_Declare(
  openxr_sdk_content
  GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
  GIT_TAG release-1.0.25
)
if(NOT openxr_sdk_content_POPULATED)
  FetchContent_Populate(openxr_sdk_content)
endif()


# android
find_path(
  android_native_app_glue_dir 
  android_native_app_glue.h
  PATH ${ANDROID_NDK}/sources/android/native_app_glue
)

find_library(android_library NAMES android REQUIRED)
find_library(android_log_library NAMES log REQUIRED)

add_library(
  android_native_app_glue_object OBJECT
  ${android_native_app_glue_dir}/android_native_app_glue.c
)
## suppress warning
target_compile_options(android_native_app_glue_object PRIVATE -w)


# OpenGLES v3
find_library(opengles_v3_library NAMES GLESv3 REQUIRED)
find_library(egl_library NAMES EGL REQUIRED)


# OpenXR loader
set(OPENXR_BUILD_TYPE Release) 
if (${CMAKE_BUILD_TYPE} STREQUAL Debug)
  set(OPENXR_BUILD_TYPE Debug) 
endif()

add_library(openxr_loader SHARED IMPORTED)
set_property(
  TARGET openxr_loader
  PROPERTY
    IMPORTED_LOCATION ${OVR_OPENXR_MOBILE_SDK_DIR}/OpenXR/Libs/Android/${ANDROID_ABI}/${OPENXR_BUILD_TYPE}/libopenxr_loader.so
)


# main target
add_library(
  zen_mirror MODULE
  
  android-logger.cc
  egl-instance.cc
  loop.cc
  main.cc
  openxr-action-source.cc
  openxr-context.cc
  openxr-event-source.cc
  openxr-view-source.cc
  remote-log-sink.cc
  remote-loop.cc
  $<TARGET_OBJECTS:android_native_app_glue_object>
)
target_precompile_headers(zen_mirror PRIVATE pch.h)

target_link_libraries(
  zen_mirror

  PRIVATE
    glm
    openxr_loader
    zen_remote::client
    ${android_library}
    ${android_log_library}
    ${egl_library}
    ${opengles_v3_library}
)

target_include_directories(
  zen_mirror

  PRIVATE
    ${openxr_sdk_content_SOURCE_DIR}/include
    ${android_native_app_glue_dir}
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_compile_options(
  zen_mirror

  PRIVATE
    -Werror -Wall -Winvalid-pch -Wextra -Wpedantic
    -Wno-gnu-zero-variadic-macro-arguments
    -Wno-missing-field-initializers
)

# For development purpose
execute_process(
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${PROJECT_DIR}/.cxx/${CMAKE_BUILD_TYPE}/compile_commands.json
)

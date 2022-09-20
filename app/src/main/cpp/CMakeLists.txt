cmake_minimum_required(VERSION 3.16)

project(zen_oculus_display_system C CXX)
message(STATUS "Using CMake version: ${CMAKE_VERSION}")

add_definitions(-DXR_USE_PLATFORM_ANDROID)
add_definitions(-DXR_USE_GRAPHICS_API_OPENGL_ES)

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/config.h.in 
    ${CMAKE_CURRENT_BINARY_DIR}/include/config.h
)

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
set(OPENXR_BUILD_TYPE "Release") 
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(OPENXR_BUILD_TYPE Debug) 
endif()

add_library(openxr_loader SHARED IMPORTED)
set_property(
    TARGET openxr_loader
    PROPERTY
        IMPORTED_LOCATION
        ${OVR_OPENXR_MOBILE_SDK_DIR}/OpenXR/Libs/Android/${ANDROID_ABI}/${OPENXR_BUILD_TYPE}/libopenxr_loader.so
)

# Setup Boost
set(Boost_VERSION 1.80.0)
set(Boost_CMAKE_DIR ${PROJECT_DIR}/build/boost/lib/cmake)

## Search for all available component-configuration directories
file(GLOB all_components LIST_DIRECTORIES true 
    RELATIVE ${Boost_CMAKE_DIR} ${Boost_CMAKE_DIR}/boost_*-${Boost_VERSION})
## ...and extract components names from it.
string(REGEX REPLACE "boost_([_a-z0-9]+)-${Boost_VERSION}" "\\1"
    all_components "${all_components}")

set(Boost_DIR ${Boost_CMAKE_DIR}/Boost-${Boost_VERSION})
foreach(component IN LISTS all_components)
    set(boost_${component}_DIR ${Boost_CMAKE_DIR}/boost_${component}-${Boost_VERSION})
endforeach()

find_package(Boost CONFIG REQUIRED)

# main target
add_library(
    zen_oculus_display_system MODULE
    
    logger-android.cc
    main.cc
    openxr-program.cc
    $<TARGET_OBJECTS:android_native_app_glue_object>
)
target_precompile_headers(zen_oculus_display_system PRIVATE pch.h)

target_link_libraries(
    zen_oculus_display_system

    openxr_loader
    ${android_library}
    ${android_log_library}
    ${egl_library}
    ${opengles_v3_library}
)

target_include_directories(
    zen_oculus_display_system

    PRIVATE ${ROOT_PROJECT_DIR}/3rdParty/OpenXR-SDK/include
    PRIVATE ${android_native_app_glue_dir}
    PRIVATE ${Boost_INCLUDE_DIRS}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include/
)

# For development purpose
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${PROJECT_DIR}/.cxx/${CMAKE_BUILD_TYPE}/compile_commands.json
)

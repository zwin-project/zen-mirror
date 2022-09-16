cmake_minimum_required(VERSION 3.16)

project(zen_oculus_display_system C CXX)
message(STATUS "Using CMake version: ${CMAKE_VERSION}")

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
# suppress warning
target_compile_options(android_native_app_glue_object PRIVATE -w)

set(Boost_DIR ${PROJECT_DIR}/build/boost)
set(Boost_INCLUDE_DIR ${PROJECT_DIR}/build/boost/include)
find_package(Boost REQUIRED)

# main target
add_library(
    zen_oculus_display_system MODULE
    
    main.cpp
    $<TARGET_OBJECTS:android_native_app_glue_object>
)
target_precompile_headers(zen_oculus_display_system PRIVATE pch.h)

target_link_libraries(
    zen_oculus_display_system
    ${android_library}
    ${android_log_library}
)

target_include_directories(
    zen_oculus_display_system
    PRIVATE ${android_native_app_glue_dir}
    PRIVATE ${Boost_INCLUDE_DIRS}
)

# For development purpose
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${PROJECT_DIR}/.cxx/${CMAKE_BUILD_TYPE}/compile_commands.json
)
cmake_minimum_required(VERSION 3.20)

project("SteamVRMirrorPreview" LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "steamvr_mirror_preview currently supports Windows only.")
endif()

# OpenVR SDK — from git submodule libraries/openvr
set(OPENVR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/openvr/headers")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PROCESSOR_ARCH "64")
else()
    set(PROCESSOR_ARCH "32")
endif()

find_library(
    OPENVR_LIB
    NAMES openvr_api
    HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/openvr/lib/win${PROCESSOR_ARCH}"
    NO_DEFAULT_PATH
    REQUIRED
)

add_executable(
    steamvr_mirror_preview
    WIN32
    SteamVRMirrorPreview.cpp
)

target_compile_definitions(
    steamvr_mirror_preview
    PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_include_directories(
    steamvr_mirror_preview
    SYSTEM PRIVATE
    "${OPENVR_INCLUDE_DIR}"
)

target_link_libraries(
    steamvr_mirror_preview
    PRIVATE
    "${OPENVR_LIB}"
    d3d11
    dxgi
    d3dcompiler
)

set_property(TARGET steamvr_mirror_preview PROPERTY VS_GLOBAL_VcpkgEnabled false)

add_custom_command(
    TARGET steamvr_mirror_preview
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/openvr/bin/win${PROCESSOR_ARCH}/openvr_api.dll"
        "$<TARGET_FILE_DIR:steamvr_mirror_preview>/openvr_api.dll"
)


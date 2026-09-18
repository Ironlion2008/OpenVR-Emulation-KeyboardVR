cmake_minimum_required(VERSION 3.16)

# Solution
project("OpenVR_Emulation_Driver")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# VS 2022 injects a vcpkg AppLocal post-build step into every C++ project that
# calls pwsh.exe to copy vcpkg DLLs. This project doesn't use vcpkg, so disable
# the integration to suppress the 'pwsh.exe is not recognized' build warning.
set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")

set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PROCESSOR_ARCH "64")
else()
    set(PROCESSOR_ARCH "32")
endif()

if(WIN32)
    set(PLATFORM_NAME "win")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_NAME "linux")
elseif(APPLE)
    set(PLATFORM_NAME "osx")
endif()


# ============================================================
# OpenVR SDK
# ============================================================
#
# CI builds provide the SDK through:
#     -DOPENVR_ROOT=<workspace>/openvr-sdk
#
# Local builds continue using:
#     libraries/openvr
#
# ============================================================

if(DEFINED OPENVR_ROOT)
    message(STATUS "Using external OpenVR SDK: ${OPENVR_ROOT}")

    set(OPENVR_INCLUDE_DIR
        "${OPENVR_ROOT}/headers"
    )

    set(OPENVR_LIB_DIR
        "${OPENVR_ROOT}/lib/win${PROCESSOR_ARCH}"
    )
else()
    message(STATUS "Using bundled OpenVR SDK")

    set(OPENVR_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/libraries/openvr/headers"
    )

    set(OPENVR_LIB_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/libraries/openvr/lib/win${PROCESSOR_ARCH}"
    )
endif()


find_library(
    OPENVR_LIB
    NAMES openvr_api
    HINTS "${OPENVR_LIB_DIR}"
    NO_DEFAULT_PATH
)

if(NOT OPENVR_LIB)
    message(FATAL_ERROR
        "Could not find OpenVR library 'openvr_api'.\n"
        "Searched in: ${OPENVR_LIB_DIR}\n"
        "OpenVR root: ${OPENVR_ROOT}"
    )
endif()

message(STATUS "OpenVR include directory: ${OPENVR_INCLUDE_DIR}")
message(STATUS "OpenVR library: ${OPENVR_LIB}")


# ============================================================
# Driver
# ============================================================

set(DRIVER_NAME "openvr-emulator")
set(DRIVER_PROJECT "driver_${DRIVER_NAME}")

file(
    GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src/*.hpp"
)

file(
    GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src/*.cpp"
)

add_library(
    "${DRIVER_PROJECT}"
    SHARED
    "${HEADERS}"
    "${SOURCES}"
)


# SYSTEM marks OpenVR as a third-party include so MSVC suppresses its warnings.
# Because the include dirs come from a Conan imported target, we extract them
# explicitly and re-add them via SYSTEM so /external:I is applied.

target_include_directories(
    "${DRIVER_PROJECT}"
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src/"
)

target_include_directories(
    "${DRIVER_PROJECT}"
    SYSTEM
    PRIVATE
    "${OPENVR_INCLUDE_DIR}"
)

target_link_libraries(
    "${DRIVER_PROJECT}"
    PRIVATE
    "${OPENVR_LIB}"
    xinput
)

source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src"
    PREFIX "Header Files"
    FILES ${HEADERS}
)

source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src"
    PREFIX "Source Files"
    FILES ${SOURCES}
)

target_compile_features(
    "${DRIVER_PROJECT}"
    PRIVATE
    cxx_std_20
)


# ============================================================
# Compiler settings
# ============================================================

set_target_properties(
    "${DRIVER_PROJECT}"
    PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(
        "${DRIVER_PROJECT}"
        PRIVATE
        /W4
        /permissive-
        /external:W0
    )
endif()


# ============================================================
# Optional clang-tidy static analysis
# ============================================================

include(scripts/static_checking.cmake)
enable_clang_tidy("${DRIVER_PROJECT}")


# ============================================================
# Visual Studio startup project
# ============================================================

set_property(
    DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY
    VS_STARTUP_PROJECT "${DRIVER_PROJECT}"
)


# ============================================================
# Preview application
# ============================================================

# Keep the preview application in normal/local builds.
# CI can disable it with -DBUILD_PREVIEW_APP=OFF.

option(
    BUILD_PREVIEW_APP
    "Build the preview application"
    ON
)

if(BUILD_PREVIEW_APP)
    add_subdirectory(preview_app)
endif()


# ============================================================
# Unit tests
# ============================================================
#
# Tests require GTest/GMock.
# The driver itself does NOT require them.
#
# Default is OFF so CI builds do not need GTest.
#
# To build tests locally:
#     cmake -DBUILD_TESTING=ON ...
#
# ============================================================

option(
    BUILD_TESTING
    "Build GTest/GMock unit tests"
    OFF
)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()


# ============================================================
# Visual Studio debugger
# ============================================================

set_target_properties(
    "${DRIVER_PROJECT}"
    PROPERTIES
    VS_DEBUGGER_COMMAND
    "C:/Program Files (x86)/Steam/steamapps/common/SteamVR/bin/win64/vrstartup.exe"
)


# ============================================================
# Copy driver assets to output folder
# ============================================================

add_custom_command(
    TARGET ${DRIVER_PROJECT}
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND}
        -E copy_directory
        ${CMAKE_SOURCE_DIR}/driver_files/driver/
        $<TARGET_FILE_DIR:${DRIVER_PROJECT}>
)


# ============================================================
# Copy DLL to driver folder
# ============================================================

add_custom_command(
    TARGET ${DRIVER_PROJECT}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -E copy
        $<TARGET_FILE:${DRIVER_PROJECT}>
        $<TARGET_FILE_DIR:${DRIVER_PROJECT}>/${DRIVER_NAME}/bin/${PLATFORM_NAME}${PROCESSOR_ARCH}/$<TARGET_FILE_NAME:${DRIVER_PROJECT}>
)


# ============================================================
# SteamVR deployment
# ============================================================

option(
    OPENVR_EMULATOR_SKIP_DEPLOY
    "Skip automatic SteamVR driver deployment"
    OFF
)

set(
    STEAMVR_DRIVERS_DIR
    "C:/Program Files (x86)/Steam/steamapps/common/SteamVR/drivers"
)

if(NOT OPENVR_EMULATOR_SKIP_DEPLOY)

    add_custom_command(
        TARGET ${DRIVER_PROJECT}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDRIVER_NAME=${DRIVER_NAME}
            -DSRC_DIR=$<TARGET_FILE_DIR:${DRIVER_PROJECT}>
            -DDST_DIR=${STEAMVR_DRIVERS_DIR}
            -P ${CMAKE_SOURCE_DIR}/scripts/deploy_driver.cmake
        COMMENT "Deploying ${DRIVER_NAME} driver to SteamVR"
    )

endif()

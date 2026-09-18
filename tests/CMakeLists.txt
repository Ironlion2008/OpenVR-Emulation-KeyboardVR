cmake_minimum_required(VERSION 3.16)
project(driver_tests CXX)

# GTest is provided by Conan (conanfile.py test_requires gtest/1.14.0).
# Run `conan install . --build=missing` before configuring CMake.
find_package(GTest REQUIRED)

set(DRIVER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../driver_files/src")
# OpenVR headers are still needed to compile InputConfig.cpp (it includes openvr_driver.h
# transitively via InputConfig.hpp → Windows.h/Xinput.h).  Use the vendored headers so
# the tests can build without a full Conan-generated toolchain being active.
set(OPENVR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/openvr/headers")

add_executable(driver_tests
    InputMathTest.cpp
    InputConfigTest.cpp
    # Compile InputConfig directly (it has no OpenVR runtime dependency)
    "${DRIVER_SRC}/Driver/InputConfig.cpp"
)

target_compile_features(driver_tests PRIVATE cxx_std_20)
set_target_properties(driver_tests PROPERTIES
    CXX_EXTENSIONS OFF
    # ConanCenter gtest is built with /MD (MD_DynamicRelease). Pin the runtime
    # to MultiThreadedDLL for all configs so Debug builds don't default to /MT.
    MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
)

target_include_directories(driver_tests
    PRIVATE
        "${DRIVER_SRC}"
)
# OpenVR headers as SYSTEM to suppress third-party warnings
target_include_directories(driver_tests SYSTEM PRIVATE "${OPENVR_INCLUDE_DIR}")

target_link_libraries(driver_tests
    PRIVATE
        GTest::gtest_main
        GTest::gmock
)

if(MSVC)
    target_compile_options(driver_tests PRIVATE /W4 /permissive- /external:W0)
endif()

include(GoogleTest)
gtest_discover_tests(driver_tests)

enable_clang_tidy(driver_tests)

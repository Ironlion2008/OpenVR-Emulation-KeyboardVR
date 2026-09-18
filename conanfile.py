from conan import ConanFile
from conan.tools.cmake import cmake_layout


class OpenVREmulationDriver(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain"

    def requirements(self):
        # Google Test + Mock (test-only dependency)
        self.test_requires("gtest/1.17.0")

    def layout(self):
        # cmake_layout organises build artefacts per-configuration and
        # generates CMakeUserPresets.json at the project root so VS / CLion
        # can discover presets automatically.
        cmake_layout(self)

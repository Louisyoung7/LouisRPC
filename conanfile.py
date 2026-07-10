from conan import ConanFile
from conan.tools.cmake import cmake_layout


class LouisRPCRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = "protobuf/4.25.3", "spdlog/1.14.1"
    generators = "CMakeToolchain", "CMakeDeps"

    def layout(self):
        cmake_layout(self)

    def build_requirements(self):
        self.tool_requires("protobuf/4.25.3")

    def test_requirements(self):
        self.test_requires("gtest/1.14.0")

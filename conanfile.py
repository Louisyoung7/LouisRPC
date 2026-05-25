from conan import ConanFile
from conan.tools.cmake import cmake_layout


class LouisRPCRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = "protobuf/3.21.12"
    build_requires = "protobuf/3.21.12"
    generators = "CMakeToolchain", "CMakeDeps"

    def layout(self):
        cmake_layout(self)

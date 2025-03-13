import os
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class paraclRecipe(ConanFile):
    name = "paracl"
    version = "1.0"
    user = "baitim"

    license = ""
    author = ""
    url = ""
    description = "paracl"
    topics = ""

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}
    test_requires = "gtest/1.15.0"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "tests/*"

    def configure(self):
        if self.settings.compiler == "msvc":
            self.settings.compiler.cppstd = "20"  # MSVC only supports "20", not "gnu20"
        elif "gnu" in str(self.settings.compiler):
            self.settings.compiler.cppstd = "gnu20"  # GCC uses "gnuXX" for GNU extensions
        else:
            self.settings.compiler.cppstd = "20"  # Default to "20" for other compilers

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

        # Delete CMakeUserPresets.json
        cmake_user_presets = os.path.join(self.source_folder, "CMakeUserPresets.json")
        if os.path.exists(cmake_user_presets):
            os.remove(cmake_user_presets)

    def build(self):
        os.environ["CONAN_PACKAGE"] = "1"
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "config")
        self.cpp_info.set_property("cmake_file_name", "ParaCL")
        self.cpp_info.set_property("cmake_target_name", "ParaCL::ParaCL")
        self.cpp_info.libs = ["ParaCL"]
        self.cpp_info.includedirs = ["include"]
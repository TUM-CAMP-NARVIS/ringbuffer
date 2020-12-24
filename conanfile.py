from conans import ConanFile, CMake
from conans import tools
from conans.tools import os_info, SystemPackageTool
import os, sys
import sysconfig
from io import StringIO

class RingbufferConan(ConanFile):
    name = "ringbuffer"
    version = "0.2.1"

    description = "Ringbuffer Library"
    url = "https://github.com/TUM-CAMP-NARVIS/ringbuffer"
    license = "GPL"

    short_paths = True
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "virtualrunenv"

    options = {
        "shared": [True, False],
        "with_cuda": [True, False],
        "with_omp": [True, False],
        "with_numa": [True, False],
        "with_nvtoolsext": [True, False],
        "enable_fibers": [True, False],
        "enable_debug": [True, False],
        "enable_trace": [True, False],
    }

    requires = (
        "Boost/1.72.0@camposs/stable",
        "gtest/1.10.0",
        "spdlog/1.8.2",
        )

    default_options = {
        "shared": True,
        "with_cuda": False,
        "with_omp": False,
        "with_numa": False,
        "with_nvtoolsext": False,
        "enable_fibers": False,
        "enable_debug": False,
        "enable_trace": False,
    }

    # all sources are deployed with the package
    exports_sources = "modules/*", "include/*", "src/*", "tests/*", "CMakeLists.txt"

    def requirements(self):
        if self.options.with_cuda:
            self.requires("cuda_dev_config/[>=1.0]@camposs/stable")

        if self.options.enable_fibers:
            self.requires("fiberpool/0.1@camposs/stable")


    def system_requirements(self):
        if tools.os_info.is_linux:
            pack_names = []
            if self.options.with_numa:
                pack_names.append("libnuma-dev")

            if self.options.with_omp:
                pack_names.append("libomp-dev")

            installer = tools.SystemPackageTool()
            for p in pack_names:
                installer.install(p)

    def configure(self):
        if self.options.shared:
            self.options['Boost'].shared = True
        if self.options.enable_fibers:
            self.options['Boost'].without_fiber = False

    def imports(self):
        self.copy(src="bin", pattern="*.dll", dst="./bin") # Copies all dll files from packages bin folder to my "bin" folder
        self.copy(src="lib", pattern="*.dll", dst="./bin") # Copies all dll files from packages bin folder to my "bin" folder
        self.copy(src="lib", pattern="*.dylib*", dst="./lib") # Copies all dylib files from packages lib folder to my "lib" folder
        self.copy(src="lib", pattern="*.so*", dst="./lib") # Copies all so files from packages lib folder to my "lib" folder
        self.copy(src="lib", pattern="*.a", dst="./lib") # Copies all static libraries from packages lib folder to my "lib" folder
        self.copy(src="bin", pattern="*", dst="./bin") # Copies all applications


    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.verbose = True

        def add_cmake_option(option, value):
            var_name = "{}".format(option).upper()
            value_str = "{}".format(value)
            var_value = "ON" if value_str == 'True' else "OFF" if value_str == 'False' else value_str
            cmake.definitions[var_name] = var_value

        for option, value in self.options.items():
            add_cmake_option(option, value)

        cmake.configure()
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()

    def package_info(self):
        # self.cpp_info.libs = tools.collect_libs(self)  # doesn't work in workspace builds ..
        self.cpp_info.libs = ['ringbuffer']


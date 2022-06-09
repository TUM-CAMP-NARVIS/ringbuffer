from conans import ConanFile, CMake
from conans import tools
from conans.tools import os_info, SystemPackageTool
import os, sys
import sysconfig
from io import StringIO

class RingbufferConan(ConanFile):
    python_requires = "camp_common/[>=0.1]@camposs/stable"
    python_requires_extend = "camp_common.CampCMakeBase"

    name = "ringbuffer"
    version = "0.2.8"

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
        "Boost/1.79.0@camposs/stable",
        "gtest/1.10.0",
        "spdlog/1.9.1",
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
            self.requires("cuda_dev_config/[>=2.0]@camposs/stable")

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

    def package_info(self):
        # self.cpp_info.libs = tools.collect_libs(self)  # doesn't work in workspace builds ..
        self.cpp_info.libs = ['ringbuffer']


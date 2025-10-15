from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain

class LightMaker(ConanFile):
    settings = "os", "build_type"

    def requirements(self):
        self.requires("tbb/2021.5")
        self.requires("openssl/3.3.1")
        self.requires("libcurl/8.9.1")
        self.requires("zlib/1.3.1")
        self.requires("eigen/3.4.0")
        self.requires("expat/2.6.2")
        self.requires("libpng/1.6.43")
        self.requires("glew/2.2.0")
        self.requires("glfw/3.4")
        self.requires("nlopt/2.7.1")
        self.requires("cereal/1.3.0")
        self.requires("openvdb/11.0.0")
        self.requires("libnoise/1.0.1")
        self.requires("libjpeg-turbo/3.0.1.1")
        self.requires("wxwidgets/3.1.5.1")
        self.requires("boost/1.83.0")
        self.requires("cgal/5.6.1")
        self.requires("opencascade/7.6.2")
        self.requires("opencv/4.5.5")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()
        cd = CMakeDeps(self)
        cd.generate()
#boost
# Find and configure boost
if(SLIC3R_STATIC)
    # Use static boost libraries.
    set(Boost_USE_STATIC_LIBS ON)
    # Use boost libraries linked statically to the C++ runtime.
    # set(Boost_USE_STATIC_RUNTIME ON)
endif()
#set(Boost_DEBUG ON)
# set(Boost_COMPILER "-mgw81")
# boost::process was introduced first in version 1.64.0,
# boost::beast::detail::base64 was introduced first in version 1.66.0
find_package(Boost 1.66 REQUIRED COMPONENTS system filesystem thread log log_setup locale regex chrono atomic date_time iostreams program_options)

add_library(boost_libs INTERFACE)
add_library(boost_headeronly INTERFACE)

if (APPLE)
    # BOOST_ASIO_DISABLE_KQUEUE : prevents a Boost ASIO bug on OS X: https://svn.boost.org/trac/boost/ticket/5339
    target_compile_definitions(boost_headeronly INTERFACE BOOST_ASIO_DISABLE_KQUEUE)
endif()

target_include_directories(boost_headeronly INTERFACE ${Boost_INCLUDE_DIRS})
target_link_libraries(boost_libs INTERFACE boost_headeronly ${Boost_LIBRARIES})
message(STATUS "Boost include dir: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost libraries: ${Boost_LIBRARIES}")

if(WIN32)
    if(MSVC)
        # BOOST_ALL_NO_LIB: Avoid the automatic linking of Boost libraries on Windows. Rather rely on explicit linking.
        # remove -DBOOST_USE_WINAPI_VERSION=0x602
        add_definitions(-DBOOST_USE_WINAPI_VERSION=0x601)
        add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_SYSTEM_USE_UTF8 )
        # Force the source code encoding to UTF-8. See OrcaSlicer GH pull request #5583
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    endif(MSVC)
endif()

#tbb
# Find and configure intel-tbb
if(SLIC3R_STATIC)
    set(TBB_STATIC 1)
endif()
set(TBB_DEBUG 1)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release "")
find_package(TBB REQUIRED)
# include_directories(${TBB_INCLUDE_DIRS})
# add_definitions(${TBB_DEFINITIONS})
# if(MSVC)
#     # Suppress implicit linking of the TBB libraries by the Visual Studio compiler.
#     add_definitions(-D__TBB_NO_IMPLICIT_LINKAGE)
# endif()
# The Intel TBB library will use the std::exception_ptr feature of C++11.
# add_definitions(-DTBB_USE_CAPTURED_EXCEPTION=0)


#openssl 
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)


add_library(libcurl INTERFACE)
target_link_libraries(libcurl INTERFACE CURL::libcurl)

find_package(ZLIB REQUIRED)
target_link_libraries(libcurl INTERFACE ZLIB::ZLIB)

# Fixing curl's cmake config script bugs
if (NOT WIN32)
    # Required by libcurl
    #find_package(ZLIB REQUIRED)
    #target_link_libraries(libcurl INTERFACE ZLIB::ZLIB)
    #find_package(Libssh2 REQUIRED)
    #target_link_libraries(libcurl INTERFACE Libssh2::libssh2)
else()
    target_link_libraries(libcurl INTERFACE crypt32)
endif()

if (SLIC3R_STATIC AND NOT SLIC3R_STATIC_EXCLUDE_CURL)
    if (NOT APPLE)
        # libcurl is always linked dynamically to the system libcurl on OSX.
        # On other systems, libcurl is linked statically if SLIC3R_STATIC is set.
        target_compile_definitions(libcurl INTERFACE CURL_STATICLIB)
    endif()
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # As of now, our build system produces a statically linked libcurl,
        # which links the OpenSSL library dynamically.
        find_package(OpenSSL REQUIRED)
        message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
        message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
        target_include_directories(libcurl INTERFACE ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(libcurl INTERFACE ${OPENSSL_LIBRARIES})
    endif()
endif()

## OPTIONAL packages

find_package(Eigen3 3.3 REQUIRED)
set(EIGEN3_INCLUDE_DIR ${Eigen3_INCLUDE_DIR})
include_directories(${EIGEN3_INCLUDE_DIR})

# Find expat or use bundled version
# Always use the system libexpat on Linux.

find_package(EXPAT REQUIRED)
set(EXPAT_LIBRARIES expat::expat)

find_package(PNG REQUIRED)

set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL REQUIRED)

set(GLEW_ROOT "${CMAKE_PREFIX_PATH}")
message("GLEW_ROOT: ${GLEW_ROOT}")
# Find glew or use bundled version
if (SLIC3R_STATIC AND NOT SLIC3R_STATIC_EXCLUDE_GLEW)
    set(GLEW_USE_STATIC_LIBS ON)
    set(GLEW_VERBOSE ON)
endif()

find_package(GLEW CONFIG REQUIRED)

find_package(glfw3 CONFIG REQUIRED)

# Find the Cereal serialization library
find_package(cereal CONFIG REQUIRED)
if (NOT TARGET cereal::cereal)
    set_target_properties(cereal PROPERTIES IMPORTED_GLOBAL TRUE)
    add_library(cereal::cereal ALIAS cereal)
else ()
    set_target_properties(cereal::cereal PROPERTIES IMPORTED_GLOBAL TRUE)
endif ()


#nlopt
find_package(NLopt CONFIG REQUIRED)

if(SLIC3R_STATIC)
    set(OPENVDB_USE_STATIC_LIBS ON)
    set(USE_BLOSC TRUE)
endif ()

find_package(OpenVDB 5.0 COMPONENTS openvdb)
if(OpenVDB_FOUND)
    slic3r_remap_configs(IlmBase::Half RelWithDebInfo Release)
    slic3r_remap_configs(Blosc::blosc RelWithDebInfo Release)
else ()
    message(FATAL_ERROR "OpenVDB could not be found with the bundled find module. "
                   "You can try to specify the find module location of your "
                   "OpenVDB installation with the OPENVDB_FIND_MODULE_PATH cache variable.")
endif ()

find_path(SPNAV_INCLUDE_DIR spnav.h)
if (SPNAV_INCLUDE_DIR)
    find_library(HAVE_SPNAV spnav)
    if (HAVE_SPNAV)
        add_definitions(-DHAVE_SPNAV)
        add_library(libspnav SHARED IMPORTED)
        target_link_libraries(libspnav INTERFACE spnav)
        message(STATUS "SPNAV library found")
    else()
        message(STATUS "SPNAV library NOT found, Spacenavd not supported")
    endif()
else()
    message(STATUS "SPNAV library NOT found, Spacenavd not supported")
endif()

cmake_policy(PUSH)
cmake_policy(SET CMP0011 NEW)
find_package(CGAL CONFIG REQUIRED)
if(NOT TARGET CGAL::CGAL)
    add_library(CGAL INTERFACE)
    add_library(CGAL::CGAL ALIAS CGAL)
endif()
find_package(OpenCV CONFIG REQUIRED COMPONENTS core)
if(NOT TARGET opencv_world)
    add_library(opencv_world INTERFACE)
    target_link_libraries(opencv_world INTERFACE opencv::opencv)
endif()
cmake_policy(POP)

# Find the OCCT and related libraries
set(OpenCASCADE_DIR "${CMAKE_PREFIX_PATH}/lib/cmake/occt")
find_package(OpenCASCADE REQUIRED)

find_package(libjpeg-turbo CONFIG REQUIRED)
add_library(JPEG::JPEG ALIAS libjpeg-turbo::libjpeg-turbo)
find_package(TIFF QUIET)

set(OCCT_LIBS
    TKXDESTEP
    TKSTEP
    TKSTEP209
    TKSTEPAttr
    TKSTEPBase
    TKXCAF
    TKXSBase
    TKVCAF
    TKCAF
    TKLCAF
    TKCDF
    TKV3d
    TKService
    TKMesh
    TKBO
    TKPrim
    TKHLR
    TKShHealing
    TKTopAlgo
    TKGeomAlgo
    TKBRep
    TKGeomBase
    TKG3d
    TKG2d
    TKMath
    TKernel
)

find_package(libnoise REQUIRED)
add_library(noise::noise ALIAS libnoise::libnoise)
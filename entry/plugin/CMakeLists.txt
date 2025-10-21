
set(SRCS module.cpp
         LightSlicerPlugin.hpp
         LightSlicerPlugin.cpp)
set(LIBS libslic3r_gui wxWidgets::wxWidgets boost_libs lm_wx)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

if(CC_BC_WIN)
    list(APPEND LIBS Shlwapi.lib)
endif()
__add_common_library(LightSlicerPlugin)
__append_global_property(LIGHTMAKER_PLUGINS LightSlicerPlugin)

__add_symlink(LightSlicerPlugin ${CMAKE_CURRENT_SOURCE_DIR}/../../resources "")

__redirect_plugin_target_output(LightSlicerPlugin)

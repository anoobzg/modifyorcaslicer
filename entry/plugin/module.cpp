#include "lmwx/interface/framework_interface.h"
#include "slic3r/GUI/AppAdapter.hpp"
#include "LightSlicerPlugin.hpp"

#include <wx/wx.h>

DYNAMIC_EXPORT FrameworkModule* CreateModule(FrameworkContext* context)
{
    LightSlicerPlugin* plugin = new LightSlicerPlugin(context);
    return plugin;
}

DYNAMIC_EXPORT void ReadInfo(FrameworkModuleInfo& info)
{
    info.name = "LightSlicer";
    info.icon = "";
    info.version = "";
    info.description = "";
    info.modify_time = "";
    info.interface_version = "";
}
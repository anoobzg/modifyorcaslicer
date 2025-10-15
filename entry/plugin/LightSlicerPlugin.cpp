#include "LightSlicerPlugin.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/Frame/GUI_Init.hpp"
#include "slic3r/GUI/GUI_App.hpp"

#include <boost/dll/runtime_symbol_info.hpp>

LightSlicerPlugin::LightSlicerPlugin(FrameworkContext* context)
    :m_context(context)
{

}

LightSlicerPlugin::~LightSlicerPlugin() 
{

}

std::string LightSlicerPlugin::name()
{
    return "LightSlicer";
}

wxPanel* LightSlicerPlugin::panel() 
{
    MainPanel* main_panel = AppAdapter::main_panel();
    wxPanel* panel = static_cast<wxPanel*>(main_panel);
    return panel;
}

bool LightSlicerPlugin::init(std::string* error)
{
    AppAdapter* adapter = AppAdapter::inst();

    AppAdapterInitParams params;
    params.resource_path = boost::dll::program_location().parent_path() / "plugins/LightSlicerPlugin/resources";
    adapter->init(m_context, params);
    return adapter->run();
}

bool LightSlicerPlugin::deinit() 
{
    return true;
}

wxApp* LightSlicerPlugin::app()
{
    return m_context->app();
}

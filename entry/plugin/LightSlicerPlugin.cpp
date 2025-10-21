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

wxString LightSlicerPlugin::name() const
{
    return wxString("LightSlicerPlugin");
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
    // Use framework root path + plugins/{name}/resources
    wxString plugin_res = m_context->root_path() + "/plugins/" + name() + "/resources";
    params.resource_path = plugin_res.ToStdString();
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

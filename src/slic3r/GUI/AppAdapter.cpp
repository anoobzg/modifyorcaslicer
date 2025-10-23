#include "AppAdapter.hpp"
#include "AppAdapter.hpp"
#include "Frame/GUI_Init.hpp"
#include "lmwx/interface/framework_interface.h"

namespace Slic3r {
namespace GUI {

AppAdapter* AppAdapter::inst()
{
    static AppAdapter* adapter = NULL;
    if (adapter == NULL)
    {
        adapter = new AppAdapter();
    }
    return adapter;
}


GUI_App* AppAdapter::gui_app()
{
    return inst()->m_gui_app;
}

wxApp* AppAdapter::app()
{
    return inst()->m_context->app();
}


Plater* AppAdapter::plater()
{
    return gui_app()->plater();
}

wxFrame* AppAdapter::mainframe()
{
    return inst()->m_context->mainframe();
}

MainPanel* AppAdapter::main_panel()
{
    return gui_app()->main_panel;
}

AppConfig* AppAdapter::app_config()
{
    return gui_app()->app_config;
}

ObjectList* AppAdapter::obj_list()
{
    return gui_app()->obj_list();
}

const wxFont& AppAdapter::normal_font()
{
    // if(inst()->m_slicer_plugin)
    // {
        return wxFont();
    // }


    // MainFrame* mainframe = static_cast<MainFrame*>(gui_app()->mainframe);
    // return mainframe->normal_font();
}

int AppAdapter::em_unit()
{
    // if(inst()->m_slicer_plugin)
    // {
        return 10;
    // }

    // MainFrame* mainframe = static_cast<MainFrame*>(gui_app()->mainframe);
    // return mainframe->em_unit();
}

AppAdapter::AppAdapter()
{
}

AppAdapter::~AppAdapter()
{

}

void AppAdapter::init(FrameworkContext* context, const AppAdapterInitParams& params)
{
    m_context = context;

    CLI().run(params.resource_path);
    m_gui_app = new GUI_App();

    if (GUI_Init() == -1)
        return;
    
    
}

int AppAdapter::exec()
{
    std::pair<int, char**> args_pair = m_context->run_params();
    if (args_pair.first > 1) {
        int                 argc = 1;
        std::vector<char *> argv;
        argv.push_back(args_pair.second[0]);
        return wxEntry(argc, argv.data());
    } else {
        return wxEntry(args_pair.first, args_pair.second);
    }

}

bool AppAdapter::run()
{
    wxFrame* mainframe = m_context->mainframe();
    if (!m_gui_app->OnInit(mainframe))
        return false;
    
    return true;
}

};
};

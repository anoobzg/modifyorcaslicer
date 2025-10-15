#include "LightSlicerApp.hpp"
#include "slic3r/GUI/Frame/GUI_Init.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"

using namespace Slic3r::GUI;

//IMPLEMENT_APP(LightSlicerApp)

LightSlicerApp::LightSlicerApp()
{

}

LightSlicerApp::~LightSlicerApp()
{

}

bool LightSlicerApp::OnInit() 
{
    AppAdapter::gui_app()->OnInit(m_mainframe);
    m_mainframe->setup_context(AppAdapter::main_panel());
    
    m_mainframe->Show(true);
    m_mainframe->Maximize(true);
    return wxApp::OnInit();
}

int LightSlicerApp::OnExit() 
{
    AppAdapter::gui_app()->OnExit();
    return wxApp::OnExit();
}

bool LightSlicerApp::OnExceptionInMainLoop() 
{
    return AppAdapter::gui_app()->OnExceptionInMainLoop();
}

wxApp* LightSlicerApp::app() 
{
    return this;
}

wxFrame* LightSlicerApp::mainframe() 
{
    return m_mainframe;
}

std::string LightSlicerApp::resource_path() 
{
    return "";
}

std::pair<int, char**> LightSlicerApp::run_params() 
{
    return m_run_params;
}

int LightSlicerApp::exec(int argc, char** argv)
{
    m_run_params.first = argc;
    m_run_params.second = argv;

    AppAdapter* adapter = AppAdapter::inst();
    adapter->init(this);

    m_mainframe = new MainFrame();
    SetTopWindow(m_mainframe);

    return adapter->exec();

}

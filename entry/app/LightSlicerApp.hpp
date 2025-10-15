#ifndef _slic3r_LightSlicerApp_hpp
#define _slic3r_LightSlicerApp_hpp

#include "framework_interface.h"
#include <wx/app.h>

namespace Slic3r {
namespace GUI {
class GUI_App;
class MainFrame;
};
};
using namespace Slic3r::GUI;

class LightSlicerApp : public wxApp, public FrameworkContext
{
public:
    LightSlicerApp();
    ~LightSlicerApp();

    /* wxapp */
    virtual bool OnInit() override;
    virtual int OnExit() override;
    virtual bool OnExceptionInMainLoop() override;

    /* context */
    virtual wxApp* app() override;
    virtual wxFrame* mainframe() override;
    virtual std::string resource_path() override;
    virtual std::pair<int, char**>  run_params() override;
    virtual void execute_cmd(const std::string& cmd, const std::vector<std::string>& args) {}
    virtual void broadcast_cmd(const std::string& cmd, const std::vector<std::string>& args) {}
    virtual void broadcast_json_cmd(const std::string& cmd, const std::string& json_arg) {}

    int exec(int argc, char** argv);

private:
    MainFrame* m_mainframe;
    std::pair<int, char**> m_run_params;

};

#endif
#ifndef _Slic3r_LightSlicerPlugin_hpp
#define _Slic3r_LightSlicerPlugin_hpp

#include "lm_wx/interface/framework_interface.h"

class wxApp;
namespace Slic3r {
namespace GUI {
class GUI_App;
};
};

using namespace Slic3r::GUI;

class LightSlicerPlugin : public FrameworkModule
{
public:
    LightSlicerPlugin(FrameworkContext* context = nullptr);
    virtual ~LightSlicerPlugin();
 
    virtual std::string name() override;
    virtual wxPanel* panel() override;

    virtual void execute_cmd(const std::string& cmd, const std::vector<std::string>& args) {}
    virtual void execute_json_cmd(const std::string& cmd, const std::string& json_arg) {}

    virtual bool init(std::string* error = NULL) override;
    virtual bool deinit() override;

    wxApp* app();
private:
    FrameworkContext* m_context;
};


#endif
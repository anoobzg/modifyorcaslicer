#ifndef _slic3r_AppAdapter_hpp
#define _slic3r_AppAdapter_hpp

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/font.h>
#include "slic3r/GUI/GUI_App.hpp"

class FrameworkContext;
class wxWindow;
namespace Slic3r {
    
class AppConfig;

namespace GUI {

class GUI_App;
class Plater;
class MainPanel;
class ObjectList;

struct AppAdapterInitParams
{
    boost::filesystem::path resource_path = "";
};

class AppAdapter
{
public:
    /* object */
    static GUI_App* gui_app();
    static wxApp* app();
    static AppAdapter* inst();
    static Plater* plater();
    static wxFrame* mainframe();
    static MainPanel* main_panel();
    static AppConfig* app_config();
    static ObjectList* obj_list();

    /* param */
    static const wxFont& normal_font();
    static int em_unit();


private:
    AppAdapter();

public:
    ~AppAdapter();

    void init(FrameworkContext* context, const AppAdapterInitParams& params = AppAdapterInitParams());

    int exec(); // for app
    bool run(); // for plugin

    FrameworkContext*  m_context;
private:
    GUI_App* m_gui_app;
};

};
};

#endif
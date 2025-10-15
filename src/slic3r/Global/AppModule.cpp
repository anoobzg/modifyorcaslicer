#include "AppModule.hpp"

#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Tab.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"

namespace Slic3r {
namespace GUI {
    wxWindow* app_main_window()
    {
        return AppAdapter::main_panel();
    }

    wxWindow* app_top_window()
    {
        return AppAdapter::app()->GetTopWindow();
    }

    wxPanel* app_plater_panel()
    {
        return AppAdapter::plater();
    }

    wxFrame* app_main_frame()
    {
        return AppAdapter::mainframe();
    }

    void queue_plater_event(wxEvent* evt)
    {
        wxQueueEvent(AppAdapter::plater(), evt);
    }

    void plater_call_after(call_after_func func)
    {
        AppAdapter::plater()->CallAfter(func);
    }

    void plater_create_printer_preset()
    {
        AppAdapter::plater()->create_printer_preset();
    }

    void plater_select_printer_preset(const std::string& preset_name)
    {
        AppAdapter::plater()->select_printer_preset(preset_name);
    }

    const Camera& app_plater_camera()
    {
        return AppAdapter::plater()->get_camera();
    }

    bool app_is_project_dirty()
    {
        return AppAdapter::plater()->is_project_dirty();
    }

    void app_save_project()
    {
        AppAdapter::plater()->save_project();
    }

    std::vector<std::string> plater_get_extruder_colors_from_plater_config()
    {
        return AppAdapter::plater()->get_extruder_colors_from_plater_config();
    }

    wxString app_printer_tab_title()
    {
        return AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->title();
    }

    void printer_tab_save_preset(std::string name /*= ""*/, bool detach, bool save_to_project, bool from_input, std::string input_name )
    {
        AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->save_preset(name, detach, save_to_project, from_input, input_name);
    }
}
}
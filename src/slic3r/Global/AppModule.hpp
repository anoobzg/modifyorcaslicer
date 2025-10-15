#pragma once

#include <functional>
namespace Slic3r {
namespace GUI {

    wxWindow* app_main_window();
    wxWindow* app_top_window();
    wxPanel* app_plater_panel();

    void queue_plater_event(wxEvent* evt);

    typedef std::function<void()> call_after_func;
    void plater_call_after(call_after_func func);
    void plater_create_printer_preset();
    void plater_select_printer_preset(const std::string& preset_name);

    wxFrame* app_main_frame();

    class Camera;
    const Camera& app_plater_camera();

    bool app_is_project_dirty();
    void app_save_project();

    std::vector<std::string> plater_get_extruder_colors_from_plater_config();

    wxString app_printer_tab_title();
    void  printer_tab_save_preset(std::string name = std::string(), bool detach = false, bool save_to_project = false, bool from_input = false, std::string input_name = "");
}
}
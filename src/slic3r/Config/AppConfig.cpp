#include "AppConfig.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"

#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/GUI.hpp"

namespace Slic3r{
namespace GUI{

std::string app_get_last_dir()
{
    return AppAdapter::app_config()->get_last_dir();
}

bool app_has(const std::string& key)
{
    return AppAdapter::app_config()->has(key);
}

std::string app_get(const std::string& key)
{
    return AppAdapter::app_config()->get(key);
}

void app_set(const std::string& key, const std::string& value)
{
    AppAdapter::app_config()->set(key, value);
}

void app_set_str(const std::string& section, const std::string& key, const std::string& value)
{
    AppAdapter::app_config()->set_str(section, key, value);
}

bool app_get_bool(const std::string& key)
{
    return AppAdapter::app_config()->get_bool(key);
}

void app_set_bool(const std::string& key, bool value)
{
    AppAdapter::app_config()->set_bool(key, value);
}

std::vector<std::string> app_get_custom_color()
{
    return AppAdapter::app_config()->get_custom_color_from_config();
}

void app_set_custom_color(const std::vector<std::string>& colors)
{
    AppAdapter::app_config()->save_custom_color_to_config(colors);
}

std::string section_get(const std::string& section, const std::string& key)
{
    return AppAdapter::app_config()->get(section, key);
}

bool show_gcode_window()
{
    return AppAdapter::app_config()->get_bool("show_gcode_window");
}

void toggle_show_gcode_window()
{
    bool show_gcode_window = AppAdapter::app_config()->get_bool("show_gcode_window");
    AppAdapter::app_config()->set_bool("show_gcode_window", !show_gcode_window);
}

bool is_enable_multi_machine()
{
    return AppAdapter::app_config()->get_bool("enable_multi_machine"); 
}

bool show_3d_navigator()
{
    return AppAdapter::app_config()->get_bool("show_3d_navigator"); 
}

void toggle_show_3d_navigator()
{ 
    AppAdapter::app_config()->set_bool("show_3d_navigator", !show_3d_navigator()); 
}

bool show_outline()
{
    return AppAdapter::app_config()->get_bool("show_outline"); 
}

void toggle_show_outline()
{ 
    AppAdapter::app_config()->set_bool("show_outline", !show_outline()); 
}

void app_config_save()
{
    AppAdapter::app_config()->save();
}

std::string bambu_privacy_url()
{
    std::string url;
    std::string country_code = Slic3r::GUI::AppAdapter::app_config()->get_country_code();

    if (country_code == "CN") {
        url = "https://www.bambulab.cn/policies/privacy";
    }
    else{
        url = "https://www.bambulab.com/policies/privacy";
    }
    return url; 
}

std::string get_http_url(std::string country_code, std::string path)
{
    std::string url;
    if (country_code == "US") {
        url = "https://api.bambulab.com/";
    }
    else if (country_code == "CN") {
        url = "https://api.bambulab.cn/";
    }
    else if (country_code == "ENV_CN_DEV") {
        url = "https://api-dev.bambu-lab.com/";
    }
    else if (country_code == "ENV_CN_QA") {
        url = "https://api-qa.bambu-lab.com/";
    }
    else if (country_code == "ENV_CN_PRE") {
        url = "https://api-pre.bambu-lab.com/";
    }
    else {
        url = "https://api.bambulab.com/";
    }

    url += path.empty() ? "v1/iot-service/api/slicer/resource" : path;
    return url;
}

std::string get_model_http_url(std::string country_code)
{
    std::string url;
    if (country_code == "US") {
        url = "https://makerworld.com/";
    }
    else if (country_code == "CN") {
        url = "https://makerworld.com/";
    }
    else if (country_code == "ENV_CN_DEV") {
        url = "https://makerhub-dev.bambu-lab.com/";
    }
    else if (country_code == "ENV_CN_QA") {
        url = "https://makerhub-qa.bambu-lab.com/";
    }
    else if (country_code == "ENV_CN_PRE") {
        url = "https://makerhub-pre.bambu-lab.com/";
    }
    else {
        url = "https://makerworld.com/";
    }

    return url;
}


std::string get_plugin_url(std::string name, std::string country_code)
{
    std::string url = get_http_url(country_code);

    std::string curr_version = SLIC3R_VERSION;
    std::string using_version = curr_version.substr(0, 9) + "00";
    if (name == "cameratools")
        using_version = curr_version.substr(0, 6) + "00.00";
    url += (boost::format("?slicer/%1%/cloud=%2%") % name % using_version).str();
    //url += (boost::format("?slicer/plugins/cloud=%1%") % "01.01.00.00").str();
    return url;
}

std::string get_local_models_path()
{
    std::string local_path = "";
    if (user_data_dir().empty()) {
        return local_path;
    }

    auto models_folder = (boost::filesystem::path(user_data_dir()) / "models");
    local_path = models_folder.string();

    if (!fs::exists(models_folder)) {
        if (!fs::create_directory(models_folder)) {
            local_path = "";
        }
        BOOST_LOG_TRIVIAL(info) << "Create models folder:" << models_folder.string();
    }
    return local_path;
}

std::string version_display = "";
std::string format_display_version()
{
    if (!version_display.empty()) return version_display;

    version_display = LightMaker_VERSION;
    return version_display;
}

std::string get_language_code()
{
    return AppAdapter::app_config()->get_language_code();
}

std::string get_country_code()
{
    return AppAdapter::app_config()->get_country_code();
}

void app_set_load_behaviour_load_all()
{
    AppAdapter::app_config()->set(SETTING_PROJECT_LOAD_BEHAVIOUR, OPTION_PROJECT_LOAD_BEHAVIOUR_LOAD_ALL);
}

void app_set_load_behaviour_load_geometry()
{
    AppAdapter::app_config()->set(SETTING_PROJECT_LOAD_BEHAVIOUR, OPTION_PROJECT_LOAD_BEHAVIOUR_LOAD_GEOMETRY);
}

void persist_window_geometry(wxTopLevelWindow *window, bool default_maximized)
{
    const std::string name = into_u8(window->GetName());

    window->Bind(wxEVT_CLOSE_WINDOW, [=](wxCloseEvent &event) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ": received wxEVT_CLOSE_WINDOW, trigger save for window_mainframe";
        window_pos_save(window, "mainframe");
        event.Skip();
    });

    if (window_pos_restore(window, "mainframe", default_maximized)) {
        on_window_geometry(window, [=]() {
            window_pos_sanitize(window);
        });
    } else {
        on_window_geometry(window, [=]() {
            window_pos_center(window);
        });
    }
}

void window_pos_save(wxTopLevelWindow* window, const std::string &name)
{
    if (name.empty()) { return; }
    const auto config_key = (boost::format("window_%1%") % name).str();

    WindowMetrics metrics = WindowMetrics::from_window(window);
    app_set(config_key, metrics.serialize());
    app_config_save();
}

bool window_pos_restore(wxTopLevelWindow* window, const std::string &name, bool default_maximized)
{
    if (name.empty()) { return false; }
    const auto config_key = (boost::format("window_%1%") % name).str();

    if (! app_has(config_key)) {
        //window->Maximize(default_maximized);
        return false;
    }

    auto metrics = WindowMetrics::deserialize(app_get(config_key));
    if (! metrics) {
        window->Maximize(default_maximized);
        return true;
    }

    const wxRect& rect = metrics->get_rect();
    window->SetPosition(rect.GetPosition());
    window->SetSize(rect.GetSize());
    window->Maximize(metrics->get_maximized());
    return true;
}

void window_pos_sanitize(wxTopLevelWindow* window)
{
    /*unsigned*/int display_idx = wxDisplay::GetFromWindow(window);
    wxRect display;
    if (display_idx == wxNOT_FOUND) {
        display = wxDisplay(0u).GetClientArea();
        window->Move(display.GetTopLeft());
    } else {
        display = wxDisplay(display_idx).GetClientArea();
    }

    auto metrics = WindowMetrics::from_window(window);
    metrics.sanitize_for_display(display);
    if (window->GetScreenRect() != metrics.get_rect()) {
        window->SetSize(metrics.get_rect());
    }
}

void window_pos_center(wxTopLevelWindow *window)
{
    /*unsigned*/int display_idx = wxDisplay::GetFromWindow(window);
    wxRect display;
    if (display_idx == wxNOT_FOUND) {
        display = wxDisplay(0u).GetClientArea();
        window->Move(display.GetTopLeft());
    } else {
        display = wxDisplay(display_idx).GetClientArea();
    }

    auto metrics = WindowMetrics::from_window(window);
    metrics.center_for_display(display);
    if (window->GetScreenRect() != metrics.get_rect()) {
        window->SetSize(metrics.get_rect());
    }
}

void link_to_network_check()
{
    std::string url;
    std::string country_code = AppAdapter::app_config()->get_country_code();


    if (country_code == "US") {
        url = "https://status.bambulab.com";
    }
    else if (country_code == "CN") {
        url = "https://status.bambulab.cn";
    }
    else {
        url = "https://status.bambulab.com";
    }
    wxLaunchDefaultBrowser(url);
}

void link_to_lan_only_wiki()
{
    std::string url;
    std::string country_code = AppAdapter::app_config()->get_country_code();

    if (country_code == "US") {
        url = "https://wiki.bambulab.com/en/knowledge-sharing/enable-lan-mode";
    }
    else if (country_code == "CN") {
        url = "https://wiki.bambulab.com/zh/knowledge-sharing/enable-lan-mode";
    }
    else {
        url = "https://wiki.bambulab.com/en/knowledge-sharing/enable-lan-mode";
    }
    wxLaunchDefaultBrowser(url);
}

bool has_model_mall()
{
    if (auto cc = AppAdapter::app_config()->get_region(); cc == "CNH" || cc == "China" || cc == "")
        return false;
    return true;
}

std::string app_logo_name()
{
    return "LightMaker"; 
}

float toolbar_icon_scale(const bool is_limited/* = false*/)
{
#ifdef __APPLE__
    const float icon_sc = 1.0f; // for Retina display will be used its own scale
#else
    const float icon_sc = app_em_unit() * 0.1f;
#endif // __APPLE__

    //return icon_sc;

    const std::string& auto_val = app_get("toolkit_size");

    if (auto_val.empty())
        return icon_sc;

    int int_val =  100;
    // correct value in respect to toolkit_size
    int_val = std::min(atoi(auto_val.c_str()), int_val);

    if (is_limited && int_val < 50)
        int_val = 50;

    return 0.01f * int_val * icon_sc;
}

void set_auto_toolbar_icon_scale(float scale)
{
#ifdef __APPLE__
    const float icon_sc = 1.0f; // for Retina display will be used its own scale
#else
    const float icon_sc = app_em_unit() * 0.1f;
#endif // __APPLE__

    long int_val = std::min(int(std::lround(scale / icon_sc * 100)), 100);
    std::string val = std::to_string(int_val);

    app_set("toolkit_size", val);
}

void init_download_path()
{
    std::string down_path = app_get("download_path");

    if (down_path.empty()) {
        std::string user_down_path = wxStandardPaths::Get().GetUserDir(wxStandardPaths::Dir_Downloads).ToUTF8().data();
        app_set("download_path", user_down_path);
    }
    else {
        fs::path dp(down_path);
        if (!fs::exists(dp)) {

            std::string user_down_path = wxStandardPaths::Get().GetUserDir(wxStandardPaths::Dir_Downloads).ToUTF8().data();
            app_set("download_path", user_down_path);
        }
    }
}

int app_get_mode()
{
    if (!app_has("user_mode"))
        return (int)comSimple;
    //BBS
    const auto mode = app_get("user_mode");
    ConfigOptionMode r =  mode == "advanced" ? comAdvanced :
           mode == "simple" ? comSimple :
           mode == "develop" ? comDevelop : comSimple;

    return (int)r;
}

std::string app_get_mode_str()
{
    if (!app_has("user_mode"))
        return "simple";
    return app_get("user_mode");
}

void app_save_mode(int mode)
{
    //BBS
    const std::string mode_str = mode == comAdvanced ? "advanced" :
                                 mode == comSimple ? "simple" :
                                 mode == comDevelop ? "develop" : "simple";
    app_set("user_mode", mode_str);
}

}
}
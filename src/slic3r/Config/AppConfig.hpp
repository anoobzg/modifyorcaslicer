#pragma once
#include <string>
#include <vector>

// single wrapper file for GUI_APP::AppConfig
namespace Slic3r {
namespace GUI {
std::string app_get_last_dir();

bool app_has(const std::string& key);
std::string app_get(const std::string& key);
void app_set(const std::string& key, const std::string& value);
void app_set_str(const std::string& section, const std::string& key, const std::string& value);

bool app_get_bool(const std::string& key);
void app_set_bool(const std::string& key, bool value);

std::vector<std::string> app_get_custom_color();
void app_set_custom_color(const std::vector<std::string>& colors);

std::string section_get(const std::string& section, const std::string& key);

void app_config_save();
// SoftFever
bool show_gcode_window();
void toggle_show_gcode_window();

bool is_enable_multi_machine();

bool show_3d_navigator();
void toggle_show_3d_navigator();

bool show_outline();
void toggle_show_outline();

std::string     bambu_privacy_url();
std::string     get_plugin_url(std::string name, std::string country_code);
std::string     get_http_url(std::string country_code, std::string path = {});
std::string     get_model_http_url(std::string country_code);
std::string     get_local_models_path();

std::string format_display_version();
std::string get_language_code();
std::string get_country_code();

void app_set_load_behaviour_load_all();
void app_set_load_behaviour_load_geometry();

void            persist_window_geometry(wxTopLevelWindow *window, bool default_maximized = false);
void            window_pos_save(wxTopLevelWindow* window, const std::string &name);
bool            window_pos_restore(wxTopLevelWindow* window, const std::string &name, bool default_maximized = false);
void            window_pos_sanitize(wxTopLevelWindow* window);
void            window_pos_center(wxTopLevelWindow *window);

void            link_to_network_check();
void            link_to_lan_only_wiki();
bool            has_model_mall();

std::string     app_logo_name();

float           toolbar_icon_scale(const bool is_limited = false);
void            set_auto_toolbar_icon_scale(float scale);

void            init_download_path();

int app_get_mode();
std::string app_get_mode_str();
void app_save_mode(int mode) ;
}
}
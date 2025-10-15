#pragma once

namespace Slic3r{
namespace GUI{

void generic_exception_handle();
std::string libslic3r_translate_callback(const char *s);

#ifdef __linux__
bool check_old_linux_datadir(const wxString& app_name); 
#endif
#if _WIN32
bool is_associate_files(std::wstring extend);
#endif

void register_win32_event();

bool has_illegal_filename_characters(const wxString& name);
bool has_illegal_filename_characters(const std::string& name);

std::string w2s(wxString sSrc);
void        GetStardardFilePath(std::string &FilePath);

void run_printer_model_wizard();

// Calls wxLaunchDefaultBrowser if user confirms in dialog.
bool            open_browser_with_warning_dialog(const wxString& url, int flags = 0);

// Ask the destop to open the datadir using the default file explorer.
void desktop_open_datadir_folder();
// Ask the destop to open one folder
void desktop_open_any_folder(const std::string& path);

#define SHOW_BACKGROUND_BITMAP_PIXEL_THRESHOLD 80
int get_brightness_value(wxImage image);

wxString choose_project_name(wxWindow *parent);
wxArrayString choose_model_name(wxWindow *parent);
wxString choose_zip_name(wxWindow* parent);
wxString choose_gcode_name(wxWindow* parent);

wxString transition_tridid(int trid_id);


const char* separator_head();
const char* separator_tail();
wxString    separator(const std::string& label);

}
}
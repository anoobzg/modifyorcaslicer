#pragma once
#include "libslic3r/Config/ThemeDef.hpp"

namespace Slic3r {
namespace GUI {

void            init_app_color();

// update color mode for whole dialog including all children
void            UpdateDlgDarkUI(wxDialog* dlg);
void            UpdateFrameDarkUI(wxFrame* dlg);
// update color mode for window
void            UpdateDarkUI(wxWindow* window, bool highlited = false, bool just_font = false);
void            UpdateDarkUIWin(wxWindow* win);
void            Update_dark_mode_flag();
// update color mode for DataViewControl
void            UpdateDVCDarkUI(wxDataViewCtrl* dvc, bool highlited = false);
// update color mode for panel including all static texts controls
void            UpdateAllStaticTextDarkUI(wxWindow* parent);

bool            dark_mode();

unsigned        get_colour_approx_luma(const wxColour& colour);

const wxColour  get_label_default_clr_system();
const wxColour  get_label_default_clr_modified();
void            init_label_colours();
void            update_label_colours_from_appconfig();

const wxColour& get_label_clr_modified();
const wxColour& get_label_clr_sys();
const wxColour& get_label_clr_default();
const wxColour& get_window_default_clr();

// BBS
const wxColour& get_label_highlight_clr();
const wxColour& get_highlight_default_clr();
const wxColour& get_color_hovered_btn_label();
const wxColour& get_color_selected_btn_bg();
void            force_colors_update(wxWindow* win);
#ifdef _MSW_DARK_MODE
void            force_menu_update();
#endif //_MSW_DARK_MODE

void            force_update_ui_colors();
}
}
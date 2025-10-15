#include "AppColor.hpp"

#include "libslic3r/FileSystem/Log.hpp"
#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Theme/AppFont.hpp"

#ifdef __WXMSW__
#include <dbt.h>
#include <shlobj.h>

#ifdef __WINDOWS__
#ifdef _MSW_DARK_MODE
#include "slic3r/Theme/WinDarkMode.hpp"
#include "wx/headerctrl.h"
#include "wx/msw/headerctrl.h"
#endif // _MSW_DARK_MODE
#endif // __WINDOWS__
#endif

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"

namespace Slic3r {
namespace GUI {
    bool  m_is_dark_mode = false;

    wxColour        m_color_label_modified;
    wxColour        m_color_label_sys;
    wxColour        m_color_label_default;
    wxColour        m_color_window_default;
    wxColour        m_color_highlight_label_default;
    wxColour        m_color_hovered_btn_label;
    wxColour        m_color_default_btn_label;
    wxColour        m_color_highlight_default;
    wxColour        m_color_selected_btn_bg;
    bool            m_force_colors_update = false;

    const wxColour& get_label_clr_modified() 
    { 
        return m_color_label_modified; 
    }

    const wxColour& get_label_clr_sys() 
    { 
        return m_color_label_sys; 
    }
    const wxColour& get_label_clr_default() 
    { 
        return m_color_label_default; 
    }
    const wxColour& get_window_default_clr() 
    { 
        return m_color_window_default; 
    }

    // BBS
    const wxColour& get_label_highlight_clr() 
    { 
        return m_color_highlight_label_default; 
    }
    const wxColour& get_highlight_default_clr() 
    { 
        return m_color_highlight_default; 
    }
    const wxColour& get_color_hovered_btn_label() 
    { 
        return m_color_hovered_btn_label; 
    }
    const wxColour& get_color_selected_btn_bg() 
    { 
        return m_color_selected_btn_bg; 
    }

    void init_app_color()
    {

#ifdef _MSW_DARK_MODE

#ifndef __WINDOWS__
        wxSystemAppearance app = wxSystemSettings::GetAppearance();
        GUI::AppAdapter::app_config()->set("dark_color_mode", app.IsDark() ? "1" : "0");
        GUI::AppAdapter::app_config()->save();
#endif // __APPLE__


        bool init_dark_color_mode = dark_mode();
        bool init_sys_menu_enabled = app_get_bool("sys_menu_enabled");
#ifdef __WINDOWS__
        NppDarkMode::InitDarkMode(init_dark_color_mode, init_sys_menu_enabled);
#endif // __WINDOWS__

#endif
        // initialize label colors and fonts
        init_label_colours();

        Update_dark_mode_flag();
        flush_logs();

#ifdef _MSW_DARK_MODE
        if (bool new_dark_color_mode = dark_mode();
            init_dark_color_mode != new_dark_color_mode) {

#ifdef __WINDOWS__
            NppDarkMode::SetDarkMode(new_dark_color_mode);
#endif // __WINDOWS__

            init_label_colours();
            //update_label_colours_from_appconfig();
        }
        if (bool new_sys_menu_enabled = app_get_bool("sys_menu_enabled");
            init_sys_menu_enabled != new_sys_menu_enabled)
#ifdef __WINDOWS__
            NppDarkMode::SetSystemMenuForApp(new_sys_menu_enabled);
#endif
#endif
    }

    // recursive function for scaling fonts for all controls in Window
    void update_dark_children_ui(wxWindow* window, bool just_buttons_update = false)
    {
        /*bool is_btn = dynamic_cast<wxButton*>(window) != nullptr;
        is_btn = false;*/
        if (!window) return;

        UpdateDarkUI(window);

        auto children = window->GetChildren();
        for (auto child : children) {
            update_dark_children_ui(child);
        }
    }

#ifdef _WIN32
    bool is_focused(HWND hWnd)
    {
        HWND hFocusedWnd = ::GetFocus();
        return hFocusedWnd && hWnd == hFocusedWnd;
    }

    bool is_default(wxWindow* win)
    {
        wxTopLevelWindow* tlw = find_toplevel_parent(win);
        if (!tlw)
            return false;

        return win == tlw->GetDefaultItem();
    }
#endif

    void UpdateDlgDarkUI(wxDialog* dlg)
    {
#ifdef __WINDOWS__
        NppDarkMode::SetDarkExplorerTheme(dlg->GetHWND());
        NppDarkMode::SetDarkTitleBar(dlg->GetHWND());
#endif
        update_dark_children_ui(dlg);
    }

    void UpdateFrameDarkUI(wxFrame* dlg)
    {
#ifdef __WINDOWS__
        NppDarkMode::SetDarkExplorerTheme(dlg->GetHWND());
        NppDarkMode::SetDarkTitleBar(dlg->GetHWND());
#endif
        update_dark_children_ui(dlg);
    }

    void UpdateDarkUI(wxWindow* window, bool highlited/* = false*/, bool just_font/* = false*/)
    {
        if (wxButton* btn = dynamic_cast<wxButton*>(window)) {
            if (btn->GetWindowStyleFlag() & wxBU_AUTODRAW)
                return;
            else {
#ifdef _WIN32
                if (btn->GetId() == wxID_OK || btn->GetId() == wxID_CANCEL) {
                    bool is_focused_button = false;
                    bool is_default_button = false;

                    if (!(btn->GetWindowStyle() & wxNO_BORDER)) {
                        btn->SetWindowStyle(btn->GetWindowStyle() | wxNO_BORDER);
                        highlited = true;
                    }

                    auto mark_button = [btn, highlited](const bool mark) {
                        btn->SetBackgroundColour(mark ? m_color_selected_btn_bg : highlited ? m_color_highlight_default : m_color_window_default);
                        btn->SetForegroundColour(mark ? m_color_hovered_btn_label : m_color_default_btn_label);
                        btn->Refresh();
                        btn->Update();
                        };

                    // hovering
                    btn->Bind(wxEVT_ENTER_WINDOW, [mark_button](wxMouseEvent& event) { mark_button(true); event.Skip(); });
                    btn->Bind(wxEVT_LEAVE_WINDOW, [mark_button, btn](wxMouseEvent& event) { mark_button(is_focused(btn->GetHWND())); event.Skip(); });
                    // focusing
                    btn->Bind(wxEVT_SET_FOCUS, [mark_button](wxFocusEvent& event) { mark_button(true); event.Skip(); });
                    btn->Bind(wxEVT_KILL_FOCUS, [mark_button](wxFocusEvent& event) { mark_button(false); event.Skip(); });

                    is_focused_button = is_focused(btn->GetHWND());
                    is_default_button = is_default(btn);
                    mark_button(is_focused_button);
                }
#endif
            }
        }

        if (Button* btn = dynamic_cast<Button*>(window)) {
            if (btn->GetWindowStyleFlag() & wxBU_AUTODRAW)
                return;
        }


        /*if (m_is_dark_mode != dark_mode() )
            m_is_dark_mode = dark_mode();*/

        if (m_is_dark_mode) {

            auto orig_col = window->GetBackgroundColour();
            auto bg_col = StateColor::darkModeColorFor(orig_col);
            // there are cases where the background color of an item is bright, specifically:
            // * the background color of a button: #009688  -- 73
            if (bg_col != orig_col) {
                window->SetBackgroundColour(bg_col);
            }

            orig_col = window->GetForegroundColour();
            auto fg_col = StateColor::darkModeColorFor(orig_col);
            auto fg_l = StateColor::GetLightness(fg_col);

            auto color_difference = StateColor::GetColorDifference(bg_col, fg_col);

            // fallback and sanity check with LAB
            // color difference of less than 2 or 3 is not normally visible, and even less than 30-40 doesn't stand out
            if (color_difference < 10) {
                fg_col = StateColor::SetLightness(fg_col, 90);
            }
            // some of the stock colors have a lightness of ~49
            if (fg_l < 45) {
                fg_col = StateColor::SetLightness(fg_col, 70);
            }
            // at this point it shouldn't be possible that fg_col is the same as bg_col, but let's be safe
            if (fg_col == bg_col) {
                fg_col = StateColor::SetLightness(fg_col, 70);
            }

            window->SetForegroundColour(fg_col);
        }
        else {
            auto original_col = window->GetBackgroundColour();
            auto bg_col = StateColor::lightModeColorFor(original_col);

            if (bg_col != original_col) {
                window->SetBackgroundColour(bg_col);
            }

            original_col = window->GetForegroundColour();
            auto fg_col = StateColor::lightModeColorFor(original_col);

            if (fg_col != original_col) {
                window->SetForegroundColour(fg_col);
            }
        }
    }

    // Note: Don't use this function for Dialog contains ScalableButtons
    void UpdateDarkUIWin(wxWindow* win)
    {
        update_dark_children_ui(win);
    }

    void UpdateDVCDarkUI(wxDataViewCtrl* dvc, bool highlited/* = false*/)
    {
#ifdef __WINDOWS__
        UpdateDarkUI(dvc, highlited ? dark_mode() : false);
#ifdef _MSW_DARK_MODE
        HWND hwnd;
        if (!dvc->HasFlag(wxDV_NO_HEADER)) {
            hwnd = (HWND)dvc->GenericGetHeader()->GetHandle();
            hwnd = GetWindow(hwnd, GW_CHILD);
            if (hwnd != NULL)
                NppDarkMode::SetDarkListViewHeader(hwnd);
            wxItemAttr attr;
            attr.SetTextColour(NppDarkMode::GetTextColor());
            attr.SetFont(app_normal_font());
            dvc->SetHeaderAttr(attr);
        }
#endif //_MSW_DARK_MODE
        if (dvc->HasFlag(wxDV_ROW_LINES))
            dvc->SetAlternateRowColour(m_color_highlight_default);
        if (dvc->GetBorder() != wxBORDER_SIMPLE)
            dvc->SetWindowStyle(dvc->GetWindowStyle() | wxBORDER_SIMPLE);
#endif
    }

    void UpdateAllStaticTextDarkUI(wxWindow* parent)
    {
#ifdef __WINDOWS__
        UpdateDarkUI(parent);

        auto children = parent->GetChildren();
        for (auto child : children) {
            if (dynamic_cast<wxStaticText*>(child))
                child->SetForegroundColour(m_color_label_default);
        }
#endif
    }

    void Update_dark_mode_flag()
    {
        m_is_dark_mode = dark_mode();
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": switch the current dark mode status to %1% ") % m_is_dark_mode;
    }

    bool dark_mode()
    {
#ifdef SUPPORT_DARK_MODE
#if __APPLE__
        // The check for dark mode returns false positive on 10.12 and 10.13,
        // which allowed setting dark menu bar and dock area, which is
        // is detected as dark mode. We must run on at least 10.14 where the
        // proper dark mode was first introduced.
        return wxPlatformInfo::Get().CheckOSVersion(10, 14) && mac_dark_mode();
#else
        return app_get_bool("dark_color_mode") ? true : check_dark_mode();
        //const unsigned luma = get_colour_approx_luma(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        //return luma < 128;
#endif
#else
        //BBS disable DarkUI mode
        return false;
#endif
    }

    unsigned get_colour_approx_luma(const wxColour& colour)
    {
        double r = colour.Red();
        double g = colour.Green();
        double b = colour.Blue();

        return std::round(std::sqrt(
            r * r * .241 +
            g * g * .691 +
            b * b * .068
        ));
    }

    const wxColour get_label_default_clr_system()
    {
        return dark_mode() ? wxColour(115, 220, 103) : wxColour(26, 132, 57);
    }

    const wxColour get_label_default_clr_modified()
    {
        return dark_mode() ? wxColour(253, 111, 40) : wxColour(252, 77, 1);
    }

    void init_label_colours()
    {
        bool is_dark_mode = dark_mode();
        m_color_label_modified = is_dark_mode ? wxColour("#F1754E") : wxColour("#F1754E");
        m_color_label_sys = is_dark_mode ? wxColour("#B2B3B5") : wxColour("#363636");

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
        m_color_label_default = is_dark_mode ? wxColour(250, 250, 250) : m_color_label_sys; // wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        m_color_highlight_label_default = is_dark_mode ? wxColour(230, 230, 230) : wxSystemSettings::GetColour(/*wxSYS_COLOUR_HIGHLIGHTTEXT*/wxSYS_COLOUR_WINDOWTEXT);
        m_color_highlight_default = is_dark_mode ? wxColour("#36363B") : wxColour("#F1F1F1"); // ORCA row highlighting
        m_color_hovered_btn_label = is_dark_mode ? wxColour(255, 255, 254) : wxColour(0, 0, 0);
        m_color_default_btn_label = is_dark_mode ? wxColour(255, 255, 254) : wxColour(0, 0, 0);
        m_color_selected_btn_bg = is_dark_mode ? wxColour(84, 84, 91) : wxColour(206, 206, 206);
#else
        m_color_label_default = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
#endif
        m_color_window_default = is_dark_mode ? wxColour(43, 43, 43) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        StateColor::SetDarkMode(is_dark_mode);
    }

    void update_label_colours_from_appconfig()
    {
        if (app_has("label_clr_sys")) {
            auto str = app_get("label_clr_sys");
            if (str != "")
                m_color_label_sys = wxColour(str);
        }

        if (app_has("label_clr_modified")) {
            auto str = app_get("label_clr_modified");
            if (str != "")
                m_color_label_modified = wxColour(str);
        }
    }

#ifdef __WINDOWS__
#ifdef _MSW_DARK_MODE
    static void update_scrolls(wxWindow* window)
    {
        wxWindowList::compatibility_iterator node = window->GetChildren().GetFirst();
        while (node)
        {
            wxWindow* win = node->GetData();
            if (dynamic_cast<wxScrollHelper*>(win) ||
                dynamic_cast<wxTreeCtrl*>(win) ||
                dynamic_cast<wxTextCtrl*>(win))
                NppDarkMode::SetDarkExplorerTheme(win->GetHWND());

            update_scrolls(win);
            node = node->GetNext();
        }
    }
#endif //_MSW_DARK_MODE
#ifdef _MSW_DARK_MODE
    void force_menu_update()
    {
        NppDarkMode::SetSystemMenuForApp(app_get_bool("sys_menu_enabled"));
    }
#endif //_MSW_DARK_MODE
#endif //__WINDOWS__

    void force_colors_update(wxWindow* win)
    {
#ifdef _MSW_DARK_MODE
#ifdef __WINDOWS__
        NppDarkMode::SetDarkMode(dark_mode());

        // anoob
        // if (WXHWND wxHWND = wxToolTip::GetToolTipCtrl())
        //     NppDarkMode::SetDarkExplorerTheme((HWND)wxHWND);

        NppDarkMode::SetDarkTitleBar(win->GetHWND());


        //NppDarkMode::SetDarkExplorerTheme((HWND)mainframe->m_settings_dialog.GetHWND());
        //NppDarkMode::SetDarkTitleBar(mainframe->m_settings_dialog.GetHWND());

#endif // __WINDOWS__
#endif //_MSW_DARK_MODE
        m_force_colors_update = true;
    }

    void force_update_ui_colors()
    {
        // Upadte UI colors before Update UI from settings
        if (m_force_colors_update) {
            m_force_colors_update = false;
            //UpdateDlgDarkUI(&mainframe->m_settings_dialog);
            //mainframe->m_settings_dialog.Refresh();
            //mainframe->m_settings_dialog.Update();

        //     if (mainframe) {
        // #ifdef __WINDOWS__
        //         mainframe->force_color_changed();
        //         update_scrolls(mainframe);
        //         update_scrolls(&mainframe->m_settings_dialog);
        // #endif //_MSW_DARK_MODE
        //         update_dark_children_ui(mainframe);
        //     }
        }        
    }
}
}
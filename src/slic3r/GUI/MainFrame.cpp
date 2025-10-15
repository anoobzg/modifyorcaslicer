#include "MainFrame.hpp"

#ifdef _WIN32
#include <dbt.h>
#include <shlobj.h>
#include <shellapi.h>
#endif // _WIN32

#include "libslic3r/AppConfig.hpp"
#include "slic3r/Utils/Str.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/Render/Mouse3DController.hpp"
#include "slic3r/Global/InstanceCheck.hpp"
// BBS
#include "slic3r/GUI/Frame/BBLTopbar.hpp"
#include "MainPanel.hpp"
#include "I18N.hpp"
#include "slic3r/GUI/wxExtensions.hpp"
#include "slic3r/GUI/Calibration/calib_dlg.hpp"

namespace Slic3r {
namespace GUI {

// BBS
#ifndef __APPLE__
#define BORDERLESS_FRAME_STYLE (wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)
#else
#define BORDERLESS_FRAME_STYLE (wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)
#endif

#ifdef __APPLE__
static const wxString ctrl = ("Ctrl+");
#else
static const wxString ctrl = _L("Ctrl+");
#endif

#ifdef _MSC_VER
    // \xA0 is a non-breaking space. It is entered here to spoil the automatic accelerators,
    // as the simple numeric accelerators spoil all numeric data entry.
static const wxString sep = "\t\xA0";
static const wxString sep_space = "\xA0";
#else
static const wxString sep = " - ";
static const wxString sep_space = "";
#endif

MainFrame::MainFrame() 
    : DPIFrame(NULL, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, BORDERLESS_FRAME_STYLE, "mainframe")
    , m_panel(nullptr)
{
#ifdef WIN32
    // SetMaximize causes the window to overlap the taskbar, due to the fact this window has wxMAXIMIZE_BOX off
    // https://forums.wxwidgets.org/viewtopic.php?t=50634
    // Fix it here
    this->Bind(wxEVT_MAXIMIZE, [this](auto &e) {
        wxDisplay display(this);
        auto      size = display.GetClientArea().GetSize();
        auto      pos  = display.GetClientArea().GetPosition();
        HWND      hWnd = GetHandle();
        RECT      borderThickness;
        SetRectEmpty(&borderThickness);
        AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE), FALSE, 0);
        const auto max_size = size + wxSize{-borderThickness.left + borderThickness.right, -borderThickness.top + borderThickness.bottom};
        const auto current_size = GetSize();
        SetSize({std::min(max_size.x, current_size.x), std::min(max_size.y, current_size.y)});
        Move(pos + wxPoint{borderThickness.left, borderThickness.top});
        e.Skip();
    });
#endif // WIN32    
}

#ifdef __WIN32__

// Orca: Fix maximized window overlaps taskbar when taskbar auto hide is enabled (#8085)
// Adopted from https://gist.github.com/MortenChristiansen/6463580
static void AdjustWorkingAreaForAutoHide(const HWND hWnd, MINMAXINFO* mmi)
{
    const auto taskbarHwnd = FindWindowA("Shell_TrayWnd", nullptr);
    if (!taskbarHwnd) {
        return;
    }
    const auto monitorContainingApplication = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONULL);
    const auto monitorWithTaskbarOnIt = MonitorFromWindow(taskbarHwnd, MONITOR_DEFAULTTONULL);
    if (monitorContainingApplication != monitorWithTaskbarOnIt) {
        return;
    }
    APPBARDATA abd;
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd   = taskbarHwnd;

    // Find if task bar has auto-hide enabled
    const auto uState = (UINT) SHAppBarMessage(ABM_GETSTATE, &abd);
    if ((uState & ABS_AUTOHIDE) != ABS_AUTOHIDE) {
        return;
    }

    RECT borderThickness;
    SetRectEmpty(&borderThickness);
    AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, 0);

    // Determine taskbar position
    SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    const auto& rc = abd.rc;
    if (rc.top == rc.left && rc.bottom > rc.right) {
        // Left
        const auto offset = borderThickness.left + 2;
        mmi->ptMaxPosition.x += offset;
        mmi->ptMaxTrackSize.x -= offset;
        mmi->ptMaxSize.x -= offset;
    } else if (rc.top == rc.left && rc.bottom < rc.right) {
        // Top
        const auto offset = borderThickness.top + 2;
        mmi->ptMaxPosition.y += offset;
        mmi->ptMaxTrackSize.y -= offset;
        mmi->ptMaxSize.y -= offset;
    } else if (rc.top > rc.left) {
        // Bottom
        const auto offset = borderThickness.bottom + 2;
        mmi->ptMaxSize.y -= offset;
        mmi->ptMaxTrackSize.y -= offset;
    } else {
        // Right
        const auto offset = borderThickness.right + 2;
        mmi->ptMaxSize.x -= offset;
        mmi->ptMaxTrackSize.x -= offset;
    }
}

WXLRESULT MainFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    HWND hWnd = GetHandle();
    /* When we have a custom titlebar in the window, we don't need the non-client area of a normal window
     * to be painted. In order to achieve this, we handle the "WM_NCCALCSIZE" which is responsible for the
     * size of non-client area of a window and set the return value to 0. Also we have to tell the
     * application to not paint this area on activate and deactivation events so we also handle
     * "WM_NCACTIVATE" message. */
    switch (nMsg) {
    case WM_NCACTIVATE: {
        /* Returning 0 from this message disable the window from receiving activate events which is not
        desirable. However When a visual style is not active (?) for this window, "lParam" is a handle to an
        optional update region for the nonclient area of the window. If this parameter is set to -1,
        DefWindowProc does not repaint the nonclient area to reflect the state change. */
        lParam = -1;
        break;
    }
    /* To remove the standard window frame, you must handle the WM_NCCALCSIZE message, specifically when
    its wParam value is TRUE and the return value is 0 */
    case WM_NCCALCSIZE:
        if (wParam) {
            /* Detect whether window is maximized or not. We don't need to change the resize border when win is
             *  maximized because all resize borders are gone automatically */
            WINDOWPLACEMENT wPos;
            // GetWindowPlacement fail if this member is not set correctly.
            wPos.length = sizeof(wPos);
            GetWindowPlacement(hWnd, &wPos);
            if (wPos.showCmd != SW_SHOWMAXIMIZED) {
                RECT borderThickness;
                SetRectEmpty(&borderThickness);
                AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL);
                borderThickness.left *= -1;
                borderThickness.top *= -1;
                NCCALCSIZE_PARAMS *sz = reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);
                // Add 1 pixel to the top border to make the window resizable from the top border
                sz->rgrc[0].top += 1; // borderThickness.top;
                sz->rgrc[0].left += borderThickness.left;
                sz->rgrc[0].right -= borderThickness.right;
                sz->rgrc[0].bottom -= borderThickness.bottom;
                return 0;
            }
        }
        break;

    case WM_GETMINMAXINFO: {
        auto mmi = (MINMAXINFO*) lParam;
        HandleGetMinMaxInfo(mmi);
        AdjustWorkingAreaForAutoHide(hWnd, mmi);
        return 0;
    }
    }
    return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}

#endif

// Called when closing the application and when switching the application language.
void MainFrame::shutdown()
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "MainFrame::shutdown enter";
    // BBS: backup
    Slic3r::set_backup_callback(nullptr);

    Plater* plater = AppAdapter::plater();
    if (plater != nullptr) {
        plater->get_ui_job_worker().cancel_all();

        // Unbinding of wxWidgets event handling in canvases needs to be done here because on MAC,
        // when closing the application using Command+Q, a mouse event is triggered after this lambda is completed,
        // causing a crash
        plater->unbind_canvas_event_handlers();

        // Cleanup of canvases' volumes needs to be done here or a crash may happen on some Linux Debian flavours
        plater->reset_canvas_volumes();
    }

    // Weird things happen as the Paint messages are floating around the windows being destructed.
    // Avoid the Paint messages by hiding the main window.
    // Also the application closes much faster without these unnecessary screen refreshes.
    // In addition, there were some crashes due to the Paint events sent to already destructed windows.
    this->Show(false);

    if (plater != nullptr) {
        // Stop the background thread (Windows and Linux).
        // Disconnect from a 3DConnextion driver (OSX).
        plater->get_mouse3d_controller().shutdown();
        // Store the device parameter database back to appconfig.
        plater->get_mouse3d_controller().save_config(*AppAdapter::app_config());
    }

	//stop listening for messages from other instances
	other_instance_message_handler()->shutdown(this);
    // Save the slic3r.ini.Usually the ini file is saved from "on idle" callback,
    // but in rare cases it may not have been called yet.
    if(AppAdapter::app_config()->dirty())
        AppAdapter::app_config()->save();

    // set to null tabs and a plater
    // to avoid any manipulations with them from App->wxEVT_IDLE after of the mainframe closing
    AppAdapter::gui_app()->tabs_list.clear();
    AppAdapter::gui_app()->model_tabs_list.clear();
    AppAdapter::gui_app()->shutdown();

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "MainFrame::shutdown exit";
}

void MainFrame::update_title()
{
    return;
}

void MainFrame::set_title(const wxString& title)
{
#ifdef __WINDOWS__
    SetTitle(title);
    m_topbar->SetTitle(title);
#else
    SetTitle(title);
    if (!title.IsEmpty())
    {
#ifdef __APPLE__
        set_title_colour_after_set_title(GetHandle());
#endif        
    }
#endif
}

void MainFrame::setup_context(MainPanel* panel)
{
#ifndef __APPLE__
    m_topbar         = new BBLTopbar(this, this);
#else
    auto panel_topbar = new wxPanel(this, wxID_ANY);
    panel_topbar->SetBackgroundColour(wxColour(38, 46, 48));
    auto sizer_tobar = new wxBoxSizer(wxVERTICAL);
    panel_topbar->SetSizer(sizer_tobar);
    panel_topbar->Layout();
#endif

    m_panel = panel;
    m_plater = panel ? panel->plater() : nullptr;
    // Load the icon either from the exe, or from the ico file.
    SetIcon(main_frame_icon());  
    
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
#ifndef __APPLE__
     mainSizer->Add(m_topbar, 0, wxEXPAND);
#else
     mainSizer->Add(panel_topbar, 0, wxEXPAND);
#endif // __WINDOWS__

    mainSizer->Add(m_panel, 1, wxEXPAND | wxALL, 0);

    SetSizer(mainSizer);

    // m_panel->Hide();
    // m_topbar->Hide();

    Bind(wxEVT_SIZE, [this](wxSizeEvent&) 
    {
#ifndef __APPLE__
            if (IsMaximized()) {
                m_topbar->SetWindowSize();
            } else {
                m_topbar->SetMaximizedSize();
            }
#endif

        wxRect workArea = wxGetClientDisplayRect();
        wxLogMessage("Work area: %d,%d %dx%d", 
                    workArea.x, workArea.y, workArea.width, workArea.height);

        wxRect frameRect = this->GetRect();
        wxLogMessage("Frame rect: %d,%d %dx%d", 
                    frameRect.x, frameRect.y, frameRect.width, frameRect.height);

        Refresh();
        Layout();
    });

#ifndef __APPLE__
        m_topbar->EnableUndoRedoItems();
#endif

    init_menubar_as_editor();
}

void MainFrame::on_dpi_changed(const wxRect& suggested_rect)
{
    //m_main_panel->on_dpi_changed(suggested_rect);
#ifndef __APPLE__
    // BBS
    m_topbar->Rescale();
#endif
    // Workarounds for correct Window rendering after rescale

    /* Even if Window is maximized during moving,
     * first of all we should imitate Window resizing. So:
     * 1. cancel maximization, if it was set
     * 2. imitate resizing
     * 3. set maximization, if it was set
     */
    const bool is_maximized = IsMaximized();
    if (is_maximized)
        Maximize(false);

    /* To correct window rendering (especially redraw of a status bar)
     * we should imitate window resizing.
     */
    const wxSize& sz = GetSize();
    SetSize(sz.x + 1, sz.y + 1);
    SetSize(sz);

    Maximize(is_maximized);
}

void MainFrame::on_sys_color_changed()
{
    //m_main_panel->on_sys_color_changed();
}

wxMenu* MainFrame::generate_help_menu()
{
    wxMenu* helpMenu = new wxMenu();

    // shortcut key
    append_menu_item(helpMenu, wxID_ANY, _L("Keyboard Shortcuts") + sep + "&?", _L("Show the list of the keyboard shortcuts"),
        [this](wxCommandEvent&) {
            // open_keyboard_shortcuts_dialog(this);
        });
    // Show Beginner's Tutorial
    append_menu_item(helpMenu, wxID_ANY, _L("Setup Wizard"), _L("Setup Wizard"), 
        [](wxCommandEvent &) {
            AppAdapter::gui_app()->ShowUserGuide();
        });

    helpMenu->AppendSeparator();
    // Open Config Folder
    append_menu_item(helpMenu, wxID_ANY, _L("Show Configuration Folder"), _L("Show Configuration Folder"),
        [](wxCommandEvent&) { 
            // Slic3r::GUI::desktop_open_datadir_folder(); 
        });

    append_menu_item(helpMenu, wxID_ANY, _L("Show Tip of the Day"), _L("Show Tip of the Day"), [](wxCommandEvent&) {
        // AppAdapter::plater()->get_dailytips()->open();
        // AppAdapter::plater()->get_current_canvas3D()->set_as_dirty();
        });

    append_menu_item(helpMenu, wxID_ANY, _L("Check for Update"), _L("Check for Update"),
        [](wxCommandEvent&) {
        }, "", nullptr, []() {
            return true;
        });

    append_menu_item(helpMenu, wxID_ANY, _L("Open Network Test"), _L("Open Network Test"), [this](wxCommandEvent&) {
            // open_network_test_dialog(this);
        });

    // About
#ifndef __APPLE__
    wxString about_title = wxString::Format(_L("&About %s"), SLIC3R_APP_FULL_NAME);
    append_menu_item(helpMenu, wxID_ANY, about_title, about_title,
            [this](wxCommandEvent&) {
                // open_about_dialog(this);
            });
#endif

    return helpMenu;
}


static void add_common_publish_menu_items(wxMenu* publish_menu, MainPanel* mainFrame)
{
}

static void add_common_view_menu_items(wxMenu* view_menu, MainPanel* mainFrame, std::function<bool(void)> can_change_view)
{
    append_menu_item(view_menu, wxID_ANY, _L("Default View") + "\t" + ctrl + "0", _L("Default View"), [mainFrame](wxCommandEvent&) {
        // mainFrame->select_view("plate");
        // mainFrame->plater()->get_current_canvas3D()->zoom_to_bed();
        },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    //view_menu->AppendSeparator();
    //TRN To be shown in the main menu View->Top
    append_menu_item(view_menu, wxID_ANY, _L("Top") + "\t" + ctrl + "1", _L("Top View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("top"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    //TRN To be shown in the main menu View->Bottom
    append_menu_item(view_menu, wxID_ANY, _L("Bottom") + "\t" + ctrl + "2", _L("Bottom View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("bottom"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _L("Front") + "\t" + ctrl + "3", _L("Front View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("front"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _L("Rear") + "\t" + ctrl + "4", _L("Rear View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("rear"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _L("Left") + "\t" + ctrl + "5", _L("Left View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("left"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _L("Right") + "\t" + ctrl + "6", _L("Right View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("right"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
}

void MainFrame::open_menubar_item(const wxString& menu_name,const wxString& item_name)
{
    if (m_menubar == nullptr)
        return;
    // Get menu object from menubar
    int     menu_index = m_menubar->FindMenu(menu_name);
    wxMenu* menu       = m_menubar->GetMenu(menu_index);
    if (menu == nullptr) {
        BOOST_LOG_TRIVIAL(error) << "Mainframe open_menubar_item function couldn't find menu: " << menu_name;
        return;
    }
    // Get item id from menu
    int     item_id   = menu->FindItem(item_name);
    if (item_id == wxNOT_FOUND)
    {
        // try adding three dots char
        item_id = menu->FindItem(item_name + dots);
    }
    if (item_id == wxNOT_FOUND)
    {
        BOOST_LOG_TRIVIAL(error) << "Mainframe open_menubar_item function couldn't find item: " << item_name;
        return;
    }
    // wxEVT_MENU will trigger item
    wxPostEvent((wxEvtHandler*)menu, wxCommandEvent(wxEVT_MENU, item_id));
}

void MainFrame::init_menubar_as_editor()
{
#ifdef __APPLE__
    m_menubar = new wxMenuBar();
#endif

    // File menu
    wxMenu* fileMenu = new wxMenu;
    {
#ifdef __APPLE__
        // New Window
        append_menu_item(fileMenu, wxID_ANY, _L("New Window"), _L("Start a new window"),
                         [](wxCommandEvent&) { start_new_slicer(); }, "", nullptr,
                         [this] { return m_plater != nullptr && AppAdapter::app_config()->get("app", "single_instance") == "false"; }, this);
#endif
        // New Project
        append_menu_item(fileMenu, wxID_ANY, _L("New Project") + "\t" + ctrl + "N", _L("Start a new project"),
            [this](wxCommandEvent&) {
                // if (m_plater) m_plater->new_project(); 
            }, "", nullptr,
            [this](){
                // return can_start_new_project(); 
                return false;
            }, this);
        // Open Project

// #ifndef __APPLE__
//         append_menu_item(fileMenu, wxID_ANY, _L("Open Project") + dots + "\t" + ctrl + "O", _L("Open a project file"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->load_project(); }, "menu_open", nullptr,
//             [this](){return can_open_project(); }, this);
// #else
//         append_menu_item(fileMenu, wxID_ANY, _L("Open Project") + dots + "\t" + ctrl + "O", _L("Open a project file"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->load_project(); }, "", nullptr,
//             [this](){return can_open_project(); }, this);
// #endif

//         // Recent Project
//         wxMenu* recent_projects_menu = new wxMenu();
//         wxMenuItem* recent_projects_submenu = append_submenu(fileMenu, recent_projects_menu, wxID_ANY, _L("Recent projects"), "");
//         m_recent_projects.UseMenu(recent_projects_menu);
//         Bind(wxEVT_MENU, [this](wxCommandEvent& evt) {
//             size_t file_id = evt.GetId() - wxID_FILE1;
//             wxString filename = m_recent_projects.GetHistoryFile(file_id);
//                 open_recent_project(file_id, filename);
//             }, wxID_FILE1, wxID_FILE1 + 49); // [5050, 5100)

//         std::vector<std::string> recent_projects = AppAdapter::app_config()->get_recent_projects();
//         std::reverse(recent_projects.begin(), recent_projects.end());
//         for (const std::string& project : recent_projects)
//         {
//             m_recent_projects.AddFileToHistory(from_u8(project));
//         }
//         m_recent_projects.LoadThumbnails();

//         Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& evt) { evt.Enable(can_open_project() && (m_recent_projects.GetCount() > 0)); }, recent_projects_submenu->GetId());

//         // BBS: close save project
// #ifndef __APPLE__
//         append_menu_item(fileMenu, wxID_ANY, _L("Save Project") + "\t" + ctrl + "S", _L("Save current project to file"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(); }, "menu_save", nullptr,
//             [this](){return m_plater != nullptr && can_save(); }, this);
// #else
//         append_menu_item(fileMenu, wxID_ANY, _L("Save Project") + "\t" + ctrl + "S", _L("Save current project to file"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(); }, "", nullptr,
//             [this](){return m_plater != nullptr && can_save(); }, this);
// #endif


// #ifndef __APPLE__
//         append_menu_item(fileMenu, wxID_ANY, _L("Save Project as") + dots + "\t" + ctrl + _L("Shift+") + "S", _L("Save current project as"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(true); }, "menu_save", nullptr,
//             [this](){return m_plater != nullptr && can_save_as(); }, this);
// #else
//         append_menu_item(fileMenu, wxID_ANY, _L("Save Project as") + dots + "\t" + ctrl + _L("Shift+") + "S", _L("Save current project as"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(true); }, "", nullptr,
//             [this](){return m_plater != nullptr && can_save_as(); }, this);
// #endif


        fileMenu->AppendSeparator();

        // BBS
        wxMenu *import_menu = new wxMenu();
// #ifndef __APPLE__
//         append_menu_item(import_menu, wxID_ANY, _L("Import 3MF/STL/STEP/SVG/OBJ/AMF") + dots + "\t" + ctrl + "I", _L("Load a model"),
//             [this](wxCommandEvent&) { if (m_plater) {
//             m_plater->add_file();
//         } }, "menu_import", nullptr,
//             [this](){return can_add_models(); }, this);
// #else
//         append_menu_item(import_menu, wxID_ANY, _L("Import 3MF/STL/STEP/SVG/OBJ/AMF") + dots + "\t" + ctrl + "I", _L("Load a model"),
//             [this](wxCommandEvent&) { if (m_plater) { m_plater->add_model(); } }, "", nullptr,
//             [this](){return can_add_models(); }, this);
// #endif
//         append_menu_item(import_menu, wxID_ANY, _L("Import Zip Archive") + dots, _L("Load models contained within a zip archive"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->import_zip_archive(); }, "menu_import", nullptr,
//             [this]() { return can_add_models(); });
//         append_menu_item(import_menu, wxID_ANY, _L("Import Configs") + dots /*+ "\tCtrl+I"*/, _L("Load configs"),
//             [this](wxCommandEvent&) { load_config_file(); }, "menu_import", nullptr,
//             [this](){return true; }, this);

//         append_submenu(fileMenu, import_menu, wxID_ANY, _L("Import"), "");


//         wxMenu* export_menu = new wxMenu();
//         // BBS export as STL
//         append_menu_item(export_menu, wxID_ANY, _L("Export all objects as one STL") + dots, _L("Export all objects as one STL"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->export_stl(); }, "menu_export_stl", nullptr,
//             [this](){return can_export_model(); }, this);
//         append_menu_item(export_menu, wxID_ANY, _L("Export all objects as STLs") + dots, _L("Export all objects as STLs"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->export_stl(false, false, true); }, "menu_export_stl", nullptr,
//             [this](){return can_export_model(); }, this);
//         append_menu_item(export_menu, wxID_ANY, _L("Export Generic 3MF") + dots/* + "\tCtrl+G"*/, _L("Export 3mf file without using some 3mf-extensions"),
//             [this](wxCommandEvent&) { if (m_plater) m_plater->export_core_3mf(); }, "menu_export_sliced_file", nullptr,
//             [this](){return can_export_model(); }, this);
//         // BBS export .gcode.3mf
//         append_menu_item(export_menu, wxID_ANY, _L("Export plate sliced file") + dots + "\t" + ctrl + "G", _L("Export current sliced file"),
//             [this](wxCommandEvent&) { if (m_plater) wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_SLICED_FILE)); }, "menu_export_sliced_file", nullptr,
//             [this](){return can_export_gcode(); }, this);

        // append_menu_item(export_menu, wxID_ANY, _L("Export all plate sliced file") + dots/* + "\tCtrl+G"*/, _L("Export all plate sliced file"),
        //     [this](wxCommandEvent&) { if (m_plater) wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_ALL_SLICED_FILE)); }, "menu_export_sliced_file", nullptr,
        //     [this]() {return can_export_all_gcode(); }, this);

        // append_menu_item(export_menu, wxID_ANY, _L("Export G-code") + dots/* + "\tCtrl+G"*/, _L("Export current plate as G-code"),
        //     [this](wxCommandEvent&) {
        //         if (m_plater)
        //             m_plater->export_gcode(false);
        //         }, 
        //         "menu_export_gcode",  nullptr, 
        //         [this]() {
        //             return can_export_gcode();
        //         }, this);
                
        // append_menu_item(
        //     export_menu, wxID_ANY, _L("Export Preset Bundle") + dots /* + "\tCtrl+E"*/, _L("Export current configuration to files"),
        //     [this](wxCommandEvent &) { export_config(); },
        //     "menu_export_config", nullptr,
        //     []() { return true; }, this);

        // append_submenu(fileMenu, export_menu, wxID_ANY, _L("Export"), "");

        fileMenu->AppendSeparator();

#ifndef __APPLE__
        append_menu_item(fileMenu, wxID_EXIT, _L("Quit"), wxString::Format(_L("Quit")),
            [this](wxCommandEvent&) { Close(false); }, "menu_exit", nullptr);
#else
        append_menu_item(fileMenu, wxID_EXIT, _L("Quit"), wxString::Format(_L("Quit")),
            [this](wxCommandEvent&) { Close(false); }, "", nullptr);
#endif
    }

    // Edit menu
//     wxMenu* editMenu = nullptr;
//     if (m_plater != nullptr)
//     {
//         editMenu = new wxMenu();
//     #ifdef __APPLE__
//         // Backspace sign
//         wxString hotkey_delete = "\u232b";
//     #else
//         wxString hotkey_delete = "Del";
//     #endif

//     auto handle_key_event = [](wxKeyEvent& evt) {
//         if (global_im_gui().update_key_data(evt)) {
//             AppAdapter::plater()->get_current_canvas3D()->render();
//             return true;
//         }
//         return false;
//     };
// #ifndef __APPLE__
//         // BBS undo
//         append_menu_item(editMenu, wxID_ANY, _L("Undo") + "\t" + ctrl + "Z",
//             _L("Undo"), [this](wxCommandEvent&) { m_plater->undo(); },
//             "menu_undo", nullptr, [this](){return m_plater->can_undo(); }, this);
//         // BBS redo
//         append_menu_item(editMenu, wxID_ANY, _L("Redo") + "\t" + ctrl + "Y",
//             _L("Redo"), [this](wxCommandEvent&) { m_plater->redo(); },
//             "menu_redo", nullptr, [this](){return m_plater->can_redo(); }, this);
//         editMenu->AppendSeparator();
//         // BBS Cut TODO
//         append_menu_item(editMenu, wxID_ANY, _L("Cut") + "\t" + ctrl + "X",
//             _L("Cut selection to clipboard"), [this](wxCommandEvent&) {m_plater->cut_selection_to_clipboard(); },
//             "menu_cut", nullptr, [this]() {return m_plater->can_copy_to_clipboard(); }, this);
//         // BBS Copy
//         append_menu_item(editMenu, wxID_ANY, _L("Copy") + "\t" + ctrl + "C",
//             _L("Copy selection to clipboard"), [this](wxCommandEvent&) { m_plater->copy_selection_to_clipboard(); },
//             "menu_copy", nullptr, [this](){return m_plater->can_copy_to_clipboard(); }, this);
//         // BBS Paste
//         append_menu_item(editMenu, wxID_ANY, _L("Paste") + "\t" + ctrl + "V",
//             _L("Paste clipboard"), [this](wxCommandEvent&) { m_plater->paste_from_clipboard(); },
//             "menu_paste", nullptr, [this](){return m_plater->can_paste_from_clipboard(); }, this);
//         // BBS Delete selected
//         append_menu_item(editMenu, wxID_ANY, _L("Delete selected") + "\t" + _L("Del"),
//             _L("Deletes the current selection"),[this](wxCommandEvent&) { m_plater->remove_selected(); },
//             "menu_remove", nullptr, [this](){return can_delete(); }, this);
//         //BBS: delete all
//         append_menu_item(editMenu, wxID_ANY, _L("Delete all") + "\t" + ctrl + "D",
//             _L("Deletes all objects"),[this](wxCommandEvent&) { m_plater->delete_all_objects_from_model(); },
//             "menu_remove", nullptr, [this](){return can_delete_all(); }, this);
//         editMenu->AppendSeparator();
//         // BBS Clone Selected
//         append_menu_item(editMenu, wxID_ANY, _L("Clone selected") /*+ "\tCtrl+M"*/,
//             _L("Clone copies of selections"),[this](wxCommandEvent&) {
//                 m_plater->clone_selection();
//             },
//             "menu_remove", nullptr, [this](){return can_clone(); }, this);
//         editMenu->AppendSeparator();
//         append_menu_item(editMenu, wxID_ANY, _L("Duplicate Current Plate"),
//             _L("Duplicate the current plate"),[this](wxCommandEvent&) {
//                 m_plater->duplicate_plate();
//             },
//             "menu_remove", nullptr, [this](){return true;}, this);
//         editMenu->AppendSeparator();
// #else
//         // BBS undo
//         append_menu_item(editMenu, wxID_ANY, _L("Undo") + "\t" + ctrl + "Z",
//             _L("Undo"), [this, handle_key_event](wxCommandEvent&) {
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.SetControlDown(true);
//                 e.m_keyCode = 'Z';
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->undo(); },
//             "", nullptr, [this](){return m_plater->can_undo(); }, this);
//         // BBS redo
//         append_menu_item(editMenu, wxID_ANY, _L("Redo") + "\t" + ctrl + "Y",
//             _L("Redo"), [this, handle_key_event](wxCommandEvent&) {
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.SetControlDown(true);
//                 e.m_keyCode = 'Y';
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->redo(); },
//             "", nullptr, [this](){return m_plater->can_redo(); }, this);
//         editMenu->AppendSeparator();
//         // BBS Cut TODO
//         append_menu_item(editMenu, wxID_ANY, _L("Cut") + "\t" + ctrl + "X",
//             _L("Cut selection to clipboard"), [this, handle_key_event](wxCommandEvent&) {
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.SetControlDown(true);
//                 e.m_keyCode = 'X';
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->cut_selection_to_clipboard(); },
//             "", nullptr, [this]() {return m_plater->can_copy_to_clipboard(); }, this);
//         // BBS Copy
//         append_menu_item(editMenu, wxID_ANY, _L("Copy") + "\t" + ctrl + "C",
//             _L("Copy selection to clipboard"), [this, handle_key_event](wxCommandEvent&) {
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.SetControlDown(true);
//                 e.m_keyCode = 'C';
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->copy_selection_to_clipboard(); },
//             "", nullptr, [this](){return m_plater->can_copy_to_clipboard(); }, this);
//         // BBS Paste
//         append_menu_item(editMenu, wxID_ANY, _L("Paste") + "\t" + ctrl + "V",
//             _L("Paste clipboard"), [this, handle_key_event](wxCommandEvent&) {
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.SetControlDown(true);
//                 e.m_keyCode = 'V';
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->paste_from_clipboard(); },
//             "", nullptr, [this](){return m_plater->can_paste_from_clipboard(); }, this);
// #if 0
//         // BBS Delete selected
//         append_menu_item(editMenu, wxID_ANY, _L("Delete selected") + "\tBackSpace",
//             _L("Deletes the current selection"),[this](wxCommandEvent&) { 
//                 m_plater->remove_selected();
//             },
//             "", nullptr, [this](){return can_delete(); }, this);
// #endif
//         //BBS: delete all
//         append_menu_item(editMenu, wxID_ANY, _L("Delete all") + "\t" + ctrl + "D",
//             _L("Deletes all objects"),[this, handle_key_event](wxCommandEvent&) {
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.SetControlDown(true);
//                 e.m_keyCode = 'D';
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->delete_all_objects_from_model(); },
//             "", nullptr, [this](){return can_delete_all(); }, this);
//         editMenu->AppendSeparator();
//         // BBS Clone Selected
//         append_menu_item(editMenu, wxID_ANY, _L("Clone selected") + "\t" + ctrl + "K",
//             _L("Clone copies of selections"),[this, handle_key_event](wxCommandEvent&) {
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.SetControlDown(true);
//                 e.m_keyCode = 'M';
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->clone_selection();
//             },
//             "", nullptr, [this](){return can_clone(); }, this);
//         editMenu->AppendSeparator();
//         append_menu_item(editMenu, wxID_ANY, _L("Duplicate Current Plate"),
//             _L("Duplicate the current plate"),[this, handle_key_event](wxCommandEvent&) {
//                 m_plater->duplicate_plate();
//             },
//             "", nullptr, [this](){return true;}, this);
//         editMenu->AppendSeparator();

// #endif

//         // BBS Select All
//         append_menu_item(editMenu, wxID_ANY, _L("Select all") + "\t" + ctrl + "A",
//             _L("Selects all objects"), [this, handle_key_event](wxCommandEvent&) { 
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.SetControlDown(true);
//                 e.m_keyCode = 'A';
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->select_all(); },
//             "", nullptr, [this](){return can_select(); }, this);
//         // BBS Deslect All
//         append_menu_item(editMenu, wxID_ANY, _L("Deselect all") + "\tEsc",
//             _L("Deselects all objects"), [this, handle_key_event](wxCommandEvent&) {
//                 wxKeyEvent e;
//                 e.SetEventType(wxEVT_KEY_DOWN);
//                 e.m_keyCode = WXK_ESCAPE;
//                 if (handle_key_event(e)) {
//                     return;
//                 }
//                 m_plater->deselect_all(); },
//             "", nullptr, [this](){return can_deselect(); }, this);
//     }

    // BBS
    // View menu
    wxMenu* viewMenu = nullptr;
    // if (m_plater) {
    //     viewMenu = new wxMenu();
    //     add_common_view_menu_items(viewMenu, this, std::bind(&MainPanel::can_change_view, this));
    //     viewMenu->AppendSeparator();

    //     //BBS perspective view
    //     wxWindowID camera_id_base = wxWindow::NewControlId(int(wxID_CAMERA_COUNT));
    //     auto perspective_item = append_menu_radio_item(viewMenu, wxID_CAMERA_PERSPECTIVE + camera_id_base, _L("Use Perspective View"), _L("Use Perspective View"),
    //         [this](wxCommandEvent&) {
    //             AppAdapter::app_config()->set_bool("use_perspective_camera", true);
    //             AppAdapter::gui_app()->update_ui_from_settings();
    //         }, nullptr);
    //     //BBS orthogonal view
    //     auto orthogonal_item = append_menu_radio_item(viewMenu, wxID_CAMERA_ORTHOGONAL + camera_id_base, _L("Use Orthogonal View"), _L("Use Orthogonal View"),
    //         [this](wxCommandEvent&) {
    //             AppAdapter::app_config()->set_bool("use_perspective_camera", false);
    //             AppAdapter::gui_app()->update_ui_from_settings();
    //         }, nullptr);
    //     this->Bind(wxEVT_UPDATE_UI, [viewMenu, camera_id_base](wxUpdateUIEvent& evt) {
    //             if (AppAdapter::app_config()->get("use_perspective_camera").compare("true") == 0)
    //                 viewMenu->Check(wxID_CAMERA_PERSPECTIVE + camera_id_base, true);
    //             else
    //                 viewMenu->Check(wxID_CAMERA_ORTHOGONAL + camera_id_base, true);
    //         }, wxID_ANY);
    //     append_menu_check_item(viewMenu, wxID_ANY, _L("Auto Perspective"), _L("Automatically switch between orthographic and perspective when changing from top/bottom/side views"),
    //         [this](wxCommandEvent&) {
    //             AppAdapter::app_config()->set_bool("auto_perspective", !AppAdapter::app_config()->get_bool("auto_perspective"));
    //             m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT));
    //         },
    //         this, [this]() { return m_tabpanel->GetSelection() == TabPosition::tp3DEditor || m_tabpanel->GetSelection() == TabPosition::tpPreview; },
    //         [this]() { return AppAdapter::app_config()->get_bool("auto_perspective"); }, this);

    //     viewMenu->AppendSeparator();
    //     append_menu_check_item(viewMenu, wxID_ANY, _L("Show &G-code Window") + "\tC", _L("Show g-code window in Preview scene"),
    //         [this](wxCommandEvent &) {
    //             toggle_show_gcode_window();
    //             m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT));
    //         },
    //         this, [this]() { return m_tabpanel->GetSelection() == tpPreview; },
    //         [this]() { return show_gcode_window(); }, this);

    //     append_menu_check_item(
    //         viewMenu, wxID_ANY, _L("Show 3D Navigator"), _L("Show 3D navigator in Prepare and Preview scene"),
    //         [this](wxCommandEvent&) {
    //             toggle_show_3d_navigator();
    //             m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT));
    //         },
    //         this, [this]() { return m_tabpanel->GetSelection() == TabPosition::tp3DEditor || m_tabpanel->GetSelection() == TabPosition::tpPreview; },
    //         [this]() { return show_3d_navigator(); }, this);

    //     append_menu_item(
    //         viewMenu, wxID_ANY, _L("Reset Window Layout"), _L("Reset to default window layout"),
    //         [this](wxCommandEvent&) { m_plater->reset_window_layout(); }, "", this,
    //         [this]() {
    //             return (m_tabpanel->GetSelection() == TabPosition::tp3DEditor || m_tabpanel->GetSelection() == TabPosition::tpPreview) &&
    //                    m_plater->is_sidebar_enabled();
    //         },
    //         this);

    //     viewMenu->AppendSeparator();
    //     append_menu_check_item(viewMenu, wxID_ANY, _L("Show &Labels") + "\t" + ctrl + "E", _L("Show object labels in 3D scene"),
    //         [this](wxCommandEvent&) { m_plater->show_view3D_labels(!m_plater->are_view3D_labels_shown()); m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT)); }, this,
    //         [this]() { return m_plater->is_view3D_shown(); }, [this]() { return m_plater->are_view3D_labels_shown(); }, this);

    //     append_menu_check_item(viewMenu, wxID_ANY, _L("Show &Overhang"), _L("Show object overhang highlight in 3D scene"),
    //         [this](wxCommandEvent &) {
    //             m_plater->show_view3D_overhang(!m_plater->is_view3D_overhang_shown());
    //             m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT));
    //         },
    //         this, [this]() { return m_plater->is_view3D_shown(); }, [this]() { return m_plater->is_view3D_overhang_shown(); }, this);

    //     append_menu_check_item(
    //         viewMenu, wxID_ANY, _L("Show Selected Outline (beta)"), _L("Show outline around selected object in 3D scene"),
    //         [this](wxCommandEvent&) {
    //             toggle_show_outline();
    //             m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT));
    //         },
    //         this, [this]() { return m_tabpanel->GetSelection() == TabPosition::tp3DEditor; },
    //         [this]() { return show_outline(); }, this);
    // }
    
//     wxWindowID config_id_base = wxWindow::NewControlId(int(ConfigMenuCnt));
// #ifdef __APPLE__
//     wxWindowID bambu_studio_id_base = wxWindow::NewControlId(int(2));
//     wxMenu* parent_menu = m_menubar->OSXGetAppleMenu();
// #else
//     wxMenu* parent_menu = m_topbar->GetTopMenu();
//     auto preference_item = new wxMenuItem(parent_menu, ConfigMenuPreferences + config_id_base, _L("Preferences") + "\t" + ctrl + "P", "");
// #endif

#ifdef __APPLE__
    wxString about_title = wxString::Format(_L("&About %s"), SLIC3R_APP_FULL_NAME);
    
    //parent_menu->Insert(0, about_item);
    append_menu_item(
        parent_menu, wxID_ANY, _L(about_title), "",
        [this](wxCommandEvent &) { Slic3r::GUI::about();},
        "", nullptr, []() { return true; }, this, 0);
    append_menu_item(
        parent_menu, wxID_ANY, _L("Preferences") + "\t" + ctrl + ",", "",
        [this](wxCommandEvent &) {
            PreferencesDialog dlg(this);
            dlg.ShowModal();
            plater()->get_current_canvas3D()->force_set_focus();
        },
        "", nullptr, []() { return true; }, this, 1);
#endif
    // Help menu
    auto helpMenu = generate_help_menu();

// #ifndef __APPLE__
    m_topbar->SetFileMenu(fileMenu);
//     if (editMenu)
//         m_topbar->AddDropDownSubMenu(editMenu, _L("Edit"));
//     if (viewMenu)
//         m_topbar->AddDropDownSubMenu(viewMenu, _L("View"));
//     //BBS add Preference

//     append_menu_item(
//         m_topbar->GetTopMenu(), wxID_ANY, _L("Preferences") + "\t" + ctrl + "P", "",
//         [this](wxCommandEvent &) {
//             AppAdapter::gui_app()->open_preferences();
//             plater()->get_current_canvas3D()->force_set_focus();
//         },
//         "", nullptr, []() { return true; }, this);
//     m_topbar->AddDropDownSubMenu(helpMenu, _L("Help"));

    // Multi-Nozzle Calibration
    append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Multi-Nozzle Calibration"), _L("Multi-Nozzle Calibration"),
        [this](wxCommandEvent&) {
            if (!m_multi_nozzle_calib_dlg)
                m_multi_nozzle_calib_dlg = new MultiNozzle_Calibration_Dlg((wxWindow*)this, wxID_ANY, m_plater);
            m_multi_nozzle_calib_dlg->ShowModal();
        });

//     // Flowrate
//     append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Temperature"), _L("Temperature Calibration"),
//         [this](wxCommandEvent&) {
//             if (!m_temp_calib_dlg)
//                 m_temp_calib_dlg = new Temp_Calibration_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_temp_calib_dlg->ShowModal();
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     auto flowrate_menu = new wxMenu();
//     append_menu_item(
//         flowrate_menu, wxID_ANY, _L("Pass 1"), _L("Flow rate test - Pass 1"),
//         [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(false, 1); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     append_menu_item(flowrate_menu, wxID_ANY, _L("Pass 2"), _L("Flow rate test - Pass 2"),
//         [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(false, 2); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     flowrate_menu->AppendSeparator();
//     append_menu_item(flowrate_menu, wxID_ANY, _L("YOLO (Recommended)"), _L("Orca YOLO flowrate calibration, 0.01 step"),
//         [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(true, 1); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     append_menu_item(flowrate_menu, wxID_ANY, _L("YOLO (perfectionist version)"), _L("Orca YOLO flowrate calibration, 0.005 step"),
//         [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(true, 2); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     m_topbar->GetCalibMenu()->AppendSubMenu(flowrate_menu, _L("Flow rate"));
//     append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Pressure advance"), _L("Pressure advance"),
//         [this](wxCommandEvent&) {
//             if (!m_pa_calib_dlg)
//                 m_pa_calib_dlg = new PA_Calibration_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_pa_calib_dlg->ShowModal();
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Retraction test"), _L("Retraction test"),
//         [this](wxCommandEvent&) {
//             if (!m_retraction_calib_dlg)
//                 m_retraction_calib_dlg = new Retraction_Test_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_retraction_calib_dlg->ShowModal();
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
        
//     append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Orca Tolerance Test"), _L("Orca Tolerance Test"),
//         [this](wxCommandEvent&) {
//             m_plater->new_project();
//         m_plater->add_model(false, Slic3r::resources_dir() + "/calib/tolerance_test/OrcaToleranceTest.stl");
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     // Advance calibrations
//     auto advance_menu = new wxMenu();

//     append_menu_item(
//         advance_menu, wxID_ANY, _L("Max flowrate"), _L("Max flowrate"),
//         [this](wxCommandEvent&) {
//             if (!m_vol_test_dlg)
//                 m_vol_test_dlg = new MaxVolumetricSpeed_Test_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_vol_test_dlg->ShowModal();
//         },
//         "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);

//     append_menu_item(
//         advance_menu, wxID_ANY, _L("VFA"), _L("VFA"),
//         [this](wxCommandEvent&) {
//             if (!m_vfa_test_dlg)
//                 m_vfa_test_dlg = new VFA_Test_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_vfa_test_dlg->ShowModal();
//         },
//         "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     m_topbar->GetCalibMenu()->AppendSubMenu(advance_menu, _L("More..."));

//     // help 
//     append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Tutorial"), _L("Calibration help"),
//         [this](wxCommandEvent&) {
//             std::string url = "https://github.com/SoftFever/OrcaSlicer/wiki/Calibration";
//             if (const std::string country_code = AppAdapter::app_config()->get_country_code(); country_code == "CN") {
//                 // Use gitee mirror for China users
//                 url = "https://gitee.com/n0isyfox/orca-slicer-docs/wikis/%E6%A0%A1%E5%87%86/%E6%89%93%E5%8D%B0%E5%8F%82%E6%95%B0%E6%A0%A1%E5%87%86";
//             }
//             wxLaunchDefaultBrowser(url, wxBROWSER_NEW_WINDOW);
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);

// #else
//     m_menubar->Append(fileMenu, wxString::Format("&%s", _L("File")));
//     if (editMenu)
//         m_menubar->Append(editMenu, wxString::Format("&%s", _L("Edit")));
//     if (viewMenu)
//         m_menubar->Append(viewMenu, wxString::Format("&%s", _L("View")));
//     /*if (publishMenu)
//         m_menubar->Append(publishMenu, wxString::Format("&%s", _L("3D Models")));*/

//     // SoftFever calibrations
//     auto calib_menu = new wxMenu();

//     // PA
//     append_menu_item(calib_menu, wxID_ANY, _L("Temperature"), _L("Temperature"),
//         [this](wxCommandEvent&) {
//             if (!m_temp_calib_dlg)
//                 m_temp_calib_dlg = new Temp_Calibration_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_temp_calib_dlg->ShowModal();
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
        
//     // Flowrate
//     auto flowrate_menu = new wxMenu();
//     append_menu_item(flowrate_menu, wxID_ANY, _L("Pass 1"), _L("Flow rate test - Pass 1"),
//         [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(false, 1); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     append_menu_item(flowrate_menu, wxID_ANY, _L("Pass 2"), _L("Flow rate test - Pass 2"),
//         [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(false, 2); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     append_submenu(calib_menu,flowrate_menu,wxID_ANY,_L("Flow rate"),_L("Flow rate"),"",
//                    [this]() {return m_plater->is_view3D_shown();; });
//     flowrate_menu->AppendSeparator();
//     append_menu_item(flowrate_menu, wxID_ANY, _L("YOLO (Recommended)"), _L("Orca YOLO flowrate calibration, 0.01 step"),
//         [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(true, 1); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     append_menu_item(flowrate_menu, wxID_ANY, _L("YOLO (perfectionist version)"), _L("Orca YOLO flowrate calibration, 0.005 step"),
//         [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(true, 2); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);

//     // PA
//     append_menu_item(calib_menu, wxID_ANY, _L("Pressure advance"), _L("Pressure advance"),
//         [this](wxCommandEvent&) {
//             if (!m_pa_calib_dlg)
//                 m_pa_calib_dlg = new PA_Calibration_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_pa_calib_dlg->ShowModal();
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);

//     // Retraction
//     append_menu_item(calib_menu, wxID_ANY, _L("Retraction test"), _L("Retraction test"),
//         [this](wxCommandEvent&) {
//             if (!m_retraction_calib_dlg)
//                 m_retraction_calib_dlg = new Retraction_Test_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_retraction_calib_dlg->ShowModal();
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);

//     // Tolerance Test
//     append_menu_item(calib_menu, wxID_ANY, _L("Orca Tolerance Test"), _L("Orca Tolerance Test"),
//         [this](wxCommandEvent&) {
//             m_plater->new_project();
//             m_plater->add_model(false, Slic3r::resources_dir() + "/calib/tolerance_test/OrcaToleranceTest.stl");
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);

//     // Advance calibrations
//     auto advance_menu = new wxMenu();
//     append_menu_item(
//         advance_menu, wxID_ANY, _L("Max flowrate"), _L("Max flowrate"),
//         [this](wxCommandEvent&) { 
//             if (!m_vol_test_dlg)
//                 m_vol_test_dlg = new MaxVolumetricSpeed_Test_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_vol_test_dlg->ShowModal(); 
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
//     append_menu_item(
//         advance_menu, wxID_ANY, _L("VFA"), _L("VFA"),
//         [this](wxCommandEvent&) { 
//             if (!m_vfa_test_dlg)
//                 m_vfa_test_dlg = new VFA_Test_Dlg((wxWindow*)this, wxID_ANY, m_plater);
//             m_vfa_test_dlg->ShowModal();
//         }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);    
       
//     append_submenu(calib_menu, advance_menu, wxID_ANY, _L("More..."), _L("More calibrations"), "",
//         [this]() {return m_plater->is_view3D_shown();; });
//     // help
//     append_menu_item(calib_menu, wxID_ANY, _L("Tutorial"), _L("Calibration help"),
//         [this](wxCommandEvent&) { wxLaunchDefaultBrowser("https://github.com/SoftFever/OrcaSlicer/wiki/Calibration", wxBROWSER_NEW_WINDOW); }, "", nullptr,
//         [this]() {return m_plater->is_view3D_shown();; }, this);
    
//     m_menubar->Append(calib_menu,wxString::Format("&%s", _L("Calibration")));
//     if (helpMenu)
//         m_menubar->Append(helpMenu, wxString::Format("&%s", _L("Help")));
//     SetMenuBar(m_menubar);

// #endif

#ifdef __APPLE__
    // This fixes a bug on Mac OS where the quit command doesn't emit window close events
    // wx bug: https://trac.wxwidgets.org/ticket/18328
    wxMenu* apple_menu = m_menubar->OSXGetAppleMenu();
    if (apple_menu != nullptr) {
        apple_menu->Bind(wxEVT_MENU, [this](wxCommandEvent &) {
            Close();
        }, wxID_EXIT);
    }
#endif // __APPLE__
}

    // MenuFactory::sys_color_changed(m_menubar);

} // GUI
} // Slic3r

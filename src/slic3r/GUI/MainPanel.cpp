#include "MainPanel.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_tree/ptree.hpp>
#include "slic3r/Theme/AppFont.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"
#include "libslic3r/FileSystem/Log.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Utils/Str.hpp"

#include "Tab.hpp"
#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Render/3DScene.hpp"
#include "slic3r/Render/GCodePreviewCanvas.hpp"
#include "slic3r/GUI/Dialog/ParamsDialog.hpp"
#include "wxExtensions.hpp"
#include "slic3r/Render/Mouse3DController.hpp"
#include "slic3r/Theme/AppColor.hpp"
#include "slic3r/Global/InstanceCheck.hpp"
#include "I18N.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "../Utils/Process.hpp"
#include "format.hpp"
// BBS
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/GUI/Dialog/Preferences.hpp"
#include "Widgets/ProgressDialog.hpp"
#include "slic3r/Theme/MacDarkMode.hpp"
#include "libslic3r/Config/ThemeDef.hpp"
#include <fstream>
#include <string_view>

#include "AppAdapter.hpp"
#include "slic3r/GUI/Dialog/UnsavedChangesDialog.hpp"
#include "Dialog/MsgDialog.hpp"
#include "Slic3r/GUI/Frame/Notebook.hpp"
#include "slic3r/GUI/Calibration/calib_dlg.hpp"

#include "GUI_Factories.hpp"
#include "slic3r/GUI/Config/GUI_ObjectList.hpp"
#include "slic3r/Scene/NotificationManager.hpp"
#include "MarkdownTip.hpp"
#include "slic3r/GUI/Dialog/ConfigWizard.hpp"
#include "Widgets/WebView.hpp"
#include "slic3r/GUI/Frame/DailyTips.hpp"

#include "slic3r/GUI/Dialog/DialogCommand.hpp"
#include "slic3r/GUI/Event/UserGLToolBarEvent.hpp"
#include "slic3r/GUI/Frame/Notebook.hpp"
#include "slic3r/GUI/Dialog/ReleaseNote.hpp"
#include "slic3r/GUI/Widgets/Tabbook.hpp"
#include "slic3r/GUI/Event/UserPlaterEvent.hpp"

#include "slic3r/GUI/Dialog/CreatePresetsDialog.hpp"
#include "slic3r/GUI/Event/UserNetEvent.hpp"
#include "slic3r/GUI/MainFrame.hpp"

#include "slic3r/GUI/Dialog/UnsavedChangesDialog.hpp"

#ifdef _WIN32
#include <dbt.h>
#include <shlobj.h>
#include <shellapi.h>
#endif // _WIN32

enum CameraMenuIDs {
    wxID_CAMERA_PERSPECTIVE,
    wxID_CAMERA_ORTHOGONAL,
    wxID_CAMERA_COUNT,
};

enum ConfigMenuIDs {
    ConfigMenuPreferences,
    ConfigMenuPrinter,
    ConfigMenuCnt,
};

namespace Slic3r {
namespace GUI {

enum class ERescaleTarget
{
    Mainframe,
    SettingsDialog
};

#ifdef __APPLE__
class OrcaSlicerTaskBarIcon : public wxTaskBarIcon
{
public:
    OrcaSlicerTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE) : wxTaskBarIcon(iconType) {}
    wxMenu *CreatePopupMenu() override {
        wxMenu *menu = new wxMenu;
        if (AppAdapter::app_config()->get("single_instance") == "false") {
            // Only allow opening a new PrusaSlicer instance on OSX if "single_instance" is disabled,
            // as starting new instances would interfere with the locking mechanism of "single_instance" support.
            append_menu_item(menu, wxID_ANY, _L("New Window"), _L("Open a new window"),
            [](wxCommandEvent&) { start_new_slicer(); }, "", nullptr);
        }
        return menu;
    }
};

#endif // __APPLE__

// BBS
#ifndef __APPLE__
#define BORDERLESS_FRAME_STYLE (wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)
#else
#define BORDERLESS_FRAME_STYLE (wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)
#endif

MainPanel::MainPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , m_recent_projects(18)
    , m_settings_dialog()
    , diff_dialog(parent)
{
    diff_dialog.Hide();
}

void MainPanel::init()
{
#ifdef __WXOSX__
    set_miniaturizable(GetHandle());
#endif

    if (!AppAdapter::app_config()->has("user_mode")) { 
        AppAdapter::app_config()->set("user_mode", "simple");
        AppAdapter::app_config()->set_bool("developer_mode", false);
        AppAdapter::app_config()->save();
    }

    AppAdapter::app_config()->set_bool("internal_developer_mode", false);

    wxString max_recent_count_str = AppAdapter::app_config()->get("max_recent_count");
    long max_recent_count = 18;
    if (max_recent_count_str.ToLong(&max_recent_count))
        set_max_recent_count((int)max_recent_count);

    //reset log level
    auto loglevel = AppAdapter::app_config()->get("log_severity_level");
    Slic3r::set_logging_level(Slic3r::level_string_to_boost(loglevel));

    // BBS
    m_recent_projects.SetMenuPathStyle(wxFH_PATH_SHOW_ALWAYS);
    MarkdownTip::Recreate(this);

    update_font_from_mainframe();

#ifdef __APPLE__
	m_reset_title_text_colour_timer = new wxTimer();
	m_reset_title_text_colour_timer->SetOwner(this);
	Bind(wxEVT_TIMER, [this](auto& e) {
		set_title_colour_after_set_title(GetHandle());
		m_reset_title_text_colour_timer->Stop();
	});
	this->Bind(wxEVT_FULLSCREEN, [this](wxFullScreenEvent& e) {
		set_tag_when_enter_full_screen(e.IsFullScreen());
		if (!e.IsFullScreen()) {
            if (m_reset_title_text_colour_timer) {
                m_reset_title_text_colour_timer->Stop();
                m_reset_title_text_colour_timer->Start(500);
            }
		}
		e.Skip();
	});
#endif

#ifdef __APPLE__
    // Initialize the docker task bar icon.
    m_taskbar_icon = std::make_unique<OrcaSlicerTaskBarIcon>(wxTBI_DOCK);
    m_taskbar_icon->SetIcon(wxIcon(Slic3r::var("OrcaSlicer-mac_256px.ico"), wxBITMAP_TYPE_ICO), "OrcaSlicer");
    }
#endif // __APPLE__

    // initialize tabpanel and menubar
    init_tabpanel();

    // BBS
#if 0
    // This is needed on Windows to fake the CTRL+# of the window menu when using the numpad
    wxAcceleratorEntry entries[6];
    entries[0].Set(wxACCEL_CTRL, WXK_NUMPAD1, wxID_HIGHEST + 1);
    entries[1].Set(wxACCEL_CTRL, WXK_NUMPAD2, wxID_HIGHEST + 2);
    entries[2].Set(wxACCEL_CTRL, WXK_NUMPAD3, wxID_HIGHEST + 3);
    entries[3].Set(wxACCEL_CTRL, WXK_NUMPAD4, wxID_HIGHEST + 4);
    entries[4].Set(wxACCEL_CTRL, WXK_NUMPAD5, wxID_HIGHEST + 5);
    entries[5].Set(wxACCEL_CTRL, WXK_NUMPAD6, wxID_HIGHEST + 6);
    wxAcceleratorTable accel(6, entries);
    SetAcceleratorTable(accel);
#endif // _WIN32

    //BBS
    Bind(EVT_SELECT_TAB, [this](wxCommandEvent&evt) {
        TabPosition pos = (TabPosition)evt.GetInt();
        m_tabpanel->SetSelection(pos);
    });

    // set default tooltip timer in msec
    // SetAutoPop supposedly accepts long integers but some bug doesn't allow for larger values
    // (SetAutoPop is not available on GTK.)
    wxToolTip::SetAutoPop(32767);

    m_loaded = true;

    // initialize layout
    m_main_sizer = new wxBoxSizer(wxVERTICAL);
    wxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    sizer->Add(m_main_sizer, 1, wxEXPAND);
    SetSizerAndFit(sizer);
    // initialize layout from config
    update_layout();
    sizer->SetSizeHints(this);

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
    // BBS
    Fit();

    const wxSize min_size = app_min_size();

    SetMinSize(min_size/*wxSize(760, 490)*/);
    SetSize(wxSize(FromDIP(1200), FromDIP(800)));

    Layout();

    update_title();

    // declare events
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ": mainframe received close_widow event";
        if (event.CanVeto() && m_plater->get_gizmos_manager()->is_in_editing_mode(true)) {
            // prevents to open the save dirty project dialog
            event.Veto();
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< "cancelled by gizmo in editing";
            return;
        }

        auto check = [](bool yes_or_no) {
            if (yes_or_no)
                return true;
            return AppAdapter::gui_app()->check_and_save_current_preset_changes(_L("Application is closing"), _L("Closing Application while some presets are modified."));
        };

        // BBS: close save project
        int result;
        if (event.CanVeto() && ((result = m_plater->close_with_confirm(check)) == wxID_CANCEL)) {
            event.Veto();
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< "cancelled by close_with_confirm selection";
            return;
        }
        if (event.CanVeto()) {
            event.Veto();
            return;
        }

        MarkdownTip::ExitTip();

        m_plater->reset();
        this->shutdown();
        // propagate event

        event.Skip();
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ": mainframe finished process close_widow event";
    });

    //FIXME it seems this method is not called on application start-up, at least not on Windows. Why?
    // The same applies to wxEVT_CREATE, it is not being called on startup on Windows.
    Bind(wxEVT_ACTIVATE, [this](wxActivateEvent& event) {
        event.Skip();
    });

// OSX specific issue:
// When we move application between Retina and non-Retina displays, The legend on a canvas doesn't redraw
// So, redraw explicitly canvas, when application is moved
//FIXME maybe this is useful for __WXGTK3__ as well?
#if __APPLE__
    Bind(wxEVT_MOVE, [](wxMoveEvent& event) {
        AppAdapter::plater()->get_current_canvas3D()->set_as_dirty();
        AppAdapter::plater()->get_current_canvas3D()->request_extra_frame();
        event.Skip();
    });
#endif   

    update_ui_from_settings();    // FIXME (?)

    if (m_plater != nullptr) {
        // BBS
        update_slice_print_status(eEventSliceUpdate, true, true);

        // BBS: backup project
        if (AppAdapter::app_config()->get("backup_switch") == "true") {
            std::string backup_interval;
            if (!AppAdapter::app_config()->get("app", "backup_interval", backup_interval))
                backup_interval = "10";
            Slic3r::set_backup_interval(boost::lexical_cast<long>(backup_interval));
        } else {
            Slic3r::set_backup_interval(0);
        }
        Slic3r::set_backup_callback([this](int action) {
            if (action == 0) {
                wxPostEvent(this, wxCommandEvent(EVT_BACKUP_POST));
            }
            else if (action == 1) {
                if (!m_plater->up_to_date(false, true)) {
                    m_plater->export_3mf(m_plater->model().get_backup_path() + "/.3mf", SaveStrategy::Backup);
                    m_plater->up_to_date(true, true);
                }
            }
         });
        Bind(EVT_BACKUP_POST, [](wxCommandEvent& e) {
            Slic3r::run_backup_ui_tasks();
            });
;    }
    this->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &evt) {
#ifdef __APPLE__
        if (evt.CmdDown() && (evt.GetKeyCode() == 'H')) {
            //call parent_menu hide behavior
            return;}
        if (evt.CmdDown() && (!evt.ShiftDown()) && (evt.GetKeyCode() == 'M')) {
            this->Iconize();
            return;
        }
        if (evt.CmdDown() && evt.GetKeyCode() == 'Q') { wxPostEvent(this, wxCloseEvent(wxEVT_CLOSE_WINDOW)); return;}
        if (evt.CmdDown() && evt.RawControlDown() && evt.GetKeyCode() == 'F') {
            EnableFullScreenView(true);
            if (IsFullScreen()) {
                ShowFullScreen(false);
            } else {
                ShowFullScreen(true);
            }
            return;}
#endif
        if (evt.CmdDown() && evt.GetKeyCode() == 'R') { if (m_slice_enable) { AppAdapter::plater()->update(true, true); wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SLICE_PLATE)); this->m_tabpanel->SetSelection(tpPreview); } return; }
        if (evt.CmdDown() && evt.ShiftDown() && evt.GetKeyCode() == 'G') {
            m_plater->apply_background_progress();
            m_print_enable = get_enable_print_status();
            m_print_btn->Enable(m_print_enable);
            if (m_print_enable) {
                    wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SEND_GCODE));
            }
            evt.Skip();
            return;
        }
        else if (evt.CmdDown() && evt.GetKeyCode() == 'G') { if (can_export_gcode()) { wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_SLICED_FILE)); } evt.Skip(); return; }
        if (evt.CmdDown() && evt.GetKeyCode() == 'J') { return; }    
        if (evt.CmdDown() && evt.GetKeyCode() == 'N') { m_plater->new_project(); return;}
        if (evt.CmdDown() && evt.GetKeyCode() == 'O') { m_plater->load_project(); return;}
        if (evt.CmdDown() && evt.ShiftDown() && evt.GetKeyCode() == 'S') { if (can_save_as()) m_plater->save_project(true); return;}
        else if (evt.CmdDown() && evt.GetKeyCode() == 'S') { if (can_save()) m_plater->save_project(); return;}
        if (evt.CmdDown() && evt.GetKeyCode() == 'F') { 
            if (m_plater && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor || m_tabpanel->GetSelection() == TabPosition::tpPreview)) {
                m_plater->sidebar().can_search();
            }
        }
#ifdef __APPLE__
        if (evt.CmdDown() && evt.GetKeyCode() == ',')
#else
        if (evt.CmdDown() && evt.GetKeyCode() == 'P')
#endif
        {
            AppAdapter::gui_app()->open_preferences();
            plater()->get_current_canvas3D()->force_set_focus();
            return;
        }

        if (evt.CmdDown() && evt.GetKeyCode() == 'I') {
            if (!can_add_models()) return;
            if (m_plater) { m_plater->add_file(); }
            return;
        }
        evt.Skip();
    });

#ifdef _MSW_DARK_MODE
    UpdateDarkUIWin(this);
#endif // _MSW_DARK_MODE

    // persist_window_geometry(m_frame, true);
    persist_window_geometry(&m_settings_dialog, true);
    // bind events from DiffDlg

    bind_diff_dialog();
}

void MainPanel::bind_diff_dialog()
{
    auto get_tab = [](Preset::Type type) {
        Tab* null_tab = nullptr;
        for (Tab* tab : AppAdapter::gui_app()->tabs_list)
            if (tab->type() == type)
                return tab;
        return null_tab;
    };

    auto transfer = [this, get_tab](Preset::Type type) {
        get_tab(type)->transfer_options(diff_dialog.get_left_preset_name(type),
                                        diff_dialog.get_right_preset_name(type),
                                        diff_dialog.get_selected_options(type));
    };

    auto process_options = [this](std::function<void(Preset::Type)> process) {
        const Preset::Type diff_dlg_type = diff_dialog.view_type();
        if (diff_dlg_type == Preset::TYPE_INVALID) {
            for (const Preset::Type& type : diff_dialog.types_list() )
                process(type);
        }
        else
            process(diff_dlg_type);
    };

    diff_dialog.Bind(EVT_DIFF_DIALOG_TRANSFER, [process_options, transfer](SimpleEvent&){
        process_options(transfer); 
    });
}

void  MainPanel::show_log_window()
{
    m_log_window = new wxLogWindow(this, _L("Logging"), true, false);
    m_log_window->Show();
}

//BBS GUI refactor: remove unused layout new/dlg
void MainPanel::update_layout()
{
    auto restore_to_creation = [this]() {
        auto clean_sizer = [](wxSizer* sizer) {
            while (!sizer->GetChildren().IsEmpty()) {
                sizer->Detach(0);
            }
        };

        // On Linux m_plater needs to be removed from m_tabpanel before to reparent it
        int plater_page_id = m_tabpanel->FindPage(m_plater);
        if (plater_page_id != wxNOT_FOUND)
            m_tabpanel->RemovePage(plater_page_id);

        if (m_plater->GetParent() != this)
            m_plater->Reparent(this);

        if (m_tabpanel->GetParent() != this)
            m_tabpanel->Reparent(this);

        plater_page_id = (m_plater_page != nullptr) ? m_tabpanel->FindPage(m_plater_page) : wxNOT_FOUND;
        if (plater_page_id != wxNOT_FOUND) {
            m_tabpanel->DeletePage(plater_page_id);
            m_plater_page = nullptr;
        }

        clean_sizer(m_main_sizer);
        clean_sizer(m_settings_dialog.GetSizer());

        if (m_settings_dialog.IsShown())
            m_settings_dialog.Close();

        m_tabpanel->Hide();
        m_plater->Hide();

        Layout();
    };

    ESettingsLayout layout =  ESettingsLayout::Old;

    if (m_layout == layout)
        return;

    wxBusyCursor busy;

    Freeze();

    // Remove old settings
    if (m_layout != ESettingsLayout::Unknown)
        restore_to_creation();

    ESettingsLayout old_layout = m_layout;
    m_layout = layout;

    // From the very beginning the Print settings should be selected
    //m_last_selected_tab = m_layout == ESettingsLayout::Dlg ? 0 : 1;
    m_last_selected_tab = 1;

    // Set new settings
    {
        m_plater->Reparent(m_tabpanel);
        m_tabpanel->InsertPage(tp3DEditor, m_plater, _L("Prepare"), std::string("tab_3d_active"), std::string("tab_3d_active"), false);
        m_tabpanel->InsertPage(tpPreview, m_plater, _L("Preview"), std::string("tab_preview_active"), std::string("tab_preview_active"), false);
        m_main_sizer->Add(m_tabpanel, 1, wxEXPAND | wxTOP, 0);

        m_tabpanel->Bind(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED, [this](wxCommandEvent& evt)
        {
            // jump to 3deditor under preview_only mode
            if (evt.GetId() == tp3DEditor){
                m_plater->update(true);

                if (!preview_only_hint())
                    return;
            }
            evt.Skip();
        });

        m_plater->Show();
        m_tabpanel->Show();
    }

#ifdef __APPLE__
    m_plater->sidebar().change_top_border_for_mode_sizer(false);
#endif

    Layout();
    Thaw();
}

// Called when closing the application and when switching the application language.
void MainPanel::shutdown()
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "MainPanel::shutdown enter";
    // BBS: backup
    Slic3r::set_backup_callback(nullptr);
#ifdef _WIN32
	if (m_hDeviceNotify) {
		::UnregisterDeviceNotification(HDEVNOTIFY(m_hDeviceNotify));
		m_hDeviceNotify = nullptr;
	}
 	if (m_ulSHChangeNotifyRegister) {
        SHChangeNotifyDeregister(m_ulSHChangeNotifyRegister);
        m_ulSHChangeNotifyRegister = 0;
 	}
#endif // _WIN32

    if (m_plater != nullptr) {
        m_plater->get_ui_job_worker().cancel_all();

        // Unbinding of wxWidgets event handling in canvases needs to be done here because on MAC,
        // when closing the application using Command+Q, a mouse event is triggered after this lambda is completed,
        // causing a crash
        m_plater->unbind_canvas_event_handlers();

        // Cleanup of canvases' volumes needs to be done here or a crash may happen on some Linux Debian flavours
        m_plater->reset_canvas_volumes();
    }

    // Weird things happen as the Paint messages are floating around the windows being destructed.
    // Avoid the Paint messages by hiding the main window.
    // Also the application closes much faster without these unnecessary screen refreshes.
    // In addition, there were some crashes due to the Paint events sent to already destructed windows.
    this->Show(false);

    if (m_settings_dialog.IsShown())
        m_settings_dialog.Close();

    if (m_plater != nullptr) {
        // Stop the background thread (Windows and Linux).
        // Disconnect from a 3DConnextion driver (OSX).
        m_plater->get_mouse3d_controller().shutdown();
        // Store the device parameter database back to appconfig.
        m_plater->get_mouse3d_controller().save_config(*AppAdapter::app_config());
    }

    // Save the slic3r.ini.Usually the ini file is saved from "on idle" callback,
    // but in rare cases it may not have been called yet.
    if(AppAdapter::app_config()->dirty())
        AppAdapter::app_config()->save();

    // set to null tabs and a plater
    // to avoid any manipulations with them from App->wxEVT_IDLE after of the mainframe closing
    AppAdapter::gui_app()->tabs_list.clear();
    AppAdapter::gui_app()->model_tabs_list.clear();
    AppAdapter::gui_app()->shutdown();

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "MainPanel::shutdown exit";
}

void MainPanel::update_filament_tab_ui()
{
    AppAdapter::gui_app()->get_tab(Preset::Type::TYPE_FILAMENT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::Type::TYPE_FILAMENT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::Type::TYPE_FILAMENT)->update_tab_ui();
}

void MainPanel::update_title()
{
    return;
}

void MainPanel::set_title(const wxString& title)
{
#ifdef __WINDOWS__
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

void MainPanel::show_option(bool show)
{
    if (!show) {
        if (m_slice_btn->IsShown()) {
            m_slice_btn->Hide();
            m_print_btn->Hide();
            m_slice_option_btn->Hide();
            m_print_option_btn->Hide();
            Layout();
        }
    } else {
        if (!m_slice_btn->IsShown()) {
            m_slice_btn->Show();
            m_print_btn->Show();
            m_slice_option_btn->Show();
            m_print_option_btn->Show();
            Layout();
        }
    }
}

void MainPanel::init_tabpanel() {
    // wxNB_NOPAGETHEME: Disable Windows Vista theme for the Notebook background. The theme performance is terrible on
    // Windows 10 with multiple high resolution displays connected.
    // BBS
    wxBoxSizer *side_tools = create_side_tools();
    m_tabpanel = new Notebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, side_tools,
                              wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
    m_tabpanel->SetBackgroundColour(*wxWHITE);

#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
    m_tabpanel->SetFont(app_normal_font());
#endif
    m_tabpanel->Hide();
    m_settings_dialog.set_tabpanel(m_tabpanel);

#ifdef __WXMSW__
    m_tabpanel->Bind(wxEVT_BOOKCTRL_PAGE_CHANGED, [this](wxBookCtrlEvent& e) {
#else
    m_tabpanel->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& e) {
#endif
        //BBS
        wxWindow* panel = m_tabpanel->GetCurrentPage();
        int sel = m_tabpanel->GetSelection();
        //wxString page_text = m_tabpanel->GetPageText(sel);
        m_last_selected_tab = m_tabpanel->GetSelection();
        if (panel == m_plater) {
            if (sel == tp3DEditor) {
                wxPostEvent(m_plater, SimpleEvent(EVT_GLVIEWTOOLBAR_3D));
                m_param_panel->OnActivate();
            }
            else if (sel == tpPreview) {
                wxPostEvent(m_plater, SimpleEvent(EVT_GLVIEWTOOLBAR_PREVIEW));
                m_param_panel->OnActivate();
            }
        }

        if (panel)
            panel->SetFocus();
    });

    m_param_panel = new ParamsPanel(m_tabpanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT | wxTAB_TRAVERSAL);

    m_plater = new Plater(this);
    AppAdapter::gui_app()->plater_ = m_plater;
    
    m_plater->init();
    m_plater->SetBackgroundColour(*wxWHITE);
    m_plater->Hide();

    create_preset_tabs();

    if (m_plater) {
        // load initial config
        auto full_config = app_preset_bundle()->full_config();
        m_plater->on_config_change(full_config);

        // Show a correct number of filament fields.
        // nozzle_diameter is undefined when SLA printer is selected
        // BBS
        if (full_config.has("filament_colour")) {
            m_plater->on_filaments_change(full_config.option<ConfigOptionStrings>("filament_colour")->values.size());
        }
    }
}

bool MainPanel::preview_only_hint()
{
    if (m_plater && ((m_plater->using_exported_file()))) {
        BOOST_LOG_TRIVIAL(info) << boost::format("skipped tab switch from %1% to %2% in preview mode")%m_tabpanel->GetSelection() %tp3DEditor;

        ConfirmBeforeSendDialog confirm_dlg(this, wxID_ANY, _L("Warning"));
        confirm_dlg.Bind(EVT_SECONDARY_CHECK_CONFIRM, [this](wxCommandEvent& e) {
            preview_only_to_editor = true;
        });
        confirm_dlg.update_btn_label(_L("Yes"), _L("No"));
        auto filename = m_plater->get_preview_only_filename();

        confirm_dlg.update_text(filename + " " + _L("will be closed before creating a new model. Do you want to continue?"));
        confirm_dlg.on_show();
        if (preview_only_to_editor) {
            m_plater->new_project();
            preview_only_to_editor = false;
        }

        return false;
    }

    return true;
}

#ifdef WIN32
void MainPanel::register_win32_callbacks()
{
    //static GUID GUID_DEVINTERFACE_USB_DEVICE  = { 0xA5DCBF10, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED };
    //static GUID GUID_DEVINTERFACE_DISK        = { 0x53f56307, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b };
    //static GUID GUID_DEVINTERFACE_VOLUME      = { 0x71a27cdd, 0x812a, 0x11d0, 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f };
    static GUID GUID_DEVINTERFACE_HID           = { 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 };

    // Register USB HID (Human Interface Devices) notifications to trigger the 3DConnexion enumeration.
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 0 };
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_HID;
    m_hDeviceNotify = ::RegisterDeviceNotification(this->GetHWND(), &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

// or register for file handle change?
//      DEV_BROADCAST_HANDLE NotificationFilter = { 0 };
//      NotificationFilter.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
//      NotificationFilter.dbch_devicetype = DBT_DEVTYP_HANDLE;

    // Using Win32 Shell API to register for media insert / removal events.
    LPITEMIDLIST ppidl;
    if (SHGetSpecialFolderLocation(this->GetHWND(), CSIDL_DESKTOP, &ppidl) == NOERROR) {
        SHChangeNotifyEntry shCNE;
        shCNE.pidl       = ppidl;
        shCNE.fRecursive = TRUE;
        // Returns a positive integer registration identifier (ID).
        // Returns zero if out of memory or in response to invalid parameters.
        m_ulSHChangeNotifyRegister = SHChangeNotifyRegister(this->GetHWND(),        // Hwnd to receive notification
            SHCNE_DISKEVENTS,                                                       // Event types of interest (sources)
            SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED,
            //SHCNE_UPDATEITEM,                                                     // Events of interest - use SHCNE_ALLEVENTS for all events
            WM_USER_MEDIACHANGED,                                                   // Notification message to be sent upon the event
            1,                                                                      // Number of entries in the pfsne array
            &shCNE);                                                                // Array of SHChangeNotifyEntry structures that
                                                                                    // contain the notifications. This array should
                                                                                    // always be set to one when calling SHChnageNotifyRegister
                                                                                    // or SHChangeNotifyDeregister will not work properly.
        assert(m_ulSHChangeNotifyRegister != 0);    // Shell notification failed
    } else {
        // Failed to get desktop location
        assert(false);
    }

    {
        static constexpr int device_count = 1;
        RAWINPUTDEVICE devices[device_count] = { 0 };
        // multi-axis mouse (SpaceNavigator, etc.)
        devices[0].usUsagePage = 0x01;
        devices[0].usUsage = 0x08;
        if (! RegisterRawInputDevices(devices, device_count, sizeof(RAWINPUTDEVICE)))
            BOOST_LOG_TRIVIAL(error) << "RegisterRawInputDevices failed";
    }
}
#endif // _WIN32

void MainPanel::create_preset_tabs()
{
    update_label_colours_from_appconfig();

    //BBS: GUI refactor
    //m_param_panel = new ParamsPanel(m_tabpanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT | wxTAB_TRAVERSAL);
    m_param_dialog = new ParamsDialog(m_plater);

    add_created_tab(new TabPrint(m_param_panel), "cog");
    add_created_tab(new TabPrintPlate(m_param_panel), "cog");
    add_created_tab(new TabPrintObject(m_param_panel), "cog");
    add_created_tab(new TabPrintPart(m_param_panel), "cog");
    add_created_tab(new TabPrintLayer(m_param_panel), "cog");
    add_created_tab(new TabFilament(m_param_dialog->panel()), "spool");
    /* BBS work around to avoid appearance bug */
    add_created_tab(new TabPrinter(m_param_dialog->panel()), "printer");

    m_param_panel->rebuild_panels();
    m_param_dialog->panel()->rebuild_panels();
    //m_tabpanel->AddPage(m_param_panel, "Parameters", "notebook_presets_active");
    //m_tabpanel->InsertPage(tpSettings, m_param_panel, _L("Parameters"), std::string("cog"));
}

void MainPanel::add_created_tab(Tab* panel,  const std::string& bmp_name /*= ""*/)
{
    panel->create_preset_tab();

    if (panel->type() == Preset::TYPE_PLATE) {
        AppAdapter::gui_app()->tabs_list.pop_back();
        AppAdapter::gui_app()->plate_tab = panel;
    }
    // BBS: model config
    if (panel->type() == Preset::TYPE_MODEL) {
        AppAdapter::gui_app()->tabs_list.pop_back();
        AppAdapter::gui_app()->model_tabs_list.push_back(panel);
    }
}

bool MainPanel::is_active_and_shown_tab(wxPanel* panel)
{
    if (panel == m_param_panel)
        panel = m_plater;
    else
        return m_param_dialog->IsShown();

    if (m_tabpanel->GetCurrentPage() != panel)
        return false;
    return true;
}

bool MainPanel::can_start_new_project() const
{
    /*return m_plater && (!m_plater->get_project_filename(".3mf").IsEmpty() ||
                        GetTitle().StartsWith('*')||
                        AppAdapter::gui_app()->has_current_preset_changes() ||
                        !m_plater->model().objects.empty());*/
    return (m_plater && !m_plater->is_background_process_slicing());
}

bool MainPanel::can_open_project() const
{
    return (m_plater && !m_plater->is_background_process_slicing());
}

bool  MainPanel::can_add_models() const
{
    return (m_plater && !m_plater->is_background_process_slicing() && !m_plater->using_exported_file());
}

bool MainPanel::can_save() const
{
    return (m_plater != nullptr) &&
        !m_plater->get_gizmos_manager()->is_in_editing_mode(false) &&
        m_plater->is_project_dirty() && !m_plater->using_exported_file();
}

bool MainPanel::can_save_as() const
{
    return (m_plater != nullptr) &&
        !m_plater->get_gizmos_manager()->is_in_editing_mode(false) && !m_plater->using_exported_file();
}

void MainPanel::save_project()
{
    save_project_as(m_plater->get_project_filename(".3mf"));
}

bool MainPanel::save_project_as(const wxString& filename)
{
    bool ret = (m_plater != nullptr) ? m_plater->export_3mf(into_path(filename)) : false;
    if (ret) {
//        AppAdapter::gui_app()->update_saved_preset_from_current_preset();
        m_plater->reset_project_dirty_after_save();
    }
    return ret;
}

bool MainPanel::can_upload() const
{
    return true;
}

bool MainPanel::can_export_model() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

bool MainPanel::can_export_toolpaths() const
{
    return (m_plater != nullptr) && (m_plater->printer_technology() == ptFFF) && m_plater->is_preview_shown() && m_plater->is_preview_loaded() && m_plater->has_toolpaths_to_export();
}

bool MainPanel::can_export_gcode() const
{
    if (m_plater == nullptr)
        return false;

    if (m_plater->model().objects.empty())
        return false;

    // TODO:: add other filters
    PartPlateList &part_plate_list = m_plater->get_partplate_list();
    PartPlate *current_plate = part_plate_list.get_curr_plate();
    if (!current_plate->is_slice_result_ready_for_print())
        return false;

    return true;
}

bool MainPanel::can_export_all_gcode() const
{
    if (m_plater == nullptr)
        return false;

    if (m_plater->model().objects.empty())
        return false;

    // TODO:: add other filters
    PartPlateList& part_plate_list = m_plater->get_partplate_list();
    return part_plate_list.is_all_slice_results_ready_for_print();
}

bool MainPanel::can_print_3mf() const
{
    if (m_plater && !m_plater->model().objects.empty()) {
        if (app_preset_bundle()->printers.get_edited_preset().is_custom_defined())
            return false;
    }
    return true;
}

bool MainPanel::can_send_gcode() const
{
    if (m_plater && !m_plater->model().objects.empty())
    {
        auto cfg = app_preset_bundle()->printers.get_edited_preset().config;
        if (const auto *print_host_opt = cfg.option<ConfigOptionString>("print_host"); print_host_opt)
            return !print_host_opt->value.empty();
    }
    return true;
}

bool MainPanel::can_slice() const
{
#ifdef SUPPORT_BACKGROUND_PROCESSING
    bool bg_proc = AppAdapter::app_config()->get("background_processing") == "1";
    return (m_plater != nullptr) ? !m_plater->model().objects.empty() && !bg_proc : false;
#else
    return (m_plater != nullptr) ? !m_plater->model().objects.empty() : false;
#endif
}

bool MainPanel::can_change_view() const
{
    int page_id = m_tabpanel->GetSelection();
    return page_id != wxNOT_FOUND && dynamic_cast<const Slic3r::GUI::Plater*>(m_tabpanel->GetPage((size_t)page_id)) != nullptr;
}

bool MainPanel::can_clone() const {
    return can_select() && !m_plater->is_selection_empty();
}

bool MainPanel::can_select() const
{
    return (m_plater != nullptr) && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor) && !m_plater->model().objects.empty();
}

bool MainPanel::can_deselect() const
{
    return (m_plater != nullptr) && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor) && !m_plater->is_selection_empty();
}

bool MainPanel::can_delete() const
{
    return (m_plater != nullptr) && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor) && !m_plater->is_selection_empty();
}

bool MainPanel::can_delete_all() const
{
    return (m_plater != nullptr) && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor) && !m_plater->model().objects.empty();
}

bool MainPanel::can_reslice() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

wxBoxSizer* MainPanel::create_side_tools()
{
    int em = AppAdapter::em_unit();
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

    m_slice_select = eSlicePlate;
    m_print_select = ePrintPlate;

    // m_publish_btn = new Button(this, _L("Upload"), "bar_publish", 0, FromDIP(16));
    m_slice_btn = new SideButton(this, _L("Slice plate"), "");
    m_slice_option_btn = new SideButton(this, "", "sidebutton_dropdown", 0, FromDIP(14));
    m_print_btn = new SideButton(this, _L("Print plate"), "");
    m_print_option_btn = new SideButton(this, "", "sidebutton_dropdown", 0, FromDIP(14));

    update_side_button_style();
    // m_publish_btn->Hide();
    m_slice_option_btn->Enable();
    m_print_option_btn->Enable();
    // sizer->Add(m_publish_btn, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    // sizer->Add(FromDIP(15), 0, 0, 0, 0);
    sizer->Add(m_slice_option_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    sizer->Add(m_slice_btn, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    sizer->Add(FromDIP(15), 0, 0, 0, 0);
    sizer->Add(m_print_option_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    sizer->Add(m_print_btn, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    sizer->Add(FromDIP(19), 0, 0, 0, 0);

    sizer->Layout();

    m_slice_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
        {
            m_plater->exit_gizmo();
            m_plater->update(true, true);
            if (m_slice_select == eSliceAll)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SLICE_ALL));
            else
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SLICE_PLATE));

            this->m_tabpanel->SetSelection(tpPreview);
        });

    m_print_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
        {
            if (m_print_select == ePrintAll || m_print_select == ePrintPlate || m_print_select == ePrintMultiMachine)
            {
                m_plater->apply_background_progress();
                // check valid of print
                m_print_enable = get_enable_print_status();
                m_print_btn->Enable(m_print_enable);
                if (m_print_enable) {
                    if (m_print_select == ePrintAll)
                        wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_ALL));
                    if (m_print_select == ePrintPlate)
                        wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_PLATE));
                    if(m_print_select == ePrintMultiMachine)
                         wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_MULTI_MACHINE));
                }
            }
            else if (m_print_select == eExportGcode)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_GCODE));
            else if (m_print_select == eSendGcode)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SEND_GCODE));
            else if (m_print_select == eUploadGcode)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_UPLOAD_GCODE));
            else if (m_print_select == eExportSlicedFile)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_SLICED_FILE));
            else if (m_print_select == eExportAllSlicedFile)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_ALL_SLICED_FILE));
            else if (m_print_select == eSendToPrinter)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SEND_TO_PRINTER));
            else if (m_print_select == eSendToPrinterAll)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SEND_TO_PRINTER_ALL));
            /* else if (m_print_select == ePrintMultiMachine)
                 wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_MULTI_MACHINE));*/
        });

    m_slice_option_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
        {
            SidePopup* p = new SidePopup(this);
            SideButton* slice_all_btn = new SideButton(p, _L("Slice all"), "");
            slice_all_btn->SetCornerRadius(0);
            SideButton* slice_plate_btn = new SideButton(p, _L("Slice plate"), "");
            slice_plate_btn->SetCornerRadius(0);

            slice_all_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                m_slice_btn->SetLabel(_L("Slice all"));
                m_slice_select = eSliceAll;
                m_slice_enable = get_enable_slice_status();
                m_slice_btn->Enable(m_slice_enable);
                this->Layout();
                p->Dismiss();
                });

            slice_plate_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                m_slice_btn->SetLabel(_L("Slice plate"));
                m_slice_select = eSlicePlate;
                m_slice_enable = get_enable_slice_status();
                m_slice_btn->Enable(m_slice_enable);
                this->Layout();
                p->Dismiss();
                });
            p->append_button(slice_all_btn);
            p->append_button(slice_plate_btn);
            p->Popup(m_slice_btn);
        }
    );

    m_print_option_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
        {
            SidePopup* p = new SidePopup(this);

            {
                // ThirdParty Buttons
                SideButton* export_gcode_btn = new SideButton(p, _L("Export G-code file"), "");
                export_gcode_btn->SetCornerRadius(0);
                export_gcode_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Export G-code file"));
                    m_print_select = eExportGcode;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                SideButton* export_sliced_file_btn = new SideButton(p, _L("Export plate sliced file"), "");
                export_sliced_file_btn->SetCornerRadius(0);
                export_sliced_file_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Export plate sliced file"));
                    m_print_select = eExportSlicedFile;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });


                SideButton* export_all_sliced_file_btn = new SideButton(p, _L("Export all sliced file"), "");
                export_all_sliced_file_btn->SetCornerRadius(0);
                export_all_sliced_file_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Export all sliced file"));
                    m_print_select = eExportAllSlicedFile;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                SideButton* send_print_btn = new SideButton(p, _L("Send GCode"), "");
                send_print_btn->SetCornerRadius(0);
                send_print_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Send GCode"));
                    m_print_select = eSendGcode;
                    m_print_enable = true;
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                p->append_button(export_gcode_btn);
                p->append_button(export_sliced_file_btn);
                p->append_button(export_all_sliced_file_btn);
                p->append_button(send_print_btn);
            }

            p->Popup(m_print_btn);
        }
    );

    sizer->Add(FromDIP(19), 0, 0, 0, 0);

    return sizer;
}

bool MainPanel::get_enable_slice_status()
{
    bool enable = true;

    bool on_slicing = m_plater->is_background_process_slicing();
    if (on_slicing) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": on slicing, return false directly!");
        return false;
    }
    else if  ( m_plater->using_exported_file()) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": in gcode/exported 3mf mode, return false directly!");
        return false;
    }

    PartPlateList &part_plate_list = m_plater->get_partplate_list();
    PartPlate *current_plate = part_plate_list.get_curr_plate();

    if (m_slice_select == eSliceAll)
    {
        /*if (part_plate_list.is_all_slice_results_valid())
        {
            enable = false;
        }
        else if (!part_plate_list.is_all_plates_ready_for_slice())
        {
            enable = false;
        }*/
        //always enable slice_all button
        enable = true;
    }
    else if (m_slice_select == eSlicePlate)
    {
        if (current_plate->is_slice_result_valid())
        {
            enable = false;
        }
        else if (!current_plate->can_slice())
        {
            enable = false;
        }
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": m_slice_select %1%, enable= %2% ")%m_slice_select %enable;
    return enable;
}

bool MainPanel::get_enable_print_status()
{
    bool enable = true;

    PartPlateList &part_plate_list = m_plater->get_partplate_list();
    PartPlate *current_plate = part_plate_list.get_curr_plate();
    bool is_all_plates = AppAdapter::plater()->get_preview_canvas3D()->is_all_plates_selected();
    if (m_print_select == ePrintAll)
    {
        if (!part_plate_list.is_all_slice_results_ready_for_print())
        {
            enable = false;
        }
    }
    else if (m_print_select == ePrintPlate)
    {
        if (!current_plate->is_slice_result_ready_for_print())
        {
            enable = false;
        }
        enable = enable && !is_all_plates;
    }
    else if (m_print_select == eExportGcode)
    {
        if (!current_plate->is_slice_result_valid())
        {
            enable = false;
        }
        enable = enable && !is_all_plates;
    }
    else if (m_print_select == eSendGcode)
    {
        if (!current_plate->is_slice_result_valid())
            enable = false;
        if (!can_send_gcode())
            enable = false;
        enable = enable && !is_all_plates;
    }
    else if (m_print_select == eUploadGcode)
    {
        if (!current_plate->is_slice_result_valid())
            enable = false;
        if (!can_send_gcode())
            enable = false;
        enable = enable && !is_all_plates;
    }
    else if (m_print_select == eExportSlicedFile)
    {
        if (!current_plate->is_slice_result_ready_for_export())
        {
            enable = false;
        }
        enable = enable && !is_all_plates;
	}
	else if (m_print_select == eSendToPrinter)
	{
		if (!current_plate->is_slice_result_ready_for_print())
		{
			enable = false;
		}
        enable = enable && !is_all_plates;
	}
    else if (m_print_select == eSendToPrinterAll)
    {
        if (!part_plate_list.is_all_slice_results_ready_for_print())
        {
            enable = false;
        }
    }
    else if (m_print_select == eExportAllSlicedFile)
    {
        if (!part_plate_list.is_all_slice_result_ready_for_export())
        {
            enable = false;
        }
    }
    else if (m_print_select == ePrintMultiMachine)
    {
        if (!current_plate->is_slice_result_ready_for_print())
        {
            enable = false;
        }
        enable = enable && !is_all_plates;
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": m_print_select %1%, enable= %2% ")%m_print_select %enable;

    return enable;
}

void MainPanel::update_side_button_style()
{
    // BBS
    int em = AppAdapter::em_unit();

    /*m_slice_btn->SetLayoutStyle(1);
    m_slice_btn->SetTextLayout(SideButton::EHorizontalOrientation::HO_Center, FromDIP(15));
    m_slice_btn->SetMinSize(wxSize(-1, FromDIP(24)));
    m_slice_btn->SetCornerRadius(FromDIP(12));
    m_slice_btn->SetExtraSize(wxSize(FromDIP(38), FromDIP(10)));
    m_slice_btn->SetBottomColour(wxColour(0x3B4446));*/
    StateColor m_btn_bg_enable = StateColor(
        std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed), 
        std::pair<wxColour, int>(wxColour(48, 221, 112), StateColor::Hovered),
        std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal)
    );

    // m_publish_btn->SetMinSize(wxSize(FromDIP(125), FromDIP(24)));
    // m_publish_btn->SetCornerRadius(FromDIP(12));
    // m_publish_btn->SetBackgroundColor(m_btn_bg_enable);
    // m_publish_btn->SetBorderColor(m_btn_bg_enable);
    // m_publish_btn->SetBackgroundColour(wxColour(59,68,70));
    // m_publish_btn->SetTextColor(StateColor::darkModeColorFor("#FFFFFE"));

    m_slice_btn->SetTextLayout(SideButton::EHorizontalOrientation::HO_Left, FromDIP(15));
    m_slice_btn->SetCornerRadius(FromDIP(12));
    m_slice_btn->SetExtraSize(wxSize(FromDIP(38), FromDIP(10)));
    m_slice_btn->SetMinSize(wxSize(-1, FromDIP(24)));

    m_slice_option_btn->SetTextLayout(SideButton::EHorizontalOrientation::HO_Center);
    m_slice_option_btn->SetCornerRadius(FromDIP(12));
    m_slice_option_btn->SetExtraSize(wxSize(FromDIP(10), FromDIP(10)));
    m_slice_option_btn->SetIconOffset(FromDIP(2));
    m_slice_option_btn->SetMinSize(wxSize(FromDIP(24), FromDIP(24)));

    m_print_btn->SetTextLayout(SideButton::EHorizontalOrientation::HO_Left, FromDIP(15));
    m_print_btn->SetCornerRadius(FromDIP(12));
    m_print_btn->SetExtraSize(wxSize(FromDIP(38), FromDIP(10)));
    m_print_btn->SetMinSize(wxSize(-1, FromDIP(24)));

    m_print_option_btn->SetTextLayout(SideButton::EHorizontalOrientation::HO_Center);
    m_print_option_btn->SetCornerRadius(FromDIP(12));
    m_print_option_btn->SetExtraSize(wxSize(FromDIP(10), FromDIP(10)));
    m_print_option_btn->SetIconOffset(FromDIP(2));
    m_print_option_btn->SetMinSize(wxSize(FromDIP(24), FromDIP(24)));
}

void MainPanel::update_slice_print_status(SlicePrintEventType event, bool can_slice, bool can_print)
{
    bool enable_print = true, enable_slice = true;

    if (!can_slice)
    {
        if (m_slice_select == eSlicePlate)
            enable_slice = false;
    }
    if (!can_print)
        enable_print = false;


    //process print logic
    if (enable_print)
    {
        enable_print = get_enable_print_status();
    }

    //process slice logic
    if (enable_slice)
    {
        enable_slice = get_enable_slice_status();
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" m_slice_select %1%: can_slice= %2%, can_print %3%, enable_slice %4%, enable_print %5% ")%m_slice_select % can_slice %can_print %enable_slice %enable_print;
    m_print_btn->Enable(enable_print);
    m_slice_btn->Enable(enable_slice);
    m_slice_enable = enable_slice;
    m_print_enable = enable_print;

    if (AppAdapter::main_panel())
        AppAdapter::plater()->update_title_dirty_status();
}


void MainPanel::on_dpi_changed(const wxRect& suggested_rect)
{

    update_font_from_mainframe();

    this->SetFont(AppAdapter::normal_font());

#ifdef _MSW_DARK_MODE
    dynamic_cast<Notebook*>(m_tabpanel)->Rescale();
#endif

    m_tabpanel->Rescale();

    update_side_button_style();

    m_slice_btn->Rescale();
    m_print_btn->Rescale();
    m_slice_option_btn->Rescale();
    m_print_option_btn->Rescale();

    // update Plater
    AppAdapter::plater()->msw_rescale();

    m_param_panel->msw_rescale();
}

void MainPanel::on_sys_color_changed()
{
    wxBusyCursor wait;

    // update label colors in respect to the system mode
    init_label_colours();

#ifndef __WINDOWS__
    AppAdapter::gui_app()->force_colors_update();
    AppAdapter::gui_app()->update_ui_from_settings();
#endif //__APPLE__

#ifdef __WXMSW__
    UpdateDarkUI(m_tabpanel);
 //   m_statusbar->update_dark_ui();
#ifdef _MSW_DARK_MODE
    // update common mode sizer
    dynamic_cast<Notebook*>(m_tabpanel)->Rescale();
#endif
#endif

    // BBS
    m_tabpanel->Rescale();
    m_param_panel->msw_rescale();

    // update Plater
    AppAdapter::plater()->sys_color_changed();
    // update Tabs
    for (auto tab : AppAdapter::gui_app()->tabs_list)
        tab->sys_color_changed();
    for (auto tab : AppAdapter::gui_app()->model_tabs_list)
        tab->sys_color_changed();
    AppAdapter::gui_app()->plate_tab->sys_color_changed();

    WebView::RecreateAll();

    this->Refresh();
}

void MainPanel::set_max_recent_count(int max)
{
    max = max < 0 ? 0 : max > 10000 ? 10000 : max;
    size_t count = m_recent_projects.GetCount();
    m_recent_projects.SetMaxFiles(max);
    if (count != m_recent_projects.GetCount()) {
        count = m_recent_projects.GetCount();
        std::vector<std::string> recent_projects;
        for (size_t i = 0; i < count; ++i) {
            recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
        }
        AppAdapter::app_config()->set_recent_projects(recent_projects);
        AppAdapter::app_config()->save();
    }
}

struct ConfigsOverwriteConfirmDialog : MessageDialog
{
    ConfigsOverwriteConfirmDialog(wxWindow *parent, wxString name, bool exported)
        : MessageDialog(parent,
                        wxString::Format(exported ? _L("A file exists with the same name: %s, do you want to overwrite it?") :
                                                  _L("A config exists with the same name: %s, do you want to overwrite it?"),
                                         name),
                        exported ? _L("Overwrite file") : _L("Overwrite config"),
                        wxYES_NO | wxNO_DEFAULT)
    {
        add_button(wxID_YESTOALL, false, _L("Yes to All"));
        add_button(wxID_NOTOALL, false, _L("No to All"));
    }
};

void MainPanel::export_config()
{
    ExportConfigsDialog export_configs_dlg(nullptr);
    export_configs_dlg.ShowModal();
    return; 

    // Generate a cummulative configuration for the selected print, filaments and printer.
    wxDirDialog dlg(this, _L("Choose a directory"),
        from_u8(!m_last_config.IsEmpty() ? get_dir_name(m_last_config) : AppAdapter::app_config()->get_last_dir()), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    wxString path;
    if (dlg.ShowModal() == wxID_OK)
        path = dlg.GetPath();
    if (!path.IsEmpty()) {
        // Export the config bundle.
        AppAdapter::app_config()->update_config_dir(into_u8(path));
        try {
            auto files = app_preset_bundle()->export_current_configs(into_u8(path), [this](std::string const & name) {
                    ConfigsOverwriteConfirmDialog dlg(this, from_u8(name), true);
                    int res = dlg.ShowModal();
                    int ids[]{wxID_NO, wxID_YES, wxID_NOTOALL, wxID_YESTOALL};
                    return std::find(ids, ids + 4, res) - ids;
            }, false);
            if (!files.empty())
                m_last_config = from_u8(files.back());
            MessageDialog dlg(this, wxString::Format(_L_PLURAL("There is %d config exported. (Only non-system configs)",
                "There are %d configs exported. (Only non-system configs)", files.size()), files.size()),
                              _L("Export result"), wxOK);
            dlg.ShowModal();
        } catch (const std::exception &ex) {
            show_error(this, ex.what());
        }
    }
}

// Load a config file containing a Print, Filament & Printer preset.
void MainPanel::load_config_file()
{
    wxFileDialog dlg(this, _L("Select profile to load:"),
        !m_last_config.IsEmpty() ? get_dir_name(m_last_config) : AppAdapter::app_config()->get_last_dir(),
        "config.json", "Config files (*.json;*.zip;*.orca_printer;*.orca_filament)|*.json;*.zip;*.orca_printer;*.orca_filament", wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST);
     wxArrayString files;
    if (dlg.ShowModal() != wxID_OK)
        return;
    dlg.GetPaths(files);
    std::vector<std::string> cfiles;
    for (auto file : files) {
        cfiles.push_back(into_u8(file));
        m_last_config = file;
    }
    bool update = false;
    app_preset_bundle()->import_presets(cfiles, [this](std::string const & name) {
            ConfigsOverwriteConfirmDialog dlg(this, from_u8(name), false);
            int           res = dlg.ShowModal();
            int           ids[]{wxID_NO, wxID_YES, wxID_NOTOALL, wxID_YESTOALL};
            return std::find(ids, ids + 4, res) - ids;
        },
        ForwardCompatibilitySubstitutionRule::Enable);
    if (!cfiles.empty()) {
        AppAdapter::app_config()->update_config_dir(get_dir_name(cfiles.back()));
        AppAdapter::gui_app()->load_current_presets();
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " presets has been import,and size is" << cfiles.size();
    }
    app_preset_bundle()->update_compatible(PresetSelectCompatibleType::Always);
    update_side_preset_ui();
    auto msg = wxString::Format(_L_PLURAL("There is %d config imported. (Only non-system and compatible configs)",
        "There are %d configs imported. (Only non-system and compatible configs)", cfiles.size()), cfiles.size());
    if(cfiles.empty())
        msg += _L("\nHint: Make sure you have added the corresponding printer before importing the configs.");
    MessageDialog dlg2(this,msg ,
                        _L("Import result"), wxOK);
    dlg2.ShowModal();
}

// Load a config file containing a Print, Filament & Printer preset from command line.
bool MainPanel::load_config_file(const std::string &path)
{
    try {
        ConfigSubstitutions config_substitutions = app_preset_bundle()->load_config_file(path, ForwardCompatibilitySubstitutionRule::Enable);
        if (!config_substitutions.empty())
            show_substitutions_info(config_substitutions, path);
    } catch (const std::exception &ex) {
        show_error(this, ex.what());
        return false;
    }
    AppAdapter::gui_app()->load_current_presets();
    return true;
}

// Load a provied DynamicConfig into the Print / Filament / Printer tabs, thus modifying the active preset.
// Also update the plater with the new presets.
void MainPanel::load_config(const DynamicPrintConfig& config)
{
	PrinterTechnology printer_technology = app_preset_bundle()->printers.get_edited_preset().printer_technology();
	const auto       *opt_printer_technology = config.option<ConfigOptionEnum<PrinterTechnology>>("printer_technology");
	if (opt_printer_technology != nullptr && opt_printer_technology->value != printer_technology) {
		printer_technology = opt_printer_technology->value;
		this->plater()->set_printer_technology(printer_technology);
	}

    for (auto tab : AppAdapter::gui_app()->tabs_list)
        if (tab->supports_printer_technology(printer_technology)) {
            // Only apply keys, which are present in the tab's config. Ignore the other keys.
			for (const std::string &opt_key : tab->get_config()->diff(config))
				// Ignore print_settings_id, printer_settings_id, filament_settings_id etc.
				if (! boost::algorithm::ends_with(opt_key, "_settings_id"))
					tab->get_config()->option(opt_key)->set(config.option(opt_key));
        }

    AppAdapter::gui_app()->load_current_presets();
}

//BBS: GUI refactor
void MainPanel::select_tab(wxPanel* panel)
{
    if (!panel)
        return;
    if (panel == m_param_panel) {
        panel = m_plater;
    } else if (dynamic_cast<ParamsPanel*>(panel)) {
        AppAdapter::gui_app()->params_dialog()->Popup();
        return;
    }
    int page_idx = m_tabpanel->FindPage(panel);
    if (page_idx == tp3DEditor && m_tabpanel->GetSelection() == tpPreview)
        return;

    select_tab(size_t(page_idx));
}

//BBS GUI refactor: remove unused layout new/dlg
void MainPanel::select_tab(size_t tab/* = size_t(-1)*/)
{
    //bool tabpanel_was_hidden = false;

    // Controls on page are created on active page of active tab now.
    // We should select/activate tab before its showing to avoid an UI-flickering
    auto select = [this, tab](bool was_hidden) {
        size_t new_selection = tab == (size_t)(-1) ? m_last_selected_tab : tab;

        if (m_tabpanel->GetSelection() != (int)new_selection)
            m_tabpanel->SetSelection(new_selection);

        if (tab == MainPanel::tp3DEditor && m_layout == ESettingsLayout::Old)
            m_plater->canvas3D()->render();
        else if (was_hidden) {
            Tab* cur_tab = dynamic_cast<Tab*>(m_tabpanel->GetPage(new_selection));
            if (cur_tab)
                cur_tab->OnActivate();
        }
    };

    select(false);
}

void MainPanel::request_select_tab(TabPosition pos)
{
    wxCommandEvent* evt = new wxCommandEvent(EVT_SELECT_TAB);
    evt->SetInt(pos);
    wxQueueEvent(this, evt);
}

// Set a camera direction, zoom to all objects.
void MainPanel::select_view(const std::string& direction)
{
     if (m_plater)
         m_plater->select_view(direction);
}

// #ys_FIXME_to_delete
void MainPanel::on_presets_changed(SimpleEvent &event)
{
    auto *tab = dynamic_cast<Tab*>(event.GetEventObject());
    wxASSERT(tab != nullptr);
    if (tab == nullptr) {
        return;
    }

    // Update preset combo boxes(Print settings, Filament, Material, Printer) from their respective tabs.
    auto presets = tab->get_presets();
    if (m_plater != nullptr && presets != nullptr) {

        // FIXME: The preset type really should be a property of Tab instead
        Slic3r::Preset::Type preset_type = tab->type();
        if (preset_type == Slic3r::Preset::TYPE_INVALID) {
            wxASSERT(false);
            return;
        }

        m_plater->on_config_change(*tab->get_config());

        m_plater->sidebar().update_presets(preset_type);
    }
}

// #ys_FIXME_to_delete
void MainPanel::on_value_changed(wxCommandEvent& event)
{
    auto *tab = dynamic_cast<Tab*>(event.GetEventObject());
    wxASSERT(tab != nullptr);
    if (tab == nullptr)
        return;

    auto opt_key = event.GetString();
    if (m_plater) {
        m_plater->on_config_change(*tab->get_config()); // propagate config change events to the plater
        if (opt_key == "extruders_count") {
            auto value = event.GetInt();
            m_plater->on_filaments_change(value);
        }
    }
}

void MainPanel::on_config_changed(DynamicPrintConfig* config) const
{
    if (m_plater)
        m_plater->on_config_change(*config); // propagate config change events to the plater
}

void MainPanel::set_print_button_to_default(PrintSelectType select_type)
{
    if (select_type == PrintSelectType::ePrintPlate) {
        m_print_btn->SetLabel(_L("Print plate"));
        m_print_select = ePrintPlate;
        if (m_print_enable)
            m_print_enable = get_enable_print_status();
        m_print_btn->Enable(m_print_enable);
        this->Layout();
    } else if (select_type == PrintSelectType::eSendGcode) {
        m_print_btn->SetLabel(_L("Print"));
        m_print_select = eSendGcode;
        if (m_print_enable)
            m_print_enable = get_enable_print_status() && can_send_gcode();
        m_print_btn->Enable(m_print_enable);
        this->Layout();
    } else if (select_type == PrintSelectType::eExportGcode) {
        m_print_btn->SetLabel(_L("Export G-code file"));
        m_print_select = eExportGcode;
        if (m_print_enable)
            m_print_enable = get_enable_print_status() && can_send_gcode();
        m_print_btn->Enable(m_print_enable);
        this->Layout();
    } else {
        // unsupport
        return;
    }
}

void MainPanel::add_to_recent_projects(const wxString& filename)
{
    if (wxFileExists(filename))
    {
        m_recent_projects.AddFileToHistory(filename);
        std::vector<std::string> recent_projects;
        size_t count = m_recent_projects.GetCount();
        for (size_t i = 0; i < count; ++i)
        {
            recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
        }
        AppAdapter::app_config()->set_recent_projects(recent_projects);
    }
}

std::wstring MainPanel::FileHistory::GetThumbnailUrl(int index) const
{
    if (m_thumbnails[index].empty()) return L"";
    std::wstringstream wss;
    wss << L"data:image/png;base64,";
    wss << wxBase64Encode(m_thumbnails[index].data(), m_thumbnails[index].size());
    return wss.str();
}

void MainPanel::FileHistory::AddFileToHistory(const wxString &file)
{
    if (this->m_fileMaxFiles == 0)
        return;
    wxFileHistory::AddFileToHistory(file);
    if (m_load_called)
        m_thumbnails.push_front(bbs_3mf_get_thumbnail(into_u8(file).c_str()));
    else
        m_thumbnails.push_front("");
}

void MainPanel::FileHistory::RemoveFileFromHistory(size_t i)
{
    if (i >= m_thumbnails.size()) // FIX zero max
        return;
    wxFileHistory::RemoveFileFromHistory(i);
    m_thumbnails.erase(m_thumbnails.begin() + i);
}

size_t MainPanel::FileHistory::FindFileInHistory(const wxString & file)
{
    return m_fileHistory.Index(file);
}

void MainPanel::FileHistory::LoadThumbnails()
{
    tbb::parallel_for(tbb::blocked_range<size_t>(0, GetCount()), [this](tbb::blocked_range<size_t> range) {
        for (size_t i = range.begin(); i < range.end(); ++i) {
            auto thumbnail = bbs_3mf_get_thumbnail(into_u8(GetHistoryFile(i)).c_str());
            if (!thumbnail.empty()) {
                m_thumbnails[i] = thumbnail;
            }
        }
    });
    m_load_called = true;
}

inline void MainPanel::FileHistory::SetMaxFiles(int max)
{
    m_fileMaxFiles  = max;
    size_t numFiles = m_fileHistory.size();
    while (numFiles > m_fileMaxFiles)
        RemoveFileFromHistory(--numFiles);
}

void MainPanel::get_recent_projects(boost::property_tree::wptree &tree, int images)
{
    for (size_t i = 0; i < m_recent_projects.GetCount(); ++i) {
        boost::property_tree::wptree item;
        std::wstring proj = m_recent_projects.GetHistoryFile(i).ToStdWstring();
        item.put(L"project_name", proj.substr(proj.find_last_of(L"/\\") + 1));
        item.put(L"path", proj);
        boost::system::error_code ec;
        std::time_t t = boost::filesystem::last_write_time(proj, ec);
        if (!ec) {
            std::wstring time = wxDateTime(t).FormatISOCombined(' ').ToStdWstring();
            item.put(L"time", time);
            if (i <= images) {
                auto thumbnail = m_recent_projects.GetThumbnailUrl(i);
                if (!thumbnail.empty()) item.put(L"image", thumbnail);
            }
        } else {
            item.put(L"time", _L("File is missing"));
        }
        tree.push_back({L"", item});
    }
}

void MainPanel::open_recent_project(size_t file_id, wxString const & filename)
{
    if (file_id == size_t(-1)) {
        file_id = m_recent_projects.FindFileInHistory(filename);
    }
    if (wxFileExists(filename)) {
        CallAfter([this, filename] {
            m_plater->load_project(filename);
        });
    }
    else
    {
        MessageDialog msg(this, _L("The project is no longer available."), _L("Error"), wxOK | wxYES_DEFAULT);
        auto result = msg.ShowModal();
        if (result == wxID_YES || result == wxID_OK)
        {
            m_recent_projects.RemoveFileFromHistory(file_id);
            std::vector<std::string> recent_projects;
            size_t count = m_recent_projects.GetCount();
            for (size_t i = 0; i < count; ++i)
            {
                recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
            }
            AppAdapter::app_config()->set_recent_projects(recent_projects);
        }
    }
}

void MainPanel::remove_recent_project(size_t file_id, wxString const &filename)
{
    if (file_id == size_t(-1)) {
        if (filename.IsEmpty())
            while (m_recent_projects.GetCount() > 0)
                m_recent_projects.RemoveFileFromHistory(0);
        else
            file_id = m_recent_projects.FindFileInHistory(filename);
    }
    if (file_id != size_t(-1))
        m_recent_projects.RemoveFileFromHistory(file_id);
    std::vector<std::string> recent_projects;
    size_t count = m_recent_projects.GetCount();
    for (size_t i = 0; i < count; ++i)
    {
        recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
    }
    AppAdapter::app_config()->set_recent_projects(recent_projects);
}

//
// Called after the Preferences dialog is closed and the program settings are saved.
// Update the UI based on the current preferences.
void MainPanel::update_ui_from_settings()
{
    if (m_plater)
        m_plater->update_ui_from_settings();
    for (auto tab: AppAdapter::gui_app()->tabs_list)
        tab->update_ui_from_settings();
}

void MainPanel::update_side_preset_ui()
{
    // select last preset
    for (auto tab : AppAdapter::gui_app()->tabs_list) {
        tab->update_tab_ui();
    }

    //BBS: update the preset
    m_plater->sidebar().update_presets(Preset::TYPE_PRINTER);
    m_plater->sidebar().update_presets(Preset::TYPE_FILAMENT);
}

std::string MainPanel::get_base_name(const wxString &full_name, const char *extension) const
{
    boost::filesystem::path filename = boost::filesystem::path(full_name.wx_str()).filename();
    if (extension != nullptr)
		filename = filename.replace_extension(extension);
    return filename.string();
}

std::string MainPanel::get_dir_name(const wxString &full_name) const
{
    return boost::filesystem::path(into_u8(full_name)).parent_path().string();
}

void MainPanel::update_font_from_mainframe()
{
    /* Only normal and bold fonts are used for an application rescale,
     * because of under MSW small and normal fonts are the same.
     * To avoid same rescaling twice, just fill this values
     * from rescaled MainPanel
     */

     update_app_font(AppAdapter::em_unit());
}


// ----------------------------------------------------------------------------
// SettingsDialog
// ----------------------------------------------------------------------------

SettingsDialog::SettingsDialog()
:DPIDialog(NULL, wxID_ANY, wxString(SLIC3R_APP_NAME) + " - " + _L("Settings"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, "settings_dialog")
{
#if defined(__WXMSW__)
    // ys_FIXME! temporary workaround for correct font scaling
    // Because of from wxWidgets 3.1.3 auto rescaling is implemented for the Fonts,
    // From the very beginning set dialog font to the wxSYS_DEFAULT_GUI_FONT
    this->SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
#else
    this->SetFont(app_normal_font());
    this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif // __WXMSW__

    // Load the icon either from the exe, or from the ico file.
#if _WIN32
    {
        TCHAR szExeFileName[MAX_PATH];
        GetModuleFileName(nullptr, szExeFileName, MAX_PATH);
        SetIcon(wxIcon(szExeFileName, wxBITMAP_TYPE_ICO));
    }
#else
    SetIcon(wxIcon(var("OrcaSlicer_128px.png"), wxBITMAP_TYPE_PNG));
#endif // _WIN32

    //just hide the Frame on closing
    this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& evt) { this->Hide(); });

    // initialize layout
    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->SetSizeHints(this);
    SetSizer(sizer);
    Fit();

    const wxSize min_size = wxSize(85 * em_unit(), 50 * em_unit());
#ifdef __APPLE__
    // Using SetMinSize() on Mac messes up the window position in some cases
    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
    SetSize(min_size);
#else
    SetMinSize(min_size);
    SetSize(GetMinSize());
#endif
    Layout();
}

void SettingsDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    const int& em = em_unit();
    const wxSize& size = wxSize(85 * em, 50 * em);

    // BBS
    m_tabpanel->Rescale();

    // update Tabs
    for (auto tab : AppAdapter::gui_app()->tabs_list)
        tab->msw_rescale();

    SetMinSize(size);
    Fit();
    Refresh();
}


} // GUI
} // Slic3r

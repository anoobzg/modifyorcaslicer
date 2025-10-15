#include "AppAdapter.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Config/ThemeDef.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"
#include "libslic3r/FileSystem/Log.hpp"

#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Utils/AssociationFiles.hpp"
#include "slic3r/Global/AppI18N.hpp"
#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/Theme/AppColor.hpp"
#include "slic3r/Utils/Str.hpp"
#include "slic3r/Global/InstanceCheck.hpp"

// Localization headers: include libslic3r version first so everything in this file
// uses the slic3r/GUI version (the macros will take precedence over the functions).
// Also, there is a check that the former is not included from slic3r module.
// This is the only place where we want to allow that, so define an override macro.
#define SLIC3R_ALLOW_LIBSLIC3R_I18N_IN_SLIC3R
#include "libslic3r/I18N.hpp"
#undef SLIC3R_ALLOW_LIBSLIC3R_I18N_IN_SLIC3R
#include "slic3r/GUI/I18N.hpp"

#include <wx/glcanvas.h>

#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/Utils/RemovableDriveManager.hpp"
#include "slic3r/Scene/NotificationManager.hpp"
#include "slic3r/Scene/HintNotification.hpp"
#include "slic3r/GUI/Widgets/WebView.hpp"
#include "slic3r/Config/PresetUpdater.hpp"
#include "slic3r/GUI/Frame/SplashScreen.hpp"

#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/GUI/Dialog/MsgDialog.hpp"
#include "slic3r/GUI/Config/GUI_ObjectList.hpp"
#include "slic3r/GUI/Tab.hpp"
#include "slic3r/GUI/Widgets/ProgressDialog.hpp"
#include "slic3r/GUI/Web/WebGuideDialog.hpp"
#include "slic3r/GUI/Dialog/ParamsDialog.hpp"
#include "slic3r/GUI/Frame/Notebook.hpp"
#include "slic3r/GUI/Dialog/Preferences.hpp"

#ifdef _WIN32
#include <boost/dll/runtime_symbol_info.hpp>
#endif

#ifdef WIN32
#include "slic3r/GUI/BaseException.h"
#endif

// Needed for forcing menu icons back under gtk2 and gtk3
#if defined(__WXGTK20__) || defined(__WXGTK3__)
    #include <gtk/gtk.h>
#endif


#define LOG_FILES_MAX_NUM           30

using namespace std::literals;

namespace Slic3r {
namespace GUI {

class MainFrame;


void GUI_App::post_init()
{
    assert(initialized());
    if (! this->initialized())
        throw Slic3r::RuntimeError("Calling post_init() while not yet initialized");

    bool switch_to_3d = false;
    bool slow_bootup = false;
    if (app_config->get("slow_bootup") == "true") {
        slow_bootup = true;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", slow bootup, won't render gl here.";
    }
    if (!switch_to_3d) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", begin load_gl_resources";
        main_panel->Freeze();
        plater_->canvas3D()->enable_render(false);
        main_panel->select_tab(size_t(MainPanel::tp3DEditor));
        plater_->select_view3d();
        //BBS init the opengl resource here
        if (plater_->canvas3D()->get_wxglcanvas()->IsShownOnScreen()&&plater_->canvas3D()->make_current_for_postinit()) {
            Size canvas_size = plater_->canvas3D()->get_canvas_size();
            global_im_gui().set_display_size(static_cast<float>(canvas_size.get_width()), static_cast<float>(canvas_size.get_height()));
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", start to init opengl";
            
            init_opengl();

            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", finished init opengl";
            plater_->canvas3D()->init();

            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", finished init canvas3D";
            global_im_gui().new_frame();

            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", finished init imgui frame";
            plater_->canvas3D()->enable_render(true);

            if (!slow_bootup) {
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", start to render a first frame for test";
                plater_->canvas3D()->render(false);
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", finished rendering a first frame for test";
            }
        }
        else {
            BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << "Found glcontext not ready, postpone the init";
        }

        main_panel->select_tab(size_t(0));
        if (app_config->get("default_page") == "1")
            main_panel->select_tab(size_t(1));
        main_panel->Thaw();
        plater_->trigger_restore_project(1);
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", end load_gl_resources";
    }

    if (app_config->get("show_hints") == "true") {
        plater_->get_notification_manager()->push_hint_notification(false);
    }

    // remove old log files over LOG_FILES_MAX_NUM
    std::string log_addr = user_data_dir();
    if (!log_addr.empty()) {
        auto log_folder = boost::filesystem::path(log_addr) / "log";
        if (boost::filesystem::exists(log_folder)) {
           std::vector<std::pair<time_t, std::string>> files_vec;
           for (auto& it : boost::filesystem::directory_iterator(log_folder)) {
               auto temp_path = it.path();
               try {
                   if (it.status().type() == boost::filesystem::regular_file) {
                       std::time_t lw_t = boost::filesystem::last_write_time(temp_path) ;
                       files_vec.push_back({ lw_t, temp_path.filename().string() });
                   }
               } catch (const std::exception &) {
               }
           }
           std::sort(files_vec.begin(), files_vec.end(), [](
               std::pair<time_t, std::string> &a, std::pair<time_t, std::string> &b) {
               return a.first > b.first;
           });

           while (files_vec.size() > LOG_FILES_MAX_NUM) {
               auto full_path = log_folder / boost::filesystem::path(files_vec[files_vec.size() - 1].second);
               BOOST_LOG_TRIVIAL(info) << "delete log file over " << LOG_FILES_MAX_NUM << ", filename: "<< files_vec[files_vec.size() - 1].second;
               try {
                   boost::filesystem::remove(full_path);
               }
               catch (const std::exception& ex) {
                   BOOST_LOG_TRIVIAL(error) << "failed to delete log file: "<< files_vec[files_vec.size() - 1].second << ". Error: " << ex.what();
               }
               files_vec.pop_back();
           }
        }
    }
    BOOST_LOG_TRIVIAL(info) << "finished post_init";

    if (!m_app_conf_exists || preset_bundle->printers.only_default_printers()) {
        BOOST_LOG_TRIVIAL(info) << "run wizard...";
        run_wizard(ConfigWizard::RR_DATA_EMPTY);
        BOOST_LOG_TRIVIAL(info) << "finished run wizard";
    } 

//BBS: remove the single instance currently
#ifdef _WIN32
    // Sets window property to main_panel so other instances can indentify it.
    OtherInstanceMessageHandler::init_windows_properties(main_panel->GetHandle());
#endif //WIN32
}

GUI_App::GUI_App()
{
    BOOST_LOG_TRIVIAL(info) << "GUI_App::GUI_App constructor";

	m_removable_drive_manager = std::make_unique<RemovableDriveManager>();
    m_downloader = std::make_unique<Downloader>();

    //app config initializes early becasuse it is used in instance checking in GUI_Init
    this->init_app_config();
}

void GUI_App::shutdown()
{
    BOOST_LOG_TRIVIAL(info) << "GUI_App::shutdown enter";

	if (m_removable_drive_manager) {
		removable_drive_manager()->shutdown();
	}

    if (m_is_recreating_gui) 
        return;

    BOOST_LOG_TRIVIAL(info) << "GUI_App::shutdown exit";
}

GUI_App::~GUI_App()
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< boost::format(": enter");
    if (app_config != nullptr) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< boost::format(": destroy app_config");
        delete app_config;
    }

    if (preset_bundle != nullptr) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< boost::format(": destroy preset_bundle");
        delete preset_bundle;
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< boost::format(": exit");
}

#if wxUSE_WEBVIEW_EDGE
void GUI_App::init_webview_runtime()
{
    // Check WebView Runtime
    if (!WebView::CheckWebViewRuntime()) {
        int nRet = wxMessageBox(_L("GUI requires the Microsoft WebView2 Runtime to operate certain features.\nClick Yes to install it now."),
                                _L("WebView2 Runtime"), wxYES_NO);
        if (nRet == wxYES) {
            WebView::DownloadAndInstallWebViewRuntime();
        }
    }
}
#endif

void GUI_App::init_app_config()
{
    //SetAppName(SLIC3R_APP_KEY);

    //BBS: remove GCodeViewer as seperate APP logic
	if (!app_config)
        app_config = new AppConfig();

    m_config_corrupted = false;
	// load settings
	m_app_conf_exists = app_config->exists();
	if (m_app_conf_exists) {
        std::string error = app_config->load();
        if (!error.empty()) {
            // Orca: if the config file is corrupted, we will show a error dialog and create a default config file.
            m_config_corrupted = true;

        }

    }
    else {
#ifdef _WIN32
        // update associate files from registry information
        if (is_associate_files(L"3mf")) {
            app_config->set("associate_3mf", "true");
        }
        if (is_associate_files(L"stl")) {
            app_config->set("associate_stl", "true");
        }
        if (is_associate_files(L"step") && is_associate_files(L"stp")) {
            app_config->set("associate_step", "true");
        }
#endif // _WIN32
    }

#if !_DEBUG
    set_logging_level(Slic3r::level_string_to_boost(app_config->get("log_severity_level")));
#endif
}

bool GUI_App::OnInit(wxFrame* frame)
{
    init_download_path();

#if wxUSE_WEBVIEW_EDGE
    this->init_webview_runtime();
#endif

    BOOST_LOG_TRIVIAL(info) << "GUI_App::GUI_App constructor end";
    flush_logs();

    try {
        return on_init_inner(frame);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(fatal) << "OnInit Got Fatal error: " << e.what();
        generic_exception_handle();
        return false;
    }
}

int GUI_App::OnExit()
{
    // Orca: clean up encrypted bbl network log file if plugin is used
    // No point to keep them as they are encrypted and can't be used for debugging
    try {
        auto              log_folder  = boost::filesystem::path(user_data_dir()) / "log";
        const std::string filePattern = R"(debug_network_.*\.log\.enc)";
        std::regex        pattern(filePattern);
        if (boost::filesystem::exists(log_folder)) {
            std::vector<boost::filesystem::path> network_logs;
            for (auto& it : boost::filesystem::directory_iterator(log_folder)) {
                auto temp_path = it.path();
                if (boost::filesystem::is_regular_file(temp_path) && std::regex_match(temp_path.filename().string(), pattern)) {
                    network_logs.push_back(temp_path.filename());
                }
            }
            for (auto f : network_logs) {
                boost::filesystem::remove(f);
            }
        }
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Failed to clean up encrypt bbl network log file";
    }

    //return wxApp::OnExit();
    return 1;
}

class wxBoostLog : public wxLog
{
    void DoLogText(const wxString &msg) override {

        BOOST_LOG_TRIVIAL(warning) << msg.ToUTF8().data();
    }
    ~wxBoostLog() override
    {
        // This is a hack. Prevent thread logs from going to wxGuiLog on app quit.
        auto t = wxLog::SetActiveTarget(this);
        wxLog::FlushActive();
        wxLog::SetActiveTarget(t);
    }
};

bool GUI_App::on_init_inner(wxFrame* frame)
{
    wxLog::SetActiveTarget(new wxBoostLog());
#if BBL_RELEASE_TO_PUBLIC
    wxLog::SetLogLevel(wxLOG_Message);
#endif

    init_app_font();

    // Set initialization of image handlers before any UI actions - See GH issue #7469
    wxInitAllImageHandlers();
    flush_logs();

#ifdef NDEBUG
    wxImage::SetDefaultLoadFlags(0); // ignore waring in release build
#endif

#if defined(_WIN32) && ! defined(_WIN64)
    // BBS: remove 32bit build prompt
    // Win32 32bit build.
#endif // _WIN64

    // Forcing back menu icons under gtk2 and gtk3. Solution is based on:
    // https://docs.gtk.org/gtk3/class.Settings.html
    // see also https://docs.wxwidgets.org/3.0/classwx_menu_item.html#a2b5d6bcb820b992b1e4709facbf6d4fb
    // TODO: Find workaround for GTK4
#if defined(__WXGTK20__) || defined(__WXGTK3__)
    g_object_set (gtk_settings_get_default (), "gtk-menu-images", TRUE, NULL);
#endif

#ifdef WIN32
    //BBS set crash log folder
    CBaseException::set_log_folder(user_data_dir());
#endif

    AppAdapter::app()->Bind(wxEVT_QUERY_END_SESSION, [this](auto & e) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< "received wxEVT_QUERY_END_SESSION";
        if (main_panel) {
            wxCloseEvent e2(wxEVT_CLOSE_WINDOW);
            e2.SetCanVeto(true);
            main_panel->GetEventHandler()->ProcessEvent(e2);
            if (e2.GetVeto()) {
                e.Veto();
                return;
            }
        }
        for (auto d : dialogStack)
            d->EndModal(wxID_ABORT);
    });

    // Verify resources path
    const wxString resources_dir = from_u8(Slic3r::resources_dir());
    wxCHECK_MSG(wxDirExists(resources_dir), false,
        wxString::Format("Resources path does not exist or is not a directory: %s", resources_dir));

#ifdef __linux__
    if (! check_old_linux_datadir(GetAppName())) {
        std::cerr << "Quitting, user chose to move their data to new location." << std::endl;
        return false;
    }
#endif

    BOOST_LOG_TRIVIAL(info) << boost::format("GUI mode, Current Version %1%")%LightMaker_VERSION;
    flush_logs();

    // If load_language() fails, the application closes.
    app_load_language(wxString(), true);

    init_imgui(into_u8(app_current_language_code()));

    Preset::update_suffix_modified((_L("*") + " ").ToUTF8().data());
    HintDatabase::get_instance().reinit();

    flush_logs();

    init_app_color();

    if(app_config->get("version") != SLIC3R_VERSION) {
        app_config->set("version", SLIC3R_VERSION);
    }

    SplashScreen * scrn = nullptr;
    if (app_config->get("show_splash_screen") == "true") {
        // make a bitmap with dark grey banner on the left side
        //BBS make BBL splash screen bitmap
        wxBitmap bmp = SplashScreen::MakeBitmap();
        // Detect position (display) to show the splash screen
        // Now this position is equal to the main_panel position
        wxPoint splashscreen_pos = wxDefaultPosition;
        if (app_config->has("window_mainframe")) {
            auto metrics = WindowMetrics::deserialize(app_config->get("window_mainframe"));
            if (metrics)
                splashscreen_pos = metrics->get_rect().GetPosition();
        }

        BOOST_LOG_TRIVIAL(info) << "begin to show the splash screen...";
        // //BBS use BBL splashScreen
        // scrn = new SplashScreen(bmp, main_panel, wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT, 1500, splashscreen_pos);
        // wxYield();
        // scrn->SetText(_L("Loading configuration")+ dots);
    }

    BOOST_LOG_TRIVIAL(info) << "loading systen presets...";
    preset_bundle = new PresetBundle();

    preset_updater = new PresetUpdater();
    // just checking for existence of Slic3r::data_dir is not enough : it may be an empty directory
    // supplied as argument to --datadir; in that case we should still run the wizard
    preset_bundle->setup_directories();

    if (true) {
#ifdef __WXMSW__
        if (app_config->get("associate_3mf") == "true")
            associate_files(L"3mf");
        if (app_config->get("associate_stl") == "true")
            associate_files(L"stl");
        if (app_config->get("associate_step") == "true") {
            associate_files(L"step");
            associate_files(L"stp");
        }
        associate_url(L"orcaslicer");

        if (app_config->get("associate_gcode") == "true")
            associate_files(L"gcode");
#endif // __WXMSW__

        AppAdapter::app()->Bind(EVT_ENTER_FORCE_UPGRADE, [this](const wxCommandEvent& evt) {
                wxString      version_str = wxString::FromUTF8(this->app_config->get("upgrade", "version"));
                wxString      description_text = wxString::FromUTF8(this->app_config->get("upgrade", "description"));
                std::string   download_url = this->app_config->get("upgrade", "url");
                wxString tips = wxString::Format(_L("Click to download new version in default browser: %s"), version_str);
                DownloadDialog dialog(this->main_panel,
                    tips,
                    _L("The Orca Slicer needs an upgrade"),
                    false,
                    wxCENTER | wxICON_INFORMATION);
                dialog.SetExtendedMessage(description_text);

                int result = dialog.ShowModal();
                switch (result)
                {
                 case wxID_YES:
                     wxLaunchDefaultBrowser(download_url);
                     break;
                 case wxID_NO:
                     AppAdapter::main_panel()->Close(true);
                     break;
                 default:
                     AppAdapter::main_panel()->Close(true);
                }
            });

        AppAdapter::app()->Bind(EVT_SHOW_NO_NEW_VERSION, [this](const wxCommandEvent& evt) {
            wxString msg = _L("This is the newest version.");
            InfoDialog dlg(nullptr, _L("Info"), msg);
            dlg.ShowModal();
        });

        AppAdapter::app()->Bind(EVT_SHOW_DIALOG, [this](const wxCommandEvent& evt) {
            wxString msg = evt.GetString();
            InfoDialog dlg(this->main_panel, _L("Info"), msg);
            dlg.Bind(wxEVT_DESTROY, [this](auto& e) {
            });
            dlg.ShowModal();
        });
    }
    else {
#ifdef __WXMSW__
        if (app_config->get("associate_gcode") == "true")
            associate_files(L"gcode");
#endif // __WXMSW__
    }

    // Suppress the '- default -' presets.
    preset_bundle->set_default_suppressed(true);

    preset_bundle->backup_user_folder();

    enable_user_preset_folder(false);

    try {
        // Enable all substitutions (in both user and system profiles), but log the substitutions in user profiles only.
        // If there are substitutions in system profiles, then a "reconfigure" event shall be triggered, which will force
        // installation of a compatible system preset, thus nullifying the system preset substitutions.
        // init_params->preset_substitutions = preset_bundle->load_presets(*app_config, ForwardCompatibilitySubstitutionRule::EnableSystemSilent);
        preset_bundle->load_presets(*app_config, ForwardCompatibilitySubstitutionRule::EnableSystemSilent);
    }
    catch (const std::exception& ex) {
        show_error(nullptr, ex.what());
    }

    //register_win32_event();

    // Let the libslic3r know the callback, which will translate messages on demand.
    Slic3r::I18N::set_translate_callback(libslic3r_translate_callback);

    BOOST_LOG_TRIVIAL(info) << "create the main window";
    main_panel = new MainPanel(frame);
    
    main_panel->init();
    // hide settings tabs after first Layout

    main_panel->select_tab(size_t(0));

    sidebar().obj_list()->init();

    plater_->init_notification_manager();

    load_current_presets();

    if (plater_ != nullptr) {
        plater_->reset_project_dirty_initial_presets();
        plater_->update_project_dirty_from_presets();
    }

    BOOST_LOG_TRIVIAL(info) << "main frame firstly shown";

    obj_list()->set_min_height();

    update_mode(); // update view mode after fix of the object_list size

#ifdef __APPLE__
   other_instance_message_handler()->bring_instance_forward();
#endif //__APPLE__

   AppAdapter::app()->Bind(wxEVT_IDLE, [this](wxIdleEvent& event)
    {
        if (! plater_)
            return;

        if (!m_post_initialized && !m_adding_script_handler) {
            m_post_initialized = true;
#ifdef WIN32
            this->main_panel->register_win32_callbacks();
#endif
            this->post_init();
        }

        if (m_post_initialized && app_config->dirty())
            app_config->save();

    });

    m_initialized = true;

    flush_logs();

    BOOST_LOG_TRIVIAL(info) << "finished the gui app init";
    if (m_config_corrupted) {
        m_config_corrupted = false;
        show_error(nullptr,
                   _u8L(
                       "The OrcaSlicer configuration file may be corrupted and cannot be parsed.\nOrcaSlicer has attempted to recreate the "
                       "configuration file.\nPlease note, application settings will be lost, but printer profiles will not be affected."));
    }
    return true;
}

void GUI_App::update_label_colours()
{
    for (Tab* tab : tabs_list)
        tab->update_label_colours();
}

void switch_window_pools();
void release_window_pools();

void GUI_App::recreate_GUI(const wxString &msg_name)
{
    // BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "recreate_GUI enter";
    // m_is_recreating_gui = true;

    // main_panel->shutdown();
    // ProgressDialog dlg(msg_name, msg_name, 100, nullptr, wxPD_AUTO_HIDE);
    // dlg.Pulse();
    // dlg.Update(10, _L("Rebuild") + dots);

    // wxPanel *old_main_panel = main_panel;
    // struct ClientData : wxClientData
    // {
    //     ~ClientData() { release_window_pools(); }
    // };
    // old_main_panel->SetClientObject(new ClientData);

    // switch_window_pools();
    // main_panel = new MainPanel(mainframe);
    // main_panel->init();
    // main_panel->select_tab(size_t(MainPanel::tp3DEditor));
    // // Propagate model objects to object list.
    // sidebar().obj_list()->init();
    // AppAdapter::app()->SetTopWindow(main_panel);

    // dlg.Update(30, _L("Rebuild") + dots);
    // old_main_panel->Destroy();

    // dlg.Update(80, _L("Loading current presets") + dots);
    // load_current_presets();
    // main_panel->Show(true);

    // dlg.Update(90, _L("Loading a mode view") + dots);

    // obj_list()->set_min_height();
    // update_mode();

    // //BBS: trigger restore project logic here, and skip confirm
    // plater_->trigger_restore_project(1);

    // m_is_recreating_gui = false;

    // BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "recreate_GUI exit";
}

void GUI_App::ShowUserGuide() {
    try {
        bool res = false;
        GuideFrame GuideDlg;
        res = GuideDlg.run();
        if (res) {
            load_current_presets();
        }
    } catch (std::exception &) {
    }
}

void GUI_App::ShowOnlyFilament() {
    try {
        bool       res = false;
        GuideFrame GuideDlg;
        GuideDlg.SetStartPage(GuideFrame::GuidePage::BBL_FILAMENT_ONLY);
        res = GuideDlg.run();
        if (res) {
            load_current_presets();
        }
    } catch (std::exception &) {
    }
}

// Called after the Preferences dialog is closed and the program settings are saved.
// Update the UI based on the current preferences.
void GUI_App::update_ui_from_settings() //anoob
{
    update_label_colours();
    force_update_ui_colors();

    if (main_panel) {main_panel->update_ui_from_settings();}
}

void GUI_App::enable_user_preset_folder(bool enable)
{
    BOOST_LOG_TRIVIAL(info) << "preset_folder: set to empty";
    app_config->set("preset_folder", "");
    GUI::app_preset_bundle()->update_user_presets_directory(DEFAULT_USER_FOLDER_NAME);
}

Tab* GUI_App::get_tab(Preset::Type type)
{
    for (Tab* tab: tabs_list)
        if (tab->type() == type)
            return tab->completed() ? tab : nullptr; // To avoid actions with no-completed Tab
    return nullptr;
}

Tab* GUI_App::get_plate_tab()
{
    return plate_tab;
}

Tab* GUI_App::get_model_tab(bool part)
{
    return model_tabs_list[part ? 1 : 0];
}

Tab* GUI_App::get_layer_tab()
{
    return model_tabs_list[2];
}

void GUI_App::save_mode(const /*ConfigOptionMode*/int mode)
{
    app_save_mode(mode);
    update_mode();
}

// Update view mode according to selected menu
void GUI_App::update_mode()
{
    sidebar().update_mode();

    //BBS: GUI refactor
    if (main_panel->m_param_panel)
        main_panel->m_param_panel->update_mode();
    if (main_panel->m_param_dialog)
        main_panel->m_param_dialog->panel()->update_mode();

#ifdef _MSW_DARK_MODE
    dynamic_cast<Notebook*>(main_panel->m_tabpanel)->UpdateMode();
#endif

    for (auto tab : tabs_list)
        tab->update_mode();
    for (auto tab : model_tabs_list)
        tab->update_mode();

    //BBS plater()->update_menus();

    plater()->update_gizmos_on_off_state();
}

void GUI_App::open_preferences(size_t open_on_tab, const std::string& highlight_option)
{
    bool app_layout_changed = false;
    {
        // the dialog needs to be destroyed before the call to recreate_GUI()
        // or sometimes the application crashes into wxDialogBase() destructor
        // so we put it into an inner scope
        PreferencesDialog dlg(main_panel, open_on_tab, highlight_option);
        dlg.ShowModal();
        this->plater_->get_current_canvas3D()->force_set_focus();

#ifdef _WIN32
        if (true) {
            if (app_config->get("associate_3mf") == "true")
                associate_files(L"3mf");
            if (app_config->get("associate_stl") == "true")
                associate_files(L"stl");
            if (app_config->get("associate_step") == "true") {
                associate_files(L"step");
                associate_files(L"stp");
            }
            associate_url(L"orcaslicer");
        }
        else {
            if (app_config->get("associate_gcode") == "true")
                associate_files(L"gcode");
        }
#endif // _WIN32
    }
}

bool GUI_App::has_unsaved_preset_changes() const
{
    PrinterTechnology printer_technology = preset_bundle->printers.get_edited_preset().printer_technology();
    for (const Tab* const tab : tabs_list) {
        if (tab->supports_printer_technology(printer_technology) && tab->saved_preset_is_dirty())
            return true;
    }
    return false;
}

bool GUI_App::has_current_preset_changes() const
{
    PrinterTechnology printer_technology = preset_bundle->printers.get_edited_preset().printer_technology();
    for (const Tab* const tab : tabs_list) {
        if (tab->supports_printer_technology(printer_technology) && tab->current_preset_is_dirty())
            return true;
    }
    return false;
}

void GUI_App::update_saved_preset_from_current_preset()
{
    PrinterTechnology printer_technology = preset_bundle->printers.get_edited_preset().printer_technology();
    for (Tab* tab : tabs_list) {
        if (tab->supports_printer_technology(printer_technology))
            tab->update_saved_preset_from_current_preset();
    }
}

std::vector<std::pair<unsigned int, std::string>> GUI_App::get_selected_presets() const
{
    std::vector<std::pair<unsigned int, std::string>> ret;
    PrinterTechnology printer_technology = preset_bundle->printers.get_edited_preset().printer_technology();
    for (Tab* tab : tabs_list) {
        if (tab->supports_printer_technology(printer_technology)) {
            const PresetCollection* presets = tab->get_presets();
            ret.push_back({ static_cast<unsigned int>(presets->type()), presets->get_selected_preset_name() });
        }
    }
    return ret;
}

// To notify the user whether he is aware that some preset changes will be lost,
// UnsavedChangesDialog: "Discard / Save / Cancel"
// This is called when:
// - Close Application & Current project isn't saved
// - Load Project      & Current project isn't saved
// - Undo / Redo with change of print technologie
// - Loading snapshot
// - Loading config_file/bundle
// UnsavedChangesDialog: "Don't save / Save / Cancel"
// This is called when:
// - Exporting config_bundle
// - Taking snapshot
bool GUI_App::check_and_save_current_preset_changes(const wxString& caption, const wxString& header, bool remember_choice/* = true*/, bool dont_save_insted_of_discard/* = false*/)
{
    if (has_current_preset_changes()) {
        int act_buttons = ActionButtons::SAVE;
        if (dont_save_insted_of_discard)
            act_buttons |= ActionButtons::DONT_SAVE;
        if (remember_choice)
            act_buttons |= ActionButtons::REMEMBER_CHOISE;
        UnsavedChangesDialog dlg(caption, header, "", act_buttons);
        bool no_need_change = dlg.getUpdateItemCount() == 0 ? true : false;
        if (!no_need_change && dlg.ShowModal() == wxID_CANCEL)
            return false;

        if (dlg.save_preset())  // save selected changes
        {
            //BBS: add project embedded preset relate logic
            for (const UnsavedChangesDialog::PresetData& nt : dlg.get_names_and_types())
                preset_bundle->save_changes_for_preset(nt.name, nt.type, dlg.get_unselected_options(nt.type), nt.save_to_project);
            //for (const std::pair<std::string, Preset::Type>& nt : dlg.get_names_and_types())
            //    preset_bundle->save_changes_for_preset(nt.first, nt.second, dlg.get_unselected_options(nt.second));

            load_current_presets(false);

            // if we saved changes to the new presets, we should to
            // synchronize config.ini with the current selections.
            preset_bundle->export_selections(*app_config);

            //MessageDialog(nullptr, _L_PLURAL("Modifications to the preset have been saved",
            //                                 "Modifications to the presets have been saved", dlg.get_names_and_types().size())).ShowModal();
        }
    }

    return true;
}

void GUI_App::apply_keeped_preset_modifications()
{
    PrinterTechnology printer_technology = preset_bundle->printers.get_edited_preset().printer_technology();
    for (Tab* tab : tabs_list) {
        if (tab->supports_printer_technology(printer_technology))
            tab->apply_config_from_cache();
    }
    load_current_presets(false);
}

// This is called when creating new project or load another project
// OR close ConfigWizard
// to ask the user what should we do with unsaved changes for presets.
// New Project          => Current project is saved    => UnsavedChangesDialog: "Keep / Discard / Cancel"
//                      => Current project isn't saved => UnsavedChangesDialog: "Keep / Discard / Save / Cancel"
// Close ConfigWizard   => Current project is saved    => UnsavedChangesDialog: "Keep / Discard / Save / Cancel"
// Note: no_nullptr postponed_apply_of_keeped_changes indicates that thie function is called after ConfigWizard is closed
bool GUI_App::check_and_keep_current_preset_changes(const wxString& caption, const wxString& header, int action_buttons, bool* postponed_apply_of_keeped_changes/* = nullptr*/)
{
    if (has_current_preset_changes()) {
        bool is_called_from_configwizard = postponed_apply_of_keeped_changes != nullptr;

        UnsavedChangesDialog dlg(caption, header, "", action_buttons);
        bool no_need_change = dlg.getUpdateItemCount() == 0 ? true : false;
        if (!no_need_change && dlg.ShowModal() == wxID_CANCEL)
            return false;

        auto reset_modifications = [this, is_called_from_configwizard]() {
            //if (is_called_from_configwizard)
            //    return; // no need to discared changes. It will be done fromConfigWizard closing

            PrinterTechnology printer_technology = preset_bundle->printers.get_edited_preset().printer_technology();
            for (const Tab* const tab : tabs_list) {
                if (tab->supports_printer_technology(printer_technology) && tab->current_preset_is_dirty())
                    tab->m_presets->discard_current_changes();
            }
            load_current_presets(false);
        };

        if (dlg.discard() || no_need_change)
            reset_modifications();
        else  // save selected changes
        {
            //BBS: add project embedded preset relate logic
            const auto& preset_names_and_types = dlg.get_names_and_types();
            if (dlg.save_preset()) {
                for (const UnsavedChangesDialog::PresetData& nt : preset_names_and_types)
                    preset_bundle->save_changes_for_preset(nt.name, nt.type, dlg.get_unselected_options(nt.type), nt.save_to_project);

                // if we saved changes to the new presets, we should to
                // synchronize config.ini with the current selections.
                preset_bundle->export_selections(*app_config);

                //MessageDialog(nullptr, text).ShowModal();
                reset_modifications();
            }
            else if (dlg.transfer_changes() && (dlg.has_unselected_options() || is_called_from_configwizard)) {
                // execute this part of code only if not all modifications are keeping to the new project
                // OR this function is called when ConfigWizard is closed and "Keep modifications" is selected
                for (const UnsavedChangesDialog::PresetData& nt : preset_names_and_types) {
                    Preset::Type type = nt.type;
                    Tab* tab = get_tab(type);
                    std::vector<std::string> selected_options = dlg.get_selected_options(type);
                    if (type == Preset::TYPE_PRINTER) {
                        auto it = std::find(selected_options.begin(), selected_options.end(), "extruders_count");
                        if (it != selected_options.end()) {
                            // erase "extruders_count" option from the list
                            selected_options.erase(it);
                            // cache the extruders count
                            static_cast<TabPrinter*>(tab)->cache_extruder_cnt();
                        }
                    }
                    std::vector<std::string> selected_options2;
                    std::transform(selected_options.begin(), selected_options.end(), std::back_inserter(selected_options2), [](auto & o) {
                        auto i = o.find('#');
                        return i != std::string::npos ? o.substr(0, i) : o;
                    });
                    tab->cache_config_diff(selected_options2);
                    if (!is_called_from_configwizard)
                        tab->m_presets->discard_current_changes();
                }
                if (is_called_from_configwizard)
                    *postponed_apply_of_keeped_changes = true;
                else
                    apply_keeped_preset_modifications();
            }
        }
    }

    return true;
}

bool GUI_App::checked_tab(Tab* tab)
{
    bool ret = true;
    if (find(tabs_list.begin(), tabs_list.end(), tab) == tabs_list.end() &&
        find(model_tabs_list.begin(), model_tabs_list.end(), tab) == model_tabs_list.end())
        ret = false;
    return ret;
}

// Update UI / Tabs to reflect changes in the currently loaded presets
//BBS: add preset combo box re-activate logic
void GUI_App::load_current_presets(bool active_preset_combox/*= false*/, bool check_printer_presets_ /*= true*/)
{
    PrinterTechnology printer_technology = preset_bundle->printers.get_edited_preset().printer_technology();
	this->plater()->set_printer_technology(printer_technology);
    for (Tab *tab : tabs_list)
		if (tab->supports_printer_technology(printer_technology)) {
			if (tab->type() == Preset::TYPE_PRINTER) {
				static_cast<TabPrinter*>(tab)->update_pages();
				this->plater()->force_print_bed_update();
			}
			tab->load_current_preset();
			//BBS: add preset combox re-active logic
			if (active_preset_combox)
				tab->reactive_preset_combo_box();
		}
    // BBS: model config
    for (Tab *tab : model_tabs_list)
		if (tab->supports_printer_technology(printer_technology)) {
            tab->rebuild_page_tree();
        }
}

bool GUI_App::OnExceptionInMainLoop()
{
    generic_exception_handle();
    return false;
}

Sidebar& GUI_App::sidebar()
{
    return plater_->sidebar();
}

GizmoObjectManipulation *GUI_App::obj_manipul()
{
    // If this method is called before plater_ has been initialized, return nullptr (to avoid a crash)
    return (plater_ != nullptr) ? &plater_->get_gizmos_manager()->get_object_manipulation() : nullptr;
}

ObjectSettings* GUI_App::obj_settings()
{
    return sidebar().obj_settings();
}

ObjectList* GUI_App::obj_list()
{
    return sidebar().obj_list();
}

ObjectLayers* GUI_App::obj_layers()
{
    return sidebar().obj_layers();
}

Plater* GUI_App::plater()
{
    return plater_;
}

const Plater* GUI_App::plater() const
{
    return plater_;
}

ParamsPanel* GUI_App::params_panel()
{
    if (main_panel)
        return main_panel->m_param_panel;
    return nullptr;
}

ParamsDialog* GUI_App::params_dialog()
{
    if (main_panel)
        return main_panel->m_param_dialog;
    return nullptr;
}

Model& GUI_App::model()
{
    return plater_->model();
}

Notebook* GUI_App::tab_panel() const
{
    if (main_panel)
        return main_panel->m_tabpanel;
    return nullptr;
}

PrintSequence GUI_App::global_print_sequence() const
{
    PrintSequence global_print_seq = PrintSequence::ByDefault;
    auto curr_preset_config = preset_bundle->prints.get_edited_preset().config;
    if (curr_preset_config.has("print_sequence"))
        global_print_seq = curr_preset_config.option<ConfigOptionEnum<PrintSequence>>("print_sequence")->value;
    return global_print_seq;
}

bool GUI_App::run_wizard(ConfigWizard::RunReason reason, ConfigWizard::StartPage start_page)
{
    wxCHECK_MSG(main_panel != nullptr, false, "Internal error: Main frame not created / null");

    std::string strFinish = AppAdapter::app_config()->get("firstguide", "finish");
    long        pStyle    = wxCAPTION | wxCLOSE_BOX | wxSYSTEM_MENU;
    if (strFinish == "false" || strFinish.empty())
        pStyle = wxCAPTION | wxTAB_TRAVERSAL;

    GuideFrame wizard(pStyle);
    auto page = start_page == ConfigWizard::SP_WELCOME ? GuideFrame::BBL_WELCOME :
                start_page == ConfigWizard::SP_FILAMENTS ? GuideFrame::BBL_FILAMENT_ONLY :
                start_page == ConfigWizard::SP_PRINTERS ? GuideFrame::BBL_MODELS_ONLY :
                GuideFrame::BBL_MODELS;
    wizard.SetStartPage(page);
    bool       res = wizard.run();

    if (res) {
        load_current_presets();
    }

    return res;
}

bool GUI_App::config_wizard_startup()
{
    if (!m_app_conf_exists || preset_bundle->printers.only_default_printers()) {
        BOOST_LOG_TRIVIAL(info) << "run wizard...";
        run_wizard(ConfigWizard::RR_DATA_EMPTY);
        BOOST_LOG_TRIVIAL(info) << "finished run wizard";
        return true;
    } 
    return false;
}

void GUI_App::start_download(std::string url)
{
    if (!plater_) {
        BOOST_LOG_TRIVIAL(error) << "Could not start URL download: plater is nullptr.";
        return;
    }
    //lets always init so if the download dest folder was changed, new dest is used
    boost::filesystem::path dest_folder(app_config->get("download_path"));
    if (dest_folder.empty() || !boost::filesystem::is_directory(dest_folder)) {
        std::string msg = _u8L("Could not start URL download. Destination folder is not set. Please choose destination folder in Configuration Wizard.");
        BOOST_LOG_TRIVIAL(error) << msg;
        show_error(nullptr, msg);
        return;
    }
    m_downloader->init(dest_folder);
    m_downloader->start_download(url);

}

} // GUI
} //Slic3r

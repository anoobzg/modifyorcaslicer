#ifndef slic3r_GUI_App_hpp_
#define slic3r_GUI_App_hpp_

#include <memory>
#include <string>

#include <boost/thread.hpp>
#include "libslic3r/Preset.hpp"
#include "slic3r/Config/VersionInfo.hpp"
#include "slic3r/GUI/Dialog/ConfigWizard.hpp"
#include "slic3r/GUI/Frame/UserNotification.hpp"

#include <wx/snglinst.h>

//#define BBL_HAS_FIRST_PAGE          1
#define STUDIO_INACTIVE_TIMEOUT     15*60*1000
#define LOG_FILES_MAX_NUM           30
#define TIMEOUT_CONNECT             15
#define TIMEOUT_RESPONSE            15

#define BE_UNACTED_ON               0x00200001

class wxMenuItem;
class wxMenuBar;
class wxTopLevelWindow;
class wxDataViewCtrl;
class wxBookCtrlBase;
struct wxLanguageInfo;

namespace Slic3r {

class AppConfig;
class Preset;
class PresetBundle;
class PresetUpdater;
class Model;

namespace GUI{
// BBS
class Notebook;
class RemovableDriveManager;
class OtherInstanceMessageHandler;
class MainFrame;
class MainPanel;
class Sidebar;
class ObjectSettings;
class ObjectList;
class ObjectLayers;
class Plater;
class ParamsPanel;
class NotificationManager;
class Downloader;
class ParamsDialog;
class AppFont;

class Tab;
class ConfigWizard;
class GizmoObjectManipulation; 

// Does our wxWidgets version support markup?
#if wxUSE_MARKUP && wxCHECK_VERSION(3, 1, 1)
    #define SUPPORTS_MARKUP
#endif

class GUI_App
{
private:
    bool            m_initialized { false };
    bool            m_post_initialized { false };
    bool            m_app_conf_exists{ false };
    bool            m_is_recreating_gui{ false };

    std::unique_ptr<RemovableDriveManager> m_removable_drive_manager;
    std::unique_ptr<Downloader> m_downloader;

    //BBS
    bool m_is_closing {false};

    bool             m_adding_script_handler { false };
public:
    bool            OnInit(wxFrame* frame);
    int             OnExit();
    bool            initialized() const { return m_initialized; }

    explicit GUI_App();
    ~GUI_App();

    bool is_recreating_gui() const { return m_is_recreating_gui; }

    // To be called after the GUI is fully built up.
    // Process command line parameters cached in this->init_params,
    // load configs, STLs etc.
    void            post_init();
    void            shutdown();

#if wxUSE_WEBVIEW_EDGE
    void            init_webview_runtime();
#endif

    void            update_label_colours();
    void            recreate_GUI(const wxString& message);

    void            ShowUserGuide();
    void            ShowOnlyFilament();

    void            enable_user_preset_folder(bool enable);

    // void            show_dialog(wxString msg);
    // void            push_notification(wxString msg, wxString title = wxEmptyString, UserNotificationStyle style = UserNotificationStyle::UNS_NORMAL);

    void            update_ui_from_settings();

    Tab*            get_tab(Preset::Type type);
    Tab*            get_plate_tab();
    Tab*            get_model_tab(bool part = false);
    Tab*            get_layer_tab();

    void            save_mode(const /*ConfigOptionMode*/int mode) ;
    void            update_mode();

    bool            has_unsaved_preset_changes() const;
    bool            has_current_preset_changes() const;
    void            update_saved_preset_from_current_preset();
    std::vector<std::pair<unsigned int, std::string>> get_selected_presets() const;
    bool            check_and_save_current_preset_changes(const wxString& caption, const wxString& header, bool remember_choice = true, bool use_dont_save_insted_of_discard = false);
    void            apply_keeped_preset_modifications();
    bool            check_and_keep_current_preset_changes(const wxString& caption, const wxString& header, int action_buttons, bool* postponed_apply_of_keeped_changes = nullptr);
    bool            checked_tab(Tab* tab);
    //BBS: add preset combox re-active logic
    void            load_current_presets(bool active_preset_combox = false, bool check_printer_presets = true);

    void            open_preferences(size_t open_on_tab = 0, const std::string& highlight_option = std::string());

    virtual bool OnExceptionInMainLoop();

    Sidebar&             sidebar();
    GizmoObjectManipulation *obj_manipul();
    ObjectSettings*      obj_settings();
    ObjectList*          obj_list();
    ObjectLayers*        obj_layers();
    Plater*              plater();
    const Plater*        plater() const;
    ParamsPanel*         params_panel();
    ParamsDialog*        params_dialog();
    Model&      		 model();

    bool            is_adding_script_handler() { return m_adding_script_handler; }
    void            set_adding_script_handler(bool status) { m_adding_script_handler = status; }

    MainPanel*      main_panel{ nullptr };
    Plater*         plater_{ nullptr };

    Notebook*       tab_panel() const ;

    PrintSequence   global_print_sequence() const;

    std::vector<Tab *>      tabs_list;
    std::vector<Tab *>      model_tabs_list;
    Tab*                    plate_tab;

	RemovableDriveManager* removable_drive_manager() { return m_removable_drive_manager.get(); }

    bool            run_wizard(ConfigWizard::RunReason reason, ConfigWizard::StartPage start_page = ConfigWizard::SP_WELCOME);

    // URL download - PrusaSlicer gets system call to open prusaslicer:// URL which should contain address of download
    void            start_download(std::string url);
private:
    bool            on_init_inner(wxFrame* frame);
    void            init_app_config();

    bool            config_wizard_startup();

    bool                    m_config_corrupted { false };
public:
    AppConfig*      app_config{ nullptr };
    PresetBundle*   preset_bundle{ nullptr };

    PresetUpdater*  preset_updater{ nullptr };
};

DECLARE_APP(GUI_App)

} // namespace GUI
} // Slic3r

#endif // slic3r_GUI_App_hpp_

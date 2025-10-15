#ifndef slic3r_MainPanel_hpp_
#define slic3r_MainPanel_hpp_

#include "libslic3r/PrintConfig.hpp"

#include <wx/frame.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/filehistory.h>
#ifdef __APPLE__
#include <wx/taskbar.h>
#endif // __APPLE__

#include <string>
#include <map>

#include "GUI_Utils.hpp"
//BBS: GUI refactor
#include "slic3r/Config/ParamsPanel.hpp"
#include "slic3r/GUI/Dialog/UnsavedChangesDialog.hpp"
#include "Widgets/SideButton.hpp"
#include "Widgets/SideMenuPopup.hpp"
#include "slic3r/GUI/Event/Event.hpp"

#include <boost/property_tree/ptree_fwd.hpp>

#include <wx/panel.h>

#define ENABEL_PRINT_ALL 0

class wxBookCtrlBase;
class wxProgressDialog;

namespace Slic3r {

namespace GUI
{

class Notebook;
class Tab;
class Plater;
class MainPanel;
class ParamsDialog;

enum QuickSlice
{
   qsUndef = 0,
   qsReslice = 1,
   qsSaveAs = 2,
   qsExportSVG = 4,
   qsExportPNG = 8
};

struct PresetTab {
    std::string       name;
    Tab*              panel;
    PrinterTechnology technology;
};

// ----------------------------------------------------------------------------
// SettingsDialog
// ----------------------------------------------------------------------------

class SettingsDialog : public DPIDialog//DPIDialog
{
    //wxNotebook* m_tabpanel { nullptr };
    Notebook* m_tabpanel{ nullptr };
    wxMenuBar*      m_menubar{ nullptr };
public:
    SettingsDialog();
    ~SettingsDialog() = default;
    //void set_tabpanel(wxNotebook* tabpanel) { m_tabpanel = tabpanel; }
    void set_tabpanel(Notebook* tabpanel) { m_tabpanel = tabpanel; }
    wxMenuBar* menubar() { return m_menubar; }

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;
};
class MainFrame;
class PA_Calibration_Dlg;
class Temp_Calibration_Dlg;
class MaxVolumetricSpeed_Test_Dlg;
class VFA_Test_Dlg;
class Retraction_Test_Dlg;
class MainPanel : public wxPanel
{
    bool        m_loaded {false};
    wxTimer* m_reset_title_text_colour_timer{ nullptr };

    wxString    m_last_config = wxEmptyString;

    wxSizer*    m_main_sizer{ nullptr };

    size_t      m_last_selected_tab;

    std::string     get_base_name(const wxString &full_name, const char *extension = nullptr) const;
    std::string     get_dir_name(const wxString &full_name) const;

    void on_presets_changed(SimpleEvent&);
    void on_value_changed(wxCommandEvent&);

    bool can_start_new_project() const;
    bool can_open_project() const;
    bool can_add_models() const;
    bool can_export_model() const;
    bool can_export_toolpaths() const;
    bool can_export_gcode() const;
    bool can_export_all_gcode() const;
    bool can_print_3mf() const;
    bool can_send_gcode() const;
    //bool can_export_gcode_sd() const;
    //bool can_eject() const;
    bool can_slice() const;
    bool can_change_view() const;
    bool can_select() const;
    bool can_deselect() const;
    bool can_clone() const;
    bool can_delete() const;
    bool can_delete_all() const;
    bool can_reslice() const;
    void bind_diff_dialog();

    // BBS
    wxBoxSizer* create_side_tools();

    // MenuBar items changeable in respect to printer technology
    enum MenuItems
    {                   //   FFF                  SLA
        miExport = 0,   // Export G-code        Export
        miSend,         // Send G-code          Send to print
        miMaterialTab,  // Filament Settings    Material Settings
        miPrinterTab,   // Different bitmap for Printer Settings
    };

    // vector of a MenuBar items changeable in respect to printer technology
    std::vector<wxMenuItem*> m_changeable_menu_items;

    struct FileHistory : wxFileHistory
    {
        FileHistory(int max) : wxFileHistory(max) {}
        std::wstring GetThumbnailUrl(int index) const;

        virtual void AddFileToHistory(const wxString &file);
        virtual void RemoveFileFromHistory(size_t i);
        size_t FindFileInHistory(const wxString &file);

        void LoadThumbnails();

        void SetMaxFiles(int max);
    private:
        std::deque<std::string> m_thumbnails;
        bool m_load_called = false;
    };

    FileHistory m_recent_projects;

    enum class ESettingsLayout
    {
        Unknown,
        Old,
        GCodeViewer
    };

    ESettingsLayout m_layout{ ESettingsLayout::Unknown };

    enum SliceSelectType
    {
        eSliceAll = 0,
        eSlicePlate = 1,
    };

    //jump to editor under preview only mode
    bool preview_only_to_editor = false;

public:
    void on_dpi_changed(const wxRect &suggested_rect);
    void on_sys_color_changed();

public:
    MainPanel(wxWindow *parent);
    ~MainPanel() = default;

    void init(); // second init

    //BBS GUI refactor
    enum TabPosition
    {
        tp3DEditor      = 0,
        tpPreview       = 1
    };

    //BBS: add slice&&print status update logic
    enum SlicePrintEventType
    {
        eEventObjectUpdate = 0,
        eEventPlateUpdate = 1,
        eEventParamUpdate = 2,
        eEventSliceUpdate = 3,
        eEventPrintUpdate = 4
    };

    // BBS GUI refactor
    enum PrintSelectType {
        ePrintAll            = 0,
        ePrintPlate          = 1,
        eExportSlicedFile    = 2,
        eExportGcode         = 3,
        eSendGcode           = 4,
        eSendToPrinter       = 5,
        eSendToPrinterAll    = 6,
        eUploadGcode         = 7,
        eExportAllSlicedFile = 8,
        ePrintMultiMachine   = 9
    };

    void update_layout();

	// Called when closing the application and when switching the application language.
	void 		shutdown();

    Plater*     plater() { return m_plater; }

    // for cali to update tab when save new preset
    void update_filament_tab_ui();

    void        update_title();
    void        set_max_recent_count(int max);

    void        set_title(const wxString& title);
    void        show_option(bool show);
    void        init_tabpanel();
    void        create_preset_tabs();
    //BBS: GUI refactor
    void        add_created_tab(Tab* panel, const std::string& bmp_name = "");
    bool        is_active_and_shown_tab(wxPanel* panel);

    // Register Win32 RawInput callbacks (3DConnexion) and removable media insert / remove callbacks.
    // Called from wxEVT_ACTIVATE, as wxEVT_CREATE was not reliable (bug in wxWidgets?).
    void        register_win32_callbacks();

    //BBS
    void        show_log_window();

    void        update_ui_from_settings();
    //BBS
    void        update_side_preset_ui();

    bool        is_loaded() const { return m_loaded; }

    void        export_config();
    // Query user for the config file and open it.
    void        load_config_file();
    // Open a config file. Return true if loaded.
    bool        load_config_file(const std::string &path);

    void        load_config(const DynamicPrintConfig& config);
    //BBS: hint when jump to 3Deditor under preview only mode
    bool        preview_only_hint();
    // Select tab in m_tabpanel
    // When tab == -1, will be selected last selected tab
    //BBS: GUI refactor
    void        select_tab(wxPanel* panel);
    void        select_tab(size_t tab = size_t(-1));
    void        request_select_tab(TabPosition pos);
    void        select_view(const std::string& direction);
    // Propagate changed configuration from the Tab to the Plater and save changes to the AppConfig
    void        on_config_changed(DynamicPrintConfig* cfg) const ;
    void        set_print_button_to_default(PrintSelectType select_type);

    bool can_save() const;
    bool can_save_as() const;
    //BBS
    bool can_upload() const;
    void save_project();
    bool save_project_as(const wxString& filename = wxString());

    void        add_to_recent_projects(const wxString& filename);
    void        get_recent_projects(boost::property_tree::wptree &tree, int images);
    void        open_recent_project(size_t file_id, wxString const & filename);
    void        remove_recent_project(size_t file_id, wxString const &filename);

    void update_font_from_mainframe();

    PA_Calibration_Dlg* m_pa_calib_dlg{ nullptr };
    Temp_Calibration_Dlg* m_temp_calib_dlg{ nullptr };
    MaxVolumetricSpeed_Test_Dlg* m_vol_test_dlg { nullptr };
    VFA_Test_Dlg* m_vfa_test_dlg { nullptr };
    Retraction_Test_Dlg* m_retraction_calib_dlg{ nullptr };

    Plater*               m_plater { nullptr };

    wxLogWindow*          m_log_window { nullptr };
    // BBS
    //wxBookCtrlBase*       m_tabpanel { nullptr };
    Notebook*             m_tabpanel{ nullptr };
    wxBoxSizer*           m_side_tools{ nullptr };
    ParamsPanel*          m_param_panel{ nullptr };
    ParamsDialog*         m_param_dialog{ nullptr };
    //BBS
    SettingsDialog        m_settings_dialog;
    DiffPresetDialog      diff_dialog;
    wxWindow*             m_plater_page{ nullptr };

    mutable int m_print_select{ ePrintAll };
    mutable int m_slice_select{ eSliceAll };
    // Button* m_publish_btn{ nullptr };
    SideButton* m_slice_btn{ nullptr };
    SideButton* m_slice_option_btn{ nullptr };
    SideButton* m_print_btn{ nullptr };
    SideButton* m_print_option_btn{ nullptr };
    mutable bool          m_slice_enable{ true };
    mutable bool          m_print_enable{ true };
    bool get_enable_slice_status();
    bool get_enable_print_status();
    //BBS
    void update_side_button_style();
    void update_slice_print_status(SlicePrintEventType event, bool can_slice = true, bool can_print = true);

#ifdef __APPLE__
    std::unique_ptr<wxTaskBarIcon> m_taskbar_icon;
#endif // __APPLE__

#ifdef _WIN32
    void*				m_hDeviceNotify { nullptr };
    uint32_t  			m_ulSHChangeNotifyRegister { 0 };
	static constexpr int WM_USER_MEDIACHANGED { 0x7FFF }; // WM_USER from 0x0400 to 0x7FFF, picking the last one to not interfere with wxWidgets allocation
#endif // _WIN32
};

} // GUI
} //Slic3r

#endif // slic3r_MainFrame_hpp_

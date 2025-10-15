#pragma once

#include "libslic3r/Config.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"

#include <cstddef>
#include <algorithm>
#include <numeric>
#include <vector>
#include <string>
#include <regex>
#include <future>
#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/bmpcbox.h>
#include <wx/statbox.h>
#include <wx/statbmp.h>
#include <wx/filedlg.h>
#include <wx/dnd.h>
#include <wx/progdlg.h>
#include <wx/string.h>
#include <wx/wupdlock.h>
#include <wx/numdlg.h>
#include <wx/debug.h>
#include <wx/busyinfo.h>
#include <wx/event.h>
#include <wx/wrapsizer.h>
#ifdef _WIN32
#include <wx/richtooltip.h>
#include <wx/custombgwin.h>
#include <wx/popupwin.h>
#endif
#include <wx/clrpicker.h>
#include <wx/tokenzr.h>
#include <wx/aui/aui.h>

#include "libslic3r/libslic3r.h"
#include "libslic3r/Format/STL.hpp"
#include "libslic3r/Format/STEP.hpp"
#include "libslic3r/Format/AMF.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"
#include "libslic3r/FileSystem/Log.hpp"
#include "slic3r/GUI/Frame/Notebook.hpp"
#include "slic3r/GUI/Widgets/Tabbook.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/FileSystem/FileHelp.hpp"

#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Config/GUI_ObjectList.hpp"
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/GUI_Factories.hpp"
#include "slic3r/GUI/wxExtensions.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/Render/3DScene.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/Render/Selection.hpp"
#include "slic3r/Render/GLToolbar.hpp"

#include "slic3r/Render/3DBed.hpp"
#include "slic3r/Scene/Camera.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Render/Mouse3DController.hpp"
#include "slic3r/GUI/Tab.hpp"
#include "slic3r/GUI/Jobs/OrientJob.hpp"
#include "slic3r/GUI/Jobs/ArrangeJob.hpp"
#include "slic3r/GUI/Jobs/FillBedJob.hpp"
#include "slic3r/GUI/Jobs/NotificationProgressIndicator.hpp"
#include "slic3r/GUI/Jobs/PlaterWorker.hpp"
#include "slic3r/GUI/Jobs/BoostThreadWorker.hpp"
#include "slic3r/Slice/BackgroundSlicingProcess.hpp"
#include "slic3r/GUI/Dialog/ConfigWizard.hpp"
#include "libslic3r/FileSystem/ASCIIFolding.hpp"
#include "slic3r/Utils/FixModelByWin10.hpp"
#include "slic3r/Utils/UndoRedo.hpp"
#include "slic3r/Utils/Process.hpp"
#include "slic3r/Utils/RemovableDriveManager.hpp"
#include "slic3r/Scene/NotificationManager.hpp"
#include "slic3r/Config/PresetComboBoxes.hpp"
#include "slic3r/GUI/Dialog/MsgDialog.hpp"
#include "slic3r/GUI/Project/ProjectDirtyStateManager.hpp"
#include "slic3r/GUI/Gizmos/GLGizmoSimplify.hpp" // create suggestion notification
#include "slic3r/GUI/Gizmos/GizmoObjectManipulation.hpp"
#include "slic3r/Theme/AppColor.hpp"
// BBS
#include "slic3r/GUI/Widgets/ProgressDialog.hpp"
#include "slic3r/Theme/BitmapCache.hpp"
#include "slic3r/GUI/Dialog/ParamsDialog.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/RadioBox.hpp"
#include "slic3r/GUI/Widgets/CheckBox.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/StaticLine.hpp"
#include "slic3r/Utils/AppWx.hpp"
#include "slic3r/GUI/Config/GUI_ObjectTable.hpp"
// #include "libslic3r/Thread.hpp"
#include "slic3r/GUI/Config/PlaterPrinterPresetComboBox.hpp"
#include "slic3r/GUI/Slice/WipingDialog.hpp"
#include "slic3r/Utils/Str.hpp"
#include "slic3r/GUI/Event/UserGLToolBarEvent.hpp"
#include "slic3r/GUI/Event/UserCanvasEvent.hpp"
#include "slic3r/GUI/Event/UserPlaterEvent.hpp"
#include "slic3r/GUI/Dialog/ReleaseNote.hpp"
#ifdef __APPLE__
#include "slic3r/GUI/Gizmos/GLGizmosManager.hpp"
#endif // __APPLE__

#include <libslic3r/CutUtils.hpp>
#include <wx/glcanvas.h>    // Needs to be last because reasons :-/
#include <libslic3r/Zip/miniz_extension.hpp>
#include "slic3r/GUI/Dialog/ObjColorDialog.hpp"

#include "libslic3r/CustomGCode.hpp"
#include "libslic3r/Base/Platform.hpp"
#include "nlohmann/json.hpp"
#include "slic3r/GUI/Event/UserNetEvent.hpp"

#include "slic3r/GUI/Frame/DailyTips.hpp"
#include "slic3r/GUI/Dialog/CreatePresetsDialog.hpp"
#include "slic3r/GUI/Dialog/FileArchiveDialog.hpp"
#include "slic3r/GUI/Config/PlateSettingsDialog.hpp"
#include "slic3r/GUI/Project/ProjectDropDialog.hpp"
#include "slic3r/GUI/Dialog/DialogCommand.hpp"

#include "slic3r/Config/ConfigUtils.hpp"
#include "slic3r/Scene/Boolean.hpp"

using boost::optional;
namespace fs = boost::filesystem;
using Slic3r::_3DScene;
using Slic3r::Preset;
using Slic3r::GUI::format_wxstr;
using namespace nlohmann;

namespace Slic3r {
namespace GUI {

class View3D;
class GCodePreview;
class GCodeViewer;
class OpenGLPanel;
class SceneRaycaster;
// State to manage showing after export notifications and device ejecting
enum ExportingStatus{
    NOT_EXPORTING,
    EXPORTING_TO_REMOVABLE,
    EXPORTING_TO_LOCAL
};

// TODO: listen on dark ui change
class FloatFrame : public wxAuiFloatingFrame
{
public:
    FloatFrame(wxWindow* parent, wxAuiManager* ownerMgr, const wxAuiPaneInfo& pane) : wxAuiFloatingFrame(parent, ownerMgr, pane)
    {
        UpdateFrameDarkUI(this);
    }
};

class AuiMgr : public wxAuiManager
{
public:
    AuiMgr() : wxAuiManager(){}

    virtual wxAuiFloatingFrame* CreateFloatingFrame(wxWindow* parent, const wxAuiPaneInfo& p) override
    {
        return new FloatFrame(parent, this, p);
    }
};

class PlaterDropTarget : public wxFileDropTarget
{
public:
    PlaterDropTarget(MainPanel& main_panel, Plater& plater) : m_main_panel(main_panel), m_plater(plater) {
        this->SetDefaultAction(wxDragCopy);
    }

    virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &filenames);

private:
    MainPanel& m_main_panel;
    Plater& m_plater;
};

struct Sidebar::priv
{
    Plater *plater;

    wxPanel *scrolled;
    PlaterPresetComboBox *combo_print;
    std::vector<PlaterPresetComboBox*> combos_filament;
    int editing_filament = -1;
    wxBoxSizer *sizer_filaments;
    PlaterPresetComboBox *combo_sla_print;
    PlaterPresetComboBox *combo_sla_material;
    PlaterPrinterPresetComboBox* combo_printer = nullptr;
    wxBoxSizer *sizer_params;

    //BBS Sidebar widgets
    wxPanel* m_panel_print_title;
    wxStaticText* m_staticText_print_title;
    wxPanel* m_panel_print_content;
    wxComboBox* m_comboBox_print_preset;
    wxStaticLine* m_staticline1;
    StaticBox* m_panel_filament_title;
    wxStaticText* m_staticText_filament_settings;
    ScalableButton *  m_bpButton_add_filament;
    ScalableButton *  m_bpButton_del_filament;
    ScalableButton *  m_bpButton_ams_filament;
    ScalableButton *  m_bpButton_set_filament;
    wxPanel* m_panel_filament_content;
    wxScrolledWindow* m_scrolledWindow_filament_content;
    wxStaticLine* m_staticline2;
    wxPanel* m_panel_project_title;
    ScalableButton* m_filament_icon = nullptr;
    Button * m_flushing_volume_btn = nullptr;
    wxSearchCtrl* m_search_bar = nullptr;
    Search::SearchObjectDialog* dia = nullptr;

    // BBS printer config
    StaticBox* m_panel_printer_title = nullptr;
    ScalableButton* m_printer_icon = nullptr;
    ScalableButton* m_printer_setting = nullptr;
    wxStaticText* m_text_printer_settings = nullptr;
    wxPanel* m_panel_printer_content = nullptr;

    ObjectList          *m_object_list{ nullptr };
    ObjectSettings      *object_settings{ nullptr };
    ObjectLayers        *object_layers{ nullptr };

    wxButton *btn_export_gcode;
    wxButton *btn_reslice;
    ScalableButton *btn_send_gcode;
    //ScalableButton *btn_eject_device;
    ScalableButton* btn_export_gcode_removable; //exports to removable drives (appears only if removable drive is connected)

    Search::OptionsSearcher     searcher;
    std::string ams_list_device;

    priv(Plater *plater);
    ~priv();

    void show_preset_comboboxes();
    void on_search_update();
    void jump_to_object(ObjectDataViewModelNode* item);
    void can_search();

#ifdef _WIN32
    wxString btn_reslice_tip;
    void show_rich_tip(const wxString& tooltip, wxButton* btn);
    void hide_rich_tip(wxButton* btn);
#endif
};

// Plater / private
struct Plater::priv
{
    // PIMPL back pointer ("Q-Pointer")
    Plater *q;
    MainPanel *main_panel;

    MenuFactory menus;

    // Data
    Slic3r::DynamicPrintConfig *config;        // FIXME: leak?
    Slic3r::Model               model;

    // GUI elements
    AuiMgr m_aui_mgr;
    wxString m_default_window_layout;
    OpenGLPanel* current_panel{ nullptr };
    std::vector<wxPanel*> panels;
    Sidebar *sidebar;
    struct SidebarLayout
    {
        bool                  is_enabled{false};
        bool                  is_collapsed{false};
        bool                  show{false};
    } sidebar_layout;
    // Bed3D bed;
    Camera camera;
    //BBS: partplate related structure
    PartPlateList partplate_list;
    //BBS: add a flag to ignore cancel event
    bool m_ignore_event{false};
    bool m_slice_all{false};
    bool m_is_slicing {false};
    int m_is_RightClickInLeftUI{-1};
    int m_cur_slice_plate;
    //BBS: m_slice_all in .gcode.3mf file case, set true when slice all
    bool m_slice_all_only_has_gcode{ false };

    bool m_need_update{false};
    //BBS: add popup object table logic
    //ObjectTableDialog* m_popup_table{ nullptr };

#if ENABLE_ENVIRONMENT_MAP
    GLTexture environment_texture;
#endif // ENABLE_ENVIRONMENT_MAP
    Mouse3DController mouse3d_controller;
    View3D* view3D;
    // BBS
    //GLToolbar view_toolbar;
    GLToolbar collapse_toolbar;
    GCodePreview *preview;
    std::unique_ptr<NotificationManager> notification_manager;

    /* GLCavans component */
    GLCanvas3DFacade* m_canvas;
    Selection m_selection;
    GLGizmosManager* m_gizmos;
    SceneRaycaster* m_scene_raycaster;

    ProjectDirtyStateManager dirty_state;

    BackgroundSlicingProcess    background_process;
    bool suppressed_backround_processing_update { false };

    // TODO: A mechanism would be useful for blocking the plater interactions:
    // objects would be frozen for the user. In case of arrange, an animation
    // could be shown, or with the optimize orientations, partial results
    // could be displayed.
    //
    // UIThreadWorker can be used as a replacement for BoostThreadWorker if
    // no additional worker threads are desired (useful for debugging or profiling)
    PlaterWorker<BoostThreadWorker> m_worker;

    int                         m_job_prepare_state;

    bool                        delayed_scene_refresh;

    wxTimer                     background_process_timer;

    std::string                 label_btn_export;
    std::string                 label_btn_send;

    bool                        show_wireframe{ false };
    bool                        wireframe_enabled{ true };

    static const std::regex pattern_bundle;
    static const std::regex pattern_3mf;
    static const std::regex pattern_zip_amf;
    static const std::regex pattern_any_amf;
    static const std::regex pattern_prusa;

    bool m_is_dark = false;

    priv(Plater *q, MainPanel *main_frame);
    ~priv();

    void init(Plater *q, MainPanel *main_frame);

    void select_printer_preset(const std::string& preset_name);

    bool need_update() const { return m_need_update; }
    void set_need_update(bool need_update) { m_need_update = need_update; }

    void set_plater_dirty(bool is_dirty) { dirty_state.set_plater_dirty(is_dirty); }
    bool is_project_dirty() const { return dirty_state.is_dirty(); }
    bool is_presets_dirty() const { return dirty_state.is_presets_dirty(); }
    void update_project_dirty_from_presets()
    {
        // BBS: backup
        Slic3r::put_other_changes();
        dirty_state.update_from_presets();
    }
    int save_project_if_dirty(const wxString& reason) {
        int res = wxID_NO;
        if (dirty_state.is_dirty()) {
            MainPanel* mainframe = AppAdapter::main_panel();
            if (mainframe->can_save_as()) {
                wxString suggested_project_name;
                wxString project_name = suggested_project_name = get_project_filename(".3mf");
                if (suggested_project_name.IsEmpty()) {
                    fs::path output_file = get_export_file_path(FT_3MF);
                    suggested_project_name = output_file.empty() ? _L("Untitled") : from_u8(output_file.stem().string());
                }
                res = MessageDialog(mainframe, reason + "\n" + format_wxstr(_L("Do you want to save changes to \"%1%\"?"), suggested_project_name),
                                    wxString(SLIC3R_APP_FULL_NAME), wxYES_NO | wxCANCEL).ShowModal();
                if (res == wxID_YES)
                    if (!mainframe->save_project_as(project_name))
                        res = wxID_CANCEL;
            }
        }
        return res;
    }
    void reset_project_dirty_after_save() { m_undo_redo_stack_main.mark_current_as_saved(); dirty_state.reset_after_save(); }
    void reset_project_dirty_initial_presets() { dirty_state.reset_initial_presets(); }

#if ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW
    void render_project_state_debug_window() const { dirty_state.render_debug_window(); }
#endif // ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW

    enum class UpdateParams {
        FORCE_FULL_SCREEN_REFRESH          = 1,
        FORCE_BACKGROUND_PROCESSING_UPDATE = 2,
        POSTPONE_VALIDATION_ERROR_MESSAGE  = 4,
    };
    void update(unsigned int flags = 0, bool reload_scene = true);
    void select_view(const std::string& direction);
    void select_next_view_3D();

    bool is_preview_shown() const;
    bool is_preview_loaded() const;
    bool is_view3D_shown() const;

    bool are_view3D_labels_shown() const;
    void show_view3D_labels(bool show);

    bool is_view3D_overhang_shown() const;
    void show_view3D_overhang(bool show);

    bool is_view3D_layers_editing_enabled() const;

    void enable_sidebar(bool enabled);
    void collapse_sidebar(bool collapse);
    void update_sidebar(bool force_update = false);
    void reset_window_layout();
    Sidebar::DockingState get_sidebar_docking_state();

    void set_current_canvas_as_dirty();
    GLCanvas3D* get_current_canvas3D(bool exclude_preview = false);
    void unbind_canvas_event_handlers();
    void reset_canvas_volumes();

    // BBS
    bool init_collapse_toolbar();

    void hide_send_to_printer_dlg() {  }

    void update_preview_bottom_toolbar();

    void reset_gcode_toolpaths();

    void reset_all_gizmos();
    void apply_free_camera_correction(bool apply = true);
    void update_ui_from_settings();
    // BBS
    std::string get_config(const std::string &key) const;

    // BBS: backup & restore
    std::vector<size_t> load_files(const std::vector<fs::path>& input_files, LoadStrategy strategy, bool ask_multi = false);
    std::vector<size_t> load_model_objects(const ModelObjectPtrs& model_objects, bool allow_negative_z = false, bool split_object = false);

    fs::path get_export_file_path(Slic3r::FileType file_type);
    wxString get_export_file(Slic3r::FileType file_type);

    const Selection& get_selection() const;
    Selection& get_selection();
    Selection* get_selection_ptr();

    int get_selected_object_idx() const;
    int get_selected_volume_idx() const;
    void selection_changed();
    void object_list_changed();

    // BBS
    void select_curr_plate_all();
    void remove_curr_plate_all();

    void select_all();
    void deselect_all();
    void exit_gizmo();
    void remove(size_t obj_idx);
    bool delete_object_from_model(size_t obj_idx, bool refresh_immediately = true); //BBS
    void delete_all_objects_from_model();
    void reset(bool apply_presets_change = false);
    void center_selection();
    void drop_selection();
    void mirror(Axis axis);
    void split_object();
    void split_volume();
    void scale_selection_to_fit_print_volume();

    // Return the active Undo/Redo stack. It may be either the main stack or the Gimzo stack.
    Slic3r::UndoRedo::Stack& undo_redo_stack() { assert(m_undo_redo_stack_active != nullptr); return *m_undo_redo_stack_active; }
    Slic3r::UndoRedo::Stack& undo_redo_stack_main() { return m_undo_redo_stack_main; }
    void enter_gizmos_stack();
    bool leave_gizmos_stack();

    void take_snapshot(const std::string& snapshot_name, UndoRedo::SnapshotType snapshot_type = UndoRedo::SnapshotType::Action);
    /*void take_snapshot(const wxString& snapshot_name, UndoRedo::SnapshotType snapshot_type = UndoRedo::SnapshotType::Action)
        { this->take_snapshot(std::string(snapshot_name.ToUTF8().data()), snapshot_type); }*/
    int  get_active_snapshot_index();

    void undo();
    void redo();
    void undo_redo_to(size_t time_to_load);

    // BBS: backup
    bool up_to_date(bool saved, bool backup);

    void suppress_snapshots()   { m_prevent_snapshots++; }
    void allow_snapshots()      { m_prevent_snapshots--; }
    // BBS: single snapshot
    void single_snapshots_enter(SingleSnapshot *single)
    {
        if (m_single == nullptr) m_single = single;
    }
    void single_snapshots_leave(SingleSnapshot *single)
    {
        if (m_single == single) m_single = nullptr;
    }

    void process_validation_warning(StringObjectException const &warning) const;

    bool background_processing_enabled() const {
#ifdef SUPPORT_BACKGROUND_PROCESSING
        return this->get_config("background_processing") == "1";
#else
        return false;
#endif
    }

    void schedule_background_process();
    // Update background processing thread from the current config and Model.
    enum UpdateBackgroundProcessReturnState {
        // that the background process was invalidated and it needs to be re-run.
        UPDATE_BACKGROUND_PROCESS_RESTART = 1,

        // that a scene needs to be refreshed (you should call _3DScene::reload_scene(canvas3Dwidget, false))
        UPDATE_BACKGROUND_PROCESS_REFRESH_SCENE = 2,
        // was sent to the status line.
        UPDATE_BACKGROUND_PROCESS_INVALID = 4,
        // Restart even if the background processing is disabled.
        UPDATE_BACKGROUND_PROCESS_FORCE_RESTART = 8,
        // Restart for G-code (or SLA zip) export or upload.
        UPDATE_BACKGROUND_PROCESS_FORCE_EXPORT = 16,
    };
    // returns bit mask of UpdateBackgroundProcessReturnState
    unsigned int update_background_process(bool force_validation = false);
    // Restart background processing thread based on a bitmask of UpdateBackgroundProcessReturnState.
    bool restart_background_process(unsigned int state);
    // returns bit mask of UpdateBackgroundProcessReturnState
    unsigned int update_restart_background_process(bool force_scene_update, bool force_preview_update);

    void reload_from_disk();
    bool replace_volume_with_stl(int object_idx, int volume_idx, const fs::path& new_path, const std::string& snapshot = "");
    void replace_with_stl();
    void reload_all_from_disk();

    void select_view3d(bool no_slice = true);
    void select_preview(bool no_slice = true);
    void select_view_3D(const std::string& name, bool no_slice = true);   // interface
    void set_current_panel(wxPanel* panel, bool no_slice = true);

    void on_combobox_select(wxCommandEvent&);
    void on_select_bed_type(wxCommandEvent&);
    void on_select_preset(wxCommandEvent&);
    void on_slicing_update(SlicingStatusEvent&);
    void on_slicing_completed(wxCommandEvent&);
    void on_process_completed(SlicingProcessCompletedEvent&);
    void on_export_began(wxCommandEvent&);
    void on_export_finished(wxCommandEvent&);
    void on_slicing_began();

    void clear_warnings();
    void add_warning(const Slic3r::PrintStateBase::Warning &warning, size_t oid);
    // Update notification manager with the current state of warnings produced by the background process (slicing).
    void actualize_slicing_warnings(const PrintBase &print);
    void actualize_object_warnings(const PrintBase& print);
    // Displays dialog window with list of warnings.
    // Returns true if user clicks OK.
    // Returns true if current_warnings vector is empty without showning the dialog
    bool warnings_dialog();

    void on_action_add(SimpleEvent&);
    void on_action_add_plate(SimpleEvent&);
    void on_action_del_plate(SimpleEvent&);
    void on_action_split_objects(SimpleEvent&);
    void on_action_split_volumes(SimpleEvent&);
    void on_action_layersediting(SimpleEvent&);
    void on_create_filament(SimpleEvent &);
    void on_modify_filament(SimpleEvent &);
    void on_add_filament(SimpleEvent &);
    void on_delete_filament(SimpleEvent &);
    void on_add_custom_filament(ColorEvent &);

    void on_object_select(SimpleEvent&);
    void show_right_click_menu(Vec2d mouse_position, wxMenu *menu);
    void on_right_click(RBtnEvent&);
    //BBS: add model repair
    void on_repair_model(wxCommandEvent &event);
    void on_filament_color_changed(wxCommandEvent &event);
    void show_preview_only_hint(wxCommandEvent &event);
    //BBS: add part plate related logic
    void on_plate_right_click(RBtnPlateEvent&);
    void on_plate_selected(SimpleEvent&);
    void on_action_request_model_id(wxCommandEvent& evt);
    void on_slice_button_status(bool enable);
    //BBS: GUI refactor: GLToolbar
    void on_action_open_project(SimpleEvent&);
    void on_action_slice_plate(SimpleEvent&);
    void on_action_slice_all(SimpleEvent&);
    void on_action_print_plate(SimpleEvent&);
    void on_action_print_all(SimpleEvent&);
    void on_action_export_gcode(SimpleEvent&);
    void on_action_send_gcode(SimpleEvent&);
    void on_action_export_sliced_file(SimpleEvent&);
    void on_action_export_all_sliced_file(SimpleEvent&);
    void on_action_select_sliced_plate(wxCommandEvent& evt);
    //BBS: change dark/light mode
    void on_change_color_mode(SimpleEvent& evt);
    void on_apple_change_color_mode(wxSysColourChangedEvent& evt);
    void apply_color_mode();
    void on_update_geometry(Vec3dsEvent<2>&);
    void on_3dcanvas_mouse_dragging_started(SimpleEvent&);
    void on_3dcanvas_mouse_dragging_finished(SimpleEvent&);

    void on_tab_selection_changing(wxBookCtrlEvent&);

    void update_bed_shape();
    void on_bed_updated();
    void set_bed_shape(const std::vector<Pointfs> & shape, const std::vector<Pointfs> & exclude_areas, const double printable_height, const std::string& custom_texture, const std::string& custom_model, bool force_as_custom = false);

    bool can_delete() const;
    bool can_delete_all() const;
    bool can_add_plate() const;
    bool can_delete_plate() const;
    bool can_increase_instances() const;
    bool can_decrease_instances() const;
    bool can_split_to_objects() const;
    bool can_split_to_volumes() const;
    bool can_arrange() const;
    bool can_layers_editing() const;
    bool can_fix_through_netfabb() const;
    bool can_simplify() const;
    bool can_set_instance_to_object() const;
    bool can_mirror() const;
    bool can_reload_from_disk() const;
    //BBS:
    bool can_fillcolor() const;
    bool can_replace_with_stl() const;
    bool can_split(bool to_objects) const;
    bool can_scale_to_print_volume() const;

    Bed3D* get_bed();
    
    //BBS: add plate_id for thumbnail
    void generate_thumbnail(ThumbnailData& data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params,
        Camera::EType camera_type, bool use_top_view = false, bool for_picking = false,bool ban_light = false);
    ThumbnailsList generate_thumbnails(const ThumbnailsParams& params, Camera::EType camera_type);
    //BBS
    void generate_calibration_thumbnail(ThumbnailData& data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params);
    PlateBBoxData generate_first_layer_bbox();

    void bring_instance_forward() const;

    // returns the path to project file with the given extension (none if extension == wxEmptyString)
    // extension should contain the leading dot, i.e.: ".3mf"
    wxString get_project_filename(const wxString& extension = wxEmptyString) const;
    wxString get_export_gcode_filename(const wxString& extension = wxEmptyString, bool only_filename = false, bool export_all = false) const;
    void set_project_filename(const wxString& filename);

    //BBS store bbs project name
    wxString get_project_name();
    void set_project_name(const wxString& project_name);
    void update_title_dirty_status();

    // Call after plater and Canvas#D is initialized
    void init_notification_manager();

    // Caching last value of show_action_buttons parameter for show_action_buttons(), so that a callback which does not know this state will not override it.
    //mutable bool    			ready_to_slice = { false };
    // Flag indicating that the G-code export targets a removable device, therefore the show_action_buttons() needs to be called at any case when the background processing finishes.
    ExportingStatus             exporting_status { NOT_EXPORTING };

    bool                        inside_snapshot_capture() { return m_prevent_snapshots != 0; }
    int                         process_completed_with_error { -1 }; //-1 means no error

    //BBS: project
    BBLProject                  project;

    //BBS: add print project related logic
    void update_fff_scene_only_shells(bool only_shells = true);
    //BBS: add popup object table logic
    bool PopupObjectTable(int object_id, int volume_id, const wxPoint& position);
    void on_action_send_to_multi_machine(SimpleEvent&);

    void update_gizmos_on_off_state();
private:
    void do_reslice(bool no_slice);

    bool layers_height_allowed() const;

    void update_fff_scene();
    void update_sla_scene();

    void undo_redo_to(std::vector<UndoRedo::Snapshot>::const_iterator it_snapshot);
    void update_after_undo_redo(const UndoRedo::Snapshot& snapshot, bool temp_snapshot_was_taken = false);
    void update_plugin_when_launch(wxCommandEvent& event);
    // path to project folder stored with no extension
    boost::filesystem::path     m_project_folder;

    /* display project name */
    wxString                    m_project_name;

    Slic3r::UndoRedo::Stack 	m_undo_redo_stack_main;
    Slic3r::UndoRedo::Stack 	m_undo_redo_stack_gizmos;
    Slic3r::UndoRedo::Stack    *m_undo_redo_stack_active = &m_undo_redo_stack_main;
    int                         m_prevent_snapshots = 0;     /* Used for avoid of excess "snapshoting".
                                                              * Like for "delete selected" or "set numbers of copies"
                                                              * we should call tack_snapshot just ones
                                                              * instead of calls for each action separately
                                                              * */
    // BBS: single snapshot
    Plater::SingleSnapshot     *m_single = nullptr;
    // BBS: backup
    size_t m_saved_timestamp = 0;
    size_t m_backup_timestamp = 0;
    std::string 				m_last_fff_printer_profile_name;
    std::string 				m_last_sla_printer_profile_name;

    // vector of all warnings generated by last slicing
    std::vector<std::pair<Slic3r::PrintStateBase::Warning, size_t>> current_warnings;
    bool show_warning_dialog { false };

    //record print preset
    void record_start_print_preset(std::string action);
};

}
}
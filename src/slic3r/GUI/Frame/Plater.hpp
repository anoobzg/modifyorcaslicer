#ifndef slic3r_Plater_hpp_
#define slic3r_Plater_hpp_

#include <memory>
#include <vector>
#include <boost/filesystem/path.hpp>

#include <wx/panel.h>
// BBS
#include <wx/notebook.h>

#include "slic3r/Render/Selection.hpp"

#include "libslic3r/enum_bitmask.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/BoundingBox.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "slic3r/GUI/Jobs/Job.hpp"
#include "slic3r/GUI/Jobs/Worker.hpp"
#include "slic3r/Config/Search.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/PrintBase.hpp"
#include "slic3r/Render/3DScene.hpp"
#include "libslic3r/calib.hpp"
#include "libslic3r/CutUtils.hpp"
#include "libslic3r/FlushVolCalc.hpp"

#define FILAMENT_SYSTEM_COLORS_NUM      16

class wxButton;
class ScalableButton;
class wxScrolledWindow;
class wxString;
class wxGLCanvas;

namespace Slic3r {

class BuildVolume;
enum class BuildVolume_Type : unsigned char;
class Model;
class ModelObject;
class MachineObject;
class ModelInstance;
class Print;
class SLAPrint;
class PartPlateList;
enum SLAPrintObjectStep : unsigned int;
enum class ConversionType : int;
class Ams;

using ModelInstancePtrs = std::vector<ModelInstance*>;


namespace UndoRedo {
    class Stack;
    enum class SnapshotType : unsigned char;
    struct Snapshot;
}

namespace GUI {
class SceneRaycaster;
class GLGizmosManager;
class SimpleEvent;
class MainPanel;
class ConfigOptionsGroup;
class ObjectSettings;
class ObjectLayers;
class ObjectList;
class GLCanvas3D;
class GCodePreviewCanvas;
class Mouse3DController;
class NotificationManager;
class DailyTipsWindow;
struct Camera;
class GLToolbar;
class PlaterPresetComboBox;
class PartPlateList;
class ComboBox;
using t_optgroups = std::vector <std::shared_ptr<ConfigOptionsGroup>>;
class GCodeResultWrapper;
class Plater;
class GLCanvas3DFacade;
class Bed3D;
enum class ActionButtonType : int;

const wxString DEFAULT_PROJECT_NAME = "Untitled";

class SidebarProps
{
public:
    static int TitlebarMargin();
    static int ContentMargin();
    static int IconSpacing();
    static int ElementSpacing();
};

class Sidebar : public wxPanel
{
    ConfigOptionMode    m_mode;
public:
    enum DockingState
    {
        None, Left, Right
    };

    Sidebar(Plater *parent);
    Sidebar(Sidebar &&) = delete;
    Sidebar(const Sidebar &) = delete;
    Sidebar &operator=(Sidebar &&) = delete;
    Sidebar &operator=(const Sidebar &) = delete;
    ~Sidebar();

    void create_printer_preset();
    void init_filament_combo(PlaterPresetComboBox **combo, const int filament_idx);
    void remove_unused_filament_combos(const size_t current_extruder_count);
    void update_all_preset_comboboxes();
    void update_presets(Slic3r::Preset::Type preset_type);
    //BBS
    void update_presets_from_to(Slic3r::Preset::Type preset_type, std::string from, std::string to);

    void change_top_border_for_mode_sizer(bool increase_border);
    void msw_rescale();
    void sys_color_changed();
    void search();
    void jump_to_option(size_t selected);
    void jump_to_option(const std::string& opt_key, Preset::Type type, const std::wstring& category);
    // BBS. Add on_filaments_change() method.
    void on_filaments_change(size_t num_filaments);
    void add_filament();
    void delete_filament();
    void add_custom_filament(wxColour new_col);
    // BBS
    void on_bed_type_change(BedType bed_type);
    void load_ams_list(std::string const & device, MachineObject* obj);
    void sync_ams_list();
    // Orca
    void show_SEMM_buttons(bool bshow);
    void update_dynamic_filament_list();

    ObjectList*             obj_list();
    ObjectSettings*         obj_settings();
    ObjectLayers*           obj_layers();
    wxPanel*                scrolled_panel();
    wxPanel* print_panel();
    wxPanel* filament_panel();

    ConfigOptionsGroup*     og_freq_chng_params(const bool is_fff);
    wxButton*               get_wiping_dialog_button();

    void                    set_btn_label(const ActionButtonType btn_type, const wxString& label) const;
    bool                    show_reslice(bool show) const;
	bool                    show_export(bool show) const;
	bool                    show_send(bool show) const;
    bool                    show_eject(bool show)const;
	bool                    show_export_removable(bool show) const;
	bool                    get_eject_shown() const;
    bool                    is_multifilament();
    void                    update_mode();
    bool                    is_collapsed();
    void                    collapse(bool collapse);
    void                    update_searcher();
    void                    update_ui_from_settings();
	bool                    show_object_list(bool show) const;
    void                    finish_param_edit();
    void                    auto_calc_flushing_volumes(const int modify_id);
    void                    jump_to_object(ObjectDataViewModelNode* item);
    void                    can_search();
    void                    show_mode_sizer(bool show);

    std::vector<PlaterPresetComboBox*>&   combos_filament();
    Search::OptionsSearcher&        get_searcher();
    std::string&                    get_search_line();

private:
    struct priv;
    std::unique_ptr<priv> p;

    wxBoxSizer* m_scrolled_sizer = nullptr;
    ComboBox* m_bed_type_list = nullptr;
    ScalableButton* connection_btn = nullptr;
    ScalableButton* ams_btn = nullptr;
};

class Plater: public wxPanel
{
public:
    using fs_path = boost::filesystem::path;

    Plater(wxWindow *parent);
    Plater(Plater &&) = delete;
    Plater(const Plater &) = delete;
    Plater &operator=(Plater &&) = delete;
    Plater &operator=(const Plater &) = delete;
    ~Plater() = default;

    void init();
    
    bool Show(bool show = true);

    void create_printer_preset();
    void select_printer_preset(const std::string& preset_name);
    
    bool is_project_dirty() const;
    bool is_presets_dirty() const;
    void set_plater_dirty(bool is_dirty);
    void update_project_dirty_from_presets();
    int  save_project_if_dirty(const wxString& reason);
    void reset_project_dirty_after_save();
    void reset_project_dirty_initial_presets();
#if ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW
    void render_project_state_debug_window() const;
#endif // ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW

    Sidebar& sidebar();
    const Model& model() const;
    Model& model();

    Bed3D* bed();
    
    bool is_normal_devide_mode();
    GLVolumeCollection::ERenderMode render_mode();

    int new_project(bool skip_confirm = false, bool silent = false, const wxString& project_name = wxString());
    // BBS: save & backup
    void load_project(wxString const & filename = "", wxString const & originfile = "-");
    int save_project(bool saveAs = false);
    //BBS download project by project id
    void import_model_id(wxString download_info);
    // BBS: check snapshot
    bool up_to_date(bool saved, bool backup);

    bool open_3mf_file(const fs::path &file_path);
    int  get_3mf_file_count(std::vector<fs::path> paths);
    void add_file();
    void add_model(bool imperial_units = false, std::string fname = "");
    void import_zip_archive();
    void extract_config_from_project();
    void load_gcode();
    void load_gcode(const wxString& filename);
    void load_gcodes(const std::vector<wxString>& filenames);
    void reload_gcode_from_disk();
    void refresh_print();

    BuildVolume_Type get_build_volume_type() const;

    //BBS: add only gcode mode
    bool using_exported_file() { return m_exported_file; }
    void set_using_exported_file(bool exported_file) {
        m_exported_file = exported_file;
    }

    // BBS
    wxString get_project_name();
    void update_all_plate_thumbnails(bool force_update = false);
    void invalid_all_plate_thumbnails();
    void force_update_all_plate_thumbnails();

    static wxColour get_next_color_for_filament();
    static wxString get_slice_warning_string(GCodeProcessorResult::SliceWarning& warning);

    bool preview_zip_archive(const boost::filesystem::path& archive_path);

    // BBS: restore
    std::vector<size_t> load_files(const std::vector<boost::filesystem::path>& input_files, LoadStrategy strategy = LoadStrategy::LoadModel | LoadStrategy::LoadConfig,  bool ask_multi = false);
    // To be called when providing a list of files to the GUI slic3r on command line.
    std::vector<size_t> load_files(const std::vector<std::string>& input_files, LoadStrategy strategy = LoadStrategy::LoadModel | LoadStrategy::LoadConfig,  bool ask_multi = false);
    // to be called on drag and drop
    bool load_files(const wxArrayString& filenames);

    void update(bool conside_update_flag = false, bool force_background_processing_update = false);
    //BBS
    void object_list_changed();

    Worker& get_ui_job_worker();
    const Worker & get_ui_job_worker() const;

    void select_view(const std::string& direction);

    void select_view3d(bool no_slice = true);
    void select_preview(bool no_slice = true);

    void reload_paint_after_background_process_apply();
    bool is_preview_shown() const;
    bool is_preview_loaded() const;
    bool is_view3D_shown() const;

    bool are_view3D_labels_shown() const;
    void show_view3D_labels(bool show);

    bool is_view3D_overhang_shown() const;
    void show_view3D_overhang(bool show);

    bool is_sidebar_enabled() const;
    void enable_sidebar(bool enabled);
    bool is_sidebar_collapsed() const;
    void collapse_sidebar(bool collapse);
    Sidebar::DockingState get_sidebar_docking_state() const;

    void reset_window_layout();

    // Called after the Preferences dialog is closed and the program settings are saved.
    // Update the UI based on the current preferences.
    void update_ui_from_settings();

    //BBS
    void select_curr_plate_all();
    void remove_curr_plate_all();

    void select_all();
    void deselect_all();
    void exit_gizmo();
    void remove(size_t obj_idx);
    void reset(bool apply_presets_change = false);
    void reset_with_confirm();
    //BBS: return int for various result
    int close_with_confirm(std::function<bool(bool yes_or_no)> second_check = nullptr); // BBS close project
    //BBS: trigger a restore project event
    void trigger_restore_project(int skip_confirm = 0);
    bool delete_object_from_model(size_t obj_idx, bool refresh_immediately = true); // BBS support refresh immediately
    void delete_all_objects_from_model(); //BBS delete all objects from model
    void set_selected_visible(bool visible);
    void remove_selected();
    void fill_bed_with_instances();
    bool is_selection_empty() const;
    void scale_selection_to_fit_print_volume();
    void convert_unit(ConversionType conv_type);
    // BBS: replace z with plane_points
    void cut(size_t obj_idx, size_t instance_idx, std::array<Vec3d, 4> plane_points, ModelObjectCutAttributes attributes);

    // BBS: segment model with CGAL
    void segment(size_t obj_idx, size_t instance_idx, double smoothing_alpha=0.5, int segment_number=5);
    void apply_cut_object_to_model(size_t init_obj_idx, const ModelObjectPtrs& cut_objects);
    void merge(size_t obj_idx, std::vector<int> &vol_indeces);

    void export_gcode(bool prefer_removable);
    void export_gcode_3mf(bool export_all = false);
    void send_gcode_finish(wxString name);
    void export_core_3mf();
    void export_stl(bool extended = false, bool selection_only = false, bool multi_stls = false);
    //BBS: remove amf
    //void export_amf();
    //BBS add extra param for exporting 3mf silence
    // BBS: backup
    int export_3mf(const boost::filesystem::path& output_path = boost::filesystem::path(), SaveStrategy strategy = SaveStrategy::Default, int export_plate_idx = -1, Export3mfProgressFn proFn = nullptr);

    //BBS
    void publish_project();

    void reload_from_disk();
    void replace_with_stl();
    void reload_all_from_disk();
    bool has_toolpaths_to_export() const;
    void export_toolpaths_to_obj() const;
    void reslice();

    void clear_before_change_mesh(int obj_idx);
    void changed_mesh(int obj_idx);

    void changed_object(ModelObject &object);
    void changed_object(int obj_idx);
    void changed_objects(const std::vector<size_t>& object_idxs);
    void schedule_background_process(bool schedule = true);
    bool is_background_process_update_scheduled() const;
    void suppress_background_process(const bool stop_background_process) ;
    void print_job_finished(wxCommandEvent &evt);
    void send_job_finished(wxCommandEvent& evt);
    void publish_job_finished(wxCommandEvent& evt);
    void open_platesettings_dialog(wxCommandEvent& evt);
    void on_change_color_mode(SimpleEvent& evt);
	void eject_drive();

    void take_snapshot(const std::string &snapshot_name);
    //void take_snapshot(const wxString &snapshot_name);
    void take_snapshot(const std::string &snapshot_name, UndoRedo::SnapshotType snapshot_type);
    //void take_snapshot(const wxString &snapshot_name, UndoRedo::SnapshotType snapshot_type);

    void undo();
    void redo();
    void undo_to(int selection);
    void redo_to(int selection);
    bool undo_redo_string_getter(const bool is_undo, int idx, const char** out_text);
    void undo_redo_topmost_string_getter(const bool is_undo, std::string& out_text);
    bool search_string_getter(int idx, const char** label, const char** tooltip);
    // For the memory statistics.
    const Slic3r::UndoRedo::Stack& undo_redo_stack_main() const;
    void clear_undo_redo_stack_main();
    // Enter / leave the Gizmos specific Undo / Redo stack. To be used by the SLA support point editing gizmo.
    void enter_gizmos_stack();
    // BBS: return false if not changed
    bool leave_gizmos_stack();

    void on_filaments_change(size_t extruders_count);
    // BBS
    void on_bed_type_change(BedType bed_type);
    bool update_filament_colors_in_full_config();
    void config_change_notification(const DynamicPrintConfig &config, const std::string& key);
    void on_config_change(const DynamicPrintConfig &config);
    void force_print_bed_update();
    std::vector<std::string> get_extruder_colors_from_plater_config() const;
    std::vector<std::string> get_colors_for_color_print() const;

    void update_menus();
    // BBS
    //void show_action_buttons(const bool is_ready_to_slice) const;

    wxString get_project_filename(const wxString& extension = wxEmptyString) const;
    wxString get_export_gcode_filename(const wxString& extension = wxEmptyString, bool only_filename = false, bool export_all = false) const;
    void set_project_filename(const wxString& filename);
    void update_print_error_info(int code, std::string msg, std::string extra);

    const Selection& get_selection() const;
    Selection& get_selection();
    Selection* get_selection_ptr();
    GLGizmosManager* get_gizmos_manager();
    SceneRaycaster* get_scene_raycaster();

    int get_selected_object_idx();
    bool is_single_full_object_selection() const;
    GLCanvas3D* canvas3D();
    const GLCanvas3D * canvas3D() const;
    GLCanvas3D* get_current_canvas3D(bool exclude_preview = false);
    GLCanvas3D* get_view3D_canvas3D();
    GCodePreviewCanvas* get_preview_canvas3D();
    GLCanvas3DFacade* canvas_facade();

    wxWindow* get_select_machine_dialog();

    void arrange();
    void orient();
    void find_new_position(const ModelInstancePtrs  &instances);
    //BBS: add job state related functions
    void set_prepare_state(int state);
    int get_prepare_state();

    void set_current_canvas_as_dirty();
    void unbind_canvas_event_handlers();
    void reset_canvas_volumes();

    PrinterTechnology   printer_technology() const;
    const DynamicPrintConfig * config() const;
    bool                set_printer_technology(PrinterTechnology printer_technology);

    //BBS
    void cut_selection_to_clipboard();

    void copy_selection_to_clipboard();
    void paste_from_clipboard();
    //BBS: add clone logic
    void clone_selection();
    void center_selection();
    void drop_selection();
    void search(bool plater_is_active, Preset::Type  type, wxWindow *tag, TextInput *etag, wxWindow *stag);
    void mirror(Axis axis);
    void split_object();
    void split_volume();
    void optimize_rotation();
    // find all empty cells on the plate and won't overlap with exclusion areas
    static std::vector<Vec2f> get_empty_cells(const Vec2f step);

    //BBS:
    void fill_color(int extruder_id);

    bool can_delete() const;
    bool can_delete_all() const;
    bool can_add_model() const;
    bool can_add_plate() const;
    bool can_delete_plate() const;
    bool can_increase_instances() const;
    bool can_decrease_instances() const;
    bool can_set_instance_to_object() const;
    bool can_fix_through_netfabb() const;
    bool can_simplify() const;
    bool can_split_to_objects() const;
    bool can_split_to_volumes() const;
    bool can_arrange() const;
    //BBS
    bool can_cut_to_clipboard() const;
    bool can_layers_editing() const;
    bool can_paste_from_clipboard() const;
    bool can_copy_to_clipboard() const;
    bool can_undo() const;
    bool can_redo() const;
    bool can_reload_from_disk() const;
    bool can_replace_with_stl() const;
    bool can_mirror() const;
    bool can_split(bool to_objects) const;
    bool can_scale_to_print_volume() const;

    //BBS:
    bool can_fillcolor() const;
    bool has_assmeble_view() const;

    void msw_rescale();
    void sys_color_changed();

    bool init_collapse_toolbar();

    const Camera& get_camera() const;
    Camera& get_camera();
    Camera* get_camera_ptr();

    //BBS: partplate list related functions
    PartPlateList& get_partplate_list();
    void validate_current_plate(bool& model_fits, bool& validate_error);
    //BBS: select the plate by index
    int select_plate(int plate_index, bool need_slice = false);
    //BBS: update progress result
    void apply_background_progress();
    //BBS: select the plate by hover_id
    int select_plate_by_hover_id(int hover_id, bool right_click = false, bool isModidyPlateName = false);
    //BBS: delete the plate, index= -1 means the current plate
    int delete_plate(int plate_index = -1);
    int duplicate_plate(int plate_index = -1);
    //BBS: select the sliced plate by index
    int select_sliced_plate(int plate_index);
    //BBS: is the background process slicing currently
    bool is_background_process_slicing() const;
    //BBS: update slicing context
    void update_slicing_context_to_current_partplate();
    //BBS: show object info
    void show_object_info();
    //BBS: post process string object exception strings by warning types
    void post_process_string_object_exception(StringObjectException &err);

#if ENABLE_ENVIRONMENT_MAP
    void init_environment_texture();
    unsigned int get_environment_texture_id() const;
#endif // ENABLE_ENVIRONMENT_MAP

    const BuildVolume& build_volume() const;

    const GLToolbar& get_collapse_toolbar() const;
    GLToolbar& get_collapse_toolbar();

    void update_preview_bottom_toolbar();
    void update_preview_moves_slider();
    void enable_preview_moves_slider(bool enable);

    void reset_gcode_toolpaths();

    const Mouse3DController& get_mouse3d_controller() const;
    Mouse3DController& get_mouse3d_controller();

    //BBS: add bed exclude area
	void set_bed_shape() const;
    void on_bed_updated();

	const NotificationManager* get_notification_manager() const;
	NotificationManager* get_notification_manager();
    DailyTipsWindow* get_dailytips() const;
    //BBS: show message in status bar
    void show_status_message(std::string s);

    void init_notification_manager();
    void set_notification_manager();

    void bring_instance_forward();

    bool need_update() const;
    void set_need_update(bool need_update);
    void update_title_dirty_status();

    void update_gizmos_on_off_state();
    void reset_all_gizmos();

    // ROII wrapper for suppressing the Undo / Redo snapshot to be taken.
	class SuppressSnapshots
	{
	public:
		SuppressSnapshots(Plater *plater) : m_plater(plater)
		{
			m_plater->suppress_snapshots();
		}
		~SuppressSnapshots()
		{
			m_plater->allow_snapshots();
		}
	private:
		Plater *m_plater;
	};

    // RAII wrapper for taking an Undo / Redo snapshot while disabling the snapshot taking by the methods called from inside this snapshot.
	class TakeSnapshot
	{
	public:
        TakeSnapshot(Plater *plater, const std::string &snapshot_name) : m_plater(plater)
        {
			m_plater->take_snapshot(snapshot_name);
			m_plater->suppress_snapshots();
		}
		/*TakeSnapshot(Plater *plater, const wxString &snapshot_name) : m_plater(plater)
		{
			m_plater->take_snapshot(snapshot_name);
			m_plater->suppress_snapshots();
		}*/
        TakeSnapshot(Plater* plater, const std::string& snapshot_name, UndoRedo::SnapshotType snapshot_type) : m_plater(plater)
        {
            m_plater->take_snapshot(snapshot_name, snapshot_type);
            m_plater->suppress_snapshots();
        }
        /*TakeSnapshot(Plater *plater, const wxString &snapshot_name, UndoRedo::SnapshotType snapshot_type) : m_plater(plater)
        {
            m_plater->take_snapshot(snapshot_name, snapshot_type);
            m_plater->suppress_snapshots();
        }*/

		~TakeSnapshot()
		{
			m_plater->allow_snapshots();
		}
	private:
		Plater *m_plater;
	};

    // BBS: limit to single snapshot taking by the methods called from inside
    // this snapshot.
    class SingleSnapshot
    {
    public:
        SingleSnapshot(Plater *plater) : m_plater(plater)
        {
            m_plater->single_snapshots_enter(this);
        }

        ~SingleSnapshot() { m_plater->single_snapshots_leave(this); }

        bool check(bool modify)
        {
            if (token && (this->modify || !modify)) return false;
            token = true;
            this->modify = modify;
            return true;
        }

    private:
        Plater *m_plater;
        bool    token = false;
        bool    modify = false;
    };

    bool inside_snapshot_capture();

    void toggle_show_wireframe();
    bool is_show_wireframe() const;
    void enable_wireframe(bool status);
    bool is_wireframe_enabled() const;

	// Wrapper around wxWindow::PopupMenu to suppress error messages popping out while tracking the popup menu.
	bool PopupMenu(wxMenu *menu, const wxPoint& pos = wxDefaultPosition);
    bool PopupMenu(wxMenu *menu, int x, int y) { return this->PopupMenu(menu, wxPoint(x, y)); }

    //BBS: add popup logic for table object
    bool PopupObjectTable(int object_id, int volume_id, const wxPoint& position);
    //BBS: popup selection at default position
    bool PopupObjectTableBySelection();

    // get same Plater/ObjectList menus
    wxMenu* plate_menu();
    wxMenu* object_menu();
    wxMenu* part_menu();
    wxMenu* text_part_menu();
    wxMenu* svg_part_menu();
    wxMenu* sla_object_menu();
    wxMenu* default_menu();
    wxMenu* instance_menu();
    wxMenu* layer_menu();
    wxMenu* multi_selection_menu();
    int     GetPlateIndexByRightMenuInLeftUI();
    void    SetPlateIndexByRightMenuInLeftUI(int);

    static void show_illegal_characters_warning(wxWindow* parent);

    std::string get_preview_only_filename() { return m_preview_only_filename; };

    bool last_arrange_job_is_finished()
    {
        bool prevRunning = false;
        return m_arrange_running.compare_exchange_strong(prevRunning, true);
    };
    std::atomic<bool> m_arrange_running{false};

    bool is_loading_project() const { return m_loading_project; }

private:
    struct priv;
    std::unique_ptr<priv> p;

    // Set true during PopupMenu() tracking to suppress immediate error message boxes.
    // The error messages are collected to m_tracking_popup_menu_error_message instead and these error messages
    // are shown after the pop-up dialog closes.
    bool 	 m_tracking_popup_menu = false;
    wxString m_tracking_popup_menu_error_message;

    wxString m_last_loaded_gcode;
    bool m_exported_file { false };
    bool skip_thumbnail_invalid { false };
    bool m_loading_project { false };
    std::string m_preview_only_filename;
    int m_valid_plates_count { 0 };

    void suppress_snapshots();
    void allow_snapshots();
    // BBS: single snapshot
    void single_snapshots_enter(SingleSnapshot *single);
    void single_snapshots_leave(SingleSnapshot *single);
    // BBS: add project slice related functions
    int start_next_slice();

    void cut_horizontal(size_t obj_idx, size_t instance_idx, double z, ModelObjectCutAttributes attributes);

    friend class SuppressBackgroundProcessingUpdate;
    friend class PlaterDropTarget;

public:
    // SoftFever Calibration
    void calib_pa(const Calib_Params& params);
    void calib_flowrate(bool is_linear, int pass);
    void calib_temp(const Calib_Params& params);
    void calib_max_vol_speed(const Calib_Params& params);
    void calib_retraction(const Calib_Params& params);
    void calib_VFA(const Calib_Params& params);
    void calib_multi_nozzle(const Calib_Params& params);

protected:
    void _calib_pa_pattern(const Calib_Params& params);
    void _calib_pa_pattern_gen_gcode();
    void _calib_pa_tower(const Calib_Params& params);
    void _calib_pa_select_added_objects();
};

class SuppressBackgroundProcessingUpdate
{
public:
    SuppressBackgroundProcessingUpdate();
    ~SuppressBackgroundProcessingUpdate();
private:
    bool m_was_scheduled;
};

} // namespace GUI
} // namespace Slic3r

#endif
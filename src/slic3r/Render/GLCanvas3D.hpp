#ifndef slic3r_GLCanvas3D_hpp_
#define slic3r_GLCanvas3D_hpp_

#include "slic3r/Scene/Camera.hpp"

#include "slic3r/Render/RenderHelpers.hpp"
#include "slic3r/Render/IMToolbar.hpp"

#include "GLToolbar.hpp"
#include "Selection.hpp"
#include "slic3r/GUI/Gizmos/GLGizmosManager.hpp"
#include "slic3r/GUI/Config/GUI_ConfigDef.hpp"
#include "GLSelectionRectangle.hpp"
#include "MeshUtils.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "SceneRaycaster.hpp"


#include "libslic3r/Slicing.hpp"
#include "EventHandler/GUIEventHandler.hpp"

class wxSizeEvent;
class wxIdleEvent;
class wxKeyEvent;
class wxMouseEvent;
class wxTimerEvent;
class wxPaintEvent;
class wxGLCanvas;
class wxGLContext;

// Support for Retina OpenGL on Mac OS.
// wxGTK3 seems to simulate OSX behavior in regard to HiDPI scaling support, enable it as well.
#define ENABLE_RETINA_GL (__APPLE__ || __WXGTK3__)

namespace Slic3r {

class BackgroundSlicingProcess;
class BuildVolume;
struct ThumbnailData;
struct ThumbnailsParams;
class ModelObject;
class ModelInstance;
class PrintObject;
class Print;
class SLAPrint;
class PresetBundle;
namespace CustomGCode { struct Item; }

namespace GUI {
class GCodeResultWrapper;
class PartPlateList;
class LayersEditing;

#if ENABLE_RETINA_GL
class RetinaHelper;
#endif

class GLCanvas3D : public HMS::GUIActionAdapte
{
    static const double DefaultCameraZoomToBoxMarginFactor;
    static const double DefaultCameraZoomToBedMarginFactor;
    static const double DefaultCameraZoomToPlateMarginFactor;

protected:
    enum class EWarning {
        ObjectOutside,
        ToolpathOutside,
        SlaSupportsOutside,
        SomethingNotShown,
        ObjectClashed,
        GCodeConflict,
        ToolHeightOutside
    };

public:
    enum ECursorType : unsigned char
    {
        Standard,
        Cross
    };

    struct ArrangeSettings
    {
        float distance           = 0.f;
//        float distance_sla       = 6.;
        float accuracy           = 0.65f; // Unused currently
        bool  enable_rotation    = false;
        bool  allow_multi_materials_on_same_plate = true;
        bool  avoid_extrusion_cali_region = true;
        //BBS: add more arrangeSettings
        bool is_seq_print        = false;
        bool  align_to_y_axis    = false;
    };

    struct OrientSettings
    {
        float overhang_angle = 60.f;
        bool  enable_rotation = false;
        bool  min_area = true;
    };

    //BBS: add canvas type for assemble view usage
    enum ECanvasType
    {
        CanvasView3D = 0,
        CanvasPreview = 1
    };

    int GetHoverId();

    bool m_is_dark = false;
    wxGLCanvas* m_canvas;
    wxGLContext* m_context;
    SceneRaycaster m_scene_raycaster;
#if ENABLE_RETINA_GL
    std::unique_ptr<RetinaHelper> m_retina_helper;
#endif
    unsigned int m_last_w, m_last_h;
    bool m_in_render;
    wxTimer m_timer;
    wxTimer m_timer_set_color;
    LayersEditing* m_layers_editing;
    MouseHelper m_mouse;
    GLGizmosManager* m_gizmos;
    //BBS: GUI refactor: GLToolbar
    mutable GLToolbar m_main_toolbar;
    mutable GLToolbar m_separator_toolbar;
    mutable IMToolbar m_sel_plate_toolbar;
    mutable float m_paint_toolbar_width;

    //BBS: add canvas type for assemble view usage
    ECanvasType m_canvas_type;
    std::array<ClippingPlane, 2> m_clipping_planes;
    ClippingPlane m_camera_clipping_plane;
    bool m_use_clipping_planes;

    std::string m_sidebar_field;
    // when true renders an extra frame by not resetting m_dirty to false
    // see request_extra_frame()
    bool m_extra_frame_requested;
    bool m_event_handlers_bound{ false };

    GLVolumeCollection m_volumes;

    RenderTimer m_render_timer;

    Selection* m_selection;
    const DynamicPrintConfig* m_config;
    Model* m_model;

    std::array<unsigned int, 2> m_old_size{ 0, 0 };

    bool m_is_touchpad_navigation{ false };

    // Screen is only refreshed from the OnIdle handler if it is dirty.
    bool m_initialized;
    //BBS: add flag to controll rendering
    bool m_render_preview{ true };
    bool m_enable_render { true };
    bool m_apply_zoom_to_volumes_filter;
    bool m_picking_enabled;
    bool m_moving_enabled;
    bool m_dynamic_background_enabled;
    bool m_multisample_allowed;
    bool m_moving;
    bool m_tab_down;
    bool m_camera_movement;
    //BBS: add toolpath outside
    bool m_toolpath_outside{ false };

    GLSelectionRectangle m_rectangle_selection;
    GLModel m_selection_center;

    //BBS:add plate related logic
    mutable std::vector<int> m_hover_volume_idxs;
    std::vector<int> m_hover_plate_idxs;
    //BBS if explosion_ratio is changed, need to update volume bounding box
    mutable float m_explosion_ratio = 1.0;
    mutable Vec3d m_rotation_center{ 0.0, 0.0, 0.0};
    //BBS store camera view
    Camera camera;

    // Following variable is obsolete and it should be safe to remove it.
    // I just don't want to do it now before a release (Lukas Matena 24.3.2019)
    bool m_render_sla_auxiliaries;

    std::string m_color_by;

    bool m_reload_delayed;

    RenderStats m_render_stats;

    int m_imgui_undo_redo_hovered_pos{ -1 };
    int m_mouse_wheel{ 0 };
    int m_selected_extruder;

    LabelsHelper m_labels;
    TooltipHelper m_tooltip;
    bool m_tooltip_enabled{ true };
    SlopeHelper m_slope;

    OrientSettings m_orient_settings_fff;

    ArrangeSettings m_arrange_settings_fff, m_arrange_settings_fff_seq_print;

    //BBS:record key botton frequency
    int auto_orient_count = 0;
    int auto_arrange_count = 0;
    int split_to_objects_count = 0;
    int split_to_part_count = 0;
    int custom_height_count = 0;
    int assembly_view_count = 0;

    HMS::EventManager  m_event_manager;

public:
    OrientSettings& get_orient_settings()
    {
        auto* ptr = &this->m_orient_settings_fff;
        return *ptr;
    }

    void load_arrange_settings();
    ArrangeSettings& get_arrange_settings();// { return get_arrange_settings(this); }
    ArrangeSettings& get_arrange_settings(PrintSequence print_seq) {
        return (print_seq == PrintSequence::ByObject) ? m_arrange_settings_fff_seq_print
            : m_arrange_settings_fff;
    }

    SequentialPrintClearance m_sequential_print_clearance;
    bool m_sequential_print_clearance_first_displacement{ true };


    ToolbarHighlighter m_toolbar_highlighter;
    GizmoHighlighter m_gizmo_highlighter;

#if ENABLE_SHOW_CAMERA_TARGET
    struct CameraTarget
    {
        std::array<GLModel, 3> axis;
        Vec3d target{ Vec3d::Zero() };
    };

    CameraTarget m_camera_target;
#endif // ENABLE_SHOW_CAMERA_TARGET
    GLModel m_background;
public:
    explicit GLCanvas3D(wxGLCanvas* canvas);
    virtual ~GLCanvas3D();

    bool is_initialized() const { return m_initialized; }

    void set_context(wxGLContext* context) { m_context = context; }
    void set_type(ECanvasType type) { m_canvas_type = type; }
    ECanvasType get_canvas_type() { return m_canvas_type; }

    wxGLCanvas* get_wxglcanvas() { return m_canvas; }
	const wxGLCanvas* get_wxglcanvas() const { return m_canvas; }

    bool init();
    void post_event(wxEvent &&event);
    void post_event(wxEvent *event);

    std::shared_ptr<SceneRaycasterItem> add_raycaster_for_picking(SceneRaycaster::EType type, int id, const MeshRaycaster& raycaster,
        const Transform3d& trafo = Transform3d::Identity(), bool use_back_faces = false) {
        return m_scene_raycaster.add_raycaster(type, id, raycaster, trafo, use_back_faces);
    }
    void remove_raycasters_for_picking(SceneRaycaster::EType type, int id) {
        m_scene_raycaster.remove_raycasters(type, id);
    }
    void remove_raycasters_for_picking(SceneRaycaster::EType type) {
        m_scene_raycaster.remove_raycasters(type);
    }

    std::vector<std::shared_ptr<SceneRaycasterItem>>* get_raycasters_for_picking(SceneRaycaster::EType type) {
        return m_scene_raycaster.get_raycasters(type);
    }

    void set_raycaster_gizmos_on_top(bool value) {
        m_scene_raycaster.set_gizmos_on_top(value);
    }

    float get_explosion_ratio() { return m_explosion_ratio; }
    void reset_explosion_ratio() { m_explosion_ratio = 1.0; }
    void on_change_color_mode(bool is_dark, bool reinit = true);
    const bool get_dark_mode_status() { return m_is_dark; }
    void set_as_dirty();

    unsigned int get_volumes_count() const;
    const GLVolumeCollection& get_volumes() const { return m_volumes; }
    void reset_volumes();
    ModelInstanceEPrintVolumeState check_volumes_outside_state() const;
    bool is_all_plates_selected() { return m_sel_plate_toolbar.m_all_plates_stats_item && m_sel_plate_toolbar.m_all_plates_stats_item->selected; }
    const float get_scale() const;

    void toggle_selected_volume_visibility(bool selected_visible);
    void toggle_model_objects_visibility(bool visible, const ModelObject* mo = nullptr, int instance_idx = -1, const ModelVolume* mv = nullptr);
    void update_instance_printable_state_for_object(size_t obj_idx);
    void update_instance_printable_state_for_objects(const std::vector<size_t>& object_idxs);

    void set_config(const DynamicPrintConfig* config);
    void set_process(BackgroundSlicingProcess* process);
    virtual void set_model(Model* model);
    const Model* get_model() const { return m_model; }

    // const Selection& get_selection() const { return m_selection; }
    // Selection& get_selection() { return m_selection; }
    virtual void set_selection(Selection* selection);
    Selection* get_selection() { return m_selection; }

    void set_gizmos_manager(GLGizmosManager* manager);
    GLGizmosManager* get_gizmos_manager();
    const GLGizmosManager* get_gizmos_manager() const;

    //void set_scene_raycaster(SceneRaycaster* raycaster);;
    SceneRaycaster* get_scene_raycaster();

    void bed_shape_changed();

    //BBS: add part plate related logic
    void plates_count_changed();

    //BBS get camera
    Camera& get_camera();

    void set_clipping_plane(unsigned int id, const ClippingPlane& plane)
    {
        if (id < 2)
        {
            m_clipping_planes[id] = plane;
        }
    }
    void set_use_clipping_planes(bool use) { m_use_clipping_planes = use; }

    bool                                get_use_clipping_planes() const { return m_use_clipping_planes; }
    const std::array<ClippingPlane, 2> &get_clipping_planes() const { return m_clipping_planes; };

    void set_use_color_clip_plane(bool use) { m_volumes.set_use_color_clip_plane(use); }
    void set_color_clip_plane(const Vec3d& cp_normal, double offset) { m_volumes.set_color_clip_plane(cp_normal, offset); }
    void set_color_clip_plane_colors(const std::array<ColorRGBA, 2>& colors) { m_volumes.set_color_clip_plane_colors(colors); }

    void refresh_camera_scene_box();
    void set_color_by(const std::string& value);

    BoundingBoxf3 volumes_bounding_box(bool current_plate_only = false) const;
    BoundingBoxf3 scene_bounding_box() const;
    BoundingBoxf3 plate_scene_bounding_box(int plate_idx) const;

    bool is_layers_editing_enabled() const;
    bool is_layers_editing_allowed() const;

    void reset_layer_height_profile();
    void adaptive_layer_height_profile(float quality_factor);
    void smooth_layer_height_profile(const HeightProfileSmoothingParams& smoothing_params);

    bool is_reload_delayed() const;

    void enable_layers_editing(bool enable);
    void enable_legend_texture(bool enable);
    void enable_picking(bool enable);
    void enable_moving(bool enable);
    void enable_selection(bool enable);
    void enable_main_toolbar(bool enable);
    //BBS: GUI refactor: GLToolbar
    void _update_select_plate_toolbar_stats_item(bool force_selected = false);
    void reset_select_plate_toolbar_selection();
    void enable_select_plate_toolbar(bool enable);
    void enable_separator_toolbar(bool enable);
    void enable_dynamic_background(bool enable);
    void enable_labels(bool enable) { m_labels.enable(enable); }
    void enable_slope(bool enable) { m_slope.enable(enable); }
    void allow_multisample(bool allow);

    void zoom_to_bed();
    void zoom_to_volumes();
    void zoom_to_selection();
    void zoom_to_gcode();
    //BBS -1 for current plate
    void zoom_to_plate(int plate_idx = -1);
    void select_view(const std::string& direction);
    //BBS: add part plate related logic
    void select_plate();
    //BBS: GUI refactor: GLToolbar&&gizmo
    int get_main_toolbar_offset() const;
    int get_main_toolbar_height() const { return m_main_toolbar.get_height(); }
    int get_main_toolbar_width() const { return m_main_toolbar.get_width(); }
    float get_separator_toolbar_width() const { return m_separator_toolbar.get_width(); }
    float get_separator_toolbar_height() const { return m_separator_toolbar.get_height(); }
    bool  is_collapse_toolbar_on_left() const;
    float get_collapse_toolbar_width() const;
    float get_collapse_toolbar_height() const;
    void  mirror_volumes();

    void update_volumes_colors_by_extruder();

    bool is_dragging() const { return m_gizmos->is_dragging() || m_moving; }

    virtual void render(bool only_init = false);
    bool is_rendering_enabled()
    {
        return m_enable_render;
    }
    void enable_render(bool enabled)
    {
        m_enable_render = enabled;
    }

    // printable_only == false -> render also non printable volumes as grayed
    void render_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params,
                                 Camera::EType           camera_type,
                                 bool                    use_top_view = false,
                                 bool                    for_picking  = false,
                                 bool                    ban_light    = false);
    void render_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params,
                                 const GLVolumeCollection &volumes,
                                 Camera::EType             camera_type,
                                 bool                      use_top_view = false,
                                 bool                      for_picking  = false,
                                 bool                      ban_light    = false);

 
    //BBS
    void update_plate_thumbnails();

    void ensure_on_bed(unsigned int object_idx, bool allow_negative_z);

    std::vector<double> get_volumes_print_zs(bool active_only) const;
    void set_volumes_z_range(const std::array<double, 2>& range);


    std::vector<int> load_object(const ModelObject& model_object, int obj_idx, std::vector<int> instance_idxs);
    std::vector<int> load_object(const Model& model, int obj_idx);


    virtual void reload_scene(bool refresh_immediately, bool force_full_scene_refresh = false);
    //Orca: shell preview improvement


    void bind_event_handlers();
    void unbind_event_handlers();

    void on_size(wxSizeEvent& evt);
    void on_idle(wxIdleEvent& evt);
    void on_char(wxKeyEvent& evt);
    void on_key(wxKeyEvent& evt);
    void on_mouse_wheel(wxMouseEvent& evt);
    void on_timer(wxTimerEvent& evt);
    void on_render_timer(wxTimerEvent& evt);
    void on_set_color_timer(wxTimerEvent& evt);
    void on_mouse(wxMouseEvent& evt);
    void on_gesture(wxGestureEvent& evt);
    void on_paint(wxPaintEvent& evt);
    void on_set_focus(wxFocusEvent& evt);
    void force_set_focus();

    bool is_camera_rotate(const wxMouseEvent& evt) const;
    bool is_camera_pan(const wxMouseEvent& evt) const;

    void rotate_gizmos_camera(const wxMouseEvent& evt);
    void rotate_camera(const wxMouseEvent& evt);
    void pan_camera(const wxMouseEvent& evt);
    void scale_camera(const wxMouseEvent& evt);

    void imgui_handle(const wxMouseEvent& evt);

    Vec3d rotate_center();
    Camera* get_camera_ptr();
    bool use_free_camera();
    MouseHelper* get_mouse();

    Size get_canvas_size() const;
    Vec2d get_local_mouse_position() const;

    // store opening position of menu
    std::optional<Vec2d> m_popup_menu_positon; // position of mouse right click
    void  set_popup_menu_position(const Vec2d &position) { m_popup_menu_positon = position; }
    const std::optional<Vec2d>& get_popup_menu_position() const { return m_popup_menu_positon; }
    void clear_popup_menu_position() { m_popup_menu_positon.reset(); }

    void set_tooltip(const std::string& tooltip);

    void handle_sidebar_focus_event(const std::string& opt_key, bool focus_on);
    void handle_layers_data_focus_event(const t_layer_height_range range, const EditorType type);

    void update_ui_from_settings();

    int get_move_volume_id() const { return m_mouse.drag.move_volume_idx; }
    int get_first_hover_volume_idx() const { return m_hover_volume_idxs.empty() ? -1 : m_hover_volume_idxs.front(); }
    void set_selected_extruder(int extruder) { m_selected_extruder = extruder;}
    void dirty()
    {
        m_dirty = true;
    }

    // BBS: add partplate logic
    WipeTowerInfoHelper get_wipe_tower_info(int plate_idx) const;

    // Returns the view ray line, in world coordinate, at the given mouse position.
    Linef3 mouse_ray(const Point& mouse_pos);

    void set_mouse_as_dragging() { m_mouse.dragging = true; }
    bool is_mouse_dragging() const { return m_mouse.dragging; }

    double get_size_proportional_to_max_bed_size(double factor) const;

    // BBS: get empty cells to put new object
    // start_point={-1,-1} means sort from bed center, step is the unscaled x,y stride
    std::vector<Vec2f> get_empty_cells(const Vec2f start_point, const Vec2f step = {10, 10});
    // BBS: get the nearest empty cell
    // start_point={-1,-1} means sort from bed center
    Vec2f get_nearest_empty_cell(const Vec2f start_point, const Vec2f step = {10, 10});

    void msw_rescale();

    void request_extra_frame() { m_extra_frame_requested = true; }

    void schedule_extra_frame(int miliseconds);

    int get_main_toolbar_item_id(const std::string& name) const { return m_main_toolbar.get_item_id(name); }
    void force_main_toolbar_left_action(int item_id) { m_main_toolbar.force_left_action(item_id, *this); }
    void force_main_toolbar_right_action(int item_id) { m_main_toolbar.force_right_action(item_id, *this); }

    void mouse_up_cleanup();

    bool are_labels_shown() const { return m_labels.is_shown(); }
    void show_labels(bool show) { m_labels.show(show); }

    bool is_overhang_shown() const { return m_slope.is_GlobalUsed(); }
    void show_overhang(bool show) { m_slope.globalUse(show); }
    
    bool is_using_slope() const { return m_slope.is_used(); }
    void use_slope(bool use) { m_slope.use(use); }
    void set_slope_normal_angle(float angle_in_deg) { m_slope.set_normal_angle(angle_in_deg); }

    void highlight_toolbar_item(const std::string& item_name);
    void highlight_gizmo(const std::string& gizmo_name);

    ArrangeSettings get_arrange_settings() const {
        const ArrangeSettings &settings = get_arrange_settings();
        ArrangeSettings ret = settings;
        if (&settings == &m_arrange_settings_fff_seq_print) {
            ret.distance = std::max(ret.distance,
                                    float(min_object_distance(*m_config)));
        }

        return ret;
    }

    // Timestamp for FPS calculation and notification fade-outs.
    static int64_t timestamp_now() {
#ifdef _WIN32
        // Cheaper on Windows, calls GetSystemTimeAsFileTime()
        return wxGetUTCTimeMillis().GetValue();
#else
        // calls clock()
        return wxGetLocalTimeMillis().GetValue();
#endif
    }

    void reset_sequential_print_clearance() {
        m_sequential_print_clearance.set_visible(false);
        m_sequential_print_clearance.set_render_fill(false);
        //BBS: add the height logic
        m_sequential_print_clearance.set_polygons(Polygons(), std::vector<std::pair<Polygon, float>>());
    }

    void set_sequential_print_clearance_visible(bool visible) {
        m_sequential_print_clearance.set_visible(visible);
    }

    void set_sequential_print_clearance_render_fill(bool render_fill) {
        m_sequential_print_clearance.set_render_fill(render_fill);
    }

    //BBS: add the height logic
    void set_sequential_print_clearance_polygons(const Polygons& polygons, const std::vector<std::pair<Polygon, float>>& height_polygons) {
        m_sequential_print_clearance.set_polygons(polygons, height_polygons);
    }

    void update_sequential_clearance();

    virtual const Print* fff_print() const;

    void reset_old_size() { m_old_size = { 0, 0 }; }

    bool is_object_sinking(int object_idx) const;

    void apply_retina_scale(Vec2d &screen_coordinate) const;

    void _perform_layer_editing_action(wxMouseEvent* evt = nullptr);

    // Convert the screen space coordinate to an object space coordinate.
    // If the Z screen space coordinate is not provided, a depth buffer value is substituted.
    Vec3d _mouse_to_3d(const Point& mouse_pos, float* z = nullptr);

    bool make_current_for_postinit();
protected:
    bool _is_shown_on_screen() const;

    void _switch_toolbars_icon_filename();
    bool _init_toolbars();
    bool _init_main_toolbar();
    bool _init_select_plate_toolbar();
    bool _update_imgui_select_plate_toolbar();
    bool _init_separator_toolbar();

    bool _init_collapse_toolbar();
    void _resize(unsigned int w, unsigned int h);

    //BBS: add part plate related logic
    BoundingBoxf3 _max_bounding_box(bool include_gizmos, bool include_bed_model, bool include_plates) const;

    void _update_camera_zoom(double zoom);

    void _refresh_if_shown_on_screen();

    void _picking_pass();
    void _rectangular_selection_picking_pass();
    void _render_background();
    void _render_bed(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool show_axes);
    //BBS: add part plate related logic
    void _render_platelist(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool only_current, bool only_body = false, int hover_id = -1, bool render_cali = false, bool show_grid = true);
    //BBS: add outline drawing logic
    void _render_volumes_for_picking(const Camera& camera) const;

    void _render_separator_toolbar_right() const;
    void _render_separator_toolbar_left() const;
    void _render_collapse_toolbar() const;
    // BBS
#if ENABLE_SHOW_CAMERA_TARGET
    void _render_camera_target();
#endif // ENABLE_SHOW_CAMERA_TARGET

    void _render_selection_sidebar_hints();
    //BBS: GUI refactor: adjust main toolbar position
    bool _render_orient_menu(float left, float right, float bottom, float top);
    bool _render_arrange_menu(float left, float right, float bottom, float top);
    void _render_3d_navigator();
    // render thumbnail using the default framebuffer
    void render_thumbnail_legacy(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params, PartPlateList& partplate_list, ModelObjectPtrs& model_objects, const GLVolumeCollection& volumes, std::vector<ColorRGBA>& extruder_colors, GLShaderProgram* shader, Camera::EType camera_type);

    void _update_volumes_hover_state();

    // Convert the screen space coordinate to world coordinate on the bed.
    Vec3d _mouse_to_bed_3d(const Point& mouse_pos);

    void _start_timer();
    void _stop_timer();

    void _set_warning_notification_if_needed(EWarning warning);

    //BBS: add partplate print volume get function
    BoundingBoxf3 _get_current_partplate_print_volume();

    // generates a warning notification containing the given message
    virtual void _set_warning_notification(EWarning warning, bool state);


    // updates the selection from the content of m_hover_volume_idxs
    void _update_selection_from_hover();

    bool _deactivate_collapse_toolbar_items();
    bool _deactivate_arrange_menu();
    //BBS: add deactivate_orient_menu
    bool _deactivate_orient_menu();
    //BBS: add _deactivate_layersediting_menu
    bool _deactivate_layersediting_menu();

    // BBS FIXME
    float get_overlay_window_width() { return 0; }

protected:
    virtual void _on_mouse(wxMouseEvent& evt);

    bool _is_any_volume_outside() const;
    bool _set_current();
    void _zoom_to_box(const BoundingBoxf3& box, double margin_factor = DefaultCameraZoomToBoxMarginFactor);

    BackgroundSlicingProcess *m_process;
    bool m_dirty;
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GLCanvas3D_hpp_

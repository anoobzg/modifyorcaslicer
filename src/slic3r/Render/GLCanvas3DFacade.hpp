#ifndef _slic3r_GLCanvas3DFacade_hpp
#define _slic3r_GLCanvas3DFacade_hpp

#include "libslic3r/libslic3r.h"
#include "MeshUtils.hpp"
#include "SceneRaycaster.hpp"
#include "RenderHelpers.hpp"
#include <wx/event.h>

namespace Slic3r {

class ModelVolume;
class ModelObject;
namespace GUI {

class View3DCanvas;
class GCodePreviewCanvas;
class Selection;
class GLGizmosManager;
class GLCanvas3DFacade
{
public:
    GLCanvas3DFacade(View3DCanvas* view3d, GCodePreviewCanvas* gcode_preview);

    void do_move(const std::string& snapshot_type);
    void do_rotate(const std::string& snapshot_type);
    void do_scale(const std::string& snapshot_type);
    void do_center();
    void do_drop();
    void do_center_plate(const int plate_idx);
    void do_mirror(const std::string& snapshot_type);
    void select_curr_plate_all();
    void select_object_from_idx(std::vector<int>& object_idxs);
    void remove_curr_plate_all();

    void set_selected_visible(bool visible);
    void select_all();
    void deselect_all();
    void exit_gizmo();
    void delete_selected();
    void mirror_selection(Axis axis);

    Selection* get_selection();
    GLGizmosManager* get_gizmos_manager();
    const Model* get_model();

    unsigned int get_volumes_count() const;
    const GLVolumeCollection& get_volumes() const;

    void set_as_dirty();

    std::shared_ptr<SceneRaycasterItem> add_raycaster_for_picking(SceneRaycaster::EType type, int id, const MeshRaycaster& raycaster,
        const Transform3d& trafo = Transform3d::Identity(), bool use_back_faces = false);
    void remove_raycasters_for_picking(SceneRaycaster::EType type, int id);
    void remove_raycasters_for_picking(SceneRaycaster::EType type);
    void set_raycaster_gizmos_on_top(bool value);

    Size get_canvas_size() const;
    int get_main_toolbar_offset() const;
    int get_main_toolbar_height() const;
    int get_main_toolbar_width() const;
    float get_separator_toolbar_width() const;
    float get_separator_toolbar_height() const;
    bool  is_collapse_toolbar_on_left() const;
    float get_collapse_toolbar_width() const;
    float get_collapse_toolbar_height() const;
    const float get_scale() const;

    void toggle_model_objects_visibility(bool visible, const ModelObject* mo = nullptr, int instance_idx = -1, const ModelVolume* mv = nullptr);

    int64_t timestamp_now();
    void schedule_extra_frame(int miliseconds);

    void handle_sidebar_focus_event(const std::string& opt_key, bool focus_on);

    BoundingBoxf3 volumes_bounding_box(bool current_plate_only = false) const;
    
    void set_use_clipping_planes(bool use);
    void set_clipping_plane(unsigned int id, const ClippingPlane& plane);

    bool is_mouse_dragging() const;
    Linef3 mouse_ray(const Point& mouse_pos);
    void post_event(wxEvent&& event);

    void enable_picking(bool enable);
    void enable_moving(bool enable);

    int get_first_hover_volume_idx() const;
    std::vector<std::shared_ptr<SceneRaycasterItem>>* get_raycasters_for_picking(SceneRaycaster::EType type);
    void mouse_up_cleanup();
    void refresh_camera_scene_box();
    void request_extra_frame();

private: 
    View3DCanvas* m_view3d { NULL };
    GCodePreviewCanvas* m_gcode_preview { NULL };

};

};
};



#endif
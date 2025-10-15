#include "GLCanvas3DFacade.hpp"
#include "View3DCanvas.hpp"
#include "GCodePreviewCanvas.hpp"

namespace Slic3r {
namespace GUI {


GLCanvas3DFacade::GLCanvas3DFacade(View3DCanvas* view3d, GCodePreviewCanvas* gcode_preview)
{
    m_view3d = view3d;
    m_gcode_preview = gcode_preview;
}

void GLCanvas3DFacade::do_move(const std::string& snapshot_type)
{
    if (m_view3d)
        m_view3d->do_move(snapshot_type);
}

void GLCanvas3DFacade::do_rotate(const std::string& snapshot_type)
{
    if (m_view3d)
        m_view3d->do_rotate(snapshot_type);
}

void GLCanvas3DFacade::do_scale(const std::string& snapshot_type)
{
    if (m_view3d)
        m_view3d->do_scale(snapshot_type);
}

void GLCanvas3DFacade::do_center()
{
    if (m_view3d)
        m_view3d->do_center();
}

void GLCanvas3DFacade::do_drop()
{
    if (m_view3d)
        m_view3d->do_drop();
}

void GLCanvas3DFacade::do_center_plate(const int plate_idx)
{
    if (m_view3d)
        m_view3d->do_center_plate(plate_idx);
}

void GLCanvas3DFacade::do_mirror(const std::string& snapshot_type)
{
    if (m_view3d)
        m_view3d->do_mirror(snapshot_type);
}

void GLCanvas3DFacade::select_curr_plate_all()
{
    if (m_view3d)
        m_view3d->select_curr_plate_all();
}

void GLCanvas3DFacade::select_object_from_idx(std::vector<int>& object_idxs)
{
    if (m_view3d)
        m_view3d->select_object_from_idx(object_idxs);
}

void GLCanvas3DFacade::remove_curr_plate_all()
{
    if (m_view3d)
        m_view3d->remove_curr_plate_all();
}

void GLCanvas3DFacade::set_selected_visible(bool visible)
{
    if (m_view3d)
        m_view3d->set_selected_visible(visible);
}

void GLCanvas3DFacade::select_all()
{
    if (m_view3d)
        m_view3d->select_all();
}

void GLCanvas3DFacade::deselect_all()
{
    if (m_view3d)
        m_view3d->deselect_all();
}

void GLCanvas3DFacade::exit_gizmo()
{
    if (m_view3d)
        m_view3d->exit_gizmo();
}

void GLCanvas3DFacade::delete_selected()
{
    if (m_view3d)
        m_view3d->delete_selected();
}

void GLCanvas3DFacade::mirror_selection(Axis axis)
{
    if (m_view3d)
        m_view3d->mirror_selection(axis);
}

Selection* GLCanvas3DFacade::get_selection()
{
    if (m_view3d)
        return m_view3d->get_selection();

    return NULL;
}

GLGizmosManager* GLCanvas3DFacade::get_gizmos_manager()
{
    return m_view3d->get_gizmos_manager();
}

const Model* GLCanvas3DFacade::get_model()
{
    return m_view3d->get_model();
}

unsigned int GLCanvas3DFacade::get_volumes_count() const
{
    if (m_view3d)
        return m_view3d->get_volumes_count();
    return 0;
}

const GLVolumeCollection& GLCanvas3DFacade::get_volumes() const  
{
    if (m_view3d)
        return m_view3d->get_volumes();
    return GLVolumeCollection();
}

void GLCanvas3DFacade::set_as_dirty()
{
    if (m_view3d)
        m_view3d->set_as_dirty();
    if (m_gcode_preview)
        m_gcode_preview->set_as_dirty();
}

void GLCanvas3DFacade::set_raycaster_gizmos_on_top(bool value)
{
    if (m_view3d)
        m_view3d->set_raycaster_gizmos_on_top(value);
}

std::shared_ptr<SceneRaycasterItem> GLCanvas3DFacade::add_raycaster_for_picking(SceneRaycaster::EType type, int id, const MeshRaycaster& raycaster,
    const Transform3d& trafo, bool use_back_faces)
{
    return m_view3d->add_raycaster_for_picking(type, id, raycaster, trafo, use_back_faces);
}

void GLCanvas3DFacade::remove_raycasters_for_picking(SceneRaycaster::EType type, int id)
{
    if (m_view3d)
        m_view3d->remove_raycasters_for_picking(type, id);
}

void GLCanvas3DFacade::remove_raycasters_for_picking(SceneRaycaster::EType type)
{
    if (m_view3d)
        m_view3d->remove_raycasters_for_picking(type);
}

Size GLCanvas3DFacade::get_canvas_size() const
{
    if (m_view3d)
        return m_view3d->get_canvas_size();
    if (m_gcode_preview)
        return m_gcode_preview->get_canvas_size();
    
    return Size(0, 0);
}

int GLCanvas3DFacade::get_main_toolbar_offset() const
{
    if (m_view3d)
        return m_view3d->get_main_toolbar_offset();

    return 0;
}

int GLCanvas3DFacade::get_main_toolbar_height() const
{
    if (m_view3d)
        return m_view3d->get_main_toolbar_height();

    return 0;
}

int GLCanvas3DFacade::get_main_toolbar_width() const
{
    if (m_view3d)
        return m_view3d->get_main_toolbar_width();

    return 0;
}

float GLCanvas3DFacade::get_separator_toolbar_width() const
{
    if (m_view3d)
        return m_view3d->get_separator_toolbar_width();

    return 0;
}

float GLCanvas3DFacade::get_separator_toolbar_height() const
{
    if (m_view3d)
        return m_view3d->get_separator_toolbar_height();

    return 0;
}

bool  GLCanvas3DFacade::is_collapse_toolbar_on_left() const
{
    if (m_view3d)
        return m_view3d->is_collapse_toolbar_on_left();

    return 0;
}

float GLCanvas3DFacade::get_collapse_toolbar_width() const
{
    if (m_view3d)
        return m_view3d->get_collapse_toolbar_width();

    return 0;
}

float GLCanvas3DFacade::get_collapse_toolbar_height() const
{
    if (m_view3d)
        return m_view3d->get_collapse_toolbar_height();

    return 0;
}

const float GLCanvas3DFacade::get_scale() const
{
    if (m_view3d)
        return m_view3d->get_scale();
    if (m_gcode_preview)
        return m_gcode_preview->get_scale();

    return 0;
}

void GLCanvas3DFacade::toggle_model_objects_visibility(bool visible, const ModelObject* mo, int instance_idx, const ModelVolume* mv)
{
    if (m_view3d)
        m_view3d->toggle_model_objects_visibility(visible, mo, instance_idx, mv);
}

int64_t GLCanvas3DFacade::timestamp_now()
{
    if (m_view3d)
        return m_view3d->timestamp_now();
    if (m_gcode_preview)
        return m_gcode_preview->timestamp_now();

    return 0;
}

void GLCanvas3DFacade::schedule_extra_frame(int miliseconds)
{
    if (m_view3d)
        m_view3d->schedule_extra_frame(miliseconds);
    if (m_gcode_preview)
        m_gcode_preview->schedule_extra_frame(miliseconds);
}

void GLCanvas3DFacade::handle_sidebar_focus_event(const std::string& opt_key, bool focus_on)
{
    if (m_view3d)
        m_view3d->handle_sidebar_focus_event(opt_key, focus_on);
}

BoundingBoxf3 GLCanvas3DFacade::volumes_bounding_box(bool current_plate_only) const
{
    if (m_view3d)
        return m_view3d->volumes_bounding_box(current_plate_only);
    if (m_gcode_preview)
        return m_gcode_preview->volumes_bounding_box(current_plate_only);

    return BoundingBoxf3();
}

void GLCanvas3DFacade::set_use_clipping_planes(bool use)
{
    if (m_view3d)
        return m_view3d->set_use_clipping_planes(use);
}

void GLCanvas3DFacade::set_clipping_plane(unsigned int id, const ClippingPlane& plane)
{
    if (m_view3d)
        return m_view3d->set_clipping_plane(id, plane);
}

bool GLCanvas3DFacade::is_mouse_dragging() const
{
    if (m_view3d)
        return m_view3d->is_mouse_dragging();
    if (m_gcode_preview)
        return m_gcode_preview->is_mouse_dragging();

    return false;
}

Linef3 GLCanvas3DFacade::mouse_ray(const Point& mouse_pos)
{
    if (m_view3d)
        return m_view3d->mouse_ray(mouse_pos);
    return Linef3();
}

void GLCanvas3DFacade::post_event(wxEvent&& event)
{
    wxEvent* evt = &event;
    if (m_view3d)
        m_view3d->post_event(evt);
    if (m_gcode_preview)
        m_gcode_preview->post_event(evt);
}

void GLCanvas3DFacade::enable_picking(bool enable)
{
    if (m_view3d)
        m_view3d->enable_picking(enable);
}

void GLCanvas3DFacade::enable_moving(bool enable)
{
    if (m_view3d)
        m_view3d->enable_moving(enable);
}

int GLCanvas3DFacade::get_first_hover_volume_idx() const
{
    if (m_view3d)
        return m_view3d->get_first_hover_volume_idx();
    return -1;
}

std::vector<std::shared_ptr<SceneRaycasterItem>>* GLCanvas3DFacade::get_raycasters_for_picking(SceneRaycaster::EType type)
{
    if (m_view3d)
        return m_view3d->get_raycasters_for_picking(type);
    
    return NULL;
}

void GLCanvas3DFacade::mouse_up_cleanup()
{
    if (m_view3d)
        return m_view3d->mouse_up_cleanup();
    if (m_gcode_preview)
        return m_gcode_preview->mouse_up_cleanup();
}

void GLCanvas3DFacade::refresh_camera_scene_box()
{
    if (m_view3d)
        return m_view3d->refresh_camera_scene_box();
    if (m_gcode_preview)
        return m_gcode_preview->refresh_camera_scene_box();
}

void GLCanvas3DFacade::request_extra_frame()
{
    if (m_view3d)
        return m_view3d->request_extra_frame();
    if (m_gcode_preview)
        return m_gcode_preview->request_extra_frame();
}

};
};

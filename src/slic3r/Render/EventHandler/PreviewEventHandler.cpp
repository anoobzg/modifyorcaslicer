#include "PreviewEventHandler.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "libslic3r/AppConfig.hpp"
#include <igl/unproject.h>

static constexpr const float TRACKBALLSIZE = 0.8f;


namespace Slic3r {
namespace GUI {

PreviewEventHandler::PreviewEventHandler()
{

}

bool PreviewEventHandler::handle(const wxEvent& event, GUIActionAdapte* adapte)
{
    m_canvas = static_cast<GLCanvas3D*>(adapte);
    m_mouse = m_canvas->get_mouse();
    const wxMouseEvent* mouse_event = dynamic_cast<const wxMouseEvent*>(&event);
    return on_mouse(mouse_event);
}

bool PreviewEventHandler::on_mouse(const wxMouseEvent* evt)
{   
    if (!m_canvas) 
        return false;
    return try_rotate_camera(evt) || try_pan_camera(evt) || try_scale_camera(evt) || try_clean_mouse(evt);
}

bool PreviewEventHandler::try_rotate_camera(const wxMouseEvent* evt)
{
    if (!is_camera_rotate(evt))
        return false;

    //bool any_gizmo_active = m_gizmos.get_current() != nullptr;
    Point pos(evt->GetX(), evt->GetY());
    bool can_rotate = true; // (any_gizmo_active || m_hover_volume_idxs.empty())

    if (!can_rotate || !m_mouse->is_start_position_3D_defined())
    {
        m_camera_movement = true;
        m_mouse->drag.start_position_3D = Vec3d((double)pos(0), (double)pos(1), 0.0);
        return false;
    }

    Camera* camera = m_canvas->get_camera_ptr();
    const Vec3d rot = (Vec3d(pos.x(), pos.y(), 0.) - m_mouse->drag.start_position_3D) * (PI * TRACKBALLSIZE / 180.);

    if (m_canvas->use_free_camera())
        // Virtual track ball (similar to the 3DConnexion mouse).
        camera->rotate_local_around_target(Vec3d(rot.y(), rot.x(), 0.));
    else {
        // Forces camera right vector to be parallel to XY plane in case it has been misaligned using the 3D mouse free rotation.
        // It is cheaper to call this function right away instead of testing AppAdapter::plater()->get_mouse3d_controller().connected(),
        // which checks an atomics (flushes CPU caches).
        // See GH issue #3816.
        bool rotate_limit = true;

        camera->recover_from_free_camera();
        if (evt->ControlDown() || evt->CmdDown()) {
            if ((m_rotation_center.x() == 0.f) && (m_rotation_center.y() == 0.f) && (m_rotation_center.z() == 0.f)) {
                auto canvas_w = float(m_canvas->get_canvas_size().get_width());
                auto canvas_h = float(m_canvas->get_canvas_size().get_height());
                Point screen_center(canvas_w / 2, canvas_h / 2);
                m_rotation_center = _mouse_to_3d(screen_center);
                m_rotation_center(2) = 0.f;
            }
            camera->rotate_on_sphere_with_target(rot.x(), rot.y(), rotate_limit, m_rotation_center);
        }
        else {
            Vec3d rotate_target = m_canvas->rotate_center();
            if (!rotate_target.isZero())
                camera->rotate_on_sphere_with_target(rot.x(), rot.y(), rotate_limit, rotate_target);
            else
                camera->rotate_on_sphere(rot.x(), rot.y(), rotate_limit);
        }
    }
    camera->auto_type(Camera::EType::Perspective);

    m_canvas->set_as_dirty();
    m_camera_movement = true;
    m_mouse->drag.start_position_3D = Vec3d((double)pos(0), (double)pos(1), 0.0);

    return true;
}

bool PreviewEventHandler::try_pan_camera(const wxMouseEvent* evt)
{
    if (!is_camera_pan(evt))
        return false;

    Point pos(evt->GetX(), evt->GetY());
    // If dragging over blank area with right button, pan.
    if (!m_mouse->is_start_position_2D_defined()) 
    {
        m_camera_movement = true;
        m_mouse->drag.start_position_2D = pos;
        return false;
    }

    // get point in model space at Z = 0
    float z = 0.0f;
    const Vec3d& cur_pos = _mouse_to_3d(pos, &z);
    Vec3d orig = _mouse_to_3d(m_mouse->drag.start_position_2D, &z);
    Camera* camera = m_canvas->get_camera_ptr();
    if (m_canvas->use_free_camera()) {
        camera->recover_from_free_camera();
    }
    int a = 5;
    Point ddrag = pos - m_mouse->drag.start_position_2D;
    Vec3d dpos = orig - cur_pos;
    if (ddrag.x() > 0 || dpos[0] > 0)
    {
        a++;
    }
    camera->set_target(camera->get_target() + dpos);
    m_canvas->set_as_dirty();
    m_mouse->ignore_right_up = true;

    m_camera_movement = true;
    m_mouse->drag.start_position_2D = pos;
    return true;

}

bool PreviewEventHandler::try_scale_camera(const wxMouseEvent* evt)
{
    if (evt->GetEventType() != wxEVT_MOUSEWHEEL)
        return false;
    // Calculate the zoom delta and apply it to the current zoom factor
    double direction_factor = AppAdapter::app_config()->get_bool("reverse_mouse_wheel_zoom") ? -1.0 : 1.0;
    auto delta = direction_factor * (double)evt->GetWheelRotation() / (double)evt->GetWheelDelta();
    bool zoom_to_mouse = AppAdapter::app_config()->get("zoom_to_mouse") == "true";
    if (!zoom_to_mouse) {// zoom to center
        Camera* camera = m_canvas->get_camera_ptr();
        camera->update_zoom(delta);
        m_canvas->set_as_dirty();
    }
    else {
        auto cnv_size = m_canvas->get_canvas_size();
        float z{ 0.f };
        auto screen_center_3d_pos = _mouse_to_3d({ cnv_size.get_width() * 0.5, cnv_size.get_height() * 0.5 }, &z);
        auto mouse_3d_pos = _mouse_to_3d({ evt->GetX(), evt->GetY() }, &z);
        Vec3d displacement = mouse_3d_pos - screen_center_3d_pos;

        Camera* camera = m_canvas->get_camera_ptr();
        camera->translate(displacement);
        auto origin_zoom = camera->get_zoom();
        camera->update_zoom(delta);
        m_canvas->set_as_dirty();
        auto new_zoom = camera->get_zoom();
        camera->translate((-displacement) / (new_zoom / origin_zoom));
    }

    return true;
}

bool PreviewEventHandler::try_clean_mouse(const wxMouseEvent* evt)
{
    if ((evt->LeftUp() || evt->MiddleUp() || evt->RightUp()) ||
        (m_camera_movement && !is_camera_rotate(evt) && !is_camera_pan(evt))) 
    {
        Point pos(evt->GetX(), evt->GetY());
        m_mouse->position = pos.cast<double>();
        m_canvas->mouse_up_cleanup();
        return true;
    }
    return false;
}

Vec3d PreviewEventHandler::_mouse_to_3d(const Point& mouse_pos, float* z)
{
    if (m_canvas == nullptr)
        return Vec3d(DBL_MAX, DBL_MAX, DBL_MAX);

    if (z == nullptr) {
        const SceneRaycaster::HitResult hit = m_canvas->get_scene_raycaster()->hit(mouse_pos.cast<double>(), m_canvas->get_camera(), nullptr);
        return hit.is_valid() ? hit.position.cast<double>() : _mouse_to_bed_3d(mouse_pos);
    }
    else {
        Camera* camera = m_canvas->get_camera_ptr();
        const Vec4i32 viewport(camera->get_viewport().data());
        Vec3d out;
        igl::unproject(Vec3d(mouse_pos.x(), viewport[3] - mouse_pos.y(), *z), camera->get_view_matrix().matrix(), camera->get_projection_matrix().matrix(), viewport, out);
        return out;
    }
}

Vec3d PreviewEventHandler::_mouse_to_bed_3d(const Point& mouse_pos)
{
    return mouse_ray(mouse_pos).intersect_plane(0.0);
}

Linef3 PreviewEventHandler::mouse_ray(const Point& mouse_pos)
{
    float z0 = 0.0f;
    float z1 = 1.0f;
    return Linef3(_mouse_to_3d(mouse_pos, &z0), _mouse_to_3d(mouse_pos, &z1));
}

bool PreviewEventHandler::is_camera_rotate(const wxMouseEvent* evt)
{
    return evt->Dragging() && evt->LeftIsDown();
}

bool PreviewEventHandler::is_camera_pan(const wxMouseEvent* evt)
{
    return evt->Dragging() && (evt->MiddleIsDown() || evt->RightIsDown());
}

};
};
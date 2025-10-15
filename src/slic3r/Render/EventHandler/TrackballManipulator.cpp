#include "TrackballManipulator.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "libslic3r/AppConfig.hpp"
#include "slic3r/Scene/PartPlate.hpp"

static constexpr const float TRACKBALLSIZE = 0.8f;

using namespace HMS;
using namespace GUI;
using namespace Slic3r;

 
void TrackballManipulator::reset()
{
    _start_position_2D = Vec2d(INT_MAX, INT_MAX);
    _start_position_3D = Vec3d(INT_MAX, INT_MAX, INT_MAX);
    _rotation_center = Vec3d();
}

bool TrackballManipulator::is_start_position_2D_defined()
{
    return _start_position_2D != Vec2d(INT_MAX, INT_MAX);
}

bool TrackballManipulator::is_start_position_3D_defined()
{
    return _start_position_3D != Vec3d(INT_MAX, INT_MAX, INT_MAX);
}

bool TrackballManipulator::handle(const wxEvent &ea, GUIActionAdapte* aa)
{
    // const wxMouseEvent *mouseEvent = dynamic_cast<const wxMouseEvent *>(&ea);
    // GLCanvas3D *pGLCanvas3D = dynamic_cast<GLCanvas3D *>(&aa);
    // Point pos(mouseEvent->GetX(), mouseEvent->GetY());

    // if (mouseEvent == nullptr || pGLCanvas3D == nullptr)
    //     return false;

    // if (mouseEvent->LeftDown())
    // {

    // }
    // else if (mouseEvent->LeftUp())
    // {
    //     reset();
    // }
    // else if (mouseEvent->RightDown())
    // {

    // }
    // else if (mouseEvent->RightUp())
    // {
    //     reset();
    // }
    // else if (mouseEvent->MiddleDown())
    // {
    // }
    // else if (mouseEvent->MiddleUp())
    // {
    //     reset();
    // }
    // else if (mouseEvent->Dragging())
    // {
    //     if (mouseEvent->LeftIsDown() ) // rotate
    //     {
    //         if (is_start_position_3D_defined())
    //         {
    //             const GLGizmosManager& gizmos = pGLCanvas3D->get_gizmos_manager();
    //             Selection* selection = pGLCanvas3D->get_selection();
    //             BoundingBoxf3 volumeBox = pGLCanvas3D->volumes_bounding_box();
    //             //Camera &camera = pGLCanvas3D->get_camera();
    //             Camera& camera = AppAdapter::plater()->get_camera();
    //             const Vec3d rot = (Vec3d(pos.x(), pos.y(), 0.) - _start_position_3D) * (PI * TRACKBALLSIZE / 180.);
    //             if (pGLCanvas3D->get_canvas_type() == GLCanvas3D::ECanvasType::CanvasAssembleView
    //                 || gizmos.get_current_type() == GLGizmosManager::FdmSupports
    //                 || gizmos.get_current_type() == GLGizmosManager::Seam
    //                 || gizmos.get_current_type() == GLGizmosManager::MmuSegmentation)
    //             {
    //                 Vec3d rotate_target = Vec3d::Zero();
    //                 if (!selection->is_empty())
    //                     rotate_target = selection->get_bounding_box().center();
    //                 else
    //                     rotate_target = volumeBox.center();
    //                 camera.rotate_on_sphere_with_target(rot.x(), rot.y(), false, rotate_target);
    //             }
    //             else
    //             {
    //                 if (AppAdapter::app_config()->get_bool("use_free_camera"))
    //                 {
    //                     camera.rotate_local_around_target(Vec3d(rot.y(), rot.x(), 0.));
    //                 }
    //                 else
    //                 {
    //                     bool rotate_limit = true;
    //                     camera.recover_from_free_camera();
    //                     if (mouseEvent->ControlDown() || mouseEvent->CmdDown())
    //                     {
    //                         if ((_rotation_center.x() == 0.f) && (_rotation_center.y() == 0.f) && (_rotation_center.z() == 0.f))
    //                         {
    //                             auto canvas_w = float(pGLCanvas3D->get_canvas_size().get_width());
    //                             auto canvas_h = float(pGLCanvas3D->get_canvas_size().get_height());
    //                             Point screen_center(canvas_w / 2, canvas_h / 2);
    //                             _rotation_center = pGLCanvas3D->_mouse_to_3d(screen_center);
    //                             _rotation_center(2) = 0.f;
    //                         }
    //                         camera.rotate_on_sphere_with_target(rot.x(), rot.y(), rotate_limit, _rotation_center);
    //                     }
    //                     else
    //                     {
    //                         Vec3d rotate_target = Vec3d::Zero();
    //                         if (pGLCanvas3D->get_canvas_type() == GLCanvas3D::ECanvasType::CanvasPreview)
    //                         {
    //                             PartPlate* plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
    //                             if (plate)
    //                                 rotate_target = plate->get_bounding_box().center();
    //                         }
    //                         else
    //                         {
    //                             volumeBox = pGLCanvas3D->volumes_bounding_box(true);
    //                             if (!selection->is_empty())
    //                                 rotate_target = selection->get_bounding_box().center();
    //                             else
    //                                 rotate_target = volumeBox.center();
    //                         }

    //                         if (!rotate_target.isZero())
    //                             camera.rotate_on_sphere_with_target(rot.x(), rot.y(), rotate_limit, rotate_target);
    //                         else
    //                             camera.rotate_on_sphere(rot.x(), rot.y(), rotate_limit);
    //                     }
    //                 }
    //                 camera.auto_type(Camera::EType::Perspective);
    //                 pGLCanvas3D->dirty();
    //             }
    //         }
           
    //         _start_position_3D = Vec3d((double)pos(0), (double)pos(1), 0.0);
    //     }

    //     if (mouseEvent->RightIsDown() || mouseEvent->MiddleIsDown()) // pan
    //     {
    //         if (is_start_position_2D_defined()) {
    //         //    // get point in model space at Z = 0
    //             float z = 0.0f;
    //             const Vec3d& cur_pos = pGLCanvas3D->_mouse_to_3d(pos, &z);
    //             Vec3d orig = pGLCanvas3D->_mouse_to_3d(Point(_start_position_2D.x(), _start_position_2D.y()), &z);
    //             Camera& camera = AppAdapter::plater()->get_camera();
    //             if (pGLCanvas3D->get_canvas_type() != GLCanvas3D::ECanvasType::CanvasAssembleView) {
    //                 if (AppAdapter::app_config()->get_bool("use_free_camera"))
    //                     // Forces camera right vector to be parallel to XY plane in case it has been misaligned using the 3D mouse free rotation.
    //                     // It is cheaper to call this function right away instead of testing AppAdapter::plater()->get_mouse3d_controller().connected(),
    //                     // which checks an atomics (flushes CPU caches).
    //                     // See GH issue #3816.
    //                     camera.recover_from_free_camera();
    //             }

    //             camera.set_target(camera.get_target() + orig - cur_pos);
    //             pGLCanvas3D->dirty();
    //         }
    //         _start_position_2D = Vec2d(pos.x(), pos.y());
    //     }
    // }

    return false;
}

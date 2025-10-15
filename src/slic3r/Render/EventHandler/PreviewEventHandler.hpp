#ifndef _slic3r_PreviewEventHandler_hpp
#define _slic3r_PreviewEventHandler_hpp

#include "GUIEventHandler.hpp"
#include "slic3r/Render/RenderHelpers.hpp"
#include <wx/event.h>

using namespace HMS;
namespace Slic3r {
namespace GUI {

class GLCanvas3D;
class MouseHelper;

class PreviewEventHandler : public GUIEventHandler
{
public:
    PreviewEventHandler();

    virtual bool handle(const wxEvent& event , GUIActionAdapte* adapte);

private:
    bool on_mouse(const wxMouseEvent* event);

    bool try_rotate_camera(const wxMouseEvent* evt);
    bool try_pan_camera(const wxMouseEvent* evt);
    bool try_scale_camera(const wxMouseEvent* evt);
    bool try_clean_mouse(const wxMouseEvent* evt);

    Vec3d _mouse_to_3d(const Point& mouse_pos, float* z = nullptr);
    Vec3d _mouse_to_bed_3d(const Point& mouse_pos);
    Linef3 mouse_ray(const Point& mouse_pos);
    bool is_camera_rotate(const wxMouseEvent* evt);
    bool is_camera_pan(const wxMouseEvent* evt);

private:
    MouseHelper* m_mouse;
    mutable Vec3d m_rotation_center{ 0.0, 0.0, 0.0};
    GLCanvas3D* m_canvas { NULL };
    bool m_camera_movement;

};

};
};


#endif
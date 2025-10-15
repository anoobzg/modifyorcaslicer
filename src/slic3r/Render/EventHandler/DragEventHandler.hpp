
#pragma once
#include "GUIEventHandler.hpp"
#include "libslic3r/Point.hpp"
#include <wx/event.h>
using namespace Slic3r;

 
namespace HMS
{
    class DragEventHandler : public GUIEventHandler
    {
    public:
        DragEventHandler()
        {
            reset();
        }
        void reset();
        virtual bool handle(const wxEvent& ea , GUIActionAdapte* aa);
        void set_move_start_threshold_position_2D_as_invalid();

    protected:
        bool  move_requires_threshold = false;
        Vec2d  move_start_threshold_position_2D = Vec2d(DBL_MAX, DBL_MAX);
        int move_volume_idx = -1;
        bool dragging = false;
        Vec3d start_position_3D = Vec3d(DBL_MAX, DBL_MAX, DBL_MAX);
        Vec3d scene_position = Vec3d(DBL_MAX, DBL_MAX, DBL_MAX);
        bool m_moving = false;
    };

   
};

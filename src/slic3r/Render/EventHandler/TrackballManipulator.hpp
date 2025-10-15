
#pragma once
#include "GUIEventHandler.hpp"
#include "libslic3r/Point.hpp"
#include <wx/event.h>

using namespace Slic3r;
 
namespace HMS
{
    class TrackballManipulator : public GUIEventHandler
    {
    public:
        TrackballManipulator()
        {
            reset();
        }
        void reset();
        virtual bool handle(const wxEvent& ea , GUIActionAdapte* aa);
    private:
        bool is_start_position_2D_defined();
        bool is_start_position_3D_defined();
    protected:
 
        // Vec2d _position;
        // bool  _bCameraMovement = false;
        // bool  _bDrag = false;
         mutable Vec3d _rotation_center{ 0.0, 0.0, 0.0};
         Vec3d _start_position_3D { 0.0, 0.0, 0.0};
         Vec2d _start_position_2D { 0.0, 0.0 };
    };
};

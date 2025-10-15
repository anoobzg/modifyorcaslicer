
#pragma once
#include "GUIEventHandler.hpp"
#include "libslic3r/Point.hpp"
#include <wx/event.h>
using namespace Slic3r;

class  Tooltip;
class  IMToolbar;
class  IMReturnToolbar;
class  GLToolbar;
 
namespace HMS
{
    class UICustomEventHandler : public GUIEventHandler
    {
    public:
        UICustomEventHandler()
        {
            reset();
        }
        void reset();
        virtual bool handle(const wxEvent& ea , GUIActionAdapte* aa);
    protected:
        Tooltip* _tooltip;
        GLToolbar* _main_toolbar;
        GLToolbar* _assemble_view_toolbar;
        GLToolbar* _collapse_toolbar;
    };
};

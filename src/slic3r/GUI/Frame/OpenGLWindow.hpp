#pragma once 
#include <wx/glcanvas.h>

namespace Slic3r {
namespace GUI {

class OpenGLWindow : public wxGLCanvas
{
public:
    OpenGLWindow(wxWindow* parent, int* attribs = nullptr);
    virtual ~OpenGLWindow();

    void set_context(wxGLContext* context);
    wxGLContext* get_context();

    bool set_current();
    void swap_buffers();
protected:
    wxGLContext* m_context;
};

}
}
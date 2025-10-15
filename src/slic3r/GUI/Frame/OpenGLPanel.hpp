#pragma once

class wxGLCanvas;
class wxGLContext;
namespace Slic3r {
namespace GUI {
class OpenGLWindow;
class OpenGLPanel : public wxPanel
{
public:
    OpenGLPanel(wxWindow* parent);
    virtual ~OpenGLPanel();

    void render_all();
    wxGLCanvas* raw_canvas();
    wxGLContext* raw_context();

    void render_update();
    virtual void attach() = 0;
    virtual void detach() = 0;
protected:
    virtual void render_impl() = 0;

protected:
    OpenGLWindow* m_opengl_window = nullptr;
};

}
}
#include "OpenGLPanel.hpp"

#include "slic3r/Render/AppRender.hpp"

#include "slic3r/GUI/Frame/OpenGLWindow.hpp"

namespace Slic3r {
namespace GUI {

OpenGLPanel::OpenGLPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 /* disable wxTAB_TRAVERSAL */)
{
    m_opengl_window = new OpenGLWindow(this, get_shared_attrib_list());
    m_opengl_window->set_context(get_shared_context(*m_opengl_window));

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(m_opengl_window, 1, wxALL | wxEXPAND, 0);
    SetSizer(main_sizer);
    Layout();
    GetSizer()->SetSizeHints(this);
    Hide();
}

OpenGLPanel::~OpenGLPanel()
{
}

wxGLCanvas* OpenGLPanel::raw_canvas()
{
    return m_opengl_window;
}

wxGLContext* OpenGLPanel::raw_context()
{
    return m_opengl_window->get_context();
}

void OpenGLPanel::render_update()
{
    m_opengl_window->Refresh();
}

void OpenGLPanel::render_all()
{
    if(!init_opengl())
        return;

    if(!m_opengl_window->set_current())
        return;

    render_impl();
    m_opengl_window->swap_buffers();
}

}
}
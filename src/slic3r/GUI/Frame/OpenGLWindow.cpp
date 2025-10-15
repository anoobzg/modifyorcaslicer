#include "OpenGLWindow.hpp"

namespace Slic3r {
namespace GUI {

OpenGLWindow::OpenGLWindow(wxWindow* parent, int* attribs)
    : wxGLCanvas(parent, wxID_ANY, attribs, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS)
    , m_context(nullptr)
{
}

OpenGLWindow::~OpenGLWindow()
{
}

void OpenGLWindow::set_context(wxGLContext* context)
{
    m_context = context;
}

wxGLContext* OpenGLWindow::get_context()
{
    return m_context;
}

bool OpenGLWindow::set_current()
{
    return m_context != nullptr && SetCurrent(*m_context);
}

void OpenGLWindow::swap_buffers()
{
    SwapBuffers();
}

} // namespace GUI
}
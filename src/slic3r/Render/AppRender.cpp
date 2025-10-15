#include "AppRender.hpp"

#include "slic3r/Render/OpenGLManager.hpp"

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"

namespace Slic3r {
namespace GUI {

std::unique_ptr<OpenGLManager> l_opengl_manager(new OpenGLManager());
OpenGLManager* opengl_manager()
{
    return l_opengl_manager.get();
}

bool init_opengl()
{
    if(l_opengl_manager->is_initialized())
        return true;
    
    bool status = true;
#ifdef __linux__
    status = l_opengl_manager->init_gl();
#else
    status = l_opengl_manager->init_gl();
#endif

    bool popup_error = true;
    bool valid_version = l_opengl_manager->get_gl_info().is_version_greater_or_equal_to(2, 0);
    if (!valid_version) {
        BOOST_LOG_TRIVIAL(error) << "Found opengl version <= 2.0"<< std::endl;
        // Complain about the OpenGL version.
        if (popup_error) {
            wxString message = from_u8((boost::format(
                _utf8(L("The application cannot run normally because OpenGL version is lower than 2.0.\n")))).str());
            message += "\n";
            message += _L("Please upgrade your graphics card driver.");
            wxMessageBox(message, _L("Unsupported OpenGL version"), wxOK | wxICON_ERROR);
        }
    }

    std::string error;
    if (valid_version && l_opengl_manager->init_shaders_happen_error(error))
    {
        // load shaders
        BOOST_LOG_TRIVIAL(error) << "Unable to load shaders: " << error << std::endl;
        if (popup_error) {
            wxString message = from_u8((boost::format(
                _utf8(L("Unable to load shaders:\n%s"))) % error).str());
            wxMessageBox(message, _L("Error loading shaders"), wxOK | wxICON_ERROR);
        }
    }

    return status;
}

wxGLContext* create_context(wxGLCanvas& canvas)
{
    return opengl_manager()->init_glcontext(canvas);
}

wxGLCanvas* create_canvas(wxWindow& parent)
{
    return opengl_manager()->create_wxglcanvas(parent);
}

int* get_shared_attrib_list()
{
    return opengl_manager()->get_shared_attrib_list();
}

wxGLContext* get_shared_context(wxGLCanvas& canvas)
{
    return opengl_manager()->get_shared_context(canvas);
}

GLShaderProgram* get_shader(const std::string& shader_name)
{ 
    return opengl_manager()->get_shader(shader_name);
}

GLShaderProgram* get_current_shader()
{ 
    return opengl_manager()->get_current_shader();
}

bool is_gl_version_greater_or_equal_to(unsigned int major, unsigned int minor)
{ 
    return OpenGLManager::get_gl_info().is_version_greater_or_equal_to(major, minor); 
}

bool is_glsl_version_greater_or_equal_to(unsigned int major, unsigned int minor) 
{ 
    return OpenGLManager::get_gl_info().is_glsl_version_greater_or_equal_to(major, minor); 
}

std::string get_gl_info(bool for_github)
{
    return OpenGLManager::get_gl_info().to_string(for_github);
}

bool can_multisample()
{
    return OpenGLManager::can_multisample();
}

bool is_arb_framebuffer()
{
    const GUI::OpenGLManager::EFramebufferType framebuffers_type = GUI::OpenGLManager::get_framebuffers_type();
    return framebuffers_type == GUI::OpenGLManager::EFramebufferType::Arb;
}

bool is_ext_framebuffer()
{
    const GUI::OpenGLManager::EFramebufferType framebuffers_type = GUI::OpenGLManager::get_framebuffers_type();
    return framebuffers_type == GUI::OpenGLManager::EFramebufferType::Ext;
}

bool is_unkown_framebuffer()
{
    const GUI::OpenGLManager::EFramebufferType framebuffers_type = GUI::OpenGLManager::get_framebuffers_type();
    return framebuffers_type == GUI::OpenGLManager::EFramebufferType::Unknown;
}

bool are_compressed_textures_supported()
{
    return OpenGLManager::are_compressed_textures_supported();
}

int get_max_texture_size()
{
    return OpenGLManager::get_gl_info().get_max_tex_size();
}

bool is_mesa()
{
    return OpenGLManager::get_gl_info().is_mesa();
}

float get_max_anisotropy()
{
    return OpenGLManager::get_gl_info().get_max_anisotropy();
}

bool force_power_of_two_textures()
{
    return OpenGLManager::force_power_of_two_textures();
}

std::string get_gl_version()
{
    return OpenGLManager::get_gl_info().get_version();
}

std::string get_glsl_version()
{
    return OpenGLManager::get_gl_info().get_glsl_version();
}

std::string get_gl_vendor()
{
    return OpenGLManager::get_gl_info().get_vendor();
}

std::string get_gl_renderer()
{
    return OpenGLManager::get_gl_info().get_renderer();
}

bool is_core_profile()
{
    return OpenGLManager::get_gl_info().is_core_profile();
}


/////////////////////////// 
bool show_render_statistic_dialog = false;
void toggle_render_statistic_dialog()
{
    show_render_statistic_dialog = !show_render_statistic_dialog;
}

bool is_render_statistic_dialog_visible()
{
    return show_render_statistic_dialog;
}

}
}
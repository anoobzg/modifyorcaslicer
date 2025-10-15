#pragma once
#include <GL/glew.h>
#include <GL/GL.h>

#ifndef NDEBUG
#define HAS_GLSAFE
#endif // NDEBUG

#ifdef HAS_GLSAFE
    extern void glAssertRecentCallImpl(const char *file_name, unsigned int line, const char *function_name);
    inline void glAssertRecentCall() { glAssertRecentCallImpl(__FILE__, __LINE__, __FUNCTION__); }
    #define glsafe(cmd) do { cmd; glAssertRecentCallImpl(__FILE__, __LINE__, __FUNCTION__); } while (false)
    #define glcheck() do { glAssertRecentCallImpl(__FILE__, __LINE__, __FUNCTION__); } while (false)
#else // HAS_GLSAFE
    inline void glAssertRecentCall() { }
    #define glsafe(cmd) cmd
    #define glcheck()
#endif // HAS_GLSAFE

class wxGLContext;
class wxGLCanvas;
class wxWindow;
namespace Slic3r {

class GLShaderProgram;
namespace GUI {
    
bool init_opengl();

wxGLContext* create_context(wxGLCanvas& canvas);
wxGLCanvas* create_canvas(wxWindow& parent);

int* get_shared_attrib_list();
wxGLContext* get_shared_context(wxGLCanvas& canvas);

GLShaderProgram* get_shader(const std::string& shader_name);
GLShaderProgram* get_current_shader();

// If formatted for github, plaintext with OpenGL extensions enclosed into <details>.
// Otherwise HTML formatted for the system info dialog.
std::string get_gl_info(bool for_github);

bool is_gl_version_greater_or_equal_to(unsigned int major, unsigned int minor);
bool is_glsl_version_greater_or_equal_to(unsigned int major, unsigned int minor);

bool can_multisample();

bool is_arb_framebuffer();
bool is_ext_framebuffer();
bool is_unkown_framebuffer();

bool are_compressed_textures_supported();

int get_max_texture_size();
bool is_mesa();

GLfloat get_max_anisotropy();

bool force_power_of_two_textures();

std::string get_gl_version();
std::string get_glsl_version();
std::string get_gl_vendor();
std::string get_gl_renderer();

// Declaration of a free function defined in OpenGLManager.cpp:
std::string gl_get_string_safe(GLenum param, const std::string& default_value);

bool is_core_profile();



//////////////// Global config
void toggle_render_statistic_dialog();
bool is_render_statistic_dialog_visible();

}
}
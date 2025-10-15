#include "GLShadersManager.hpp"

#include "libslic3r/Base/Platform.hpp"
#include "GLShader.hpp"
#include "slic3r/Render/AppRender.hpp"

using namespace std::literals;
namespace Slic3r {

std::pair<bool, std::string> GLShadersManager::init()
{
    std::string error;

    auto append_shader = [this, &error](const std::string& name, const GLShaderProgram::ShaderFilenames& filenames,
        const std::initializer_list<std::string_view> &defines = {}) {
        m_shaders.push_back(new GLShaderProgram());
        if (!m_shaders.back()->init_from_files(name, filenames, defines)) {
            error += name + "\n";
            // if any error happens while initializating the shader, we remove it from the list
            m_shaders.pop_back();
            return false;
        }
        return true;
    };

    assert(m_shaders.empty());

    bool valid = true;

    const std::string prefix = GUI::is_gl_version_greater_or_equal_to(3, 1) ? "140/" : "110/";
    // imgui shader
    valid &= append_shader("imgui", { prefix + "imgui.vs", prefix + "imgui.fs" });
    // basic shader, used to render all what was previously rendered using the immediate mode
    valid &= append_shader("flat", { prefix + "flat.vs", prefix + "flat.fs" });
    // basic shader with plane clipping, used to render volumes in picking pass
    valid &= append_shader("flat_clip", { prefix + "flat_clip.vs", prefix + "flat_clip.fs" });
    // basic shader for textures, used to render textures
    valid &= append_shader("flat_texture", { prefix + "flat_texture.vs", prefix + "flat_texture.fs" });
    // used to render 3D scene background
    valid &= append_shader("background", { prefix + "background.vs", prefix + "background.fs" });
    // used to render bed axes and model, selection hints, gcode sequential view marker model, preview shells, options in gcode preview
    valid &= append_shader("gouraud_light", { prefix + "gouraud_light.vs", prefix + "gouraud_light.fs" });
    valid &= append_shader("gouraud_light_bed", { prefix + "gouraud_light_bed.vs", prefix + "gouraud_light_bed.fs" });
    // used to render mirror gcode
    valid &= append_shader("mirror_gcode", { prefix + "mirror_gcode.vs", prefix + "mirror_gcode.fs" });
    //used to render thumbnail
    valid &= append_shader("thumbnail", { prefix + "thumbnail.vs", prefix + "thumbnail.fs"});
    // used to render printbed
    valid &= append_shader("printbed", { prefix + "printbed.vs", prefix + "printbed.fs" });
    // used to render options in gcode preview
    if (GUI::is_gl_version_greater_or_equal_to(3, 3)) {
        valid &= append_shader("gouraud_light_instanced", { prefix + "gouraud_light_instanced.vs", prefix + "gouraud_light_instanced.fs" });
    }

    // used to render objects in 3d editor
    valid &= append_shader("gouraud", { prefix + "gouraud.vs", prefix + "gouraud.fs" }
#if ENABLE_ENVIRONMENT_MAP
        , { "ENABLE_ENVIRONMENT_MAP"sv }
#endif // ENABLE_ENVIRONMENT_MAP
        );
    // used to render variable layers heights in 3d editor
    valid &= append_shader("variable_layer_height", { prefix + "variable_layer_height.vs", prefix + "variable_layer_height.fs" });
    // used to render highlight contour around selected triangles inside the multi-material gizmo
    valid &= append_shader("mm_contour", { prefix + "mm_contour.vs", prefix + "mm_contour.fs" });
    // Used to render painted triangles inside the multi-material gizmo. Triangle normals are computed inside fragment shader.
    // For Apple's on Arm CPU computed triangle normals inside fragment shader using dFdx and dFdy has the opposite direction.
    // Because of this, objects had darker colors inside the multi-material gizmo.
    // Based on https://stackoverflow.com/a/66206648, the similar behavior was also spotted on some other devices with Arm CPU.
    // Since macOS 12 (Monterey), this issue with the opposite direction on Apple's Arm CPU seems to be fixed, and computed
    // triangle normals inside fragment shader have the right direction.
    if (platform_flavor() == PlatformFlavor::OSXOnArm && wxPlatformInfo::Get().GetOSMajorVersion() < 12)
        valid &= append_shader("mm_gouraud", { prefix + "mm_gouraud.vs", prefix + "mm_gouraud.fs" }, { "FLIP_TRIANGLE_NORMALS"sv });
    else
        valid &= append_shader("mm_gouraud", { prefix + "mm_gouraud.vs", prefix + "mm_gouraud.fs" });

    return { valid, error };
}

void GLShadersManager::shutdown()
{
    for(GLShaderProgram* shader : m_shaders) {
        delete shader;
    }
    m_shaders.clear();
}

GLShaderProgram* GLShadersManager::get_shader(const std::string& shader_name)
{
    auto it = std::find_if(m_shaders.begin(), m_shaders.end(), [&shader_name](GLShaderProgram* p) { return p->get_name() == shader_name; });
    return (it != m_shaders.end()) ? *it : nullptr;
}

GLShaderProgram* GLShadersManager::get_current_shader()
{
    GLint id = 0;
    glsafe(::glGetIntegerv(GL_CURRENT_PROGRAM, &id));
    if (id == 0)
        return nullptr;

    auto it = std::find_if(m_shaders.begin(), m_shaders.end(), [id](GLShaderProgram* p) { return static_cast<GLint>(p->get_id()) == id; });
    return (it != m_shaders.end()) ? *it : nullptr;
}

} // namespace Slic3r


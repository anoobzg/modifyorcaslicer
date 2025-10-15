#include "LayersEditing.hpp"

#include "libslic3r/Model.hpp"

#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/GUI/Event/UserCanvasEvent.hpp"

#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/GUI/Config/GUI_ObjectList.hpp"

#include <imgui/imgui_internal.h>
#include <wx/glcanvas.h>

namespace Slic3r {
namespace GUI {

LayersEditing::~LayersEditing()
{
    if (m_z_texture_id != 0) {
        glsafe(::glDeleteTextures(1, &m_z_texture_id));
        m_z_texture_id = 0;
    }
    delete m_slicing_parameters;
}

const float LayersEditing::THICKNESS_BAR_WIDTH = 70.0f;

void LayersEditing::init()
{
    glsafe(::glGenTextures(1, (GLuint*)&m_z_texture_id));
    glsafe(::glBindTexture(GL_TEXTURE_2D, m_z_texture_id));
    glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP));
    glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP));
    glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST));
    glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1));
    glsafe(::glBindTexture(GL_TEXTURE_2D, 0));
}

void LayersEditing::set_config(const DynamicPrintConfig* config)
{
    m_config = config;
    delete m_slicing_parameters;
    m_slicing_parameters = nullptr;
    m_layers_texture.valid = false;
    m_layer_height_profile.clear();
}

void LayersEditing::select_object(const Model& model, int object_id)
{
    const ModelObject* model_object_new = (object_id >= 0) ? model.objects[object_id] : nullptr;
    // Maximum height of an object changes when the object gets rotated or scaled.
    // Changing maximum height of an object will invalidate the layer heigth editing profile.
    // m_model_object->bounding_box() is cached, therefore it is cheap even if this method is called frequently.
    const float new_max_z = (model_object_new == nullptr) ? 0.0f : static_cast<float>(model_object_new->max_z());

    if (m_model_object != model_object_new || this->last_object_id != object_id || m_object_max_z != new_max_z ||
        (model_object_new != nullptr && m_model_object->id() != model_object_new->id())) {
        m_layer_height_profile.clear();
        delete m_slicing_parameters;
        m_slicing_parameters = nullptr;
        m_layers_texture.valid = false;
        this->last_object_id = object_id;
        m_model_object = model_object_new;
        m_object_max_z = new_max_z;
    }
}

bool LayersEditing::is_allowed() const
{
    return get_shader("variable_layer_height") != nullptr && m_z_texture_id > 0;
}

bool LayersEditing::is_enabled() const
{
    return m_enabled;
}

void LayersEditing::set_enabled(bool enabled)
{
    m_enabled = is_allowed() && enabled;
}

float LayersEditing::s_overlay_window_width;

void LayersEditing::show_tooltip_information(const GLCanvas3D& canvas, std::map<wxString, wxString> captions_texts, float x, float y)
{
    ImTextureID normal_id = canvas.get_gizmos_manager()->get_icon_texture_id(GLGizmosManager::MENU_ICON_NAME::IC_TOOLBAR_TOOLTIP);
    ImTextureID hover_id = canvas.get_gizmos_manager()->get_icon_texture_id(GLGizmosManager::MENU_ICON_NAME::IC_TOOLBAR_TOOLTIP_HOVER);

    ImGuiWrapper& imgui = global_im_gui();
    float caption_max = 0.f;
    for (auto caption_text : captions_texts) {
        caption_max = std::max(imgui.calc_text_size(caption_text.first).x, caption_max);
    }
    caption_max += GImGui->Style.WindowPadding.x + imgui.scaled(1);

	float  scale       = canvas.get_scale();
    ImVec2 button_size = ImVec2(25 * scale, 25 * scale); // ORCA: Use exact resolution will prevent blur on icon
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0}); // ORCA: Dont add padding
    ImGui::ImageButton3(normal_id, hover_id, button_size);

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip2(ImVec2(x, y));
        auto draw_text_with_caption = [this, &caption_max, &imgui](const wxString& caption, const wxString& text) {
            imgui.text_colored(ImGuiWrapper::COL_ACTIVE, caption);
            ImGui::SameLine(caption_max);
            imgui.text_colored(ImGuiWrapper::COL_WINDOW_BG, text);
        };

        for (const auto& caption_text : captions_texts) draw_text_with_caption(caption_text.first, caption_text.second);

        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar(2);
}

void LayersEditing::render_variable_layer_height_dialog(const GLCanvas3D& canvas) {
    if (!m_enabled)
        return;

    ImGuiWrapper& imgui = global_im_gui();
    const Size& cnv_size = canvas.get_canvas_size();
    float left_pos = canvas.m_main_toolbar.get_item("layersediting")->render_left_pos;
    const float x = (1 + left_pos) * cnv_size.get_width() / 2;
    imgui.set_next_window_pos(x, canvas.m_main_toolbar.get_height(), ImGuiCond_Always, 0.0f, 0.0f);

    imgui.push_toolbar_style(canvas.get_scale());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * canvas.get_scale(), 4.0f * canvas.get_scale()));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f * canvas.get_scale(), 10.0f * canvas.get_scale()));
    imgui.begin(_L("Variable layer height"), ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    const float sliders_width = imgui.scaled(7.0f);
    const float input_box_width = 1.5 * imgui.get_slider_icon_size().x;

    if (imgui.button(_L("Adaptive")))
        wxPostEvent((wxEvtHandler*)canvas.get_wxglcanvas(), Event<float>(EVT_GLCANVAS_ADAPTIVE_LAYER_HEIGHT_PROFILE, m_adaptive_quality));
    ImGui::SameLine();
    static float text_align = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    text_align = std::max(text_align, ImGui::GetCursorPosX());
    ImGui::SetCursorPosX(text_align);
    imgui.text(_L("Quality / Speed"));
    if (ImGui::IsItemHovered()) {
        //ImGui::BeginTooltip();
        //ImGui::TextUnformatted(_L("Higher print quality versus higher print speed.").ToUTF8());
        //ImGui::EndTooltip();
    }
    ImGui::SameLine();
    static float slider_align = ImGui::GetCursorPosX();
    ImGui::PushItemWidth(sliders_width);
    m_adaptive_quality = std::clamp(m_adaptive_quality, 0.0f, 1.f);
    slider_align = std::max(slider_align, ImGui::GetCursorPosX());
    ImGui::SetCursorPosX(slider_align);
    imgui.bbl_slider_float_style("##adaptive_slider", &m_adaptive_quality, 0.0f, 1.f, "%.2f");
    ImGui::SameLine();
    static float input_align = ImGui::GetCursorPosX();
    ImGui::PushItemWidth(input_box_width);
    input_align = std::max(input_align, ImGui::GetCursorPosX());
    ImGui::SetCursorPosX(input_align);
    ImGui::BBLDragFloat("##adaptive_input", &m_adaptive_quality, 0.05f, 0.0f, 0.0f, "%.2f");

    if (imgui.button(_L("Smooth")))
        wxPostEvent((wxEvtHandler*)canvas.get_wxglcanvas(), HeightProfileSmoothEvent(EVT_GLCANVAS_SMOOTH_LAYER_HEIGHT_PROFILE, m_smooth_params));
    ImGui::SameLine();
    text_align = std::max(text_align, ImGui::GetCursorPosX());
    ImGui::SetCursorPosX(text_align);
    ImGui::AlignTextToFramePadding();
    imgui.text(_L("Radius"));
    ImGui::SameLine();
    slider_align = std::max(slider_align, ImGui::GetCursorPosX());
    ImGui::SetCursorPosX(slider_align);
    ImGui::PushItemWidth(sliders_width);
    int radius = (int)m_smooth_params.radius;
    int v_min = 1, v_max = 10;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(238 / 255.0f, 238 / 255.0f, 238 / 255.0f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(238 / 255.0f, 238 / 255.0f, 238 / 255.0f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.81f, 0.81f, 0.81f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.00f, 0.59f, 0.53f, 1.00f));
    if(ImGui::BBLSliderScalar("##radius_slider", ImGuiDataType_S32, &radius, &v_min, &v_max)){
        radius = std::clamp(radius, 1, 10);
        m_smooth_params.radius = (unsigned int)radius;
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::SameLine();
    input_align = std::max(input_align, ImGui::GetCursorPosX());
    ImGui::SetCursorPosX(input_align);
    ImGui::PushItemWidth(input_box_width);
    ImGui::PushStyleColor(ImGuiCol_BorderActive, ImVec4(0.00f, 0.59f, 0.53f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.00f, 0.59f, 0.53f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.00f, 0.59f, 0.53f, 0.00f));
    ImGui::BBLDragScalar("##radius_input", ImGuiDataType_S32, &radius, 1, &v_min, &v_max);
    ImGui::PopStyleColor(3);

    imgui.bbl_checkbox("##keep_min", m_smooth_params.keep_min);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    imgui.text(_L("Keep min"));

    ImGui::Separator();

    float get_cur_y = ImGui::GetContentRegionMax().y + ImGui::GetFrameHeight() + canvas.m_main_toolbar.get_height();
    std::map<wxString, wxString> captions_texts = {
        {_L("Left mouse button:") ,_L("Add detail")},
        {_L("Right mouse button:"), _L("Remove detail")},
        {_L("Shift + Left mouse button:"),_L("Reset to base")},
        {_L("Shift + Right mouse button:"), _L("Smoothing")},
        {_L("Mouse wheel:"), _L("Increase/decrease edit area")}
    };
    show_tooltip_information(canvas, captions_texts, x, get_cur_y);
    ImGui::SameLine();
    if (imgui.button(_L("Reset")))
        wxPostEvent((wxEvtHandler*)canvas.get_wxglcanvas(), SimpleEvent(EVT_GLCANVAS_RESET_LAYER_HEIGHT_PROFILE));

    LayersEditing::s_overlay_window_width = ImGui::GetWindowSize().x;
    imgui.end();
    ImGui::PopStyleVar(2);
    imgui.pop_toolbar_style();
}

void LayersEditing::render_overlay(const GLCanvas3D& canvas)
{
    render_variable_layer_height_dialog(canvas);
    render_active_object_annotations(canvas);
    render_profile(canvas);
}

float LayersEditing::get_cursor_z_relative(const GLCanvas3D& canvas)
{
    const Vec2d mouse_pos = canvas.get_local_mouse_position();
    const Rect& rect = get_bar_rect_screen(canvas);
    float x = (float)mouse_pos.x();
    float y = (float)mouse_pos.y();
    float t = rect.get_top();
    float b = rect.get_bottom();

    return (rect.get_left() <= x && x <= rect.get_right() && t <= y && y <= b) ?
        // Inside the bar.
        (b - y - 1.0f) / (b - t - 1.0f) :
        // Outside the bar.
        -1000.0f;
}

bool LayersEditing::bar_rect_contains(const GLCanvas3D& canvas, float x, float y)
{
    const Rect& rect = get_bar_rect_screen(canvas);
    return rect.get_left() <= x && x <= rect.get_right() && rect.get_top() <= y && y <= rect.get_bottom();
}

Rect LayersEditing::get_bar_rect_screen(const GLCanvas3D& canvas)
{
    const Size& cnv_size = canvas.get_canvas_size();
    float w = (float)cnv_size.get_width();
    float h = (float)cnv_size.get_height();

    return { w - thickness_bar_width(canvas), 0.0f, w, h };
}

bool LayersEditing::is_initialized() const
{
    return get_shader("variable_layer_height") != nullptr;
}

std::string LayersEditing::get_tooltip(const GLCanvas3D& canvas) const
{
    std::string ret;
    if (m_enabled && m_layer_height_profile.size() >= 4) {
        float z = get_cursor_z_relative(canvas);
        if (z != -1000.0f) {
            z *= m_object_max_z;

            float h = 0.0f;
            for (size_t i = m_layer_height_profile.size() - 2; i >= 2; i -= 2) {
                const float zi = static_cast<float>(m_layer_height_profile[i]);
                const float zi_1 = static_cast<float>(m_layer_height_profile[i - 2]);
                if (zi_1 <= z && z <= zi) {
                    float dz = zi - zi_1;
                    h = (dz != 0.0f) ? static_cast<float>(lerp(m_layer_height_profile[i - 1], m_layer_height_profile[i + 1], (z - zi_1) / dz)) :
                        static_cast<float>(m_layer_height_profile[i + 1]);
                    break;
                }
            }
            if (h > 0.0f)
                ret = wxString::Format("%.3f",h).ToStdString();
        }
    }
    return ret;
}

void LayersEditing::render_active_object_annotations(const GLCanvas3D& canvas)
{
    if (!m_enabled)
        return;

    const Size cnv_size = canvas.get_canvas_size();
    const float cnv_width  = (float)cnv_size.get_width();
    const float cnv_height = (float)cnv_size.get_height();
    if (cnv_width == 0.0f || cnv_height == 0.0f)
        return;

    const float cnv_inv_width = 1.0f / cnv_width;

    GLShaderProgram* shader = get_shader("variable_layer_height");
    if (shader == nullptr)
        return;

    shader->start_using();

    shader->set_uniform("z_to_texture_row", float(m_layers_texture.cells - 1) / (float(m_layers_texture.width) * m_object_max_z));
    shader->set_uniform("z_texture_row_to_normalized", 1.0f / (float)m_layers_texture.height);
    shader->set_uniform("z_cursor", m_object_max_z * this->get_cursor_z_relative(canvas));
    shader->set_uniform("z_cursor_band_width", band_width);
    shader->set_uniform("object_max_z", m_object_max_z);
    shader->set_uniform("view_model_matrix", Transform3d::Identity());
    shader->set_uniform("projection_matrix", Transform3d::Identity());
    shader->set_uniform("view_normal_matrix", (Matrix3d)Matrix3d::Identity());

    glsafe(::glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    glsafe(::glBindTexture(GL_TEXTURE_2D, m_z_texture_id));

    // Render the color bar
    if (!m_profile.background.is_initialized() || m_profile.old_canvas_width != cnv_width) {
        m_profile.old_canvas_width = cnv_width;
        m_profile.background.reset();

        GLModel::Geometry init_data;
        init_data.format = { GLModel::Geometry::EPrimitiveType::Triangles, GLModel::Geometry::EVertexLayout::P3N3T2 };
        init_data.reserve_vertices(4);
        init_data.reserve_indices(6);

        // vertices
        const float l = 1.0f - 2.0f * THICKNESS_BAR_WIDTH * cnv_inv_width;
        const float r = 1.0f;
        const float t = 1.0f;
        const float b = -1.0f;
        init_data.add_vertex(Vec3f(l, b, 0.0f), Vec3f::UnitZ(), Vec2f(0.0f, 0.0f));
        init_data.add_vertex(Vec3f(r, b, 0.0f), Vec3f::UnitZ(), Vec2f(1.0f, 0.0f));
        init_data.add_vertex(Vec3f(r, t, 0.0f), Vec3f::UnitZ(), Vec2f(1.0f, 1.0f));
        init_data.add_vertex(Vec3f(l, t, 0.0f), Vec3f::UnitZ(), Vec2f(0.0f, 1.0f));

        // indices
        init_data.add_triangle(0, 1, 2);
        init_data.add_triangle(2, 3, 0);

        m_profile.background.init_from(std::move(init_data));
    }

    m_profile.background.render();

    glsafe(::glBindTexture(GL_TEXTURE_2D, 0));

    shader->stop_using();
}

void LayersEditing::render_profile(const GLCanvas3D& canvas)
{
    if (!m_enabled)
        return;

    //FIXME show some kind of legend.

    if (!m_slicing_parameters)
        return;

    const Size cnv_size = canvas.get_canvas_size();
    const float cnv_width  = (float)cnv_size.get_width();
    const float cnv_height = (float)cnv_size.get_height();
    if (cnv_width == 0.0f || cnv_height == 0.0f)
        return;

    // Make the vertical bar a bit wider so the layer height curve does not touch the edge of the bar region.
    const float scale_x = THICKNESS_BAR_WIDTH / float(1.12 * m_slicing_parameters->max_layer_height);
    const float scale_y = cnv_height / m_object_max_z;

    const float cnv_inv_width  = 1.0f / cnv_width;
    const float cnv_inv_height = 1.0f / cnv_height;

    // Baseline
    if (!m_profile.baseline.is_initialized() || m_profile.old_layer_height_profile != m_layer_height_profile) {
        m_profile.baseline.reset();

        GLModel::Geometry init_data;
        init_data.format = { GLModel::Geometry::EPrimitiveType::Lines, GLModel::Geometry::EVertexLayout::P2};
        init_data.color = ColorRGBA::BLACK();
        init_data.reserve_vertices(2);
        init_data.reserve_indices(2);

        // vertices
        const float axis_x = 2.0f * ((cnv_width - THICKNESS_BAR_WIDTH + float(m_slicing_parameters->layer_height) * scale_x) * cnv_inv_width - 0.5f);
        init_data.add_vertex(Vec2f(axis_x, -1.0f));
        init_data.add_vertex(Vec2f(axis_x, 1.0f));

        // indices
        init_data.add_line(0, 1);

        m_profile.baseline.init_from(std::move(init_data));
    }

    if (!m_profile.profile.is_initialized() || m_profile.old_layer_height_profile != m_layer_height_profile) {
        m_profile.old_layer_height_profile = m_layer_height_profile;
        m_profile.profile.reset();

        GLModel::Geometry init_data;
        init_data.format = { GLModel::Geometry::EPrimitiveType::LineStrip, GLModel::Geometry::EVertexLayout::P2 };
        init_data.color = ColorRGBA::BLUE();
        init_data.reserve_vertices(m_layer_height_profile.size() / 2);
        init_data.reserve_indices(m_layer_height_profile.size() / 2);

        // vertices + indices
        for (unsigned int i = 0; i < (unsigned int)m_layer_height_profile.size(); i += 2) {
            init_data.add_vertex(Vec2f(2.0f * ((cnv_width - THICKNESS_BAR_WIDTH + float(m_layer_height_profile[i + 1]) * scale_x) * cnv_inv_width - 0.5f),
                                       2.0f * (float(m_layer_height_profile[i]) * scale_y * cnv_inv_height - 0.5)));
            init_data.add_index(i / 2);
        }

        m_profile.profile.init_from(std::move(init_data));
    }

    GLShaderProgram* shader = get_shader("flat");
    if (shader != nullptr) {
        shader->start_using();
        shader->set_uniform("view_model_matrix", Transform3d::Identity());
        shader->set_uniform("projection_matrix", Transform3d::Identity());
        m_profile.baseline.render();
        m_profile.profile.render();
        shader->stop_using();
    }
}

void LayersEditing::render_volumes(const GLCanvas3D& canvas, const GLVolumeCollection& volumes)
{
    assert(this->is_allowed());
    assert(this->last_object_id != -1);

    GLShaderProgram* current_shader = get_current_shader();
    ScopeGuard guard([current_shader]() { if (current_shader != nullptr) current_shader->start_using(); });
    if (current_shader != nullptr)
        current_shader->stop_using();

    GLShaderProgram* shader = get_shader("variable_layer_height");
    if (shader == nullptr)
        return;

    shader->start_using();

    generate_layer_height_texture();

    // Uniforms were resolved, go ahead using the layer editing shader.
    shader->set_uniform("z_to_texture_row", float(m_layers_texture.cells - 1) / (float(m_layers_texture.width) * float(m_object_max_z)));
    shader->set_uniform("z_texture_row_to_normalized", 1.0f / float(m_layers_texture.height));
    shader->set_uniform("z_cursor", float(m_object_max_z) * float(this->get_cursor_z_relative(canvas)));
    shader->set_uniform("z_cursor_band_width", float(this->band_width));

    const Camera& camera = AppAdapter::plater()->get_camera();
    shader->set_uniform("projection_matrix", camera.get_projection_matrix());

    // Initialize the layer height texture mapping.
    const GLsizei w = (GLsizei)m_layers_texture.width;
    const GLsizei h = (GLsizei)m_layers_texture.height;
    const GLsizei half_w = w / 2;
    const GLsizei half_h = h / 2;
    glsafe(::glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    glsafe(::glBindTexture(GL_TEXTURE_2D, m_z_texture_id));
    glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
    glsafe(::glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, half_w, half_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
    glsafe(::glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, m_layers_texture.data.data()));
    glsafe(::glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, half_w, half_h, GL_RGBA, GL_UNSIGNED_BYTE, m_layers_texture.data.data() + m_layers_texture.width * m_layers_texture.height * 4));
    for (GLVolume* glvolume : volumes.volumes) {
        // Render the object using the layer editing shader and texture.
        if (!glvolume->is_active || glvolume->composite_id.object_id != this->last_object_id || glvolume->is_modifier)
            continue;

        shader->set_uniform("volume_world_matrix", glvolume->world_matrix());
        shader->set_uniform("object_max_z", 0.0f);
        const Transform3d& view_matrix = camera.get_view_matrix();
        const Transform3d model_matrix = glvolume->world_matrix();
        shader->set_uniform("view_model_matrix", view_matrix * model_matrix);
        const Matrix3d view_normal_matrix = view_matrix.matrix().block(0, 0, 3, 3) * model_matrix.matrix().block(0, 0, 3, 3).inverse().transpose();
        shader->set_uniform("view_normal_matrix", view_normal_matrix);

        glvolume->render();
    }
    // Revert back to the previous shader.
    glBindTexture(GL_TEXTURE_2D, 0);
}

void LayersEditing::adjust_layer_height_profile()
{
    this->update_slicing_parameters();
    PrintObject::update_layer_height_profile(*m_model_object, *m_slicing_parameters, m_layer_height_profile);
    Slic3r::adjust_layer_height_profile(*m_model_object, *m_slicing_parameters, m_layer_height_profile, this->last_z, this->strength, this->band_width, this->last_action);
    m_layers_texture.valid = false;
}

void LayersEditing::reset_layer_height_profile(GLCanvas3D & canvas)
{
    const_cast<ModelObject*>(m_model_object)->layer_height_profile.clear();
    m_layer_height_profile.clear();
    m_layers_texture.valid = false;
    canvas.post_event(SimpleEvent(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS));
    AppAdapter::obj_list()->update_info_items(last_object_id);
}

void LayersEditing::adaptive_layer_height_profile(GLCanvas3D & canvas, float quality_factor)
{
    this->update_slicing_parameters();
    m_layer_height_profile = layer_height_profile_adaptive(*m_slicing_parameters, *m_model_object, quality_factor);
    const_cast<ModelObject*>(m_model_object)->layer_height_profile.set(m_layer_height_profile);
    m_layers_texture.valid = false;
    canvas.post_event(SimpleEvent(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS));
    AppAdapter::obj_list()->update_info_items(last_object_id);
}

void LayersEditing::smooth_layer_height_profile(GLCanvas3D & canvas, const HeightProfileSmoothingParams & smoothing_params)
{
    this->update_slicing_parameters();
    m_layer_height_profile = smooth_height_profile(m_layer_height_profile, *m_slicing_parameters, smoothing_params);
    const_cast<ModelObject*>(m_model_object)->layer_height_profile.set(m_layer_height_profile);
    m_layers_texture.valid = false;
    canvas.post_event(SimpleEvent(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS));
    AppAdapter::obj_list()->update_info_items(last_object_id);
}

void LayersEditing::generate_layer_height_texture()
{
    this->update_slicing_parameters();
    // Always try to update the layer height profile.
    bool update = !m_layers_texture.valid;
    if (PrintObject::update_layer_height_profile(*m_model_object, *m_slicing_parameters, m_layer_height_profile)) {
        // Initialized to the default value.
        update = true;
    }
    // Update if the layer height profile was changed, or when the texture is not valid.
    if (!update && !m_layers_texture.data.empty() && m_layers_texture.cells > 0)
        // Texture is valid, don't update.
        return;

    if (m_layers_texture.data.empty()) {
        m_layers_texture.width = 1024;
        m_layers_texture.height = 1024;
        m_layers_texture.levels = 2;
        m_layers_texture.data.assign(m_layers_texture.width * m_layers_texture.height * 5, 0);
    }

    bool level_of_detail_2nd_level = true;
    m_layers_texture.cells = Slic3r::generate_layer_height_texture(
        *m_slicing_parameters,
        Slic3r::generate_object_layers(*m_slicing_parameters, m_layer_height_profile, false),
        m_layers_texture.data.data(), m_layers_texture.height, m_layers_texture.width, level_of_detail_2nd_level);
    m_layers_texture.valid = true;
}

void LayersEditing::accept_changes(GLCanvas3D & canvas)
{
    if (last_object_id >= 0) {
        AppAdapter::plater()->take_snapshot("Variable layer height - Manual edit");
        const_cast<ModelObject*>(m_model_object)->layer_height_profile.set(m_layer_height_profile);
        canvas.post_event(SimpleEvent(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS));
        AppAdapter::obj_list()->update_info_items(last_object_id);
    }
}

void LayersEditing::update_slicing_parameters()
{
    if (m_slicing_parameters == nullptr) {
        m_slicing_parameters = new SlicingParameters();
        *m_slicing_parameters = PrintObject::slicing_parameters(*m_config, *m_model_object, m_object_max_z, m_shrinkage_compensation);
    }
    
}

float LayersEditing::thickness_bar_width(const GLCanvas3D & canvas)
{
    return
#if ENABLE_RETINA_GL
        canvas.get_canvas_size().get_scale_factor()
#else
        canvas.get_wxglcanvas()->GetContentScaleFactor()
#endif
        * THICKNESS_BAR_WIDTH;
}

}
}
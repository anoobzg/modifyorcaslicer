#include "RenderHelpers.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Tesselate.hpp"
#include "libslic3r/PresetBundle.hpp"

#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/GUI/Event/UserCanvasEvent.hpp"

#include <imgui/imgui_internal.h>

namespace Slic3r {
namespace GUI {

int RenderStats::get_fps_and_reset_if_needed() 
{
    auto cur_time = std::chrono::high_resolution_clock::now();
    int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time-m_measuring_start).count();
    if (elapsed_ms > 1000  || m_fps_out == -1) {
        m_measuring_start = cur_time;
        m_fps_out = int (1000. * m_fps_running / elapsed_ms);
        m_fps_running = 0;
    }
    return m_fps_out;
}

const Point MouseHelper::Drag::Invalid_2D_Point(INT_MAX, INT_MAX);
const Vec3d MouseHelper::Drag::Invalid_3D_Point(DBL_MAX, DBL_MAX, DBL_MAX);
const int MouseHelper::Drag::MoveThresholdPx = 5;

MouseHelper::Drag::Drag()
    : start_position_2D(Invalid_2D_Point)
    , start_position_3D(Invalid_3D_Point)
    , move_volume_idx(-1)
    , move_requires_threshold(false)
    , move_start_threshold_position_2D(Invalid_2D_Point)
{
}

MouseHelper::MouseHelper()
    : dragging(false)
    , position(DBL_MAX, DBL_MAX)
    , scene_position(DBL_MAX, DBL_MAX, DBL_MAX)
    , ignore_left_up(false)
    , ignore_right_up(false)
{
}

void LabelsHelper::render(const std::vector<const ModelInstance*>& sorted_instances) const
{
    if (!m_enabled || !is_shown() || m_canvas.get_gizmos_manager()->is_running())
        return;

    const Camera& camera = AppAdapter::plater()->get_camera();
    const Model* model = m_canvas.get_model();
    if (model == nullptr)
        return;

    Transform3d world_to_eye = camera.get_view_matrix();
    Transform3d world_to_screen = camera.get_projection_matrix() * world_to_eye;
    const std::array<int, 4>& viewport = camera.get_viewport();

    struct Owner
    {
        int obj_idx;
        int inst_idx;
        size_t model_instance_id;
        BoundingBoxf3 world_box;
        double eye_center_z;
        std::string title;
        std::string label;
        std::string print_order;
        bool selected;
    };

    // collect owners world bounding boxes and data from volumes
    std::vector<Owner> owners;
    const GLVolumeCollection& volumes = m_canvas.get_volumes();
    PartPlate* cur_plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
    for (const GLVolume* volume : volumes.volumes) {
        int obj_idx = volume->object_idx();
        if (0 <= obj_idx && obj_idx < (int)model->objects.size()) {
            int inst_idx = volume->instance_idx();
            //only show current plate's label
            if (!cur_plate->contain_instance(obj_idx, inst_idx))
                continue;
            std::vector<Owner>::iterator it = std::find_if(owners.begin(), owners.end(), [obj_idx, inst_idx](const Owner& owner) {
                return (owner.obj_idx == obj_idx) && (owner.inst_idx == inst_idx);
                });
            if (it != owners.end()) {
                it->world_box.merge(volume->transformed_bounding_box());
                it->selected &= volume->selected;
            } else {
                const ModelObject* model_object = model->objects[obj_idx];
                Owner owner;
                owner.obj_idx = obj_idx;
                owner.inst_idx = inst_idx;
                owner.model_instance_id = model_object->instances[inst_idx]->id().id;
                owner.world_box = volume->transformed_bounding_box();
                owner.title = "object" + std::to_string(obj_idx) + "_inst##" + std::to_string(inst_idx);
                owner.label = model_object->name;
                if (model_object->instances.size() > 1)
                    owner.label += " (" + std::to_string(inst_idx + 1) + ")";
                owner.selected = volume->selected;
                owners.emplace_back(owner);
            }
        }
    }

    // updates print order strings
    if (sorted_instances.size() > 0) {
        for (size_t i = 0; i < sorted_instances.size(); ++i) {
            size_t id = sorted_instances[i]->id().id;
            std::vector<Owner>::iterator it = std::find_if(owners.begin(), owners.end(), [id](const Owner& owner) {
                return owner.model_instance_id == id;
                });
            if (it != owners.end())
                //it->print_order = std::string((_(L("Sequence"))).ToUTF8()) + "#: " + std::to_string(i + 1);
                it->print_order = std::string((_(L("Sequence"))).ToUTF8()) + "#: " + std::to_string(sorted_instances[i]->arrange_order);
        }
    }

    // calculate eye bounding boxes center zs
    for (Owner& owner : owners) {
        owner.eye_center_z = (world_to_eye * owner.world_box.center())(2);
    }

    // sort owners by center eye zs and selection
    std::sort(owners.begin(), owners.end(), [](const Owner& owner1, const Owner& owner2) {
        if (!owner1.selected && owner2.selected)
            return true;
        else if (owner1.selected && !owner2.selected)
            return false;
        else
            return (owner1.eye_center_z < owner2.eye_center_z);
        });

    ImGuiWrapper& imgui = global_im_gui();

    // render info windows
    for (const Owner& owner : owners) {
        Vec3d screen_box_center = world_to_screen * owner.world_box.center();
        float x = 0.0f;
        float y = 0.0f;
        if (camera.get_type() == Camera::EType::Perspective) {
            x = (0.5f + 0.001f * 0.5f * (float)screen_box_center(0)) * viewport[2];
            y = (0.5f - 0.001f * 0.5f * (float)screen_box_center(1)) * viewport[3];
        } else {
            x = (0.5f + 0.5f * (float)screen_box_center(0)) * viewport[2];
            y = (0.5f - 0.5f * (float)screen_box_center(1)) * viewport[3];
        }

        if (x < 0.0f || viewport[2] < x || y < 0.0f || viewport[3] < y)
            continue;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, owner.selected ? 3.0f : 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, owner.selected ? ImVec4(0.757f, 0.404f, 0.216f, 1.0f) : ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        imgui.set_next_window_pos(x, y, ImGuiCond_Always, 0.5f, 0.5f);
        imgui.begin(owner.title, ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        float win_w = ImGui::GetWindowWidth();
        ImGui::AlignTextToFramePadding();
        imgui.text(owner.label);

        if (!owner.print_order.empty()) {
            ImGui::Separator();
            float po_len = imgui.calc_text_size(owner.print_order).x;
            ImGui::SetCursorPosX(0.5f * (win_w - po_len));
            ImGui::AlignTextToFramePadding();
            imgui.text(owner.print_order);
        }

        // force re-render while the windows gets to its final size (it takes several frames)
        if (ImGui::GetWindowContentRegionWidth() + 2.0f * ImGui::GetStyle().WindowPadding.x != ImGui::CalcWindowNextAutoFitSize(ImGui::GetCurrentWindow()).x)
            imgui.set_requires_extra_frame();

        imgui.end();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

void TooltipHelper::set_text(const std::string& text)
{
    // If the mouse is inside an ImGUI dialog, then the tooltip is suppressed.
    m_text = m_in_imgui ? std::string() : text;
}

void TooltipHelper::render(const Vec2d& mouse_position, GLCanvas3D& canvas)
{
    static ImVec2 size(0.0f, 0.0f);

    auto validate_position = [](const Vec2d& position, const GLCanvas3D& canvas, const ImVec2& wnd_size) {
        const Size cnv_size = canvas.get_canvas_size();
        const float x = std::clamp((float)position.x(), 0.0f, (float)cnv_size.get_width() - wnd_size.x);
        const float y = std::clamp((float)position.y() + 16.0f, 0.0f, (float)cnv_size.get_height() - wnd_size.y);
        return Vec2f(x, y);
    };

    if (m_text.empty()) {
        m_start_time = std::chrono::steady_clock::now();
        return;
    }

    // draw the tooltip as hidden until the delay is expired
    // use a value of alpha slightly different from 0.0f because newer imgui does not calculate properly the window size if alpha == 0.0f
    const float alpha = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start_time).count() < 500) ? 0.01f : 1.0f;

    const Vec2f position = validate_position(mouse_position, canvas, size);

    ImGuiWrapper& imgui = global_im_gui();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    imgui.set_next_window_pos(position.x(), position.y(), ImGuiCond_Always, 0.0f, 0.0f);

    imgui.begin(wxString("canvas_tooltip"), ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImGui::TextUnformatted(m_text.c_str());

    if (alpha < 1.0f || ImGui::GetWindowContentRegionWidth() + 2.0f * ImGui::GetStyle().WindowPadding.x != ImGui::CalcWindowNextAutoFitSize(ImGui::GetCurrentWindow()).x)
        imgui.set_requires_extra_frame();

    size = ImGui::GetWindowSize();

    imgui.end();
    ImGui::PopStyleVar(2);
}

//BBS: add height limit logic
void SequentialPrintClearance::set_polygons(const Polygons& polygons, const std::vector<std::pair<Polygon, float>>& height_polygons)
{
    //BBS: add height limit logic
    m_height_limit.reset();
    m_perimeter.reset();
    m_fill.reset();
    if (!polygons.empty()) {
        if (m_render_fill) {
            GLModel::Geometry fill_data;
            fill_data.format = { GLModel::Geometry::EPrimitiveType::Triangles, GLModel::Geometry::EVertexLayout::P3 };
            fill_data.color  = { 0.8f, 0.8f, 1.0f, 0.5f };

            // vertices + indices
            const ExPolygons polygons_union = union_ex(polygons);
            unsigned int vertices_counter = 0;
            for (const ExPolygon& poly : polygons_union) {
                const std::vector<Vec3d> triangulation = triangulate_expolygon_3d(poly);
                fill_data.reserve_vertices(fill_data.vertices_count() + triangulation.size());
                fill_data.reserve_indices(fill_data.indices_count() + triangulation.size());
                for (const Vec3d& v : triangulation) {
                    fill_data.add_vertex((Vec3f)(v.cast<float>() + 0.0125f * Vec3f::UnitZ())); // add a small positive z to avoid z-fighting
                    ++vertices_counter;
                    if (vertices_counter % 3 == 0)
                        fill_data.add_triangle(vertices_counter - 3, vertices_counter - 2, vertices_counter - 1);
                }
            }

            m_fill.init_from(std::move(fill_data));
        }

        m_perimeter.init_from(polygons, 0.025f); // add a small positive z to avoid z-fighting
    }

    //BBS: add the height limit compute logic
    if (!height_polygons.empty()) {
        GLModel::Geometry height_fill_data;
        height_fill_data.format = { GLModel::Geometry::EPrimitiveType::Triangles, GLModel::Geometry::EVertexLayout::P3 };
        height_fill_data.color  = {0.8f, 0.8f, 1.0f, 0.5f};

        // vertices + indices
        unsigned int vertices_counter = 0;
        for (const auto &poly : height_polygons) {
            ExPolygon                ex_poly(poly.first);
            const std::vector<Vec3d> height_triangulation = triangulate_expolygon_3d(ex_poly);
            for (const Vec3d &v : height_triangulation) {
                Vec3f point{(float) v.x(), (float) v.y(), poly.second};
                height_fill_data.add_vertex(point);
                ++vertices_counter;
                if (vertices_counter % 3 == 0)
                    height_fill_data.add_triangle(vertices_counter - 3, vertices_counter - 2, vertices_counter - 1);
            }
        }

        m_height_limit.init_from(std::move(height_fill_data));
    }
}

void SequentialPrintClearance::render()
{
    const ColorRGBA FILL_COLOR = { 0.7f, 0.7f, 1.0f, 0.5f };
    const ColorRGBA NO_FILL_COLOR = { 0.75f, 0.75f, 0.75f, 0.75f };

    GLShaderProgram* shader = get_shader("flat");
    if (shader == nullptr)
        return;

    shader->start_using();

    const Camera& camera = AppAdapter::plater()->get_camera();
    shader->set_uniform("view_model_matrix", camera.get_view_matrix());
    shader->set_uniform("projection_matrix", camera.get_projection_matrix());

    glsafe(::glEnable(GL_DEPTH_TEST));
    glsafe(::glDisable(GL_CULL_FACE));
    glsafe(::glEnable(GL_BLEND));
    glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    m_perimeter.set_color(m_render_fill ? FILL_COLOR : NO_FILL_COLOR);
    m_perimeter.render();
    m_fill.render();
    //BBS: add height limit
    m_height_limit.set_color(m_render_fill ? FILL_COLOR : NO_FILL_COLOR);
    m_height_limit.render();

    glsafe(::glDisable(GL_BLEND));
    glsafe(::glEnable(GL_CULL_FACE));
    glsafe(::glDisable(GL_DEPTH_TEST));

    shader->stop_using();
}

void ToolbarHighlighter::set_timer_owner(wxEvtHandler* owner, int timerid/* = wxID_ANY*/)
{
    m_timer.SetOwner(owner, timerid);
}

void ToolbarHighlighter::init(GLToolbarItem* toolbar_item, GLCanvas3D* canvas)
{
    if (m_timer.IsRunning())
        invalidate();
    if (!toolbar_item || !canvas)
        return;

    m_timer.Start(300, false);

    m_toolbar_item = toolbar_item;
    m_canvas       = canvas;
}

void ToolbarHighlighter::invalidate()
{
    m_timer.Stop();

    if (m_toolbar_item) {
        m_toolbar_item->set_highlight(GLToolbarItem::EHighlightState::NotHighlighted);
    }
    m_toolbar_item = nullptr;
    m_blink_counter = 0;
    m_render_arrow = false;
}

void ToolbarHighlighter::blink()
{
    if (m_toolbar_item) {
        char state = m_toolbar_item->get_highlight();
        if (state != (char)GLToolbarItem::EHighlightState::HighlightedShown)
            m_toolbar_item->set_highlight(GLToolbarItem::EHighlightState::HighlightedShown);
        else
            m_toolbar_item->set_highlight(GLToolbarItem::EHighlightState::HighlightedHidden);

        m_render_arrow = !m_render_arrow;
        m_canvas->set_as_dirty();
    }
    else
        invalidate();

    if ((++m_blink_counter) >= 11)
        invalidate();
}

void RenderTimer::Notify()
{
    wxPostEvent((wxEvtHandler*)GetOwner(), RenderTimerEvent( EVT_GLCANVAS_RENDER_TIMER, *this));
}

void ToolbarHighlighterTimer::Notify()
{
    wxPostEvent((wxEvtHandler*)GetOwner(), ToolbarHighlighterTimerEvent(EVT_GLCANVAS_TOOLBAR_HIGHLIGHTER_TIMER, *this));
}

void GizmoHighlighterTimer::Notify()
{
    wxPostEvent((wxEvtHandler*)GetOwner(), GizmoHighlighterTimerEvent(EVT_GLCANVAS_GIZMO_HIGHLIGHTER_TIMER, *this));
}

void GizmoHighlighter::set_timer_owner(wxEvtHandler* owner, int timerid/* = wxID_ANY*/)
{
    m_timer.SetOwner(owner, timerid);
}

void GizmoHighlighter::init(GLGizmosManager* manager, GLGizmosManager::EType gizmo, GLCanvas3D* canvas)
{
    if (m_timer.IsRunning())
        invalidate();
    if (!gizmo || !canvas)
        return;

    m_timer.Start(300, false);

    m_gizmo_manager = manager;
    m_gizmo_type    = gizmo;
    m_canvas        = canvas;
}

void GizmoHighlighter::invalidate()
{
    m_timer.Stop();

    if (m_gizmo_manager) {
        m_gizmo_manager->set_highlight(GLGizmosManager::EType::Undefined, false);
    }
    m_gizmo_manager = nullptr;
    m_gizmo_type = GLGizmosManager::EType::Undefined;
    m_blink_counter = 0;
    m_render_arrow = false;
}

void GizmoHighlighter::blink()
{
    if (m_gizmo_manager) {
        if (m_blink_counter % 2 == 0)
            m_gizmo_manager->set_highlight(m_gizmo_type, true);
        else
            m_gizmo_manager->set_highlight(m_gizmo_type, false);

        m_render_arrow = !m_render_arrow;
        m_canvas->set_as_dirty();
    }
    else
        invalidate();

    if ((++m_blink_counter) >= 11)
        invalidate();
}

void WipeTowerInfoHelper::apply_wipe_tower() const
{
    // BBS: add partplate logic
    DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
    Vec3d plate_origin = AppAdapter::plater()->get_partplate_list().get_plate(m_plate_idx)->get_origin();
    ConfigOptionFloat wipe_tower_x(m_pos(X) - plate_origin(0));
    ConfigOptionFloat wipe_tower_y(m_pos(Y) - plate_origin(1));

    ConfigOptionFloats* wipe_tower_x_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_x", true);
    ConfigOptionFloats* wipe_tower_y_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_y", true);
    wipe_tower_x_opt->set_at(&wipe_tower_x, m_plate_idx, 0);
    wipe_tower_y_opt->set_at(&wipe_tower_y, m_plate_idx, 0);

    //q->update();
}

}
}
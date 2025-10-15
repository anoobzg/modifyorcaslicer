#include "GCodeRenderer.hpp"
#include "GCodeDefine.hpp"
#include "GCodeViewerData.hpp"
#include "slic3r/Render/3DScene.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/Render/GLShader.hpp"
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Render/RenderUtils.hpp"
#include <GL/glew.h>
#include <imgui/imgui_internal.h>

namespace Slic3r {
namespace GUI {
namespace GCode {

struct RenderPathPropertyEqual {
    bool operator() (const RenderPath &l, const RenderPath &r) const {
        return l.tbuffer_id == r.tbuffer_id && l.ibuffer_id == r.ibuffer_id && l.color == r.color;
    }
};

// helper to render shells
struct Shells
{
    GLVolumeCollection volumes;
    bool visible{ false };
    //BBS: always load shell when preview
    int print_id{ -1 };
    int print_modify_count { -1 };
    bool previewing{ false };
};

GCodeRenderer::GCodeRenderer()
{
    m_shells = new Shells();
}

GCodeRenderer::~GCodeRenderer()
{
    delete m_shells;
}

void GCodeRenderer::set_render_mode(RenderMode mode)
{
    m_mode = mode;
}

void GCodeRenderer::load_shells(const Print* print, bool initialized, bool force_previewing)
{
    // BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": initialized=%1%, force_previewing=%2%")%initialized %force_previewing;
    if ((print->id().id == m_shells->print_id)&&(print->get_modified_count() == m_shells->print_modify_count)) {
        //BBS: update force previewing logic
        if (force_previewing)
            m_shells->previewing = force_previewing;
        //already loaded
        // BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": already loaded, print=%1% print_id=%2%, print_modify_count=%3%, force_previewing %4%")%(print) %m_shells->print_id %m_shells->print_modify_count %force_previewing;
        return;
    }

    //reset shell firstly
    reset_shell();

    //BBS: move behind of reset_shell, to clear previous shell for empty plate
    if (print->objects().empty()) {
        // no shells, return
        return;
    }
    // adds objects' volumes
    // BBS: fix the issue that object_idx is not assigned as index of Model.objects array
    int object_count = 0;
    const ModelObjectPtrs& model_objs = AppAdapter::gui_app()->model().objects;
    for (const PrintObject* obj : print->objects()) {
        const ModelObject* model_obj = obj->model_object();

        int object_idx = -1;
        for (int idx = 0; idx < model_objs.size(); idx++) {
            if (model_objs[idx]->id() == model_obj->id()) {
                object_idx = idx;
                break;
            }
        }

        // BBS: object may be deleted when this method is called when deleting an object
        if (object_idx == -1)
            continue;

        std::vector<int> instance_ids(model_obj->instances.size());
        //BBS: only add the printable instance
        int instance_index = 0;
        for (int i = 0; i < (int)model_obj->instances.size(); ++i) {
            //BBS: only add the printable instance
            if (model_obj->instances[i]->is_printable())
                instance_ids[instance_index++] = i;
        }
        instance_ids.resize(instance_index);

        size_t current_volumes_count = m_shells->volumes.volumes.size();
        m_shells->volumes.load_object(model_obj, object_idx, instance_ids, "object", initialized, false);

        // adjust shells' z if raft is present
        const SlicingParameters& slicing_parameters = obj->slicing_parameters();
        if (slicing_parameters.object_print_z_min != 0.0) {
            const Vec3d z_offset = slicing_parameters.object_print_z_min * Vec3d::UnitZ();
            for (size_t i = current_volumes_count; i < m_shells->volumes.volumes.size(); ++i) {
                GLVolume* v = m_shells->volumes.volumes[i];
                v->set_volume_offset(v->get_volume_offset() + z_offset);
            }
        }

        object_count++;
    }
     // remove modifiers
    while (true) {
        GLVolumePtrs::iterator it = std::find_if(m_shells->volumes.volumes.begin(), m_shells->volumes.volumes.end(), [](GLVolume* volume) { return volume->is_modifier; });
        if (it != m_shells->volumes.volumes.end()) {
            delete (*it);
            m_shells->volumes.volumes.erase(it);
        }
        else
            break;
    }

    for (GLVolume* volume : m_shells->volumes.volumes) {
        volume->zoom_to_volumes = false;
        volume->color.a(0.5f);
        volume->force_native_color = true;
        volume->set_render_color();
        //BBS: add shell bounding box logic
        //m_shell_bounding_box.merge(volume->transformed_bounding_box());
    }

    //BBS: always load shell when preview
    m_shells->print_id = print->id().id;
    m_shells->print_modify_count = print->get_modified_count();
    m_shells->previewing = true;
    // BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": shell loaded, id change to %1%, modify_count %2%, object count %3%, glvolume count %4%")
    //     % m_shells->print_id % m_shells->print_modify_count % object_count %m_shells->volumes.volumes.size();
}

void GCodeRenderer::update_shells_color_by_extruder(const DynamicPrintConfig *config)
{
    if (config != nullptr)
        m_shells->volumes.update_colors_by_extruder(config, false);
}

void GCodeRenderer::set_shell_transparency(float alpha) 
{
     m_shells->volumes.set_transparency(alpha); 
}

//BBS: always load shell at preview
void GCodeRenderer::reset_shell()
{
    m_shells->volumes.clear();
    m_shells->print_id = -1;
    //m_shell_bounding_box = BoundingBoxf3();
}

void GCodeRenderer::render_shells(int canvas_width, int canvas_height)
{
//BBS: add shell previewing logic
    if ((!m_shells->previewing && !m_shells->visible) || m_shells->volumes.empty())
        //if (!m_shells->visible || m_shells->volumes.empty())
        return;

    GLShaderProgram* shader = get_shader("gouraud_light");
    if (shader == nullptr)
        return;

    glsafe(::glDepthMask(GL_FALSE));

    shader->start_using();
    shader->set_uniform("emission_factor", 0.1f);
    const Camera& camera = AppAdapter::plater()->get_camera();
    shader->set_uniform("z_far", camera.get_far_z());
    shader->set_uniform("z_near", camera.get_near_z());
    m_shells->volumes.render(GLVolumeCollection::ERenderType::Transparent, false, camera.get_view_matrix(), camera.get_projection_matrix(), {canvas_width, canvas_height});
    shader->set_uniform("emission_factor", 0.0f);
    shader->stop_using();

    glsafe(::glDepthMask(GL_TRUE));
}

void GCodeRenderer::set_data(GCodeViewerData* data)
{
    m_data = data;
}

void GCodeRenderer::refresh_render_paths(bool keep_sequential_current_first, bool keep_sequential_current_last)
{
    if (m_data == NULL)
        return;

    auto extrusion_color = [this](const Path& path) {
        ColorRGBA color;
        switch ((EViewType)m_data->m_view_type)
        {
        case EViewType::FeatureType:    { color = Extrusion_Role_Colors[static_cast<unsigned int>(path.role)]; break; }
        case EViewType::Height:         { color = m_data->m_extrusions.ranges.height.get_color_at(path.height); break; }
        case EViewType::Width:          { color = m_data->m_extrusions.ranges.width.get_color_at(path.width); break; }
        case EViewType::Feedrate:       { color = m_data->m_extrusions.ranges.feedrate.get_color_at(path.feedrate); break; }
        case EViewType::FanSpeed:       { color = m_data->m_extrusions.ranges.fan_speed.get_color_at(path.fan_speed); break; }
        case EViewType::Temperature:    { color = m_data->m_extrusions.ranges.temperature.get_color_at(path.temperature); break; }
        case EViewType::LayerTime:      { color = m_data->m_extrusions.ranges.layer_duration.get_color_at(path.layer_time); break; }
        case EViewType::LayerTimeLog:   { color = m_data->m_extrusions.ranges.layer_duration_log.get_color_at(path.layer_time); break; }
        case EViewType::VolumetricRate: { color = m_data->m_extrusions.ranges.volumetric_rate.get_color_at(path.volumetric_rate); break; }
        case EViewType::Tool:           { color = m_data->m_tools.m_tool_colors[path.extruder_id]; break; }
        case EViewType::ColorPrint:     {
            if (path.cp_color_id >= static_cast<unsigned char>(m_data->m_tools.m_tool_colors.size()))
                color = ColorRGBA::GRAY();
            else {
                color = m_data->m_tools.m_tool_colors[path.cp_color_id];
                color = adjust_color_for_rendering(color);
            }
            break;
        }
        case EViewType::FilamentId: {
            float id = float(path.extruder_id)/256;
            float role = float(path.role) / 256;
            color      = {id, role, id, 1.0f};
            break;
        }
        default: { color = ColorRGBA::WHITE(); break; }
        }

        return color;
    };

    auto travel_color = [](const Path& path) {
        return (path.delta_extruder < 0.0f) ? Travel_Colors[2] /* Retract */ :
            ((path.delta_extruder > 0.0f) ? Travel_Colors[1] /* Extrude */ :
                Travel_Colors[0] /* Move */);
    };

    auto is_in_layers_range = [this](const Path& path, size_t min_id, size_t max_id) {
        auto in_layers_range = [this, min_id, max_id](size_t id) {
            return m_data->m_layers.get_endpoints_at(min_id).first <= id && id <= m_data->m_layers.get_endpoints_at(max_id).last;
        };

        return in_layers_range(path.sub_paths.front().first.s_id) && in_layers_range(path.sub_paths.back().last.s_id);
    };

    //BBS
    auto is_extruder_in_layer_range = [this](const Path& path, size_t extruder_id) {
        return path.extruder_id == extruder_id;
    };


    auto is_travel_in_layers_range = [this](size_t path_id, size_t min_id, size_t max_id) {
        const TBuffer& buffer = m_data->m_buffers[buffer_id(EMoveType::Travel)];
        if (path_id >= buffer.paths.size())
            return false;

        Path path = buffer.paths[path_id];
        size_t first = path_id;
        size_t last = path_id;

        // check adjacent paths
        while (first > 0 && path.sub_paths.front().first.position.isApprox(buffer.paths[first - 1].sub_paths.back().last.position)) {
            --first;
            path.sub_paths.front().first = buffer.paths[first].sub_paths.front().first;
        }
        while (last < buffer.paths.size() - 1 && path.sub_paths.back().last.position.isApprox(buffer.paths[last + 1].sub_paths.front().first.position)) {
            ++last;
            path.sub_paths.back().last = buffer.paths[last].sub_paths.back().last;
        }

        const size_t min_s_id = m_data->m_layers.get_endpoints_at(min_id).first;
        const size_t max_s_id = m_data->m_layers.get_endpoints_at(max_id).last;

        return (min_s_id <= path.sub_paths.front().first.s_id && path.sub_paths.front().first.s_id <= max_s_id) ||
            (min_s_id <= path.sub_paths.back().last.s_id && path.sub_paths.back().last.s_id <= max_s_id);
    };

    const bool top_layer_only = true;

    //BBS
    Endpoints global_endpoints = { m_data->m_sequential_view.gcode_ids.size() , 0 };
    Endpoints top_layer_endpoints = global_endpoints;
    GCodeViewerState* sequential_view = const_cast<GCodeViewerState*>(&m_data->m_sequential_view);
    if (top_layer_only || !keep_sequential_current_first) sequential_view->current.first = 0;
    //BBS
    if (!keep_sequential_current_last) sequential_view->current.last = m_data->m_sequential_view.gcode_ids.size();

    // first pass: collect visible paths and update sequential view data
    std::vector<std::tuple<unsigned char, unsigned int, unsigned int, unsigned int>> paths;

    for (size_t b = 0; b < m_data->m_buffers.size(); ++b) {
        TBuffer& buffer = const_cast<TBuffer&>(m_data->m_buffers[b]);
        // reset render paths
        buffer.render_paths.clear();

        if (!buffer.visible)
           continue;

        if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::InstancedModel ||
            buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::BatchedModel) {
            for (size_t id : buffer.model.instances.s_ids) {
                if (id < m_data->m_layers.get_endpoints_at(m_data->m_layers_z_range[0]).first || m_data->m_layers.get_endpoints_at(m_data->m_layers_z_range[1]).last < id)
                    continue;

                global_endpoints.first = std::min(global_endpoints.first, id);
                global_endpoints.last = std::max(global_endpoints.last, id);

                if (top_layer_only) {
                    if (id < m_data->m_layers.get_endpoints_at(m_data->m_layers_z_range[1]).first || m_data->m_layers.get_endpoints_at(m_data->m_layers_z_range[1]).last < id)
                        continue;

                    top_layer_endpoints.first = std::min(top_layer_endpoints.first, id);
                    top_layer_endpoints.last = std::max(top_layer_endpoints.last, id);
                }
            }
        }
        else {
            for (size_t i = 0; i < buffer.paths.size(); ++i) {
                const Path& path = buffer.paths[i];
                if (path.type == EMoveType::Travel) {
                    if (!is_travel_in_layers_range(i, m_data->m_layers_z_range[0], m_data->m_layers_z_range[1]))
                        continue;
                }
                else if (!is_in_layers_range(path, m_data->m_layers_z_range[0], m_data->m_layers_z_range[1]))
                    continue;

                if (path.type == EMoveType::Extrude && !m_data->is_visible(path))
                    continue;

                if ((EViewType)m_data->m_view_type == EViewType::ColorPrint && !m_data->m_tools.m_tool_visibles[path.extruder_id])
                    continue;

                // store valid path
                for (size_t j = 0; j < path.sub_paths.size(); ++j) {
                    paths.push_back({ static_cast<unsigned char>(b), path.sub_paths[j].first.b_id, static_cast<unsigned int>(i), static_cast<unsigned int>(j) });
                }

                global_endpoints.first = std::min(global_endpoints.first, path.sub_paths.front().first.s_id);
                global_endpoints.last = std::max(global_endpoints.last, path.sub_paths.back().last.s_id);

                if (top_layer_only) {
                    if (path.type == EMoveType::Travel) {
                        if (is_travel_in_layers_range(i, m_data->m_layers_z_range[1], m_data->m_layers_z_range[1])) {
                            top_layer_endpoints.first = std::min(top_layer_endpoints.first, path.sub_paths.front().first.s_id);
                            top_layer_endpoints.last = std::max(top_layer_endpoints.last, path.sub_paths.back().last.s_id);
                        }
                    }
                    else if (is_in_layers_range(path, m_data->m_layers_z_range[1], m_data->m_layers_z_range[1])) {
                        top_layer_endpoints.first = std::min(top_layer_endpoints.first, path.sub_paths.front().first.s_id);
                        top_layer_endpoints.last = std::max(top_layer_endpoints.last, path.sub_paths.back().last.s_id);
                    }
                }
            }
        }
    }

    // update current sequential position
    sequential_view->current.first = !top_layer_only && keep_sequential_current_first ? std::clamp(sequential_view->current.first, global_endpoints.first, global_endpoints.last) : global_endpoints.first;
    if (global_endpoints.last == 0) {
        m_no_render_path = true;
        sequential_view->current.last = global_endpoints.last;
    } else {
        m_no_render_path = false;
        sequential_view->current.last = keep_sequential_current_last ? std::clamp(sequential_view->current.last, global_endpoints.first, global_endpoints.last) : global_endpoints.last;
    }

    // get the world position from the vertex buffer
    bool found = false;
    for (const TBuffer& buffer : m_data->m_buffers) {
        if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::InstancedModel ||
            buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::BatchedModel) {
            for (size_t i = 0; i < buffer.model.instances.s_ids.size(); ++i) {
                if (buffer.model.instances.s_ids[i] == m_data->m_sequential_view.current.last) {
                    size_t offset = i * buffer.model.instances.instance_size_floats();
                    sequential_view->current_position.x() = buffer.model.instances.buffer[offset + 0];
                    sequential_view->current_position.y() = buffer.model.instances.buffer[offset + 1];
                    sequential_view->current_position.z() = buffer.model.instances.buffer[offset + 2];
                    sequential_view->current_offset = buffer.model.instances.offsets[i];
                    found = true;
                    break;
                }
            }
        }
        else {
            // searches the path containing the current position
            for (const Path& path : buffer.paths) {
                if (path.contains(m_data->m_sequential_view.current.last)) {
                    const int sub_path_id = path.get_id_of_sub_path_containing(m_data->m_sequential_view.current.last);
                    if (sub_path_id != -1) {
                        const Path::Sub_Path& sub_path = path.sub_paths[sub_path_id];
                        unsigned int offset = static_cast<unsigned int>(m_data->m_sequential_view.current.last - sub_path.first.s_id);
                        if (offset > 0) {
                            if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::Line) {
                                for (size_t i = sub_path.first.s_id + 1; i < m_data->m_sequential_view.current.last + 1; i++) {
                                    size_t move_id = m_data->m_ssid_to_moveid_map[i];
                                    const GCodeProcessorResult::MoveVertex& curr = m_data->m_gcode_result->moves[move_id];
                                    if (curr.is_arc_move()) {
                                        offset += curr.interpolation_points.size();
                                    }
                                }
                                offset = 2 * offset - 1;
                            }
                            else if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::Triangle) {
                                unsigned int indices_count = buffer.indices_per_segment();
                                // BBS: modify to support moves which has internal point
                                for (size_t i = sub_path.first.s_id + 1; i < m_data->m_sequential_view.current.last + 1; i++) {
                                    size_t move_id = m_data->m_ssid_to_moveid_map[i];
                                    const GCodeProcessorResult::MoveVertex& curr = m_data->m_gcode_result->moves[move_id];
                                    if (curr.is_arc_move()) {
                                        offset += curr.interpolation_points.size();
                                    }
                                }
                                offset = indices_count * (offset - 1) + (indices_count - 2);
                                if (sub_path_id == 0)
                                    offset += 6; // add 2 triangles for starting cap
                            }
                        }
                        offset += static_cast<unsigned int>(sub_path.first.i_id);

                        // gets the vertex index from the index buffer on gpu
                        const IBuffer& i_buffer = buffer.indices[sub_path.first.b_id];
                        unsigned int index = 0;
                        glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i_buffer.ibo));
                        glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(offset * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&index)));
                        glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

                        // gets the position from the vertices buffer on gpu
                        glsafe(::glBindBuffer(GL_ARRAY_BUFFER, i_buffer.vbo));
                        glsafe(::glGetBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(index * buffer.vertices.vertex_size_bytes()), static_cast<GLsizeiptr>(3 * sizeof(float)), static_cast<void*>(sequential_view->current_position.data())));
                        glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));

                        sequential_view->current_offset = Vec3f::Zero();
                        found = true;
                        break;
                    }
                }
            }
        }

        if (found)
            break;
    }

    // second pass: filter paths by sequential data and collect them by color
    RenderPath* render_path = nullptr;
    for (const auto& [tbuffer_id, ibuffer_id, path_id, sub_path_id] : paths) {
        TBuffer& buffer = const_cast<TBuffer&>(m_data->m_buffers[tbuffer_id]);
        const Path& path = buffer.paths[path_id];
        const Path::Sub_Path& sub_path = path.sub_paths[sub_path_id];
        if (m_data->m_sequential_view.current.last < sub_path.first.s_id || sub_path.last.s_id < m_data->m_sequential_view.current.first)
            continue;

        ColorRGBA color;
        switch (path.type)
        {
        case EMoveType::Tool_change:
        case EMoveType::Color_change:
        case EMoveType::Pause_Print:
        case EMoveType::Custom_GCode:
        case EMoveType::Retract:
        case EMoveType::Unretract:
        case EMoveType::Seam: { color = option_color(path.type); break; }
        case EMoveType::Extrude: {
            if (!top_layer_only ||
                m_data->m_sequential_view.current.last == global_endpoints.last ||
                is_in_layers_range(path, m_data->m_layers_z_range[1], m_data->m_layers_z_range[1]))
                color = extrusion_color(path);
            else
                color = Neutral_Color;

            break;
        }
        case EMoveType::Travel: {
            if (!top_layer_only || m_data->m_sequential_view.current.last == global_endpoints.last || is_travel_in_layers_range(path_id, m_data->m_layers_z_range[1], m_data->m_layers_z_range[1]))
                color = ((EViewType)m_data->m_view_type == EViewType::Feedrate || (EViewType)m_data->m_view_type == EViewType::Tool) ? extrusion_color(path) : travel_color(path);
            else
                color = Neutral_Color;

            break;
        }
        case EMoveType::Wipe: { color = Wipe_Color; break; }
        default: { color = { 0.0f, 0.0f, 0.0f, 1.0f }; break; }
        }

        RenderPath key{ tbuffer_id, color, static_cast<unsigned int>(ibuffer_id), path_id };
        if (render_path == nullptr || !RenderPathPropertyEqual()(*render_path, key)) {
            buffer.render_paths.emplace_back(key);
            render_path = const_cast<RenderPath*>(&buffer.render_paths.back());
        }

        unsigned int delta_1st = 0;
        if (sub_path.first.s_id < m_data->m_sequential_view.current.first && m_data->m_sequential_view.current.first <= sub_path.last.s_id)
            delta_1st = static_cast<unsigned int>(m_data->m_sequential_view.current.first - sub_path.first.s_id);

        unsigned int size_in_indices = 0;
        switch (buffer.render_primitive_type)
        {
        case TBuffer::ERenderPrimitiveType::Line:
        case TBuffer::ERenderPrimitiveType::Triangle: {
            // BBS: modify to support moves which has internal point
            size_t max_s_id = std::min(m_data->m_sequential_view.current.last, sub_path.last.s_id);
            size_t min_s_id = std::max(m_data->m_sequential_view.current.first, sub_path.first.s_id);
            unsigned int segments_count = max_s_id - min_s_id;
            for (size_t i = min_s_id + 1; i < max_s_id + 1; i++) {
                size_t move_id = m_data->m_ssid_to_moveid_map[i];
                const GCodeProcessorResult::MoveVertex& curr = m_data->m_gcode_result->moves[move_id];
                if (curr.is_arc_move()) {
                    segments_count += curr.interpolation_points.size();
                }
            }
            size_in_indices = buffer.indices_per_segment() * segments_count;
            break;
        }
        default: { break; }
        }

        if (size_in_indices == 0)
            continue;

        if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::Triangle) {
            if (sub_path_id == 0 && delta_1st == 0)
                size_in_indices += 6; // add 2 triangles for starting cap
            if (sub_path_id == path.sub_paths.size() - 1 && path.sub_paths.back().last.s_id <= m_data->m_sequential_view.current.last)
                size_in_indices += 6; // add 2 triangles for ending cap
            if (delta_1st > 0)
                size_in_indices -= 6; // remove 2 triangles for corner cap
        }

        render_path->sizes.push_back(size_in_indices);

        if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::Triangle) {
            delta_1st *= buffer.indices_per_segment();
            if (delta_1st > 0) {
                delta_1st += 6; // skip 2 triangles for corner cap
                if (sub_path_id == 0)
                    delta_1st += 6; // skip 2 triangles for starting cap
            }
        }

        render_path->offsets.push_back(static_cast<size_t>((sub_path.first.i_id + delta_1st) * sizeof(IBufferType)));

#if 0
        // check sizes and offsets against index buffer size on gpu
        GLint buffer_size;
        glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->indices[render_path->ibuffer_id].ibo));
        glsafe(::glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &buffer_size));
        glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
        if (render_path->offsets.back() + render_path->sizes.back() * sizeof(IBufferType) > buffer_size)
            BOOST_LOG_TRIVIAL(error) << "GCodeViewer::refresh_render_paths: Invalid render path data";
#endif
    }

    // Removes empty render paths and sort.
    for (size_t b = 0; b < m_data->m_buffers.size(); ++b) {
        TBuffer* buffer = const_cast<TBuffer*>(&m_data->m_buffers[b]);
        buffer->render_paths.erase(std::remove_if(buffer->render_paths.begin(), buffer->render_paths.end(),
            [](const auto &path){ return path.sizes.empty() || path.offsets.empty(); }),
            buffer->render_paths.end());
    }

    // second pass: for buffers using instanced and batched models, update the instances render ranges
    for (size_t b = 0; b < m_data->m_buffers.size(); ++b) {
        TBuffer& buffer = const_cast<TBuffer&>(m_data->m_buffers[b]);
        if (buffer.render_primitive_type != TBuffer::ERenderPrimitiveType::InstancedModel &&
            buffer.render_primitive_type != TBuffer::ERenderPrimitiveType::BatchedModel)
            continue;

        buffer.model.instances.render_ranges.reset();

        if (!buffer.visible || buffer.model.instances.s_ids.empty())
            continue;

        buffer.model.instances.render_ranges.ranges.push_back({ 0, 0, 0, buffer.model.color });
        bool has_second_range = top_layer_only && m_data->m_sequential_view.current.last != m_data->m_sequential_view.global.last;
        if (has_second_range)
            buffer.model.instances.render_ranges.ranges.push_back({ 0, 0, 0, Neutral_Color });

        if (m_data->m_sequential_view.current.first <= buffer.model.instances.s_ids.back() && buffer.model.instances.s_ids.front() <= m_data->m_sequential_view.current.last) {
            for (size_t id : buffer.model.instances.s_ids) {
                if (has_second_range) {
                    if (id < m_data->m_sequential_view.endpoints.first) {
                        ++buffer.model.instances.render_ranges.ranges.front().offset;
                        if (id <= m_data->m_sequential_view.current.first)
                            ++buffer.model.instances.render_ranges.ranges.back().offset;
                        else
                            ++buffer.model.instances.render_ranges.ranges.back().count;
                    }
                    else if (id <= m_data->m_sequential_view.current.last)
                        ++buffer.model.instances.render_ranges.ranges.front().count;
                    else
                        break;
                }
                else {
                    if (id <= m_data->m_sequential_view.current.first)
                        ++buffer.model.instances.render_ranges.ranges.front().offset;
                    else if (id <= m_data->m_sequential_view.current.last)
                        ++buffer.model.instances.render_ranges.ranges.front().count;
                    else
                        break;
                }
            }
        }
    }

    // set sequential data to their final value
    sequential_view->endpoints = top_layer_only ? top_layer_endpoints : global_endpoints;
    sequential_view->current.first = !top_layer_only && keep_sequential_current_first ? std::clamp(sequential_view->current.first, sequential_view->endpoints.first, sequential_view->endpoints.last) : sequential_view->endpoints.first;
    sequential_view->global = global_endpoints;

    // updates sequential range caps
    std::array<SequentialRangeCap, 2>* sequential_range_caps = const_cast<std::array<SequentialRangeCap, 2>*>(&m_sequential_range_caps);
    (*sequential_range_caps)[0].reset();
    (*sequential_range_caps)[1].reset();

    if (m_data->m_sequential_view.current.first != m_data->m_sequential_view.current.last) {
        for (const auto& [tbuffer_id, ibuffer_id, path_id, sub_path_id] : paths) {
            TBuffer& buffer = const_cast<TBuffer&>(m_data->m_buffers[tbuffer_id]);
            if (buffer.render_primitive_type != TBuffer::ERenderPrimitiveType::Triangle)
                continue;

            const Path& path = buffer.paths[path_id];
            const Path::Sub_Path& sub_path = path.sub_paths[sub_path_id];
            if (m_data->m_sequential_view.current.last <= sub_path.first.s_id || sub_path.last.s_id <= m_data->m_sequential_view.current.first)
                continue;

            // update cap for first endpoint of current range
            if (m_data->m_sequential_view.current.first > sub_path.first.s_id) {
                SequentialRangeCap& cap = (*sequential_range_caps)[0];
                const IBuffer& i_buffer = buffer.indices[ibuffer_id];
                cap.buffer = &buffer;
                cap.vbo = i_buffer.vbo;

                // calculate offset into the index buffer
                unsigned int offset = sub_path.first.i_id;
                offset += 6; // add 2 triangles for corner cap
                offset += static_cast<unsigned int>(m_data->m_sequential_view.current.first - sub_path.first.s_id) * buffer.indices_per_segment();
                if (sub_path_id == 0)
                    offset += 6; // add 2 triangles for starting cap

                // extract indices from index buffer
                std::array<IBufferType, 6> indices{ 0, 0, 0, 0, 0, 0 };
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i_buffer.ibo));
                glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>((offset + 0) * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&indices[0])));
                glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>((offset + 7) * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&indices[1])));
                glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>((offset + 1) * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&indices[2])));
                glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>((offset + 13) * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&indices[4])));
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
                indices[3] = indices[0];
                indices[5] = indices[1];

                // send indices to gpu
                glsafe(::glGenBuffers(1, &cap.ibo));
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cap.ibo));
                glsafe(::glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(IBufferType), indices.data(), GL_STATIC_DRAW));
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

                // extract color from render path
                size_t offset_bytes = offset * sizeof(IBufferType);
                for (const RenderPath& render_path : buffer.render_paths) {
                    if (render_path.ibuffer_id == ibuffer_id) {
                        for (size_t j = 0; j < render_path.offsets.size(); ++j) {
                            if (render_path.contains(offset_bytes)) {
                                cap.color = render_path.color;
                                break;
                            }
                        }
                    }
                }
            }

            // update cap for last endpoint of current range
            if (m_data->m_sequential_view.current.last < sub_path.last.s_id) {
                SequentialRangeCap& cap = (*sequential_range_caps)[1];
                const IBuffer& i_buffer = buffer.indices[ibuffer_id];
                cap.buffer = &buffer;
                cap.vbo = i_buffer.vbo;

                // calculate offset into the index buffer
                unsigned int offset = sub_path.first.i_id;
                offset += 6; // add 2 triangles for corner cap
                offset += static_cast<unsigned int>(m_data->m_sequential_view.current.last - 1 - sub_path.first.s_id) * buffer.indices_per_segment();
                if (sub_path_id == 0)
                    offset += 6; // add 2 triangles for starting cap

                // extract indices from index buffer
                std::array<IBufferType, 6> indices{ 0, 0, 0, 0, 0, 0 };
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i_buffer.ibo));
                glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>((offset + 2) * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&indices[0])));
                glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>((offset + 4) * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&indices[1])));
                glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>((offset + 10) * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&indices[2])));
                glsafe(::glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>((offset + 16) * sizeof(IBufferType)), static_cast<GLsizeiptr>(sizeof(IBufferType)), static_cast<void*>(&indices[5])));
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
                indices[3] = indices[0];
                indices[4] = indices[2];

                // send indices to gpu
                glsafe(::glGenBuffers(1, &cap.ibo));
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cap.ibo));
                glsafe(::glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(IBufferType), indices.data(), GL_STATIC_DRAW));
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

                // extract color from render path
                size_t offset_bytes = offset * sizeof(IBufferType);
                for (const RenderPath& render_path : buffer.render_paths) {
                    if (render_path.ibuffer_id == ibuffer_id) {
                        for (size_t j = 0; j < render_path.offsets.size(); ++j) {
                            if (render_path.contains(offset_bytes)) {
                                cap.color = render_path.color;
                                break;
                            }
                        }
                    }
                }
            }

            if ((*sequential_range_caps)[0].is_renderable() && (*sequential_range_caps)[1].is_renderable())
                break;
        }
    }

    ////BBS
    //enable_moves_slider(!paths.empty());
}

void GCodeRenderer::render_toolpaths()
{
 
    if (m_data == NULL)
        return;

    const Camera& camera = AppAdapter::plater()->get_camera();
    const double zoom = camera.get_zoom();

    auto render_as_lines = [](std::vector<RenderPath>::iterator it_path, std::vector<RenderPath>::iterator it_end, GLShaderProgram& shader, int uniform_color) {
        for (auto it = it_path; it != it_end && it_path->ibuffer_id == it->ibuffer_id; ++it) {
            const RenderPath& path = *it;
            // Some OpenGL drivers crash on empty glMultiDrawElements, see GH #7415.
            assert(! path.sizes.empty());
            assert(! path.offsets.empty());
            shader.set_uniform(uniform_color, path.color);
            glsafe(::glMultiDrawElements(GL_LINES, (const GLsizei*)path.sizes.data(), GL_UNSIGNED_SHORT, (const void* const*)path.offsets.data(), (GLsizei)path.sizes.size()));
        }
    };

    auto render_as_triangles = [](std::vector<RenderPath>::iterator it_path, std::vector<RenderPath>::iterator it_end, GLShaderProgram& shader, int uniform_color) {
        for (auto it = it_path; it != it_end && it_path->ibuffer_id == it->ibuffer_id; ++it) {
            const RenderPath& path = *it;
            // Some OpenGL drivers crash on empty glMultiDrawElements, see GH #7415.
            assert(! path.sizes.empty());
            assert(! path.offsets.empty());
            shader.set_uniform(uniform_color, path.color);
            glsafe(::glMultiDrawElements(GL_TRIANGLES, (const GLsizei*)path.sizes.data(), GL_UNSIGNED_SHORT, (const void* const*)path.offsets.data(), (GLsizei)path.sizes.size()));
        }
    };

    auto render_as_instanced_model = [](TBuffer& buffer, GLShaderProgram & shader) {
        for (auto& range : buffer.model.instances.render_ranges.ranges) {
            if (range.vbo == 0 && range.count > 0) {
                glsafe(::glGenBuffers(1, &range.vbo));
                glsafe(::glBindBuffer(GL_ARRAY_BUFFER, range.vbo));
                glsafe(::glBufferData(GL_ARRAY_BUFFER, range.count * buffer.model.instances.instance_size_bytes(), (const void*)&buffer.model.instances.buffer[range.offset * buffer.model.instances.instance_size_floats()], GL_STATIC_DRAW));
                glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));
            }

            if (range.vbo > 0) {
                buffer.model.model.set_color(range.color);
                buffer.model.model.render_instanced(range.vbo, range.count);
            }
        }
    };

    auto render_as_batched_model = [](TBuffer& buffer, GLShaderProgram& shader, int position_id, int normal_id) {
        struct Range
        {
            unsigned int first;
            unsigned int last;
            bool intersects(const Range& other) const { return (other.last < first || other.first > last) ? false : true; }
        };
        Range buffer_range = { 0, 0 };
        const size_t indices_per_instance = buffer.model.data.indices_count();

        for (size_t j = 0; j < buffer.indices.size(); ++j) {
            const IBuffer& i_buffer = buffer.indices[j];
            buffer_range.last = buffer_range.first + i_buffer.count / indices_per_instance;
            glsafe(::glBindBuffer(GL_ARRAY_BUFFER, i_buffer.vbo));
            if (position_id != -1) {
                glsafe(::glVertexAttribPointer(position_id, buffer.vertices.position_size_floats(), GL_FLOAT, GL_FALSE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.position_offset_bytes()));
                glsafe(::glEnableVertexAttribArray(position_id));
            }
            const bool has_normals = buffer.vertices.normal_size_floats() > 0;
            if (has_normals) {
                if (normal_id != -1) {
                    if (buffer.vertices.format == VBuffer::EFormat::PositionNormal3Byte) {
                        // For byte format, use GL_UNSIGNED_BYTE and normalize
                        glsafe(::glVertexAttribPointer(normal_id, 3, GL_UNSIGNED_BYTE, GL_TRUE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.normal_offset_bytes()));
                    } else {
                        // For float format, use GL_FLOAT
                        glsafe(::glVertexAttribPointer(normal_id, buffer.vertices.normal_size_floats(), GL_FLOAT, GL_FALSE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.normal_offset_bytes()));
                    }
                    glsafe(::glEnableVertexAttribArray(normal_id));
                }
            }

            glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i_buffer.ibo));

            for (auto& range : buffer.model.instances.render_ranges.ranges) {
                const Range range_range = { range.offset, range.offset + range.count };
                if (range_range.intersects(buffer_range)) {
                    shader.set_uniform("uniform_color", range.color);
                    const unsigned int offset = (range_range.first > buffer_range.first) ? range_range.first - buffer_range.first : 0;
                    const size_t offset_bytes = static_cast<size_t>(offset) * indices_per_instance * sizeof(IBufferType);
                    const Range render_range = { std::max(range_range.first, buffer_range.first), std::min(range_range.last, buffer_range.last) };
                    const size_t count = static_cast<size_t>(render_range.last - render_range.first) * indices_per_instance;
                    if (count > 0) {
                        glsafe(::glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_SHORT, (const void*)offset_bytes));
                    }
                }
            }

            glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            if (normal_id != -1)
                glsafe(::glDisableVertexAttribArray(normal_id));
            if (position_id != -1)
                glsafe(::glDisableVertexAttribArray(position_id));
            glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));

            buffer_range.first = buffer_range.last;
        }
    };

    auto line_width = [](double zoom) {
        return (zoom < 5.0) ? 1.0 : (1.0 + 5.0 * (zoom - 5.0) / (100.0 - 5.0));
    };

    const unsigned char begin_id = buffer_id(EMoveType::Retract);
    const unsigned char end_id   = buffer_id(EMoveType::Count);

    auto render_buffer = [&](GLShaderProgram* shader, TBuffer& buffer)
        {
            shader->set_uniform("view_model_matrix", camera.get_view_matrix());
            shader->set_uniform("projection_matrix", camera.get_projection_matrix());
            shader->set_uniform("view_normal_matrix", (Matrix3d)Matrix3d::Identity());

            if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::InstancedModel) {
                shader->set_uniform("emission_factor", 0.25f);
                render_as_instanced_model(buffer, *shader);
                shader->set_uniform("emission_factor", 0.0f);
            }
            else if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::BatchedModel) {
                shader->set_uniform("emission_factor", 0.25f);
                const int position_id = shader->get_attrib_location("v_position");
                const int normal_id = shader->get_attrib_location("v_normal");
                render_as_batched_model(buffer, *shader, position_id, normal_id);
                shader->set_uniform("emission_factor", 0.0f);
            }
            else {
                const int position_id = shader->get_attrib_location("v_position");
                const int normal_id = shader->get_attrib_location("v_normal");
                const int uniform_color = shader->get_uniform_location("uniform_color");

                auto it_path = buffer.render_paths.begin();
                for (unsigned int ibuffer_id = 0; ibuffer_id < static_cast<unsigned int>(buffer.indices.size()); ++ibuffer_id) {
                    const IBuffer& i_buffer = buffer.indices[ibuffer_id];
                    // Skip all paths with ibuffer_id < ibuffer_id.
                    for (; it_path != buffer.render_paths.end() && it_path->ibuffer_id < ibuffer_id; ++it_path);
                    if (it_path == buffer.render_paths.end() || it_path->ibuffer_id > ibuffer_id)
                        // Not found. This shall not happen.
                        continue;

                    glsafe(::glBindBuffer(GL_ARRAY_BUFFER, i_buffer.vbo));
                    if (position_id != -1) {
                        glsafe(::glVertexAttribPointer(position_id, buffer.vertices.position_size_floats(), GL_FLOAT, GL_FALSE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.position_offset_bytes()));
                        glsafe(::glEnableVertexAttribArray(position_id));
                    }
                    const bool has_normals = buffer.vertices.normal_size_floats() > 0;
                    if (has_normals) {
                        if (normal_id != -1) {
                            if (buffer.vertices.format == VBuffer::EFormat::PositionNormal3Byte) {
                                // For byte format, use GL_UNSIGNED_BYTE and normalize
                                glsafe(::glVertexAttribPointer(normal_id, 3, GL_UNSIGNED_BYTE, GL_TRUE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.normal_offset_bytes()));
                            } else {
                                // For float format, use GL_FLOAT
                                glsafe(::glVertexAttribPointer(normal_id, buffer.vertices.normal_size_floats(), GL_FLOAT, GL_FALSE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.normal_offset_bytes()));
                            }
                            glsafe(::glEnableVertexAttribArray(normal_id));
                        }
                    }

                    glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i_buffer.ibo));

                    // Render all elements with it_path->ibuffer_id == ibuffer_id, possible with varying colors.
                    switch (buffer.render_primitive_type)
                    {
                    case TBuffer::ERenderPrimitiveType::Line: {
                        glsafe(::glLineWidth(static_cast<GLfloat>(line_width(zoom))));
                        render_as_lines(it_path, buffer.render_paths.end(), *shader, uniform_color);
                        break;
                    }
                    case TBuffer::ERenderPrimitiveType::Triangle: {
                        render_as_triangles(it_path, buffer.render_paths.end(), *shader, uniform_color);
                        break;
                    }
                    default: { break; }
                    }

                    glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

                    if (normal_id != -1)
                        glsafe(::glDisableVertexAttribArray(normal_id));
                    if (position_id != -1)
                        glsafe(::glDisableVertexAttribArray(position_id));
                    glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));
                }
            }

        };

    for (unsigned char i = begin_id; i < end_id; ++i) {
        TBuffer& buffer = m_data->m_buffers[i];
        if (!buffer.visible || !buffer.has_data())
            continue;

        {
            GLShaderProgram* shader = get_shader(buffer.shader.c_str());
            if (shader != nullptr)
            {
                shader->start_using();
                render_buffer(shader, buffer);
                shader->stop_using();
            }
        }
        if (m_data->m_idex_mode == IdexMode_Mirror)
        {
            GLShaderProgram* shader = get_shader("mirror_gcode");
            if (shader != nullptr)
            {
                shader->start_using();

                glEnable(GL_BLEND);
                glDisable(GL_CULL_FACE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
               
                Matrix4d mirrorMat;
                mirrorMat.setIdentity();
                mirrorMat(1, 1) = -1.0;
                mirrorMat(1, 3) = m_data->mirror_center.y() * 2.0;
                shader->set_uniform("alpha", 0.1);
                shader->set_uniform("mirror_matrix", mirrorMat);

                render_buffer(shader, buffer);
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
                shader->stop_using();
                
            }
        }
        else if (m_data->m_idex_mode == IdexMode_Copy)
        {
            GLShaderProgram* shader = get_shader("mirror_gcode");
            if (shader != nullptr)
            {
                shader->start_using();

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                //shader->set_uniform("offset", Vec3d(0.0, -180.0, 0.0));
                //shader->set_uniform("offset", Vec3d( m_data->ms_offset.x(), m_data->ms_offset.y(), 0.0));
                shader->set_uniform("alpha", 0.1);
                Matrix4d copyMat;
                copyMat.setIdentity();
                copyMat(0, 3) = m_data->ms_offset.x();
                copyMat(1, 3) = m_data->ms_offset.y();
                shader->set_uniform("alpha", 0.1);
                shader->set_uniform("mirror_matrix", copyMat);
                render_buffer(shader, buffer);
                glDisable(GL_BLEND);
                shader->stop_using();
            }
        }
    }

    auto render_sequential_range_cap = [&camera](const SequentialRangeCap& cap) {
        const TBuffer* buffer = cap.buffer;
        GLShaderProgram* shader = get_shader(buffer->shader.c_str());
        if (shader == nullptr)
            return;

        shader->start_using();

        shader->set_uniform("view_model_matrix", camera.get_view_matrix());
        shader->set_uniform("projection_matrix", camera.get_projection_matrix());
        shader->set_uniform("view_normal_matrix", (Matrix3d)Matrix3d::Identity());

        const int position_id = shader->get_attrib_location("v_position");
        const int normal_id   = shader->get_attrib_location("v_normal");

        glsafe(::glBindBuffer(GL_ARRAY_BUFFER, cap.vbo));
        if (position_id != -1) {
            glsafe(::glVertexAttribPointer(position_id, buffer->vertices.position_size_floats(), GL_FLOAT, GL_FALSE, buffer->vertices.vertex_size_bytes(), (const void*)buffer->vertices.position_offset_bytes()));
            glsafe(::glEnableVertexAttribArray(position_id));
        }
        const bool has_normals = buffer->vertices.normal_size_floats() > 0;
        if (has_normals) {
            if (normal_id != -1) {
                if (buffer->vertices.format == VBuffer::EFormat::PositionNormal3Byte) {
                    // For byte format, use GL_UNSIGNED_BYTE and normalize
                    glsafe(::glVertexAttribPointer(normal_id, 3, GL_UNSIGNED_BYTE, GL_TRUE, buffer->vertices.vertex_size_bytes(), (const void*)buffer->vertices.normal_offset_bytes()));
                } else {
                    // For float format, use GL_FLOAT
                    glsafe(::glVertexAttribPointer(normal_id, buffer->vertices.normal_size_floats(), GL_FLOAT, GL_FALSE, buffer->vertices.vertex_size_bytes(), (const void*)buffer->vertices.normal_offset_bytes()));
                }
                glsafe(::glEnableVertexAttribArray(normal_id));
            }
        }

        shader->set_uniform("uniform_color", cap.color);

        glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cap.ibo));
        glsafe(::glDrawElements(GL_TRIANGLES, (GLsizei)cap.indices_count(), GL_UNSIGNED_SHORT, nullptr));
        glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        if (normal_id != -1)
            glsafe(::glDisableVertexAttribArray(normal_id));
        if (position_id != -1)
            glsafe(::glDisableVertexAttribArray(position_id));

        glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));

        shader->stop_using();
    };

    for (unsigned int i = 0; i < 2; ++i) {
        if (m_sequential_range_caps[i].is_renderable())
            render_sequential_range_cap(m_sequential_range_caps[i]);
    }
}

void GCodeRenderer::render_marker(int canvas_width, int canvas_height, IdexMode indexMode)
{
    if (!m_data)
        return;

    EViewType view_type = m_data->m_view_type;
    Marker& marker = m_data->m_sequential_view.marker;
    marker.set_world_position(m_data->m_sequential_view.current_position);
    marker.set_world_offset(m_data->m_sequential_view.current_offset);
 
    Transform3d auxiliary_matrix;
    auxiliary_matrix.setIdentity();
    if (indexMode == IdexMode::IdexMode_Mirror)
    {
         auxiliary_matrix.matrix()(1,1)= -1.0;
         auxiliary_matrix.matrix()(1,3) =  2.0*m_data->mirror_center.y();
    }
    else if(indexMode == IdexMode::IdexMode_Copy)
    {
         auxiliary_matrix.matrix()(0,3) = m_data->ms_offset.x();
         auxiliary_matrix.matrix()(1,3) = m_data->ms_offset.y();
    }

    if (!marker.m_visible)
        return;

    GLShaderProgram* shader = get_shader("gouraud_light");
    if (shader == nullptr)
        return;

    glsafe(::glEnable(GL_BLEND));
    glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    shader->start_using();
    shader->set_uniform("emission_factor", 0.0f);

    const Camera& camera = AppAdapter::plater()->get_camera();
    const Transform3d& view_matrix = camera.get_view_matrix();
    const Transform3d model_matrix = marker.m_world_transform.cast<double>();
    
    shader->set_uniform("view_model_matrix", view_matrix *auxiliary_matrix* model_matrix);
    shader->set_uniform("projection_matrix", camera.get_projection_matrix());
    const Matrix3d view_normal_matrix = view_matrix.matrix().block(0, 0, 3, 3) * model_matrix.matrix().block(0, 0, 3, 3).inverse().transpose();
    shader->set_uniform("view_normal_matrix", view_normal_matrix);

    marker.m_model.render();

    shader->stop_using();

    glsafe(::glDisable(GL_BLEND));

    static float last_window_width = 0.0f;
    size_t text_line = 0;
    static size_t last_text_line = 0;
    const ImU32 text_name_clr = marker.m_is_dark ? IM_COL32(255, 255, 255, 0.88 * 255) : IM_COL32(38, 46, 48, 255);
    const ImU32 text_value_clr = marker.m_is_dark ? IM_COL32(255, 255, 255, 0.4 * 255) : IM_COL32(144, 144, 144, 255);

    ImGuiWrapper& imgui = global_im_gui();
    //BBS: GUI refactor: add canvas size from parameters
    imgui.set_next_window_pos(0.5f * static_cast<float>(canvas_width), static_cast<float>(canvas_height), ImGuiCond_Always, 0.5f, 1.0f);
    imgui.push_toolbar_style(marker.m_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0, 4.0 * marker.m_scale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0 * marker.m_scale, 6.0 * marker.m_scale));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, text_name_clr);
    ImGui::PushStyleColor(ImGuiCol_Text, text_value_clr);
    imgui.begin(std::string("ExtruderPosition"), ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
    ImGui::AlignTextToFramePadding();
    //BBS: minus the plate offset when show tool position
    PartPlateList& partplate_list = AppAdapter::plater()->get_partplate_list();
    PartPlate* plate = partplate_list.get_curr_plate();
    const Vec3f position = marker.m_world_position + marker.m_world_offset;
    std::string x = ImGui::ColorMarkerStart + std::string("X: ") + ImGui::ColorMarkerEnd;
    std::string y = ImGui::ColorMarkerStart + std::string("Y: ") + ImGui::ColorMarkerEnd;
    std::string z = ImGui::ColorMarkerStart + std::string("Z: ") + ImGui::ColorMarkerEnd;
    std::string height = ImGui::ColorMarkerStart + _u8L("Height: ") + ImGui::ColorMarkerEnd;
    std::string width = ImGui::ColorMarkerStart + _u8L("Width: ") + ImGui::ColorMarkerEnd;
    std::string speed = ImGui::ColorMarkerStart + _u8L("Speed: ") + ImGui::ColorMarkerEnd;
    std::string flow = ImGui::ColorMarkerStart + _u8L("Flow: ") + ImGui::ColorMarkerEnd;
    std::string layer_time = ImGui::ColorMarkerStart + _u8L("Layer Time: ") + ImGui::ColorMarkerEnd;
    std::string fanspeed = ImGui::ColorMarkerStart + _u8L("Fan: ") + ImGui::ColorMarkerEnd;
    std::string temperature = ImGui::ColorMarkerStart + _u8L("Temperature: ") + ImGui::ColorMarkerEnd;
    const float item_size = imgui.calc_text_size(std::string_view{"X: 000.000  "}).x;
    const float item_spacing = imgui.get_item_spacing().x;
    const float window_padding = ImGui::GetStyle().WindowPadding.x;

    char buf[1024];
     if (true)
    {
        float startx2 = window_padding + item_size + item_spacing;
        float startx3 = window_padding + 2*(item_size + item_spacing);
        sprintf(buf, "%s%.3f", x.c_str(), position.x() - plate->get_origin().x());
        ImGui::PushItemWidth(item_size);
        imgui.text(buf);

        ImGui::SameLine(startx2);
        sprintf(buf, "%s%.3f", y.c_str(), position.y() - plate->get_origin().y());
        ImGui::PushItemWidth(item_size);
        imgui.text(buf);

        ImGui::SameLine(startx3);
        sprintf(buf, "%s%.3f", z.c_str(), position.z());
        ImGui::PushItemWidth(item_size);
        imgui.text(buf);

        sprintf(buf, "%s%.0f", speed.c_str(), marker.m_curr_move.feedrate);
        ImGui::PushItemWidth(item_size);
        imgui.text(buf);

        switch (view_type) {
        case EViewType::Height: {
            ImGui::SameLine(startx2);
            sprintf(buf, "%s%.2f", height.c_str(), marker.m_curr_move.height);
            ImGui::PushItemWidth(item_size);
            imgui.text(buf);
            break;
        }
        case EViewType::Width: {
            ImGui::SameLine(startx2);
            sprintf(buf, "%s%.2f", width.c_str(), marker.m_curr_move.width);
            ImGui::PushItemWidth(item_size);
            imgui.text(buf);
            break;
        }
        case EViewType::VolumetricRate: {
            ImGui::SameLine(startx2);
            sprintf(buf, "%s%.2f", flow.c_str(), marker.m_curr_move.volumetric_rate());
            ImGui::PushItemWidth(item_size);
            imgui.text(buf);
            break;
        }
        case EViewType::FanSpeed: {
            ImGui::SameLine(startx2);
            sprintf(buf, "%s%.0f", fanspeed.c_str(), marker.m_curr_move.fan_speed);
            ImGui::PushItemWidth(item_size);
            imgui.text(buf);
            break;
        }
        case EViewType::Temperature: {
            ImGui::SameLine(startx2);
            sprintf(buf, "%s%.0f", temperature.c_str(), marker.m_curr_move.temperature);
            ImGui::PushItemWidth(item_size);
            imgui.text(buf);
            break;
        }
        case EViewType::LayerTime:
        case EViewType::LayerTimeLog: {
            ImGui::SameLine(startx2);
            sprintf(buf, "%s%.1f", layer_time.c_str(), marker.m_curr_move.layer_duration);
            ImGui::PushItemWidth(item_size);
            imgui.text(buf);
            break;
        }
        default:
            break;
        }
        text_line = 2;
    }

    // force extra frame to automatically update window size
    float window_width = ImGui::GetWindowWidth();
    if (window_width != last_window_width || text_line != last_text_line) {
        last_window_width = window_width;
        last_text_line = text_line;
        imgui.set_requires_extra_frame();
    }

    imgui.end();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    imgui.pop_toolbar_style();

}


#define ENABLE_CALIBRATION_THUMBNAIL_OUTPUT 0
#if ENABLE_CALIBRATION_THUMBNAIL_OUTPUT
static void debug_calibration_output_thumbnail(const ThumbnailData& thumbnail_data)
{
    // debug export of generated image
    wxImage image(thumbnail_data.width, thumbnail_data.height);
    image.InitAlpha();

    for (unsigned int r = 0; r < thumbnail_data.height; ++r)
    {
        unsigned int rr = (thumbnail_data.height - 1 - r) * thumbnail_data.width;
        for (unsigned int c = 0; c < thumbnail_data.width; ++c)
        {
            unsigned char* px = (unsigned char*)thumbnail_data.pixels.data() + 4 * (rr + c);
            image.SetRGB((int)c, (int)r, px[0], px[1], px[2]);
            image.SetAlpha((int)c, (int)r, px[3]);
        }
    }

    image.SaveFile("D:/calibrate.png", wxBITMAP_TYPE_PNG);
}
#endif

//BBS
void GCodeRenderer::render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const BoundingBoxf3& box)
{
    if (!m_data)
        return;

    // reset values and refresh render
    int       last_view_type_sel = m_data->m_view_type_sel;
    EViewType last_view_type     = m_data->m_view_type;
    unsigned int last_role_visibility_flags = m_data->m_extrusions.role_visibility_flags;
    // set color scheme to FilamentId
    for (int i = 0; i < m_data->view_type_items.size(); i++) {
        if (m_data->view_type_items[i] == EViewType::FilamentId) {
            m_data->m_view_type_sel = i;
            break;
        }
    }
    m_data->set_view_type(EViewType::FilamentId, false);
    // set m_data->m_layers_z_range to 0, 1;
    // To be safe, we include both layers here although layer 1 seems enough
    // layer 0: custom extrusions such as flow calibration etc.
    // layer 1: the real first layer of object
    std::array<unsigned int, 2> tmp_layers_z_range = m_data->m_layers_z_range;
    m_data->m_layers_z_range = {0, 1};
    // BBS exclude feature types
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags & ~(1 << erSkirt);
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags & ~(1 << erCustom);
    // BBS include feature types
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags | (1 << erWipeTower);
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags | (1 << erPerimeter);
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags | (1 << erExternalPerimeter);
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags | (1 << erOverhangPerimeter);
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags | (1 << erSolidInfill);
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags | (1 << erTopSolidInfill);
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags | (1 << erInternalInfill);
    m_data->m_extrusions.role_visibility_flags = m_data->m_extrusions.role_visibility_flags | (1 << erBottomSurface);

    refresh_render_paths(false, false);

    render_calibration_thumbnail_framebuffer(*m_data, thumbnail_data, w, h, box);

    // restore values and refresh render
    // reset m_data->m_layers_z_range and view type
    m_data->m_view_type_sel = last_view_type_sel;
    m_data->set_view_type(last_view_type, false);
    m_data->m_layers_z_range = tmp_layers_z_range;
    m_data->m_extrusions.role_visibility_flags = last_role_visibility_flags;
    refresh_render_paths(false, false);
}


};
};
};

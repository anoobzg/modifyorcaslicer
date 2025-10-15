#include "GCodeViewInstance.hpp"
#include "GCodeViewerData.hpp"
#include "GCodeRenderer.hpp"
#include "GCodeExtensions.hpp"

namespace Slic3r {
namespace GUI {
namespace GCode {

GCodeViewInstance::GCodeViewInstance()
{
    m_data =  std::make_shared<GCodeViewerData>() ;
    m_renderer = new GCodeRenderer;

    
    m_data->m_extrusions.reset_role_visibility_flags();
    // m_data->m_sequential_view.skip_invisible_moves = true;
}

std::shared_ptr<GCodeViewerData> GCodeViewInstance::data()
{
    return m_data;
}

void GCodeViewInstance::init(ConfigOptionMode mode, PresetBundle* preset_bundle)
{
    m_data->init(mode, preset_bundle);
}

void GCodeViewInstance::load(const GCodeProcessorResult& gcode_result, const Print& print, const BuildVolume& build_volume,
                const std::vector<BuildVolume>& sub_build_volumes,
                const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode)
{

    m_data->load_gcode(gcode_result, print, build_volume, sub_build_volumes, exclude_bounding_box, mode, only_gcode);
    m_renderer->set_data(m_data.get());
    m_renderer->refresh_render_paths(false, false);
}


void GCodeViewInstance::on_change_color_mode(bool is_dark) 
{
    m_data->on_change_color_mode(is_dark);
}

void GCodeViewInstance::set_scale(float scale)
{
    m_data->set_scale(scale);
}

void GCodeViewInstance::update_by_mode(ConfigOptionMode mode)
{
    m_data->update_by_mode(mode);
}

void GCodeViewInstance::reset()
{
    m_data->reset();
}
bool GCodeViewInstance::has_data() const 
{ 
    return m_data->has_data(); 
}

bool GCodeViewInstance::can_export_toolpaths() const
{
    return has_data() && m_data->m_buffers[buffer_id(EMoveType::Extrude)].render_primitive_type == TBuffer::ERenderPrimitiveType::Triangle;
}

bool GCodeViewInstance::update_sequential_view_current(unsigned int first, unsigned int last)
{
    bool is_changed = m_data->update_viewer_state_current(first, last);
    m_renderer->refresh_render_paths(true, true);

    return is_changed;
}

void GCodeViewInstance::set_layers_z_range(const std::array<unsigned int, 2>& layers_z_range)
{
    bool keep_sequential_current_first = layers_z_range[0] >= m_data->m_layers_z_range[0];
    bool keep_sequential_current_last = layers_z_range[1] <= m_data->m_layers_z_range[1];
    m_data->m_layers_z_range = layers_z_range;
    m_renderer->refresh_render_paths(keep_sequential_current_first, keep_sequential_current_last);
}

void GCodeViewInstance::export_toolpaths_to_obj(const char* filename) const
{
    GCode::export_toolpaths_to_obj(m_data.get(), filename);
}

void GCodeViewInstance::refresh(const GCodeProcessorResult& gcode_result, const std::vector<std::string>& str_tool_colors)
{
    m_data->refresh(gcode_result, str_tool_colors);
    // update buffers' render paths
    m_renderer->refresh_render_paths(false, false); 
}

void GCodeViewInstance::update_shells_color_by_extruder(const DynamicPrintConfig *config)
{
    m_renderer->update_shells_color_by_extruder(config);
}

void GCodeViewInstance::set_shell_transparency(float alpha) 
{ 
    m_renderer->set_shell_transparency(alpha);
}

void GCodeViewInstance::refresh_render_paths(bool keep_sequential_current_first, bool keep_sequential_current_last)
{
    m_renderer->refresh_render_paths(keep_sequential_current_first, keep_sequential_current_last);
}

void GCodeViewInstance::render_shells(int width, int height)
{
    m_renderer->render_shells(width, height);
}

void GCodeViewInstance::render_toolpaths()
{
    m_renderer->render_toolpaths();
}

void GCodeViewInstance::render_marker(int canvas_width, int canvas_height)
{
    if (m_data->m_sequential_view.m_show_marker || 
        m_data->m_sequential_view.current.last != m_data->m_sequential_view.endpoints.last)
        {
            
            m_renderer->render_marker(canvas_width, canvas_height);
            if(m_data->m_idex_mode != IdexMode::IdexMode_Pack)
            {
                m_renderer->render_marker(canvas_width, canvas_height,m_data->m_idex_mode);
            }
        }   
}

void GCodeViewInstance::render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const BoundingBoxf3& box)
{
    m_renderer->render_calibration_thumbnail(thumbnail_data, w, h, box);
}

void GCodeViewInstance::load_shells(const Print& print, bool initialized, bool force_previewing)
{
    m_renderer->load_shells(&print, initialized, force_previewing);
}

void GCodeViewInstance::log_memory_used(const std::string& label, int64_t additional) const
{
    GCode::log_memory_used(m_data.get(), label, additional);
}

void GCodeViewInstance::set_render_mode(int mode)
{
    m_renderer->set_render_mode((GCodeRenderer::RenderMode)mode);
}

void GCodeViewInstance::update_marker_curr_move()
{
    if (!m_data || m_data->m_gcode_result == NULL)
        return;

    if ((int)m_data->m_last_result_id != -1) {
       auto it = std::find_if(m_data->m_gcode_result->moves.begin(), m_data->m_gcode_result->moves.end(), [this](auto move) {
               if (m_data->m_sequential_view.current.last < m_data->m_sequential_view.gcode_ids.size() && m_data->m_sequential_view.current.last >= 0) {
                   return move.gcode_id == static_cast<uint64_t>(m_data->m_sequential_view.gcode_ids[m_data->m_sequential_view.current.last]);
               }
               return false;
           });
       if (it != m_data->m_gcode_result->moves.end())
           m_data->m_sequential_view.marker.update_curr_move(*it);
    }
}

const float GCodeViewInstance::get_max_print_height() const { return m_data->m_max_print_height; }
const BoundingBoxf3& GCodeViewInstance::get_paths_bounding_box() const { return m_data->m_paths_bounding_box; }
const BoundingBoxf3& GCodeViewInstance::get_max_bounding_box() const { return m_data->m_max_bounding_box; }
const BoundingBoxf3& GCodeViewInstance::get_shell_bounding_box() const { return m_data->m_shell_bounding_box; }
const std::vector<double>& GCodeViewInstance::get_layers_zs() const { return m_data->m_layers.get_zs(); }
const std::array<unsigned int, 2>& GCodeViewInstance::get_layers_z_range() const { return m_data->m_layers_z_range; }
bool GCodeViewInstance::is_contained_in_bed() const { return m_data->m_contained_in_bed; }
EViewType GCodeViewInstance::get_view_type() const { return m_data->m_view_type; }
void GCodeViewInstance::set_view_type(EViewType type, bool reset_feature_type_visible) { m_data->set_view_type(type, reset_feature_type_visible); }
std::vector<CustomGCode::Item>& GCodeViewInstance::get_custom_gcode_per_print_z() { return m_data->m_custom_gcode_per_print_z; }
size_t GCodeViewInstance::get_extruders_count() { return m_data->m_extruders_count; }

};
};
};
#include "GCodeViewer.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/GUI/Widgets/ProgressDialog.hpp"
#include "slic3r/Slice/GCodeResultWrapper.hpp"
#include "slic3r/GCode/GCodePanel.hpp"
#include "slic3r/GCode/GCodeViewInstance.hpp"
#include "slic3r/GCode/GCodeExtensions.hpp"
#include "slic3r/GCode/GCodePlayer.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include <GL/glew.h>
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Render/ImGUI/IMSlider.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"


using namespace std;
namespace Slic3r {
namespace GUI {

    GCodeViewer::GCodeViewer()
    {
       
        m_current_plane =  std::make_shared<GCodePanel>();
        m_gcode_player = std::make_shared<GCodePlayer>();
        m_current_gcode_view_instance = std::make_shared<GCodeViewInstance>();

        m_refresh_func = [&](bool only_toolpaths)
        {
         
            if(m_current_plane != nullptr)
               m_current_plane->update_moves_slider();
            
            AppAdapter::plater()->get_current_canvas3D()->set_as_dirty();
        };

        m_select_part_func = [&](int part)
        {
            if (part >= m_gcode_view_instance_list.size())
                return;

            m_current_gcode_view_instance = m_gcode_view_instance_list.at(part);
            m_current_plane = nullptr;
            if (m_InstanceToPlaneMap.find(m_current_gcode_view_instance) != m_InstanceToPlaneMap.end())
                m_current_plane = m_InstanceToPlaneMap[m_current_gcode_view_instance];

            // if (m_current_plane != nullptr)
            // {
            //     m_current_plane->update_moves_slider();
            //     m_current_plane->update_layers_slider(m_current_gcode_view_instance->get_layers_zs(), false);
            // }

            m_current_plane->set_refresh_func(m_refresh_func);
            m_current_plane->set_select_mode_func(m_select_mode_func);
            m_current_plane->set_select_part_func(m_select_part_func);
        };

        m_select_mode_func = [&]( int part)
        {
            for (auto ins : m_gcode_view_instance_list)
                ins->set_render_mode(part);
        };
 
    }

GCodeViewer::~GCodeViewer()
{
    // delete m_gcode_viewer_data;
}

void GCodeViewer::init(ConfigOptionMode mode, PresetBundle* preset_bundle)
{
    m_mode = mode;
    m_preset_bundle = preset_bundle;
    m_current_gcode_view_instance->init(mode, preset_bundle);
}

void GCodeViewer::on_change_color_mode(bool is_dark) 
{
    m_is_dark = is_dark;
    m_current_gcode_view_instance->on_change_color_mode(m_is_dark);
}

void GCodeViewer::set_scale(float scale)
{
    m_scale = scale;
    m_current_gcode_view_instance->set_scale(scale);
}

void GCodeViewer::update_by_mode(ConfigOptionMode mode)
{
    m_current_gcode_view_instance->update_by_mode(mode);
}

void GCodeViewer::load(const GCodeProcessorResult& gcode_result, const Print& print, const BuildVolume& build_volume,
                const std::vector<BuildVolume>& sub_build_volumes,
                const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode)
{

    m_InstanceToPlaneMap.clear();
    m_gcode_view_instance_list.clear();
    std::shared_ptr<GCodeViewInstance> ins = std::make_shared<GCodeViewInstance>();
    // GCodeViewInstance* ins = new GCodeViewInstance;
    ins->load(gcode_result, print, build_volume, sub_build_volumes, exclude_bounding_box, mode, only_gcode);
    std::shared_ptr<GCodePanel> gcode_panel = std::make_shared<GCodePanel>();
    m_InstanceToPlaneMap[ins] = gcode_panel;
    m_gcode_view_instance_list.push_back(ins);
    m_gcode_player->set_instances(m_InstanceToPlaneMap);
    gcode_panel->set_instance(ins);
    //IMSlider* layers_slider = gcode_panel->get_layers_slider();
    //layers_slider->SetHigherValue(layers_slider->GetMinValue());
    m_current_plane = gcode_panel;
    m_current_plane->set_refresh_func(m_refresh_func);
    m_current_plane->set_select_mode_func(m_select_mode_func);
    m_current_plane->set_select_part_func(m_select_part_func);
}

void GCodeViewer::load(std::vector<const GCodeProcessorResult*> gcode_result_list, const Print& print, const BuildVolume& build_volume,
    const std::vector<BuildVolume>& sub_build_volumes,
    const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode)
{
 
    m_InstanceToPlaneMap.clear();
    m_gcode_view_instance_list.clear();
    for (int i = 0, size = gcode_result_list.size(); i < size; ++i)
    {
        std::shared_ptr<GCodeViewInstance> ins = std::make_shared<GCodeViewInstance>();
        ins->init(m_mode, m_preset_bundle);
        ins->load(*gcode_result_list[i], print, build_volume, sub_build_volumes,exclude_bounding_box, mode, only_gcode);
        std::shared_ptr<GCodePanel> gcode_panel = std::make_shared<GCodePanel>();
        m_InstanceToPlaneMap[ins] = gcode_panel;
        gcode_panel->set_instance(ins);
       // IMSlider* layers_slider = gcode_panel->get_layers_slider();
       // layers_slider->SetHigherValue(layers_slider->GetMinValue());
        if (i == 0)
        {
            m_current_gcode_view_instance = ins;
            m_current_plane = gcode_panel;
            m_current_plane->set_refresh_func(m_refresh_func);
            m_current_plane->set_select_mode_func(m_select_mode_func);
            m_current_plane->set_select_part_func(m_select_part_func);
        }
        m_gcode_view_instance_list.push_back(ins);
       
    }
 
    m_gcode_player->set_instances(m_InstanceToPlaneMap);
 
}

void GCodeViewer::refresh(const GCodeProcessorResult& gcode_result, const std::vector<std::string>& str_tool_colors)
{
    m_current_gcode_view_instance->refresh(gcode_result, str_tool_colors);
}


void GCodeViewer::update_shells_color_by_extruder(const DynamicPrintConfig *config)
{
    for (auto ins : m_gcode_view_instance_list)
    {
        ins->update_shells_color_by_extruder(config);
    }
}

void GCodeViewer::set_shell_transparency(float alpha)
{

    for (auto ins : m_gcode_view_instance_list)
    {
        ins->set_shell_transparency(alpha);
    }
}

void GCodeViewer::set_schedule_background_process(std::function<void()>& func)
{
    m_current_plane->set_schedule_background_process(func);
}

void GCodeViewer::reset()
{
    for (auto ins : m_gcode_view_instance_list)
        ins->reset();
}

//BBS: GUI refactor: add canvas width and height
void GCodeViewer::render(int canvas_width, int canvas_height, int right_margin)
{
    glsafe(::glEnable(GL_DEPTH_TEST));
    //render_shells(canvas_width, canvas_height);

    int bottom_margin = SLIDER_BOTTOM_MARGIN * GCODE_VIEWER_SLIDER_SCALE;

    bool has_no_data = true;
    for (auto ins : m_gcode_view_instance_list)
    {
        ins->render_shells(canvas_width, canvas_height);
        if (ins->has_data())
        {
            has_no_data = false;
            ins->render_toolpaths();
            if (!m_no_render_path)
                ins->render_marker(canvas_width, canvas_height - bottom_margin * m_scale);
        }
    }
    if (has_no_data)
        return;

    //render_toolpaths();
    float legend_height = 0.0f;
    m_current_plane->set_instance(m_current_gcode_view_instance);
    m_current_plane->render(legend_height, canvas_width, canvas_height, right_margin);

    int instance_count = m_gcode_view_instance_list.size();
    // if (instance_count > 1)
        m_current_plane->render_group_window(0, 60, (float)(canvas_width - 200) * 0.5, instance_count);

    if (!m_no_render_path)
    {
        m_current_plane->render_gcode_window(legend_height + 2, std::max(10.f, (float)canvas_height - 40), (float)canvas_width - (float)right_margin);
    }

     m_gcode_player->render((float)canvas_width - (float)right_margin, (float)canvas_height - (float)right_margin);
}

void GCodeViewer::render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params, PartPlateList& partplate_list)
{
    int plate_idx = thumbnail_params.plate_id;
    PartPlate* plate = partplate_list.get_plate(plate_idx);
    BoundingBoxf3 plate_box = plate->get_bounding_box(false);

    m_current_gcode_view_instance->render_calibration_thumbnail(thumbnail_data, w, h, plate_box);
}

bool GCodeViewer::has_data() const 
{ 
    return m_current_gcode_view_instance->has_data();
}

bool GCodeViewer::can_export_toolpaths() const
{
    return m_current_gcode_view_instance->can_export_toolpaths();
}

const float GCodeViewer::get_max_print_height() const 
{ 
    return m_current_gcode_view_instance->get_max_print_height();
}

const BoundingBoxf3& GCodeViewer::get_paths_bounding_box() const 
{ 
    return m_current_gcode_view_instance->get_paths_bounding_box();
}

const BoundingBoxf3& GCodeViewer::get_max_bounding_box() const 
{ 
    return m_current_gcode_view_instance->get_max_bounding_box();
}

const BoundingBoxf3& GCodeViewer::get_shell_bounding_box() const 
{ 
    return m_current_gcode_view_instance->get_shell_bounding_box();
}

const std::vector<double>& GCodeViewer::get_layers_zs() const 
{ 
    return m_current_gcode_view_instance->get_layers_zs();
}

const std::array<unsigned int, 2>& GCodeViewer::get_layers_z_range() const 
{ 
    return m_current_gcode_view_instance->get_layers_z_range();
}

bool GCodeViewer::is_contained_in_bed() const 
{ 
    return m_current_gcode_view_instance->is_contained_in_bed();
}

EViewType GCodeViewer::get_view_type() const 
{ 
    return m_current_gcode_view_instance->get_view_type();
}

void GCodeViewer::set_view_type(EViewType type, bool reset_feature_type_visible) 
{ 
    m_current_gcode_view_instance->set_view_type(type, reset_feature_type_visible);
}

std::vector<CustomGCode::Item>& GCodeViewer::get_custom_gcode_per_print_z() 
{ 
    return m_current_gcode_view_instance->get_custom_gcode_per_print_z();
}

size_t GCodeViewer::get_extruders_count() 
{
    return m_current_gcode_view_instance->get_extruders_count();
}

void GCodeViewer::update_sequential_view_current(unsigned int first, unsigned int last)
{
    if (m_current_gcode_view_instance->update_sequential_view_current(first, last)) {
        update_moves_slider();
    }
}

IMSlider* GCodeViewer::get_moves_slider() 
{ 
    return m_current_plane->get_moves_slider();
}

IMSlider* GCodeViewer::get_layers_slider() 
{
    return m_current_plane->get_layers_slider();
}

void GCodeViewer::enable_moves_slider(bool enable) const
{
    m_current_plane->update_moves_slider(enable);
}

void GCodeViewer::update_moves_slider(bool set_to_max)
{
    m_current_plane->update_moves_slider(set_to_max);
}

void GCodeViewer::update_layers_slider_mode()
{
    m_current_plane->update_layers_slider_mode();
}

void GCodeViewer::update_marker_curr_move() 
{
    m_current_gcode_view_instance->update_marker_curr_move();
}

void GCodeViewer::update_layers_slider(const std::vector<double>& layers_z, bool keep_z_range)
{
    m_current_plane->update_layers_slider(layers_z, keep_z_range);
}

void GCodeViewer::set_layers_z_range(const std::array<unsigned int, 2>& layers_z_range)
{
    m_current_gcode_view_instance->set_layers_z_range(layers_z_range);
    update_moves_slider(true);
}

void GCodeViewer::export_toolpaths_to_obj(const char* filename) const
{
    m_current_gcode_view_instance->export_toolpaths_to_obj(filename);
}

void GCodeViewer::load_shells(const Print& print, bool initialized, bool force_previewing)
{
    m_gcode_view_instance_list.clear();
    std::shared_ptr<GCodeViewInstance> ins = std::make_shared<GCodeViewInstance>();
    ins->load_shells(print, initialized, force_previewing);
    m_gcode_view_instance_list.push_back(ins); 
    //m_current_gcode_view_instance->load_shells(print, initialized, force_previewing);
}

void GCodeViewer::refresh_render_paths(bool keep_sequential_current_first, bool keep_sequential_current_last) const
{
    m_current_gcode_view_instance->refresh_render_paths(keep_sequential_current_first, keep_sequential_current_last);
}

void GCodeViewer::render_toolpaths()
{
    m_current_gcode_view_instance->render_toolpaths();
}

void GCodeViewer::render_shells(int canvas_width, int canvas_height)
{
    m_current_gcode_view_instance->render_shells(canvas_width, canvas_height);
}

void GCodeViewer::render_all_plates_stats(const std::vector<const GCodeProcessorResult*>& gcode_result_list, bool show /*= true*/) 
{
    m_current_plane->render_all_plates_stats(gcode_result_list, show);
}

void GCodeViewer::log_memory_used(const std::string& label, int64_t additional) const
{
    m_current_gcode_view_instance->log_memory_used(label, additional);
}

void GCodeViewer::refresh()
{
    for (auto &item : m_InstanceToPlaneMap)
    {
        auto & pInstance = item.first;
        auto &plane = item.second;
        IMSlider *layers_slider = plane->get_layers_slider();
        IMSlider *move_slider = plane->get_moves_slider();
     

        pInstance->update_sequential_view_current((move_slider->GetLowerValueD() - 1.0), static_cast<unsigned int>(move_slider->GetHigherValueD() - 1.0));
        pInstance->update_marker_curr_move();
        plane->update_moves_slider();

        if (layers_slider->is_dirty())
        {
            if (pInstance->has_data())
            {
                pInstance->set_layers_z_range({static_cast<unsigned int>(layers_slider->GetLowerValue()), static_cast<unsigned int>(layers_slider->GetHigherValue())});
            }
        }

        if (m_slider_fresh_callback != nullptr)
        {
            m_slider_fresh_callback(layers_slider, move_slider);
        }
    }
}
} // namespace GUI
} // namespace Slic3r


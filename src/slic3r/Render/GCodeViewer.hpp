#ifndef slic3r_GCodeViewer_hpp_
#define slic3r_GCodeViewer_hpp_

#include <memory>
#include "slic3r/GCode/GCodeDefine.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"

using namespace Slic3r::GUI::GCode;

namespace Slic3r {

class Print;
class TriangleMesh;
class PresetBundle;
namespace GUI {

class IMSlider;
class GCodeResultWrapper;
namespace GCode {

class GCodePanel;
class GCodeViewInstance;
class GCodePlayer;
};

class PartPlateList;
class GCodeViewer
{
public:
    ConflictResultOpt m_conflict_result;

private:
     GCodeResultWrapper* m_gcode_result_wrapper;
    // GCodePanel* m_gcode_panel;
    // GCodeViewInstance* m_gcode_view_instance;
    // std::vector<GCodeViewInstance*> m_gcode_view_instance_list;
    std::shared_ptr<GCodeViewInstance>  m_current_gcode_view_instance = nullptr;
    std::shared_ptr<GCodePlayer>  m_gcode_player = nullptr;
    std::map<std::shared_ptr<GCodeViewInstance>, std::shared_ptr<GCodePanel>> m_InstanceToPlaneMap;
    std::vector<std::shared_ptr<GCodeViewInstance>> m_gcode_view_instance_list;
    std::shared_ptr<GCodePanel> m_current_plane;
 

    ConfigOptionMode m_mode;
    PresetBundle* m_preset_bundle;
    

    // GCodeRenderer* m_gcode_renderer;

    bool m_gl_data_initialized{ false };
    bool m_legend_enabled{ true };
    float m_legend_height;
    float m_scale{ 1.0 };

    std::array<SequentialRangeCap, 2> m_sequential_range_caps;
    std::function<void(IMSlider*, IMSlider*)> m_slider_fresh_callback = nullptr; // 滑动条的回调 
    mutable bool m_no_render_path{ false };
    bool m_is_dark = false;
    std::function<void(bool)> m_refresh_func = nullptr;
    std::function<void(int)>  m_select_part_func = nullptr;
    std::function<void(int)>  m_select_mode_func = nullptr;
public:
    GCodeViewer();
    ~GCodeViewer();

    void on_change_color_mode(bool is_dark);
    void set_scale(float scale = 1.0);
    void init(ConfigOptionMode mode, Slic3r::PresetBundle* preset_bundle);
    void update_by_mode(ConfigOptionMode mode);
    void add_slider_fresh_callback(std::function<void(IMSlider*, IMSlider*)> pCallBack)
    {
        if(m_slider_fresh_callback == nullptr)
           m_slider_fresh_callback = pCallBack;       
    }

    void refresh();
    // extract rendering data from the given parameters
    //BBS: add only gcode mode
    // void load(GCodeResultWrapper* gcode_result_wrapper, const Print& print, const BuildVolume& build_volume,
    //         const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode = false);
    // // recalculate ranges in dependence of what is visible and sets tool/print colors
    // void refresh(GCodeResultWrapper* gcode_result_wrapper, const std::vector<std::string>& str_tool_colors);

    void load(const GCodeProcessorResult& gcode_result, const Print& print, const BuildVolume& build_volume,
        const std::vector<BuildVolume>& sub_build_volumes,
        const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode = false);

    void load(std::vector<const GCodeProcessorResult*> gcode_result_list, const Print& print, const BuildVolume& build_volume,
        const std::vector<BuildVolume>& sub_build_volumes,
        const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode = false);

    // recalculate ranges in dependence of what is visible and sets tool/print colors
    void refresh(const GCodeProcessorResult& gcode_result, const std::vector<std::string>& str_tool_colors);

    void refresh_render_paths(bool keep_sequential_current_first = false, bool keep_sequential_current_last = false) const;
    void update_shells_color_by_extruder(const DynamicPrintConfig* config);
    void set_shell_transparency(float alpha = 0.15f);
    void set_schedule_background_process(std::function<void()>& func);

    void reset();
    //BBS: always load shell at preview
    void reset_shell();
    void load_shells(const Print& print, bool initialized, bool force_previewing = false);
    void set_shells_on_preview(bool is_previewing) /*{ m_shells.previewing = is_previewing; }*/ {}
    //BBS: add all plates filament statistics
    void render_all_plates_stats(const std::vector<const GCodeProcessorResult*>& gcode_result_list, bool show = true);
    //BBS: GUI refactor: add canvas width and height
    void render(int canvas_width, int canvas_height, int right_margin);
    //BBS
    void render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params, PartPlateList& partplate_list);

    bool has_data() const;
    bool can_export_toolpaths() const;

    const float                get_max_print_height() const;
    const BoundingBoxf3& get_paths_bounding_box() const;
    const BoundingBoxf3& get_max_bounding_box() const;
    const BoundingBoxf3& get_shell_bounding_box() const;
    const std::vector<double>& get_layers_zs() const;
    const std::array<unsigned int, 2>& get_layers_z_range() const;
    bool is_contained_in_bed() const;
    EViewType get_view_type() const;
    void set_view_type(EViewType type, bool reset_feature_type_visible = true);
    std::vector<CustomGCode::Item>& get_custom_gcode_per_print_z();
    size_t get_extruders_count();

    void update_sequential_view_current(unsigned int first, unsigned int last);

    /* BBS IMSlider */
    IMSlider* get_moves_slider();
    IMSlider* get_layers_slider();
    void enable_moves_slider(bool enable) const;
    void update_moves_slider(bool set_to_max = false);
    void update_layers_slider_mode();
    void update_marker_curr_move();
    void update_layers_slider(const std::vector<double>& layers_z, bool keep_z_range);

    void set_layers_z_range(const std::array<unsigned int, 2>& layers_z_range);

    bool is_legend_enabled() const { return m_legend_enabled; }
    void enable_legend(bool enable) { m_legend_enabled = enable; }
    float get_legend_height() { return m_legend_height; }

    void export_toolpaths_to_obj(const char* filename) const;


private:
    void render_toolpaths();
    void render_shells(int canvas_width, int canvas_height);

    void log_memory_used(const std::string& label, int64_t additional = 0) const;
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GCodeViewer_hpp_


#ifndef _slic3r_GCodeViewInstance_hpp_
#define _slic3r_GCodeViewInstance_hpp_

#include "GCodeDefine.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"

namespace Slic3r {
class PresetBundle;

namespace GUI {
namespace GCode {

class GCodeViewerData;
class GCodeRenderer;
class GCodeViewInstance
{
public:
    GCodeViewInstance();

    std::shared_ptr<GCodeViewerData> data();

    void init(ConfigOptionMode mode, PresetBundle* preset_bundle);
    void load(const GCodeProcessorResult& gcode_result, const Print& print, const BuildVolume& build_volume,
                const std::vector<BuildVolume>& sub_build_volumes,
                const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode);

    void on_change_color_mode(bool is_dark);
    void set_scale(float scale);
    void update_by_mode(ConfigOptionMode mode);
    void reset();
    bool has_data() const ;
    bool can_export_toolpaths() const;
    bool update_sequential_view_current(unsigned int first, unsigned int last);
    void set_layers_z_range(const std::array<unsigned int, 2>& layers_z_range);
    void export_toolpaths_to_obj(const char* filename) const;
    void refresh(const GCodeProcessorResult& gcode_result, const std::vector<std::string>& str_tool_colors);
    void update_shells_color_by_extruder(const DynamicPrintConfig *config);
    void set_shell_transparency(float alpha);
    void refresh_render_paths(bool, bool);
    void render_shells(int width, int height);
    void render_toolpaths();
    void render_marker(int width, int height);
    void render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const BoundingBoxf3& box);
    void load_shells(const Print& print, bool initialized, bool force_previewing);
    void log_memory_used(const std::string& label, int64_t additional) const;
    void set_render_mode(int mode);
    void update_marker_curr_move();

    const float                get_max_print_height() const;
    const BoundingBoxf3& get_paths_bounding_box() const;
    const BoundingBoxf3& get_max_bounding_box() const;
    const BoundingBoxf3& get_shell_bounding_box() const;
    const std::vector<double>& get_layers_zs() const;
    const std::array<unsigned int, 2>& get_layers_z_range() const;
    bool is_contained_in_bed() const;
    EViewType get_view_type() const;
    void set_view_type(EViewType type, bool reset_feature_type_visible);
    std::vector<CustomGCode::Item>& get_custom_gcode_per_print_z();
    size_t get_extruders_count();

private:
    std::shared_ptr<GCodeViewerData> m_data = nullptr;
    GCodeRenderer* m_renderer;

};

};
};
};


#endif
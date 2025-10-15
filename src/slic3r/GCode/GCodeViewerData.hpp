#ifndef _slic3r_GCodeViewerData_hpp_
#define _slic3r_GCodeViewerData_hpp_

#include "GCodeDefine.hpp"
#include "GCodeViewerState.hpp"

namespace Slic3r {
class PresetBundle;
namespace GUI {
namespace GCode {

class GCodeViewerData
{
public:
    GCodeViewerData();

    void init(ConfigOptionMode mode, PresetBundle* preset_bundle);

    void load_gcode(const GCodeProcessorResult& gcode_result, const Print& print, const BuildVolume& build_volume,
        const std::vector<BuildVolume>& sub_build_volumes,
        const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode);

    void refresh(const GCodeProcessorResult& gcode_result, const std::vector<std::string>& str_tool_colors);

    void update_by_mode(ConfigOptionMode mode);
    void reset();

    bool is_visible(ExtrusionRole role);
    bool is_visible(const Path& path);

    void set_view_type(EViewType type, bool reset_feature_type_visible = true);
    void reset_visible(EViewType type);

    bool is_toolpath_move_type_visible(EMoveType type) const;
    void set_toolpath_move_type_visible(EMoveType type, bool visible);

    bool has_data() const;
    
    void set_scale(float scale);

    bool update_viewer_state_current(unsigned int first, unsigned int last);
    void on_change_color_mode(bool is_dark);

private:
    void load_parameters1(const GCodeProcessorResult& gcode_result, bool only_gcode);
    void load_toolpaths(const GCodeProcessorResult& gcode_result, const BuildVolume& build_volume, const std::vector<BuildVolume>& sub_build_volumes,const std::vector<BoundingBoxf3>& exclude_bounding_box);
    void load_parameters2(const GCodeProcessorResult& gcode_result);

public:
    float m_scale = 1.0;
    bool m_gl_data_initialized { false };
    
    size_t m_last_result_id;
    size_t m_moves_count{ 0 };
    //BBS: save m_gcode_result as well
    const GCodeProcessorResult* m_gcode_result{ NULL };
    //BBS: add only gcode mode
    bool m_only_gcode_in_preview{ false };
    std::vector<size_t> m_ssid_to_moveid_map;

    std::vector<TBuffer> m_buffers{ static_cast<size_t>(EMoveType::Extrude) };
    // bounding box of toolpaths
    BoundingBoxf3 m_paths_bounding_box;
    // bounding box of toolpaths + marker tools
    BoundingBoxf3 m_max_bounding_box;
    //BBS: add shell bounding box
    BoundingBoxf3 m_shell_bounding_box;
    float m_max_print_height{ 0.0f };

    //BBS save m_tools_color and m_tools_visible
    ETools m_tools;
    ConfigOptionMode m_user_mode;
    bool m_fold = { false };
    IdexMode m_idex_mode { IdexMode_Pack };

    PrintEstimatedStatistics m_print_statistics;
    PrintEstimatedStatistics::ETimeMode m_time_estimate_mode{ PrintEstimatedStatistics::ETimeMode::Normal };
    ConflictResultOpt m_conflict_result;

    Layers m_layers;
    std::array<unsigned int, 2> m_layers_z_range;
    std::vector<ExtrusionRole> m_roles;
    size_t m_extruders_count;
    std::vector<unsigned char> m_extruder_ids;
    std::vector<float> m_filament_diameters;
    std::vector<float> m_filament_densities;
    Extrusions m_extrusions;

    /*BBS GUI refactor, store displayed items in color scheme combobox */
    std::vector<EViewType> view_type_items;
    std::vector<std::string> view_type_items_str;
    int       m_view_type_sel = 0;
    EViewType m_view_type{ EViewType::FeatureType };
    std::vector<EMoveType> options_items;

    GCodeProcessorResult::SettingsIds m_settings_ids;
    std::vector<CustomGCode::Item> m_custom_gcode_per_print_z;

    bool m_contained_in_bed{ true };

    std::vector<int> m_plater_extruder;

    GCodeViewerState m_sequential_view;

    Vec2d  ms_offset;
    Vec2d mirror_center;
};

};
};
};


#endif 
#pragma once
#include "libslic3r/PrintConfig.hpp"
#include "slic3r/Theme/BitmapCache.hpp"
#include "libslic3r/Point.hpp"

namespace Slic3r {
    class DynamicPrintConfig;
    std::vector<int> get_min_flush_volumes(const DynamicPrintConfig& full_config);


    // Caching bitmaps for the all bitmaps, used in preset comboboxes
    GUI::BitmapCache& bitmap_cache();

    class Preset;
    wxBitmap *get_filament_preset_bmp(Preset const &preset);
    wxBitmap *get_printer_preset_bmp(Preset const &preset);

    wxBitmap* get_preset_bmp(  std::string bitmap_key, const std::string& main_icon_name, const std::string& next_icon_name,
                        bool is_enabled = true, bool is_compatible = true, bool is_system = false);
    wxBitmap* get_preset_bmp(  std::string bitmap_key, bool wide_icons, const std::string& main_icon_name, 
                        bool is_compatible = true, bool is_system = false, bool is_single_bar = false,
                        const std::string& filament_rgb = "", const std::string& extruder_rgb = "", const std::string& material_rgb = "");






    std::vector<Pointfs> spilte_four_shapes(const Pointfs& shape, float xSpace, float ySpace);
    void get_bed_shapes_from_config(const Slic3r::DynamicPrintConfig& config, std::vector<Pointfs>& bed_shapes, std::vector<Pointfs>& exclude_shapes);
    BedDivide hot_bed_divide_from_config(const Slic3r::DynamicPrintConfig& config);
    IdexMode idex_mode_from_config(const Slic3r::DynamicPrintConfig& config);

    bool is_mirror_mode_config(const Slic3r::DynamicPrintConfig& config);
    bool is_copy_mode_config(const Slic3r::DynamicPrintConfig& config);
    bool is_normal_mode_config(const Slic3r::DynamicPrintConfig& config);

    int platepart_area_count_from_config(const Slic3r::DynamicPrintConfig& config);
}
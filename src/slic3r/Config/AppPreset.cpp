#include "AppPreset.hpp"

#include "libslic3r/PresetBundle.hpp"
#include "slic3r/GUI/AppAdapter.hpp"

namespace Slic3r {
namespace GUI {

    PresetBundle*   app_preset_bundle()
    {
        return AppAdapter::gui_app()->preset_bundle;        
    }

    // extruders count from selected printer preset
    int preset_extruders_cnt()
    {
        const Preset& preset = app_preset_bundle()->printers.get_selected_preset();
        return preset.printer_technology() == ptSLA ? 1 :
            preset.config.option<ConfigOptionFloats>("nozzle_diameter")->values.size();
    }

    // extruders count from edited printer preset
    int preset_extruders_edited_cnt()
    {
        const Preset& preset = app_preset_bundle()->printers.get_edited_preset();
        return preset.printer_technology() == ptSLA ? 1 :
            preset.config.option<ConfigOptionFloats>("nozzle_diameter")->values.size();
    }

    // BBS
    int preset_filaments_cnt()
    {
        return app_preset_bundle()->filament_presets.size();
    }

    bool is_support_filament(int extruder_id)
    {
        auto &filament_presets = app_preset_bundle()->filament_presets;
        auto &filaments        = app_preset_bundle()->filaments;

        if (extruder_id >= filament_presets.size()) return false;

        Slic3r::Preset *filament = filaments.find_preset(filament_presets[extruder_id]);
        if (filament == nullptr) return false;

        Slic3r::ConfigOptionBools *support_option = dynamic_cast<Slic3r::ConfigOptionBools *>(filament->config.option("filament_is_support"));
        if (support_option == nullptr) return false;

        return support_option->get_at(0);
    }

    PresetCollection* app_printer_preset_collection()
    {
        return &app_preset_bundle()->printers;
    }
}
}
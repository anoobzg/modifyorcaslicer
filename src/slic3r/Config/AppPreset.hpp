#pragma once
#include "libslic3r/Config.hpp"

namespace Slic3r {
class PresetBundle;
class PresetCollection;
namespace GUI {
    int             preset_extruders_cnt();
    int             preset_extruders_edited_cnt();
    int             preset_filaments_cnt();

    PresetBundle*   app_preset_bundle();

    bool is_support_filament(int extruder_id);

    PresetCollection* app_printer_preset_collection();
}
}
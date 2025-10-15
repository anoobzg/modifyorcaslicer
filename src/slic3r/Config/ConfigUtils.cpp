#include "ConfigUtils.hpp"
#include "libslic3r/PrintConfig.hpp"

namespace Slic3r {

std::vector<int> get_min_flush_volumes(const DynamicPrintConfig& full_config)
{
    std::vector<int>extra_flush_volumes;
    //const auto& full_config = app_preset_bundle()->full_config();
    //auto& printer_config = app_preset_bundle()->printers.get_edited_preset().config;

    const ConfigOption* nozzle_volume_opt = full_config.option("nozzle_volume");
    int nozzle_volume_val = nozzle_volume_opt ? (int)nozzle_volume_opt->getFloat() : 0;

    const ConfigOptionInt* enable_long_retraction_when_cut_opt = full_config.option<ConfigOptionInt>("enable_long_retraction_when_cut");
    int machine_enabled_level = 0;
    if (enable_long_retraction_when_cut_opt) {
        machine_enabled_level = enable_long_retraction_when_cut_opt->value;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": get enable_long_retraction_when_cut from config, value=%1%")%machine_enabled_level;
    }
    const ConfigOptionBools* long_retractions_when_cut_opt = full_config.option<ConfigOptionBools>("long_retractions_when_cut");
    bool machine_activated = false;
    if (long_retractions_when_cut_opt) {
        machine_activated = long_retractions_when_cut_opt->values[0] == 1;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": get long_retractions_when_cut from config, value=%1%, activated=%2%")%long_retractions_when_cut_opt->values[0] %machine_activated;
    }

    size_t filament_size = full_config.option<ConfigOptionFloats>("filament_diameter")->values.size();
    std::vector<double> filament_retraction_distance_when_cut(filament_size, 18.0f), printer_retraction_distance_when_cut(filament_size, 18.0f);
    std::vector<unsigned char> filament_long_retractions_when_cut(filament_size, 0);
    const ConfigOptionFloats* filament_retraction_distances_when_cut_opt = full_config.option<ConfigOptionFloats>("filament_retraction_distances_when_cut");
    if (filament_retraction_distances_when_cut_opt) {
        filament_retraction_distance_when_cut = filament_retraction_distances_when_cut_opt->values;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": get filament_retraction_distance_when_cut from config, size=%1%, values=%2%")%filament_retraction_distance_when_cut.size() %filament_retraction_distances_when_cut_opt->serialize();
    }

    const ConfigOptionFloats* printer_retraction_distance_when_cut_opt = full_config.option<ConfigOptionFloats>("retraction_distances_when_cut");
    if (printer_retraction_distance_when_cut_opt) {
        printer_retraction_distance_when_cut = printer_retraction_distance_when_cut_opt->values;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": get retraction_distances_when_cut from config, size=%1%, values=%2%")%printer_retraction_distance_when_cut.size() %printer_retraction_distance_when_cut_opt->serialize();
    }

    const ConfigOptionBools* filament_long_retractions_when_cut_opt = full_config.option<ConfigOptionBools>("filament_long_retractions_when_cut");
    if (filament_long_retractions_when_cut_opt) {
        filament_long_retractions_when_cut = filament_long_retractions_when_cut_opt->values;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": get filament_long_retractions_when_cut from config, size=%1%, values=%2%")%filament_long_retractions_when_cut.size() %filament_long_retractions_when_cut_opt->serialize();
    }

    for (size_t idx = 0; idx < filament_size; ++idx) {
        int extra_flush_volume = nozzle_volume_val;
        int retract_length = machine_enabled_level && machine_activated ? printer_retraction_distance_when_cut[0] : 0;

        unsigned char filament_activated = filament_long_retractions_when_cut[idx];
        double filament_retract_length = filament_retraction_distance_when_cut[idx];

        if (filament_activated == 0)
            retract_length = 0;
        else if (filament_activated == 1 && machine_enabled_level == LongRectrationLevel::EnableFilament) {
            if (!std::isnan(filament_retract_length))
                retract_length = (int)filament_retraction_distance_when_cut[idx];
            else
                retract_length = printer_retraction_distance_when_cut[0];
        }

        extra_flush_volume -= PI * 1.75 * 1.75 / 4 * retract_length;
        extra_flush_volumes.emplace_back(extra_flush_volume);
    }
    return extra_flush_volumes;
}

GUI::BitmapCache& bitmap_cache()
{
    static GUI::BitmapCache bmps;
    return bmps;
}

wxBitmap* get_filament_preset_bmp(Preset const &preset)
{
    static wxBitmap sbmp;
    // if (m_type == Preset::TYPE_FILAMENT) {
    //     Preset const & preset2 = &m_collection->get_selected_preset() == &preset ? m_collection->get_edited_preset() : preset;
    //     wxString color = preset2.config.opt_string("default_filament_colour", 0);
    //     wxColour clr(color);
    //     if (clr.IsOk()) {
    //         std::string bitmap_key = "default_filament_colour_" + color.ToStdString();
    //         wxBitmap *bmp        = bitmap_cache().find(bitmap_key);
    //         if (bmp == nullptr) {
    //             wxImage img(16, 16);
    //             if (clr.Red() > 224 && clr.Blue() > 224 && clr.Green() > 224) {
    //                 img.SetRGB(wxRect({0, 0}, img.GetSize()), 128, 128, 128);
    //                 img.SetRGB(wxRect({1, 1}, img.GetSize() - wxSize{2, 2}), clr.Red(), clr.Green(), clr.Blue());
    //             } else {
    //                 img.SetRGB(wxRect({0, 0}, img.GetSize()), clr.Red(), clr.Green(), clr.Blue());
    //             }
    //             bmp = new wxBitmap(img);
    //             bmp = bitmap_cache().insert(bitmap_key, *bmp);
    //         }
    //         return bmp;
    //     }
    // }
    return &sbmp;
}

wxBitmap *get_printer_preset_bmp(Preset const &preset)
{
    static wxBitmap sbmp;
    return &sbmp; 
}

wxBitmap* get_preset_bmp(  std::string bitmap_key, const std::string& main_icon_name, const std::string& next_icon_name,
                    bool is_enabled, bool is_compatible, bool is_system)
{
    static wxBitmap bmp;
    return &bmp;   
}

wxBitmap* get_preset_bmp(  std::string bitmap_key, bool wide_icons, const std::string& main_icon_name,
                                    bool is_compatible/* = true*/, bool is_system/* = false*/, bool is_single_bar/* = false*/,
                                    const std::string& filament_rgb/* = ""*/, const std::string& extruder_rgb/* = ""*/, const std::string& material_rgb/* = ""*/)
{
    static wxBitmap bmp;
    return &bmp;
}




std::vector<Pointfs> spilte_four_shapes(const Pointfs& shape, float xSpace, float ySpace)
{
    auto calDistance = [](Vec2d a, Vec2d b) {

        Vec2d dis = b - a;
        return  sqrt(dis.x() * dis.x() + dis.y() * dis.y());
        };

    if (shape.size() < 4)
        return {};

    float dis1 = calDistance(shape[0], shape[1]);
    float dis2 = calDistance(shape[0], shape[3]);

    Vec2d dir1 = shape[1] - shape[0];
    Vec2d dir2 = shape[3] - shape[0];

    Pointfs shape1;
    Pointfs shape2;
    Pointfs shape3;
    Pointfs shape4;

    shape1.resize(4);
    shape2.resize(4);
    shape3.resize(4);
    shape4.resize(4);

    shape1[0] = shape[0];
    shape1[1] = shape1[0] + dir1 * ((dis1 * 0.5 - xSpace * 0.5) / dis1);
    shape1[2] = shape1[1] + dir2 * ((dis2 * 0.5 - ySpace * 0.5) / dis2);
    shape1[3] = shape1[0] + dir2 * ((dis2 * 0.5 - ySpace * 0.5) / dis2);

    BOOST_LOG_TRIVIAL(info) << "shapes1: " << shape1[0] << ", " << shape1[1] << ", " << shape1[2] << ", " << shape1[3];


    shape2[0] = shape[1] - dir1 * ((dis1 * 0.5 - xSpace * 0.5) / dis1);
    shape2[1] = shape[1];
    shape2[2] = shape2[1] + dir2 * ((dis2 * 0.5 - ySpace * 0.5) / dis2);
    shape2[3] = shape2[0] + dir2 * ((dis2 * 0.5 - ySpace * 0.5) / dis2);

    BOOST_LOG_TRIVIAL(info) << "shapes2: " << shape2[0] << ", " << shape2[1] << ", " << shape2[2] << ", " << shape2[3];

    shape3[0] = shape[2] - dir1 * ((dis1 * 0.5 - xSpace * 0.5) / dis1) - dir2 * ((dis2 * 0.5 - ySpace * 0.5) / dis2);
    shape3[1] = shape3[0] + dir1 * ((dis1 * 0.5 - xSpace * 0.5) / dis1);
    shape3[2] = shape[2];
    shape3[3] = shape3[0] + dir2 * ((dis2 * 0.5 - ySpace * 0.5) / dis2);

    BOOST_LOG_TRIVIAL(info) << "shapes3: " << shape3[0] << ", " << shape3[1] << ", " << shape3[2] << ", " << shape3[3];

    shape4[3] = shape[3];
    shape4[0] = shape4[3] - dir2 * ((dis2 * 0.5 - ySpace * 0.5) / dis2);
    shape4[2] = shape4[3] + dir1 * ((dis1 * 0.5 - xSpace * 0.5) / dis1);
    shape4[1] = shape4[0] + dir1 * ((dis1 * 0.5 - xSpace * 0.5) / dis1);

    BOOST_LOG_TRIVIAL(info) << "shapes4: " << shape4[0] << ", " << shape4[1] << ", " << shape4[2] << ", " << shape4[3];


    return { shape1 ,shape2 ,shape3 ,shape4 };
}

void get_bed_shapes_from_config(const Slic3r::DynamicPrintConfig& config, std::vector<Pointfs>& bed_shapes, std::vector<Pointfs>& exclude_shapes)
{
    Pointfs shape = config.option<ConfigOptionPoints>("printable_area")->values;
    Pointfs exclude_shape = config.option<ConfigOptionPoints>("bed_exclude_area")->values;
    std::string printer_model = config.option<ConfigOptionString>("printer_model")->value;
    BedDivide bed_divide = hot_bed_divide_from_config(config);
    IdexMode idex_mode = idex_mode_from_config(config);
    
    // Only split bed shape for Copy/Mirror mode
    // Pack mode (e.g., multi-nozzle calibration) keeps single large platform
    bool should_split = boost::contains(printer_model, "LightMaker L1") 
                        && bed_divide == BedDivide::Four_Areas
                        && (idex_mode == IdexMode::IdexMode_Copy || idex_mode == IdexMode::IdexMode_Mirror);
    
    if (should_split) {
        bed_shapes = spilte_four_shapes(shape, 10.0f, 10.0f);
        exclude_shapes = { {}, {}, {}, {}};
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ 
            << " - Splitting bed into 4 areas for IDEX Copy/Mirror mode";
    } else {
        bed_shapes.push_back(shape);
        exclude_shapes = { {} };
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ 
            << " - Using single bed area (idex_mode=" 
            << (idex_mode == IdexMode::IdexMode_Pack ? "Pack" : "Other") << ")";
    }
}

BedDivide hot_bed_divide_from_config(const Slic3r::DynamicPrintConfig& config)
{
    BedDivide divide = (BedDivide)config.option<ConfigOptionEnum<BedDivide>>("hot_bed_divide")->value;
    return divide;    
}

IdexMode idex_mode_from_config(const Slic3r::DynamicPrintConfig& config)
{
    std::string printer_model = config.option<ConfigOptionString>("printer_model")->value;
    bool is_L1_model = boost::contains(printer_model, "LightMaker L1");
    BedDivide divide = hot_bed_divide_from_config(config);
    if (divide == BedDivide::One_Area || !is_L1_model)
        return IdexMode::IdexMode_Pack;
    else 
    {
        IdexMode mode = (IdexMode)config.option<ConfigOptionEnum<IdexMode>>("idex_mode")->value;
        return mode;
    }
}

bool is_mirror_mode_config(const Slic3r::DynamicPrintConfig& config)
{
    return idex_mode_from_config(config) == IdexMode::IdexMode_Mirror;
}

bool is_copy_mode_config(const Slic3r::DynamicPrintConfig& config)
{
    return idex_mode_from_config(config) == IdexMode::IdexMode_Copy;
}

bool is_normal_mode_config(const Slic3r::DynamicPrintConfig& config)
{
    return idex_mode_from_config(config) == IdexMode::IdexMode_Pack;
}

int platepart_area_count_from_config(const Slic3r::DynamicPrintConfig& config)
{
    int count = 1;

    IdexMode mode = idex_mode_from_config(config);
    
    // IDEX Copy/Mirror mode: 2 actual render areas (top-left, top-right)
    // Bottom areas auto copy/mirror from top
    if (mode == IdexMode::IdexMode_Copy || mode == IdexMode::IdexMode_Mirror)
    {
        count = 2;  // 2 Print areas: top-left, top-right
    }
    else
    {
        count = 1;
    }   
    return count; 
}

}
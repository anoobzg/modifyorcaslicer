#include "GCodeDefine.hpp"
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/Render/AppRender.hpp"
#include <gl/glew.h>
#include "slic3r/GUI/I18N.hpp"

namespace Slic3r {
namespace GUI {
namespace GCode {

const float GCODE_VIEWER_SLIDER_SCALE = 0.6f;
const float SLIDER_DEFAULT_RIGHT_MARGIN  = 10.0f;
const float SLIDER_DEFAULT_BOTTOM_MARGIN = 10.0f;
const float SLIDER_RIGHT_MARGIN = 124.0f;
const float SLIDER_BOTTOM_MARGIN = 64.0f;

const std::vector<ColorRGBA> Extrusion_Role_Colors{ {
    { 0.90f, 0.70f, 0.70f, 1.0f },   // erNone
    { 1.00f, 0.90f, 0.30f, 1.0f },   // erPerimeter
    { 1.00f, 0.49f, 0.22f, 1.0f },   // erExternalPerimeter
    { 0.12f, 0.12f, 1.00f, 1.0f },   // erOverhangPerimeter
    { 0.69f, 0.19f, 0.16f, 1.0f },   // erInternalInfill
    { 0.59f, 0.33f, 0.80f, 1.0f },   // erSolidInfill
    { 0.94f, 0.25f, 0.25f, 1.0f },   // erTopSolidInfill
    { 0.40f, 0.36f, 0.78f, 1.0f },   // erBottomSurface
    { 1.00f, 0.55f, 0.41f, 1.0f },   // erIroning
    { 0.30f, 0.40f, 0.63f, 1.0f },   // erBridgeInfill
    { 0.30f, 0.50f, 0.73f, 1.0f },   // erInternalBridgeInfill
    { 1.00f, 1.00f, 1.00f, 1.0f },   // erGapFill
    { 0.00f, 0.53f, 0.43f, 1.0f },   // erSkirt
    { 0.00f, 0.23f, 0.43f, 1.0f },   // erBrim
    { 0.00f, 1.00f, 0.00f, 1.0f },   // erSupportMaterial
    { 0.00f, 0.50f, 0.00f, 1.0f },   // erSupportMaterialInterface
    { 0.00f, 0.25f, 0.00f, 1.0f },   // erSupportTransition
    { 0.70f, 0.89f, 0.67f, 1.0f },   // erWipeTower
    { 0.37f, 0.82f, 0.58f, 1.0f }    // erCustom
}};

const std::vector<ColorRGBA> Options_Colors{ {
    { 0.803f, 0.135f, 0.839f, 1.0f },   // Retractions
    { 0.287f, 0.679f, 0.810f, 1.0f },   // Unretractions
    { 0.900f, 0.900f, 0.900f, 1.0f },   // Seams
    { 0.758f, 0.744f, 0.389f, 1.0f },   // ToolChanges
    { 0.856f, 0.582f, 0.546f, 1.0f },   // ColorChanges
    { 0.322f, 0.942f, 0.512f, 1.0f },   // PausePrints
    { 0.886f, 0.825f, 0.262f, 1.0f }    // CustomGCodes
}};

const std::vector<ColorRGBA> Travel_Colors{ {
    { 0.219f, 0.282f, 0.609f, 1.0f }, // Move
    { 0.112f, 0.422f, 0.103f, 1.0f }, // Extrude
    { 0.505f, 0.064f, 0.028f, 1.0f }  // Retract
}};

// Normal ranges
// blue to red
const std::vector<ColorRGBA> Range_Colors{ {
    decode_color_to_float_array("#0b2c7a"),  // bluish
    decode_color_to_float_array("#135985"),
    decode_color_to_float_array("#1c8891"),
    decode_color_to_float_array("#04d60f"),
    decode_color_to_float_array("#aaf200"),
    decode_color_to_float_array("#fcf903"),
    decode_color_to_float_array("#f5ce0a"),
    //decode_color_to_float_array("#e38820"),
    decode_color_to_float_array("#d16830"),
    decode_color_to_float_array("#c2523c"),
    decode_color_to_float_array("#942616")    // reddish
}};

const ColorRGBA Wipe_Color    = ColorRGBA::YELLOW();
const ColorRGBA Neutral_Color = ColorRGBA::DARK_GRAY();

std::string get_view_type_string(EViewType view_type)
{
    if (view_type == EViewType::FeatureType)
       return _u8L("Line Type");
    else if (view_type == EViewType::Height)
       return _u8L("Layer Height");
    else if (view_type == EViewType::Width)
       return _u8L("Line Width");
    else if (view_type == EViewType::Feedrate)
       return _u8L("Speed");
    else if (view_type == EViewType::FanSpeed)
       return _u8L("Fan Speed");
    else if (view_type == EViewType::Temperature)
       return _u8L("Temperature");
    else if (view_type == EViewType::VolumetricRate)
       return _u8L("Flow");
    else if (view_type == EViewType::Tool)
       return _u8L("Tool");
    else if (view_type == EViewType::ColorPrint)
       return _u8L("Filament");
    else if (view_type == EViewType::LayerTime)
       return _u8L("Layer Time");
    else if (view_type == EViewType::LayerTimeLog)
       return _u8L("Layer Time (log)");
    return "";
}

unsigned char buffer_id(EMoveType type) {
    return static_cast<unsigned char>(type) - static_cast<unsigned char>(EMoveType::Retract);
}

EMoveType buffer_type(unsigned char id) {
    return static_cast<EMoveType>(static_cast<unsigned char>(EMoveType::Retract) + id);
}

// Round to a bin with minimum two digits resolution.
// Equivalent to conversion to string with sprintf(buf, "%.2g", value) and conversion back to float, but faster.
float round_to_bin(const float value)
{
    //    assert(value > 0);
    constexpr float const scale[5] = { 100.f,  1000.f,  10000.f,  100000.f,  1000000.f };
    constexpr float const invscale[5] = { 0.01f,  0.001f,  0.0001f,  0.00001f,  0.000001f };
    constexpr float const threshold[5] = { 0.095f, 0.0095f, 0.00095f, 0.000095f, 0.0000095f };
    // Scaling factor, pointer to the tables above.
    int                   i = 0;
    // While the scaling factor is not yet large enough to get two integer digits after scaling and rounding:
    for (; value < threshold[i] && i < 4; ++i);
    return std::round(value * scale[i]) * invscale[i];
}

// Find an index of a value in a sorted vector, which is in <z-eps, z+eps>.
// Returns -1 if there is no such member.
int find_close_layer_idx(const std::vector<double>& zs, double& z, double eps)
{
    if (zs.empty()) return -1;
    auto it_h = std::lower_bound(zs.begin(), zs.end(), z);
    if (it_h == zs.end()) {
        auto it_l = it_h;
        --it_l;
        if (z - *it_l < eps) return int(zs.size() - 1);
    }
    else if (it_h == zs.begin()) {
        if (*it_h - z < eps) return 0;
    }
    else {
        auto it_l = it_h;
        --it_l;
        double dist_l = z - *it_l;
        double dist_h = *it_h - z;
        if (std::min(dist_l, dist_h) < eps) { return (dist_l < dist_h) ? int(it_l - zs.begin()) : int(it_h - zs.begin()); }
    }
    return -1;
}


ColorRGBA option_color(EMoveType move_type)
{
    switch (move_type)
    {
    case EMoveType::Tool_change: { return Options_Colors[static_cast<unsigned int>(EOptionsColors::ToolChanges)]; }
    case EMoveType::Color_change: { return Options_Colors[static_cast<unsigned int>(EOptionsColors::ColorChanges)]; }
    case EMoveType::Pause_Print: { return Options_Colors[static_cast<unsigned int>(EOptionsColors::PausePrints)]; }
    case EMoveType::Custom_GCode: { return Options_Colors[static_cast<unsigned int>(EOptionsColors::CustomGCodes)]; }
    case EMoveType::Retract: { return Options_Colors[static_cast<unsigned int>(EOptionsColors::Retractions)]; }
    case EMoveType::Unretract: { return Options_Colors[static_cast<unsigned int>(EOptionsColors::Unretractions)]; }
    case EMoveType::Seam: { return Options_Colors[static_cast<unsigned int>(EOptionsColors::Seams)]; }
    default: { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
    }
}

void VBuffer::reset()
{
    // release gpu memory
    if (!vbos.empty()) {
        glsafe(::glDeleteBuffers(static_cast<GLsizei>(vbos.size()), static_cast<const GLuint*>(vbos.data())));
        vbos.clear();
    }
    sizes.clear();
    count = 0;
}

void InstanceVBuffer::Ranges::reset()
{
    for (Range& range : ranges) {
        // release gpu memory
        if (range.vbo > 0)
            glsafe(::glDeleteBuffers(1, &range.vbo));
    }

    ranges.clear();
}

void InstanceVBuffer::reset()
{
    s_ids.clear();
    s_ids.shrink_to_fit();
    buffer.clear();
    buffer.shrink_to_fit();
    render_ranges.reset();
}

void IBuffer::reset()
{
    // release gpu memory
    if (ibo > 0) {
        glsafe(::glDeleteBuffers(1, &ibo));
        ibo = 0;
    }

    vbo = 0;
    count = 0;
}

bool Path::matches(const GCodeProcessorResult::MoveVertex& move) const
{
    auto matches_percent = [](float value1, float value2, float max_percent) {
        return std::abs(value2 - value1) / value1 <= max_percent;
    };

    switch (move.type)
    {
    case EMoveType::Tool_change:
    case EMoveType::Color_change:
    case EMoveType::Pause_Print:
    case EMoveType::Custom_GCode:
    case EMoveType::Retract:
    case EMoveType::Unretract:
    case EMoveType::Seam:
    case EMoveType::Extrude: {
        // use rounding to reduce the number of generated paths
        return type == move.type && extruder_id == move.extruder_id && cp_color_id == move.cp_color_id && role == move.extrusion_role &&
            move.position.z() <= sub_paths.front().first.position.z() && feedrate == move.feedrate && fan_speed == move.fan_speed &&
            height == round_to_bin(move.height) && width == round_to_bin(move.width) &&
            matches_percent(volumetric_rate, move.volumetric_rate(), 0.05f) && layer_time == move.layer_duration;
    }
    case EMoveType::Travel: {
        return type == move.type && feedrate == move.feedrate && extruder_id == move.extruder_id && cp_color_id == move.cp_color_id;
    }
    default: { return false; }
    }
}

void TBuffer::Model::reset()
{
    instances.reset();
}

void TBuffer::reset()
{
    vertices.reset();
    for (IBuffer& buffer : indices) {
        buffer.reset();
    }

    indices.clear();
    paths.clear();
    render_paths.clear();
    model.reset();
}

void TBuffer::add_path(const GCodeProcessorResult::MoveVertex& move, unsigned int b_id, size_t i_id, size_t s_id)
{
    Path::Endpoint endpoint = { b_id, i_id, s_id, move.position };
    // use rounding to reduce the number of generated paths
    paths.push_back({ move.type, move.extrusion_role, move.delta_extruder,
        round_to_bin(move.height), round_to_bin(move.width),
        move.feedrate, move.fan_speed, move.temperature,
        move.volumetric_rate(), move.layer_duration, move.extruder_id, move.cp_color_id, { { endpoint, endpoint } } });
}

ColorRGBA Extrusions::Range::get_color_at(float value) const
{
    // Input value scaled to the colors range
    const float step = step_size();
    float _min = min;
    if(log_scale) {
        value = std::log(value);
        _min = std::log(min);
    }
    const float global_t = (step != 0.0f) ? std::max(0.0f, value - _min) / step : 0.0f; // lower limit of 0.0f

    const size_t color_max_idx = Range_Colors.size() - 1;

    // Compute the two colors just below (low) and above (high) the input value
    const size_t color_low_idx = std::clamp<size_t>(static_cast<size_t>(global_t), 0, color_max_idx);
    const size_t color_high_idx = std::clamp<size_t>(color_low_idx + 1, 0, color_max_idx);

    // Interpolate between the low and high colors to find exactly which color the input value should get
    return lerp(Range_Colors[color_low_idx], Range_Colors[color_high_idx], global_t - static_cast<float>(color_low_idx));
}

float Extrusions::Range::step_size() const {
if (log_scale)
    {
        float min_range = min;
        if (min_range == 0)
            min_range = 0.001f;
        return (std::log(max / min_range) / (static_cast<float>(Range_Colors.size()) - 1.0f));
    } else
    return (max - min) / (static_cast<float>(Range_Colors.size()) - 1.0f);
}

float Extrusions::Range::get_value_at_step(int step) const {
    if (!log_scale)
        return min + static_cast<float>(step) * step_size();
    else
    return std::exp(std::log(min) + static_cast<float>(step) * step_size());
    
}
SequentialRangeCap::~SequentialRangeCap() {
    if (ibo > 0)
        glsafe(::glDeleteBuffers(1, &ibo));
}

void SequentialRangeCap::reset() {
    if (ibo > 0)
        glsafe(::glDeleteBuffers(1, &ibo));

    buffer = nullptr;
    ibo = 0;
    vbo = 0;
    color = { 0.0f, 0.0f, 0.0f, 1.0f };
}

//void SequentialView::render(const bool has_render_path, float legend_height, int canvas_width, int canvas_height, int right_margin, const EViewType& view_type)
//{
//    if (has_render_path && m_show_marker) {
//        marker.set_world_position(current_position);
//        marker.set_world_offset(current_offset);
//
//        marker.render(canvas_width, canvas_height, view_type);
//    }
//
//    //float bottom = AppAdapter::plater()->get_current_canvas3D()->get_canvas_size().get_height();
//    // BBS
//#if 0
//    if (AppAdapter::gui_app()->is_editor())
//        bottom -= AppAdapter::plater()->get_view_toolbar().get_height();
//#endif
//    if (has_render_path)
//        gcode_window.render(legend_height + 2, std::max(10.f, (float)canvas_height - 40), (float)canvas_width - (float)right_margin, static_cast<uint64_t>(gcode_ids[current.last]));
//}

};
};
};
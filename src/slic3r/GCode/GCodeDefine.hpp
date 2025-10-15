#ifndef _slic3r_GCodeDefine_hpp_
#define _slic3r_GCodeDefine_hpp_

#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Color.hpp"
#include "slic3r/Render/3DScene.hpp"
#include "slic3r/Render/GLModel.hpp"
// #include <boost/iostreams/device/mapped_file.hpp>

using namespace Slic3r::GUI;

namespace Slic3r {
namespace GUI {
namespace GCode {

extern const float GCODE_VIEWER_SLIDER_SCALE;
extern const float SLIDER_DEFAULT_RIGHT_MARGIN;
extern const float SLIDER_DEFAULT_BOTTOM_MARGIN;
extern const float SLIDER_RIGHT_MARGIN;
extern const float SLIDER_BOTTOM_MARGIN;

using IBufferType = unsigned short;
using VertexBuffer = std::vector<float>;
using MultiVertexBuffer = std::vector<VertexBuffer>;
using IndexBuffer = std::vector<IBufferType>;
using MultiIndexBuffer = std::vector<IndexBuffer>;
using InstanceBuffer = std::vector<float>;
using InstanceIdBuffer = std::vector<size_t>;
using InstancesOffsets = std::vector<Vec3f>;

extern const std::vector<ColorRGBA> Extrusion_Role_Colors;
extern const std::vector<ColorRGBA> Options_Colors;
extern const std::vector<ColorRGBA> Travel_Colors;
extern const std::vector<ColorRGBA> Range_Colors;
extern const ColorRGBA              Wipe_Color;
extern const ColorRGBA              Neutral_Color;

enum class EOptionsColors : unsigned char
{
    Retractions,
    Unretractions,
    Seams,
    ToolChanges,
    ColorChanges,
    PausePrints,
    CustomGCodes
};

enum class EViewType : unsigned char
{
   FeatureType = 0,
   Height,
   Width,
   Feedrate,
   FanSpeed,
   Temperature,
   VolumetricRate,
   Tool,
   ColorPrint,
   FilamentId,
   LayerTime,
   LayerTimeLog,
   Count
};

// vbo buffer containing vertices data used to render a specific toolpath type
struct VBuffer
{
    enum class EFormat : unsigned char
    {
        // vertex format: 3 floats -> position.x|position.y|position.z
        Position,
        // vertex format: 4 floats -> position.x|position.y|position.z|normal.x
        PositionNormal1,
        // vertex format: 6 floats -> position.x|position.y|position.z|normal.x|normal.y|normal.z
        PositionNormal3,
        // vertex format: 4 floats + 4 bytes -> position.x|position.y|position.z|normal.x|normal.y|normal.z|padding (normal as 3 bytes + 1 byte padding)
        PositionNormal3Byte
    };

    EFormat format{ EFormat::Position };
    // vbos id
    std::vector<unsigned int> vbos;
    // sizes of the buffers, in bytes, used in export to obj
    std::vector<size_t> sizes;
    // count of vertices, updated after data are sent to gpu
    size_t count{ 0 };

    size_t data_size_bytes() const { return count * vertex_size_bytes(); }
    // We set 65536 as max count of vertices inside a vertex buffer to allow
    // to use unsigned short in place of unsigned int for indices in the index buffer, to save memory
    size_t max_size_bytes() const { return 65536 * vertex_size_bytes(); }

    size_t vertex_size_floats() const { return position_size_floats() + normal_size_floats(); }
    size_t vertex_size_bytes() const { 
        if (format == EFormat::PositionNormal3Byte) {
            return position_size_bytes() + normal_size_bytes();
        }
        return vertex_size_floats() * sizeof(float); 
    }

    size_t position_offset_floats() const { return 0; }
    size_t position_offset_bytes() const { return position_offset_floats() * sizeof(float); }

    size_t position_size_floats() const { return 3; }
    size_t position_size_bytes() const { return position_size_floats() * sizeof(float); }

    size_t normal_offset_floats() const {
        assert(format == EFormat::PositionNormal1 || format == EFormat::PositionNormal3 || format == EFormat::PositionNormal3Byte);
        return position_size_floats();
    }
    size_t normal_offset_bytes() const { return normal_offset_floats() * sizeof(float); }

    size_t normal_size_floats() const {
        switch (format)
        {
        case EFormat::PositionNormal1: { return 1; }
        case EFormat::PositionNormal3: { return 3; }
        case EFormat::PositionNormal3Byte: { return 1; } // 4 bytes = 1 float
        default:                       { return 0; }
        }
    }
    size_t normal_size_bytes() const { 
        switch (format)
        {
        case EFormat::PositionNormal3Byte: { return 4; } // 3 bytes normal + 1 byte padding
        default: { return normal_size_floats() * sizeof(float); }
        }
    }

    void reset();
};

// buffer containing instances data used to render a toolpaths using instanced or batched models
// instance record format:
// instanced models: 5 floats -> position.x|position.y|position.z|width|height (which are sent to the shader as -> vec3 (offset) + vec2 (scales) in GLModel::render_instanced())
// batched models:   3 floats -> position.x|position.y|position.z
struct InstanceVBuffer
{
    // ranges used to render only subparts of the intances
    struct Ranges
    {
        struct Range
        {
            // offset in bytes of the 1st instance to render
            unsigned int offset;
            // count of instances to render
            unsigned int count;
            // vbo id
            unsigned int vbo{ 0 };
            // Color to apply to the instances
            ColorRGBA color;
        };

        std::vector<Range> ranges;

        void reset();
    };

    enum class EFormat : unsigned char
    {
        InstancedModel,
        BatchedModel
    };

    EFormat format;

    // cpu-side buffer containing all instances data
    InstanceBuffer buffer;
    // indices of the moves for all instances
    std::vector<size_t> s_ids;
    // position offsets, used to show the correct value of the tool position
    InstancesOffsets offsets;
    Ranges render_ranges;

    size_t data_size_bytes() const { return s_ids.size() * instance_size_bytes(); }

    size_t instance_size_floats() const {
        switch (format)
        {
        case EFormat::InstancedModel: { return 5; }
        case EFormat::BatchedModel: { return 3; }
        default: { return 0; }
        }
    }
    size_t instance_size_bytes() const { return instance_size_floats() * sizeof(float); }

    void reset();
};

// ibo buffer containing indices data (for lines/triangles) used to render a specific toolpath type
struct IBuffer
{
    // id of the associated vertex buffer
    unsigned int vbo{ 0 };
    // ibo id
    unsigned int ibo{ 0 };
    // count of indices, updated after data are sent to gpu
    size_t count{ 0 };

    void reset();
};

// Used to identify different toolpath sub-types inside a IBuffer
struct Path
{
    struct Endpoint
    {
        // index of the buffer in the multibuffer vector
        // the buffer type may change:
        // it is the vertex buffer while extracting vertices data,
        // the index buffer while extracting indices data
        unsigned int b_id{ 0 };
        // index into the buffer
        size_t i_id{ 0 };
        // move id
        size_t s_id{ 0 };
        Vec3f position{ Vec3f::Zero() };
    };

    struct Sub_Path
    {
        Endpoint first;
        Endpoint last;

        bool contains(size_t s_id) const {
            return first.s_id <= s_id && s_id <= last.s_id;
        }
    };

    EMoveType type{ EMoveType::Noop };
    ExtrusionRole role{ erNone };
    float delta_extruder{ 0.0f };
    float height{ 0.0f };
    float width{ 0.0f };
    float feedrate{ 0.0f };
    float fan_speed{ 0.0f };
    float temperature{ 0.0f };
    float volumetric_rate{ 0.0f };
    float layer_time{ 0.0f };
    unsigned char extruder_id{ 0 };
    unsigned char cp_color_id{ 0 };
    std::vector<Sub_Path> sub_paths;

    bool matches(const GCodeProcessorResult::MoveVertex& move) const;
    size_t vertices_count() const {
        return sub_paths.empty() ? 0 : sub_paths.back().last.s_id - sub_paths.front().first.s_id + 1;
    }
    bool contains(size_t s_id) const {
        return sub_paths.empty() ? false : sub_paths.front().first.s_id <= s_id && s_id <= sub_paths.back().last.s_id;
    }
    int get_id_of_sub_path_containing(size_t s_id) const {
        if (sub_paths.empty())
            return -1;
        else {
            for (int i = 0; i < static_cast<int>(sub_paths.size()); ++i) {
                if (sub_paths[i].contains(s_id))
                    return i;
            }
            return -1;
        }
    }
    void add_sub_path(const GCodeProcessorResult::MoveVertex& move, unsigned int b_id, size_t i_id, size_t s_id) {
        Endpoint endpoint = { b_id, i_id, s_id, move.position };
        sub_paths.push_back({ endpoint , endpoint });
    }
};

// Used to batch the indices needed to render the paths
struct RenderPath
{
    // Index of the parent tbuffer
    unsigned char               tbuffer_id;
    // Render path property
    ColorRGBA                       color;
    // Index of the buffer in TBuffer::indices
    unsigned int                ibuffer_id;
    // Render path content
    // Index of the path in TBuffer::paths
    unsigned int                path_id;
    std::vector<unsigned int>   sizes;
    std::vector<size_t>         offsets; // use size_t because we need an unsigned integer whose size matches pointer's size (used in the call glMultiDrawElements())
    bool contains(size_t offset) const {
        for (size_t i = 0; i < offsets.size(); ++i) {
            if (offsets[i] <= offset && offset <= offsets[i] + static_cast<size_t>(sizes[i] * sizeof(IBufferType)))
                return true;
        }
        return false;
    }
};

// buffer containing data for rendering a specific toolpath type
struct TBuffer
{
    enum class ERenderPrimitiveType : unsigned char
    {
        Line,
        Triangle,
        InstancedModel,
        BatchedModel
    };

    ERenderPrimitiveType render_primitive_type;

    // buffers for point, line and triangle primitive types
    VBuffer vertices;
    std::vector<IBuffer> indices;

    struct Model
    {
        GLModel model;
        ColorRGBA color;
        InstanceVBuffer instances;
        GLModel::Geometry data;

        void reset();
    };

    // contain the buffer for model primitive types
    Model model;

    std::string shader;
    std::vector<Path> paths;
    std::vector<RenderPath> render_paths;
    bool visible{ false };

    void reset();

    // b_id index of buffer contained in this->indices
    // i_id index of first index contained in this->indices[b_id]
    // s_id index of first vertex contained in this->vertices
    void add_path(const GCodeProcessorResult::MoveVertex& move, unsigned int b_id, size_t i_id, size_t s_id);

    unsigned int max_vertices_per_segment() const {
        switch (render_primitive_type)
        {
        case ERenderPrimitiveType::Line:     { return 2; }
        case ERenderPrimitiveType::Triangle: { return 8; }
        default:                             { return 0; }
        }
    }

    size_t max_vertices_per_segment_size_floats() const { return vertices.vertex_size_floats() * static_cast<size_t>(max_vertices_per_segment()); }
    size_t max_vertices_per_segment_size_bytes() const { return max_vertices_per_segment_size_floats() * sizeof(float); }
    unsigned int indices_per_segment() const {
        switch (render_primitive_type)
        {
        case ERenderPrimitiveType::Line:     { return 2; }
        case ERenderPrimitiveType::Triangle: { return 30; } // 3 indices x 10 triangles
        default:                             { return 0; }
        }
    }
    size_t indices_per_segment_size_bytes() const { return static_cast<size_t>(indices_per_segment() * sizeof(IBufferType)); }
    unsigned int max_indices_per_segment() const {
        switch (render_primitive_type)
        {
        case ERenderPrimitiveType::Line:     { return 2; }
        case ERenderPrimitiveType::Triangle: { return 36; } // 3 indices x 12 triangles
        default:                             { return 0; }
        }
    }
    size_t max_indices_per_segment_size_bytes() const { return max_indices_per_segment() * sizeof(IBufferType); }

    bool has_data() const {
        switch (render_primitive_type)
        {
        case ERenderPrimitiveType::Line:
        case ERenderPrimitiveType::Triangle: {
            return !vertices.vbos.empty() && vertices.vbos.front() != 0 && !indices.empty() && indices.front().ibo != 0;
        }
        case ERenderPrimitiveType::InstancedModel: { return model.model.is_initialized() && !model.instances.buffer.empty(); }
        case ERenderPrimitiveType::BatchedModel: {
            return !model.data.vertices.empty() && !model.data.indices.empty() &&
                !vertices.vbos.empty() && vertices.vbos.front() != 0 && !indices.empty() && indices.front().ibo != 0;
        }
        default: { return false; }
        }
    }
};

// helper to render extrusion paths
struct Extrusions
{
    struct Range
    {
        float min;
        float max;
        unsigned int count;
        bool log_scale;

        Range() { reset(); }
        void update_from(const float value) {
            if (value != max && value != min)
                ++count;
            min = std::min(min, value);
            max = std::max(max, value);
        }
        void reset(bool log = false) { min = FLT_MAX; max = -FLT_MAX; count = 0; log_scale = log; }

        float step_size() const;
        ColorRGBA get_color_at(float value) const;
        float get_value_at_step(int step) const;

    };

    struct Ranges
    {
        // Color mapping by layer height.
        Range height;
        // Color mapping by extrusion width.
        Range width;
        // Color mapping by feedrate.
        Range feedrate;
        // Color mapping by fan speed.
        Range fan_speed;
        // Color mapping by volumetric extrusion rate.
        Range volumetric_rate;
        // Color mapping by extrusion temperature.
        Range temperature;
        // Color mapping by layer time.
        Range layer_duration;
        Range layer_duration_log;
        void reset() {
            height.reset();
            width.reset();
            feedrate.reset();
            fan_speed.reset();
            volumetric_rate.reset();
            temperature.reset();
            layer_duration.reset();
            layer_duration_log.reset(true);
        }
    };

    unsigned int role_visibility_flags{ 0 };
    Ranges ranges;

    void reset_role_visibility_flags() {
        role_visibility_flags = 0;
        for (unsigned int i = 0; i < erCount; ++i) {
            role_visibility_flags |= 1 << i;
        }
    }

    void reset_ranges() { ranges.reset(); }
};

class Layers
{
public:
    struct Endpoints
    {
        size_t first{ 0 };
        size_t last{ 0 };

        bool operator == (const Endpoints& other) const { return first == other.first && last == other.last; }
        bool operator != (const Endpoints& other) const { return !operator==(other); }
    };

private:
    std::vector<double> m_zs;
    std::vector<Endpoints> m_endpoints;

public:
    void append(double z, Endpoints endpoints) {
        m_zs.emplace_back(z);
        m_endpoints.emplace_back(endpoints);
    }

    void reset() {
        m_zs = std::vector<double>();
        m_endpoints = std::vector<Endpoints>();
    }

    size_t size() const { return m_zs.size(); }
    bool empty() const { return m_zs.empty(); }
    const std::vector<double>& get_zs() const { return m_zs; }
    const std::vector<Endpoints>& get_endpoints() const { return m_endpoints; }
    std::vector<Endpoints>& get_endpoints() { return m_endpoints; }
    double get_z_at(unsigned int id) const { return (id < m_zs.size()) ? m_zs[id] : 0.0; }
    Endpoints get_endpoints_at(unsigned int id) const { return (id < m_endpoints.size()) ? m_endpoints[id] : Endpoints(); }
    int                           get_l_at(float z) const
    {
        auto iter = std::upper_bound(m_zs.begin(), m_zs.end(), z);
        return std::distance(m_zs.begin(), iter);
    }

    bool operator != (const Layers& other) const {
        if (m_zs != other.m_zs)
            return true;
        if (m_endpoints != other.m_endpoints)
            return true;
        return false;
    }
};

// used to render the toolpath caps of the current sequential range
// (i.e. when sliding on the horizontal slider)
struct SequentialRangeCap
{
    TBuffer* buffer{ nullptr };
    unsigned int ibo{ 0 };
    unsigned int vbo{ 0 };
    ColorRGBA color;

    ~SequentialRangeCap();
    bool is_renderable() const { return buffer != nullptr; }
    void reset();
    size_t indices_count() const { return 6; }
};

struct ETools
{
    std::vector<ColorRGBA> m_tool_colors;
    std::vector<bool>  m_tool_visibles;
};

extern std::string get_view_type_string(EViewType view_type);

extern unsigned char buffer_id(EMoveType type);

extern EMoveType buffer_type(unsigned char id);

// Round to a bin with minimum two digits resolution.
// Equivalent to conversion to string with sprintf(buf, "%.2g", value) and conversion back to float, but faster.
extern float round_to_bin(const float value);

// Find an index of a value in a sorted vector, which is in <z-eps, z+eps>.
// Returns -1 if there is no such member.
extern int find_close_layer_idx(const std::vector<double>& zs, double& z, double eps);

extern ColorRGBA option_color(EMoveType move_type);

};
};
};


#endif 
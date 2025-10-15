#ifndef _slic3r_GCodeViewerState_hpp_
#define _slic3r_GCodeViewerState_hpp_

#include "slic3r/Render/GLModel.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/BoundingBox.hpp"
#include <boost/iostreams/device/mapped_file.hpp>

namespace Slic3r {
namespace GUI {
namespace GCode {

class Marker
{
public:
    GLModel m_model;
    Vec3f m_world_position;
    Transform3f m_world_transform;
    // for seams, the position of the marker is on the last endpoint of the toolpath containing it
    // the offset is used to show the correct value of tool position in the "ToolPosition" window
    // see implementation of render() method
    Vec3f m_world_offset;
    float m_z_offset{ 0.5f };
    GCodeProcessorResult::MoveVertex m_curr_move;
    bool m_visible{ true };
    bool m_is_dark = false;
    float m_scale = 1.0f;

    void init(std::string filename);
    const BoundingBoxf3& get_bounding_box() const { return m_model.get_bounding_box();}
    void set_world_position(const Vec3f& position);
    void set_world_offset(const Vec3f& offset);
    bool is_visible() const;
    void set_visible(bool visible);
    void on_change_color_mode(bool is_dark);
    void update_curr_move(const GCodeProcessorResult::MoveVertex move);
    //void render(int canvas_width, int canvas_height, const EViewType& view_type);
};

struct Line
{
    std::string command;
    std::string parameters;
    std::string comment;
};

class GCodeFile
{
public:
    bool m_is_dark = false;
    uint64_t m_selected_line_id{ 0 };
    size_t m_last_lines_size{ 0 };
    std::string m_filename;
    boost::iostreams::mapped_file_source m_file;
    bool m_is_mapping{ false };
    // map for accessing data in file by line number
    std::vector<size_t> m_lines_ends;
    // current visible lines
    std::vector<Line> m_lines;

    GCodeFile() = default;
    ~GCodeFile();
    void load_gcode(const std::string& filename, const std::vector<size_t>& lines_ends);
    void reset();
    void on_change_color_mode(bool is_dark);
    void stop_mapping();
};

struct Endpoints
{
    size_t first{ 0 };
    size_t last{ 0 };
};

// replace class SequentialView, split the class to two part, data and view.
struct GCodeViewerState
{
    bool skip_invisible_moves{ false };
    Endpoints endpoints;
    Endpoints current;
    Endpoints last_current;
    Endpoints global;
    Vec3f current_position{ Vec3f::Zero() };
    Vec3f current_offset{ Vec3f::Zero() };

    Marker marker;
    GCodeFile file;
    std::vector<unsigned int> gcode_ids;
    float m_scale = 1.0;
    bool m_show_marker = false;
};

};
};
};

#endif
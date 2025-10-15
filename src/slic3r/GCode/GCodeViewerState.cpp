#include "GCodeViewerState.hpp"
#include "libslic3r/Geometry.hpp"

namespace Slic3r {
namespace GUI {
namespace GCode {

/* Marker */
void Marker::init(std::string filename)
{
    if (filename.empty()) {
        m_model.init_from(stilized_arrow(16, 1.5f, 3.0f, 0.8f, 3.0f));
    }
    else {
        m_model.init_from_file(filename);
    }
    m_model.set_color({ 1.0f, 1.0f, 1.0f, 0.5f });
}

void Marker::set_world_position(const Vec3f& position)
{
    m_world_position = position;
    m_world_transform = (Geometry::assemble_transform((position + m_z_offset * Vec3f::UnitZ()).cast<double>()) * Geometry::assemble_transform(m_model.get_bounding_box().size().z() * Vec3d::UnitZ(), { M_PI, 0.0, 0.0 })).cast<float>();
}

void Marker::set_world_offset(const Vec3f& offset) 
{
    m_world_offset = offset; 
}

bool Marker::is_visible() const 
{ 
    return m_visible; 
}

void Marker::set_visible(bool visible) 
{ 
    m_visible = visible; 
}

void Marker::on_change_color_mode(bool is_dark) 
{ 
    m_is_dark = is_dark; 
}

void Marker::update_curr_move(const GCodeProcessorResult::MoveVertex move)
{
    m_curr_move = move;
}

/* GCodeFile */
GCodeFile::~GCodeFile() 
{ 
    stop_mapping(); 
}

void GCodeFile::load_gcode(const std::string& filename, const std::vector<size_t>& lines_ends)
{
    assert(!m_file.is_open());
    if (m_file.is_open())
        return;

    if (m_is_mapping)
        return;

    m_filename = filename;
    m_lines_ends = lines_ends;

    m_selected_line_id = 0;
    m_last_lines_size = 0;

    try
    {
        m_file.open(boost::filesystem::path(m_filename));
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ": mapping file " << m_filename;
        m_is_mapping = true;
    }
    catch (...)
    {
        BOOST_LOG_TRIVIAL(error) << "Unable to map file " << m_filename << ". Cannot show G-code window.";
        reset();
    }
}

void GCodeFile::reset() 
{
    stop_mapping();
    m_lines_ends.clear();
    m_lines_ends.shrink_to_fit();
    m_lines.clear();
    m_lines.shrink_to_fit();
    m_filename.clear();
    m_filename.shrink_to_fit();
}

void GCodeFile::on_change_color_mode(bool is_dark) 
{ 
    m_is_dark = is_dark; 
}

void GCodeFile::stop_mapping()
{
    //BBS: add log to trace the gcode file issue
    if (m_file.is_open() || m_is_mapping) {
        m_file.close();
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ": finished mapping file " << m_filename;
    }
    m_is_mapping = false;
}

/* GCodeViewerState */


};
};
};
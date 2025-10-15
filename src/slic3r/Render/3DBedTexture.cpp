#include "3DBedTexture.hpp"
#include "3DBed.hpp"
#include "AppRender.hpp"
#include "slic3r/Theme/AppFont.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"

static const double LOGICAL_PART_PLATE_GAP = 1. / 5.;
static const float GROUND_Z = -0.03f;

namespace Slic3r {
namespace GUI {

Bed3DTexture::Bed3DTexture()
{
    m_is_dark = false;
    m_need_generate = true;
}

Bed3DTexture::~Bed3DTexture()
{
	release_icon_textures();
}

void Bed3DTexture::set_dark_mode(bool is_dark)
{
	if (m_is_dark != is_dark)
	{
		m_is_dark = is_dark;
		m_need_generate = true;
	}
	
}

const std::string &Bed3DTexture::get_logo_texture_filename() 
{ 
    return m_logo_texture_filename; 
}

void Bed3DTexture::update_logo_texture_filename(const std::string &texture_filename)
{
    auto check_texture = [](const std::string &texture) {
        boost::system::error_code ec; // so the exists call does not throw (e.g. after a permission problem)
        return !texture.empty() && (boost::algorithm::iends_with(texture, ".png") || boost::algorithm::iends_with(texture, ".svg")) && boost::filesystem::exists(texture, ec);
    };
    if (!texture_filename.empty() && !check_texture(texture_filename)) {
		m_logo_texture_filename = "";
        BOOST_LOG_TRIVIAL(error) << "Unable to load bed texture: " << texture_filename;
    } else
        m_logo_texture_filename = texture_filename;
}

void Bed3DTexture::generate_icon_textures()
{
	if (!m_need_generate)
		return;

// use higher resolution images if graphic card and opengl version allow
	GLint max_tex_size = (GLint)get_max_texture_size(), icon_size = max_tex_size / 8;
	std::string path = resources_dir() + "/images/";
	std::string file_name;

	if (icon_size > 256)
		icon_size = 256;
	//if (m_del_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_close_dark.svg" : "plate_close.svg");
		if (!m_del_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_del_hovered_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_close_hover_dark.svg" : "plate_close_hover.svg");
		if (!m_del_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	
	// if (m_move_front_texture.get_id() == 0)
    {
        file_name = path + (m_is_dark ? "plate_move_front_dark.svg" : "plate_move_front.svg");
        if (!m_move_front_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
        }
    }

    // if (m_move_front_hovered_texture.get_id() == 0)
    {
        file_name = path + (m_is_dark ? "plate_move_front_hover_dark.svg" : "plate_move_front_hover.svg");
        if (!m_move_front_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
        }
    }

	//if (m_arrange_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_arrange_dark.svg" : "plate_arrange.svg");
		if (!m_arrange_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_arrange_hovered_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_arrange_hover_dark.svg" : "plate_arrange_hover.svg");
		if (!m_arrange_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_orient_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_orient_dark.svg" : "plate_orient.svg");
		if (!m_orient_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_orient_hovered_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_orient_hover_dark.svg" : "plate_orient_hover.svg");
		if (!m_orient_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_locked_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_locked_dark.svg" : "plate_locked.svg");
		if (!m_locked_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_locked_hovered_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_locked_hover_dark.svg" : "plate_locked_hover.svg");
		if (!m_locked_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_lockopen_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_unlocked_dark.svg" : "plate_unlocked.svg");
		if (!m_lockopen_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_lockopen_hovered_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_unlocked_hover_dark.svg" : "plate_unlocked_hover.svg");
		if (!m_lockopen_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_bedtype_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_settings_dark.svg" : "plate_settings.svg");
		if (!m_plate_settings_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_bedtype_changed_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_settings_changed_dark.svg" : "plate_settings_changed.svg");
		if (!m_plate_settings_changed_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_bedtype_hovered_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_settings_hover_dark.svg" : "plate_settings_hover.svg");
		if (!m_plate_settings_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	//if (m_bedtype_changed_hovered_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_settings_changed_hover_dark.svg" : "plate_settings_changed_hover.svg");
		if (!m_plate_settings_changed_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	// if (m_plate_name_edit_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_name_edit_dark.svg" : "plate_name_edit.svg");
		if (!m_plate_name_edit_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		 }
	}
    // if (m_plate_name_edit_hovered_texture.get_id() == 0)
	{
		file_name = path + (m_is_dark ? "plate_name_edit_hover_dark.svg" : "plate_name_edit_hover.svg");
		if (!m_plate_name_edit_hovered_texture.load_from_svg_file(file_name, true, false, false, icon_size)) {
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
		}
	}

	std::string text_str = "01";
	wxFont* font = find_font(text_str,32);

	for (int i = 0; i < MAX_PLATES_COUNT; i++) {
		if (m_idx_textures[i].get_id() == 0) {
			//file_name = path + (boost::format("plate_%1%.svg") % (i + 1)).str();
			if ( i < 9 )
				file_name = std::string("0") + std::to_string(i+1);
			else
				file_name = std::to_string(i+1);

			wxColour foreground(0xf2, 0x75, 0x4e, 0xff);
			if (!m_idx_textures[i].generate_from_text_string(file_name, *font, *wxBLACK, foreground)) {
				BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(":load file %1% failed") % file_name;
			}
		}
	}

	m_need_generate = false;
}

void Bed3DTexture::release_icon_textures()
{
	m_logo_texture.reset();
	m_del_texture.reset();
	m_del_hovered_texture.reset();
    m_move_front_hovered_texture.reset();
    m_move_front_texture.reset();
	m_arrange_texture.reset();
	m_arrange_hovered_texture.reset();
	m_orient_texture.reset();
	m_orient_hovered_texture.reset();
	m_locked_texture.reset();
	m_locked_hovered_texture.reset();
	m_lockopen_texture.reset();
	m_lockopen_hovered_texture.reset();
	m_plate_settings_texture.reset();
	m_plate_settings_texture.reset();
	m_plate_settings_texture.reset();
	m_plate_settings_hovered_texture.reset();
	m_plate_name_edit_texture.reset();
	m_plate_name_edit_hovered_texture.reset();
	for (int i = 0;i < MAX_PLATES_COUNT; i++) {
		m_idx_textures[i].reset();
	}
	//reset
	is_load_bedtype_textures = false;
	is_load_cali_texture = false;
	for (int i = 0; i < btCount; i++) {
		for (auto& part: bed_texture_info[i].parts) {
			if (part.texture) {
				part.texture->reset();
				delete part.texture;
			}
			if (part.buffer) {
				delete part.buffer;
			}
		}
	}
}

void Bed3DTexture::set_render_option(bool bedtype_texture, bool plate_settings, bool cali)
{
    render_bedtype_logo = bedtype_texture;
    render_plate_settings = plate_settings;
    render_cali_logo = cali;
}

void Bed3DTexture::BedTextureInfo::TexturePart::update_buffer()
{
	if (w == 0 || h == 0) {
		return;
	}

	Pointfs rectangle;
	rectangle.push_back(Vec2d(x, y));
	rectangle.push_back(Vec2d(x+w, y));
	rectangle.push_back(Vec2d(x+w, y+h));
	rectangle.push_back(Vec2d(x, y+h));
	ExPolygon poly;

	for (int i = 0; i < 4; i++) {
		const Vec2d & p = rectangle[i];
		for (auto& p : rectangle) {
			Vec2d pp = Vec2d(p.x() + offset.x(), p.y() + offset.y());
			poly.contour.append({ scale_(pp(0)), scale_(pp(1)) });
		}
	}

	if (!buffer)
        buffer = new GLModel();

	buffer->reset();

	if (!init_model_from_poly(*buffer, poly, GROUND_Z + 0.02f)) {
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ":Unable to create buffer triangles\n";
	}
}

void Bed3DTexture::BedTextureInfo::TexturePart::reset()
{
    if (texture) {
        texture->reset();
        delete texture;
    }
    if (buffer)
        delete buffer;
}

void Bed3DTexture::BedTextureInfo::reset()
{
    for (size_t i = 0; i < parts.size(); i++)
        parts[i].reset();
}

void Bed3DTexture::init_bed_type_info()
{
	BedTextureInfo::TexturePart pct_part_left(10, 130, 10, 110, "orca_bed_pct_left.svg");
	BedTextureInfo::TexturePart st_part1(9, 70, 12.5, 170, "bbl_bed_st_left.svg");
	BedTextureInfo::TexturePart st_part2(74, -10, 148, 12, "bbl_bed_st_bottom.svg");
	BedTextureInfo::TexturePart pc_part1(10, 130, 10, 110, "bbl_bed_pc_left.svg");
	BedTextureInfo::TexturePart pc_part2(74, -10, 148, 12, "bbl_bed_pc_bottom.svg");
	BedTextureInfo::TexturePart ep_part1(7.5, 90, 12.5, 150, "bbl_bed_ep_left.svg");
	BedTextureInfo::TexturePart ep_part2(74, -10, 148, 12, "bbl_bed_ep_bottom.svg");
	BedTextureInfo::TexturePart pei_part1(7.5, 50, 12.5, 190, "bbl_bed_pei_left.svg");
	BedTextureInfo::TexturePart pei_part2(74, -10, 148, 12, "bbl_bed_pei_bottom.svg");
	BedTextureInfo::TexturePart pte_part1(10, 80, 10, 160, "bbl_bed_pte_left.svg");
	BedTextureInfo::TexturePart pte_part2(74, -10, 148, 12, "bbl_bed_pte_bottom.svg");
	for (size_t i = 0; i < btCount; i++) {
		bed_texture_info[i].reset();
		bed_texture_info[i].parts.clear();
	}
	bed_texture_info[btSuperTack].parts.push_back(st_part1);
	bed_texture_info[btSuperTack].parts.push_back(st_part2);
	bed_texture_info[btPC].parts.push_back(pc_part1);
	bed_texture_info[btPC].parts.push_back(pc_part2);
	bed_texture_info[btPCT].parts.push_back(pct_part_left);
	bed_texture_info[btPCT].parts.push_back(pc_part2);
	bed_texture_info[btEP].parts.push_back(ep_part1);
	bed_texture_info[btEP].parts.push_back(ep_part2);
	bed_texture_info[btPEI].parts.push_back(pei_part1);
	bed_texture_info[btPEI].parts.push_back(pei_part2);
	bed_texture_info[btPTE].parts.push_back(pte_part1);
	bed_texture_info[btPTE].parts.push_back(pte_part2);

}

void Bed3DTexture::update_bed_type_info(const std::vector<Pointfs>& shape_group)
{
	Points shapes;
	shapes.reserve(4 * shape_group.size());
	for (auto& shape : shape_group)
	{
		shapes.insert(shapes.end(), shape.begin(), shape.end());
	}
	auto  bed_ext = get_extents(shapes);
	int   bed_width = bed_ext.size()(0);
	int   bed_height = bed_ext.size()(1);
	float base_width = 256;
	float base_height = 256;
	float x_rate = bed_width / base_width;
	float y_rate = bed_height / base_height;
	for (int i = 0; i < btCount; i++) {
		for (int j = 0; j < bed_texture_info[i].parts.size(); j++) {
			bed_texture_info[i].parts[j].x *= x_rate;
			bed_texture_info[i].parts[j].y *= y_rate;
			bed_texture_info[i].parts[j].w *= x_rate;
			bed_texture_info[i].parts[j].h *= y_rate;
			bed_texture_info[i].parts[j].update_buffer();
		}
	}
}

void Bed3DTexture::load_bedtype_textures()
{
	if (is_load_bedtype_textures) return;

	init_bed_type_info();
	GLint max_tex_size = (GLint)get_max_texture_size();
	GLint logo_tex_size = (max_tex_size < 2048) ? max_tex_size : 2048;
	for (int i = 0; i < (unsigned int)btCount; ++i) {
		for (int j = 0; j < bed_texture_info[i].parts.size(); j++) {
			std::string filename = resources_dir() + "/images/" + bed_texture_info[i].parts[j].filename;
			if (boost::filesystem::exists(filename)) {
				Bed3DTexture::bed_texture_info[i].parts[j].texture = new GLTexture();
				if (!Bed3DTexture::bed_texture_info[i].parts[j].texture->load_from_svg_file(filename, true, true, true, logo_tex_size)) {
					BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": load logo texture from %1% failed!") % filename;
				}
			} else {
				BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": load logo texture from %1% failed!") % filename;
			}
		}
	}
	is_load_bedtype_textures = true;
}

void Bed3DTexture::init_cali_texture_info()
{
	BedTextureInfo::TexturePart cali_line(18, 2, 224, 16, "bbl_cali_lines.svg");
	cali_texture_info.parts.push_back(cali_line);

	for (int j = 0; j < cali_texture_info.parts.size(); j++) {
		cali_texture_info.parts[j].update_buffer();
	}
}

void Bed3DTexture::load_cali_textures()
{
	if (is_load_cali_texture) return;

	init_cali_texture_info();
	GLint max_tex_size = (GLint)get_max_texture_size();
	GLint logo_tex_size = (max_tex_size < 2048) ? max_tex_size : 2048;
	for (int i = 0; i < (unsigned int)btCount; ++i) {
		for (int j = 0; j < cali_texture_info.parts.size(); j++) {
			std::string filename = resources_dir() + "/images/" + cali_texture_info.parts[j].filename;
			if (boost::filesystem::exists(filename)) {
				Bed3DTexture::cali_texture_info.parts[j].texture = new GLTexture();
				if (!Bed3DTexture::cali_texture_info.parts[j].texture->load_from_svg_file(filename, true, true, true, logo_tex_size)) {
					BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": load cali texture from %1% failed!") % filename;
				}
			}
			else {
				BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": load cali texture from %1% failed!") % filename;
			}
		}
	}
	is_load_cali_texture = true;
}



};
};
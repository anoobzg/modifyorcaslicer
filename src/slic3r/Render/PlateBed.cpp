#include "PlateBed.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "slic3r/Render/3DBed.hpp"
#include "slic3r/Render/3DBedTexture.hpp"
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Render/GLColors.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Render/SceneRaycaster.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/Theme/Font.hpp"
#include "GLShader.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"

static const float GROUND_Z = -0.03f;
static const float GROUND_Z_GRIDLINE = -0.26f;
static const float GRABBER_X_FACTOR = 0.20f;
static const float GRABBER_Y_FACTOR = 0.03f;
static const float GRABBER_Z_VALUE = 0.5f;
static unsigned int GLOBAL_PLATE_INDEX = 0;

static const double LOGICAL_PART_PLATE_GAP = 1. / 5.;
static const int PARTPLATE_ICON_SIZE = 16;
static const int PARTPLATE_EDIT_PLATE_NAME_ICON_SIZE = 12;
static const int PARTPLATE_ICON_GAP_TOP = 3;
static const int PARTPLATE_ICON_GAP_LEFT = 3;
static const int PARTPLATE_ICON_GAP_Y = 5;
static const int PARTPLATE_TEXT_OFFSET_X1 = 3;
static const int PARTPLATE_TEXT_OFFSET_X2 = 1;
static const int PARTPLATE_TEXT_OFFSET_Y = 1;
static const int PARTPLATE_PLATENAME_OFFSET_Y  = 10;

namespace Slic3r {
namespace GUI {

ColorRGBA PlateBed::SELECT_COLOR		= { 0.2666f, 0.2784f, 0.2784f, 1.0f }; //{ 0.4196f, 0.4235f, 0.4235f, 1.0f };
ColorRGBA PlateBed::UNSELECT_COLOR		= { 0.82f, 0.82f, 0.82f, 1.0f };
ColorRGBA PlateBed::UNSELECT_DARK_COLOR		= { 0.384f, 0.384f, 0.412f, 1.0f };
ColorRGBA PlateBed::DEFAULT_COLOR		= { 0.5f, 0.5f, 0.5f, 1.0f };
ColorRGBA PlateBed::LINE_TOP_COLOR		= { 0.89f, 0.89f, 0.89f, 1.0f };
ColorRGBA PlateBed::LINE_TOP_DARK_COLOR		= { 0.431f, 0.431f, 0.463f, 1.0f };
ColorRGBA PlateBed::LINE_TOP_SEL_COLOR  = { 0.5294f, 0.5451, 0.5333f, 1.0f};
ColorRGBA PlateBed::LINE_TOP_SEL_DARK_COLOR = { 0.298f, 0.298f, 0.3333f, 1.0f};
ColorRGBA PlateBed::LINE_BOTTOM_COLOR	= { 0.8f, 0.8f, 0.8f, 0.4f };
ColorRGBA PlateBed::HEIGHT_LIMIT_TOP_COLOR		= { 0.6f, 0.6f, 1.0f, 1.0f };
ColorRGBA PlateBed::HEIGHT_LIMIT_BOTTOM_COLOR	= { 0.4f, 0.4f, 1.0f, 1.0f };

static Bed3D* inst()
{
    static Bed3D* bed = NULL;
    if (!bed)
        bed = new Bed3D();

    return bed;
}

static Bed3DTexture* texture_inst()
{
    return inst()->m_texture;
}

bool PlateBed::set_shape(const std::vector<Pointfs>& printable_area, const double printable_height, const std::string& custom_model, bool force_as_custom,
    const Vec2d& position, bool with_reset)
{
    Bed3D* bed = inst();
    return bed->set_shape(printable_area, printable_height, custom_model, force_as_custom, position, with_reset);
}

BoundingBoxf PlateBed::bounding_volume2d()
{
    Bed3D* bed = inst();
    return bed->build_volume().bounding_volume2d();
}

const BuildVolume& PlateBed::build_volume()
{
    Bed3D* bed = inst();
    return bed->build_volume();
}

Vec2d PlateBed::get_master_slave_offset()
{
    Bed3D* bed = inst();
    return bed->get_master_slave_offset();
}

const std::vector<BuildVolume>& PlateBed::sub_build_volume()
{
    Bed3D* bed = inst();
    return bed->sub_build_volume();
}

void PlateBed::set_axes_mode(bool origin)
{
    Bed3D* bed = inst();
    bed->set_axes_mode(origin);
}

void PlateBed::on_change_color_mode(bool is_dark)
{
    Bed3D* bed = inst();
    bed->on_change_color_mode(is_dark);
}

BoundingBoxf3 PlateBed::extended_bounding_box()
{
    Bed3D* bed = inst();
    return bed->extended_bounding_box();
}

void PlateBed::set_position(const Vec2d& pos)
{
    Bed3D* bed = inst();
    bed->set_position(pos);
}

const std::string &PlateBed::get_logo_texture_filename()
{
    Bed3DTexture* texture = texture_inst();
    return texture->get_logo_texture_filename();
}

void PlateBed::update_logo_texture_filename(const std::string &texture_filename)
{
    Bed3DTexture* texture = texture_inst();
    texture->update_logo_texture_filename(texture_filename);
}

void PlateBed::generate_icon_textures()
{
    Bed3DTexture* texture = texture_inst();
    texture->generate_icon_textures();
}

void PlateBed::release_icon_textures()
{
    Bed3DTexture* texture = texture_inst();
    texture->release_icon_textures();
}

void PlateBed::set_render_option(bool bedtype_texture, bool plate_settings, bool cali)
{
    Bed3DTexture* texture = texture_inst();
    texture->set_render_option(bedtype_texture, plate_settings, cali);
}

PlateBed::PlateBed() :
    m_bed(inst())
{

}

void PlateBed::set_model(Model* model)
{
	m_model = model;
}

void PlateBed::set_selected(bool selected)
{
    m_selected = selected;
}

void PlateBed::render(const RenderConfig& config)
{
	m_plate_id = config.plate_id;

    if (m_selected)
		m_bed->render(config.view_matrix, config.projection_matrix, config.bottom, true);

	glsafe(::glEnable(GL_DEPTH_TEST));

	GLShaderProgram* shader = get_shader("flat");
	if (shader != nullptr) {
		shader->start_using();
		glsafe(::glEnable(GL_BLEND));
		glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

		shader->set_uniform("view_model_matrix", config.view_matrix);
		shader->set_uniform("projection_matrix", config.projection_matrix);

		render_background(config);

		render_exclude_area(config);

		render_grid(config);

		//render_height_limit(mode);

		glsafe(::glDisable(GL_BLEND));


		shader->stop_using();
	}

	if (!config.bottom && m_selected && !config.force_background_color) {
		render_logo(config);
	}

	render_icons(config);
	render_only_numbers(config);

	glsafe(::glDisable(GL_DEPTH_TEST));
}


static bool init_model_from_lines(GLModel& model, const Lines& lines, float z)
{

	GLModel::Geometry init_data;
	init_data.format = { GLModel::Geometry::EPrimitiveType::Lines, GLModel::Geometry::EVertexLayout::P3 };
	init_data.reserve_vertices(2 * lines.size());
	init_data.reserve_indices(2 * lines.size());

	for (const auto& l : lines) {
		init_data.add_vertex(Vec3f(unscale<float>(l.a.x()), unscale<float>(l.a.y()), z));
		init_data.add_vertex(Vec3f(unscale<float>(l.b.x()), unscale<float>(l.b.y()), z));
		const unsigned int vertices_counter = (unsigned int)init_data.vertices_count();
		init_data.add_line(vertices_counter - 2, vertices_counter - 1);
	}

	model.init_from(std::move(init_data));

	return true;
}

static bool init_model_from_lines(GLModel& model, const Lines3& lines)
{

	GLModel::Geometry init_data;
	init_data.format = { GLModel::Geometry::EPrimitiveType::Lines, GLModel::Geometry::EVertexLayout::P3 };
	init_data.reserve_vertices(2 * lines.size());
	init_data.reserve_indices(2 * lines.size());

	for (const auto& l : lines) {
		init_data.add_vertex(Vec3f(unscale<float>(l.a.x()), unscale<float>(l.a.y()), unscale<float>(l.a.z())));
		init_data.add_vertex(Vec3f(unscale<float>(l.b.x()), unscale<float>(l.b.y()), unscale<float>(l.b.z())));
		const unsigned int vertices_counter = (unsigned int)init_data.vertices_count();
		init_data.add_line(vertices_counter - 2, vertices_counter - 1);
	}

	model.init_from(std::move(init_data));

	return true;
}

static void init_raycaster_from_model(PickingModel& model)
{
	assert(model.mesh_raycaster == nullptr);

	const GLModel::Geometry& geometry = model.model.get_geometry();

	indexed_triangle_set its;
	its.vertices.reserve(geometry.vertices_count());
	for (size_t i = 0; i < geometry.vertices_count(); ++i) {
		its.vertices.emplace_back(geometry.extract_position_3(i));
	}
	its.indices.reserve(geometry.indices_count() / 3);
	for (size_t i = 0; i < geometry.indices_count() / 3; ++i) {
		const size_t tri_id = i * 3;
		its.indices.emplace_back(geometry.extract_index(tri_id), geometry.extract_index(tri_id + 1), geometry.extract_index(tri_id + 2));
	}

	model.mesh_raycaster = std::make_unique<MeshRaycaster>(std::make_shared<const TriangleMesh>(std::move(its)));
}

const std::vector<Pointfs>& PlateBed::get_shape() const
{
	return m_shape_group;
}

bool PlateBed::set_shape(const std::vector<Pointfs>& shape_group, const std::vector<Pointfs>& exclude_areas_group, Vec2d position, float height_to_lid, float height_to_rod)
{
	std::vector<Pointfs> new_shape_group, new_exclude_areas_group;
	m_raw_shape_group = shape_group;
	m_position = position;
	
	for (int i = 0; i < shape_group.size(); ++i)
	{
		auto& shape = shape_group[i];
		auto& exclude_areas = exclude_areas_group[i];
		Pointfs new_shape, new_exclude_areas;
		for (const Vec2d& p : shape) {
			new_shape.push_back(Vec2d(p.x() + position.x(), p.y() + position.y()));
		}

		for (const Vec2d& p : exclude_areas) {
			new_exclude_areas.push_back(Vec2d(p.x() + position.x(), p.y() + position.y()));
		}
		new_shape_group.push_back(new_shape);
		new_exclude_areas_group.push_back(new_exclude_areas);
	}

	if ((m_shape_group == new_shape_group) && (m_exclude_area_group == new_exclude_areas_group)
		&& (m_height_to_lid == height_to_lid) && (m_height_to_rod == height_to_rod)) {
		BOOST_LOG_TRIVIAL(info) << "PartPlate same shape, skip directly";
		return false;
	}

	m_height_to_lid =  height_to_lid;
	m_height_to_rod =  height_to_rod;

	if ((m_shape_group != new_shape_group) || (m_exclude_area_group != new_exclude_areas_group))
	{
	 	m_shape_group = std::move(new_shape_group);
		m_exclude_area_group = std::move(new_exclude_areas_group);

		calc_bounding_boxes();

		ExPolygon logo_poly;
		generate_logo_polygon(logo_poly);
		m_logo_triangles.reset();
		if (!init_model_from_poly(m_logo_triangles, logo_poly, GROUND_Z + 0.02f))
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ":Unable to create logo triangles\n";

		std::vector<ExPolygon> polys;
		generate_print_polygon(polys);
		calc_triangles(polys);
        init_raycaster_from_model(m_triangles);

		std::vector<ExPolygon> exclude_polys;
		generate_exclude_polygon(exclude_polys);
		calc_exclude_triangles(exclude_polys);

	 
		calc_gridlines(polys);

		calc_vertex_for_icons(0, m_del_icon);
        calc_vertex_for_icons(1, m_orient_icon);
        calc_vertex_for_icons(2, m_arrange_icon);
        calc_vertex_for_icons(3, m_lock_icon);
        calc_vertex_for_icons(4, m_plate_settings_icon);
        calc_vertex_for_icons(5, m_move_front_icon);
		calc_vertex_for_number(0, false, m_plate_idx_icon);

		generate_plate_name_texture();
	}

	calc_height_limit();

	return true;
}


static void register_model_for_picking(SceneRaycaster* raycaster, PickingModel &model, int id)
{
	raycaster->add_raycaster(SceneRaycaster::EType::Bed, id, *model.mesh_raycaster, Transform3d::Identity(), false);
}

void PlateBed::register_raycasters_for_picking(SceneRaycaster* raycaster, int plate_id)
{
	m_plate_id = plate_id;
	m_scene_raycaster = raycaster;
	int base_id = m_plate_id * GRABBER_COUNT;

    register_model_for_picking(raycaster, m_triangles, base_id + 0);
    register_model_for_picking(raycaster, m_del_icon, base_id + 1);
    register_model_for_picking(raycaster, m_orient_icon, base_id + 2);
    register_model_for_picking(raycaster, m_arrange_icon, base_id + 3);
    register_model_for_picking(raycaster, m_lock_icon, base_id + 4);
     if (texture_inst()->render_plate_settings)
        register_model_for_picking(raycaster, m_plate_settings_icon, base_id + 5);

    raycaster->remove_raycasters(SceneRaycaster::EType::Bed, base_id + 6);
    register_model_for_picking(raycaster, m_plate_name_edit_icon, base_id + 6);
    register_model_for_picking(raycaster, m_move_front_icon, base_id + 7);
}

void PlateBed::generate_plate_name_texture()
{
   m_plate_name_icon.reset();

	// generate m_name_texture texture from m_name with generate_from_text_string
	m_name_texture.reset();
	auto text = m_name.empty()? _L("Untitled") : from_u8(m_name);
	wxCoord w, h;

	auto* font = &Font::Head_32;

	wxColour foreground(0xf2, 0x75, 0x4e, 0xff);
   if (!m_name_texture.generate_from_text_string(text.ToUTF8().data(), *font, *wxBLACK, foreground))
		BOOST_LOG_TRIVIAL(error) << "PlateBed::generate_plate_name_texture(): generate_from_text_string() failed";
  
	Points shapes;
	shapes.reserve(4 * m_shape_group.size());
	for (auto& shape : m_shape_group)
	{
		shapes.insert(shapes.end(), shape.begin(), shape.end());
	}
	auto bed_ext = get_extents(shapes);

   auto factor = bed_ext.size()(1) / 200.0;
	ExPolygon poly;
	float offset_x = 1 * factor;
   float offset_y = PARTPLATE_TEXT_OFFSET_Y * factor;
   w = int(factor * (m_name_texture.get_width() * 16) / m_name_texture.get_height());
   h = int(factor * 16);
   Point p = bed_ext[3] + Point(0.0, 1.0 + h * m_name_texture.m_original_height / m_name_texture.get_height());
	poly.contour.append({ scale_(p(0) + offset_x)    , scale_(p(1) - h + offset_y) });
	poly.contour.append({ scale_(p(0) + w - offset_x), scale_(p(1) - h + offset_y) });
	poly.contour.append({ scale_(p(0) + w - offset_x), scale_(p(1) - offset_y) });
	poly.contour.append({ scale_(p(0) + offset_x)    , scale_(p(1) - offset_y) });

   if (!init_model_from_poly(m_plate_name_icon, poly, GROUND_Z))
       BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to generate geometry buffers for icons\n";

	if (m_scene_raycaster)
	{
		int base_id = m_plate_id * GRABBER_COUNT;
		m_scene_raycaster->remove_raycasters(SceneRaycaster::EType::Bed, base_id + 6);
		calc_vertex_for_plate_name_edit_icon(&m_name_texture, 0, m_plate_name_edit_icon);
		register_model_for_picking(m_scene_raycaster, m_plate_name_edit_icon, base_id + 6);
	}
}

std::vector<BoundingBoxf3> PlateBed::get_exclude_bounding_box()
{
	return m_exclude_bounding_box;
}

const BoundingBox PlateBed::get_bounding_box_crd()
{
	Pointfs shapes;
	shapes.reserve(m_shape_group.size()*4);
	for (auto &shape: m_shape_group)
	{
		shapes.insert(shapes.end(), shape.begin(), shape.end());
	}

	const auto plate_shape = Slic3r::Polygon::new_scale(shapes);
	return plate_shape.bounding_box();
}

BoundingBoxf3 PlateBed::get_build_volume()
{
	auto  eps=Slic3r::BuildVolume::SceneEpsilon;
	Vec3d         up_point  = m_bounding_box.max + Vec3d(eps, eps, m_origin.z() + m_height + eps);
	Vec3d         low_point = m_bounding_box.min + Vec3d(-eps, -eps, m_origin.z() - eps);
	BoundingBoxf3 plate_box(low_point, up_point);
	return plate_box;
}

bool PlateBed::check_intersects(const BoundingBoxf3& box)
{
	 bool result = false;
	 if (AppAdapter::plater()->is_normal_devide_mode())
	 {
	 	auto  eps = Slic3r::BuildVolume::SceneEpsilon;
	 	Vec3d         up_point  = m_bounding_box.max + Vec3d(eps, eps, m_origin.z() + m_height + eps);
	 	Vec3d         low_point = m_bounding_box.min + Vec3d(-eps, -eps, m_origin.z() - eps);
	 	BoundingBoxf3 plate_box(low_point, up_point);
	 	result = get_build_volume().intersects(box);
	 }
	 else 
	 {
	 	if (m_shape_group.size() == 4)
	 	{
	 		{
	 			Vec2d min_2d = m_shape_group[2][0];
	 			Vec2d max_2d = m_shape_group[2][2];
	 			Vec3d min(min_2d[0], min_2d[1], 0);
	 			Vec3d max(max_2d[0], max_2d[1], m_height);
	 			BoundingBoxf3 left_plate_box(min, max);
	 			if (left_plate_box.intersects(box))
	 				result = true;
	 		}
	 		{
	 			Vec2d min_2d = m_shape_group[3][0];
	 			Vec2d max_2d = m_shape_group[3][2];
	 			Vec3d min(min_2d[0], min_2d[1], 0);
	 			Vec3d max(max_2d[0], max_2d[1], m_height);
	 			BoundingBoxf3 right_plate_box(min, max);
	 			if (right_plate_box.intersects(box))
	 				result = true;
	 		}
	 	}
	 }
	 return result;
}

bool PlateBed::contains(const Vec3d& point) const
{
	return m_bounding_box.contains(point);
}

bool PlateBed::contains(const GLVolume& v) const
{
	return m_bounding_box.contains(v.bounding_box());
}

bool PlateBed::contains(const BoundingBoxf3& bb) const
{
	// Allow the objects to protrude below the print bed
	BoundingBoxf3 print_volume(Vec3d(m_bounding_box.min(0), m_bounding_box.min(1), 0.0), Vec3d(m_bounding_box.max(0), m_bounding_box.max(1), 1e3));
	print_volume.min(2) = -1e10;
	print_volume.min(0) -= Slic3r::BuildVolume::BedEpsilon;
	print_volume.min(1) -= Slic3r::BuildVolume::BedEpsilon;
	print_volume.max(0) += Slic3r::BuildVolume::BedEpsilon;
	print_volume.max(1) += Slic3r::BuildVolume::BedEpsilon;
	return print_volume.contains(bb);
}

bool PlateBed::intersects(const BoundingBoxf3& bb) const
{
	// Allow the objects to protrude below the print bed
	BoundingBoxf3 print_volume(Vec3d(m_bounding_box.min(0), m_bounding_box.min(1), 0.0), Vec3d(m_bounding_box.max(0), m_bounding_box.max(1), 1e3));
	print_volume.min(2) = -1e10;
	print_volume.min(0) -= Slic3r::BuildVolume::BedEpsilon;
	print_volume.min(1) -= Slic3r::BuildVolume::BedEpsilon;
	print_volume.max(0) += Slic3r::BuildVolume::BedEpsilon;
	print_volume.max(1) += Slic3r::BuildVolume::BedEpsilon;
	return print_volume.intersects(bb);
}

bool PlateBed::check_outside(int obj_id, int instance_id, BoundingBoxf3* bounding_box)
{
	bool outside = true;

	if (!m_model)
		return outside;

	ModelObject* object = m_model->objects[obj_id];
	ModelInstance* instance = object->instances[instance_id];

	BoundingBoxf3 instance_box = bounding_box? *bounding_box: object->instance_convex_hull_bounding_box(instance_id);
	Polygon hull = instance->convex_hull_2d();
	BoundingBoxf3 plate_box = get_build_volume();
	if (instance_box.max.z() > plate_box.min.z())
		plate_box.min.z() += instance_box.min.z(); // not considering outsize if sinking

	if (plate_box.contains(instance_box))
	{
		if (m_exclude_bounding_box.size() > 0)
		{
			Polygon hull = instance->convex_hull_2d();
			int index;
			for (index = 0; index < m_exclude_bounding_box.size(); index ++)
			{
				Polygon p = m_exclude_bounding_box[index].polygon(true);  // instance convex hull is scaled, so we need to scale here
				if (intersection({ p }, { hull }).empty() == false)
				//if (m_exclude_bounding_box[index].intersects(instance_box))
				{
					break;
				}
			}
			if (index >= m_exclude_bounding_box.size())
				outside = false;
		}
		else
			outside = false;
	}

	return outside;
}

Vec3d PlateBed::get_center_origin()
{
	Vec3d origin;

	origin(0) = (m_bounding_box.min(0) + m_bounding_box.max(0)) / 2;//m_origin.x() + m_width / 2;
	origin(1) = (m_bounding_box.min(1) + m_bounding_box.max(1)) / 2; //m_origin.y() + m_depth / 2;
	origin(2) = m_origin.z();

	return  origin;
}

const BoundingBoxf3& PlateBed::get_bounding_box(bool extended)
{ 
	return extended ? m_extended_bounding_box : m_bounding_box; 
}

const std::vector<BoundingBoxf3>& PlateBed::get_exclude_areas()
{ 
	return m_exclude_bounding_box; 
}

void PlateBed::generate_print_polygon(std::vector<ExPolygon> &print_polygon)
{
	for (auto& shape : m_shape_group)
	{
		ExPolygon poly;
		auto compute_points = [&poly](Vec2d& center, double radius, double start_angle, double stop_angle, int count)
			{
				double angle_steps;
				angle_steps = (stop_angle - start_angle) / (count - 1);
				for (int j = 0; j < count; j++)
				{
					double angle = start_angle + j * angle_steps;
					double x = center(0) + ::cos(angle) * radius;
					double y = center(1) + ::sin(angle) * radius;
					poly.contour.append({ scale_(x), scale_(y) });
				}
			};

		for (const Vec2d& p : shape) {
			poly.contour.append({ scale_(p(0)), scale_(p(1)) });
		}
		print_polygon.push_back(poly);
	}

}

void PlateBed::generate_exclude_polygon(std::vector<ExPolygon> &exclude_polygon)
{
	
	for (auto& exclude_area : m_exclude_area_group)
	{
		ExPolygon poly;
		auto compute_exclude_points = [&poly](Vec2d& center, double radius, double start_angle, double stop_angle, int count)
			{
				double angle_steps;
				angle_steps = (stop_angle - start_angle) / (count - 1);
				for (int j = 0; j < count; j++)
				{
					double angle = start_angle + j * angle_steps;
					double x = center(0) + ::cos(angle) * radius;
					double y = center(1) + ::sin(angle) * radius;
					poly.contour.append({ scale_(x), scale_(y) });
				}
			};

		int points_count = 8;
		if (exclude_area.size() == 4)
		{
			//rectangle case
			for (int i = 0; i < 4; i++)
			{
				const Vec2d& p = exclude_area[i];
				Vec2d center;
				double start_angle, stop_angle, radius;
				switch (i) {
				case 0:
					radius = 5.f;
					center(0) = p(0) + radius;
					center(1) = p(1) + radius;
					start_angle = PI;
					stop_angle = 1.5 * PI;
					compute_exclude_points(center, radius, start_angle, stop_angle, points_count);
					break;
				case 1:
					poly.contour.append({ scale_(p(0)), scale_(p(1)) });
					break;
				case 2:
					radius = 3.f;
					center(0) = p(0) - radius;
					center(1) = p(1) - radius;
					start_angle = 0;
					stop_angle = 0.5 * PI;
					compute_exclude_points(center, radius, start_angle, stop_angle, points_count);
					break;
				case 3:
					poly.contour.append({ scale_(p(0)), scale_(p(1)) });
					break;
				}
			}
		}
		else {
			for (const Vec2d& p : exclude_area) {
				poly.contour.append({ scale_(p(0)), scale_(p(1)) });
			}
		}

		exclude_polygon.push_back(poly);
	}
}

void PlateBed::generate_logo_polygon(ExPolygon &logo_polygon)
{
	if (m_shape_group.size() == 0)
		return; 
    
	Pointfs shape = m_shape_group.front();
	if (shape.size() == 4)
	{
        bool is_bbl_vendor = false;
        //rectangle case
		for (int i = 0; i < 4; i++)
		{
			const Vec2d& p = shape[i];
			if ((i  == 0) || (i  == 1)) {
                logo_polygon.contour.append({scale_(p(0)), scale_(is_bbl_vendor ? p(1) - 12.f : p(1))});
            }
			else {
				logo_polygon.contour.append({ scale_(p(0)), scale_(p(1)) });
			}
		}
	}
	else {
		for (const Vec2d& p : shape) {
			logo_polygon.contour.append({ scale_(p(0)), scale_(p(1)) });
		}
	}
}


void PlateBed::calc_bounding_boxes() const {
	
	BoundingBoxf3* bounding_box = const_cast<BoundingBoxf3*>(&m_bounding_box);
	*bounding_box = BoundingBoxf3();
	for (auto& shape : m_shape_group)
	{
		for (const Vec2d& p : shape) {
			bounding_box->merge({ p(0), p(1), 0.0 });
		}
	}
 
	BoundingBoxf3* extended_bounding_box = const_cast<BoundingBoxf3*>(&m_extended_bounding_box);
	*extended_bounding_box = m_bounding_box;

	double half_x = bounding_box->size().x() * GRABBER_X_FACTOR;
	double half_y = bounding_box->size().y() * 1.0f * GRABBER_Y_FACTOR;
	double half_z = GRABBER_Z_VALUE;
	Vec3d center(bounding_box->center().x(), bounding_box->min(1) -half_y, GROUND_Z);
	m_grabber_box.min = Vec3d(center.x() - half_x, center.y() - half_y, center.z() - half_z);
	m_grabber_box.max = Vec3d(center.x() + half_x, center.y() + half_y, center.z() + half_z);
	m_grabber_box.defined = true;
	extended_bounding_box->merge(m_grabber_box);

    //calc exclude area bounding box
    m_exclude_bounding_box.clear();
    BoundingBoxf3 exclude_bb;
	for (auto& exclude_area : m_exclude_area_group)
	{
		for (int index = 0; index < exclude_area.size(); index++) {
			const Vec2d& p = exclude_area[index];

			if (index % 4 == 0)
				exclude_bb = BoundingBoxf3();

			exclude_bb.merge({ p(0), p(1), 0.0 });

			if (index % 4 == 3)
			{
				exclude_bb.max(2) = m_depth;
				exclude_bb.min(2) = GROUND_Z;
				m_exclude_bounding_box.emplace_back(exclude_bb);
			}
		}
	}
}


void PlateBed::calc_triangles(const std::vector<ExPolygon> &polys)
{

    m_triangles.reset();

    if (!init_model_from_polys(m_triangles.model, polys, GROUND_Z))
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ":Unable to create plate triangles\n";
}

void PlateBed::calc_exclude_triangles(const std::vector<ExPolygon> &polys)
{

	for (auto& poly : polys)
	{
		GLModel  exclude_triangles;
		//exclude_triangles.reset();
		if (!init_model_from_poly(exclude_triangles, poly, GROUND_Z))
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ":Unable to create exclude triangles\n";


		m_exclude_triangles_group.push_back(exclude_triangles);
	}
 }

void PlateBed::calc_gridlines(const std::vector<ExPolygon>& polys) {

	m_gridlines_group.clear();
	m_gridlines_bolder_group.clear();

	for (auto& poly : polys)
	{
		GLModel gridlines_model;
		GLModel gridlines_bolder_model;
		BoundingBox& pp_bbox = poly.contour.bounding_box();

		Polylines axes_lines, axes_lines_bolder;
		int count = 0;
		int step = 10;
		// Orca: use 500 x 500 bed size as baseline.
		const Point grid_counts = pp_bbox.size() / ((coord_t)scale_(step * 50));
		// if the grid is too dense, we increase the step
		if (grid_counts.minCoeff() > 1) {
			step = static_cast<int>(grid_counts.minCoeff() + 1) * 10;
		}
		for (coord_t x = pp_bbox.min(0); x <= pp_bbox.max(0); x += scale_(step)) {
			Polyline line;
			line.append(Point(x, pp_bbox.min(1)));
			line.append(Point(x, pp_bbox.max(1)));

			if ((count % 5) == 0)
				axes_lines_bolder.push_back(line);
			else
				axes_lines.push_back(line);
			count++;
		}
		count = 0;
		for (coord_t y = pp_bbox.min(1); y <= pp_bbox.max(1); y += scale_(step)) {
			Polyline line;
			line.append(Point(pp_bbox.min(0), y));
			line.append(Point(pp_bbox.max(0), y));
			axes_lines.push_back(line);

			if ((count % 5) == 0)
				axes_lines_bolder.push_back(line);
			else
				axes_lines.push_back(line);
			count++;
		}

		// clip with a slightly grown expolygon because our lines lay on the contours and may get erroneously clipped
		Lines gridlines = to_lines(intersection_pl(axes_lines, offset(poly, (float)SCALED_EPSILON)));
		Lines gridlines_bolder = to_lines(intersection_pl(axes_lines_bolder, offset(poly, (float)SCALED_EPSILON)));

		// append bed contours
		Lines contour_lines = to_lines(poly);
		std::copy(contour_lines.begin(), contour_lines.end(), std::back_inserter(gridlines));

		if (!init_model_from_lines(gridlines_model, gridlines, GROUND_Z_GRIDLINE))
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to create bed grid lines\n";

		if (!init_model_from_lines(gridlines_bolder_model, gridlines_bolder, GROUND_Z_GRIDLINE))
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to create bed grid lines\n";

		m_gridlines_group.push_back(gridlines_model);
		m_gridlines_bolder_group.push_back(gridlines_bolder_model);
	}
}

void PlateBed::calc_height_limit() {
   
	//m_height_limit_common.reset();
 //   m_height_limit_bottom.reset();
 //   m_height_limit_top.reset();

	m_height_limit_common_group.clear();
	m_height_limit_bottom_group.clear();
	m_height_limit_top_group.clear();

	for(auto & shape : m_shape_group)
	{
		GLModel height_limit_common;
		GLModel height_limit_bottom;
		GLModel height_limit_top;

		Lines3 bottom_h_lines, top_lines, top_h_lines, common_lines;
		int shape_count = shape.size();
		float first_z = 0.02f;
		for (int i = 0; i < shape_count; i++) {
			auto& cur_p = shape[i];
			Vec3crd p1(scale_(cur_p.x()), scale_(cur_p.y()), scale_(first_z));
			Vec3crd p2(scale_(cur_p.x()), scale_(cur_p.y()), scale_(m_height_to_rod));
			Vec3crd p3(scale_(cur_p.x()), scale_(cur_p.y()), scale_(m_height_to_lid));

			common_lines.emplace_back(p1, p2);
			top_lines.emplace_back(p2, p3);

			Vec2d next_p;
			if (i < (shape_count - 1)) {
				next_p = shape[i + 1];

			}
			else {
				next_p = shape[0];
			}
			Vec3crd p4(scale_(cur_p.x()), scale_(cur_p.y()), scale_(m_height_to_rod));
			Vec3crd p5(scale_(next_p.x()), scale_(next_p.y()), scale_(m_height_to_rod));
			bottom_h_lines.emplace_back(p4, p5);

			Vec3crd p6(scale_(cur_p.x()), scale_(cur_p.y()), scale_(m_height_to_lid));
			Vec3crd p7(scale_(next_p.x()), scale_(next_p.y()), scale_(m_height_to_lid));
			top_h_lines.emplace_back(p6, p7);
		}
		//std::copy(bottom_lines.begin(), bottom_lines.end(), std::back_inserter(bottom_h_lines));
		std::copy(top_lines.begin(), top_lines.end(), std::back_inserter(top_h_lines));

		if (!init_model_from_lines(height_limit_common, common_lines))
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to create height limit bottom lines\n";

		if (!init_model_from_lines(height_limit_bottom, bottom_h_lines))
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to create height limit bottom lines\n";

		if (!init_model_from_lines(height_limit_top, top_h_lines))
			BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to create height limit top lines\n";
	   
		m_height_limit_common_group.push_back(height_limit_common);
		m_height_limit_common_group.push_back(height_limit_bottom);
		m_height_limit_common_group.push_back(height_limit_top);
	}
}

void PlateBed::calc_vertex_for_number(int index, bool one_number, GLModel &buffer)
{
    buffer.reset();

	ExPolygon poly;

	Points shapes;
	shapes.reserve(4 * m_shape_group.size());
	for (auto& shape : m_shape_group)
	{
		shapes.insert(shapes.end(), shape.begin(), shape.end());
	}
	auto  bed_ext = get_extents(shapes);
    auto p        = bed_ext[1];
    float factor   = bed_ext.size()(1) / 200.0;
    float size     = PARTPLATE_ICON_SIZE     * factor;
    float offset_y = PARTPLATE_TEXT_OFFSET_Y * factor;
    float offset_x = (one_number?PARTPLATE_TEXT_OFFSET_X1: PARTPLATE_TEXT_OFFSET_X2) * factor;
    float gap_left = PARTPLATE_ICON_GAP_LEFT * factor;
    p += Point(gap_left,0.0f);

    poly.contour.append({ scale_(p(0) + offset_x)       , scale_(p(1) + offset_y) });
    poly.contour.append({ scale_(p(0) + size - offset_x), scale_(p(1) + offset_y) });
    poly.contour.append({ scale_(p(0) + size - offset_x), scale_(p(1) + size - offset_y) });
    poly.contour.append({ scale_(p(0) + offset_x)       , scale_(p(1) + size - offset_y) });

    if (!init_model_from_poly(buffer, poly, GROUND_Z))
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to generate geometry buffers for icons\n";
}

void PlateBed::calc_vertex_for_plate_name_edit_icon(GLTexture *texture, int index, PickingModel &model) {
    model.reset();

	Points shapes;
	shapes.reserve(4 * m_shape_group.size());
	for (auto& shape : m_shape_group)
	{
		shapes.insert(shapes.end(), shape.begin(), shape.end());
	}
	auto bed_ext = get_extents(shapes);

	ExPolygon poly;
	auto p = bed_ext[3];
    auto  factor   = bed_ext.size()(1) / 200.0;
    float width    = 0.f;
    float height   = PARTPLATE_EDIT_PLATE_NAME_ICON_SIZE * factor;
    float offset_x = 1 * factor;
    float offset_y = PARTPLATE_TEXT_OFFSET_Y * factor;

    if (texture && texture->get_width() > 0 && texture->get_height())
        width = int(factor * (texture->get_original_width() * 16) / texture->get_height());

    p += Point(width + offset_x, offset_y + height);

    poly.contour.append({ scale_(p(0))         , scale_(p(1) - height) });
    poly.contour.append({ scale_(p(0) + height), scale_(p(1) - height) });
    poly.contour.append({ scale_(p(0) + height), scale_(p(1)) });
    poly.contour.append({ scale_(p(0))         , scale_(p(1)) });

    if (!init_model_from_poly(model.model, poly, GROUND_Z))
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to generate geometry buffers for icons\n";

    init_raycaster_from_model(model);
}

void PlateBed::calc_vertex_for_icons(int index, PickingModel &model)
{
    model.reset();
	Pointfs shapes;
	shapes.reserve(4 * m_shape_group.size());
	for (auto& shape : m_shape_group)
	{
		shapes.insert(shapes.end(), shape.begin(), shape.end());
	}
	auto  bed_ext = get_extents(shapes);
	ExPolygon poly;
    Vec2d p        = bed_ext[2];
    auto  factor   = bed_ext.size()(1) / 200.0;
    float size     = PARTPLATE_ICON_SIZE     * factor;
    float gap_left = PARTPLATE_ICON_GAP_LEFT * factor;
    float gap_y    = PARTPLATE_ICON_GAP_Y    * factor;
    float gap_top  = PARTPLATE_ICON_GAP_TOP  * factor;
    p += Vec2d(gap_left,-1 * (index * (size + gap_y) + gap_top));

    if (m_bed->get_build_volume_type() == BuildVolume_Type::Circle)
        p[1] -= std::max(0.0, (bed_ext.size()(1) - 5 * size - 4 * gap_y - gap_top) / 2);

    poly.contour.append({ scale_(p(0))       , scale_(p(1) - size) });
    poly.contour.append({ scale_(p(0) + size), scale_(p(1) - size) });
    poly.contour.append({ scale_(p(0) + size), scale_(p(1)) });
    poly.contour.append({ scale_(p(0))       , scale_(p(1)) });

	if (!init_model_from_poly(model.model, poly, GROUND_Z))
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "Unable to generate geometry buffers for icons\n";

	init_raycaster_from_model(model);
}

/* render */
void PlateBed::render_background(const RenderConfig& config)
{
	//return directly for current plate
	if (m_selected && !config.force_background_color) return;

	// draw background
	glsafe(::glDepthMask(GL_FALSE));

	ColorRGBA color;
	if (!config.force_background_color) {
		if (m_selected) {
            color = PlateBed::SELECT_COLOR;
		}
		else {
            //color = m_partplate_list->m_is_dark ? PlateBed::UNSELECT_DARK_COLOR : PlateBed::UNSELECT_COLOR;
			color = false ? PlateBed::UNSELECT_DARK_COLOR : PlateBed::UNSELECT_COLOR;
		}
	}
	else {
        color = PlateBed::DEFAULT_COLOR;
	}
    m_triangles.model.set_color(color);
    m_triangles.model.render();
	glsafe(::glDepthMask(GL_TRUE));
}

void PlateBed::render_logo(const RenderConfig& config)
{
	Bed3DTexture* texture = texture_inst();
	if (!texture->render_bedtype_logo) {
		// render third-party printer texture logo
		if (texture->m_logo_texture_filename.empty()) {
			texture->m_logo_texture.reset();
			return;
		}

		if (texture->m_logo_texture.get_id() == 0 || texture->m_logo_texture.get_source() != texture->m_logo_texture_filename) {
			texture->m_logo_texture.reset();

			if (boost::algorithm::iends_with(texture->m_logo_texture_filename, ".svg")) {
				// starts generating the main texture, compression will run asynchronously
				GLint max_tex_size = (GLint)get_max_texture_size();
				GLint logo_tex_size = (max_tex_size < 2048) ? max_tex_size : 2048;
				if (!texture->m_logo_texture.load_from_svg_file(texture->m_logo_texture_filename, true, true, true, logo_tex_size)) {
					return;
				}
			}
			else if (boost::algorithm::iends_with(texture->m_logo_texture_filename, ".png")) {
				// starts generating the main texture, compression will run asynchronously
				if (!texture->m_logo_texture.load_from_file(texture->m_logo_texture_filename, true, GLTexture::MultiThreaded, true)) {
					return;
				}
			}
			else {
				BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": can not load logo texture from %1%, unsupported format") % texture->m_logo_texture_filename;
				return; 
			}
		}
		else if (texture->m_logo_texture.unsent_compressed_data_available()) {
			// sends to gpu the already available compressed levels of the main texture
			texture->m_logo_texture.send_compressed_data_to_gpu();
		}

		if (m_logo_triangles.is_initialized())
			render_logo_texture(config, texture->m_logo_texture, m_logo_triangles);
		return;
	}

	texture->load_bedtype_textures();
	texture->load_cali_textures();

	// btDefault should be skipped
	auto curr_bed_type = config.bed_type;
	if (curr_bed_type == btDefault) {
       //DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
       //if (proj_cfg.has(std::string("curr_bed_type")))
       //    curr_bed_type = proj_cfg.opt_enum<BedType>(std::string("curr_bed_type"));
	}
	int bed_type_idx = (int)curr_bed_type;
	// render bed textures
	for (auto &part : texture->bed_texture_info[bed_type_idx].parts) {
		if (part.texture) {
			if (part.buffer && part.buffer->is_initialized()
				//&& part.vbo_id != 0
				) {
				if (part.offset.x() != m_origin.x() || part.offset.y() != m_origin.y()) {
					part.offset = Vec2d(m_origin.x(), m_origin.y());
					part.update_buffer();
				}
				render_logo_texture(config, *(part.texture), *(part.buffer));
			}
		}
	}

	// render cali texture
	if (config.render_cali) {
		for (auto& part : texture->cali_texture_info.parts) {
			if (part.texture) {
               if (part.buffer && part.buffer->is_initialized()) {
					if (part.offset.x() != m_origin.x() || part.offset.y() != m_origin.y()) {
						part.offset = Vec2d(m_origin.x(), m_origin.y());
						part.update_buffer();
					}
					render_logo_texture(config, *(part.texture), *(part.buffer));
				}
			}
		}
	}
}

void PlateBed::render_exclude_area(const RenderConfig& config)
{
	if (config.force_background_color) //for thumbnail case
		return;

	for (auto& exclude_triangles : m_exclude_triangles_group)
	{
		ColorRGBA select_color{ 0.765f, 0.7686f, 0.7686f, 1.0f };
		ColorRGBA unselect_color{ 0.9f, 0.9f, 0.9f, 1.0f };
		//ColorRGBA default_color{ 0.9f, 0.9f, 0.9f, 1.0f };

		// draw exclude area
		glsafe(::glDepthMask(GL_FALSE));

		if (m_selected) {
			glsafe(::glColor4fv(select_color.data()));
		}
		else {
			glsafe(::glColor4fv(unselect_color.data()));
		}

		exclude_triangles.set_color(m_selected ? select_color : unselect_color);
		exclude_triangles.render();
		glsafe(::glDepthMask(GL_TRUE));
	}

}

void PlateBed::render_grid(const RenderConfig& config)
{
	if (!config.show_grid)
		return;
	//glsafe(::glEnable(GL_MULTISAMPLE));
	// draw grid
	for (int i = 0; i < m_gridlines_group.size(); ++i)
	{
		auto& m_gridlines = m_gridlines_group.at(i);
		auto& m_gridlines_bolder = m_gridlines_bolder_group .at(i);

		glsafe(::glLineWidth(1.0f * config.scale_factor));

		ColorRGBA color;
		if (config.bottom)
			color = LINE_BOTTOM_COLOR;
		else {
			//if (m_selected)
			//	color = m_partplate_list->m_is_dark ? LINE_TOP_SEL_DARK_COLOR : LINE_TOP_SEL_COLOR;
			//else
			//	color = m_partplate_list->m_is_dark ? LINE_TOP_DARK_COLOR : LINE_TOP_COLOR;
			if (m_selected)
				color = config.is_dark ? LINE_TOP_SEL_DARK_COLOR : LINE_TOP_SEL_COLOR;
			else
				color = config.is_dark ? LINE_TOP_DARK_COLOR : LINE_TOP_COLOR;
		}
		m_gridlines.set_color(color);
		m_gridlines.render();

		glsafe(::glLineWidth(2.0f * config.scale_factor));
		m_gridlines_bolder.set_color(color);
		m_gridlines_bolder.render();
	}

}

void PlateBed::render_height_limit(const RenderConfig& config)
{
	//if (m_print && m_print->config().print_sequence == PrintSequence::ByObject && mode != HEIGHT_LIMIT_NONE)
	//{
	//	
	//	for (int i = 0; i < m_height_limit_common_group.size(); ++i)
	//	{
	//		auto& height_limit_common = m_height_limit_common_group[i];
	//		auto& height_limit_top = m_height_limit_top_group[i];
	//		auto& height_limit_bottom = m_height_limit_bottom_group[i];


	//		// draw lower limit
	//		glsafe(::glLineWidth(3.0f * config.scale_factor));
	//		height_limit_common.set_color(HEIGHT_LIMIT_BOTTOM_COLOR);
	//		height_limit_common.render();

	//		if ((mode == HEIGHT_LIMIT_BOTTOM) || (mode == HEIGHT_LIMIT_BOTH)) {
	//			glsafe(::glLineWidth(3.0f * config.scale_factor));
	//			height_limit_bottom.set_color(HEIGHT_LIMIT_BOTTOM_COLOR);
	//			height_limit_bottom.render();
	//		}

	//		// draw upper limit
	//		if ((mode == HEIGHT_LIMIT_TOP) || (mode == HEIGHT_LIMIT_BOTH)) {
	//			glsafe(::glLineWidth(3.0f * config.scale_factor));
	//			height_limit_top.set_color(HEIGHT_LIMIT_TOP_COLOR);
	//			height_limit_top.render();
	//		}
	//	}
	//}

}


void PlateBed::render_icons(const RenderConfig& config)
{
	Bed3DTexture* texture = texture_inst();

	GLShaderProgram* shader = get_shader("printbed");
	if (shader != nullptr) {
		shader->start_using();
		shader->set_uniform("view_model_matrix", config.view_matrix);
		shader->set_uniform("projection_matrix", config.projection_matrix);
		shader->set_uniform("transparent_background", config.bottom);
		//shader->set_uniform("svg_source", boost::algorithm::iends_with(texture->m_del_texture.get_source(), ".svg"));
		shader->set_uniform("svg_source", 0);

      //if (bottom)
      //    glsafe(::glFrontFace(GL_CW));
      glsafe(::glDepthMask(GL_FALSE));

      glsafe(::glEnable(GL_BLEND));
      glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

      if (!config.only_body) {
          if (config.hover_id == 1) {
              render_icon_texture(m_del_icon.model, texture->m_del_hovered_texture);
              show_tooltip(_u8L("Remove current plate (if not last one)"), config.scale);
          }
          else
              render_icon_texture(m_del_icon.model, texture->m_del_texture);

          if (config.hover_id == 2) {
              render_icon_texture(m_orient_icon.model, texture->m_orient_hovered_texture);
              show_tooltip(_u8L("Auto orient objects on current plate"), config.scale);
          }
          else
              render_icon_texture(m_orient_icon.model, texture->m_orient_texture);

          if (config.hover_id == 3) {
              render_icon_texture(m_arrange_icon.model, texture->m_arrange_hovered_texture);
              show_tooltip(_u8L("Arrange objects on current plate"), config.scale);
          }
          else
              render_icon_texture(m_arrange_icon.model, texture->m_arrange_texture);

          if (config.hover_id == 4) {
              if (config.is_locked) {
                  render_icon_texture(m_lock_icon.model,
                                      texture->m_locked_hovered_texture);
                  show_tooltip(_u8L("Unlock current plate"), config.scale);
              }
              else {
                  render_icon_texture(m_lock_icon.model,
                                      texture->m_lockopen_hovered_texture);
                  show_tooltip(_u8L("Lock current plate"), config.scale);
              }
          } else {
              if (config.is_locked)
                  render_icon_texture(m_lock_icon.model, texture->m_locked_texture);
              else
                  render_icon_texture(m_lock_icon.model, texture->m_lockopen_texture);
          }

			if (config.hover_id == 6) {
              render_icon_texture(m_plate_name_edit_icon.model, texture->m_plate_name_edit_hovered_texture);
              show_tooltip(_u8L("Edit current plate name"), config.scale);
			}
			else
              render_icon_texture(m_plate_name_edit_icon.model, texture->m_plate_name_edit_texture);

			if (config.hover_id == 7) {
              render_icon_texture(m_move_front_icon.model, texture->m_move_front_hovered_texture);
              show_tooltip(_u8L("Move plate to the front"), config.scale);
          } else
              render_icon_texture(m_move_front_icon.model, texture->m_move_front_texture);


		if (texture->render_plate_settings) {
             if (config.hover_id == 5) {
                 if (!config.has_plate_settings)
                     render_icon_texture(m_plate_settings_icon.model, texture->m_plate_settings_hovered_texture);
                 else
                     render_icon_texture(m_plate_settings_icon.model, texture->m_plate_settings_changed_hovered_texture);

                 show_tooltip(_u8L("Customize current plate"), config.scale);
             } else {
                 if (!config.has_plate_settings)
                     render_icon_texture(m_plate_settings_icon.model, texture->m_plate_settings_texture);
                 else
                     render_icon_texture(m_plate_settings_icon.model, texture->m_plate_settings_changed_texture);
             }
          }

          if (config.plate_id >= 0 && config.plate_id < config.max_count) {
             render_icon_texture(m_plate_idx_icon, texture->m_idx_textures[config.plate_id]);
          }
      }
		// render_plate_name_texture();

      glsafe(::glDisable(GL_BLEND));


      glsafe(::glDepthMask(GL_TRUE));
      shader->stop_using();
   }
}

void PlateBed::render_only_numbers(const RenderConfig& config)
{
	if (config.force_background_color)
		return;

	Bed3DTexture* texture = texture_inst();
	GLShaderProgram* shader = get_shader("printbed");
	if (shader != nullptr) {
		shader->start_using();
       shader->set_uniform("view_model_matrix", config.view_matrix);
       shader->set_uniform("projection_matrix", config.projection_matrix);
		shader->set_uniform("transparent_background", config.bottom);
		//shader->set_uniform("svg_source", boost::algorithm::iends_with(m_partplate_list->m_del_texture.get_source(), ".svg"));
		shader->set_uniform("svg_source", 0);

       //if (bottom)
       //    glsafe(::glFrontFace(GL_CW));
       glsafe(::glDepthMask(GL_FALSE));

       glsafe(::glEnable(GL_BLEND));
       glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

       if (config.plate_id >= 0 && config.plate_id < config.max_count) {
         render_icon_texture(m_plate_idx_icon, texture->m_idx_textures[config.plate_id]);
       }

       glsafe(::glDisable(GL_BLEND));

       glsafe(::glDepthMask(GL_TRUE));
       shader->stop_using();
   }
}

void PlateBed::render_icon_texture(GLModel& buffer, GLTexture &texture)
{
	GLuint tex_id = (GLuint)texture.get_id();
	glsafe(::glBindTexture(GL_TEXTURE_2D, tex_id));
    buffer.render();
	glsafe(::glBindTexture(GL_TEXTURE_2D, 0));
}

void PlateBed::show_tooltip(const std::string tooltip, float scale)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6 * scale, 3 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, {3 * scale});
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGuiWrapper::COL_WINDOW_BACKGROUND);
    ImGui::PushStyleColor(ImGuiCol_Border, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(tooltip.c_str());
    ImGui::EndTooltip();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

void PlateBed::render_logo_texture(const RenderConfig& config, GLTexture &logo_texture, GLModel& logo_buffer)
{
	//check valid
	if (logo_texture.unsent_compressed_data_available()) {
		// sends to gpu the already available compressed levels of the main texture
		logo_texture.send_compressed_data_to_gpu();
	}

	if (logo_buffer.is_initialized()) {
		GLShaderProgram* shader = get_shader("printbed");
		if (shader != nullptr) {
			shader->start_using();
            shader->set_uniform("view_model_matrix", config.view_matrix);
            shader->set_uniform("projection_matrix", config.projection_matrix);
			shader->set_uniform("transparent_background", 0);
			shader->set_uniform("svg_source", 0);

			//glsafe(::glEnable(GL_DEPTH_TEST));
			glsafe(::glDepthMask(GL_FALSE));

			glsafe(::glEnable(GL_BLEND));
            glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

			if (config.bottom)
				glsafe(::glFrontFace(GL_CW));

			// show the temporary texture while no compressed data is available
			GLuint tex_id = (GLuint)logo_texture.get_id();

			glsafe(::glBindTexture(GL_TEXTURE_2D, tex_id));
            logo_buffer.render();
			glsafe(::glBindTexture(GL_TEXTURE_2D, 0));

			if (config.bottom)
				glsafe(::glFrontFace(GL_CCW));

            glsafe(::glDisable(GL_BLEND));

			glsafe(::glDepthMask(GL_TRUE));

			shader->stop_using();
		}
	}
}

};
};
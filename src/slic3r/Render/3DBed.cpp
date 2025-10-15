#include "3DBed.hpp"

#include "libslic3r/Tesselate.hpp"
#include "libslic3r/Geometry.hpp"
#include "libslic3r/PresetBundle.hpp"

#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Render/GLShader.hpp"
#include "slic3r/Render/GLColors.hpp"
#include "slic3r/Render/3DBedTexture.hpp"
#include "slic3r/Scene/Camera.hpp"

#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"

static const float GROUND_Z = -0.04f;
static const Slic3r::ColorRGBA DEFAULT_MODEL_COLOR             = { 0.3255f, 0.337f, 0.337f, 1.0f };
static const Slic3r::ColorRGBA DEFAULT_MODEL_COLOR_DARK        = { 0.255f, 0.255f, 0.283f, 1.0f };
static const Slic3r::ColorRGBA DEFAULT_SOLID_GRID_COLOR        = { 0.9f, 0.9f, 0.9f, 1.0f };
static const Slic3r::ColorRGBA DEFAULT_TRANSPARENT_GRID_COLOR  = { 0.9f, 0.9f, 0.9f, 0.6f };

namespace Slic3r {
namespace GUI {

bool init_model_from_poly(GLModel &model, const ExPolygon &poly, float z)
{
    if (poly.empty())
        return false;

    std::vector<ExPolygon> polys;
    polys.push_back(poly);
    return init_model_from_polys(model, polys, z);
}

bool init_model_from_polys(GLModel& model, const std::vector<ExPolygon>&polys, float z)
{
     
    bool bRet = false;

    GLModel::Geometry init_data;
    init_data.format = { GLModel::Geometry::EPrimitiveType::Triangles, GLModel::Geometry::EVertexLayout::P3T2 };
    int indics = 0;
    for (auto& poly : polys)
    {
        if (poly.empty())
            continue;

        const std::vector<Vec2f> triangles = triangulate_expolygon_2f(poly, NORMALS_UP);
        if (triangles.empty() || triangles.size() % 3 != 0)
            continue;

        init_data.reserve_vertices(init_data.vertices_count()+triangles.size());
        init_data.reserve_indices(init_data.vertices_count()/3 + triangles.size() / 3);
        indics = init_data.indices_count();
        Vec2f min = triangles.front();
        Vec2f max = min;

        for (const Vec2f& v : triangles) {
            min = min.cwiseMin(v).eval();
            max = max.cwiseMax(v).eval();
        }
     
        const Vec2f size = max - min;
        if (size.x() <= 0.0f || size.y() <= 0.0f)
            continue;

        Vec2f inv_size = size.cwiseInverse();
        inv_size.y() *= -1.0f;

        // vertices + indices
        unsigned int vertices_counter = 0;
        for (const Vec2f& v : triangles) {
            const Vec3f p = { v.x(), v.y(), z };
            init_data.add_vertex(p, (Vec2f)(v - min).cwiseProduct(inv_size).eval());
            ++vertices_counter;
            if (vertices_counter % 3 == 0)
                init_data.add_triangle(indics + vertices_counter - 3, indics + vertices_counter - 2, indics + vertices_counter - 1);
        }

        bRet = true;
    }
    model.init_from(std::move(init_data));

    return bRet;
}

const float Bed3D::Axes::DefaultStemRadius = 0.5f;
const float Bed3D::Axes::DefaultStemLength = 25.0f;
const float Bed3D::Axes::DefaultTipRadius = 2.5f * Bed3D::Axes::DefaultStemRadius;
const float Bed3D::Axes::DefaultTipLength = 5.0f;

ColorRGBA Bed3D::AXIS_X_COLOR = ColorRGBA::X();
ColorRGBA Bed3D::AXIS_Y_COLOR = ColorRGBA::Y();
ColorRGBA Bed3D::AXIS_Z_COLOR = ColorRGBA::Z();

void bed3d_update_render_colors()
{
    Bed3D::AXIS_X_COLOR = ImGuiWrapper::from_ImVec4(RenderColor::colors[RenderCol_Axis_X]);
    Bed3D::AXIS_Y_COLOR = ImGuiWrapper::from_ImVec4(RenderColor::colors[RenderCol_Axis_Y]);
    Bed3D::AXIS_Z_COLOR = ImGuiWrapper::from_ImVec4(RenderColor::colors[RenderCol_Axis_Z]);
}

void bed3d_load_render_colors()
{
    RenderColor::colors[RenderCol_Axis_X] = ImGuiWrapper::to_ImVec4(Bed3D::AXIS_X_COLOR);
    RenderColor::colors[RenderCol_Axis_Y] = ImGuiWrapper::to_ImVec4(Bed3D::AXIS_Y_COLOR);
    RenderColor::colors[RenderCol_Axis_Z] = ImGuiWrapper::to_ImVec4(Bed3D::AXIS_Z_COLOR);
}

void Bed3D::Axes::render()
{
    auto render_axis = [this](GLShaderProgram* shader, const Transform3d& transform) {
        const Camera& camera = AppAdapter::plater()->get_camera();
        const Transform3d& view_matrix = camera.get_view_matrix();
        shader->set_uniform("view_model_matrix", view_matrix * transform);
        shader->set_uniform("projection_matrix", camera.get_projection_matrix());
        const Matrix3d view_normal_matrix = view_matrix.matrix().block(0, 0, 3, 3) * transform.matrix().block(0, 0, 3, 3).inverse().transpose();
        shader->set_uniform("view_normal_matrix", view_normal_matrix);
        m_arrow.render();
    };

    if (!m_arrow.is_initialized())
        m_arrow.init_from(stilized_arrow(16, DefaultTipRadius, DefaultTipLength, DefaultStemRadius, m_stem_length));

    GLShaderProgram* shader = get_shader("gouraud_light");
    if (shader == nullptr)
        return;

    glsafe(::glEnable(GL_DEPTH_TEST));

    shader->start_using();
    shader->set_uniform("emission_factor", 0.0f);

    // x axis
    m_arrow.set_color(AXIS_X_COLOR);
    render_axis(shader, Geometry::assemble_transform(m_origin, { 0.0, 0.5 * M_PI, 0.0 }));

    // y axis
    m_arrow.set_color(AXIS_Y_COLOR);
    render_axis(shader, Geometry::assemble_transform(m_origin, { -0.5 * M_PI, 0.0, 0.0 }));

    // z axis
    m_arrow.set_color(AXIS_Z_COLOR);
    render_axis(shader, Geometry::assemble_transform(m_origin));

    shader->stop_using();

    glsafe(::glDisable(GL_DEPTH_TEST));
}

Bed3D::Bed3D()
{
    m_texture = new Bed3DTexture;
}

Bed3D::~Bed3D()
{
    delete m_texture;
}

bool Bed3D::set_shape(const std::vector<Pointfs>& printable_area, const double printable_height, const std::string& custom_model, bool force_as_custom,
    const Vec2d& position, bool with_reset)
{
    Pointfs area;
    area.resize(printable_area.size());

    for (size_t i = 0; i < printable_area.size(); i++)
    {
        auto& shape = printable_area.at(i);
        area[i] = shape.at(i);
    }

    auto check_model = [](const std::string& model) {
        boost::system::error_code ec;
        return !model.empty() && boost::algorithm::iends_with(model, ".stl") && boost::filesystem::exists(model, ec);
        };

    Type type;
    std::string model;
    std::string texture;
    if (force_as_custom)
        type = Type::Custom;
    else {
        auto [new_type, system_model, system_texture] = detect_type(area);
        type = new_type;
        model = system_model;
        texture = system_texture;
    }

    std::string model_filename = custom_model.empty() ? model : custom_model;
    if (!model_filename.empty() && !check_model(model_filename)) {
        BOOST_LOG_TRIVIAL(error) << "Unable to load bed model: " << model_filename;
        model_filename.clear();
    }

    //BBS: add position related logic
    if (m_bed_shape_group == printable_area && m_build_volume.printable_height() == printable_height && m_type == type && m_model_filename == model_filename && position == m_position)
        // No change, no need to update the UI.
        return false;

    //BBS: add part plate logic, apply position to bed shape
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":current position {%1%,%2%}, new position {%3%, %4%}") % m_position.x() % m_position.y() % position.x() % position.y();
    m_position = position;
    m_bed_shape_group = printable_area;

    if ((position(0) != 0) || (position(1) != 0)) {
        m_sub_build_volumes.clear();
        for (auto& m_bed_shape : m_bed_shape_group)
        {
            Pointfs new_bed_shape;
            for (const Vec2d& p : m_bed_shape) {
                Vec2d point(p(0) + m_position.x(), p(1) + m_position.y());
                new_bed_shape.push_back(point);
            }
            //new_bed_shape_group.push_back(bed_new_shape);
            BuildVolume sub_build_volume = BuildVolume{ {new_bed_shape}, printable_height };
            m_sub_build_volumes.push_back(std::move(sub_build_volume));
        }
    }
    else
    {
        if (!printable_area.empty())
            m_build_volume = BuildVolume{ printable_area, printable_height };
        m_sub_build_volumes.clear();
        for (auto& m_bed_shape : printable_area)
        {
            BuildVolume sub_build_volume = BuildVolume{ {m_bed_shape}, printable_height };
            m_sub_build_volumes.push_back(std::move(sub_build_volume));
        }
    }
       

    m_type = type;
    //m_texture_filename = texture_filename;
    m_model_filename = model_filename;
    //BBS: add part plate logic
    m_extended_bounding_box = this->calc_extended_bounding_box(false);

    //BBS: add part plate logic

    //BBS add default bed
    m_triangle_group.resize(printable_area.size());
    m_model_offset_group.resize(printable_area.size());
    for (auto & m_triangle : m_triangle_group)
    {
        m_triangle.reset();
    }

    if (with_reset) {
        //m_texture.reset(); 
         m_model.reset();
    
    }
    //BBS: add part plate logic, always update model offset
    //else {
    update_model_offset();
    //}

    // Set the origin and size for rendering the coordinate system axes.
    m_axes.set_origin({ 0.0, 0.0, static_cast<double>(GROUND_Z) });
    m_axes.set_stem_length(0.1f * static_cast<float>(m_build_volume.bounding_volume().max_size()));

    // unregister from picking
    // BBS: remove the bed picking logic
    // AppAdapter::plater()->canvas3D()->remove_raycasters_for_picking(SceneRaycaster::EType::Bed);

    // Let the calee to update the UI.
    return true;
}

//BBS: add api to set position for partplate related bed
void Bed3D::set_position(const Vec2d& position)
{
    set_shape(m_bed_shape_group, m_build_volume.printable_height(), m_model_filename, false, position, false);
}

void Bed3D::set_axes_mode(bool origin)
{
    if (origin) {
        m_axes.set_origin({ 0.0, 0.0, static_cast<double>(GROUND_Z) });
    }
    else {
        m_axes.set_origin({ m_position.x(), m_position.y(), static_cast<double>(GROUND_Z) });
    }
}

Vec2d Bed3D::get_master_slave_offset()
{
    if (m_sub_build_volumes.size() < 4)
        return Vec2d(0, 0);

    Vec2d offset;
    Vec2d center1 = m_sub_build_volumes[0].bed_center();
    Vec2d center4 = m_sub_build_volumes[3].bed_center();
    offset = center1 - center4;;
    return offset;
}

void Bed3D::on_change_color_mode(bool is_dark)
{
    m_is_dark = is_dark;
}

void Bed3D::render(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool show_axes)
{
    render_internal(view_matrix, projection_matrix, bottom, show_axes);
}

void Bed3D::render_internal(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool show_axes)
{
    for (auto &model : m_bed_shape_group)
    {
        if (show_axes)
            render_axes();

        glsafe(::glEnable(GL_DEPTH_TEST));

        m_model.set_color(m_is_dark ? DEFAULT_MODEL_COLOR_DARK : DEFAULT_MODEL_COLOR);

        switch (m_type)
        {
        case Type::System: { render_system(view_matrix, projection_matrix, bottom); break; }
        default:
        case Type::Custom: { render_custom(view_matrix, projection_matrix, bottom); break; }
        }

        glsafe(::glDisable(GL_DEPTH_TEST));
    }
}

//BBS: add partplate related logic
//BBS: add partplate related logic
// Calculate an extended bounding box from axes and current model for visualization purposes.
BoundingBoxf3 Bed3D::calc_extended_bounding_box(bool consider_model_offset) const
{
    BoundingBoxf3 out { m_build_volume.bounding_volume() };

    
    const Vec3d size = out.size();
    // ensures that the bounding box is set as defined or the following calls to merge() will not work as intented
    if (size.x() > 0.0 && size.y() > 0.0 && !out.defined)
        out.defined = true;
    // Reset the build volume Z, we don't want to zoom to the top of the build volume if it is empty.
    out.min.z() = 0.0;
    out.max.z() = 0.0;
    // extend to contain axes
    //BBS: add part plate related logic.
    Vec3d offset{ m_position.x(), m_position.y(), 0.f };
    //out.merge(m_axes.get_origin() + offset + m_axes.get_total_length() * Vec3d::Ones());
    out.merge(Vec3d(0.f, 0.f, GROUND_Z) + offset + m_axes.get_total_length() * Vec3d::Ones());
    out.merge(out.min + Vec3d(-Axes::DefaultTipRadius, -Axes::DefaultTipRadius, out.max.z()));
    //BBS: add part plate related logic.
    if (consider_model_offset) {
        // extend to contain model, if any
        for (int i = 0; i < m_model_offset_group.size(); ++i)
        {
            //auto& model = m_model_group[i];
            BoundingBoxf3 model_bb = m_model.get_bounding_box();
            if (model_bb.defined) {
                model_bb.translate(m_model_offset_group[i]);
                out.merge(model_bb);
            }
        }
    }
    return out;
}

// Try to match the print bed shape with the shape of an active profile. If such a match exists,
// return the print bed model.
std::tuple<Bed3D::Type, std::string, std::string> Bed3D::detect_type(const Pointfs& shape)
{
    auto bundle = app_preset_bundle();
    if (bundle != nullptr) {
        const Preset* curr = &bundle->printers.get_selected_preset();
        while (curr != nullptr) {
            if (curr->config.has("printable_area")) {
                std::string texture_filename, model_filename;
                if (shape == dynamic_cast<const ConfigOptionPoints*>(curr->config.option("printable_area"))->values) {
                    if (curr->is_system)
                        model_filename = PresetUtils::system_printer_bed_model(*curr);
                    else {
                        auto *printer_model = curr->config.opt<ConfigOptionString>("printer_model");
                        if (printer_model != nullptr && ! printer_model->value.empty()) {
                            model_filename = bundle->get_stl_model_for_printer_model(printer_model->value);
                        }
                    }
                    //std::string model_filename = PresetUtils::system_printer_bed_model(*curr);
                    //std::string texture_filename = PresetUtils::system_printer_bed_texture(*curr);
                    if (!model_filename.empty())
                        return { Type::System, model_filename, texture_filename };
                }
            }

            curr = bundle->printers.get_preset_parent(*curr);
        }
    }

    return { Type::Custom, {}, {} };
}

void Bed3D::render_axes()
{
    if (m_build_volume.valid())
        m_axes.render();
}

void Bed3D::render_system(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom)
{
    if (!bottom)
        render_model(view_matrix, projection_matrix);
}

//BBS: add part plate related logic
void Bed3D::update_model_offset()
{
    // move the model so that its origin (0.0, 0.0, 0.0) goes into the bed shape center and a bit down to avoid z-fighting with the texture quad
    
    auto getSize = [&]() -> Vec2d {

        std::vector< Vec2d> allPoint;
        for (auto& shape : m_bed_shape_group)
        {
            for (auto& point : shape)
            {
                allPoint.push_back(point);
            }
        }
        BoundingBoxf bed_ext_group = get_extents(allPoint);
        Vec2d size = bed_ext_group.size();
        return size;
        };

    Vec3d shift = m_build_volume.bounding_volume().center() + Vec3d(m_position[0], m_position[1], 0);
    shift(2) = -0.03;
    Vec2d size = getSize();
    for (int i = 0; i < m_model_offset_group.size(); ++i)
    { 
        auto& m_model_offset = m_model_offset_group.at(i);
        auto& m_bed_shape = m_bed_shape_group.at(i);
        Vec3d* model_offset_ptr = const_cast<Vec3d*>(&m_model_offset);
        *model_offset_ptr = shift;
        //BBS: TODO: hack for current stl for BBL printer
        //if (std::string::npos != m_model_filename.find("bbl-3dp-"))
        {
            Vec2d center = m_bed_shape[0] + (m_bed_shape[2] - m_bed_shape[0]) / 2;
            (*model_offset_ptr)(0) += size.x() / 2;
            (*model_offset_ptr)(1) += size.y() / 2;
            (*model_offset_ptr)(0) -= center.x();
            (*model_offset_ptr)(1) -= center.y();
        }
        (*model_offset_ptr)(2) = -0.41 + GROUND_Z;
    }
    
    // update extended bounding box
    const_cast<BoundingBoxf3&>(m_extended_bounding_box) = calc_extended_bounding_box(false);
    for (auto& m_triangles: m_triangle_group)
    {
        m_triangles.reset();
    }
}

void Bed3D::update_bed_triangles()
{
    for (size_t i = 0; i < m_triangle_group.size(); i++)
    {
        auto& m_triangles = m_triangle_group.at(i);
        auto& m_model_offset = m_model_offset_group.at(i);
        auto& m_bed_shape = m_bed_shape_group.at(i);

        if (m_triangles.is_initialized()) {
            return;
        }

        Vec3d shift = m_extended_bounding_box.center();
        shift(2) = -0.03;
        Vec3d* model_offset_ptr = const_cast<Vec3d*>(&m_model_offset);
        *model_offset_ptr = shift;
        //BBS: TODO: hack for default bed
        BoundingBoxf3 build_volume;

        if (!m_build_volume.valid()) return;
        auto bed_ext = get_extents(m_bed_shape);
        (*model_offset_ptr)(0) = m_build_volume.bounding_volume2d().min.x() + bed_ext.min.x();
        (*model_offset_ptr)(1) = m_build_volume.bounding_volume2d().min.y() + bed_ext.min.y();
        (*model_offset_ptr)(2) = -0.41 + GROUND_Z;

        std::vector<Vec2d> origin_bed_shape;
        for (size_t i = 0; i < m_bed_shape.size(); i++) {
            origin_bed_shape.push_back(m_bed_shape[i] - m_bed_shape[0]);
        }
        std::vector<Vec2d> new_bed_shape; // offset to correct origin
        for (auto point : origin_bed_shape) {
            Vec2d new_point(point.x() + model_offset_ptr->x(), point.y() + model_offset_ptr->y());
            new_bed_shape.push_back(new_point);
        }
        ExPolygon poly{ Polygon::new_scale(new_bed_shape) };
        if (!init_model_from_poly(m_triangles, poly, GROUND_Z)) {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ":Unable to update plate triangles\n";
        }
        // update extended bounding box
    }
    
    const_cast<BoundingBoxf3&>(m_extended_bounding_box) = calc_extended_bounding_box();
}

void Bed3D::render_model(const Transform3d& view_matrix, const Transform3d& projection_matrix)
{
 
    int gradient[] = {
        1,1,0,0
    };

    Vec3d  gradient_color[] = {
        {0.44,0.277,0.168},
        {0.21,0.43,0.11},
        {0.21,0.43,0.11},
        {0.44,0.277,0.168},
    };

	GLShaderProgram* shader = get_shader("gouraud_light_bed");
	if (shader == nullptr)
		return;

	shader->start_using();

	auto getScale = [&](std::vector<Vec2d>& bed_shape, Vec2d& scale) -> void {

		std::vector< Vec2d> allPoint;
		for (auto& shape : m_bed_shape_group)
		{
			for (auto& point : shape)
			{
				allPoint.push_back(point);
			}
		}
		BoundingBoxf bed_ext_group = get_extents(allPoint);
		BoundingBoxf bed_ext = get_extents(bed_shape);
		float xScale = bed_ext.size().x() / bed_ext_group.size().x();
		float yScale = bed_ext.size().y() / bed_ext_group.size().y();
		scale = Vec2d(xScale, yScale);
		};

    
	for (size_t i = 0; i < m_bed_shape_group.size(); i++)
	{

		auto& m_model_offset = m_model_offset_group.at(i);
		auto& bed_shape = m_bed_shape_group.at(i);

		if (m_model_filename.empty())
			return;

		if (m_model.get_filename() != m_model_filename && m_model.init_from_file(m_model_filename)) {
			m_model.set_color(m_is_dark ? DEFAULT_MODEL_COLOR_DARK : DEFAULT_MODEL_COLOR);

        	update_model_offset();		}
        
		Vec2d scale;
		getScale(bed_shape, scale);

		if (!m_model.get_filename().empty()) 
        {
            const BoundingBoxf3&  box = m_model.get_bounding_box();
            const std::array<double, 4> xyRange = { box.min.x(), box.min.y(), box.max.x(), box.max.y() };
			shader->set_uniform("emission_factor", 0.0f);
			Transform3d model_matrix = Geometry::assemble_transform(m_model_offset, Vec3d::Zero(), Vec3d(scale.x(), scale.y(), 1.0));
			shader->set_uniform("view_model_matrix", view_matrix * model_matrix);
			shader->set_uniform("projection_matrix", projection_matrix);
			const Matrix3d view_normal_matrix = view_matrix.matrix().block(0, 0, 3, 3) * model_matrix.matrix().block(0, 0, 3, 3).inverse().transpose();
			shader->set_uniform("view_normal_matrix", view_normal_matrix);
            shader->set_uniform("xyRange", xyRange);
           // int gradient = i % 2;
            shader->set_uniform("gradient", gradient[i]);
            shader->set_uniform("gradient_color", gradient_color[i]);
			m_model.render();
		}
	}
	shader->stop_using();
}

void Bed3D::render_custom(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom)
{
    if (m_model_filename.empty()) {
        render_default(bottom, view_matrix, projection_matrix);
        return;
    }

    if (!bottom)
        render_model(view_matrix, projection_matrix);
}

void Bed3D::render_default(bool bottom, const Transform3d& view_matrix, const Transform3d& projection_matrix)
{
    // m_texture.reset();

    update_bed_triangles();

    for (size_t i = 0; i < m_triangle_group.size(); i++)
    {
        
        auto& m_triangles = m_triangle_group.at(i);

        GLShaderProgram* shader = get_shader("flat");
        if (shader != nullptr) {
            shader->start_using();

            shader->set_uniform("view_model_matrix", view_matrix);
            shader->set_uniform("projection_matrix", projection_matrix);

            glsafe(::glEnable(GL_DEPTH_TEST));
            glsafe(::glEnable(GL_BLEND));
            glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

            if (m_model.get_filename().empty() && !bottom) {
                // draw background
                glsafe(::glDepthMask(GL_FALSE));
                m_triangles.set_color(DEFAULT_MODEL_COLOR);
                m_triangles.render();
                glsafe(::glDepthMask(GL_TRUE));
            }

            /*if (!picking) {
                // draw grid
                glsafe(::glLineWidth(1.5f * m_scale_factor));
                m_gridlines.set_color(picking ? DEFAULT_SOLID_GRID_COLOR : DEFAULT_TRANSPARENT_GRID_COLOR);
                m_gridlines.render();
            }*/

            glsafe(::glDisable(GL_BLEND));

            shader->stop_using();
        }
    }
  
}

} // GUI
} // Slic3r

#ifndef slic3r_3DBed_hpp_
#define slic3r_3DBed_hpp_

#include "slic3r/Render/GLModel.hpp"

#include "libslic3r/BuildVolume.hpp"
#include "libslic3r/ExPolygon.hpp"

#include <tuple>
#include <array>
#include <vector>

namespace Slic3r {
namespace GUI {

class Bed3DTexture;
class GLCanvas3D;

bool init_model_from_poly(GLModel &model, const ExPolygon &poly, float z);
bool init_model_from_polys(GLModel& model, const std::vector<ExPolygon>& polys, float z);
 
class Bed3D
{
public:
    static ColorRGBA AXIS_X_COLOR;
    static ColorRGBA AXIS_Y_COLOR;
    static ColorRGBA AXIS_Z_COLOR;

    class Axes
    {
    public:
        static const float DefaultStemRadius;
        static const float DefaultStemLength;
        static const float DefaultTipRadius;
        static const float DefaultTipLength;

    private:
        Vec3d m_origin{ Vec3d::Zero() };
        float m_stem_length{ DefaultStemLength };
        GLModel m_arrow;

    public:
        const Vec3d& get_origin() const { return m_origin; }
        void set_origin(const Vec3d& origin) { m_origin = origin; }
        void set_stem_length(float length) {
            m_stem_length = length;
            m_arrow.reset();
        }
        float get_total_length() const { return m_stem_length + DefaultTipLength; }
        void render();
    };

public:
    enum class Type : unsigned char
    {
        // The print bed model and texture are available from some printer preset.
        System,
        // The print bed model is unknown, thus it is rendered procedurally.
        Custom
    };

    Bed3DTexture* m_texture;
private:
    BuildVolume m_build_volume;
    std::vector <BuildVolume> m_sub_build_volumes;  //  
    Type m_type{ Type::System };
    //std::string m_texture_filename;
    std::string m_model_filename;
    // Print volume bounding box exteded with axes and model.
    BoundingBoxf3 m_extended_bounding_box;
    // Slightly expanded print bed polygon, for collision detection.
    //Polygon m_polygon;
    std::vector<GLModel> m_triangle_group;
    
    GLModel m_model;
    std::vector<Vec3d> m_model_offset_group;
    //Vec3d m_model_offset{ Vec3d::Zero() };
    Axes m_axes;
    float m_scale_factor{ 1.0f };
    //BBS: add part plate related logic
    Vec2d m_position{ Vec2d::Zero() };
    std::vector<std::vector<Vec2d>> m_bed_shape_group;
    bool m_is_dark = false;

public:
    Bed3D();
    ~Bed3D();

    bool set_shape(const std::vector<Pointfs>& printable_area, const double printable_height, const std::string& custom_model, bool force_as_custom,
        const Vec2d& position, bool with_reset = true);

    void set_position(const Vec2d& position);
    void set_axes_mode(bool origin);
    const Vec2d& get_position() const { return m_position; }

    // Build volume geometry for various collision detection tasks.
    const BuildVolume& build_volume() const { return m_build_volume; }

    const std::vector<BuildVolume>& sub_build_volume() const { return m_sub_build_volumes; }

    // Was the model provided, or was it generated procedurally?
    Type get_type() const { return m_type; }
    // Was the model generated procedurally?
    bool is_custom() const { return m_type == Type::Custom; }

    // for divided bed
    Vec2d get_master_slave_offset();

    // get the bed shape type
    BuildVolume_Type get_build_volume_type() const { return m_build_volume.type(); }

    // Bounding box around the print bed, axes and model, for rendering.
    const BoundingBoxf3& extended_bounding_box() const { return m_extended_bounding_box; }

    // Check against an expanded 2d bounding box.
    //FIXME shall one check against the real build volume?
    bool contains(const Point& point) const;
    Point point_projection(const Point& point) const;

    void render(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool show_axes);

    void on_change_color_mode(bool is_dark);

private:
    //BBS: add partplate related logic
    // Calculate an extended bounding box from axes and current model for visualization purposes.
    BoundingBoxf3 calc_extended_bounding_box(bool consider_model_offset = true) const;
    void update_model_offset();
    //BBS: with offset
    void update_bed_triangles();
    static std::tuple<Type, std::string, std::string> detect_type(const Pointfs& shape);
    void render_internal(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool show_axes);
    void render_axes();
    void render_system(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom);
    //void render_texture(bool bottom, GLCanvas3D& canvas);
    void render_model(const Transform3d& view_matrix, const Transform3d& projection_matrix);
    void render_custom(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom);
    void render_default(bool bottom, const Transform3d& view_matrix, const Transform3d& projection_matrix);
    
};

} // GUI
} // Slic3r

#endif // slic3r_3DBed_hpp_

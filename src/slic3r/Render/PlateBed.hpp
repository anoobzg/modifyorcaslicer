#ifndef _slic3r_PlateBed_hpp
#define _slic3r_PlateBed_hpp

#include "libslic3r/BuildVolume.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "slic3r/Scene/Camera.hpp"
#include "GLModel.hpp"
#include "MeshUtils.hpp"
#include "GLTexture.hpp"
#include "libslic3r/Geometry.hpp"
#include "slic3r/Render/MeshUtils.hpp"
#include "slic3r/Render/3DScene.hpp"

namespace Slic3r {
namespace GUI {

class Plater;
class Bed3D;
class SceneRaycaster;
class PlateBed
{
public:
    enum HeightLimitMode{
        HEIGHT_LIMIT_NONE,
        HEIGHT_LIMIT_BOTTOM,
        HEIGHT_LIMIT_TOP,
        HEIGHT_LIMIT_BOTH
    };

    struct RenderConfig 
    {
        Transform3d view_matrix;
        Transform3d projection_matrix;
        bool bottom;
        bool only_body; 
        bool force_background_color; 
        HeightLimitMode mode;
        int hover_id; 
        bool render_cali;
        bool show_grid;
        bool is_locked;
        int plate_id; 
        int max_count; // MAX_PLATES_COUNT
        double scale_factor;
        bool has_plate_settings;// bool has_plate_settings = get_bed_type() != BedType::btDefault || get_print_seq() != PrintSequence::ByDefault || !get_first_layer_print_sequence().empty() || !get_other_layers_print_sequence().empty() || has_spiral_mode_config();
        double scale; // m_plater->get_current_canvas3D()->get_scale();
        bool is_dark;
        BedType bed_type;
    };
    
    static const unsigned int PLATE_NAME_HOVER_ID = 6;
    static const unsigned int GRABBER_COUNT = 8;

    static ColorRGBA SELECT_COLOR;
    static ColorRGBA UNSELECT_COLOR;
    static ColorRGBA UNSELECT_DARK_COLOR;
    static ColorRGBA DEFAULT_COLOR;
    static ColorRGBA LINE_BOTTOM_COLOR;
    static ColorRGBA LINE_TOP_COLOR;
    static ColorRGBA LINE_TOP_DARK_COLOR;
    static ColorRGBA LINE_TOP_SEL_COLOR;
    static ColorRGBA LINE_TOP_SEL_DARK_COLOR;
    static ColorRGBA HEIGHT_LIMIT_BOTTOM_COLOR;
    static ColorRGBA HEIGHT_LIMIT_TOP_COLOR;

    static bool set_shape(const std::vector<Pointfs>& printable_area, const double printable_height, const std::string& custom_model, bool force_as_custom,
        const Vec2d& position, bool with_reset = true);

    static BoundingBoxf bounding_volume2d();
    static const BuildVolume& build_volume();
    static Vec2d get_master_slave_offset();
    static const std::vector<BuildVolume>& sub_build_volume();
    static void set_axes_mode(bool origin);
    static void on_change_color_mode(bool is_dark);
    static BoundingBoxf3 extended_bounding_box();
    void set_position(const Vec2d& pos);

    /* texture */
    static const std::string &get_logo_texture_filename();
    static void update_logo_texture_filename(const std::string &texture_filename);
    static void generate_icon_textures();
    static void release_icon_textures();
    static void set_render_option(bool bedtype_texture, bool plate_settings, bool cali = true);

    PlateBed();
    
    void set_model(Model* model);
    void set_selected(bool selected);
    Vec2d get_size() const { return Vec2d(m_width, m_depth); }
    Vec3d get_origin() { return m_origin; }

    void render(const RenderConfig& config);

    const std::vector<Pointfs>& get_shape() const;
    bool set_shape(const std::vector<Pointfs>& shape_group, const std::vector<Pointfs>& exclude_areas_group, Vec2d position, float height_to_lid, float height_to_rod);

    void register_raycasters_for_picking(SceneRaycaster* raycaster, int plate_id);
    void generate_plate_name_texture();

    std::vector<BoundingBoxf3> get_exclude_bounding_box();
    const BoundingBox get_bounding_box_crd();
    BoundingBoxf3 get_build_volume();
    bool check_intersects(const BoundingBoxf3& box);
    bool contains(const Vec3d& point) const;
    bool contains(const GLVolume& v) const;
    bool contains(const BoundingBoxf3& bb) const;
    bool intersects(const BoundingBoxf3& bb) const;
    bool check_outside(int obj_id, int instance_id, BoundingBoxf3* bounding_box);

    Vec3d get_center_origin();

    const BoundingBoxf3& get_bounding_box(bool extended = false);
    const std::vector<BoundingBoxf3>& get_exclude_areas();

private:
    void generate_print_polygon(std::vector<ExPolygon> &print_polygon);
    void generate_exclude_polygon(std::vector<ExPolygon>&exclude_polygon);
    void generate_logo_polygon(ExPolygon &logo_polygon);
    void calc_bounding_boxes() const;
    void calc_triangles(const std::vector<ExPolygon>& polys);
    void calc_exclude_triangles(const std::vector<ExPolygon>& polys);
    void calc_gridlines(const std::vector<ExPolygon>& polys);
    void calc_height_limit();
    void calc_vertex_for_number(int index, bool one_number, GLModel &buffer);
    void calc_vertex_for_plate_name_edit_icon(GLTexture *texture, int index, PickingModel &model);
    void calc_vertex_for_icons(int index, PickingModel &model);

private: /* render */
    void render_background(const RenderConfig& config);
    void render_logo(const RenderConfig& config);
    void render_exclude_area(const RenderConfig& config);
    void render_grid(const RenderConfig& config);
    void render_height_limit(const RenderConfig& config);
    void render_icons(const RenderConfig& config);
    void render_only_numbers(const RenderConfig& config);

    void render_icon_texture(GLModel &buffer, GLTexture &texture);
    void show_tooltip(const std::string tooltip, float scale);
    void render_logo_texture(const RenderConfig& config, GLTexture &logo_texture, GLModel& logo_buffer);

public:
    int m_plate_id;

    bool m_selected;
    Vec2d m_position;
    int m_width;
    int m_depth;
    Vec3d m_origin;
    int m_height;
    float m_height_to_lid;
    float m_height_to_rod;

    std::string m_name;
    GLModel m_plate_name_icon;
    GLTexture m_name_texture;

private:
    Model* m_model; 
    Bed3D* m_bed;
    SceneRaycaster* m_scene_raycaster { NULL };

    std::vector<Pointfs>  m_raw_shape_group;
    std::vector<Pointfs> m_shape_group;
    std::vector<Pointfs> m_exclude_area_group;
    BoundingBoxf3 m_bounding_box;
    BoundingBoxf3 m_extended_bounding_box;
    mutable std::vector<BoundingBoxf3> m_exclude_bounding_box;
    mutable BoundingBoxf3 m_grabber_box;
    Transform3d m_grabber_trans_matrix;
    Slic3r::Geometry::Transformation  m_transPos;
    std::vector<Vec3f> positions;
    PickingModel m_triangles;
    std::vector<GLModel> m_exclude_triangles_group;
    GLModel m_logo_triangles;
    std::vector<GLModel> m_gridlines_group;

    std::vector<GLModel> m_gridlines_bolder_group;
    std::vector<GLModel> m_height_limit_common_group;
    std::vector<GLModel> m_height_limit_bottom_group;
    std::vector<GLModel> m_height_limit_top_group;
    PickingModel m_del_icon;
    PickingModel m_arrange_icon;
    PickingModel m_orient_icon;
    PickingModel m_lock_icon;
    PickingModel m_plate_settings_icon;
    PickingModel m_plate_name_edit_icon;
    PickingModel m_move_front_icon;
    GLModel m_plate_idx_icon;
    GLTexture m_texture;

    Plater* m_plater;

};


};
};



#endif 
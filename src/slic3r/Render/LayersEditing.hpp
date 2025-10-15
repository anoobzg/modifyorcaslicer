#pragma once 
#include "libslic3r/Slicing.hpp"
#include "slic3r/Render/3DScene.hpp"
#include "slic3r/GUI/Gizmos/GLGizmosManager.hpp"

namespace Slic3r {

struct SlicingParameters;
class DynamicPrintConfig;
class ModelObject;
namespace GUI {

class GLCanvas3D;
class LayersEditing
{
public:
    enum EState : unsigned char
    {
        Unknown,
        Editing,
        Completed,
        Num_States
    };

    static const float THICKNESS_BAR_WIDTH;
    
    // Orca: Shrinkage compensation
    void set_shrinkage_compensation(const Vec3d &shrinkage_compensation) { m_shrinkage_compensation = shrinkage_compensation; };

private:
    bool                        m_enabled{ false };
    unsigned int                m_z_texture_id{ 0 };
    // Not owned by LayersEditing.
    const DynamicPrintConfig* m_config{ nullptr };
    // ModelObject for the currently selected object (Model::objects[last_object_id]).
    const ModelObject* m_model_object{ nullptr };
    // Maximum z of the currently selected object (Model::objects[last_object_id]).
    float                       m_object_max_z{ 0.0f };
    // Owned by LayersEditing.
    SlicingParameters* m_slicing_parameters{ nullptr };
    std::vector<double>         m_layer_height_profile;
    
    // Orca: Shrinkage compensation to apply when we need to use object_max_z with Z compensation.
    Vec3d                       m_shrinkage_compensation{ Vec3d::Ones() };

    mutable float               m_adaptive_quality{ 0.5f };
    mutable HeightProfileSmoothingParams m_smooth_params;

    static float                s_overlay_window_width;

    struct LayersTexture
    {
        // Texture data
        std::vector<char>   data;
        // Width of the texture, top level.
        size_t              width{ 0 };
        // Height of the texture, top level.
        size_t              height{ 0 };
        // For how many levels of detail is the data allocated?
        size_t              levels{ 0 };
        // Number of texture cells allocated for the height texture.
        size_t              cells{ 0 };
        // Does it need to be refreshed?
        bool                valid{ false };
    };
    LayersTexture   m_layers_texture;

public:
    EState state{ Unknown };
    float band_width{ 2.0f };
    float strength{ 0.005f };
    int last_object_id{ -1 };
    float last_z{ 0.0f };
    LayerHeightEditActionType last_action{ LAYER_HEIGHT_EDIT_ACTION_INCREASE };
    struct Profile
    {
        GLModel baseline;
        GLModel profile;
        GLModel background;
        float old_canvas_width{ 0.0f };
        std::vector<double> old_layer_height_profile;
    };
    Profile m_profile;

    LayersEditing() = default;
    ~LayersEditing();

    void init();

    void set_config(const DynamicPrintConfig* config);
    void select_object(const Model& model, int object_id);

    bool is_allowed() const;

    bool is_enabled() const;
    void set_enabled(bool enabled);

    void show_tooltip_information(const GLCanvas3D& canvas, std::map<wxString, wxString> captions_texts, float x, float y);
    void render_variable_layer_height_dialog(const GLCanvas3D& canvas);
    void render_overlay(const GLCanvas3D& canvas);
    void render_volumes(const GLCanvas3D& canvas, const GLVolumeCollection& volumes);

    void adjust_layer_height_profile();
    void accept_changes(GLCanvas3D& canvas);
    void reset_layer_height_profile(GLCanvas3D& canvas);
    void adaptive_layer_height_profile(GLCanvas3D& canvas, float quality_factor);
    void smooth_layer_height_profile(GLCanvas3D& canvas, const HeightProfileSmoothingParams& smoothing_params);

    static float get_cursor_z_relative(const GLCanvas3D& canvas);
    static bool bar_rect_contains(const GLCanvas3D& canvas, float x, float y);
    static Rect get_bar_rect_screen(const GLCanvas3D& canvas);
    static float get_overlay_window_width() { return LayersEditing::s_overlay_window_width; }

    float object_max_z() const { return m_object_max_z; }

    std::string get_tooltip(const GLCanvas3D& canvas) const;
    

private:
    bool is_initialized() const;
    void generate_layer_height_texture();
    void render_active_object_annotations(const GLCanvas3D& canvas);
    void render_profile(const GLCanvas3D& canvas);
    void update_slicing_parameters();

    static float thickness_bar_width(const GLCanvas3D& canvas);
};

}
}
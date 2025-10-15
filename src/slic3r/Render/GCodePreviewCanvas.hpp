#pragma once
#include "slic3r/Render/GLCanvas3D.hpp"
#include "libslic3r/Config.hpp"

namespace Slic3r {

class BackgroundSlicingProcess;
class PartPlateList;
class PresetBundle;
namespace GUI {

class GCodeViewer;
class GCodePreviewCanvas : public GLCanvas3D
{
public:
    GCodePreviewCanvas(wxGLCanvas* canvas);
    ~GCodePreviewCanvas();

    void set_process(BackgroundSlicingProcess* process);
    void set_plate_list(PartPlateList* plate_list);

    /* canvas method */
    void set_gcode_view_preview_type(int type);
    int get_gcode_view_preview_type() const;
    void set_shells_on_previewing(bool is_preview);
    bool is_gcode_legend_enabled() const;
    int get_gcode_view_type() const;
    std::vector<CustomGCode::Item>& get_custom_gcode_per_print_z();
    const std::vector<double>& get_gcode_layers_zs() const;
    size_t get_gcode_extruders_count();
    GCodeViewer* get_gcode_viewer();
    void init_gcode_viewer(ConfigOptionMode mode, Slic3r::PresetBundle* preset_bundle);
    void reset_gcode_toolpaths();
    void update_gcode_sequential_view_current(unsigned int first, unsigned int last);
    void load_gcode_preview(GCodeResultWrapper* gcode_result, const std::vector<std::string>& str_tool_colors, bool only_gcode);
    void refresh_gcode_preview_render_paths();

    void set_shell_transparence(float alpha = 0.2f);
    void load_shells(const Print& print, bool force_previewing = false);
   //BBS use gcoder viewer render calibration thumbnails
    void render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params);

    bool has_toolpaths_to_export() const;
    void export_toolpaths_to_obj(const char* filename) const;



    void key_handle(wxKeyEvent& evt);
    void _render_gcode(int canvas_width, int canvas_height);
    void _set_warning_notification_if_needed(EWarning warning);
    void enable_legend_texture(bool enable);
    void zoom_to_gcode();

private:
    void _render_imgui_select_plate_toolbar();
    virtual void _set_warning_notification(EWarning warning, bool state);
    
public:
    virtual void _on_mouse(wxMouseEvent& evt) override;
    virtual void reload_scene(bool refresh_immediately, bool force_full_scene_refresh = false) override;
    virtual void render(bool only_init = false) override;

    // virtual const Print* fff_print() const override;


private:  
    PartPlateList* m_plate_list;
    // BackgroundSlicingProcess* m_process;
    GCodeViewer* m_gcode_viewer;

};

}
}
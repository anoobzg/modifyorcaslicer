#pragma once 
#include "slic3r/GUI/Frame/OpenGLPanel.hpp"

#include "libslic3r/Model.hpp"
#include "libslic3r/PrintBase.hpp"

#include "slic3r/Render/Selection.hpp"
#include "slic3r/Scene/Camera.hpp"
#include "slic3r/Render/3DScene.hpp"

namespace Slic3r { 
class DynamicPrintConfig;
class Print;
class BackgroundSlicingProcess;
class Model;
namespace GUI {

class GLCanvas3D;
class GCodePreviewCanvas;
class GCodeResultWrapper;
class GLToolbar;
class Plater;
class PartPlateList;
class IMSlider;

class GCodePreview : public OpenGLPanel
{
    GCodePreviewCanvas* m_canvas { nullptr };
    DynamicPrintConfig* m_config;
    BackgroundSlicingProcess* m_process;
    GCodeResultWrapper* m_gcode_result;

#ifdef __linux__
    // We are getting mysterious crashes on Linux in gtk due to OpenGL context activation GH #1874 #1955.
    // So we are applying a workaround here.
    bool m_volumes_cleanup_required { false };
#endif /* __linux__ */

    // Calling this function object forces Plater::schedule_background_process.
    std::function<void()> m_schedule_background_process;

    unsigned int m_number_extruders { 1 };
    bool m_keep_current_preview_type{ false };

    //bool m_loaded { false };
    //BBS: add logic for preview print
    const Slic3r::PrintBase* m_loaded_print { nullptr };
    //BBS: add only gcode mode
    bool m_only_gcode { false };
    bool m_reload_paint_after_background_process_apply{false};

public:
    enum class OptionType : unsigned int
    {
        Travel,
        Wipe,
        Retractions,
        Unretractions,
        Seams,
        ToolChanges,
        ColorChanges,
        PausePrints,
        CustomGCodes,
        Shells,
        ToolMarker,
        Legend
    };

    GCodePreview(wxWindow* parent, Model* model, DynamicPrintConfig* config, BackgroundSlicingProcess* process, PartPlateList* plate_list,
        GCodeResultWrapper* gcode_result, std::function<void()> schedule_background_process = []() {});
    virtual ~GCodePreview();

    //BBS: update gcode_result
    void update_gcode_result(GCodeResultWrapper* gcode_result);

    wxGLCanvas* get_wxglcanvas();
    // GLCanvas3D* get_canvas3d();
    GCodePreviewCanvas* get_canvas3d();

    void set_as_dirty();

    void bed_shape_changed();
    void select_view(const std::string& direction);
    void set_drop_target(wxDropTarget* target);

    //BBS: add only gcode mode
    void load_print(bool keep_z_range = false, bool only_gcode = false);
    void reload_print(bool keep_volumes = false, bool only_gcode = false);
    void refresh_print();
    //BBS: always load shell at preview
    void load_shells(const Print& print, bool force_previewing = false);

    void msw_rescale();
    void sys_color_changed();

    //BBS: add m_loaded_print logic
    bool is_loaded() const { return (m_loaded_print != nullptr); }
    //BBS
    void on_tick_changed();

    void show_sliders(bool show = true);
    void show_moves_sliders(bool show = true);
    void show_layers_sliders(bool show = true);
    void set_reload_paint_after_background_process_apply(bool flag) { m_reload_paint_after_background_process_apply = flag; }
    bool get_reload_paint_after_background_process_apply() { return m_reload_paint_after_background_process_apply; }

        
    IMSlider* get_layers_slider();

private:
    bool init(wxWindow* parent, Model* model, PartPlateList* plate_list);
    
    void check_layers_slider_values(std::vector<CustomGCode::Item>& ticks_from_model,
        const std::vector<double>& layers_z);

    void update_layers_slider(const std::vector<double>& layers_z, bool keep_z_range = false);    
    void update_layers_slider_mode();
    void update_layers_slider_from_canvas(wxKeyEvent &event);
    //BBS: add only gcode mode
    void load_print_as_fff(bool keep_z_range = false, bool only_gcode = false);

    void do_reslice();
    
public:
    virtual void attach() override;
    virtual void detach() override;
protected:
    void render_impl() override;
};

}
}
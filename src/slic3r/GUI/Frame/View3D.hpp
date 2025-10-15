#pragma once 
#include "slic3r/GUI/Frame/OpenGLPanel.hpp"

#include "libslic3r/Model.hpp"
//#include "slic3r/Render/Selection.hpp"
#include "slic3r/Scene/Camera.hpp"
#include "slic3r/Render/3DScene.hpp"

namespace Slic3r { 
class DynamicPrintConfig;
class Print;
class BackgroundSlicingProcess;
class Model;
namespace GUI {
class GLCanvas3D;
class View3DCanvas;
class GLToolbar;
class Plater;
class Selection;
class View3D : public OpenGLPanel
{
    View3DCanvas* m_canvas;

public:
    View3D(wxWindow* parent, Model* model, DynamicPrintConfig* config, BackgroundSlicingProcess* process, Selection* selection);
    virtual ~View3D();

    wxGLCanvas* get_wxglcanvas();
    GLCanvas3D* get_canvas3d();

    void set_as_dirty();
    void bed_shape_changed();
    void plates_count_changed();

    void select_view(const std::string& direction);

    //BBS
    void select_curr_plate_all();
    void select_object_from_idx(std::vector<int> &object_idxs);
    void remove_curr_plate_all();

    void select_all();
    void deselect_all();
    void exit_gizmo();
    void delete_selected();
    void center_selected();
    void drop_selected();
    void center_selected_plate(const int plate_idx);
    void mirror_selection(Axis axis);

    bool is_layers_editing_enabled() const;
    bool is_layers_editing_allowed() const;
    void enable_layers_editing(bool enable);

    bool is_dragging() const;
    bool is_reload_delayed() const;

    void reload_scene(bool refresh_immediately, bool force_full_scene_refresh = false);
    void render();

private:
    bool init(wxWindow* parent, Model* model, DynamicPrintConfig* config, BackgroundSlicingProcess* process, Selection* selection);

public:
    virtual void attach() override;
    virtual void detach() override;
protected:
    void render_impl() override;
};

}
}
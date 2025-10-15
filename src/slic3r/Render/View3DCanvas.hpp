#pragma once
#include "slic3r/Render/GLCanvas3D.hpp"

namespace Slic3r {

class Model;
namespace GUI {

class Selection;
class View3DCanvas : public GLCanvas3D
{
public:
    View3DCanvas(wxGLCanvas* canvas, Selection* selection);
    ~View3DCanvas();

    virtual void _on_mouse(wxMouseEvent& evt);
    virtual void reload_scene(bool refresh_immediately, bool force_full_scene_refresh = false) override;
    virtual void render(bool only_init = false) override;

    virtual void set_model(Model* model) override;
    virtual void set_selection(Selection* selection) override;

    void select_curr_plate_all();
    void select_object_from_idx(std::vector<int>& object_idxs);
    void remove_curr_plate_all();
    void select_all();
    void deselect_all();
    void exit_gizmo();
    void set_selected_visible(bool visible) ;
    void delete_selected();
    void mirror_selection(Axis axis);
    void do_move(const std::string& snapshot_type);
    void do_rotate(const std::string& snapshot_type);
    void do_scale(const std::string& snapshot_type);
    void do_center();
    void do_drop();
    void do_center_plate(const int plate_idx);
    void do_mirror(const std::string& snapshot_type);

private:
    void _render_objects(GLVolumeCollection::ERenderType type, GLVolumeCollection::ERenderMode render_mode, bool with_outline);
    void _render_selection();
    void _render_sequential_clearance();
    void _render_selection_center();
    void _check_and_update_toolbar_icon_scale();
    void _render_current_gizmo() const;
    void _render_gizmos_overlay();
    void _render_main_toolbar();

};

}
}
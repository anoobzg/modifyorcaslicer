#include "View3D.hpp"

#include "slic3r/Render/AppRender.hpp"
#include "slic3r/GUI/Frame/OpenGLWindow.hpp"

#include "slic3r/Render/View3DCanvas.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"

namespace Slic3r { 
namespace GUI {

View3D::View3D(wxWindow* parent, Model* model, DynamicPrintConfig* config, BackgroundSlicingProcess* process, Selection* selection)
    : OpenGLPanel(parent)
{

    init(parent, model, config, process, selection);
}

View3D::~View3D()
{
    delete m_canvas;
}

wxGLCanvas* View3D::get_wxglcanvas() 
{ 
    return raw_canvas(); 
}

GLCanvas3D* View3D::get_canvas3d() 
{ 
    return m_canvas; 
}

bool View3D::init(wxWindow* parent, Model* model, DynamicPrintConfig* config, BackgroundSlicingProcess* process, Selection* selection)
{
    m_canvas = new View3DCanvas(m_opengl_window, selection);
    m_canvas->set_context(raw_context());

    m_canvas->allow_multisample(can_multisample());
    // XXX: If have OpenGL
    m_canvas->enable_picking(true);
    m_canvas->get_selection()->set_mode(Selection::Instance);
    m_canvas->enable_moving(true);
    // XXX: more config from 3D.pm
    m_canvas->set_model(model);
    m_canvas->set_process(process);
    m_canvas->set_type(GLCanvas3D::ECanvasType::CanvasView3D);
    m_canvas->set_config(config);
    m_canvas->enable_selection(true);
    m_canvas->enable_main_toolbar(true);
    //BBS: GUI refactor: GLToolbar
    m_canvas->enable_select_plate_toolbar(false);
    m_canvas->enable_separator_toolbar(true);
    m_canvas->enable_labels(true);
    m_canvas->enable_slope(true);

    return true;
}

void View3D::set_as_dirty()
{
    if (m_canvas != nullptr) {
        m_canvas->set_as_dirty();
    }
}

void View3D::bed_shape_changed()
{
    if (m_canvas != nullptr)
        m_canvas->bed_shape_changed();
}

void View3D::plates_count_changed()
{
    if (m_canvas != nullptr)
        m_canvas->plates_count_changed();
}

void View3D::select_view(const std::string& direction)
{
    if (m_canvas != nullptr)
        m_canvas->select_view(direction);
}

//BBS
void View3D::select_curr_plate_all()
{
    if (m_canvas != nullptr)
        m_canvas->select_curr_plate_all();
}

void View3D::select_object_from_idx(std::vector<int>& object_idxs) {
    if (m_canvas != nullptr)
        m_canvas->select_object_from_idx(object_idxs);
}

//BBS
void View3D::remove_curr_plate_all()
{
    if (m_canvas != nullptr)
        m_canvas->remove_curr_plate_all();
}

void View3D::select_all()
{
    if (m_canvas != nullptr)
        m_canvas->select_all();
}

void View3D::deselect_all()
{
    if (m_canvas != nullptr)
        m_canvas->deselect_all();
}

void View3D::exit_gizmo()
{
    if (m_canvas != nullptr)
        m_canvas->exit_gizmo();
}

void View3D::delete_selected()
{
    if (m_canvas != nullptr)
        m_canvas->delete_selected();
}

void View3D::center_selected()
{
    if (m_canvas != nullptr)
        m_canvas->do_center();
}

void View3D::drop_selected()
{
    if (m_canvas != nullptr)
        m_canvas->do_drop();
}

void View3D::center_selected_plate(const int plate_idx) {
    if (m_canvas != nullptr)
        m_canvas->do_center_plate(plate_idx);
}

void View3D::mirror_selection(Axis axis)
{
    if (m_canvas != nullptr)
        m_canvas->mirror_selection(axis);
}

bool View3D::is_layers_editing_enabled() const
{
    return (m_canvas != nullptr) ? m_canvas->is_layers_editing_enabled() : false;
}

bool View3D::is_layers_editing_allowed() const
{
    return (m_canvas != nullptr) ? m_canvas->is_layers_editing_allowed() : false;
}

void View3D::enable_layers_editing(bool enable)
{
    if (m_canvas != nullptr)
        m_canvas->enable_layers_editing(enable);
}

bool View3D::is_dragging() const
{
    return (m_canvas != nullptr) ? m_canvas->is_dragging() : false;
}

bool View3D::is_reload_delayed() const
{
    return (m_canvas != nullptr) ? m_canvas->is_reload_delayed() : false;
}

void View3D::reload_scene(bool refresh_immediately, bool force_full_scene_refresh)
{
    if (m_canvas != nullptr)
        m_canvas->reload_scene(refresh_immediately, force_full_scene_refresh);
}

void View3D::render()
{
    if (m_canvas != nullptr)
        //m_canvas->render();
        m_canvas->set_as_dirty();
}

void View3D::attach() 
{
    //PlateBed::show_axes();
    m_canvas->bind_event_handlers();
    m_canvas->render();
    Show();

    Camera& cam = AppAdapter::plater()->get_camera();
    m_canvas->get_camera().load_camera_view(cam);
    cam.load_camera_view(m_canvas->get_camera());
    if (is_reload_delayed()) 
        reload_scene(true);

    set_as_dirty();
    m_canvas->reset_old_size();

}

void View3D::detach() 
{
    //PlateBed::hide_axes();
    m_canvas->unbind_event_handlers();
    Hide();

    Camera& cam = AppAdapter::plater()->get_camera();
    m_canvas->get_camera().load_camera_view(cam);
    cam.load_camera_view(m_canvas->get_camera());

    GLGizmosManager* gizmos = m_canvas->get_gizmos_manager(); 
    if (gizmos->is_running()) {
        gizmos->reset_all_states();
        gizmos->update_data();
    }
}

void View3D::render_impl()
{

}
}
}
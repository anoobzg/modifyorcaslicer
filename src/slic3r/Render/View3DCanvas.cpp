#include "View3DCanvas.hpp"
#include "GLCanvas3D.hpp"

#include "libslic3r/libslic3r.h"
#include "libslic3r/AppConfig.hpp"

#include <igl/unproject.h>

#include "libslic3r/BuildVolume.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"
#include "libslic3r/Geometry/ConvexHull.hpp"
#include "libslic3r/ExtrusionEntity.hpp"
#include "libslic3r/Layer.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/Technologies.hpp"
#include "libslic3r/Tesselate.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"
#include "libslic3r/FileSystem/Log.hpp"
#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "3DBed.hpp"
#include "3DScene.hpp"
#include "slic3r/Slice/BackgroundSlicingProcess.hpp"
#include "GLShader.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/Tab.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/Config/GUI_ObjectList.hpp"
#include "GLColors.hpp"
#include "Mouse3DController.hpp"
#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/Scene/NotificationManager.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/Frame/DailyTips.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Render/ImGUI/IMSlider.hpp"
#include "slic3r/Slice/GCodeResultWrapper.hpp"
#include "slic3r/Utils/UndoRedo.hpp"
#include "slic3r/Theme/MacDarkMode.hpp"
#include <slic3r/GUI/GUI_Utils.hpp>
#include "slic3r/GUI/Event/UserCanvasEvent.hpp"
#include "slic3r/GUI/Event/UserGLToolBarEvent.hpp"
#include "slic3r/Render/LayersEditing.hpp"
#include "GCodeViewer.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#if ENABLE_RETINA_GL
#include "slic3r/Utils/RetinaHelper.hpp"
#endif

#include <GL/glew.h>
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Render/RenderUtils.hpp"
#include "slic3r/Render/PlateBed.hpp"

#include <wx/glcanvas.h>
#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
#include <wx/settings.h>
#include <wx/tooltip.h>
#include <wx/debug.h>
#include <wx/fontutil.h>
// Print now includes tbb, and tbb includes Windows. This breaks compilation of wxWidgets if included before wx.
#include "libslic3r/Print.hpp"

#include "slic3r/GUI/wxExtensions.hpp"

#include <tbb/parallel_for.h>
#include <tbb/spin_mutex.h>

#include <boost/log/trivial.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <iostream>
#include <float.h>
#include <algorithm>
#include <cmath>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui/imgui_internal.h>

#include <imguizmo/ImGuizmo.h>
#include "EventHandler/GUIEventHandler.hpp"
#include "EventHandler/TrackballManipulator.hpp"
#include "EventHandler/PreviewEventHandler.hpp"

#include "GCodePreviewCanvas.hpp"

namespace Slic3r {
namespace GUI {

View3DCanvas::View3DCanvas(wxGLCanvas* canvas, Selection* selection)
    :GLCanvas3D(canvas)
{
    set_selection(selection);
}

View3DCanvas::~View3DCanvas()
{

}

void View3DCanvas::set_model(Model* model)
{
    GLCanvas3D::set_model(model);
}

void View3DCanvas::set_selection(Selection* selection)
{
    GLCanvas3D::set_selection(selection);
}

//BBS
void View3DCanvas::select_curr_plate_all()
{
    if (m_selection)
        m_selection->add_curr_plate();
    m_dirty = true;
}

void View3DCanvas::select_object_from_idx(std::vector<int>& object_idxs) {
    if (m_selection)
        m_selection->add_object_from_idx(object_idxs);
    m_dirty = true;
}

//BBS
void View3DCanvas::remove_curr_plate_all()
{
    if (m_selection)
        m_selection->remove_curr_plate();
    m_dirty = true;
}

void View3DCanvas::select_all()
{
    if (m_selection)
        m_selection->add_all();
    m_dirty = true;
}

void View3DCanvas::deselect_all()
{
    if (m_selection)
        m_selection->remove_all();
    m_gizmos->reset_all_states();
    m_gizmos->update_data();
    post_event(SimpleEvent(EVT_GLCANVAS_OBJECT_SELECT));
}

void View3DCanvas::exit_gizmo() {
    if (m_gizmos->get_current_type() != GLGizmosManager::Undefined) {
        m_gizmos->reset_all_states();
        m_gizmos->update_data();
    }
}

void View3DCanvas::set_selected_visible(bool visible)
{
    if (!m_selection)
        return;
    for (unsigned int i : m_selection->get_volume_idxs()) {
        GLVolume* volume = const_cast<GLVolume*>(m_selection->get_volume(i));
        volume->visible = visible;
        volume->color.a(visible ? 1.f : GLVolume::MODEL_HIDDEN_COL.a());
        volume->render_color.a(volume->color.a());
        volume->force_native_color = !visible;
    }
}

void View3DCanvas::delete_selected()
{
    if (!m_selection)
        return;
    m_selection->erase();
}

void View3DCanvas::mirror_selection(Axis axis)
{
    TransformationType transformation_type;
    if (AppAdapter::gui_app()->obj_manipul()->is_local_coordinates())
        transformation_type.set_local();
    else if (AppAdapter::gui_app()->obj_manipul()->is_instance_coordinates())
        transformation_type.set_instance();

    transformation_type.set_relative();

    if (m_selection)
    {
        m_selection->setup_cache();
        m_selection->mirror(axis, transformation_type);
    }

    do_mirror(L("Mirror Object"));
}

void View3DCanvas::do_move(const std::string& snapshot_type)
{
    if (m_model == nullptr || !m_selection)
        return;

    if (!snapshot_type.empty())
        AppAdapter::plater()->take_snapshot(snapshot_type);

    std::set<std::pair<int, int>> done;  // keeps track of modified instances
    bool object_moved = false;

    // BBS: support wipe-tower for multi-plates
    int n_plates = AppAdapter::plater()->get_partplate_list().get_plate_count();
    std::vector<Vec3d> wipe_tower_origins(n_plates, Vec3d::Zero());

    Selection::EMode selection_mode = m_selection->get_mode();

    for (const GLVolume* v : m_volumes.volumes) {
        int object_idx = v->object_idx();
        int instance_idx = v->instance_idx();
        int volume_idx = v->volume_idx();

        if (volume_idx < 0)
            continue;

        std::pair<int, int> done_id(object_idx, instance_idx);

        if (0 <= object_idx && object_idx < (int)m_model->objects.size()) {
            done.insert(done_id);

            // Move instances/volumes
            ModelObject* model_object = m_model->objects[object_idx];
            if (model_object != nullptr) {
                if (selection_mode == Selection::Instance)
                    model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
                else if (selection_mode == Selection::Volume) {
                    if (model_object->volumes[volume_idx]->get_transformation() != v->get_volume_transformation()) {
                        model_object->volumes[volume_idx]->set_transformation(v->get_volume_transformation());
                        // BBS: backup
                        Slic3r::save_object_mesh(*model_object);
                    }
                }

                object_moved = true;
                model_object->invalidate_bounding_box();
            }
        }
        else if (object_idx >= 1000 && object_idx < 1000 + n_plates) {
            // Move a wipe tower proxy.
            wipe_tower_origins[object_idx - 1000] = v->get_volume_offset();
        }
    }

    //BBS: notify instance updates to part plater list
    m_selection->notify_instance_update(-1, 0);

    // Fixes flying instances
    for (const std::pair<int, int>& i : done) {
        ModelObject* m = m_model->objects[i.first];
        const double shift_z = m->get_instance_min_z(i.second);
        //BBS: don't call translate if the z is zero
        if ((shift_z > SINKING_Z_THRESHOLD) && (shift_z != 0.0f)) {
            const Vec3d shift(0.0, 0.0, -shift_z);
            m_selection->translate(i.first, i.second, shift);
            m->translate_instance(i.second, shift);
            //BBS: notify instance updates to part plater list
            m_selection->notify_instance_update(i.first, i.second);
        }
        AppAdapter::obj_list()->update_info_items(static_cast<size_t>(i.first));
    }
    //BBS: nofity object list to update
    AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();

    if (object_moved)
        post_event(SimpleEvent(EVT_GLCANVAS_INSTANCE_MOVED));

    // BBS: support wipe-tower for multi-plates
    for (int plate_id = 0; plate_id < wipe_tower_origins.size(); plate_id++) {
        Vec3d& wipe_tower_origin = wipe_tower_origins[plate_id];
        if (wipe_tower_origin == Vec3d::Zero())
            continue;

        PartPlateList& ppl = AppAdapter::plater()->get_partplate_list();
        DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
        Vec3d plate_origin = ppl.get_plate(plate_id)->get_origin();
        ConfigOptionFloat wipe_tower_x(wipe_tower_origin(0) - plate_origin(0));
        ConfigOptionFloat wipe_tower_y(wipe_tower_origin(1) - plate_origin(1));

        ConfigOptionFloats* wipe_tower_x_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_x", true);
        ConfigOptionFloats* wipe_tower_y_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_y", true);
        wipe_tower_x_opt->set_at(&wipe_tower_x, plate_id, 0);
        wipe_tower_y_opt->set_at(&wipe_tower_y, plate_id, 0);
    }

    reset_sequential_print_clearance();

    m_dirty = true;
}

void View3DCanvas::do_rotate(const std::string& snapshot_type)
{
    // m_manipulator->do_rotate(snapshot_type);
    if (m_model == nullptr || !m_selection)
        return;

    if (!snapshot_type.empty())
        AppAdapter::plater()->take_snapshot(snapshot_type);

    // stores current min_z of instances
    std::map<std::pair<int, int>, double> min_zs;
    for (int i = 0; i < static_cast<int>(m_model->objects.size()); ++i) {
        const ModelObject* obj = m_model->objects[i];
        for (int j = 0; j < static_cast<int>(obj->instances.size()); ++j) {
            if (snapshot_type == L("Gizmo-Place on Face") && m_selection->get_object_idx() == i) {
                // This means we are flattening this object. In that case pretend
                // that it is not sinking (even if it is), so it is placed on bed
                // later on (whatever is sinking will be left sinking).
                min_zs[{ i, j }] = SINKING_Z_THRESHOLD;
            }
            else
                min_zs[{ i, j }] = obj->instance_bounding_box(j).min.z();

        }
    }

    std::set<std::pair<int, int>> done;  // keeps track of modified instances

    Selection::EMode selection_mode = m_selection->get_mode();

    for (const GLVolume* v : m_volumes.volumes) {
        const int object_idx = v->object_idx();
        if (object_idx < 0 || (int)m_model->objects.size() <= object_idx)
            continue;

        const int instance_idx = v->instance_idx();
        const int volume_idx = v->volume_idx();

        if (volume_idx < 0)
            continue;

        done.insert(std::pair<int, int>(object_idx, instance_idx));

        // Rotate instances/volumes.
        ModelObject* model_object = m_model->objects[object_idx];
        if (model_object != nullptr) {
            if (selection_mode == Selection::Instance)
                model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
            else if (selection_mode == Selection::Volume) {
                if (model_object->volumes[volume_idx]->get_transformation() != v->get_volume_transformation()) {
                	model_object->volumes[volume_idx]->set_transformation(v->get_volume_transformation());
                    // BBS: backup
                    Slic3r::save_object_mesh(*model_object);
                }
            }
            model_object->invalidate_bounding_box();
        }
    }

    //BBS: notify instance updates to part plater list
    m_selection->notify_instance_update(-1, -1);

    // Fixes sinking/flying instances
    for (const std::pair<int, int>& i : done) {
        ModelObject* m = m_model->objects[i.first];

        //BBS: don't call translate if the z is zero
        const double shift_z = m->get_instance_min_z(i.second);
        // leave sinking instances as sinking
        if ((min_zs.find({ i.first, i.second })->second >= SINKING_Z_THRESHOLD || shift_z > SINKING_Z_THRESHOLD)&&(shift_z != 0.0f)) {
            const Vec3d shift(0.0, 0.0, -shift_z);
            m_selection->translate(i.first, i.second, shift);
            m->translate_instance(i.second, shift);
            //BBS: notify instance updates to part plater list
            m_selection->notify_instance_update(i.first, i.second);
        }

        AppAdapter::obj_list()->update_info_items(static_cast<size_t>(i.first));
    }
    //BBS: nofity object list to update
    AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();

    if (!done.empty())
        post_event(SimpleEvent(EVT_GLCANVAS_INSTANCE_ROTATED));

    m_dirty = true;
}

void View3DCanvas::do_scale(const std::string& snapshot_type)
{
    if (m_model == nullptr || !m_selection)
        return;

    if (!snapshot_type.empty())
        AppAdapter::plater()->take_snapshot(snapshot_type);

    // stores current min_z of instances
    std::map<std::pair<int, int>, double> min_zs;
    if (!snapshot_type.empty()) {
        for (int i = 0; i < static_cast<int>(m_model->objects.size()); ++i) {
            const ModelObject* obj = m_model->objects[i];
            for (int j = 0; j < static_cast<int>(obj->instances.size()); ++j) {
                min_zs[{ i, j }] = obj->instance_bounding_box(j).min.z();
            }
        }
    }

    std::set<std::pair<int, int>> done;  // keeps track of modified instances

    Selection::EMode selection_mode = m_selection->get_mode();

    for (const GLVolume* v : m_volumes.volumes) {
        const int object_idx = v->object_idx();
        if (object_idx < 0 || (int)m_model->objects.size() <= object_idx)
            continue;

        const int instance_idx = v->instance_idx();
        const int volume_idx = v->volume_idx();

        if (volume_idx < 0)
            continue;

        done.insert(std::pair<int, int>(object_idx, instance_idx));

        // Rotate instances/volumes
        ModelObject* model_object = m_model->objects[object_idx];
        if (model_object != nullptr) {
            if (selection_mode == Selection::Instance)
                model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
            else if (selection_mode == Selection::Volume) {
                if (model_object->volumes[volume_idx]->get_transformation() != v->get_volume_transformation()) {
                    model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
                    model_object->volumes[volume_idx]->set_transformation(v->get_volume_transformation());
                    // BBS: backup
                    Slic3r::save_object_mesh(*model_object);
                }
            }
            model_object->invalidate_bounding_box();
        }
    }

    //BBS: notify instance updates to part plater list
    m_selection->notify_instance_update(-1, -1);

    // Fixes sinking/flying instances
    for (const std::pair<int, int>& i : done) {
        ModelObject* m = m_model->objects[i.first];

        //BBS: don't call translate if the z is zero
        double shift_z = m->get_instance_min_z(i.second);
        // leave sinking instances as sinking
        if ((min_zs.empty() || min_zs.find({ i.first, i.second })->second >= SINKING_Z_THRESHOLD || shift_z > SINKING_Z_THRESHOLD) && (shift_z != 0.0f)) {
            Vec3d shift(0.0, 0.0, -shift_z);
            m_selection->translate(i.first, i.second, shift);
            m->translate_instance(i.second, shift);
            //BBS: notify instance updates to part plater list
            m_selection->notify_instance_update(i.first, i.second);
        }
        AppAdapter::obj_list()->update_info_items(static_cast<size_t>(i.first));
    }
    //BBS: nofity object list to update
    AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();
    //BBS: notify object info update
    AppAdapter::plater()->show_object_info();

    if (!done.empty())
        post_event(SimpleEvent(EVT_GLCANVAS_INSTANCE_SCALED));

    m_dirty = true;
}

void View3DCanvas::do_center()
{
    if (m_model == nullptr || !m_selection)
        return;

    m_selection->center();
}

void View3DCanvas::do_drop()
{
    if (m_model == nullptr || !m_selection)
        return;

    m_selection->drop();
}

void View3DCanvas::do_center_plate(const int plate_idx) {
    if (m_model == nullptr || !m_selection)
        return;

    m_selection->center_plate(plate_idx);
}

void View3DCanvas::do_mirror(const std::string& snapshot_type)
{
    if (m_model == nullptr || !m_selection)
        return;

    if (!snapshot_type.empty())
        AppAdapter::plater()->take_snapshot(snapshot_type);

    // stores current min_z of instances
    std::map<std::pair<int, int>, double> min_zs;
    if (!snapshot_type.empty()) {
        for (int i = 0; i < static_cast<int>(m_model->objects.size()); ++i) {
            const ModelObject* obj = m_model->objects[i];
            for (int j = 0; j < static_cast<int>(obj->instances.size()); ++j) {
                min_zs[{ i, j }] = obj->instance_bounding_box(j).min.z();
            }
        }
    }

    std::set<std::pair<int, int>> done;  // keeps track of modified instances

    Selection::EMode selection_mode = m_selection->get_mode();

    for (const GLVolume* v : m_volumes.volumes) {
        int object_idx = v->object_idx();
        if (object_idx < 0 || (int)m_model->objects.size() <= object_idx)
            continue;

        int instance_idx = v->instance_idx();
        int volume_idx = v->volume_idx();

        done.insert(std::pair<int, int>(object_idx, instance_idx));

        // Mirror instances/volumes
        ModelObject* model_object = m_model->objects[object_idx];
        if (model_object != nullptr) {
            if (selection_mode == Selection::Instance)
                model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
            else if (selection_mode == Selection::Volume) {
                if (model_object->volumes[volume_idx]->get_transformation() != v->get_volume_transformation()) {
                    model_object->volumes[volume_idx]->set_transformation(v->get_volume_transformation());
                    // BBS: backup
                    Slic3r::save_object_mesh(*model_object);
                }
            }

            model_object->invalidate_bounding_box();
        }
    }

    //BBS: notify instance updates to part plater list
    m_selection->notify_instance_update(-1, -1);

    // Fixes sinking/flying instances
    for (const std::pair<int, int>& i : done) {
        ModelObject* m = m_model->objects[i.first];

        //BBS: don't call translate if the z is zero
        double shift_z = m->get_instance_min_z(i.second);
        // leave sinking instances as sinking
        if ((min_zs.empty() || min_zs.find({ i.first, i.second })->second >= SINKING_Z_THRESHOLD || shift_z > SINKING_Z_THRESHOLD)&&(shift_z != 0.0f)) {
            Vec3d shift(0.0, 0.0, -shift_z);
            m_selection->translate(i.first, i.second, shift);
            m->translate_instance(i.second, shift);
            //BBS: notify instance updates to part plater list
            m_selection->notify_instance_update(i.first, i.second);
        }
        AppAdapter::obj_list()->update_info_items(static_cast<size_t>(i.first));

        //BBS: notify instance updates to part plater list
        PartPlateList &plate_list = AppAdapter::plater()->get_partplate_list();
        plate_list.notify_instance_update(i.first, i.second);

        //BBS: nofity object list to update
        AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();
    }
    //BBS: nofity object list to update
    AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();

    post_event(SimpleEvent(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS));

    m_dirty = true;
}



void View3DCanvas::_on_mouse(wxMouseEvent& evt)
{
    if (!m_initialized || !_set_current() || !m_selection)
        return;

    // BBS: single snapshot
    Plater::SingleSnapshot single(AppAdapter::plater());

#if ENABLE_RETINA_GL
    const float scale = m_retina_helper->get_scale_factor();
    evt.SetX(evt.GetX() * scale);
    evt.SetY(evt.GetY() * scale);
#endif

    Point pos(evt.GetX(), evt.GetY());
    if (evt.LeftDown())
    {
        m_mouse.position = pos.cast<double>();
    }

    ImGuiWrapper& imgui = global_im_gui();
    if (m_tooltip.is_in_imgui() && evt.LeftUp())
        // ignore left up events coming from imgui windows and not processed by them
        m_mouse.ignore_left_up = true;
    m_tooltip.set_in_imgui(false);
    if (imgui.update_mouse_data(evt)) {
        if ((evt.LeftDown() || (evt.Moving() && (evt.AltDown() || evt.ShiftDown()))) && m_canvas != nullptr)
            m_canvas->SetFocus();
        m_mouse.position = evt.Leaving() ? Vec2d(-1.0, -1.0) : pos.cast<double>();
        m_tooltip.set_in_imgui(true);
        render();
#ifdef SLIC3R_DEBUG_MOUSE_EVENTS
        printf((format_mouse_event_debug_message(evt) + " - Consumed by ImGUI\n").c_str());
#endif /* SLIC3R_DEBUG_MOUSE_EVENTS */
        m_dirty = true;
        // do not return if dragging or tooltip not empty to allow for tooltip update
        // also, do not return if the mouse is moving and also is inside MM gizmo to allow update seed fill selection
        if (!m_mouse.dragging && m_tooltip.is_empty())
            return;
    }

#ifdef __WXMSW__
	bool on_enter_workaround = false;
    if (! evt.Entering() && ! evt.Leaving() && m_mouse.position.x() == -1.0) {
        // Workaround for SPE-832: There seems to be a mouse event sent to the window before evt.Entering()
        m_mouse.position = pos.cast<double>();
        render();
#ifdef SLIC3R_DEBUG_MOUSE_EVENTS
		printf((format_mouse_event_debug_message(evt) + " - OnEnter workaround\n").c_str());
#endif /* SLIC3R_DEBUG_MOUSE_EVENTS */
		on_enter_workaround = true;
    } else
#endif /* __WXMSW__ */
    {
#ifdef SLIC3R_DEBUG_MOUSE_EVENTS
		printf((format_mouse_event_debug_message(evt) + " - other\n").c_str());
#endif /* SLIC3R_DEBUG_MOUSE_EVENTS */
    }
    const int selected_object_idx      = m_selection->get_object_idx();
    const int layer_editing_object_idx = is_layers_editing_enabled() ? selected_object_idx : -1;
    const bool mouse_in_layer_editing  = layer_editing_object_idx != -1 && m_layers_editing->bar_rect_contains(*this, pos(0), pos(1));

    if (!mouse_in_layer_editing && m_main_toolbar.on_mouse(evt, *this)) {
        if (m_main_toolbar.is_any_item_pressed())
            m_gizmos->reset_all_states();
        if (evt.LeftUp() || evt.MiddleUp() || evt.RightUp())
            mouse_up_cleanup();
        m_mouse.set_start_position_3D_as_invalid();
        return;
    }

    if (!mouse_in_layer_editing && AppAdapter::plater()->get_collapse_toolbar().on_mouse(evt, *this)) {
        if (evt.LeftUp() || evt.MiddleUp() || evt.RightUp())
            mouse_up_cleanup();
        m_mouse.set_start_position_3D_as_invalid();
        return;
    }

    for (GLVolume* volume : m_volumes.volumes) {
        volume->force_sinking_contours = false;
    }

    auto show_sinking_contours = [this]() {
        const Selection::IndicesList& idxs = m_selection->get_volume_idxs();
        for (unsigned int idx : idxs) {
            m_volumes.volumes[idx]->force_sinking_contours = true;
        }
        m_dirty = true;
    };

    if (!mouse_in_layer_editing && m_gizmos->on_mouse(evt)) {
        if (m_gizmos->is_running()) {
            _deactivate_arrange_menu();
            _deactivate_orient_menu();
            _deactivate_layersediting_menu();
        }
        if (wxWindow::FindFocus() != m_canvas)
            // Grab keyboard focus for input in gizmo dialogs.
            m_canvas->SetFocus();

        if (evt.LeftDown()) {
            // Clear hover state in main toolbar
            wxMouseEvent evt2 = evt;
            evt2.SetEventType(wxEVT_MOTION);
            evt2.SetLeftDown(false);
            m_main_toolbar.on_mouse(evt2, *this);
        }

        if (evt.LeftUp() || evt.MiddleUp() || evt.RightUp())
            mouse_up_cleanup();

        m_mouse.set_start_position_3D_as_invalid();
        m_mouse.position = pos.cast<double>();

        // It should be detection of volume change
        // Not only detection of some modifiers !!!
        if (evt.Dragging()) {
            GLGizmosManager::EType c = m_gizmos->get_current_type();
            if (fff_print()->config().print_sequence == PrintSequence::ByObject) {
                if (c == GLGizmosManager::EType::Move ||
                    c == GLGizmosManager::EType::Scale ||
                    c == GLGizmosManager::EType::Rotate )
                    update_sequential_clearance();
            } else {
                if (c == GLGizmosManager::EType::Move ||
                    c == GLGizmosManager::EType::Scale ||
                    c == GLGizmosManager::EType::Rotate)
                    show_sinking_contours();
            }
        }
        else if (evt.LeftUp() &&
            m_gizmos->get_current_type() == GLGizmosManager::EType::Scale &&
            m_gizmos->get_current()->get_state() == GLGizmoBase::EState::On) {
            AppAdapter::obj_list()->selection_changed();
        }

        return;
    }

    bool any_gizmo_active = m_gizmos->get_current() != nullptr;

    if (m_mouse.drag.move_requires_threshold && m_mouse.is_move_start_threshold_position_2D_defined() && m_mouse.is_move_threshold_met(pos)) {
        m_mouse.drag.move_requires_threshold = false;
        m_mouse.set_move_start_threshold_position_2D_as_invalid();
    }

    if (evt.ButtonDown() && wxWindow::FindFocus() != m_canvas)
        // Grab keyboard focus on any mouse click event.
        m_canvas->SetFocus();

    if (evt.Entering()) {
        if (m_canvas != nullptr) {
            // Only set focus, if the top level window of this canvas is active.
            auto p = dynamic_cast<wxWindow*>(evt.GetEventObject());
            while (p->GetParent())
                p = p->GetParent();
            auto *top_level_wnd = dynamic_cast<wxTopLevelWindow*>(p);
            m_mouse.position = pos.cast<double>();
            m_tooltip_enabled = false;
            // 1) forces a frame render to ensure that m_hover_volume_idxs is updated even when the user right clicks while
            // the context menu is shown, ensuring it to disappear if the mouse is outside any volume and to
            // change the volume hover state if any is under the mouse
            // 2) when switching between 3d view and preview the size of the canvas changes if the side panels are visible,
            // so forces a resize to avoid multiple renders with different sizes (seen as flickering)
            _refresh_if_shown_on_screen();
            m_tooltip_enabled = true;
        }
        m_mouse.set_start_position_2D_as_invalid();
    }
    else if (evt.Leaving()) {
        // to remove hover on objects when the mouse goes out of this canvas
        m_mouse.position = Vec2d(-1.0, -1.0);
        m_dirty = true;
    }
    else if (evt.LeftDClick()) {
        // switch to object panel if double click on object, otherwise switch to global panel if double click on background
        if (selected_object_idx >= 0)
            post_event(SimpleEvent(EVT_GLCANVAS_SWITCH_TO_OBJECT));
        else
            post_event(SimpleEvent(EVT_GLCANVAS_SWITCH_TO_GLOBAL));
    }
    else if (evt.LeftDown() || evt.RightDown() || evt.MiddleDown()) {
        //BBS: add orient deactivate logic
        if (!m_gizmos->on_mouse(evt)) {
            if (_deactivate_arrange_menu() || _deactivate_orient_menu())
                return;
        }

        // If user pressed left or right button we first check whether this happened on a volume or not.
        m_layers_editing->state = LayersEditing::Unknown;
        if (mouse_in_layer_editing) {
            // A volume is selected and the mouse is inside the layer thickness bar.
            // Start editing the layer height.
            m_layers_editing->state = LayersEditing::Editing;
            _perform_layer_editing_action(&evt);
        }

        else {
            // BBS: define Alt key to enable volume selection mode
            m_selection->set_volume_selection_mode(evt.AltDown() ? Selection::Volume : Selection::Instance);
            if (evt.LeftDown() && evt.ShiftDown() && m_picking_enabled && m_layers_editing->state != LayersEditing::Editing) {
                m_rectangle_selection.start_dragging(m_mouse.position, evt.ShiftDown() ? GLSelectionRectangle::Select : GLSelectionRectangle::Deselect);
                m_dirty = true;
            }
            else {
                // Select volume in this 3D canvas.
                // Don't deselect a volume if layer editing is enabled or any gizmo is active. We want the object to stay selected
                // during the scene manipulation.

                if (m_picking_enabled && (!any_gizmo_active || !evt.CmdDown()) && (!m_hover_volume_idxs.empty())) {
                    if (evt.LeftDown() && !m_hover_volume_idxs.empty()) {
                        int volume_idx = get_first_hover_volume_idx();
                        bool already_selected = m_selection->contains_volume(volume_idx);
                        bool ctrl_down = evt.CmdDown();

                        Selection::IndicesList curr_idxs = m_selection->get_volume_idxs();

                        if (already_selected && ctrl_down)
                            m_selection->remove(volume_idx);
                        else {
                            m_selection->add(volume_idx, !ctrl_down, true);
                            m_mouse.drag.move_requires_threshold = !already_selected;
                            if (already_selected)
                                m_mouse.set_move_start_threshold_position_2D_as_invalid();
                            else
                                m_mouse.drag.move_start_threshold_position_2D = pos;
                        }

                        // propagate event through callback
                        if (curr_idxs != m_selection->get_volume_idxs()) {
                            if (m_selection->is_empty())
                                m_gizmos->reset_all_states();
                            else
                                m_gizmos->refresh_on_off_state();

                            m_gizmos->update_data();
                            post_event(SimpleEvent(EVT_GLCANVAS_OBJECT_SELECT));
                            m_dirty = true;
                        }
                    }
                }

                if (!m_hover_volume_idxs.empty()) {
                    if (evt.LeftDown() && m_moving_enabled && m_mouse.drag.move_volume_idx == -1) {
                        // Only accept the initial position, if it is inside the volume bounding box.
                        int volume_idx = get_first_hover_volume_idx();
                        BoundingBoxf3 volume_bbox = m_volumes.volumes[volume_idx]->transformed_bounding_box();
                        volume_bbox.offset(1.0);
                        const bool is_cut_connector_selected = m_selection->is_any_connector();
                        if ((!any_gizmo_active || !evt.CmdDown()) && volume_bbox.contains(m_mouse.scene_position) && !is_cut_connector_selected) {
                            m_volumes.volumes[volume_idx]->hover = GLVolume::HS_None;
                            // The dragging operation is initiated.
                            m_mouse.drag.move_volume_idx = volume_idx;
                            m_selection->setup_cache();
                            m_mouse.drag.start_position_3D = m_mouse.scene_position;
                            m_sequential_print_clearance_first_displacement = true;
                            m_moving = true;
                        }
                    }
                }
            }
        }
    }
    else if (evt.Dragging() && evt.LeftIsDown() && m_mouse.drag.move_volume_idx != -1 && m_layers_editing->state == LayersEditing::Unknown) {
        if (!m_mouse.drag.move_requires_threshold) {
            m_mouse.dragging = true;
            Vec3d cur_pos = m_mouse.drag.start_position_3D;
            // we do not want to translate objects if the user just clicked on an object while pressing shift to remove it from the selection and then drag
            if (m_selection->contains_volume(get_first_hover_volume_idx())) {
                const Camera& camera = AppAdapter::plater()->get_camera();
                if (std::abs(camera.get_dir_forward()(2)) < EPSILON) {
                    // side view -> move selected volumes orthogonally to camera view direction
                    Linef3 ray = mouse_ray(pos);
                    Vec3d dir = ray.unit_vector();
                    // finds the intersection of the mouse ray with the plane parallel to the camera viewport and passing throught the starting position
                    // use ray-plane intersection see i.e. https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection algebric form
                    // in our case plane normal and ray direction are the same (orthogonal view)
                    // when moving to perspective camera the negative z unit axis of the camera needs to be transformed in world space and used as plane normal
                    Vec3d inters = ray.a + (m_mouse.drag.start_position_3D - ray.a).dot(dir) / dir.squaredNorm() * dir;
                    // vector from the starting position to the found intersection
                    Vec3d inters_vec = inters - m_mouse.drag.start_position_3D;

                    Vec3d camera_right = camera.get_dir_right();
                    Vec3d camera_up = camera.get_dir_up();

                    // finds projection of the vector along the camera axes
                    double projection_x = inters_vec.dot(camera_right);
                    double projection_z = inters_vec.dot(camera_up);

                    // apply offset
                    cur_pos = m_mouse.drag.start_position_3D + projection_x * camera_right + projection_z * camera_up;
                }
                else {
                    // Generic view
                    // Get new position at the same Z of the initial click point.
                    float z0 = 0.0f;
                    float z1 = 1.0f;
                    cur_pos = Linef3(_mouse_to_3d(pos, &z0), _mouse_to_3d(pos, &z1)).intersect_plane(m_mouse.drag.start_position_3D(2));
                }
            }

            TransformationType trafo_type;
            trafo_type.set_relative();
            m_selection->translate(cur_pos - m_mouse.drag.start_position_3D, trafo_type);
            if ((fff_print()->config().print_sequence == PrintSequence::ByObject))
                update_sequential_clearance();
            m_dirty = true;
        }
    }
    else if (evt.Dragging() && evt.LeftIsDown() && m_picking_enabled && m_rectangle_selection.is_dragging()) {
        m_rectangle_selection.dragging(pos.cast<double>());
        m_dirty = true;
    }
    else if (evt.Dragging() || is_camera_rotate(evt) || is_camera_pan(evt)) {
        m_mouse.dragging = true;

        if (m_layers_editing->state != LayersEditing::Unknown && layer_editing_object_idx != -1) {
            if (m_layers_editing->state == LayersEditing::Editing) {
                _perform_layer_editing_action(&evt);
                m_mouse.position = pos.cast<double>();
            }
        }
        else 
        {
            m_event_manager.dispatchEvent(evt, this);
        }

    }
    else if ((evt.LeftUp() || evt.MiddleUp() || evt.RightUp()) ||
               (m_camera_movement && !is_camera_rotate(evt) && !is_camera_pan(evt))) {
        m_mouse.position = pos.cast<double>();

        if (evt.LeftUp()) {
            m_rotation_center(0) = m_rotation_center(1) = m_rotation_center(2) = 0.f;
        }

        if (m_layers_editing->state != LayersEditing::Unknown) {
            m_layers_editing->state = LayersEditing::Unknown;
            _stop_timer();
            m_layers_editing->accept_changes(*this);
        }
        else if (m_mouse.drag.move_volume_idx != -1 && m_mouse.dragging) {
            do_move(L("Move Object"));
            post_event(SimpleEvent(EVT_GLCANVAS_MOUSE_DRAGGING_FINISHED));
        }
        else if (evt.LeftUp() && m_picking_enabled && m_rectangle_selection.is_dragging() && m_layers_editing->state != LayersEditing::Editing) {
            if (evt.ShiftDown())
                _update_selection_from_hover();

            m_rectangle_selection.stop_dragging();
        }
        else if (evt.LeftUp() && !m_mouse.ignore_left_up && !m_mouse.dragging && m_hover_volume_idxs.empty() && m_hover_plate_idxs.empty() && !is_layers_editing_enabled()) {
            // deselect and propagate event through callback
            if (!evt.ShiftDown() && (!any_gizmo_active || !evt.CmdDown()) && m_picking_enabled)
                deselect_all();
        }
        //BBS Select plate in this 3D canvas.
        else if (evt.LeftUp() && !m_mouse.dragging && m_picking_enabled && !m_hover_plate_idxs.empty() && (m_canvas_type == CanvasView3D) && !is_layers_editing_enabled())
        {
                int hover_idx = m_hover_plate_idxs.front();
                AppAdapter::plater()->select_plate_by_hover_id(hover_idx);
                //AppAdapter::plater()->get_partplate_list().select_plate_view();
                //deselect all the objects
                if (m_hover_volume_idxs.empty())
                    deselect_all();
        }
        else if (evt.RightUp() && !is_layers_editing_enabled()) {
            // forces a frame render to ensure that m_hover_volume_idxs is updated even when the user right clicks while
            // the context menu is already shown
            render();
            if (!m_hover_volume_idxs.empty()) {
                // if right clicking on volume, propagate event through callback (shows context menu)
                int volume_idx = get_first_hover_volume_idx();
                if (!m_volumes.volumes[volume_idx]->is_wipe_tower)  // disable context menu when the gizmo is open
                {
                    m_selection->add(volume_idx, true, true);
                    m_gizmos->refresh_on_off_state();
                    post_event(SimpleEvent(EVT_GLCANVAS_OBJECT_SELECT));
                    m_gizmos->update_data();
                    render();
                }
            }

            //BBS change plate selection
            if (!m_hover_plate_idxs.empty() && (m_canvas_type == CanvasView3D) && !m_mouse.dragging) {
                int hover_idx = m_hover_plate_idxs.front();
                AppAdapter::plater()->select_plate_by_hover_id(hover_idx, true);
                if (m_hover_volume_idxs.empty())
                    deselect_all();
                render();
            }

            Vec2d logical_pos = pos.cast<double>();
#if ENABLE_RETINA_GL
            const float factor = m_retina_helper->get_scale_factor();
            logical_pos = logical_pos.cwiseQuotient(Vec2d(factor, factor));
#endif // ENABLE_RETINA_GL

            if (!m_mouse.ignore_right_up && m_gizmos->get_current_type() == GLGizmosManager::EType::Undefined) {
                //BBS post right click event
                if (!m_hover_plate_idxs.empty()) {
                    post_event(RBtnPlateEvent(EVT_GLCANVAS_PLATE_RIGHT_CLICK, { logical_pos, m_hover_plate_idxs.front() }));
                }
                else {
                    // do not post the event if the user is panning the scene
                    // or if right click was done over the wipe tower
                    bool post_right_click_event = m_hover_volume_idxs.empty() || !m_volumes.volumes[get_first_hover_volume_idx()]->is_wipe_tower;
                    if (post_right_click_event)
                        post_event(RBtnEvent(EVT_GLCANVAS_RIGHT_CLICK, { logical_pos, m_hover_volume_idxs.empty() }));
                }
            }
        }

        mouse_up_cleanup();
    }
    else if (evt.Moving()) {
        m_mouse.position = pos.cast<double>();

        // updates gizmos overlay
        if (m_selection->is_empty())
            m_gizmos->reset_all_states();

        m_dirty = true;
    }
    else
        evt.Skip();

    // Detection of doubleclick on text to open emboss edit window
    auto type = m_gizmos->get_current_type();
    if (evt.LeftDClick() && !m_hover_volume_idxs.empty() && 
        (type == GLGizmosManager::EType::Undefined ||
         type == GLGizmosManager::EType::Move ||
         type == GLGizmosManager::EType::Rotate ||
         type == GLGizmosManager::EType::Scale) ) {
        for (int hover_volume_id : m_hover_volume_idxs) { 
            const GLVolume &hover_gl_volume = *m_volumes.volumes[hover_volume_id];
            int object_idx = hover_gl_volume.object_idx();
            if (object_idx < 0 || static_cast<size_t>(object_idx) >= m_model->objects.size()) continue;
            const ModelObject* hover_object = m_model->objects[object_idx];
            int hover_volume_idx = hover_gl_volume.volume_idx();
            if (hover_volume_idx < 0 || static_cast<size_t>(hover_volume_idx) >= hover_object->volumes.size()) continue;
            const ModelVolume* hover_volume = hover_object->volumes[hover_volume_idx];

            if (hover_volume->text_configuration.has_value()) {
                m_selection->add_volumes(Selection::EMode::Volume, {(unsigned) hover_volume_id});
/*                if (type != GLGizmosManager::EType::Emboss)
                    m_gizmos->open_gizmo(GLGizmosManager::EType::Emboss);          */  
                AppAdapter::obj_list()->update_selections();
                return;
            } else if (hover_volume->emboss_shape.has_value()) {
                m_selection->add_volumes(Selection::EMode::Volume, {(unsigned) hover_volume_id});
                //if (type != GLGizmosManager::EType::Svg)
                //    m_gizmos->open_gizmo(GLGizmosManager::EType::Svg);
                AppAdapter::obj_list()->update_selections();
                return;
            }
        }
    }

    if (m_moving)
        show_sinking_contours();

#ifdef __WXMSW__
	if (on_enter_workaround)
		m_mouse.position = Vec2d(-1., -1.);
#endif /* __WXMSW__ */
}


void View3DCanvas::reload_scene(bool refresh_immediately, bool force_full_scene_refresh) 
{
if (m_canvas == nullptr || m_config == nullptr || m_model == nullptr)
        return;

    if (!m_initialized)
        return;
    
    _set_current();

    m_hover_volume_idxs.clear();

    struct ModelVolumeState {
        ModelVolumeState(const GLVolume* volume) :
            model_volume(nullptr), geometry_id(volume->geometry_id), volume_idx(-1) {}
        ModelVolumeState(const ModelVolume* model_volume, const ObjectID& instance_id, const GLVolume::CompositeID& composite_id) :
            model_volume(model_volume), geometry_id(std::make_pair(model_volume->id().id, instance_id.id)), composite_id(composite_id), volume_idx(-1) {}
        ModelVolumeState(const ObjectID& volume_id, const ObjectID& instance_id) :
            model_volume(nullptr), geometry_id(std::make_pair(volume_id.id, instance_id.id)), volume_idx(-1) {}
        bool new_geometry() const { return this->volume_idx == size_t(-1); }
        const ModelVolume* model_volume;
        // ObjectID of ModelVolume + ObjectID of ModelInstance
        // or timestamp of an SLAPrintObjectStep + ObjectID of ModelInstance
        std::pair<size_t, size_t>   geometry_id;
        GLVolume::CompositeID       composite_id;
        // Volume index in the new GLVolume vector.
        size_t                      volume_idx;
    };
    std::vector<ModelVolumeState> model_volume_state;
    std::vector<ModelVolumeState> aux_volume_state;

    struct GLVolumeState {
        GLVolumeState() :
            volume_idx(size_t(-1)) {}
        GLVolumeState(const GLVolume* volume, unsigned int volume_idx) :
            composite_id(volume->composite_id), volume_idx(volume_idx) {}
        GLVolumeState(const GLVolume::CompositeID &composite_id) :
            composite_id(composite_id), volume_idx(size_t(-1)) {}

        GLVolume::CompositeID       composite_id;
        // Volume index in the old GLVolume vector.
        size_t                      volume_idx;
    };

    std::vector<size_t> instance_ids_selected;
    std::vector<size_t> map_glvolume_old_to_new(m_volumes.volumes.size(), size_t(-1));
    std::vector<GLVolumeState> deleted_volumes;
    // BBS
    std::vector<GLVolumeState> deleted_wipe_towers;
    std::vector<GLVolume*> glvolumes_new;
    glvolumes_new.reserve(m_volumes.volumes.size());
    auto model_volume_state_lower = [](const ModelVolumeState& m1, const ModelVolumeState& m2) { return m1.geometry_id < m2.geometry_id; };

    m_reload_delayed = !m_canvas->IsShown() && !refresh_immediately && !force_full_scene_refresh;

    // BBS: support wipe tower for multi-plates
    PartPlateList& ppl = AppAdapter::plater()->get_partplate_list();
    int n_plates = ppl.get_plate_count();
    std::vector<int> volume_idxs_wipe_tower_old(n_plates, -1);

    // Release invalidated volumes to conserve GPU memory in case of delayed refresh (see m_reload_delayed).
    // First initialize model_volumes_new_sorted & model_instances_new_sorted.
    for (int object_idx = 0; object_idx < (int)m_model->objects.size(); ++object_idx) {
        const ModelObject* model_object = m_model->objects[object_idx];
        for (int instance_idx = 0; instance_idx < (int)model_object->instances.size(); ++instance_idx) {
            const ModelInstance* model_instance = model_object->instances[instance_idx];
            for (int volume_idx = 0; volume_idx < (int)model_object->volumes.size(); ++volume_idx) {
                const ModelVolume* model_volume = model_object->volumes[volume_idx];
                model_volume_state.emplace_back(model_volume, model_instance->id(), GLVolume::CompositeID(object_idx, volume_idx, instance_idx));
            }
        }
    }

    std::sort(model_volume_state.begin(), model_volume_state.end(), model_volume_state_lower);
    std::sort(aux_volume_state.begin(), aux_volume_state.end(), model_volume_state_lower);

    // BBS: normalize painting data with current filament count
    for (unsigned int obj_idx = 0; obj_idx < (unsigned int)m_model->objects.size(); ++obj_idx) {
        const ModelObject& model_object = *m_model->objects[obj_idx];
        for (int volume_idx = 0; volume_idx < (int)model_object.volumes.size(); ++volume_idx) {
            ModelVolume& model_volume = *model_object.volumes[volume_idx];
            if (!model_volume.is_model_part())
                continue;

            unsigned int filaments_count = (unsigned int)dynamic_cast<const ConfigOptionStrings*>(m_config->option("filament_colour"))->values.size();
            model_volume.update_extruder_count(filaments_count);
        }
    }

    // Release all ModelVolume based GLVolumes not found in the current Model. Find the GLVolume of a hollowed mesh.
    for (size_t volume_id = 0; volume_id < m_volumes.volumes.size(); ++volume_id) {
        GLVolume* volume = m_volumes.volumes[volume_id];
        ModelVolumeState  key(volume);
        ModelVolumeState* mvs = nullptr;
        if (volume->volume_idx() < 0) {
        }
        else {
            auto it = std::lower_bound(model_volume_state.begin(), model_volume_state.end(), key, model_volume_state_lower);
            if (it != model_volume_state.end() && it->geometry_id == key.geometry_id)
                mvs = &(*it);
        }
        // Emplace instance ID of the volume. Both the aux volumes and model volumes share the same instance ID.
        // The wipe tower has its own wipe_tower_instance_id().
        if (m_selection && m_selection->contains_volume(volume_id)) {
            instance_ids_selected.emplace_back(volume->geometry_id.second);
        }
        if (mvs == nullptr || force_full_scene_refresh) {
            // This GLVolume will be released.
            if (volume->is_wipe_tower) {
                // There is only one wipe tower.
                //assert(volume_idx_wipe_tower_old == -1);
                int plate_id = volume->composite_id.object_id - 1000;
                if (plate_id < n_plates)
                    volume_idxs_wipe_tower_old[plate_id] = (int)volume_id;
            }
            if (!m_reload_delayed) {
                deleted_volumes.emplace_back(volume, volume_id);
                // BBS
                if (volume->is_wipe_tower)
                    deleted_wipe_towers.emplace_back(volume, volume_id);
                delete volume;
            }
        }
        else {
            // This GLVolume will be reused.
            map_glvolume_old_to_new[volume_id] = glvolumes_new.size();
            mvs->volume_idx = glvolumes_new.size();
            glvolumes_new.emplace_back(volume);
            // Update color of the volume based on the current extruder.
            if (mvs->model_volume != nullptr) {
                int extruder_id = mvs->model_volume->extruder_id();
                if (extruder_id != -1)
                    volume->extruder_id = extruder_id;

                volume->is_modifier = !mvs->model_volume->is_model_part();
                volume->set_color(color_from_model_volume(*mvs->model_volume));

                ModelInstance* instance = mvs->model_volume->get_object()->instances[mvs->composite_id.instance_id];
                // updates volumes transformations
                volume->set_instance_transformation(instance->get_transformation());
                volume->set_volume_transformation(mvs->model_volume->get_transformation());
                
                volume->mirror_center = instance->get_idex_mirror_center();
                Vec2d ms_offset = PlateBed::get_master_slave_offset();
                volume->copy_offset = Vec3d(ms_offset[0], ms_offset[1], 0);

                // updates volumes convex hull
                if (mvs->model_volume->is_model_part() && ! volume->convex_hull())
                    // Model volume was likely changed from modifier or support blocker / enforcer to a model part.
                    // Only model parts require convex hulls.
                    volume->set_convex_hull(mvs->model_volume->get_convex_hull_shared_ptr());
                volume->set_offset_to_assembly(Vec3d(0, 0, 0));
            
            }
        }
    }
    sort_remove_duplicates(instance_ids_selected);
    auto deleted_volumes_lower = [](const GLVolumeState &v1, const GLVolumeState &v2) { return v1.composite_id < v2.composite_id; };
    std::sort(deleted_volumes.begin(), deleted_volumes.end(), deleted_volumes_lower);

    //BBS clean hover_volume_idxs
    m_hover_volume_idxs.clear();

    if (m_reload_delayed)
        return;

    // BBS: do not check wipe tower changes
    bool update_object_list = false;
    if (deleted_volumes.size() != deleted_wipe_towers.size())
        update_object_list = true;

    if (m_volumes.volumes != glvolumes_new && !update_object_list) {
        int vol_idx = 0;
        for (; vol_idx < std::min(m_volumes.volumes.size(), glvolumes_new.size()); vol_idx++) {
            if (m_volumes.volumes[vol_idx] != glvolumes_new[vol_idx]) {
                update_object_list = true;
                break;
            }
        }
        for (int temp_idx = vol_idx; temp_idx < m_volumes.volumes.size() && !update_object_list; temp_idx++) {
            // Volumes in m_volumes might not exist anymore, so we cannot
            // directly check if they are is_wipe_towers, for which we do
            // not want to update the object list.  Instead, we do a kind of
            // slow thing of seeing if they were in the deleted list, and if
            // so, if they were a wipe tower.
            bool was_deleted_wipe_tower = false;
            for (int del_idx = 0; del_idx < deleted_wipe_towers.size(); del_idx++) {
                if (deleted_wipe_towers[del_idx].volume_idx == temp_idx) {
                    was_deleted_wipe_tower = true;
                    break;
                }
            }
            if (!was_deleted_wipe_tower) {
                update_object_list = true;
            }
        }
        for (int temp_idx = vol_idx; temp_idx < glvolumes_new.size() && !update_object_list; temp_idx++) {
            if (!glvolumes_new[temp_idx]->is_wipe_tower)
                update_object_list = true;
        }
    }
    m_volumes.volumes = std::move(glvolumes_new);
    for (unsigned int obj_idx = 0; obj_idx < (unsigned int)m_model->objects.size(); ++ obj_idx) {
        const ModelObject &model_object = *m_model->objects[obj_idx];
        for (int volume_idx = 0; volume_idx < (int)model_object.volumes.size(); ++ volume_idx) {
			const ModelVolume &model_volume = *model_object.volumes[volume_idx];
            for (int instance_idx = 0; instance_idx < (int)model_object.instances.size(); ++ instance_idx) {
				const ModelInstance &model_instance = *model_object.instances[instance_idx];
				ModelVolumeState key(model_volume.id(), model_instance.id());
				auto it = std::lower_bound(model_volume_state.begin(), model_volume_state.end(), key, model_volume_state_lower);
				assert(it != model_volume_state.end() && it->geometry_id == key.geometry_id);
                if (it->new_geometry()) {
                    // New volume.
                    auto it_old_volume = std::lower_bound(deleted_volumes.begin(), deleted_volumes.end(), GLVolumeState(it->composite_id), deleted_volumes_lower);
                    if (it_old_volume != deleted_volumes.end() && it_old_volume->composite_id == it->composite_id)
                        // If a volume changed its ObjectID, but it reuses a GLVolume's CompositeID, maintain its selection.
                        map_glvolume_old_to_new[it_old_volume->volume_idx] = m_volumes.volumes.size();
                    // Note the index of the loaded volume, so that we can reload the main model GLVolume with the hollowed mesh
                    // later in this function.
                    it->volume_idx = m_volumes.volumes.size();
                    m_volumes.load_object_volume(&model_object, obj_idx, volume_idx, instance_idx, m_color_by, m_initialized, false);
                    m_volumes.volumes.back()->geometry_id = key.geometry_id;
                    update_object_list = true;
                } else {
					// Recycling an old GLVolume.
					GLVolume &existing_volume = *m_volumes.volumes[it->volume_idx];
                    assert(existing_volume.geometry_id == key.geometry_id);
					// Update the Object/Volume/Instance indices into the current Model.
					if (existing_volume.composite_id != it->composite_id) {
						existing_volume.composite_id = it->composite_id;
						update_object_list = true;
					}
                }
            }
        }
    }

    // BBS
    if (m_config->has("filament_colour")) {
        // Should the wipe tower be visualized ?
        unsigned int filaments_count = (unsigned int)dynamic_cast<const ConfigOptionStrings*>(m_config->option("filament_colour"))->values.size();

        bool wt = dynamic_cast<const ConfigOptionBool*>(m_config->option("enable_prime_tower"))->value;
        auto co = dynamic_cast<const ConfigOptionEnum<PrintSequence>*>(m_config->option<ConfigOptionEnum<PrintSequence>>("print_sequence"));

        const DynamicPrintConfig &dconfig           = app_preset_bundle()->prints.get_edited_preset().config;
        auto timelapse_type = dconfig.option<ConfigOptionEnum<TimelapseType>>("timelapse_type");
        bool timelapse_enabled = timelapse_type ? (timelapse_type->value == TimelapseType::tlSmooth) : false;

        if (wt && (timelapse_enabled || filaments_count > 1)) {
            for (int plate_id = 0; plate_id < n_plates; plate_id++) {
                // If print ByObject and there is only one object in the plate, the wipe tower is allowed to be generated.
                PartPlate* part_plate = ppl.get_plate(plate_id);
                if (part_plate->get_print_seq() == PrintSequence::ByObject ||
                    (part_plate->get_print_seq() == PrintSequence::ByDefault && co != nullptr && co->value == PrintSequence::ByObject)) {
                    if (ppl.get_plate(plate_id)->printable_instance_size() != 1)
                        continue;
                }

                DynamicPrintConfig& proj_cfg = app_preset_bundle()->project_config;
                float x = dynamic_cast<const ConfigOptionFloats*>(proj_cfg.option("wipe_tower_x"))->get_at(plate_id);
                float y = dynamic_cast<const ConfigOptionFloats*>(proj_cfg.option("wipe_tower_y"))->get_at(plate_id);
                float w = dynamic_cast<const ConfigOptionFloat*>(m_config->option("prime_tower_width"))->value;
                float a = dynamic_cast<const ConfigOptionFloat*>(proj_cfg.option("wipe_tower_rotation_angle"))->value;
                float tower_brim_width = dynamic_cast<const ConfigOptionFloat*>(m_config->option("prime_tower_brim_width"))->value;
                // BBS
                // float v = dynamic_cast<const ConfigOptionFloat*>(m_config->option("prime_volume"))->value;
                Vec3d plate_origin = ppl.get_plate(plate_id)->get_origin();

                const Print* print = m_process->fff_print();
                const auto& wipe_tower_data = print->wipe_tower_data(filaments_count);
                float brim_width = wipe_tower_data.brim_width;
                const DynamicPrintConfig &print_cfg   = app_preset_bundle()->prints.get_edited_preset().config;
                Vec3d wipe_tower_size = ppl.get_plate(plate_id)->estimate_wipe_tower_size(print_cfg, w, wipe_tower_data.depth);

                const float   margin     = WIPE_TOWER_MARGIN + tower_brim_width;
                BoundingBoxf3 plate_bbox = AppAdapter::plater()->get_partplate_list().get_plate(plate_id)->get_bounding_box();
                coordf_t plate_bbox_x_max_local_coord = plate_bbox.max(0) - plate_origin(0);
                coordf_t plate_bbox_y_max_local_coord = plate_bbox.max(1) - plate_origin(1);
                bool need_update = false;
                if (x + margin + wipe_tower_size(0) > plate_bbox_x_max_local_coord) {
                    x = plate_bbox_x_max_local_coord - wipe_tower_size(0) - margin;
                    need_update = true;
                }
                else if (x < margin) {
                    x = margin;
                    need_update = true;
                }
                if (need_update) {
                    ConfigOptionFloat wt_x_opt(x);
                    dynamic_cast<ConfigOptionFloats *>(proj_cfg.option("wipe_tower_x"))->set_at(&wt_x_opt, plate_id, 0);
                    need_update = false;
                }

                if (y + margin + wipe_tower_size(1) > plate_bbox_y_max_local_coord) {
                    y = plate_bbox_y_max_local_coord - wipe_tower_size(1) - margin;
                    need_update = true;
                }
                else if (y < margin) {
                    y = margin;
                    need_update = true;
                }
                if (need_update) {
                    ConfigOptionFloat wt_y_opt(y);
                    dynamic_cast<ConfigOptionFloats *>(proj_cfg.option("wipe_tower_y"))->set_at(&wt_y_opt, plate_id, 0);
                }

                int volume_idx_wipe_tower_new = m_volumes.load_wipe_tower_preview(
                    1000 + plate_id, x + plate_origin(0), y + plate_origin(1),
                    (float)wipe_tower_size(0), (float)wipe_tower_size(1), (float)wipe_tower_size(2), a,
                    /*!print->is_step_done(psWipeTower)*/ true, brim_width);
                int volume_idx_wipe_tower_old = volume_idxs_wipe_tower_old[plate_id];
                if (volume_idx_wipe_tower_old != -1)
                    map_glvolume_old_to_new[volume_idx_wipe_tower_old] = volume_idx_wipe_tower_new;
            }
        }
    }

    update_volumes_colors_by_extruder();
	// Update selection indices based on the old/new GLVolumeCollection.
    if (m_selection)
    {
        if (m_selection->get_mode() == Selection::Instance)
            m_selection->instances_changed(instance_ids_selected);
        else
            m_selection->volumes_changed(map_glvolume_old_to_new);
    }

    // @Enrico suggest this solution to preven accessing pointer on caster without data
    m_scene_raycaster.remove_raycasters(SceneRaycaster::EType::Bed);
    m_scene_raycaster.remove_raycasters(SceneRaycaster::EType::Volume);
    m_gizmos->update_data();
    m_gizmos->refresh_on_off_state();

    // Update the toolbar
    //BBS: notify the PartPlateList to reload all objects
    if (update_object_list)
        post_event(SimpleEvent(EVT_GLCANVAS_OBJECT_SELECT));

    // checks for geometry outside the print volume to render it accordingly
    if (!m_volumes.empty()) 
    {
        for (auto volume : m_volumes.volumes)
            volume->is_outside = true;

        int traverse_from = AppAdapter::plater()->is_normal_devide_mode() ? 0 : 2;

        std::vector<ModelInstanceEPrintVolumeState> area_states;
        auto& sub_volumes = PlateBed::sub_build_volume();
        bool contained_min_one = false;
        bool partlyOut = false;
        bool fullyOut = true;
        for (int i = traverse_from, count = sub_volumes.size(); i < count; ++i)
        {
            const BuildVolume& sub_volume = sub_volumes[i];
            ModelInstanceEPrintVolumeState state;
            bool area_contained_min_one = m_volumes.check_outside_state(sub_volume, &state);
            contained_min_one |= area_contained_min_one;
            area_states.push_back(state);
        }
        for (ModelInstanceEPrintVolumeState state : area_states)
        {
            partlyOut |= (state == ModelInstanceEPrintVolumeState::ModelInstancePVS_Partly_Outside);
            if (state != ModelInstanceEPrintVolumeState::ModelInstancePVS_Fully_Outside)
                fullyOut = false;
        }
        _set_warning_notification(EWarning::ObjectClashed, partlyOut);
        post_event(Event<bool>(EVT_GLCANVAS_ENABLE_ACTION_BUTTONS,
            contained_min_one && !m_model->objects.empty() && !partlyOut));
    }
    else {
        _set_warning_notification(EWarning::ObjectOutside, false);
        _set_warning_notification(EWarning::ObjectClashed, false);
        _set_warning_notification(EWarning::SlaSupportsOutside, false);
        post_event(Event<bool>(EVT_GLCANVAS_ENABLE_ACTION_BUTTONS, false));
    }


    refresh_camera_scene_box();

    if (m_selection && m_selection->is_empty()) {
        // If no object is selected, deactivate the active gizmo, if any
        // Otherwise it may be shown after cleaning the scene (if it was active while the objects were deleted)
        m_gizmos->reset_all_states();
    }

    // // refresh bed raycasters for picking
    AppAdapter::plater()->get_partplate_list().register_raycasters_for_picking(&m_scene_raycaster);

    // refresh volume raycasters for picking
    for (size_t i = 0; i < m_volumes.volumes.size(); ++i) {
        const GLVolume* v = m_volumes.volumes[i];
        assert(v->mesh_raycaster != nullptr);
        std::shared_ptr<SceneRaycasterItem> raycaster = add_raycaster_for_picking(SceneRaycaster::EType::Volume, i, *v->mesh_raycaster, v->world_matrix());
        raycaster->set_active(v->is_active);
    }

    // refresh gizmo elements raycasters for picking
    GLGizmoBase* curr_gizmo = m_gizmos->get_current();
    if (curr_gizmo != nullptr)
        curr_gizmo->unregister_raycasters_for_picking();
    m_scene_raycaster.remove_raycasters(SceneRaycaster::EType::Gizmo);
    m_scene_raycaster.remove_raycasters(SceneRaycaster::EType::FallbackGizmo);
    if (curr_gizmo != nullptr && m_selection && !m_selection->is_empty())
        curr_gizmo->register_raycasters_for_picking();

    // and force this canvas to be redrawn.
    m_dirty = true;
}

void View3DCanvas::render(bool only_init) 
{
    if (m_in_render) {
        // if called recursively, return
        m_dirty = true;
        return;
    }

    m_in_render = true;
    Slic3r::ScopeGuard in_render_guard([this]() { m_in_render = false; });
    (void)in_render_guard;

    if (m_canvas == nullptr)
        return;

    //BBS: add enable_render
    if (!m_enable_render)
        return;

    // ensures this canvas is current and initialized
    if (!_is_shown_on_screen() || !_set_current() || !init_opengl())
        return;

    if (!is_initialized() && !init())
        return;

    if (m_canvas_type == ECanvasType::CanvasView3D  && m_gizmos->get_current_type() == GLGizmosManager::Undefined) { 

    }

    if (only_init)
        return;

#if ENABLE_ENVIRONMENT_MAP
    AppAdapter::plater()->init_environment_texture();
#endif // ENABLE_ENVIRONMENT_MAP

    const Size& cnv_size = get_canvas_size();
    // Probably due to different order of events on Linux/GTK2, when one switched from 3D scene
    // to preview, this was called before canvas had its final size. It reported zero width
    // and the viewport was set incorrectly, leading to tripping glAsserts further down
    // the road (in apply_projection). That's why the minimum size is forced to 10.
    Camera& camera = AppAdapter::plater()->get_camera();
    camera.set_viewport(0, 0, std::max(10u, (unsigned int)cnv_size.get_width()), std::max(10u, (unsigned int)cnv_size.get_height()));
    apply_viewport(camera);

    if (camera.requires_zoom_to_bed) {
        zoom_to_bed();
        _resize((unsigned int)cnv_size.get_width(), (unsigned int)cnv_size.get_height());
        camera.requires_zoom_to_bed = false;
    }

    if (camera.requires_zoom_to_plate > REQUIRES_ZOOM_TO_PLATE_IDLE) {
        zoom_to_plate(camera.requires_zoom_to_plate);
        _resize((unsigned int)cnv_size.get_width(), (unsigned int)cnv_size.get_height());
        camera.requires_zoom_to_plate = REQUIRES_ZOOM_TO_PLATE_IDLE;
    }

    if (camera.requires_zoom_to_volumes) {
        zoom_to_volumes();
        _resize((unsigned int)cnv_size.get_width(), (unsigned int)cnv_size.get_height());
        camera.requires_zoom_to_volumes = false;
    }

    camera.apply_projection(_max_bounding_box(true, true, true));

    global_im_gui().new_frame();

    if (m_picking_enabled) {
        if (m_rectangle_selection.is_dragging())
            // picking pass using rectangle selection
            _rectangular_selection_picking_pass();
        //BBS: enable picking when no volumes for partplate logic
        //else if (!m_volumes.empty())
        else {
            // regular picking pass
            _picking_pass();

#if ENABLE_RAYCAST_PICKING_DEBUG
            ImGuiWrapper& imgui = global_im_gui();
            imgui.begin(std::string("Hit result"), ImGuiWindowFlags_AlwaysAutoResize);
            imgui.text("Picking disabled");
            imgui.end();
#endif // ENABLE_RAYCAST_PICKING_DEBUG
        }
    }

    // draw scene
    glsafe(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    _render_background();

    //BBS add partplater rendering logic
    bool only_current = false, only_body = false, show_axes = true, no_partplate = false;
    bool show_grid = true;
    GLGizmosManager::EType gizmo_type = m_gizmos->get_current_type();
    if (!m_main_toolbar.is_enabled()) {
        //only_body = true;
        only_current = true;
    }

    /* view3D render*/
    int hover_id = (m_hover_plate_idxs.size() > 0)?m_hover_plate_idxs.front():-1;
    {
        //BBS: add outline logic
        _render_selection();
        if (!no_partplate)
            _render_bed(camera.get_view_matrix(), camera.get_projection_matrix(), !camera.is_looking_downward(), show_axes);
        if (!no_partplate) //BBS: add outline logic
            _render_platelist(camera.get_view_matrix(), camera.get_projection_matrix(), !camera.is_looking_downward(), only_current, only_body, hover_id, true, show_grid);
       
        auto& partplate_list = AppAdapter::plater()->get_partplate_list();
        GLVolumeCollection::ERenderMode render_mode = AppAdapter::plater()->render_mode();
        _render_objects(GLVolumeCollection::ERenderType::Opaque, render_mode, !m_gizmos->is_running());
        _render_objects(GLVolumeCollection::ERenderType::Transparent, render_mode, !m_gizmos->is_running());
    
    }

     _render_sequential_clearance();
    _render_selection_center();

    // we need to set the mouse's scene position here because the depth buffer
    // could be invalidated by the following gizmo render methods
    // this position is used later into on_mouse() to drag the objects
    if (m_picking_enabled)
        m_mouse.scene_position = _mouse_to_3d(m_mouse.position.cast<coord_t>());

    // sidebar hints need to be rendered before the gizmos because the depth buffer
    // could be invalidated by the following gizmo render methods
    _render_selection_sidebar_hints();
    _render_current_gizmo();

#if ENABLE_RAYCAST_PICKING_DEBUG
    if (m_picking_enabled && !m_mouse.dragging && !m_gizmos->is_dragging() && !m_rectangle_selection.is_dragging())
        m_scene_raycaster.render_hit(camera);
#endif // ENABLE_RAYCAST_PICKING_DEBUG

#if ENABLE_SHOW_CAMERA_TARGET
    _render_camera_target();
#endif // ENABLE_SHOW_CAMERA_TARGET

    if (m_picking_enabled && m_rectangle_selection.is_dragging())
        m_rectangle_selection.render(*this);

    // draw overlays
    {
        glsafe(::glDisable(GL_DEPTH_TEST));

        _check_and_update_toolbar_icon_scale();

        // _render_separator_toolbar_right();
        _render_separator_toolbar_left();
        _render_main_toolbar();
        _render_collapse_toolbar();

        //move gizmos behind of main
        _render_gizmos_overlay();

        if (m_layers_editing->last_object_id >= 0 && m_layers_editing->object_max_z() > 0.0f)
            m_layers_editing->render_overlay(*this);

        auto curr_plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
        auto curr_print_seq = curr_plate->get_real_print_seq();
        const Print* print = fff_print();
        bool sequential_print = (curr_print_seq == PrintSequence::ByObject) || print->config().print_order == PrintOrder::AsObjectList;
        std::vector<const ModelInstance*> sorted_instances;
        if (sequential_print) {
            if (print) {
                for (const PrintObject *print_object : print->objects())
                {
                    for (const PrintInstance &instance : print_object->instances())
                    {
                        sorted_instances.emplace_back(instance.model_instance);
                    }
                }
            }
        }
        m_labels.render(sorted_instances);

        _render_3d_navigator();
    }


#if ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW
    if (AppAdapter::plater()->is_view3D_shown())
        AppAdapter::plater()->render_project_state_debug_window();
#endif // ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW

#if ENABLE_CAMERA_STATISTICS
    camera.debug_render();
#endif // ENABLE_CAMERA_STATISTICS

    std::string tooltip;

	// Negative coordinate means out of the window, likely because the window was deactivated.
	// In that case the tooltip should be hidden.
    if (m_mouse.position.x() >= 0. && m_mouse.position.y() >= 0.) {
        if (tooltip.empty())
            tooltip = m_layers_editing->get_tooltip(*this);

	    if (tooltip.empty())
	        tooltip = m_gizmos->get_tooltip();

	    if (tooltip.empty())
	        tooltip = m_main_toolbar.get_tooltip();

	    if (tooltip.empty())
            tooltip = AppAdapter::plater()->get_collapse_toolbar().get_tooltip();
    }

    set_tooltip(tooltip);

    if (m_tooltip_enabled)
        m_tooltip.render(m_mouse.position, *this);

    AppAdapter::plater()->get_mouse3d_controller().render_settings_dialog(*this);

    float right_margin = SLIDER_DEFAULT_RIGHT_MARGIN;
    float bottom_margin = SLIDER_DEFAULT_BOTTOM_MARGIN;
    if (m_canvas_type == ECanvasType::CanvasPreview) {
        float scale_factor = get_scale();
#ifdef WIN32
        int dpi = get_dpi_for_window(AppAdapter::app()->GetTopWindow());
        scale_factor *= (float) dpi / (float) DPI_DEFAULT;
#endif // WIN32
        right_margin = SLIDER_RIGHT_MARGIN * scale_factor * GCODE_VIEWER_SLIDER_SCALE;
        bottom_margin = SLIDER_BOTTOM_MARGIN * scale_factor * GCODE_VIEWER_SLIDER_SCALE;
    }
    get_notification_manager()->render_notifications(*this, get_overlay_window_width(), bottom_margin, right_margin);
    AppAdapter::plater()->get_dailytips()->render();  

    global_im_gui().render();

    m_canvas->SwapBuffers();
    m_render_stats.increment_fps_counter();
}


//BBS: add outline drawing logic
void View3DCanvas::_render_objects(GLVolumeCollection::ERenderType type, GLVolumeCollection::ERenderMode render_mode, bool with_outline)
{
    if (m_volumes.empty())
        return;

    glsafe(::glEnable(GL_DEPTH_TEST));

    m_camera_clipping_plane = m_gizmos->get_clipping_plane();

    if (m_picking_enabled && m_selection)
        // Update the layer editing selection to the first object selected, update the current object maximum Z.
        m_layers_editing->select_object(*m_model, this->is_layers_editing_enabled() ? m_selection->get_object_idx() : -1);

    if (const BuildVolume &build_volume = PlateBed::build_volume(); build_volume.valid()) {
        switch (build_volume.type()) {
        case BuildVolume_Type::Rectangle: {
            const BoundingBox3Base<Vec3d> bed_bb = build_volume.bounding_volume().inflated(BuildVolume::SceneEpsilon);
            m_volumes.set_print_volume({ 0, // Rectangle
                { float(bed_bb.min.x()), float(bed_bb.min.y()), float(bed_bb.max.x()), float(bed_bb.max.y()) },
                { 0.0f, float(build_volume.printable_height()) } });
            break;
        }
        case BuildVolume_Type::Circle: {
            m_volumes.set_print_volume({ 1, // Circle
                { unscaled<float>(build_volume.circle().center.x()), unscaled<float>(build_volume.circle().center.y()), unscaled<float>(build_volume.circle().radius + BuildVolume::SceneEpsilon), 0.0f },
                { 0.0f, float(build_volume.printable_height() + BuildVolume::SceneEpsilon) } });
            break;
        }
        default:
        case BuildVolume_Type::Convex:
        case BuildVolume_Type::Custom: {
            m_volumes.set_print_volume({ static_cast<int>(type),
                { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX },
                { -FLT_MAX, FLT_MAX } }
            );
        }
        }
    }

    if (m_use_clipping_planes)
        m_volumes.set_z_range(-m_clipping_planes[0].get_data()[3], m_clipping_planes[1].get_data()[3]);
    else
        m_volumes.set_z_range(-FLT_MAX, FLT_MAX);

    if (m_gizmos)
    {
        GLGizmoBase* current_gizmo = m_gizmos->get_current();
        if (current_gizmo && !current_gizmo->apply_clipping_plane()) {
            m_volumes.set_clipping_plane(ClippingPlane::ClipsNothing().get_data());
        }
        else {
            m_volumes.set_clipping_plane(m_camera_clipping_plane.get_data());
        }

        m_volumes.set_show_sinking_contours(!m_gizmos->is_hiding_instances());
    }

    GLShaderProgram* shader = get_shader("gouraud");
    ECanvasType canvas_type = this->m_canvas_type;
    if (shader != nullptr) {
        shader->start_using();

        const Size&   cvn_size = get_canvas_size();
        {
            const Camera& camera = AppAdapter::plater()->get_camera();
            shader->set_uniform("z_far", camera.get_far_z());
            shader->set_uniform("z_near", camera.get_near_z());
        }
        switch (type)
        {
        default:
        case GLVolumeCollection::ERenderType::Opaque:
        {
			GLGizmosManager* gm = get_gizmos_manager();
            bool using_layer_editing = m_picking_enabled && m_layers_editing->is_enabled() && (m_layers_editing->last_object_id != -1) && (m_layers_editing->object_max_z() > 0.0f);
            std::function<bool(const GLVolume&)> filter_func;

            int object_id = m_layers_editing->last_object_id;
            if (using_layer_editing) 
            {
                filter_func = [object_id](const GLVolume& volume) {
                    // Which volume to paint without the layer height profile shader?
                    return volume.is_active && (volume.is_modifier || volume.composite_id.object_id != object_id); };
            }
            else
            {
                filter_func = [this, canvas_type](const GLVolume& volume)
                    { return (m_render_sla_auxiliaries || volume.composite_id.volume_id >= 0); };

            }
            // for (auto volume : m_volumes.volumes)
            //     volume->set_render_color();
            const Camera& camera = AppAdapter::plater()->get_camera();
            if (render_mode == GLVolumeCollection::ERenderMode::Mirror)
                // 镜像物体可能反面，这里先不背面剔除， 由于只是渲染几何物体， 一般效率还可以    
                m_volumes.render(type, true, camera.get_view_matrix(), camera.get_projection_matrix(), cvn_size, filter_func, GLVolumeCollection::ERenderMode::Mirror);
            else if (render_mode == GLVolumeCollection::ERenderMode::Copy)
                m_volumes.render(type, false, camera.get_view_matrix(), camera.get_projection_matrix(), cvn_size, filter_func, GLVolumeCollection::ERenderMode::Copy);
            else
                m_volumes.render(type, false, camera.get_view_matrix(), camera.get_projection_matrix(), cvn_size, filter_func, GLVolumeCollection::ERenderMode::Normal);

            if (using_layer_editing)
                m_layers_editing->render_volumes(*this, m_volumes);
			break;
        }
        case GLVolumeCollection::ERenderType::Transparent:
        {
            const Camera& camera = AppAdapter::plater()->get_camera();
            //BBS:add assemble view related logic
            m_volumes.render(type, false, camera.get_view_matrix(), camera.get_projection_matrix(), cvn_size, [this, canvas_type](const GLVolume& volume) {
                    return true;
                });
            break;
        }
        }

        shader->stop_using();
    }

    m_camera_clipping_plane = ClippingPlane::ClipsNothing();
}

void View3DCanvas::_render_selection()
{
    float scale_factor = 1.0;
#if ENABLE_RETINA_GL
    scale_factor = m_retina_helper->get_scale_factor();
#endif // ENABLE_RETINA_GL

    if (!m_gizmos->is_running() && m_selection)
        m_selection->render(scale_factor);
}

void View3DCanvas::_render_sequential_clearance()
{
    if (m_gizmos->is_dragging())
        return;

    m_sequential_print_clearance.render();
}

void View3DCanvas::_render_selection_center()
{
#if ENABLE_RENDER_SELECTION_CENTER
    bool gizmo_is_dragging = m_gizmos->is_dragging();

    if (!m_valid || m_selection->is_empty())
        return;

    GLShaderProgram* shader = get_shader("flat");
    if (shader == nullptr)
        return;

    shader->start_using();

    const Vec3d center = gizmo_is_dragging ? m_selection->m_cache.dragging_center : m_selection->get_bounding_box().center();

    glsafe(::glDisable(GL_DEPTH_TEST));

    const Camera& camera = AppAdapter::plater()->get_camera();
    Transform3d view_model_matrix = camera.get_view_matrix() * Geometry::assemble_transform(center);

    shader->set_uniform("view_model_matrix", view_model_matrix);
    shader->set_uniform("projection_matrix", camera.get_projection_matrix());
	
    m_vbo_sphere.set_color(ColorRGBA::WHITE());
    m_vbo_sphere.render();

    shader->stop_using();  
#endif // ENABLE_RENDER_SELECTION_CENTER  
}

void View3DCanvas::_check_and_update_toolbar_icon_scale()
{
    // Update collapse toolbar
    GLToolbar& collapse_toolbar = AppAdapter::plater()->get_collapse_toolbar();
    collapse_toolbar.set_enabled(AppAdapter::plater()->get_sidebar_docking_state() != Sidebar::None);

    float scale = toolbar_icon_scale() * get_scale();
    Size cnv_size = get_canvas_size();

    //BBS: GUI refactor: GLToolbar
    int size_i = int(GLToolbar::Default_Icons_Size * scale);
    // force even size
    if (size_i % 2 != 0)
        size_i -= 1;
    float size   = size_i;

    // Set current size for all top toolbars. It will be used for next calculations
    //BBS: GUI refactor: GLToolbar
    m_main_toolbar.set_icons_size(size);
    m_separator_toolbar.set_icons_size(size);
    collapse_toolbar.set_icons_size(size / 2.0);
    m_gizmos->set_overlay_icon_size(size);

    //BBS: GUI refactor: GLToolbar
#if BBS_TOOLBAR_ON_TOP
    float collapse_toolbar_width = collapse_toolbar.is_enabled() ? collapse_toolbar.get_width() : 0;

    float top_tb_width = m_main_toolbar.get_width() + m_gizmos->get_scaled_total_width() + m_separator_toolbar.get_width() + collapse_toolbar_width * 2;
    int   items_cnt = m_main_toolbar.get_visible_items_cnt() + m_gizmos->get_selectable_icons_cnt() + m_separator_toolbar.get_visible_items_cnt() + collapse_toolbar.get_visible_items_cnt();
    float noitems_width = top_tb_width - size * items_cnt; // width of separators and borders in top toolbars

    // calculate scale needed for items in all top toolbars
    float new_h_scale = (cnv_size.get_width() - noitems_width) / (items_cnt * GLToolbar::Default_Icons_Size);

    //for protect
    if (new_h_scale <= 0) {
        new_h_scale = 1;
    }

    //use the same value as horizon
    float new_v_scale = new_h_scale;
#else
    float top_tb_width = = collapse_toolbar.get_width();
    int   items_cnt = collapse_toolbar.get_visible_items_cnt();
    float noitems_width = top_tb_width - size * items_cnt; // width of separators and borders in top toolbars

    // calculate scale needed for items in all top toolbars
    float new_h_scale = (cnv_size.get_width() - noitems_width) / (items_cnt * GLToolbar::Default_Icons_Size);

    // calculate scale needed for items in the gizmos toolbar
    items_cnt = m_main_toolbar.get_visible_items_cnt() + m_gizmos->get_selectable_icons_cnt() + m_assemble_view_toolbar.get_visible_items_cnt();
    float new_v_scale = cnv_size.get_height() / (items_cnt * GLGizmosManager::Default_Icons_Size);
#endif

    // set minimum scale as a auto scale for the toolbars
    float new_scale = std::min(new_h_scale, new_v_scale);
    new_scale /= get_scale();
    if (fabs(new_scale - scale) > 0.05) // scale is changed by 5% and more
        set_auto_toolbar_icon_scale(new_scale);
}


void View3DCanvas::_render_current_gizmo() const
{
    //BBS update inv_zoom
    GLGizmoBase::INV_ZOOM = (float)AppAdapter::plater()->get_camera().get_inv_zoom();
    m_gizmos->render_current_gizmo();
}

//BBS: GUI refactor: GLToolbar adjust
//move the size calc to GLCanvas
void View3DCanvas::_render_gizmos_overlay()
{
    m_gizmos->render_overlay();

    if (m_gizmo_highlighter.m_render_arrow)
    {
        m_gizmos->render_arrow(*this, m_gizmo_highlighter.m_gizmo_type);
    }
}

//BBS: GUI refactor: GLToolbar adjust
//when rendering, {0, 0} is at the center, left-up is -0.5, 0.5, right-up is 0.5, -0.5
void View3DCanvas::_render_main_toolbar()
{
    if (!m_main_toolbar.is_enabled())
        return;

    const Size cnv_size = get_canvas_size();
    const float top = 0.5f * (float)cnv_size.get_height();

    const float left = -0.5f * cnv_size.get_width() + get_main_toolbar_offset();
    m_main_toolbar.set_position(top, left);
    m_main_toolbar.render(*this);
    if (m_toolbar_highlighter.m_render_arrow)
        m_main_toolbar.render_arrow(*this, m_toolbar_highlighter.m_toolbar_item);
}


}
}
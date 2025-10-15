#include "SelectionManipulator.hpp"
#include "libslic3r/Model.hpp"
#include "slic3r/Render/Selection.hpp"



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

#if ENABLE_RETINA_GL
#include "slic3r/Utils/RetinaHelper.hpp"
#endif

#include <GL/glew.h>
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Render/RenderUtils.hpp"

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
#include "slic3r/Render/3DScene.hpp"


namespace Slic3r {
namespace GUI {


SelectionManipulator::SelectionManipulator(Model* model, Selection* selection, GLVolumeCollection* volumes)
{
    m_model = model;
    m_selection = selection;
    m_volumes = volumes;
}

void SelectionManipulator::set_model(Model* model)
{
    m_model = model;
}

void SelectionManipulator::set_selection(Selection* selection)
{
    m_selection = selection;
}

void SelectionManipulator::set_volumes(GLVolumeCollection* volumes)
{
    m_volumes = volumes;
}

void SelectionManipulator::do_move(const std::string& snapshot_type)
{
    // if (m_model == nullptr || !m_selection)
    //     return;

    // if (!snapshot_type.empty())
    //     AppAdapter::plater()->take_snapshot(snapshot_type);

    // std::set<std::pair<int, int>> done;  // keeps track of modified instances
    // bool object_moved = false;

    // // BBS: support wipe-tower for multi-plates
    // int n_plates = AppAdapter::plater()->get_partplate_list().get_plate_count();
    // std::vector<Vec3d> wipe_tower_origins(n_plates, Vec3d::Zero());

    // Selection::EMode selection_mode = m_selection->get_mode();

    // for (const GLVolume* v : m_volumes->volumes) {
    //     int object_idx = v->object_idx();
    //     int instance_idx = v->instance_idx();
    //     int volume_idx = v->volume_idx();

    //     if (volume_idx < 0)
    //         continue;

    //     std::pair<int, int> done_id(object_idx, instance_idx);

    //     if (0 <= object_idx && object_idx < (int)m_model->objects.size()) {
    //         done.insert(done_id);

    //         // Move instances/volumes
    //         ModelObject* model_object = m_model->objects[object_idx];
    //         if (model_object != nullptr) {
    //             if (selection_mode == Selection::Instance)
    //                 model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
    //             else if (selection_mode == Selection::Volume) {
    //                 if (model_object->volumes[volume_idx]->get_transformation() != v->get_volume_transformation()) {
    //                     model_object->volumes[volume_idx]->set_transformation(v->get_volume_transformation());
    //                     // BBS: backup
    //                     Slic3r::save_object_mesh(*model_object);
    //                 }
    //             }

    //             object_moved = true;
    //             model_object->invalidate_bounding_box();
    //         }
    //     }
    //     else if (object_idx >= 1000 && object_idx < 1000 + n_plates) {
    //         // Move a wipe tower proxy.
    //         wipe_tower_origins[object_idx - 1000] = v->get_volume_offset();
    //     }
    // }

    // //BBS: notify instance updates to part plater list
    // m_selection->notify_instance_update(-1, 0);

    // // Fixes flying instances
    // for (const std::pair<int, int>& i : done) {
    //     ModelObject* m = m_model->objects[i.first];
    //     const double shift_z = m->get_instance_min_z(i.second);
    //     //BBS: don't call translate if the z is zero
    //     if ((shift_z > SINKING_Z_THRESHOLD) && (shift_z != 0.0f)) {
    //         const Vec3d shift(0.0, 0.0, -shift_z);
    //         m_selection->translate(i.first, i.second, shift);
    //         m->translate_instance(i.second, shift);
    //         //BBS: notify instance updates to part plater list
    //         m_selection->notify_instance_update(i.first, i.second);
    //     }
    //     AppAdapter::obj_list()->update_info_items(static_cast<size_t>(i.first));
    // }
    // //BBS: nofity object list to update
    // AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();

    // if (object_moved)
    //     post_event(SimpleEvent(EVT_GLCANVAS_INSTANCE_MOVED));

    // // BBS: support wipe-tower for multi-plates
    // for (int plate_id = 0; plate_id < wipe_tower_origins.size(); plate_id++) {
    //     Vec3d& wipe_tower_origin = wipe_tower_origins[plate_id];
    //     if (wipe_tower_origin == Vec3d::Zero())
    //         continue;

    //     PartPlateList& ppl = AppAdapter::plater()->get_partplate_list();
    //     DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
    //     Vec3d plate_origin = ppl.get_plate(plate_id)->get_origin();
    //     ConfigOptionFloat wipe_tower_x(wipe_tower_origin(0) - plate_origin(0));
    //     ConfigOptionFloat wipe_tower_y(wipe_tower_origin(1) - plate_origin(1));

    //     ConfigOptionFloats* wipe_tower_x_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_x", true);
    //     ConfigOptionFloats* wipe_tower_y_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_y", true);
    //     wipe_tower_x_opt->set_at(&wipe_tower_x, plate_id, 0);
    //     wipe_tower_y_opt->set_at(&wipe_tower_y, plate_id, 0);
    // }

    // reset_sequential_print_clearance();

    // m_dirty = true;
}

void SelectionManipulator::do_rotate(const std::string& snapshot_type)
{
    // if (m_model == nullptr || !m_selection)
    //    return;

    // if (!snapshot_type.empty())
    //    AppAdapter::plater()->take_snapshot(snapshot_type);

    // // stores current min_z of instances
    // std::map<std::pair<int, int>, double> min_zs;
    // for (int i = 0; i < static_cast<int>(m_model->objects.size()); ++i) {
    //    const ModelObject* obj = m_model->objects[i];
    //    for (int j = 0; j < static_cast<int>(obj->instances.size()); ++j) {
    //        if (snapshot_type == L("Gizmo-Place on Face") && m_selection->get_object_idx() == i) {
    //            // This means we are flattening this object. In that case pretend
    //            // that it is not sinking (even if it is), so it is placed on bed
    //            // later on (whatever is sinking will be left sinking).
    //            min_zs[{ i, j }] = SINKING_Z_THRESHOLD;
    //        }
    //        else
    //            min_zs[{ i, j }] = obj->instance_bounding_box(j).min.z();

    //    }
    // }

    // std::set<std::pair<int, int>> done;  // keeps track of modified instances

    // Selection::EMode selection_mode = m_selection->get_mode();

    // for (const GLVolume* v : m_volumes->volumes) {
    //    const int object_idx = v->object_idx();
    //    if (object_idx < 0 || (int)m_model->objects.size() <= object_idx)
    //        continue;

    //    const int instance_idx = v->instance_idx();
    //    const int volume_idx = v->volume_idx();

    //    if (volume_idx < 0)
    //        continue;

    //    done.insert(std::pair<int, int>(object_idx, instance_idx));

    //    // Rotate instances/volumes.
    //    ModelObject* model_object = m_model->objects[object_idx];
    //    if (model_object != nullptr) {
    //        if (selection_mode == Selection::Instance)
    //            model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
    //        else if (selection_mode == Selection::Volume) {
    //            if (model_object->volumes[volume_idx]->get_transformation() != v->get_volume_transformation()) {
    //            	model_object->volumes[volume_idx]->set_transformation(v->get_volume_transformation());
    //                // BBS: backup
    //                Slic3r::save_object_mesh(*model_object);
    //            }
    //        }
    //        model_object->invalidate_bounding_box();
    //    }
    // }

    // //BBS: notify instance updates to part plater list
    // m_selection->notify_instance_update(-1, -1);

    // // Fixes sinking/flying instances
    // for (const std::pair<int, int>& i : done) {
    //    ModelObject* m = m_model->objects[i.first];

    //    //BBS: don't call translate if the z is zero
    //    const double shift_z = m->get_instance_min_z(i.second);
    //    // leave sinking instances as sinking
    //    if ((min_zs.find({ i.first, i.second })->second >= SINKING_Z_THRESHOLD || shift_z > SINKING_Z_THRESHOLD)&&(shift_z != 0.0f)) {
    //        const Vec3d shift(0.0, 0.0, -shift_z);
    //        m_selection->translate(i.first, i.second, shift);
    //        m->translate_instance(i.second, shift);
    //        //BBS: notify instance updates to part plater list
    //        m_selection->notify_instance_update(i.first, i.second);
    //    }

    //    AppAdapter::obj_list()->update_info_items(static_cast<size_t>(i.first));
    // }
    // //BBS: nofity object list to update
    // AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();

    // if (!done.empty())
    //    post_event(SimpleEvent(EVT_GLCANVAS_INSTANCE_ROTATED));

    // m_dirty = true;
}

void SelectionManipulator::do_scale(const std::string& snapshot_type)
{
    // if (m_model == nullptr || !m_selection)
    //     return;

    // if (!snapshot_type.empty())
    //     AppAdapter::plater()->take_snapshot(snapshot_type);

    // // stores current min_z of instances
    // std::map<std::pair<int, int>, double> min_zs;
    // if (!snapshot_type.empty()) {
    //     for (int i = 0; i < static_cast<int>(m_model->objects.size()); ++i) {
    //         const ModelObject* obj = m_model->objects[i];
    //         for (int j = 0; j < static_cast<int>(obj->instances.size()); ++j) {
    //             min_zs[{ i, j }] = obj->instance_bounding_box(j).min.z();
    //         }
    //     }
    // }

    // std::set<std::pair<int, int>> done;  // keeps track of modified instances

    // Selection::EMode selection_mode = m_selection->get_mode();

    // for (const GLVolume* v : m_volumes.volumes) {
    //     const int object_idx = v->object_idx();
    //     if (object_idx < 0 || (int)m_model->objects.size() <= object_idx)
    //         continue;

    //     const int instance_idx = v->instance_idx();
    //     const int volume_idx = v->volume_idx();

    //     if (volume_idx < 0)
    //         continue;

    //     done.insert(std::pair<int, int>(object_idx, instance_idx));

    //     // Rotate instances/volumes
    //     ModelObject* model_object = m_model->objects[object_idx];
    //     if (model_object != nullptr) {
    //         if (selection_mode == Selection::Instance)
    //             model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
    //         else if (selection_mode == Selection::Volume) {
    //             if (model_object->volumes[volume_idx]->get_transformation() != v->get_volume_transformation()) {
    //                 model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
    //                 model_object->volumes[volume_idx]->set_transformation(v->get_volume_transformation());
    //                 // BBS: backup
    //                 Slic3r::save_object_mesh(*model_object);
    //             }
    //         }
    //         model_object->invalidate_bounding_box();
    //     }
    // }

    // //BBS: notify instance updates to part plater list
    // m_selection->notify_instance_update(-1, -1);

    // // Fixes sinking/flying instances
    // for (const std::pair<int, int>& i : done) {
    //     ModelObject* m = m_model->objects[i.first];

    //     //BBS: don't call translate if the z is zero
    //     double shift_z = m->get_instance_min_z(i.second);
    //     // leave sinking instances as sinking
    //     if ((min_zs.empty() || min_zs.find({ i.first, i.second })->second >= SINKING_Z_THRESHOLD || shift_z > SINKING_Z_THRESHOLD) && (shift_z != 0.0f)) {
    //         Vec3d shift(0.0, 0.0, -shift_z);
    //         m_selection->translate(i.first, i.second, shift);
    //         m->translate_instance(i.second, shift);
    //         //BBS: notify instance updates to part plater list
    //         m_selection->notify_instance_update(i.first, i.second);
    //     }
    //     AppAdapter::obj_list()->update_info_items(static_cast<size_t>(i.first));
    // }
    // //BBS: nofity object list to update
    // AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();
    // //BBS: notify object info update
    // AppAdapter::plater()->show_object_info();

    // if (!done.empty())
    //     post_event(SimpleEvent(EVT_GLCANVAS_INSTANCE_SCALED));

    // m_dirty = true;
}

void SelectionManipulator::do_center()
{
    // if (m_model == nullptr || !m_selection)
    //     return;

    // m_selection->center();
}

void SelectionManipulator::do_drop()
{
    // if (m_model == nullptr || !m_selection)
    //     return;

    // m_selection->drop();
}

void SelectionManipulator::do_center_plate(const int plate_idx) {
    // if (m_model == nullptr || !m_selection)
    //     return;

    // m_selection->center_plate(plate_idx);
}

// void SelectionManipulator::do_mirror(const std::string& snapshot_type)
// {
//     if (m_model == nullptr || !m_selection)
//         return;

//     if (!snapshot_type.empty())
//         AppAdapter::plater()->take_snapshot(snapshot_type);

//     // stores current min_z of instances
//     std::map<std::pair<int, int>, double> min_zs;
//     if (!snapshot_type.empty()) {
//         for (int i = 0; i < static_cast<int>(m_model->objects.size()); ++i) {
//             const ModelObject* obj = m_model->objects[i];
//             for (int j = 0; j < static_cast<int>(obj->instances.size()); ++j) {
//                 min_zs[{ i, j }] = obj->instance_bounding_box(j).min.z();
//             }
//         }
//     }

//     std::set<std::pair<int, int>> done;  // keeps track of modified instances

//     Selection::EMode selection_mode = m_selection->get_mode();

//     for (const GLVolume* v : m_volumes.volumes) {
//         int object_idx = v->object_idx();
//         if (object_idx < 0 || (int)m_model->objects.size() <= object_idx)
//             continue;

//         int instance_idx = v->instance_idx();
//         int volume_idx = v->volume_idx();

//         done.insert(std::pair<int, int>(object_idx, instance_idx));

//         // Mirror instances/volumes
//         ModelObject* model_object = m_model->objects[object_idx];
//         if (model_object != nullptr) {
//             if (selection_mode == Selection::Instance)
//                 model_object->instances[instance_idx]->set_transformation(v->get_instance_transformation());
//             else if (selection_mode == Selection::Volume) {
//                 if (model_object->volumes[volume_idx]->get_transformation() != v->get_volume_transformation()) {
//                     model_object->volumes[volume_idx]->set_transformation(v->get_volume_transformation());
//                     // BBS: backup
//                     Slic3r::save_object_mesh(*model_object);
//                 }
//             }

//             model_object->invalidate_bounding_box();
//         }
//     }

//     //BBS: notify instance updates to part plater list
//     m_selection->notify_instance_update(-1, -1);

//     // Fixes sinking/flying instances
//     for (const std::pair<int, int>& i : done) {
//         ModelObject* m = m_model->objects[i.first];

//         //BBS: don't call translate if the z is zero
//         double shift_z = m->get_instance_min_z(i.second);
//         // leave sinking instances as sinking
//         if ((min_zs.empty() || min_zs.find({ i.first, i.second })->second >= SINKING_Z_THRESHOLD || shift_z > SINKING_Z_THRESHOLD)&&(shift_z != 0.0f)) {
//             Vec3d shift(0.0, 0.0, -shift_z);
//             m_selection->translate(i.first, i.second, shift);
//             m->translate_instance(i.second, shift);
//             //BBS: notify instance updates to part plater list
//             m_selection->notify_instance_update(i.first, i.second);
//         }
//         AppAdapter::obj_list()->update_info_items(static_cast<size_t>(i.first));

//         //BBS: notify instance updates to part plater list
//         PartPlateList &plate_list = AppAdapter::plater()->get_partplate_list();
//         plate_list.notify_instance_update(i.first, i.second);

//         //BBS: nofity object list to update
//         AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();
//     }
//     //BBS: nofity object list to update
//     AppAdapter::plater()->sidebar().obj_list()->update_plate_values_for_items();

//     post_event(SimpleEvent(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS));

//     m_dirty = true;
// }



};
};
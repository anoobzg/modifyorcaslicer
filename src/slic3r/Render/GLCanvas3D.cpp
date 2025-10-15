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
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/Render/PlateBed.hpp"
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
#include "slic3r/GUI/MainFrame.hpp"

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


static constexpr const float TRACKBALLSIZE = 0.8f;
using namespace Slic3r;
using namespace GUI;

static Slic3r::ColorRGBA DEFAULT_BG_LIGHT_COLOR      = { 0.906f, 0.906f, 0.906f, 1.0f };
static Slic3r::ColorRGBA DEFAULT_BG_LIGHT_COLOR_DARK = { 0.329f, 0.329f, 0.353f, 1.0f };
static Slic3r::ColorRGBA ERROR_BG_LIGHT_COLOR        = { 0.753f, 0.192f, 0.039f, 1.0f };
static Slic3r::ColorRGBA ERROR_BG_LIGHT_COLOR_DARK   = { 0.753f, 0.192f, 0.039f, 1.0f };

// Number of floats
static constexpr const size_t MAX_VERTEX_BUFFER_SIZE     = 131072 * 6; // 3.15MB

namespace Slic3r {
namespace GUI {

void canvas_update_render_colors()
{
    DEFAULT_BG_LIGHT_COLOR = ImGuiWrapper::from_ImVec4(RenderColor::colors[RenderCol_3D_Background]);
}

void canvas_load_render_colors()
{
    RenderColor::colors[RenderCol_3D_Background] = ImGuiWrapper::to_ImVec4(DEFAULT_BG_LIGHT_COLOR);
}

#ifdef __WXGTK3__
// wxGTK3 seems to simulate OSX behavior in regard to HiDPI scaling support.
RetinaHelper::RetinaHelper(wxWindow* window) : m_window(window), m_self(nullptr) {}
RetinaHelper::~RetinaHelper() {}
float RetinaHelper::get_scale_factor() { return float(m_window->GetContentScaleFactor()); }
#endif // __WXGTK3__

// Fixed the collision between BuildVolume_Type::Convex and macro Convex defined inside /usr/include/X11/X.h that is included by WxWidgets 3.0.
#if defined(__linux__) && defined(Convex)
#undef Convex
#endif

const double GLCanvas3D::DefaultCameraZoomToBoxMarginFactor = 1.25;
const double GLCanvas3D::DefaultCameraZoomToBedMarginFactor = 2.00;
const double GLCanvas3D::DefaultCameraZoomToPlateMarginFactor = 1.25;

void GLCanvas3D::load_arrange_settings()
{
    std::string dist_fff_str =
        AppAdapter::app_config()->get("arrange", "min_object_distance_fff");

    std::string dist_fff_seq_print_str =
        AppAdapter::app_config()->get("arrange", "min_object_distance_seq_print_fff");

    std::string en_rot_fff_str =
        AppAdapter::app_config()->get("arrange", "enable_rotation_fff");

    std::string en_rot_fff_seqp_str =
        AppAdapter::app_config()->get("arrange", "enable_rotation_seq_print");
    
    std::string en_allow_multiple_materials_str =
        AppAdapter::app_config()->get("arrange", "allow_multi_materials_on_same_plate");
    
    std::string en_avoid_region_str =
        AppAdapter::app_config()->get("arrange", "avoid_extrusion_cali_region");
    
    

    if (!dist_fff_str.empty())
        m_arrange_settings_fff.distance = std::stof(dist_fff_str);

    if (!dist_fff_seq_print_str.empty())
        m_arrange_settings_fff_seq_print.distance = std::stof(dist_fff_seq_print_str);

    if (!en_rot_fff_str.empty())
        m_arrange_settings_fff.enable_rotation = (en_rot_fff_str == "1" || en_rot_fff_str == "true");
    
    if (!en_allow_multiple_materials_str.empty())
        m_arrange_settings_fff.allow_multi_materials_on_same_plate = (en_allow_multiple_materials_str == "1" || en_allow_multiple_materials_str == "true");
    

    if (!en_rot_fff_seqp_str.empty())
        m_arrange_settings_fff_seq_print.enable_rotation = (en_rot_fff_seqp_str == "1" || en_rot_fff_seqp_str == "true");
    
    if(!en_avoid_region_str.empty())
        m_arrange_settings_fff.avoid_extrusion_cali_region = (en_avoid_region_str == "1" || en_avoid_region_str == "true");

    //BBS: add specific arrange settings
    m_arrange_settings_fff_seq_print.is_seq_print = true;
}

GLCanvas3D::ArrangeSettings& GLCanvas3D::get_arrange_settings()
{
    auto* ptr = &m_arrange_settings_fff;

    if (AppAdapter::gui_app()->global_print_sequence() == PrintSequence::ByObject)
        ptr = &m_arrange_settings_fff_seq_print;
    else
        ptr = &m_arrange_settings_fff;

    return *ptr;
}

int GLCanvas3D::GetHoverId()
{
    if (m_hover_plate_idxs.size() == 0) {
        return -1; }
    return m_hover_plate_idxs.front();

}

GLCanvas3D::GLCanvas3D(wxGLCanvas* canvas)
    : m_canvas(canvas)
    , m_context(nullptr)
#if ENABLE_RETINA_GL
    , m_retina_helper(nullptr)
#endif
    , m_in_render(false)
    , m_main_toolbar(GLToolbar::Normal, "Main")
    , m_separator_toolbar(GLToolbar::Normal, "Separator")
    , m_canvas_type(ECanvasType::CanvasView3D)
    , m_gizmos(NULL)
    , m_use_clipping_planes(false)
    , m_sidebar_field("")
    , m_extra_frame_requested(false)
    , m_config(nullptr)
    , m_process(nullptr)
    , m_model(nullptr)
    , m_dirty(true)
    , m_initialized(false)
    , m_apply_zoom_to_volumes_filter(false)
    , m_picking_enabled(false)
    , m_moving_enabled(false)
    , m_dynamic_background_enabled(false)
    , m_multisample_allowed(false)
    , m_moving(false)
    , m_tab_down(false)
    , m_camera_movement(false)
    , m_color_by("volume")
    , m_reload_delayed(false)
    , m_render_sla_auxiliaries(true)
    , m_labels(*this)
    , m_slope(m_volumes)
    , m_selection(NULL)
    // , m_gcode_viewer(new GCodeViewer)
{
    m_layers_editing = new LayersEditing();
    if (m_canvas != nullptr) {
        m_timer.SetOwner(m_canvas);
        m_render_timer.SetOwner(m_canvas);
#if ENABLE_RETINA_GL
        m_retina_helper.reset(new RetinaHelper(canvas));
#endif // ENABLE_RETINA_GL
    }
    m_timer_set_color.Bind(wxEVT_TIMER, &GLCanvas3D::on_set_color_timer, this);
    load_arrange_settings();

}

GLCanvas3D::~GLCanvas3D()
{
    reset_volumes();

    m_sel_plate_toolbar.del_all_item();
    m_sel_plate_toolbar.del_stats_item();
}

void GLCanvas3D::post_event(wxEvent &&event)
{
    event.SetEventObject(m_canvas);
    wxPostEvent(m_canvas, event);
}

void GLCanvas3D::post_event(wxEvent *event)
{
    event->SetEventObject(m_canvas);
    wxPostEvent(m_canvas, *event);
}

bool GLCanvas3D::init()
{
    if (m_initialized)
        return true;

    if (m_canvas == nullptr || m_context == nullptr)
        return false;

    // init dark mode status
    on_change_color_mode(AppAdapter::app_config()->get("dark_color_mode") == "1", false);

    BOOST_LOG_TRIVIAL(info) <<__FUNCTION__<< " enter";
    glsafe(::glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
    glsafe(::glClearDepth(1.0f));

    glsafe(::glDepthFunc(GL_LESS));

    glsafe(::glEnable(GL_DEPTH_TEST));
    glsafe(::glEnable(GL_CULL_FACE));
    glsafe(::glEnable(GL_BLEND));
    glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    if (m_multisample_allowed)
        glsafe(::glEnable(GL_MULTISAMPLE));

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ": before m_layers_editing init";
    if (m_main_toolbar.is_enabled())
        m_layers_editing->init();

    BOOST_LOG_TRIVIAL(info) <<__FUNCTION__<< ": before gizmo init";
    if (m_gizmos && m_gizmos->is_enabled() && !m_gizmos->init())
        std::cout << "Unable to initialize gizmos: please, check that all the required textures are available" << std::endl;

    BOOST_LOG_TRIVIAL(info) <<__FUNCTION__<< ": before _init_toolbars";
    if (!_init_toolbars())
        return false;

    BOOST_LOG_TRIVIAL(info) <<__FUNCTION__<< ": finish _init_toolbars";
    if (m_selection && m_selection->is_enabled() && !m_selection->init())
        return false;

    BOOST_LOG_TRIVIAL(info) <<__FUNCTION__<< ": finish m_selection";

    m_initialized = true;

    // add camera
    // m_event_manager.addEventHandler(std::make_shared<HMS::TrackballManipulator>());
    m_event_manager.addEventHandler(std::make_shared<PreviewEventHandler>());

    return true;
}

void GLCanvas3D::on_change_color_mode(bool is_dark, bool reinit) {
    m_is_dark = is_dark;
    // Bed color
    PlateBed::on_change_color_mode(is_dark);
    // GcodeViewer color
    // m_gcode_viewer->on_change_color_mode(is_dark);
    // ImGui Style
    global_im_gui().on_change_color_mode(is_dark);
    // Notification
    get_notification_manager()->on_change_color_mode(is_dark);
    // DailyTips Window
    AppAdapter::plater()->get_dailytips()->on_change_color_mode(is_dark);
    // Preview Slider
    // IMSlider* m_layers_slider = get_gcode_viewer()->get_layers_slider();
    // IMSlider* m_moves_slider = get_gcode_viewer()->get_moves_slider();
    // m_layers_slider->on_change_color_mode(is_dark);
    // m_moves_slider->on_change_color_mode(is_dark);
    // Partplate
    AppAdapter::plater()->get_partplate_list().on_change_color_mode(is_dark);

    // Toolbar
    if (m_canvas_type == CanvasView3D) {
        m_gizmos->on_change_color_mode(is_dark);
        if (reinit) {
            // reset svg
            _switch_toolbars_icon_filename();
            m_gizmos->switch_gizmos_icon_filename();
            // set dirty to re-generate icon texture
            m_separator_toolbar.set_icon_dirty();
            m_main_toolbar.set_icon_dirty();
            AppAdapter::plater()->get_collapse_toolbar().set_icon_dirty();
            m_gizmos->set_icon_dirty();
        }
    }
}

void GLCanvas3D::set_as_dirty()
{
    m_dirty = true;
}

const float GLCanvas3D::get_scale() const
{
#if ENABLE_RETINA_GL
    return m_retina_helper->get_scale_factor();
#else
    return 1.0f;
#endif
}

unsigned int GLCanvas3D::get_volumes_count() const
{
    return (unsigned int)m_volumes.volumes.size();
}

void GLCanvas3D::reset_volumes()
{
    if (!m_initialized)
        return;

    if (m_volumes.empty())
        return;

    _set_current();

    if (m_selection)
        m_selection->clear();
    m_volumes.clear();
    m_dirty = true;

    _set_warning_notification(EWarning::ObjectOutside, false);
}

//BBS: get current plater's bounding box
BoundingBoxf3 GLCanvas3D::_get_current_partplate_print_volume()
{
    BoundingBoxf3 test_volume;
    if (m_process && m_config)
    {
        BoundingBoxf3 plate_bb = m_process->get_current_plate()->get_bounding_box(false);
        BoundingBoxf3 print_volume({ plate_bb.min(0), plate_bb.min(1), 0.0 }, { plate_bb.max(0), plate_bb.max(1), m_config->opt_float("printable_height") });
        // Allow the objects to protrude below the print bed
        print_volume.min(2) = -1e10;
        print_volume.min(0) -= Slic3r::BuildVolume::BedEpsilon;
        print_volume.min(1) -= Slic3r::BuildVolume::BedEpsilon;
        print_volume.max(0) += Slic3r::BuildVolume::BedEpsilon;
        print_volume.max(1) += Slic3r::BuildVolume::BedEpsilon;
        test_volume = print_volume;
    }
    else
        test_volume = BoundingBoxf3();

    return test_volume;
}

ModelInstanceEPrintVolumeState GLCanvas3D::check_volumes_outside_state() const
{
    //BBS: if not initialized, return inside directly insteadof assert
    if (!m_initialized) {
        return ModelInstancePVS_Inside;
    }
    //assert(m_initialized);

    ModelInstanceEPrintVolumeState state;
    GUI::PartPlate* curr_plate = GUI::AppAdapter::plater()->get_partplate_list().get_selected_plate();
    const std::vector<Pointfs>& pp_bed_shape = curr_plate->get_shape();
    BuildVolume plate_build_volume(pp_bed_shape, PlateBed::build_volume().printable_height());
    m_volumes.check_outside_state(plate_build_volume, &state);
    return state;
}

void GLCanvas3D::toggle_selected_volume_visibility(bool selected_visible)
{
    m_render_sla_auxiliaries = !selected_visible;
    if (selected_visible && m_selection) {
        const Selection::IndicesList &idxs = m_selection->get_volume_idxs();
        if (idxs.size() > 0) {
            for (GLVolume *vol : m_volumes.volumes) {
                if (vol->composite_id.object_id >= 1000 && vol->composite_id.object_id < 1000 + AppAdapter::plater()->get_partplate_list().get_plate_count())
                    continue; // the wipe tower
                if (vol->composite_id.volume_id >= 0) {
                    vol->is_active = false;
                }
            }
            for (unsigned int idx : idxs) {
                GLVolume *v  = const_cast<GLVolume *>(m_selection->get_volume(idx));
                v->is_active = true;
            }
        }
    } else { // show all
        for (GLVolume *vol : m_volumes.volumes) {
            if (vol->composite_id.object_id >= 1000 && vol->composite_id.object_id < 1000 + AppAdapter::plater()->get_partplate_list().get_plate_count())
                continue; // the wipe tower
            if (vol->composite_id.volume_id >= 0) {
                vol->is_active = true;
            }
        }
    }
}

void GLCanvas3D::toggle_model_objects_visibility(bool visible, const ModelObject* mo, int instance_idx, const ModelVolume* mv)
{
    std::vector<std::shared_ptr<SceneRaycasterItem>>* raycasters = get_raycasters_for_picking(SceneRaycaster::EType::Volume);
    for (GLVolume* vol : m_volumes.volumes) {
        // BBS: add partplate logic
        if (vol->composite_id.object_id >= 1000 &&
            vol->composite_id.object_id < 1000 + AppAdapter::plater()->get_partplate_list().get_plate_count()) { // wipe tower
            vol->is_active = (visible && mo == nullptr);
        }
        else {
            if ((mo == nullptr || m_model->objects[vol->composite_id.object_id] == mo)
            && (instance_idx == -1 || vol->composite_id.instance_id == instance_idx)
            && (mv == nullptr || m_model->objects[vol->composite_id.object_id]->volumes[vol->composite_id.volume_id] == mv)) {
                vol->is_active = visible;
                if (!vol->is_modifier)
                    vol->color.a(1.f);

                if (instance_idx == -1) {
                    vol->force_native_color = false;
                    vol->force_neutral_color = false;
                } else {
                    vol->force_native_color = true;
                }
            }
        }

        auto it = std::find_if(raycasters->begin(), raycasters->end(), [vol](std::shared_ptr<SceneRaycasterItem> item) { return item->get_raycaster() == vol->mesh_raycaster.get(); });
        if (it != raycasters->end())
            (*it)->set_active(vol->is_active);
    }

    if (!mo && !visible && !m_model->objects.empty() && (m_model->objects.size() > 1 || m_model->objects.front()->instances.size() > 1))
        _set_warning_notification(EWarning::SomethingNotShown, true);

    if (!mo && visible)
        _set_warning_notification(EWarning::SomethingNotShown, false);
}

void GLCanvas3D::update_instance_printable_state_for_object(const size_t obj_idx)
{
    ModelObject* model_object = m_model->objects[obj_idx];
    for (int inst_idx = 0; inst_idx < (int)model_object->instances.size(); ++inst_idx) {
        ModelInstance* instance = model_object->instances[inst_idx];

        for (GLVolume* volume : m_volumes.volumes) {
            if ((volume->object_idx() == (int)obj_idx) && (volume->instance_idx() == inst_idx))
                volume->printable = instance->printable;
                if (!volume->printable) {
                    volume->render_color = GLVolume::UNPRINTABLE_COLOR;
                }
        }
    }
}

void GLCanvas3D::update_instance_printable_state_for_objects(const std::vector<size_t>& object_idxs)
{
    for (size_t obj_idx : object_idxs)
        update_instance_printable_state_for_object(obj_idx);
}

void GLCanvas3D::set_config(const DynamicPrintConfig* config)
{
    m_config = config;
    m_layers_editing->set_config(config);
    
    // Orca: Filament shrinkage compensation
    const Print *print = fff_print();
    if (print != nullptr)
        m_layers_editing->set_shrinkage_compensation(fff_print()->shrinkage_compensation());
}

void GLCanvas3D::set_process(BackgroundSlicingProcess *process)
{
    m_process = process;
}

void GLCanvas3D::set_model(Model* model)
{
    m_model = model;
    if (m_selection)
        m_selection->set_model(m_model);
}

void GLCanvas3D::set_selection(Selection* selection)
{
    m_selection = selection;
    if (m_selection)
        m_selection->set_volumes(&m_volumes.volumes);
}

void GLCanvas3D::set_gizmos_manager(GLGizmosManager* manager)
{
    m_gizmos = manager;
    m_gizmos->set_enabled(true);
}

GLGizmosManager* GLCanvas3D::get_gizmos_manager()
{
    return m_gizmos;
}

const GLGizmosManager* GLCanvas3D::get_gizmos_manager() const
{
    return m_gizmos;
}

//void GLCanvas3D::set_scene_raycaster(SceneRaycaster* raycaster)
//{
//    m_scene_raycaster = raycaster;
//}

SceneRaycaster* GLCanvas3D::get_scene_raycaster()
{
    return &m_scene_raycaster;
}

void GLCanvas3D::bed_shape_changed()
{
    refresh_camera_scene_box();
    AppAdapter::plater()->get_camera().requires_zoom_to_bed = true;
    m_dirty = true;
}

void GLCanvas3D::plates_count_changed()
{
    refresh_camera_scene_box();
    m_dirty = true;
}

Camera& GLCanvas3D::get_camera()
{
    return camera;
}

void GLCanvas3D::set_color_by(const std::string& value)
{
    m_color_by = value;
}

void GLCanvas3D::refresh_camera_scene_box()
{
    AppAdapter::plater()->get_camera().set_scene_box(scene_bounding_box());
}

BoundingBoxf3 GLCanvas3D::volumes_bounding_box(bool current_plate_only) const
{
    BoundingBoxf3 bb;
    BoundingBoxf3 expand_part_plate_list_box;

    auto        plate_list_box = current_plate_only ? AppAdapter::plater()->get_partplate_list().get_curr_plate()->get_bounding_box() :
                                                        AppAdapter::plater()->get_partplate_list().get_bounding_box();
    auto        horizontal_radius = 0.5 * sqrt(std::pow(plate_list_box.min[0] - plate_list_box.max[0], 2) + std::pow(plate_list_box.min[1] - plate_list_box.max[1], 2));
    const float scale             = 2;
    expand_part_plate_list_box.merge(plate_list_box.min - scale * Vec3d(horizontal_radius, horizontal_radius, 0));
    expand_part_plate_list_box.merge(plate_list_box.max + scale * Vec3d(horizontal_radius, horizontal_radius, 0));

    for (const GLVolume *volume : m_volumes.volumes) {
        if (!m_apply_zoom_to_volumes_filter || ((volume != nullptr) && volume->zoom_to_volumes)) {
            const auto v_bb     = volume->transformed_bounding_box();
            if (!expand_part_plate_list_box.overlap(v_bb))
                continue;
            bb.merge(v_bb);
        }
    }
    return bb;
}

BoundingBoxf3 GLCanvas3D::scene_bounding_box() const
{
    BoundingBoxf3 bb = volumes_bounding_box();
    bb.merge(PlateBed::extended_bounding_box());
    double h = PlateBed::build_volume().printable_height();
    //FIXME why -h?
    bb.min.z() = std::min(bb.min.z(), -h);
    bb.max.z() = std::max(bb.max.z(), h);

    //BBS merge plate scene bounding box
    if (m_canvas_type == ECanvasType::CanvasView3D) {
        PartPlateList& plate = AppAdapter::plater()->get_partplate_list();
        bb.merge(plate.get_bounding_box());
    }

    return bb;
}

BoundingBoxf3 GLCanvas3D::plate_scene_bounding_box(int plate_idx) const
{
    PartPlate* plate = AppAdapter::plater()->get_partplate_list().get_plate(plate_idx);

    BoundingBoxf3 bb = plate->get_bounding_box(true);
    if (m_config != nullptr) {
        double h = m_config->opt_float("printable_height");
        bb.min(2) = std::min(bb.min(2), -h);
        bb.max(2) = std::max(bb.max(2), h);
    }

    return bb;
}

bool GLCanvas3D::is_layers_editing_enabled() const
{
    return m_layers_editing->is_enabled();
}

bool GLCanvas3D::is_layers_editing_allowed() const
{
    return m_layers_editing->is_allowed();
}

void GLCanvas3D::reset_layer_height_profile()
{
    AppAdapter::plater()->take_snapshot("Variable layer height - Reset");
    m_layers_editing->reset_layer_height_profile(*this);
    m_layers_editing->state = LayersEditing::Completed;
    m_dirty = true;
}

void GLCanvas3D::adaptive_layer_height_profile(float quality_factor)
{
    AppAdapter::plater()->take_snapshot("Variable layer height - Adaptive");
    m_layers_editing->adaptive_layer_height_profile(*this, quality_factor);
    m_layers_editing->state = LayersEditing::Completed;
    m_dirty = true;
}

void GLCanvas3D::smooth_layer_height_profile(const HeightProfileSmoothingParams& smoothing_params)
{
    AppAdapter::plater()->take_snapshot("Variable layer height - Smooth all");
    m_layers_editing->smooth_layer_height_profile(*this, smoothing_params);
    m_layers_editing->state = LayersEditing::Completed;
    m_dirty = true;
}

bool GLCanvas3D::is_reload_delayed() const
{
    return m_reload_delayed;
}

void GLCanvas3D::enable_layers_editing(bool enable)
{
    m_layers_editing->set_enabled(enable);
    set_as_dirty();
}

void GLCanvas3D::enable_picking(bool enable)
{
    m_picking_enabled = enable;
}

void GLCanvas3D::enable_moving(bool enable)
{
    m_moving_enabled = enable;
}

void GLCanvas3D::enable_selection(bool enable)
{
    if (m_selection)
        m_selection->set_enabled(enable);
}

void GLCanvas3D::enable_main_toolbar(bool enable)
{
    m_main_toolbar.set_enabled(enable);
}

void GLCanvas3D::reset_select_plate_toolbar_selection() {
    if (m_sel_plate_toolbar.m_all_plates_stats_item)
        m_sel_plate_toolbar.m_all_plates_stats_item->selected = false;
    if (AppAdapter::main_panel())
        AppAdapter::main_panel()->update_slice_print_status(MainPanel::eEventSliceUpdate, true, true);
}

void GLCanvas3D::enable_select_plate_toolbar(bool enable)
{
    m_sel_plate_toolbar.set_enabled(enable);
}

void GLCanvas3D::enable_separator_toolbar(bool enable)
{
    m_separator_toolbar.set_enabled(enable);
}

void GLCanvas3D::enable_dynamic_background(bool enable)
{
    m_dynamic_background_enabled = enable;
}

void GLCanvas3D::allow_multisample(bool allow)
{
    m_multisample_allowed = allow;
}

void GLCanvas3D::zoom_to_bed()
{
    BoundingBoxf3 box = PlateBed::build_volume().bounding_volume();
    box.min.z() = 0.0;
    box.max.z() = 0.0;
    _zoom_to_box(box, DefaultCameraZoomToBedMarginFactor);
}

void GLCanvas3D::zoom_to_volumes()
{
    m_apply_zoom_to_volumes_filter = true;
    _zoom_to_box(volumes_bounding_box());
    m_apply_zoom_to_volumes_filter = false;
}

void GLCanvas3D::zoom_to_selection()
{
    if (m_selection && !m_selection->is_empty())
        _zoom_to_box(m_selection->get_bounding_box());
}

void GLCanvas3D::zoom_to_plate(int plate_idx)
{
    BoundingBoxf3 box;
    if (plate_idx == REQUIRES_ZOOM_TO_ALL_PLATE) {
        box = AppAdapter::plater()->get_partplate_list().get_bounding_box();
        box.min.z() = 0.0;
        box.max.z() = 0.0;
        _zoom_to_box(box, DefaultCameraZoomToPlateMarginFactor);
    } else {
        PartPlate* plate = nullptr;
        if (plate_idx == REQUIRES_ZOOM_TO_CUR_PLATE) {
            plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
        }else {
            assert(plate_idx >= 0 && plate_idx < AppAdapter::plater()->get_partplate_list().get_plate_count());
            plate = AppAdapter::plater()->get_partplate_list().get_plate(plate_idx);
        }
        box = plate->get_bounding_box(true);
        box.min.z() = 0.0;
        box.max.z() = 0.0;
        _zoom_to_box(box, DefaultCameraZoomToPlateMarginFactor);
    }
}

void GLCanvas3D::select_view(const std::string& direction)
{
    AppAdapter::plater()->get_camera().select_view(direction);
    if (m_canvas != nullptr)
        m_canvas->Refresh();
}

void GLCanvas3D::select_plate()
{
    AppAdapter::plater()->get_partplate_list().select_plate_view();
    if (m_canvas != nullptr)
        m_canvas->Refresh();
}

void GLCanvas3D::update_volumes_colors_by_extruder()
{
    if (m_config != nullptr)
        m_volumes.update_colors_by_extruder(m_config);
}

bool GLCanvas3D::is_collapse_toolbar_on_left() const
{
    auto state = AppAdapter::plater()->get_sidebar_docking_state();
    return state == Sidebar::Left;
}

float GLCanvas3D::get_collapse_toolbar_width() const
{
    GLToolbar& collapse_toolbar = AppAdapter::plater()->get_collapse_toolbar();
    const auto state            = AppAdapter::plater()->get_sidebar_docking_state();

    return state != Sidebar::None ? collapse_toolbar.get_width() : 0;
}

float GLCanvas3D::get_collapse_toolbar_height() const
{
    GLToolbar& collapse_toolbar = AppAdapter::plater()->get_collapse_toolbar();
    const auto state            = AppAdapter::plater()->get_sidebar_docking_state();

    return state != Sidebar::None ? collapse_toolbar.get_height() : 0;
}

bool GLCanvas3D::make_current_for_postinit() {
    return _set_current();
}

void GLCanvas3D::render(bool only_init)
{
   
}

void GLCanvas3D::render_thumbnail(ThumbnailData &         thumbnail_data,
                                  unsigned int            w,
                                  unsigned int            h,
                                  const ThumbnailsParams &thumbnail_params,
                                  Camera::EType           camera_type,
                                  bool                    use_top_view,
                                  bool                    for_picking,
                                  bool                    ban_light)
{
    render_thumbnail(thumbnail_data, w, h, thumbnail_params, m_volumes, camera_type, use_top_view, for_picking, ban_light);
}

void GLCanvas3D::render_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params,
                                  const GLVolumeCollection &volumes,
                                  Camera::EType             camera_type,
                                  bool                      use_top_view,
                                  bool                      for_picking,
                                  bool                      ban_light)
{
    GLShaderProgram* shader = nullptr;
    if (for_picking)
        shader = get_shader("flat");
    else
        shader = get_shader("thumbnail");
    ModelObjectPtrs& model_objects = GUI::AppAdapter::gui_app()->model().objects;
    std::vector<ColorRGBA> colors = ::get_extruders_colors();

    if(is_arb_framebuffer())
    { 
        render_thumbnail_framebuffer(thumbnail_data, w, h, thumbnail_params, AppAdapter::plater()->get_partplate_list(), model_objects, volumes, colors, shader, camera_type,
                                     use_top_view, for_picking, ban_light);
    }else if(is_ext_framebuffer())
    { 
        render_thumbnail_framebuffer_ext(thumbnail_data, w, h, thumbnail_params, AppAdapter::plater()->get_partplate_list(), model_objects, volumes, colors, shader, camera_type,
                                         use_top_view, for_picking, ban_light);
    }else{
        render_thumbnail_legacy(thumbnail_data, w, h, thumbnail_params, AppAdapter::plater()->get_partplate_list(), model_objects, volumes, colors, shader, camera_type);
    }
}

void GLCanvas3D::update_plate_thumbnails()
{
    _update_imgui_select_plate_toolbar();
}

void GLCanvas3D::ensure_on_bed(unsigned int object_idx, bool allow_negative_z)
{
    if (allow_negative_z)
        return;

    typedef std::map<std::pair<int, int>, double> InstancesToZMap;
    InstancesToZMap instances_min_z;

    for (GLVolume* volume : m_volumes.volumes) {
        if (volume->object_idx() == (int)object_idx && !volume->is_modifier) {
            double min_z = volume->transformed_convex_hull_bounding_box().min.z();
            std::pair<int, int> instance = std::make_pair(volume->object_idx(), volume->instance_idx());
            InstancesToZMap::iterator it = instances_min_z.find(instance);
            if (it == instances_min_z.end())
                it = instances_min_z.insert(InstancesToZMap::value_type(instance, DBL_MAX)).first;

            it->second = std::min(it->second, min_z);
        }
    }

    for (GLVolume* volume : m_volumes.volumes) {
        std::pair<int, int> instance = std::make_pair(volume->object_idx(), volume->instance_idx());
        InstancesToZMap::iterator it = instances_min_z.find(instance);
        if (it != instances_min_z.end())
            volume->set_instance_offset(Z, volume->get_instance_offset(Z) - it->second);
    }
}


std::vector<double> GLCanvas3D::get_volumes_print_zs(bool active_only) const
{
    return m_volumes.get_current_print_zs(active_only);
}


void GLCanvas3D::set_volumes_z_range(const std::array<double, 2>& range)
{
    m_volumes.set_range(range[0] - 1e-6, range[1] + 1e-6);
}

std::vector<int> GLCanvas3D::load_object(const ModelObject& model_object, int obj_idx, std::vector<int> instance_idxs)
{
    if (instance_idxs.empty()) {
        for (unsigned int i = 0; i < model_object.instances.size(); ++i) {
            instance_idxs.emplace_back(i);
        }
    }
    return m_volumes.load_object(&model_object, obj_idx, instance_idxs, m_color_by, m_initialized);
}

std::vector<int> GLCanvas3D::load_object(const Model& model, int obj_idx)
{
    if (0 <= obj_idx && obj_idx < (int)model.objects.size()) {
        const ModelObject* model_object = model.objects[obj_idx];
        if (model_object != nullptr)
            return load_object(*model_object, obj_idx, std::vector<int>());
    }

    return std::vector<int>();
}

// Reload the 3D scene of
// 1) Model / ModelObjects / ModelInstances / ModelVolumes
// 2) Print bed
// 3) SLA support meshes for their respective ModelObjects / ModelInstances
// 4) Wipe tower preview
// 5) Out of bed collision status & message overlay (texture)
void GLCanvas3D::reload_scene(bool refresh_immediately, bool force_full_scene_refresh)
{
    
}


void GLCanvas3D::bind_event_handlers()
{
    if (m_canvas != nullptr) {
        m_canvas->Bind(wxEVT_SIZE, &GLCanvas3D::on_size, this);
        m_canvas->Bind(wxEVT_IDLE, &GLCanvas3D::on_idle, this);
        m_canvas->Bind(wxEVT_CHAR, &GLCanvas3D::on_char, this);
        m_canvas->Bind(wxEVT_KEY_DOWN, &GLCanvas3D::on_key, this);
        m_canvas->Bind(wxEVT_KEY_UP, &GLCanvas3D::on_key, this);
        m_canvas->Bind(wxEVT_MOUSEWHEEL, &GLCanvas3D::on_mouse_wheel, this);
        m_canvas->Bind(wxEVT_TIMER, &GLCanvas3D::on_timer, this);
        m_canvas->Bind(EVT_GLCANVAS_RENDER_TIMER, &GLCanvas3D::on_render_timer, this);
        m_toolbar_highlighter.set_timer_owner(m_canvas, 0);
        m_canvas->Bind(EVT_GLCANVAS_TOOLBAR_HIGHLIGHTER_TIMER, [this](wxTimerEvent&) { m_toolbar_highlighter.blink(); });
        m_gizmo_highlighter.set_timer_owner(m_canvas, 0);
        m_canvas->Bind(EVT_GLCANVAS_GIZMO_HIGHLIGHTER_TIMER, [this](wxTimerEvent&) { m_gizmo_highlighter.blink(); });
        m_canvas->Bind(wxEVT_LEFT_DOWN, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_LEFT_UP, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_MIDDLE_DOWN, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_MIDDLE_UP, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_RIGHT_DOWN, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_RIGHT_UP, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_MOTION, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_ENTER_WINDOW, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_LEAVE_WINDOW, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_LEFT_DCLICK, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_MIDDLE_DCLICK, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_RIGHT_DCLICK, &GLCanvas3D::on_mouse, this);
        m_canvas->Bind(wxEVT_PAINT, &GLCanvas3D::on_paint, this);
        m_canvas->Bind(wxEVT_SET_FOCUS, &GLCanvas3D::on_set_focus, this);
        m_canvas->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& evt) {
                ImGui::SetWindowFocus(nullptr);
                render();
                evt.Skip();
            });
        m_event_handlers_bound = true;

        m_canvas->Bind(wxEVT_GESTURE_PAN, &GLCanvas3D::on_gesture, this);
        m_canvas->Bind(wxEVT_GESTURE_ZOOM, &GLCanvas3D::on_gesture, this);
        m_canvas->Bind(wxEVT_GESTURE_ROTATE, &GLCanvas3D::on_gesture, this);
        m_canvas->EnableTouchEvents(wxTOUCH_ZOOM_GESTURE | wxTOUCH_ROTATE_GESTURE);
#if __WXOSX__
        initGestures(m_canvas->GetHandle(), m_canvas); // for UIPanGestureRecognizer allowedScrollTypesMask
#endif
    }
}

void GLCanvas3D::unbind_event_handlers()
{
    if (m_canvas != nullptr && m_event_handlers_bound) {
        m_canvas->Unbind(wxEVT_SIZE, &GLCanvas3D::on_size, this);
        m_canvas->Unbind(wxEVT_IDLE, &GLCanvas3D::on_idle, this);
        m_canvas->Unbind(wxEVT_CHAR, &GLCanvas3D::on_char, this);
        m_canvas->Unbind(wxEVT_KEY_DOWN, &GLCanvas3D::on_key, this);
        m_canvas->Unbind(wxEVT_KEY_UP, &GLCanvas3D::on_key, this);
        m_canvas->Unbind(wxEVT_MOUSEWHEEL, &GLCanvas3D::on_mouse_wheel, this);
        m_canvas->Unbind(wxEVT_TIMER, &GLCanvas3D::on_timer, this);
        m_canvas->Unbind(EVT_GLCANVAS_RENDER_TIMER, &GLCanvas3D::on_render_timer, this);
        m_canvas->Unbind(wxEVT_LEFT_DOWN, &GLCanvas3D::on_mouse, this);
		m_canvas->Unbind(wxEVT_LEFT_UP, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_MIDDLE_DOWN, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_MIDDLE_UP, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_RIGHT_DOWN, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_RIGHT_UP, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_MOTION, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_ENTER_WINDOW, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_LEAVE_WINDOW, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_LEFT_DCLICK, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_MIDDLE_DCLICK, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_RIGHT_DCLICK, &GLCanvas3D::on_mouse, this);
        m_canvas->Unbind(wxEVT_PAINT, &GLCanvas3D::on_paint, this);
        m_canvas->Unbind(wxEVT_SET_FOCUS, &GLCanvas3D::on_set_focus, this);
        m_event_handlers_bound = false;

        m_canvas->Unbind(wxEVT_GESTURE_PAN, &GLCanvas3D::on_gesture, this);
        m_canvas->Unbind(wxEVT_GESTURE_ZOOM, &GLCanvas3D::on_gesture, this);
        m_canvas->Unbind(wxEVT_GESTURE_ROTATE, &GLCanvas3D::on_gesture, this);
    }
}

void GLCanvas3D::on_size(wxSizeEvent& evt)
{
    m_dirty = true;
}

void GLCanvas3D::on_idle(wxIdleEvent& evt)
{
    if (!m_initialized)
        return;

    m_dirty |= m_main_toolbar.update_items_state();
    m_dirty |= AppAdapter::plater()->get_collapse_toolbar().update_items_state();
    _update_imgui_select_plate_toolbar();
    bool mouse3d_controller_applied = AppAdapter::plater()->get_mouse3d_controller().apply(AppAdapter::plater()->get_camera());
    m_dirty |= mouse3d_controller_applied;
    //m_dirty |= get_notification_manager()->update_notifications(*this);

    if (m_gizmos)
    {
        auto gizmo = m_gizmos->get_current();
        if (gizmo != nullptr) m_dirty |= gizmo->update_items_state();
    }

    bool imgui_requires_extra_frame = global_im_gui().requires_extra_frame();
    m_dirty |= imgui_requires_extra_frame;

    if (!m_dirty)
        return;

    global_im_gui().reset_requires_extra_frame();

    _refresh_if_shown_on_screen();

    if (m_extra_frame_requested || mouse3d_controller_applied || imgui_requires_extra_frame || global_im_gui().requires_extra_frame()) {
        m_extra_frame_requested = false;
        evt.RequestMore();
    }
    else
        m_dirty = false;
}

void GLCanvas3D::on_char(wxKeyEvent& evt)
{
    if (!m_initialized)
        return;

    // see include/wx/defs.h enum wxKeyCode
    int keyCode = evt.GetKeyCode();
    int ctrlMask = wxMOD_CONTROL;
    int shiftMask = wxMOD_SHIFT;

    ImGuiWrapper& imgui = global_im_gui();
    if (imgui.update_key_data(evt)) {
        render();
        return;
    }

    //BBS: add orient deactivate logic
    if (keyCode == WXK_ESCAPE
        && (_deactivate_arrange_menu() || _deactivate_orient_menu()))
        return;

    if (m_gizmos->on_char(evt))
        return;

    if ((evt.GetModifiers() & ctrlMask) != 0) {
        // CTRL is pressed
        switch (keyCode) {
#ifdef __APPLE__
        case 'a':
        case 'A':
#else /* __APPLE__ */
        case WXK_CONTROL_A:
#endif /* __APPLE__ */
            if (!m_layers_editing->is_enabled())
                post_event(SimpleEvent(EVT_GLCANVAS_SELECT_ALL));
        break;
#ifdef __APPLE__
        case 'c':
        case 'C':
#else /* __APPLE__ */
        case WXK_CONTROL_C:
#endif /* __APPLE__ */
            post_event(SimpleEvent(EVT_GLTOOLBAR_COPY));
        break;
#ifdef __APPLE__
        case 'm':
        case 'M':
#else /* __APPLE__ */
        case WXK_CONTROL_M:
#endif /* __APPLE__ */
        {
#ifdef _WIN32
            if (AppAdapter::app_config()->get("use_legacy_3DConnexion") == "true") {
#endif //_WIN32
#ifdef __APPLE__
            // On OSX use Cmd+Shift+M to "Show/Hide 3Dconnexion devices settings dialog"
            if ((evt.GetModifiers() & shiftMask) != 0) {
#endif // __APPLE__
                Mouse3DController& controller = AppAdapter::plater()->get_mouse3d_controller();
                controller.show_settings_dialog(!controller.is_settings_dialog_shown());
                m_dirty = true;
#ifdef __APPLE__
            }
            else
            // and Cmd+M to minimize application
                AppAdapter::main_panel()->Iconize();
#endif // __APPLE__
#ifdef _WIN32
            }
#endif //_WIN32
            break;
        }
#ifdef __APPLE__
        case 'v':
        case 'V':
#else /* __APPLE__ */
        case WXK_CONTROL_V:
#endif /* __APPLE__ */
            post_event(SimpleEvent(EVT_GLTOOLBAR_PASTE));
        break;

#ifdef __APPLE__
        case 'x':
        case 'X':
#else /* __APPLE__ */
        case WXK_CONTROL_X:
#endif /* __APPLE__ */
            post_event(SimpleEvent(EVT_GLTOOLBAR_CUT));
        break;

#ifdef __APPLE__
        case 'f':
        case 'F':
#else /* __APPLE__ */
        case WXK_CONTROL_F:
#endif /* __APPLE__ */
            break;


#ifdef __APPLE__
        case 'y':
        case 'Y':
#else /* __APPLE__ */
        case WXK_CONTROL_Y:
#endif /* __APPLE__ */
            if (m_canvas_type == CanvasView3D) {
                post_event(SimpleEvent(EVT_GLCANVAS_REDO));
            }
        break;
#ifdef __APPLE__
        case 'z':
        case 'Z':
#else /* __APPLE__ */
        case WXK_CONTROL_Z:
#endif /* __APPLE__ */
            // only support redu/undo in CanvasView3D
            if (m_canvas_type == CanvasView3D) {
                post_event(SimpleEvent(EVT_GLCANVAS_UNDO));
            }
        break;

        // BBS
#ifdef __APPLE__
        case 'E':
        case 'e':
#else /* __APPLE__ */
        case WXK_CONTROL_E:
#endif /* __APPLE__ */
        { m_labels.show(!m_labels.is_shown()); m_dirty = true; break; }
        case '0': {
            select_view("plate");
            zoom_to_bed();
            break; }
        case '1': { select_view("top"); break; }
        case '2': { select_view("bottom"); break; }
        case '3': { select_view("front"); break; }
        case '4': { select_view("rear"); break; }
        case '5': { select_view("left"); break; }
        case '6': { select_view("right"); break; }
        case '7': { select_plate(); break; }

        //case WXK_BACK:
        //case WXK_DELETE:
#ifdef __APPLE__
        case 'd':
        case 'D':
#else /* __APPLE__ */
        case WXK_CONTROL_D:
#endif /* __APPLE__ */
            post_event(SimpleEvent(EVT_GLTOOLBAR_DELETE_ALL));
            break;
#ifdef __APPLE__
        case 'k':
        case 'K':
#else /* __APPLE__ */
        case WXK_CONTROL_K:
#endif /* __APPLE__ */
            post_event(SimpleEvent(EVT_GLTOOLBAR_CLONE));
            break;
        default:            evt.Skip();
        }
    } else {
        auto obj_list = AppAdapter::obj_list();
        switch (keyCode)
        {
        //case WXK_BACK:
        case WXK_DELETE: { post_event(SimpleEvent(EVT_GLTOOLBAR_DELETE)); break; }
        // BBS
#ifdef __APPLE__
        case WXK_BACK: { post_event(SimpleEvent(EVT_GLTOOLBAR_DELETE)); break; }
#endif
        //case WXK_ESCAPE: { deselect_all(); break; }
        case WXK_F5: {
            break;
        }

        // BBS: use keypad to change extruder
        case '1': {
            if (!m_timer_set_color.IsRunning()) {
                m_timer_set_color.StartOnce(500);
                break;
            }
        }
        case '0':   //Color logic for material 10
        case '2':
        case '3':
        case '4':
        case '5':
        case '6': 
        case '7':
        case '8':
        case '9': {
            if (m_timer_set_color.IsRunning()) {
                if (keyCode < '7')  keyCode += 10;
                m_timer_set_color.Stop();
            }
            obj_list->set_extruder_for_selected_items(keyCode - '0');
            break;
        }

        case '?': { post_event(SimpleEvent(EVT_GLCANVAS_QUESTION_MARK)); break; }
        case 'A':
        case 'a':
            {
                if ((evt.GetModifiers() & shiftMask) != 0)
                    post_event(SimpleEvent(EVT_GLCANVAS_ARRANGE_PARTPLATE));
                else
                    post_event(SimpleEvent(EVT_GLCANVAS_ARRANGE));
                break;
            }
        case 'r':
        case 'R':
            {
                if ((evt.GetModifiers() & shiftMask) != 0)
                    post_event(SimpleEvent(EVT_GLCANVAS_ORIENT_PARTPLATE));
                else
                    post_event(SimpleEvent(EVT_GLCANVAS_ORIENT));
                break;
            }
        case 'C':
        case 'c': { toggle_show_gcode_window(); m_dirty = true; request_extra_frame(); break; }
        case 'I':
        case 'i': { _update_camera_zoom(1.0); break; }
        case 'O':
        case 'o': { _update_camera_zoom(-1.0); break; }
        default:  { evt.Skip(); break; }
        }
    }
}

class TranslationProcessor
{
    using UpAction = std::function<void(void)>;
    using DownAction = std::function<void(const Vec3d&, bool, bool)>;

    UpAction m_up_action{ nullptr };
    DownAction m_down_action{ nullptr };

    bool m_running{ false };
    Vec3d m_direction{ Vec3d::UnitX() };

public:
    TranslationProcessor(UpAction up_action, DownAction down_action)
        : m_up_action(up_action), m_down_action(down_action)
    {
    }

    void process(wxKeyEvent& evt)
    {
        const int keyCode = evt.GetKeyCode();
        wxEventType type = evt.GetEventType();
        if (type == wxEVT_KEY_UP) {
            switch (keyCode)
            {
            case WXK_NUMPAD_LEFT:  case WXK_LEFT:
            case WXK_NUMPAD_RIGHT: case WXK_RIGHT:
            case WXK_NUMPAD_UP:    case WXK_UP:
            case WXK_NUMPAD_DOWN:  case WXK_DOWN:
            {
                m_running = false;
                m_up_action();
                break;
            }
            default: { break; }
            }
        }
        else if (type == wxEVT_KEY_DOWN) {
            bool apply = false;

            switch (keyCode)
            {
            case WXK_SHIFT:
            {
                if (m_running)
                    apply = true;

                break;
            }
            case WXK_NUMPAD_LEFT:
            case WXK_LEFT:
            {
                m_direction = -Vec3d::UnitX();
                apply = true;
                break;
            }
            case WXK_NUMPAD_RIGHT:
            case WXK_RIGHT:
            {
                m_direction = Vec3d::UnitX();
                apply = true;
                break;
            }
            case WXK_NUMPAD_UP:
            case WXK_UP:
            {
                m_direction = Vec3d::UnitY();
                apply = true;
                break;
            }
            case WXK_NUMPAD_DOWN:
            case WXK_DOWN:
            {
                m_direction = -Vec3d::UnitY();
                apply = true;
                break;
            }
            default: { break; }
            }

            if (apply) {
                m_running = true;
                m_down_action(m_direction, evt.ShiftDown(), evt.CmdDown());
            }
        }
    }
};

void GLCanvas3D::on_key(wxKeyEvent& evt)
{
    static GLCanvas3D const * thiz = nullptr;
    static TranslationProcessor translationProcessor(nullptr, nullptr);
    if (thiz != this) {
        thiz = this;
        translationProcessor = TranslationProcessor(
        [this]() {
            //do_move(L("Tool Move"));
            //m_gizmos->update_data();

            //post_event(SimpleEvent(EVT_GLCANVAS_MOUSE_DRAGGING_FINISHED));
            //// updates camera target constraints
            //refresh_camera_scene_box();
            //m_dirty = true;
        },
        [this](const Vec3d& direction, bool slow, bool camera_space) {
                if (!m_selection)
                    return;
            m_selection->setup_cache();
            double multiplier = slow ? 1.0 : 10.0;

            Vec3d displacement;
            if (camera_space) {
                Eigen::Matrix<double, 3, 3, Eigen::DontAlign> inv_view_3x3 = AppAdapter::plater()->get_camera().get_view_matrix().inverse().matrix().block(0, 0, 3, 3);
                displacement = multiplier * (inv_view_3x3 * direction);
                displacement.z() = 0.0;
            }
            else
                displacement = multiplier * direction;

            TransformationType trafo_type;
            trafo_type.set_relative();
            m_selection->translate(displacement, trafo_type);
            m_dirty = true;
        }
    );}

    const int keyCode = evt.GetKeyCode();

    ImGuiWrapper& imgui = global_im_gui();
    if (imgui.update_key_data(evt)) {
        render();
    }
    else
    {
        if (m_gizmos && !m_gizmos->on_key(evt)) {
            if (evt.GetEventType() == wxEVT_KEY_UP) {
                if (evt.ShiftDown() && evt.ControlDown() && keyCode == WXK_SPACE) { // fps viewer
#if !BBL_RELEASE_TO_PUBLIC
                    toggle_render_statistic_dialog();
                    m_dirty = true;
#endif
                } else if ((evt.ShiftDown() && evt.ControlDown() && keyCode == WXK_RETURN) ||
                    evt.ShiftDown() && evt.AltDown() && keyCode == WXK_RETURN) {
                    AppAdapter::plater()->toggle_show_wireframe();
                    m_dirty = true;
                }
                else if (m_tab_down && keyCode == WXK_TAB && !evt.HasAnyModifiers()) {
                    // Enable switching between 3D and Preview with Tab
                    // m_canvas->HandleAsNavigationKey(evt);   // XXX: Doesn't work in some cases / on Linux
                    post_event(SimpleEvent(EVT_GLCANVAS_TAB));
                }
                else if (keyCode == WXK_TAB && evt.ShiftDown() && !evt.ControlDown()) {
                    // Collapse side-panel with Shift+Tab
                    post_event(SimpleEvent(EVT_GLCANVAS_COLLAPSE_SIDEBAR));
                }
                else if (keyCode == WXK_SHIFT) {
                    translationProcessor.process(evt);

                    if (m_picking_enabled && m_rectangle_selection.is_dragging()) {
                        _update_selection_from_hover();
                        m_rectangle_selection.stop_dragging();
                        m_mouse.ignore_left_up = true;
                        m_dirty = true;
                    }
                }
                else if (keyCode == WXK_ALT) {
                    if (m_picking_enabled && m_rectangle_selection.is_dragging()) {
                        _update_selection_from_hover();
                        m_rectangle_selection.stop_dragging();
                        m_mouse.ignore_left_up = true;
                        m_dirty = true;
                    }
#ifdef __WXMSW__
                    if (m_camera_movement && m_is_touchpad_navigation) {
                        m_camera_movement = false;
                        m_mouse.set_start_position_3D_as_invalid();
                    }
#endif
                }
                else if (keyCode == WXK_CONTROL)
                    m_dirty = true;
                else if (m_gizmos->is_enabled() && m_selection && !m_selection->is_empty()) {
                    translationProcessor.process(evt);

                    switch (keyCode)
                    {
                    case WXK_NUMPAD_PAGEUP:   case WXK_PAGEUP:
                    case WXK_NUMPAD_PAGEDOWN: case WXK_PAGEDOWN:
                    {
                        //do_rotate(L("Tool Rotate"));
                        //m_gizmos->update_data();

                        //post_event(SimpleEvent(EVT_GLCANVAS_MOUSE_DRAGGING_FINISHED));
                        //// updates camera target constraints
                        //refresh_camera_scene_box();
                        //m_dirty = true;

                        break;
                    }
                    default: { break; }
                    }
                }

                // BBS: add select view logic
                if (evt.ControlDown()) {
                    switch (keyCode) {
                        case '0':
                        case WXK_NUMPAD0: //0 on numpad
                            { select_view("plate");
                              zoom_to_bed();
                            break;
                        }
                        case '1':
                        case WXK_NUMPAD1: //1 on numpad
                            { select_view("top"); break; }
                        case '2':
                        case WXK_NUMPAD2: //2 on numpad
                            { select_view("bottom"); break; }
                        case '3':
                        case WXK_NUMPAD3: //3 on numpad
                            { select_view("front"); break; }
                        case '4':
                        case WXK_NUMPAD4: //4 on numpad
                            { select_view("rear"); break; }
                        case '5':
                        case WXK_NUMPAD5: //5 on numpad
                            { select_view("left"); break; }
                        case '6':
                        case WXK_NUMPAD6: //6 on numpad
                            { select_view("right"); break; }
                        case '7':
                        case WXK_NUMPAD7: //7 on numpad
                            { select_plate(); break; }
                        default: break;
                    }
                }
            }
            else if (evt.GetEventType() == wxEVT_KEY_DOWN) {
                m_tab_down = keyCode == WXK_TAB && !evt.HasAnyModifiers();
                if (keyCode == WXK_SHIFT) {
                    translationProcessor.process(evt);

                    if (m_picking_enabled)
                    {
                        m_mouse.ignore_left_up = false;
                    }
                }
                else if (keyCode == WXK_ALT) {
                    if (m_picking_enabled)
                    {
                        m_mouse.ignore_left_up = false;
                    }
                }
                else if (keyCode == WXK_CONTROL)
                    m_dirty = true;
                else if (m_gizmos->is_enabled() && m_selection && !m_selection->is_empty()) {
                    auto _do_rotate = [this](double angle_z_rad) {
                        m_selection->setup_cache();
                        m_selection->rotate(Vec3d(0.0, 0.0, angle_z_rad), TransformationType(TransformationType::World_Relative_Joint));
                        m_dirty = true;
                    };

                    translationProcessor.process(evt);

                    switch (keyCode)
                    {
                    case WXK_NUMPAD_PAGEUP:   case WXK_PAGEUP:   { _do_rotate(0.25 * M_PI); break; }
                    case WXK_NUMPAD_PAGEDOWN: case WXK_PAGEDOWN: { _do_rotate(-0.25 * M_PI); break; }
                    default: { break; }
                    }
                } else if (!m_gizmos->is_enabled()) {
                    // DoubleSlider navigation in Preview
                    if (m_canvas_type == CanvasPreview) {
                        GCodePreviewCanvas* preview_canvas = static_cast<GCodePreviewCanvas*>(this);
                        preview_canvas->key_handle(evt);
                    }
                }
            }
        }
        else return;
    }

    if (keyCode != WXK_TAB
        && keyCode != WXK_LEFT
        && keyCode != WXK_UP
        && keyCode != WXK_RIGHT
        && keyCode != WXK_DOWN) {
        evt.Skip();   // Needed to have EVT_CHAR generated as well
    }
}

void GLCanvas3D::on_mouse_wheel(wxMouseEvent& evt)
{
#ifdef WIN32
    // Try to filter out spurious mouse wheel events comming from 3D mouse.
    if (AppAdapter::plater()->get_mouse3d_controller().process_mouse_wheel())
        return;
#endif

    if (!m_initialized)
        return;

    // Ignore the wheel events if the middle button is pressed.
    if (evt.MiddleIsDown())
        return;

#if ENABLE_RETINA_GL
    const float scale = m_retina_helper->get_scale_factor();
    evt.SetX(evt.GetX() * scale);
    evt.SetY(evt.GetY() * scale);
#endif

    if (global_im_gui().update_mouse_data(evt)) {
        // if (m_canvas_type == CanvasPreview) {
        //     IMSlider* m_layers_slider = get_gcode_viewer()->get_layers_slider();
        //     IMSlider* m_moves_slider = get_gcode_viewer()->get_moves_slider();
        //     m_layers_slider->on_mouse_wheel(evt);
        //     m_moves_slider->on_mouse_wheel(evt);
        // }
        render();
        m_dirty = true;
        return;
    }

#ifdef __WXMSW__
	// For some reason the Idle event is not being generated after the mouse scroll event in case of scrolling with the two fingers on the touch pad,
	// if the event is not allowed to be passed further.
    // evt.Skip() used to trigger the needed screen refresh, but it does no more. wxWakeUpIdle() seem to work now.
    wxWakeUpIdle();
#endif /* __WXMSW__ */

    // Performs layers editing updates, if enabled
    if (is_layers_editing_enabled() && m_selection) {
        int object_idx_selected = m_selection->get_object_idx();
        if (object_idx_selected != -1) {
            // A volume is selected. Test, whether hovering over a layer thickness bar.
            if (m_layers_editing->bar_rect_contains(*this, (float)evt.GetX(), (float)evt.GetY())) {
                // Adjust the width of the selection.
                m_layers_editing->band_width = std::max(std::min(m_layers_editing->band_width * (1.0f + 0.1f * (float)evt.GetWheelRotation() / (float)evt.GetWheelDelta()), 10.0f), 1.5f);
                if (m_canvas != nullptr)
                    m_canvas->Refresh();

                return;
            }
        }
    }

    // Inform gizmos about the event so they have the opportunity to react.
    if (m_gizmos->on_mouse_wheel(evt))
        return;

    // scale_camera(evt);
    m_event_manager.dispatchEvent(evt, this);
}

void GLCanvas3D::on_timer(wxTimerEvent& evt)
{
    if (m_layers_editing->state == LayersEditing::Editing)
        _perform_layer_editing_action();
}

void GLCanvas3D::on_render_timer(wxTimerEvent& evt)
{
}

void GLCanvas3D::on_set_color_timer(wxTimerEvent& evt)
{
    auto obj_list = AppAdapter::obj_list();
    obj_list->set_extruder_for_selected_items(1);
    m_timer_set_color.Stop();
}


void GLCanvas3D::schedule_extra_frame(int miliseconds)
{
    // Schedule idle event right now
    if (miliseconds == 0)
    {
        // We want to wakeup idle evnt but most likely this is call inside render cycle so we need to wait
        if (m_in_render)
            miliseconds = 33;
        else {
            m_dirty = true;
            wxWakeUpIdle();
            return;
        }
    }
    int remaining_time = m_render_timer.GetInterval();
    // Timer is not running
    if (!m_render_timer.IsRunning()) {
        m_render_timer.StartOnce(miliseconds);
    // Timer is running - restart only if new period is shorter than remaning period
    } else {
        if (miliseconds + 20 < remaining_time) {
            m_render_timer.Stop();
            m_render_timer.StartOnce(miliseconds);
        }
    }
}

#ifndef NDEBUG
// #define SLIC3R_DEBUG_MOUSE_EVENTS
#endif

#ifdef SLIC3R_DEBUG_MOUSE_EVENTS
std::string format_mouse_event_debug_message(const wxMouseEvent &evt)
{
	static int idx = 0;
	char buf[2048];
	std::string out;
	sprintf(buf, "Mouse Event %d - ", idx ++);
	out = buf;

	if (evt.Entering())
		out += "Entering ";
	if (evt.Leaving())
		out += "Leaving ";
	if (evt.Dragging())
		out += "Dragging ";
	if (evt.Moving())
		out += "Moving ";
	if (evt.Magnify())
		out += "Magnify ";
	if (evt.LeftDown())
		out += "LeftDown ";
	if (evt.LeftUp())
		out += "LeftUp ";
	if (evt.LeftDClick())
		out += "LeftDClick ";
	if (evt.MiddleDown())
		out += "MiddleDown ";
	if (evt.MiddleUp())
		out += "MiddleUp ";
	if (evt.MiddleDClick())
		out += "MiddleDClick ";
	if (evt.RightDown())
		out += "RightDown ";
	if (evt.RightUp())
		out += "RightUp ";
	if (evt.RightDClick())
		out += "RightDClick ";
    if (evt.AltDown())
        out += "AltDown ";
    if (evt.ShiftDown())
        out += "ShiftDown ";
    if (evt.ControlDown())
        out += "ControlDown ";

	sprintf(buf, "(%d, %d)", evt.GetX(), evt.GetY());
	out += buf;
	return out;
}
#endif /* SLIC3R_DEBUG_MOUSE_EVENTS */

void GLCanvas3D::on_gesture(wxGestureEvent &evt)
{
    if (!m_initialized || !_set_current())
        return;

    auto & camera = AppAdapter::plater()->get_camera();
    if (evt.GetEventType() == wxEVT_GESTURE_PAN) {
        auto p = evt.GetPosition();
        auto d = static_cast<wxPanGestureEvent&>(evt).GetDelta();
        float z = 0;
        const Vec3d &p2 = _mouse_to_3d({p.x, p.y}, &z);
        const Vec3d &p1 = _mouse_to_3d({p.x - d.x, p.y - d.y}, &z);
        camera.set_target(camera.get_target() + p1 - p2);
    } else if (evt.GetEventType() == wxEVT_GESTURE_ZOOM) {
        static float zoom_start = 1;
        if (evt.IsGestureStart())
            zoom_start = camera.get_zoom();
        camera.set_zoom(zoom_start * static_cast<wxZoomGestureEvent&>(evt).GetZoomFactor());
    } else if (evt.GetEventType() == wxEVT_GESTURE_ROTATE) {
        PartPlate* plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
        bool rotate_limit = true;
        static double last_rotate = 0;
        if (evt.IsGestureStart())
            last_rotate = 0;
        auto rotate = static_cast<wxRotateGestureEvent&>(evt).GetRotationAngle() - last_rotate;
        last_rotate += rotate;
        if (plate)
            camera.rotate_on_sphere_with_target(-rotate, 0, rotate_limit, plate->get_bounding_box().center());
        else
            camera.rotate_on_sphere(-rotate, 0, rotate_limit);
        camera.auto_type(Camera::EType::Perspective);
    }
    m_dirty = true;
}

void GLCanvas3D::on_mouse(wxMouseEvent& evt)
{
    _on_mouse(evt);  
}

void GLCanvas3D::on_paint(wxPaintEvent& evt)
{
    if (m_initialized)
        m_dirty = true;
    else
        // Call render directly, so it gets initialized immediately, not from On Idle handler.
        this->render();
}

void GLCanvas3D::force_set_focus() {
    m_canvas->SetFocus();
};

void GLCanvas3D::on_set_focus(wxFocusEvent& evt)
{
    m_tooltip_enabled = false;
    if (m_canvas_type == ECanvasType::CanvasPreview) {
        // update thumbnails and update plate toolbar
        AppAdapter::plater()->update_all_plate_thumbnails();
        _update_imgui_select_plate_toolbar();
    }
    _refresh_if_shown_on_screen();
    m_tooltip_enabled = true;
    m_is_touchpad_navigation = AppAdapter::app_config()->get_bool("camera_navigation_style");
}

bool GLCanvas3D::is_camera_rotate(const wxMouseEvent& evt) const
{
    if (m_is_touchpad_navigation) {
        return evt.Moving() && evt.AltDown() && !evt.ShiftDown();
    } else {
        return evt.Dragging() && evt.LeftIsDown();
    }
}

bool GLCanvas3D::is_camera_pan(const wxMouseEvent& evt) const
{
    if (m_is_touchpad_navigation) {
        return evt.Moving() && evt.ShiftDown() && !evt.AltDown();
    } else {
        return evt.Dragging() && (evt.MiddleIsDown() || evt.RightIsDown());
    }
}

void GLCanvas3D::rotate_gizmos_camera(const wxMouseEvent& evt)
{
    bool any_gizmo_active = m_gizmos->get_current() != nullptr;
    Point pos(evt.GetX(), evt.GetY());

    bool can_rotate = true; // (any_gizmo_active || m_hover_volume_idxs.empty())
    bool is_gizmos = true; // if (this->m_canvas_type == ECanvasType::CanvasAssembleView || m_gizmos->get_current_type() == GLGizmosManager::FdmSupports ||  m_gizmos->get_current_type() == GLGizmosManager::Seam || m_gizmos->get_current_type() == GLGizmosManager::MmuSegmentation) {

    if (!can_rotate || !is_gizmos || !m_mouse.is_start_position_3D_defined())
    {
        m_camera_movement = true;
        m_mouse.drag.start_position_3D = Vec3d((double)pos(0), (double)pos(1), 0.0);
        return;
    }

    Camera* camera = get_camera_ptr();
    const Vec3d rot = (Vec3d(pos.x(), pos.y(), 0.) - m_mouse.drag.start_position_3D) * (PI * TRACKBALLSIZE / 180.);

    Vec3d rotate_target = rotate_center();
    camera->rotate_on_sphere_with_target(rot.x(), rot.y(), false, rotate_target);

    camera->auto_type(Camera::EType::Perspective);

    m_dirty = true;

    m_camera_movement = true;
    m_mouse.drag.start_position_3D = Vec3d((double)pos(0), (double)pos(1), 0.0);
}


void GLCanvas3D::rotate_camera(const wxMouseEvent& evt)
{
    bool any_gizmo_active = m_gizmos->get_current() != nullptr;
    Point pos(evt.GetX(), evt.GetY());

    bool can_rotate = true; // (any_gizmo_active || m_hover_volume_idxs.empty())

    if (!can_rotate || !m_mouse.is_start_position_3D_defined())
    {
        m_camera_movement = true;
        m_mouse.drag.start_position_3D = Vec3d((double)pos(0), (double)pos(1), 0.0);
        return;
    }

    Camera* camera = get_camera_ptr();
    const Vec3d rot = (Vec3d(pos.x(), pos.y(), 0.) - m_mouse.drag.start_position_3D) * (PI * TRACKBALLSIZE / 180.);

    if (use_free_camera())
        // Virtual track ball (similar to the 3DConnexion mouse).
        camera->rotate_local_around_target(Vec3d(rot.y(), rot.x(), 0.));
    else {
        // Forces camera right vector to be parallel to XY plane in case it has been misaligned using the 3D mouse free rotation.
        // It is cheaper to call this function right away instead of testing AppAdapter::plater()->get_mouse3d_controller().connected(),
        // which checks an atomics (flushes CPU caches).
        // See GH issue #3816.
        bool rotate_limit = true;

        camera->recover_from_free_camera();
        if (evt.ControlDown() || evt.CmdDown()) {
            if ((m_rotation_center.x() == 0.f) && (m_rotation_center.y() == 0.f) && (m_rotation_center.z() == 0.f)) {
                auto canvas_w = float(get_canvas_size().get_width());
                auto canvas_h = float(get_canvas_size().get_height());
                Point screen_center(canvas_w/2, canvas_h/2);
                m_rotation_center = _mouse_to_3d(screen_center);
                m_rotation_center(2) = 0.f;
            }
            camera->rotate_on_sphere_with_target(rot.x(), rot.y(), rotate_limit, m_rotation_center);
        } else {
            Vec3d rotate_target = rotate_center();
            if (!rotate_target.isZero())
                camera->rotate_on_sphere_with_target(rot.x(), rot.y(), rotate_limit, rotate_target);
            else
                camera->rotate_on_sphere(rot.x(), rot.y(), rotate_limit);
        }
    }
    camera->auto_type(Camera::EType::Perspective);

    m_dirty = true;

    m_camera_movement = true;
    m_mouse.drag.start_position_3D = Vec3d((double)pos(0), (double)pos(1), 0.0);
}

void GLCanvas3D::pan_camera(const wxMouseEvent& evt)
{
    Point pos(evt.GetX(), evt.GetY());
      // If dragging over blank area with right button, pan.
    if (m_mouse.is_start_position_2D_defined()) {
        // get point in model space at Z = 0
        float z = 0.0f;
        const Vec3d& cur_pos = _mouse_to_3d(pos, &z);
        Vec3d orig = _mouse_to_3d(m_mouse.drag.start_position_2D, &z);
        Camera* camera = get_camera_ptr();
        if (use_free_camera()) {
            camera->recover_from_free_camera();
        }

        camera->set_target(camera->get_target() + orig - cur_pos);
        m_dirty = true;
        m_mouse.ignore_right_up = true;
    }

    m_camera_movement = true;
    m_mouse.drag.start_position_2D = pos;

}

void GLCanvas3D::scale_camera(const wxMouseEvent& evt)
{
        // Calculate the zoom delta and apply it to the current zoom factor
    double direction_factor = AppAdapter::app_config()->get_bool("reverse_mouse_wheel_zoom") ? -1.0 : 1.0;
    auto delta = direction_factor * (double)evt.GetWheelRotation() / (double)evt.GetWheelDelta();
    bool zoom_to_mouse = AppAdapter::app_config()->get("zoom_to_mouse") == "true";
    if (!zoom_to_mouse) {// zoom to center
        _update_camera_zoom(delta);
    }
    else {
        auto cnv_size = get_canvas_size();
        float z{0.f};
        auto screen_center_3d_pos = _mouse_to_3d({ cnv_size.get_width() * 0.5, cnv_size.get_height() * 0.5 }, &z);
        auto mouse_3d_pos = _mouse_to_3d({evt.GetX(), evt.GetY()}, &z);
        Vec3d displacement = mouse_3d_pos - screen_center_3d_pos;

        Camera* camera = get_camera_ptr();
        camera->translate(displacement);
        auto origin_zoom = camera->get_zoom();
        _update_camera_zoom(delta);
        auto new_zoom = camera->get_zoom();
        camera->translate((-displacement) / (new_zoom / origin_zoom));
    }
}

void GLCanvas3D::imgui_handle(const wxMouseEvent& evt)
{

}

Vec3d GLCanvas3D::rotate_center()
{
    Vec3d rotate_target = Vec3d::Zero();
    // if (m_canvas_type == ECanvasType::CanvasPreview) {
    //     PartPlate *plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
    //     if (plate)
    //         rotate_target = plate->get_bounding_box().center();
    // }
    // else {
    // }
    if (m_selection && !m_selection->is_empty())
        rotate_target = m_selection->get_bounding_box().center();
    else
        rotate_target = volumes_bounding_box().center();

    return rotate_target;
}

Camera* GLCanvas3D::get_camera_ptr()
{
    return AppAdapter::plater()->get_camera_ptr();
}

bool GLCanvas3D::use_free_camera()
{
    if (AppAdapter::app_config()->get_bool("use_free_camera"))
            return true;

    return false;
}

MouseHelper* GLCanvas3D::get_mouse()
{
    return &m_mouse;
}

Size GLCanvas3D::get_canvas_size() const
{
    int w = 0;
    int h = 0;

    if (m_canvas != nullptr)
        m_canvas->GetSize(&w, &h);

#if ENABLE_RETINA_GL
    const float factor = m_retina_helper->get_scale_factor();
    w *= factor;
    h *= factor;
#else
    const float factor = 1.0f;
#endif

    return Size(w, h, factor);
}

Vec2d GLCanvas3D::get_local_mouse_position() const
{
    if (m_canvas == nullptr)
		return Vec2d::Zero();

    wxPoint mouse_pos = m_canvas->ScreenToClient(wxGetMousePosition());
    const double factor =
#if ENABLE_RETINA_GL
        m_retina_helper->get_scale_factor();
#else
        1.0;
#endif
    return Vec2d(factor * mouse_pos.x, factor * mouse_pos.y);
}

void GLCanvas3D::set_tooltip(const std::string& tooltip)
{
    if (m_canvas != nullptr)
        m_tooltip.set_text(tooltip);
}

void GLCanvas3D::handle_sidebar_focus_event(const std::string& opt_key, bool focus_on)
{
    m_sidebar_field = focus_on ? opt_key : "";

    m_dirty = true;
}

void GLCanvas3D::handle_layers_data_focus_event(const t_layer_height_range range, const EditorType type)
{
    std::string field = "layer_" + std::to_string(type) + "_" + std::to_string(range.first) + "_" + std::to_string(range.second);
    m_gizmos->reset_all_states();
    handle_sidebar_focus_event(field, true);
}

void GLCanvas3D::update_ui_from_settings()
{
    m_dirty = true;

#if __APPLE__
    // Update OpenGL scaling on OSX after the user toggled the "use_retina_opengl" settings in Preferences dialog.
    const float orig_scaling = m_retina_helper->get_scale_factor();

    const bool use_retina = true;
    BOOST_LOG_TRIVIAL(debug) << "GLCanvas3D: Use Retina OpenGL: " << use_retina;
    m_retina_helper->set_use_retina(use_retina);
    const float new_scaling = m_retina_helper->get_scale_factor();

    if (new_scaling != orig_scaling) {
        BOOST_LOG_TRIVIAL(debug) << "GLCanvas3D: Scaling factor: " << new_scaling;

        Camera& camera = AppAdapter::plater()->get_camera();
        camera.set_zoom(camera.get_zoom() * new_scaling / orig_scaling);
        _refresh_if_shown_on_screen();
    }
#endif // ENABLE_RETINA_GL
}

// BBS: add partplate logic
WipeTowerInfoHelper GLCanvas3D::get_wipe_tower_info(int plate_idx) const
{
    WipeTowerInfoHelper wti;

    for (const GLVolume* vol : m_volumes.volumes) {
        if (vol->is_wipe_tower && vol->object_idx() - 1000 == plate_idx) {
            DynamicPrintConfig& proj_cfg = app_preset_bundle()->project_config;
            wti.m_pos = Vec2d(proj_cfg.opt<ConfigOptionFloats>("wipe_tower_x")->get_at(plate_idx),
                              proj_cfg.opt<ConfigOptionFloats>("wipe_tower_y")->get_at(plate_idx));
            // BBS: don't support rotation
            //wti.m_rotation = (M_PI/180.) * proj_cfg->opt_float("wipe_tower_rotation_angle");

            auto& preset = app_preset_bundle()->prints.get_edited_preset();
            float wt_brim_width = preset.config.opt_float("prime_tower_brim_width");

            const BoundingBoxf3& bb = vol->bounding_box();
            wti.m_bb = BoundingBoxf{to_2d(bb.min), to_2d(bb.max)};
            wti.m_bb.offset(wt_brim_width);

            float brim_width = app_preset_bundle()->prints.get_edited_preset().config.opt_float("prime_tower_brim_width");
            wti.m_bb.offset((brim_width));

            // BBS: the wipe tower pos might be outside bed
            PartPlate* plate = AppAdapter::plater()->get_partplate_list().get_plate(plate_idx);
            Vec2d plate_size = plate->get_size();
            wti.m_pos.x() = std::clamp(wti.m_pos.x(), 0.0, plate_size(0) - wti.m_bb.size().x());
            wti.m_pos.y() = std::clamp(wti.m_pos.y(), 0.0, plate_size(1) - wti.m_bb.size().y());

            // BBS: add partplate logic
            wti.m_plate_idx = plate_idx;
            break;
        }
    }

    return wti;
}

Linef3 GLCanvas3D::mouse_ray(const Point& mouse_pos)
{
    float z0 = 0.0f;
    float z1 = 1.0f;
    return Linef3(_mouse_to_3d(mouse_pos, &z0), _mouse_to_3d(mouse_pos, &z1));
}

double GLCanvas3D::get_size_proportional_to_max_bed_size(double factor) const
{
    const BoundingBoxf& bbox = PlateBed::build_volume().bounding_volume2d();
    return factor * std::max(bbox.size()[0], bbox.size()[1]);
}

//BBS
std::vector<Vec2f> GLCanvas3D::get_empty_cells(const Vec2f start_point, const Vec2f step)
{
    PartPlate* plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
    BoundingBoxf3 build_volume = plate->get_build_volume();
    Vec2d vmin(build_volume.min.x(), build_volume.min.y()), vmax(build_volume.max.x(), build_volume.max.y());
    BoundingBoxf bbox(vmin, vmax);
    std::vector<Vec2f> cells;
    std::vector<Vec2f> cells_ret;
    auto min_x = start_point.x() - step(0) * int((start_point.x() - bbox.min.x()) / step(0));
    auto min_y = start_point.y() - step(1) * int((start_point.y() - bbox.min.y()) / step(1));
    cells.reserve(((bbox.max.x() - min_x) / step(0)) * ((bbox.max.y() - min_y) / step(1)));
    cells_ret.reserve(cells.size());
    for (float x = min_x; x < bbox.max.x() - step(0) / 2; x += step(0))
        for (float y = min_y; y < bbox.max.y() - step(1) / 2; y += step(1))
        {
            cells.emplace_back(x, y);
        }
    for (size_t i = 0; i < m_model->objects.size(); ++i) {
        ModelObject* model_object = m_model->objects[i];
        auto id = model_object->id().id;
        ModelInstance* model_instance0 = model_object->instances.front();
        Polygon hull_2d = model_object->convex_hull_2d(Geometry::assemble_transform({ 0.0, 0.0, model_instance0->get_offset().z() }, model_instance0->get_rotation(),
            model_instance0->get_scaling_factor(), model_instance0->get_mirror()));
        if (hull_2d.empty())
            continue;

        const auto& instances = model_object->instances;
        double rotation_z0 = instances.front()->get_rotation().z();
        for (const auto& instance : instances) {
            Geometry::Transformation transformation;
            const Vec3d& offset = instance->get_offset();
            transformation.set_offset({ scale_(offset.x()), scale_(offset.y()), 0.0 });
            transformation.set_rotation(Z, instance->get_rotation().z() - rotation_z0);
            const Transform3d& trafo = transformation.get_matrix();
            Polygon inst_hull_2d = hull_2d.transform(trafo);

            for (auto it = cells.begin(); it != cells.end(); )
            {
                if (!inst_hull_2d.contains(Point(scale_(it->x()), scale_(it->y()))))
                    cells_ret.push_back(*it);

                it++;
            }
        }
    }

    Vec2f start = start_point;
    if (start_point(0) < 0 && start_point(1) < 0) {
        start(0) = bbox.center()(0);
        start(1) = bbox.center()(1);
    }
    std::sort(cells_ret.begin(), cells_ret.end(), [start](const Vec2f& cell1, const Vec2f& cell2) {return (cell1 - start).norm() < (cell2 - start).norm(); });
    return cells_ret;
}

Vec2f GLCanvas3D::get_nearest_empty_cell(const Vec2f start_point, const Vec2f step)
{
    std::vector<Vec2f> empty_cells = get_empty_cells(start_point, step);
    if (!empty_cells.empty())
        return empty_cells.front();
    else {
        double offset = get_size_proportional_to_max_bed_size(0.05);
        return { start_point(0) + offset, start_point(1) + offset };
    }
}

void GLCanvas3D::msw_rescale()
{
}

void GLCanvas3D::mouse_up_cleanup()
{
    m_moving = false;
    m_camera_movement = false;
    m_mouse.drag.move_volume_idx = -1;
    m_mouse.set_start_position_3D_as_invalid();
    m_mouse.set_start_position_2D_as_invalid();
    m_mouse.dragging = false;
    m_mouse.ignore_left_up = false;
    m_mouse.ignore_right_up = false;
    m_dirty = true;

    if (m_canvas->HasCapture())
        m_canvas->ReleaseMouse();
}

void GLCanvas3D::update_sequential_clearance()
{
    if ((fff_print()->config().print_sequence == PrintSequence::ByLayer))
        return;

    if (m_gizmos->is_dragging())
        return;

    // collects instance transformations from volumes
    // first define temporary cache
    unsigned int instances_count = 0;
    std::vector<std::vector<std::optional<Geometry::Transformation>>> instance_transforms;
    for (size_t obj = 0; obj < m_model->objects.size(); ++obj) {
        instance_transforms.emplace_back(std::vector<std::optional<Geometry::Transformation>>());
        const ModelObject* model_object = m_model->objects[obj];
        for (size_t i = 0; i < model_object->instances.size(); ++i) {
            instance_transforms[obj].emplace_back(std::optional<Geometry::Transformation>());
            ++instances_count;
        }
    }

    // second fill temporary cache with data from volumes
    for (const GLVolume* v : m_volumes.volumes) {
        if (v->is_modifier || v->is_wipe_tower)
            continue;

        auto& transform = instance_transforms[v->object_idx()][v->instance_idx()];
        if (!transform.has_value())
            transform = v->get_instance_transformation();
    }

    // calculates objects 2d hulls (see also: Print::sequential_print_horizontal_clearance_valid())
    // this is done only the first time this method is called while moving the mouse,
    // the results are then cached for following displacements
    if (m_sequential_print_clearance_first_displacement) {
        m_sequential_print_clearance.m_hull_2d_cache.clear();
        auto [object_skirt_offset, _] = fff_print()->object_skirt_offset();
        float shrink_factor;
        if (fff_print()->is_all_objects_are_short())
            shrink_factor = scale_(std::max(0.5f * MAX_OUTER_NOZZLE_DIAMETER, object_skirt_offset) - 0.1);
        else
            shrink_factor = static_cast<float>(scale_(0.5 * fff_print()->config().extruder_clearance_radius.value + object_skirt_offset - 0.1));

        double mitter_limit = scale_(0.1);
        m_sequential_print_clearance.m_hull_2d_cache.reserve(m_model->objects.size());
        for (size_t i = 0; i < m_model->objects.size(); ++i) {
            ModelObject* model_object = m_model->objects[i];
            ModelInstance* model_instance0 = model_object->instances.front();
            Polygon hull_no_offset = model_object->convex_hull_2d(Geometry::assemble_transform({ 0.0, 0.0, model_instance0->get_offset().z() }, model_instance0->get_rotation(),
                model_instance0->get_scaling_factor(), model_instance0->get_mirror()));
            auto tmp = offset(hull_no_offset,
                // Shrink the extruder_clearance_radius a tiny bit, so that if the object arrangement algorithm placed the objects
                // exactly by satisfying the extruder_clearance_radius, this test will not trigger collision.
                shrink_factor,
                jtRound, mitter_limit);
            Polygon hull_2d = !tmp.empty() ? tmp.front() : hull_no_offset;// tmp may be empty due to clipper's bug, see STUDIO-2452

            Pointf3s& cache_hull_2d = m_sequential_print_clearance.m_hull_2d_cache.emplace_back(Pointf3s());
            cache_hull_2d.reserve(hull_2d.points.size());
            for (const Point& p : hull_2d.points) {
                cache_hull_2d.emplace_back(unscale<double>(p.x()), unscale<double>(p.y()), 0.0);
            }
        }
        m_sequential_print_clearance_first_displacement = false;
    }

    // calculates instances 2d hulls (see also: Print::sequential_print_horizontal_clearance_valid())
    //BBS: add the height logic
    PartPlate* plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
    Polygons polygons;
    std::vector<std::pair<Polygon, float>> height_polygons;
    polygons.reserve(instances_count);
    height_polygons.reserve(instances_count);
    std::vector<struct height_info> convex_and_bounding_boxes;
    struct height_info
    {
        double         instance_height;
        BoundingBox    bounding_box;
        Polygon        hull_polygon;
    };
    for (size_t i = 0; i < instance_transforms.size(); ++i) {
        const auto& instances = instance_transforms[i];
        double rotation_z0 = instances.front()->get_rotation().z();
        int index = 0;
        for (const auto& instance : instances) {
            Geometry::Transformation transformation;
            const Vec3d& offset = instance->get_offset();
            transformation.set_offset({ offset.x(), offset.y(), 0.0 });
            transformation.set_rotation(Z, instance->get_rotation().z() - rotation_z0);
            const Transform3d& trafo = transformation.get_matrix();
            const Pointf3s& hull_2d = m_sequential_print_clearance.m_hull_2d_cache[i];
            Points inst_pts;
            inst_pts.reserve(hull_2d.size());
            for (size_t j = 0; j < hull_2d.size(); ++j) {
                const Vec3d p = trafo * hull_2d[j];
                inst_pts.emplace_back(scaled<double>(p.x()), scaled<double>(p.y()));
            }
            Polygon convex_hull(std::move(inst_pts));
            BoundingBox bouding_box = convex_hull.bounding_box();
            BoundingBox plate_bb = plate->get_bounding_box_crd();
            double instance_height = m_model->objects[i]->get_instance_max_z(index++);
            //skip the object for not current plate
            if (!plate_bb.overlap(bouding_box))
                continue;
            convex_and_bounding_boxes.push_back({instance_height, bouding_box, convex_hull});
            polygons.emplace_back(std::move(convex_hull));
        }
    }

    //sort the print instance
    std::sort(convex_and_bounding_boxes.begin(), convex_and_bounding_boxes.end(),
        [](auto &l, auto &r) {
            auto ly1 = l.bounding_box.min.y();
            auto ly2 = l.bounding_box.max.y();
            auto ry1 = r.bounding_box.min.y();
            auto ry2 = r.bounding_box.max.y();
            auto inter_min = std::max(ly1, ry1);
            auto inter_max = std::min(ly2, ry2);
            auto lx = l.bounding_box.min.x();
            auto rx = r.bounding_box.min.x();
            if (inter_max - inter_min > 0)
                return (lx < rx) || ((lx == rx)&&(ly1 < ry1));
            else
                return (ly1 < ry1);
        });

    int bounding_box_count = convex_and_bounding_boxes.size();
    double printable_height = fff_print()->config().printable_height;
    double hc1 = fff_print()->config().extruder_clearance_height_to_lid;
    double hc2 = fff_print()->config().extruder_clearance_height_to_rod;
    for (int k = 0; k < bounding_box_count; k++)
    {
        Polygon& convex = convex_and_bounding_boxes[k].hull_polygon;
        BoundingBox& bbox = convex_and_bounding_boxes[k].bounding_box;
        auto iy1 = bbox.min.y();
        auto iy2 = bbox.max.y();
        double height = (k == (bounding_box_count - 1))?printable_height:hc1;

        for (int i = k+1; i < bounding_box_count; i++)
        {
            Polygon&     next_convex = convex_and_bounding_boxes[i].hull_polygon;
            BoundingBox& next_bbox   = convex_and_bounding_boxes[i].bounding_box;
            auto py1 = next_bbox.min.y();
            auto py2 = next_bbox.max.y();
            auto inter_min = std::max(iy1, py1); // min y of intersection
            auto inter_max = std::min(iy2, py2); // max y of intersection. length=max_y-min_y>0 means intersection exists
            if (inter_max - inter_min > 0) {
                height = hc2;
                break;
            }
        }
        if (height < convex_and_bounding_boxes[k].instance_height)
            height_polygons.emplace_back(std::make_pair(convex, height));
    }

    // sends instances 2d hulls to be rendered
    set_sequential_print_clearance_visible(true);
    set_sequential_print_clearance_render_fill(false);
    set_sequential_print_clearance_polygons(polygons, height_polygons);
}

bool GLCanvas3D::is_object_sinking(int object_idx) const
{
    for (const GLVolume* v : m_volumes.volumes) {
        if (v->object_idx() == object_idx && (v->is_sinking() || (!v->is_modifier && v->is_below_printbed())))
            return true;
    }
    return false;
}

void GLCanvas3D::apply_retina_scale(Vec2d &screen_coordinate) const 
{
#if ENABLE_RETINA_GL
    double scale = static_cast<double>(m_retina_helper->get_scale_factor());
    screen_coordinate *= scale;
#endif // ENABLE_RETINA_GL
}

int GLCanvas3D::get_main_toolbar_offset() const
{
    const float cnv_width              = get_canvas_size().get_width();
    const float collapse_toolbar_width = get_collapse_toolbar_width() * 2;
    const float gizmo_width            = m_gizmos->get_scaled_total_width();
    const float separator_width        = m_separator_toolbar.get_width();
    const float toolbar_total_width    = m_main_toolbar.get_width() + separator_width + gizmo_width + collapse_toolbar_width;

    if (cnv_width < toolbar_total_width) {
        return is_collapse_toolbar_on_left() ? collapse_toolbar_width : 0;
    } else {
        const float offset = (cnv_width - toolbar_total_width) / 2;
        return is_collapse_toolbar_on_left() ? offset + collapse_toolbar_width : offset;
    }
}

bool GLCanvas3D::_is_shown_on_screen() const
{
    //return (m_canvas != nullptr) ? m_canvas->IsShownOnScreen() : false;
    return (m_canvas != nullptr) ? AppAdapter::mainframe()->IsShown() : false;
}

// Getter for the const char*[]
static bool string_getter(const bool is_undo, int idx, const char** out_text)
{
    return AppAdapter::plater()->undo_redo_string_getter(is_undo, idx, out_text);
}

// Getter for the const char*[] for the search list
static bool search_string_getter(int idx, const char** label, const char** tooltip)
{
    return AppAdapter::plater()->search_string_getter(idx, label, tooltip);
}

//BBS: GUI refactor: adjust main toolbar position
bool GLCanvas3D::_render_orient_menu(float left, float right, float bottom, float top)
{
    ImGuiWrapper& imgui = global_im_gui();

    auto canvas_w = float(get_canvas_size().get_width());
    auto canvas_h = float(get_canvas_size().get_height());
    //BBS: GUI refactor: move main toolbar to the right
    //original use center as {0.0}, and top is (canvas_h/2), bottom is (-canvas_h/2), also plus inv_camera
    //now change to left_up as {0,0}, and top is 0, bottom is canvas_h
#if BBS_TOOLBAR_ON_TOP
    const float x = (1 + left) * canvas_w / 2;
    ImGuiWrapper::push_toolbar_style(get_scale());
    imgui.set_next_window_pos(x, m_main_toolbar.get_height(), ImGuiCond_Always, 0.5f, 0.0f);
#else
    const float x = canvas_w - m_main_toolbar.get_width();
    const float y = 0.5f * canvas_h - top * float(AppAdapter::plater()->get_camera().get_zoom());
    imgui.set_next_window_pos(x, y, ImGuiCond_Always, 1.0f, 0.0f);
#endif

    imgui.begin(_L("Auto Orientation options"), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    OrientSettings settings = get_orient_settings();
    OrientSettings& settings_out = get_orient_settings();

    auto appcfg = AppAdapter::app_config();

    bool settings_changed = false;
    float angle_min = 45.f;
    std::string angle_key = "overhang_angle", rot_key = "enable_rotation";
    std::string key_min_area = "min_area";
    std::string postfix = "_fff";

    angle_key += postfix;
    rot_key += postfix;

    if (imgui.checkbox(_L("Enable rotation"), settings.enable_rotation)) {
        settings_out.enable_rotation = settings.enable_rotation;
        appcfg->set("orient", rot_key, settings_out.enable_rotation ? "1" : "0");
        settings_changed = true;
    }

    if (imgui.checkbox(_L("Optimize support interface area"), settings.min_area)) {
        settings_out.min_area = settings.min_area;
        appcfg->set("orient", key_min_area, settings_out.min_area ? "1" : "0");
        settings_changed = true;
    }

    ImGui::Separator();

    if (imgui.button(_L("Orient"))) {
        AppAdapter::plater()->set_prepare_state(Job::PREPARE_STATE_DEFAULT);
        AppAdapter::plater()->orient();
    }

    ImGui::SameLine();

    if (imgui.button(_L("Reset"))) {
        settings_out = OrientSettings{};
        settings_out.overhang_angle = 60.f;
        appcfg->set("orient", angle_key, std::to_string(settings_out.overhang_angle));
        appcfg->set("orient", rot_key, settings_out.enable_rotation ? "1" : "0");
        appcfg->set("orient", key_min_area, settings_out.min_area? "1" : "0");
        settings_changed = true;
    }

    imgui.end();
    ImGuiWrapper::pop_toolbar_style();
    return settings_changed;
}

//BBS: GUI refactor: adjust main toolbar position
bool GLCanvas3D::_render_arrange_menu(float left, float right, float bottom, float top)
{
    ImGuiWrapper& imgui = global_im_gui();

    auto canvas_w = float(get_canvas_size().get_width());
    auto canvas_h = float(get_canvas_size().get_height());
    //BBS: GUI refactor: move main toolbar to the right
    //original use center as {0.0}, and top is (canvas_h/2), bottom is (-canvas_h/2), also plus inv_camera
    //now change to left_up as {0,0}, and top is 0, bottom is canvas_h
#if BBS_TOOLBAR_ON_TOP
    float left_pos = m_main_toolbar.get_item("arrange")->render_left_pos;
    const float x = (1 + left_pos) * canvas_w / 2;
    imgui.set_next_window_pos(x, m_main_toolbar.get_height(), ImGuiCond_Always, 0.0f, 0.0f);

#else
    const float x = canvas_w - m_main_toolbar.get_width();
    const float y = 0.5f * canvas_h - top * float(AppAdapter::plater()->get_camera().get_zoom());
    imgui.set_next_window_pos(x, y, ImGuiCond_Always, 1.0f, 0.0f);
#endif

    //BBS
    ImGuiWrapper::push_toolbar_style(get_scale());

    imgui.begin(_L("Arrange options"), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    ArrangeSettings settings = get_arrange_settings();
    ArrangeSettings &settings_out = get_arrange_settings();
    const float slider_icon_width = imgui.get_slider_icon_size().x;
    const float cursor_slider_left = imgui.calc_text_size(_L("Spacing")).x + imgui.scaled(1.5f);
    const float minimal_slider_width = imgui.scaled(4.f);
    float window_width  = minimal_slider_width + 2 * slider_icon_width;
    auto appcfg = AppAdapter::app_config();

    bool settings_changed = false;
    float dist_min = 0.f;  // 0 means auto
    std::string dist_key = "min_object_distance", rot_key = "enable_rotation";
    std::string bed_shrink_x_key = "bed_shrink_x", bed_shrink_y_key = "bed_shrink_y";
    std::string multi_material_key = "allow_multi_materials_on_same_plate";
    std::string avoid_extrusion_key = "avoid_extrusion_cali_region";
    std::string align_to_y_axis_key = "align_to_y_axis";
    std::string postfix;
    //BBS:
    bool seq_print = false;

    if (true) {
        seq_print = &settings == &m_arrange_settings_fff_seq_print;
        if (seq_print) {
            postfix      = "_fff_seq_print";
        } else {
            postfix     = "_fff";
        }
    }

    dist_key += postfix;
    rot_key  += postfix;
    bed_shrink_x_key += postfix;
    bed_shrink_y_key += postfix;

    ImGui::AlignTextToFramePadding();
    imgui.text(_L("Spacing"));
    ImGui::SameLine(1.2 * cursor_slider_left);
    ImGui::PushItemWidth(window_width - slider_icon_width);
    bool b_Spacing = imgui.bbl_slider_float_style("##Spacing", &settings.distance, dist_min, 100.0f, "%5.2f") || dist_min > settings.distance;
    ImGui::SameLine(window_width - slider_icon_width + 1.3 * cursor_slider_left);
    ImGui::PushItemWidth(1.5 * slider_icon_width);
    bool b_spacing_input = ImGui::BBLDragFloat("##spacing_input", &settings.distance, 0.05f, 0.0f, 0.0f, "%.2f");
    if (b_Spacing || b_spacing_input)
    {
        settings.distance = std::max(dist_min, settings.distance);
        settings_out.distance = settings.distance;
        appcfg->set("arrange", dist_key.c_str(), float_to_string_decimal_point(settings_out.distance));
        settings_changed = true;
    }
    imgui.text(_L("0 means auto spacing."));

    ImGui::Separator();
    if (imgui.bbl_checkbox(_L("Auto rotate for arrangement"), settings.enable_rotation)) {
        settings_out.enable_rotation = settings.enable_rotation;
        appcfg->set("arrange", rot_key.c_str(), settings_out.enable_rotation);
        settings_changed = true;
    }

    if (imgui.bbl_checkbox(_L("Allow multiple materials on same plate"), settings.allow_multi_materials_on_same_plate)) {
        settings_out.allow_multi_materials_on_same_plate = settings.allow_multi_materials_on_same_plate;
        appcfg->set("arrange", multi_material_key.c_str(), settings_out.allow_multi_materials_on_same_plate );
        settings_changed = true;
    }

    // only show this option if the printer has micro Lidar and can do first layer scan
    DynamicPrintConfig &current_config = app_preset_bundle()->printers.get_edited_preset().config;
    auto                op             = current_config.option("scan_first_layer");
    {
        settings_out.avoid_extrusion_cali_region = false;
    }

    // Align to Y axis. Only enable this option when auto rotation not enabled
    {
        if (settings_out.enable_rotation) {  // do not allow align to Y axis if rotation is enabled
            imgui.disabled_begin(true);
            settings_out.align_to_y_axis = false;
        }

        if (imgui.bbl_checkbox(_L("Align to Y axis"), settings.align_to_y_axis)) {
            settings_out.align_to_y_axis = settings.align_to_y_axis;
            appcfg->set("arrange", align_to_y_axis_key, settings_out.align_to_y_axis ? "1" : "0");
            settings_changed = true;
        }

        if (settings_out.enable_rotation == true) { imgui.disabled_end(); }
    }

    ImGui::Separator();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(15.0f, 10.0f));
    if (imgui.button(_L("Arrange"))) {
        AppAdapter::plater()->set_prepare_state(Job::PREPARE_STATE_DEFAULT);
        AppAdapter::plater()->arrange();
    }

    ImGui::SameLine();

    if (imgui.button(_L("Reset"))) {
        settings_out = ArrangeSettings{};
        settings_out.distance = std::max(dist_min, settings_out.distance);
        //BBS: add specific arrange settings
        if (seq_print) settings_out.is_seq_print = true;

        if (auto printer_structure_opt = app_preset_bundle()->printers.get_edited_preset().config.option<ConfigOptionEnum<PrinterStructure>>("printer_structure")) {
            settings_out.align_to_y_axis = (printer_structure_opt->value == PrinterStructure::psI3);
        }
        else
            settings_out.align_to_y_axis = false;

        appcfg->set("arrange", dist_key, float_to_string_decimal_point(settings_out.distance));
        appcfg->set("arrange", rot_key, settings_out.enable_rotation ? "1" : "0");
        appcfg->set("arrange", align_to_y_axis_key, settings_out.align_to_y_axis ? "1" : "0");
        settings_changed = true;
    }
    ImGui::PopStyleVar(1);
    imgui.end();

    //BBS
    ImGuiWrapper::pop_toolbar_style();

    return settings_changed;
}

static const float cameraProjection[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};

void GLCanvas3D::_render_3d_navigator()
{
    if (!show_3d_navigator()) {
        return;
    }

    ImGuizmo::BeginFrame();

    auto& style                                = ImGuizmo::GetStyle();
    style.Colors[ImGuizmo::COLOR::DIRECTION_X] = ImGuiWrapper::to_ImVec4(ColorRGBA::Y());
    style.Colors[ImGuizmo::COLOR::DIRECTION_Y] = ImGuiWrapper::to_ImVec4(ColorRGBA::Z());
    style.Colors[ImGuizmo::COLOR::DIRECTION_Z] = ImGuiWrapper::to_ImVec4(ColorRGBA::X());
    style.Colors[ImGuizmo::COLOR::TEXT] = m_is_dark ? ImVec4(224 / 255.f, 224 / 255.f, 224 / 255.f, 1.f) : ImVec4(.2f, .2f, .2f, 1.0f);
    style.Colors[ImGuizmo::COLOR::FACE]        = m_is_dark ? ImVec4(0.23f, 0.23f, 0.23f, 1.f) : ImVec4(0.77f, 0.77f, 0.77f, 1);
    strcpy(style.AxisLabels[ImGuizmo::Axis::Axis_X], "y");
    strcpy(style.AxisLabels[ImGuizmo::Axis::Axis_Y], "z");
    strcpy(style.AxisLabels[ImGuizmo::Axis::Axis_Z], "x");
    strcpy(style.FaceLabels[ImGuizmo::FACES::FACE_FRONT], _utf8("Front").c_str());
    strcpy(style.FaceLabels[ImGuizmo::FACES::FACE_BACK], _utf8("Back").c_str());
    strcpy(style.FaceLabels[ImGuizmo::FACES::FACE_TOP], _utf8("Top").c_str());
    strcpy(style.FaceLabels[ImGuizmo::FACES::FACE_BOTTOM], _utf8("Bottom").c_str());
    strcpy(style.FaceLabels[ImGuizmo::FACES::FACE_LEFT], _utf8("Left").c_str());
    strcpy(style.FaceLabels[ImGuizmo::FACES::FACE_RIGHT], _utf8("Right").c_str());
    
    float sc = get_scale();
#ifdef WIN32
    const int dpi = get_dpi_for_window(AppAdapter::app()->GetTopWindow());
    sc *= (float) dpi / (float) DPI_DEFAULT;
#endif // WIN32

    const ImGuiIO& io              = ImGui::GetIO();
    const float viewManipulateLeft = 0;
    const float viewManipulateTop  = io.DisplaySize.y;
    const float camDistance        = 8.f;

    Camera&     camera           = AppAdapter::plater()->get_camera();
    Transform3d m                = Transform3d::Identity();
    m.matrix().block(0, 0, 3, 3) = camera.get_view_rotation().toRotationMatrix();
    // Rotate along X and Z axis for 90 degrees to have Y-up
    const auto coord_mapping_transform = Geometry::rotation_transform(Vec3d(0.5 * PI, 0, 0.5 * PI));
    m = m * coord_mapping_transform;
    float cameraView[16];
    for (unsigned int c = 0; c < 4; ++c) {
        for (unsigned int r = 0; r < 4; ++r) {
            cameraView[c * 4 + r] = m(r, c);
        }
    }

    const float size  = 128 * sc;
    const auto result = ImGuizmo::ViewManipulate(cameraView, cameraProjection, ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::WORLD, nullptr,
                                                 camDistance, ImVec2(viewManipulateLeft, viewManipulateTop - size), ImVec2(size, size),
                                                 0x00101010);

    if (result.changed) {
        for (unsigned int c = 0; c < 4; ++c) {
            for (unsigned int r = 0; r < 4; ++r) {
                m(r, c) = cameraView[c * 4 + r];
            }
        }
        // Rotate back
        m = m * (coord_mapping_transform.inverse());
        camera.set_rotation(m);

        if (result.dragging) {
            // Switch back to perspective view when normal dragging
            camera.auto_type(Camera::EType::Perspective);
        } else if (result.clicked_box >= 0) {
            switch (result.clicked_box) {
            case 4: // back
            case 10: // top
            case 12: // right
            case 14: // left
            case 16: // bottom
            case 22: // front
                // Automatically switch to orthographic view when click the center face of the navigator
                camera.auto_type(Camera::EType::Ortho); break;
            default:
                // Otherwise switch back to perspective view
                camera.auto_type(Camera::EType::Perspective); break;
            }
        }

        request_extra_frame();
    }
}

#define ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT 0
#if ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT
static void debug_output_thumbnail(const ThumbnailData& thumbnail_data)
{
    // debug export of generated image
    wxImage image(thumbnail_data.width, thumbnail_data.height);
    image.InitAlpha();

    for (unsigned int r = 0; r < thumbnail_data.height; ++r)
    {
        unsigned int rr = (thumbnail_data.height - 1 - r) * thumbnail_data.width;
        for (unsigned int c = 0; c < thumbnail_data.width; ++c)
        {
            unsigned char* px = (unsigned char*)thumbnail_data.pixels.data() + 4 * (rr + c);
            image.SetRGB((int)c, (int)r, px[0], px[1], px[2]);
            image.SetAlpha((int)c, (int)r, px[3]);
        }
    }

    image.SaveFile("C:/bambu/test/test.png", wxBITMAP_TYPE_PNG);
}
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT

void GLCanvas3D::render_thumbnail_legacy(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params, PartPlateList &partplate_list, ModelObjectPtrs& model_objects, const GLVolumeCollection& volumes, std::vector<ColorRGBA>& extruder_colors, GLShaderProgram* shader, Camera::EType camera_type)
{
    // check that thumbnail size does not exceed the default framebuffer size
    const Size& cnv_size = get_canvas_size();
    unsigned int cnv_w = (unsigned int)cnv_size.get_width();
    unsigned int cnv_h = (unsigned int)cnv_size.get_height();
    if (w > cnv_w || h > cnv_h) {
        float ratio = std::min((float)cnv_w / (float)w, (float)cnv_h / (float)h);
        w = (unsigned int)(ratio * (float)w);
        h = (unsigned int)(ratio * (float)h);
    }

    thumbnail_data.set(w, h);
    if (!thumbnail_data.is_valid())
        return;

    render_thumbnail_internal(thumbnail_data, thumbnail_params, partplate_list,  model_objects, volumes, extruder_colors, shader, camera_type);

    glsafe(::glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)thumbnail_data.pixels.data()));
#if ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT
    debug_output_thumbnail(thumbnail_data);
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT
}

//BBS: GUI refractor

void GLCanvas3D::_switch_toolbars_icon_filename()
{
    BackgroundTexture::Metadata background_data;
    background_data.filename = m_is_dark ? "toolbar_background_dark.png" : "toolbar_background.png";
    background_data.left = 16;
    background_data.top = 16;
    background_data.right = 16;
    background_data.bottom = 16;
    m_main_toolbar.init(background_data);
    m_separator_toolbar.init(background_data);
    AppAdapter::plater()->get_collapse_toolbar().init(background_data);

    // main toolbar
    {
        GLToolbarItem* item;
        item = m_main_toolbar.get_item("add");
        item->set_icon_filename(m_is_dark ? "toolbar_open_dark.svg" : "toolbar_open.svg");

        item = m_main_toolbar.get_item("addplate");
        item->set_icon_filename(m_is_dark ? "toolbar_add_plate_dark.svg" : "toolbar_add_plate.svg");

        item = m_main_toolbar.get_item("orient");
        item->set_icon_filename(m_is_dark ? "toolbar_orient_dark.svg" : "toolbar_orient.svg");

        item = m_main_toolbar.get_item("addplate");
        item->set_icon_filename(m_is_dark ? "toolbar_add_plate_dark.svg" : "toolbar_add_plate.svg");

        item = m_main_toolbar.get_item("arrange");
        item->set_icon_filename(m_is_dark ? "toolbar_arrange_dark.svg" : "toolbar_arrange.svg");

        item = m_main_toolbar.get_item("splitobjects");
        item->set_icon_filename(m_is_dark ? "split_objects_dark.svg" : "split_objects.svg");

        item = m_main_toolbar.get_item("splitvolumes");
        item->set_icon_filename(m_is_dark ? "split_parts_dark.svg" : "split_parts.svg");

        item = m_main_toolbar.get_item("layersediting");
        item->set_icon_filename(m_is_dark ? "toolbar_variable_layer_height_dark.svg" : "toolbar_variable_layer_height.svg");
    }

}
bool GLCanvas3D::_init_toolbars()
{
    if (!_init_main_toolbar())
        return false;

    if (!_init_separator_toolbar())
        return false;

    if (!_init_select_plate_toolbar())
        return false;

    if (!_init_collapse_toolbar())
        return false;

    return true;
}

//BBS: GUI refactor: GLToolbar
bool GLCanvas3D::_init_main_toolbar()
{
    if (!m_main_toolbar.is_enabled())
        return true;

    BackgroundTexture::Metadata background_data;
    background_data.filename = m_is_dark ? "toolbar_background_dark.png" : "toolbar_background.png";
    background_data.left = 16;
    background_data.top = 16;
    background_data.right = 16;
    background_data.bottom = 16;

    if (!m_main_toolbar.init(background_data))
    {
        // unable to init the toolbar texture, disable it
        enable_main_toolbar(false);
        return true;
    }
    // init arrow
    if (!m_main_toolbar.init_arrow("toolbar_arrow.svg"))
        BOOST_LOG_TRIVIAL(error) << "Main toolbar failed to load arrow texture.";

    // m_gizmos is created at constructor, thus we can init arrow here.
    if (!m_gizmos->init_arrow("toolbar_arrow.svg"))
        BOOST_LOG_TRIVIAL(error) << "Gizmos manager failed to load arrow texture.";

    m_main_toolbar.set_layout_type(GLToolbar::Layout::Horizontal);
    //BBS: main toolbar is at the top and left, we don't need the rounded-corner effect at the right side and the top side
    m_main_toolbar.set_horizontal_orientation(GLToolbar::Layout::HO_Right);
    m_main_toolbar.set_vertical_orientation(GLToolbar::Layout::VO_Top);
    m_main_toolbar.set_border(4.0f);
    m_main_toolbar.set_separator_size(4);
    m_main_toolbar.set_gap_size(4);

    m_main_toolbar.del_all_item();

    GLToolbarItem::Data item;

    item.name = "add";
    item.icon_filename = m_is_dark ? "toolbar_open_dark.svg" : "toolbar_open.svg";
    item.tooltip = _utf8(L("Add")) + " [" + GUI::shortkey_ctrl_prefix() + "I]";
    item.sprite_id = 0;
    item.left.action_callback = [this]() { if (m_canvas != nullptr) wxPostEvent(m_canvas, SimpleEvent(EVT_GLTOOLBAR_ADD)); };
    item.enabling_callback = []()->bool {return AppAdapter::plater()->can_add_model(); };
    if (!m_main_toolbar.add_item(item))
        return false;

    item.name = "addplate";
    item.icon_filename = m_is_dark ? "toolbar_add_plate_dark.svg" : "toolbar_add_plate.svg";
    item.tooltip = _utf8(L("Add plate"));
    item.sprite_id++;
    item.left.action_callback = [this]() { if (m_canvas != nullptr) wxPostEvent(m_canvas, SimpleEvent(EVT_GLTOOLBAR_ADD_PLATE)); };
    item.enabling_callback = []()->bool {return AppAdapter::plater()->can_add_plate(); };
    if (!m_main_toolbar.add_item(item))
        return false;

    item.name = "orient";
    item.icon_filename = m_is_dark ? "toolbar_orient_dark.svg" : "toolbar_orient.svg";
    item.tooltip = _utf8(L("Auto orient"));
    item.sprite_id++;
    item.left.render_callback = nullptr;
    item.enabling_callback = []()->bool { return AppAdapter::plater()->can_arrange(); };
    item.left.toggable = false;  // allow right mouse click
    //BBS: GUI refactor: adjust the main toolbar position
    item.left.action_callback = [this]() {
        if (m_canvas != nullptr)
        {
            AppAdapter::plater()->set_prepare_state(Job::PREPARE_STATE_DEFAULT);
            AppAdapter::plater()->orient();
        }
    };
    if (!m_main_toolbar.add_item(item))
        return false;

    item.name = "arrange";
    item.icon_filename = m_is_dark ? "toolbar_arrange_dark.svg" : "toolbar_arrange.svg";
    item.tooltip = _utf8(L("Arrange all objects")) + " [A]\n" + _utf8(L("Arrange objects on selected plates")) + " [Shift+A]";
    item.sprite_id++;
    item.left.action_callback = []() {};
    item.enabling_callback = []()->bool { return AppAdapter::plater()->can_arrange(); };
    item.left.toggable = true;
    //BBS: GUI refactor: adjust the main toolbar position
    item.left.render_callback = [this](float left, float right, float bottom, float top) {
        if (m_canvas != nullptr)
        {
            _render_arrange_menu(left, right, bottom, top);
        }
    };
    if (!m_main_toolbar.add_item(item))
        return false;

    item.right.toggable = false;
    item.right.render_callback = GLToolbarItem::Default_Render_Callback;

    if (!m_main_toolbar.add_separator())
        return false;

    item.name = "splitobjects";
    item.icon_filename = m_is_dark ? "split_objects_dark.svg" : "split_objects.svg";
    item.tooltip = _utf8(L("Split to objects"));
    item.sprite_id++;
    item.left.render_callback = nullptr;
    item.left.action_callback = [this]() { if (m_canvas != nullptr) wxPostEvent(m_canvas, SimpleEvent(EVT_GLTOOLBAR_SPLIT_OBJECTS)); };
    item.visibility_callback = GLToolbarItem::Default_Visibility_Callback;
    item.left.toggable = false;
    item.enabling_callback = []()->bool { return AppAdapter::plater()->can_split_to_objects(); };
    if (!m_main_toolbar.add_item(item))
        return false;

    item.name = "splitvolumes";
    item.icon_filename = m_is_dark ? "split_parts_dark.svg" : "split_parts.svg";
    item.tooltip = _utf8(L("Split to parts"));
    item.sprite_id++;
    item.left.action_callback = [this]() { if (m_canvas != nullptr) wxPostEvent(m_canvas, SimpleEvent(EVT_GLTOOLBAR_SPLIT_VOLUMES)); };
    item.visibility_callback = GLToolbarItem::Default_Visibility_Callback;
    item.enabling_callback = []()->bool { return AppAdapter::plater()->can_split_to_volumes(); };
    if (!m_main_toolbar.add_item(item))
        return false;

    item.name = "layersediting";
    item.icon_filename = m_is_dark ? "toolbar_variable_layer_height_dark.svg" : "toolbar_variable_layer_height.svg";
    item.tooltip = _utf8(L("Variable layer height"));
    item.sprite_id++;
    item.left.toggable = true; // ORCA Closes popup if other toolbar icon clicked and it allows closing popup when clicked its button
    item.left.action_callback = [this]() { if (m_canvas != nullptr) wxPostEvent(m_canvas, SimpleEvent(EVT_GLTOOLBAR_LAYERSEDITING)); };
    item.visibility_callback = [this]()->bool {
        bool res = true;
        // turns off if changing printer technology
        if (!res && m_main_toolbar.is_item_visible("layersediting") && m_main_toolbar.is_item_pressed("layersediting"))
            force_main_toolbar_left_action(get_main_toolbar_item_id("layersediting"));

        return res;
    };
    item.enabling_callback = []()->bool { return AppAdapter::plater()->can_layers_editing(); };
    if (!m_main_toolbar.add_item(item))
        return false;

    return true;
}

//BBS: GUI refactor: GLToolbar
bool GLCanvas3D::_init_select_plate_toolbar()
{
    std::string path = resources_dir() + "/images/";
    IMToolbarItem* item = new IMToolbarItem();
    bool result = item->image_texture.load_from_svg_file(path + "im_all_plates_stats.svg", false, false, false, 128);
    result = result && item->image_texture_transparent.load_from_svg_file(path + "im_all_plates_stats_transparent.svg", false, false, false, 128);
    m_sel_plate_toolbar.m_all_plates_stats_item = item;

    return result;
}

void GLCanvas3D::_update_select_plate_toolbar_stats_item(bool force_selected) {
    PartPlateList& plate_list = AppAdapter::plater()->get_partplate_list();
    if (plate_list.get_nonempty_plate_list().size() > 1)
        m_sel_plate_toolbar.show_stats_item = true;
    else
        m_sel_plate_toolbar.show_stats_item = false;

    if (force_selected && m_sel_plate_toolbar.show_stats_item)
        m_sel_plate_toolbar.m_all_plates_stats_item->selected = true;
}

bool GLCanvas3D::_update_imgui_select_plate_toolbar()
{
    bool result = true;
    if (!m_sel_plate_toolbar.is_enabled() || m_sel_plate_toolbar.is_render_finish) return false;

    _update_select_plate_toolbar_stats_item();

    m_sel_plate_toolbar.del_all_item();

    PartPlateList& plate_list = AppAdapter::plater()->get_partplate_list();
    for (int i = 0; i < plate_list.get_plate_count(); i++) {
        IMToolbarItem* item = new IMToolbarItem();
        PartPlate* plate = plate_list.get_plate(i);
        if (plate && plate->thumbnail_data.is_valid()) {
            PartPlate* plate = plate_list.get_plate(i);
            item->image_data = plate->thumbnail_data.pixels;
            item->image_width = plate->thumbnail_data.width;
            item->image_height = plate->thumbnail_data.height;
            result = item->generate_texture();
        }
        m_sel_plate_toolbar.m_items.push_back(item);
    }

    m_sel_plate_toolbar.is_display_scrollbar = false;
    return result;
}

bool GLCanvas3D::_init_separator_toolbar()
{
    if (!m_separator_toolbar.is_enabled())
        return true;

    BackgroundTexture::Metadata background_data;
    background_data.filename = m_is_dark ? "toolbar_background_dark.png" : "toolbar_background.png";
    background_data.left = 0;
    background_data.top = 0;
    background_data.right = 0;
    background_data.bottom = 0;

    if (!m_separator_toolbar.init(background_data))
    {
        // unable to init the toolbar texture, disable it
        m_separator_toolbar.set_enabled(false);
        return true;
    }

    m_separator_toolbar.set_layout_type(GLToolbar::Layout::Horizontal);
    //BBS: assemble toolbar is at the top and right, we don't need the rounded-corner effect at the left side and the top side
    m_separator_toolbar.set_horizontal_orientation(GLToolbar::Layout::HO_Left);
    m_separator_toolbar.set_vertical_orientation(GLToolbar::Layout::VO_Top);
    m_separator_toolbar.set_border(4.0f);

    m_separator_toolbar.del_all_item();

    GLToolbarItem::Data sperate_item;
    sperate_item.name = "start_seperator";
    sperate_item.icon_filename = "seperator.svg";
    sperate_item.sprite_id = 0;
    sperate_item.left.action_callback = [this]() {};
    sperate_item.visibility_callback = []()->bool { return true; };
    sperate_item.enabling_callback = []()->bool { return false; };
    if (!m_separator_toolbar.add_item(sperate_item))
        return false;

     return true;
}

bool GLCanvas3D::_init_collapse_toolbar()
{
    return AppAdapter::plater()->init_collapse_toolbar();
}

bool GLCanvas3D::_set_current()
{
    return m_context != nullptr && m_canvas->SetCurrent(*m_context);
}

void GLCanvas3D::_resize(unsigned int w, unsigned int h)
{
    if (m_canvas == nullptr && m_context == nullptr)
        return;

    const std::array<unsigned int, 2> new_size = { w, h };
    if (m_old_size == new_size)
        return;

    m_old_size = new_size;

    ImGuiWrapper& imgui = global_im_gui();
    imgui.set_display_size(static_cast<float>(w), static_cast<float>(h));

    //BBS reduce render
    if (m_last_w == w && m_last_h == h) {
        return;
    }

    m_last_w = w;
    m_last_h = h;

    float font_size = app_em_unit();

#ifdef _WIN32
    // On Windows, if manually scaled here, rendering issues can occur when the system's Display
    // scaling is greater than 300% as the font's size gets to be to large. So, use imgui font
    // scaling instead (see: ImGuiWrapper::init_font() and issue #3401)
    font_size *= (font_size > 30.0f) ? 1.0f : 1.5f;
#else
    font_size *= 1.5f;
#endif

#if ENABLE_RETINA_GL
    imgui.set_scaling(font_size, 1.0f, m_retina_helper->get_scale_factor());
#else
    imgui.set_scaling(font_size, m_canvas->GetContentScaleFactor(), 1.0f);
#endif

    this->request_extra_frame();

    _set_current();
}

BoundingBoxf3 GLCanvas3D::_max_bounding_box(bool include_gizmos, bool include_bed_model, bool include_plates) const
{
    BoundingBoxf3 bb = volumes_bounding_box();

    // The following is a workaround for gizmos not being taken in account when calculating the tight camera frustrum
    // A better solution would ask the gizmo manager for the bounding box of the current active gizmo, if any
    if (include_gizmos && m_gizmos && m_gizmos->is_running() && m_selection)
    {
        BoundingBoxf3 sel_bb = m_selection->get_bounding_box();
        Vec3d sel_bb_center = sel_bb.center();
        Vec3d extend_by = sel_bb.max_size() * Vec3d::Ones();
        bb.merge(BoundingBoxf3(sel_bb_center - extend_by, sel_bb_center + extend_by));
    }

    bb.merge(include_bed_model ? PlateBed::extended_bounding_box() : PlateBed::build_volume().bounding_volume());
    if (include_plates) {
        bb.merge(AppAdapter::plater()->get_partplate_list().get_bounding_box());
    }

    if ((m_canvas_type == CanvasView3D) /* && (fff_print()->config().print_sequence == PrintSequence::ByObject) */) {
        float height_to_lid, height_to_rod;
        AppAdapter::plater()->get_partplate_list().get_height_limits(height_to_lid, height_to_rod);
        bb.max.z() = std::max(bb.max.z(), (double)height_to_lid);
    }

    return bb;
}

void GLCanvas3D::_zoom_to_box(const BoundingBoxf3& box, double margin_factor)
{
    AppAdapter::plater()->get_camera().zoom_to_box(box, margin_factor);
    m_dirty = true;
}

void GLCanvas3D::_update_camera_zoom(double zoom)
{
    AppAdapter::plater()->get_camera().update_zoom(zoom);
    m_dirty = true;
}

void GLCanvas3D::_refresh_if_shown_on_screen()
{
    if (_is_shown_on_screen()) {
        const Size& cnv_size = get_canvas_size();
        _resize((unsigned int)cnv_size.get_width(), (unsigned int)cnv_size.get_height());

        // Because of performance problems on macOS, where PaintEvents are not delivered
        // frequently enough, we call render() here directly when we can.
        render();
    }
}

void GLCanvas3D::_picking_pass()
{
    if (!m_picking_enabled || m_mouse.dragging || m_mouse.position == Vec2d(DBL_MAX, DBL_MAX) || m_gizmos->is_dragging()) {
#if ENABLE_RAYCAST_PICKING_DEBUG
        ImGuiWrapper& imgui = global_im_gui();
        imgui.begin(std::string("Hit result"), ImGuiWindowFlags_AlwaysAutoResize);
        imgui.text("Picking disabled");
        imgui.end();
#endif // ENABLE_RAYCAST_PICKING_DEBUG
        return;
    }

    m_hover_volume_idxs.clear();
    m_hover_plate_idxs.clear();

    // Orca: ignore clipping plane if not applying
    GLGizmoBase *current_gizmo  = m_gizmos->get_current();
    const ClippingPlane clipping_plane = ((!current_gizmo || current_gizmo->apply_clipping_plane()) ? m_gizmos->get_clipping_plane() :
                                                                                                      ClippingPlane::ClipsNothing())
                                             .inverted_normal();
    const SceneRaycaster::HitResult hit = m_scene_raycaster.hit(m_mouse.position, AppAdapter::plater()->get_camera(), &clipping_plane);
    if (hit.is_valid()) {
        switch (hit.type)
        {
        case SceneRaycaster::EType::Volume:
        {
            if (0 <= hit.raycaster_id && hit.raycaster_id < (int)m_volumes.volumes.size()) {
                const GLVolume* volume = m_volumes.volumes[hit.raycaster_id];
                if (volume->is_active && !volume->disabled && (volume->composite_id.volume_id >= 0 || m_render_sla_auxiliaries)) {
                    // do not add the volume id if any gizmo is active and CTRL is pressed
                    if (m_gizmos->get_current_type() == GLGizmosManager::EType::Undefined || !wxGetKeyState(WXK_CONTROL))
                        m_hover_volume_idxs.emplace_back(hit.raycaster_id);
                    m_gizmos->set_hover_id(-1);
                }
            }
            else
                assert(false);

            break;
        }
        case SceneRaycaster::EType::Gizmo:
        case SceneRaycaster::EType::FallbackGizmo:
        {
            const Size& cnv_size = get_canvas_size();
            const bool inside = 0 <= m_mouse.position.x() && m_mouse.position.x() < cnv_size.get_width() &&
                0 <= m_mouse.position.y() && m_mouse.position.y() < cnv_size.get_height();
            m_gizmos->set_hover_id(inside ? hit.raycaster_id : -1);
            break;
        }
        case SceneRaycaster::EType::Bed:
        {
            // BBS: add plate picking logic
            int plate_hover_id = hit.raycaster_id;
            if (plate_hover_id >= 0 && plate_hover_id < PartPlateList::MAX_PLATES_COUNT * PartPlate::GRABBER_COUNT) {
                AppAdapter::plater()->get_partplate_list().set_hover_id(plate_hover_id);
                m_hover_plate_idxs.emplace_back(plate_hover_id);
            } else {
                AppAdapter::plater()->get_partplate_list().reset_hover_id();
            }
            m_gizmos->set_hover_id(-1);
            break;
        }
        default:
        {
            assert(false);
            break;
        }
        }
    }
    else
        m_gizmos->set_hover_id(-1);

    _update_volumes_hover_state();

}

void GLCanvas3D::_rectangular_selection_picking_pass()
{
    m_gizmos->set_hover_id(-1);

    std::set<int> idxs;

    if (m_picking_enabled) {
        const size_t width  = std::max<size_t>(m_rectangle_selection.get_width(), 1);
        const size_t height = std::max<size_t>(m_rectangle_selection.get_height(), 1);

        bool use_framebuffer = !is_unkown_framebuffer();

        GLuint render_fbo = 0;
        GLuint render_tex = 0;
        GLuint render_depth = 0;
        if (use_framebuffer) {
            // setup a framebuffer which covers only the selection rectangle
            if (is_arb_framebuffer()) {
                glsafe(::glGenFramebuffers(1, &render_fbo));
                glsafe(::glBindFramebuffer(GL_FRAMEBUFFER, render_fbo));
            }
            else {
                glsafe(::glGenFramebuffersEXT(1, &render_fbo));
                glsafe(::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, render_fbo));
            }
            glsafe(::glGenTextures(1, &render_tex));
            glsafe(::glBindTexture(GL_TEXTURE_2D, render_tex));
            glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
            glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
            glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
            if (is_arb_framebuffer()) {
                glsafe(::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_tex, 0));
                glsafe(::glGenRenderbuffers(1, &render_depth));
                glsafe(::glBindRenderbuffer(GL_RENDERBUFFER, render_depth));
                glsafe(::glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height));
                glsafe(::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_depth));
            }
            else {
                glsafe(::glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, render_tex, 0));
                glsafe(::glGenRenderbuffersEXT(1, &render_depth));
                glsafe(::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_depth));
                glsafe(::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height));
                glsafe(::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, render_depth));
            }
            const GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
            glsafe(::glDrawBuffers(1, drawBufs));
            if (is_arb_framebuffer()) {
                if (::glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    use_framebuffer = false;
            }
            else {
                if (::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
                    use_framebuffer = false;
            }
        }

        if (m_multisample_allowed)
        	// This flag is often ignored by NVIDIA drivers if rendering into a screen buffer.
            glsafe(::glDisable(GL_MULTISAMPLE));

        glsafe(::glDisable(GL_BLEND));
        glsafe(::glEnable(GL_DEPTH_TEST));

        glsafe(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        Camera& main_camera = AppAdapter::plater()->get_camera();
        Camera framebuffer_camera;
        Camera* camera = &main_camera;
        if (use_framebuffer) {
            // setup a camera which covers only the selection rectangle
            const std::array<int, 4>& viewport = camera->get_viewport();
            const double near_left   = camera->get_near_left();
            const double near_bottom = camera->get_near_bottom();
            const double near_width  = camera->get_near_width();
            const double near_height = camera->get_near_height();

            const double ratio_x = near_width / double(viewport[2]);
            const double ratio_y = near_height / double(viewport[3]);

            const double rect_near_left   = near_left + double(m_rectangle_selection.get_left()) * ratio_x;
            const double rect_near_bottom = near_bottom + (double(viewport[3]) - double(m_rectangle_selection.get_bottom())) * ratio_y;
            double rect_near_right = near_left + double(m_rectangle_selection.get_right()) * ratio_x;
            double rect_near_top   = near_bottom + (double(viewport[3]) - double(m_rectangle_selection.get_top())) * ratio_y;

            if (rect_near_left == rect_near_right)
                rect_near_right = rect_near_left + ratio_x;
            if (rect_near_bottom == rect_near_top)
                rect_near_top = rect_near_bottom + ratio_y;

            framebuffer_camera.look_at(camera->get_position(), camera->get_target(), camera->get_dir_up());
            framebuffer_camera.apply_projection(rect_near_left, rect_near_right, rect_near_bottom, rect_near_top, camera->get_near_z(), camera->get_far_z());
            framebuffer_camera.set_viewport(0, 0, width, height);
            apply_viewport(framebuffer_camera);
            camera = &framebuffer_camera;
        }

        _render_volumes_for_picking(*camera);
        //BBS: remove the bed picking logic
        //_render_bed_for_picking(!AppAdapter::plater()->get_camera().is_looking_downward());

        if (m_multisample_allowed)
            glsafe(::glEnable(GL_MULTISAMPLE));

        const size_t px_count = width * height;

        const size_t left = use_framebuffer ? 0 : (size_t)m_rectangle_selection.get_left();
        const size_t top  = use_framebuffer ? 0 : (size_t)get_canvas_size().get_height() - (size_t)m_rectangle_selection.get_top();
#define USE_PARALLEL 1
#if USE_PARALLEL
            struct Pixel
            {
                std::array<GLubyte, 4> data;
            	// Only non-interpolated colors are valid, those have their lowest three bits zeroed.
                bool valid() const { return picking_checksum_alpha_channel(data[0], data[1], data[2]) == data[3]; }
                // we reserve color = (0,0,0) for occluders (as the printbed)
                // volumes' id are shifted by 1
                // see: _render_volumes_for_picking()
                //BBS: remove the bed picking logic
                int id() const { return data[0] + (data[1] << 8) + (data[2] << 16); }
                //int id() const { return data[0] + (data[1] << 8) + (data[2] << 16) - 1; }
            };

            std::vector<Pixel> frame(px_count);
            glsafe(::glReadPixels(left, top, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)frame.data()));

            tbb::spin_mutex mutex;
            tbb::parallel_for(tbb::blocked_range<size_t>(0, frame.size(), (size_t)width),
                [this, &frame, &idxs, &mutex](const tbb::blocked_range<size_t>& range) {
                for (size_t i = range.begin(); i < range.end(); ++i)
                	if (frame[i].valid()) {
                    	int volume_id = frame[i].id();
                    	if (0 <= volume_id && volume_id < (int)m_volumes.volumes.size()) {
                        	mutex.lock();
                        	idxs.insert(volume_id);
                        	mutex.unlock();
                    	}
                	}
            });
#else
            std::vector<GLubyte> frame(4 * px_count);
            glsafe(::glReadPixels(left, top, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)frame.data()));

            for (int i = 0; i < px_count; ++i)
            {
                int px_id = 4 * i;
                int volume_id = frame[px_id] + (frame[px_id + 1] << 8) + (frame[px_id + 2] << 16);
                if (0 <= volume_id && volume_id < (int)m_volumes.volumes.size())
                    idxs.insert(volume_id);
            }
#endif // USE_PARALLEL
            if (camera != &main_camera)
                apply_viewport(main_camera);

            if (is_arb_framebuffer()) {
                glsafe(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
                if (render_depth != 0)
                    glsafe(::glDeleteRenderbuffers(1, &render_depth));
                if (render_fbo != 0)
                    glsafe(::glDeleteFramebuffers(1, &render_fbo));
            }
            else if (is_ext_framebuffer()) {
                glsafe(::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0));
                if (render_depth != 0)
                    glsafe(::glDeleteRenderbuffersEXT(1, &render_depth));
                if (render_fbo != 0)
                    glsafe(::glDeleteFramebuffersEXT(1, &render_fbo));
            }

            if (render_tex != 0)
                glsafe(::glDeleteTextures(1, &render_tex));
    }

    m_hover_volume_idxs.assign(idxs.begin(), idxs.end());
    _update_volumes_hover_state();
}

void GLCanvas3D::_render_background()
{
    bool use_error_color = false;
    if (true) {
        use_error_color = m_dynamic_background_enabled && (!m_volumes.empty());

        if (!m_volumes.empty())
            use_error_color &= _is_any_volume_outside();
        else {
            // //BBS: use current plater's bounding box
            // //BoundingBoxf3 test_volume = (m_config != nullptr) ? print_volume(*m_config) : BoundingBoxf3();
            // BoundingBoxf3 test_volume = (const_cast<GLCanvas3D*>(this))->_get_current_partplate_print_volume();
            // const BoundingBoxf3& path_bounding_box = m_gcode_viewer->get_paths_bounding_box();
            // if (empty(path_bounding_box))
            //     use_error_color = false;
            // else
            //     //BBS: use previous result
            //     use_error_color = (test_volume.radius() > 0.0) ? m_toolpath_outside : false;
        }
    }

    // Draws a bottom to top gradient over the complete screen.
    glsafe(::glDisable(GL_DEPTH_TEST));

    ColorRGBA background_color = m_is_dark ? DEFAULT_BG_LIGHT_COLOR_DARK : DEFAULT_BG_LIGHT_COLOR;
    ColorRGBA error_background_color = m_is_dark ? ERROR_BG_LIGHT_COLOR_DARK : ERROR_BG_LIGHT_COLOR;
    const ColorRGBA bottom_color = use_error_color ? error_background_color : background_color;

    if (!m_background.is_initialized()) {
        m_background.reset();

        GLModel::Geometry init_data;
        init_data.format = { GLModel::Geometry::EPrimitiveType::Triangles, GLModel::Geometry::EVertexLayout::P2T2 };
        init_data.reserve_vertices(4);
        init_data.reserve_indices(6);

        // vertices
        init_data.add_vertex(Vec2f(-1.0f, -1.0f), Vec2f(0.0f, 0.0f));
        init_data.add_vertex(Vec2f(1.0f, -1.0f),  Vec2f(1.0f, 0.0f));
        init_data.add_vertex(Vec2f(1.0f, 1.0f),   Vec2f(1.0f, 1.0f));
        init_data.add_vertex(Vec2f(-1.0f, 1.0f),  Vec2f(0.0f, 1.0f));

        // indices
        init_data.add_triangle(0, 1, 2);
        init_data.add_triangle(2, 3, 0);

        m_background.init_from(std::move(init_data));
    }

    GLShaderProgram* shader = get_shader("background");
    if (shader != nullptr) {
        shader->start_using();
        shader->set_uniform("top_color", bottom_color);
        shader->set_uniform("bottom_color", bottom_color);
        m_background.render();
        shader->stop_using();
    }

    glsafe(::glEnable(GL_DEPTH_TEST));
}

void GLCanvas3D::_render_bed(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool show_axes)
{
    float scale_factor = 1.0;
#if ENABLE_RETINA_GL
    scale_factor = m_retina_helper->get_scale_factor();
#endif // ENABLE_RETINA_GL

    PlateBed::set_axes_mode(m_main_toolbar.is_enabled());
    // m_bed.render(*this, view_matrix, projection_matrix, bottom, scale_factor, show_axes);
}

void GLCanvas3D::_render_platelist(const Transform3d& view_matrix, const Transform3d& projection_matrix, bool bottom, bool only_current, bool only_body, int hover_id, bool render_cali, bool show_grid)
{
    AppAdapter::plater()->get_partplate_list().render(view_matrix, projection_matrix, bottom, only_current, only_body, hover_id, render_cali, show_grid);
}

void GLCanvas3D::_render_volumes_for_picking(const Camera& camera) const
{
    GLShaderProgram* shader = get_shader("flat_clip");
    if (shader == nullptr)
        return;

    // do not cull backfaces to show broken geometry, if any
    glsafe(::glDisable(GL_CULL_FACE));

    const Transform3d& view_matrix = camera.get_view_matrix();
    for (size_t type = 0; type < 2; ++ type) {
        GLVolumeWithIdAndZList to_render = volumes_to_render(m_volumes.volumes, (type == 0) ? GLVolumeCollection::ERenderType::Opaque : GLVolumeCollection::ERenderType::Transparent, view_matrix);
        for (const GLVolumeWithIdAndZ& volume : to_render)
	        if (!volume.first->disabled && (volume.first->composite_id.volume_id >= 0 || m_render_sla_auxiliaries)) {
		        // Object picking mode. Render the object with a color encoding the object index.
                // we reserve color = (0,0,0) for occluders (as the printbed)
                // so we shift volumes' id by 1 to get the proper color
                //BBS: remove the bed picking logic
                const unsigned int id = volume.second.first;
                //const unsigned int id = 1 + volume.second.first;
                volume.first->model.set_color(picking_decode(id));
                shader->start_using();
                shader->set_uniform("view_model_matrix", view_matrix * volume.first->world_matrix());
                shader->set_uniform("projection_matrix", camera.get_projection_matrix());
                shader->set_uniform("volume_world_matrix", volume.first->world_matrix());
                shader->set_uniform("z_range", m_volumes.get_z_range());
                shader->set_uniform("clipping_plane", m_volumes.get_clipping_plane());
                volume.first->picking = true;
                volume.first->render();
                volume.first->picking = false;
                shader->stop_using();
	        }
	}

    glsafe(::glEnable(GL_CULL_FACE));
}

void GLCanvas3D::_render_separator_toolbar_right() const
{
    if (!m_separator_toolbar.is_enabled())
        return;

    const Size cnv_size = get_canvas_size();
    const float gizmo_width = m_gizmos->get_scaled_total_width();
    const float separator_width = m_separator_toolbar.get_width();
    const float top = 0.5f * (float)cnv_size.get_height();
    const float main_toolbar_left = -0.5f * cnv_size.get_width() + get_main_toolbar_offset();
    const float left = main_toolbar_left + (m_main_toolbar.get_width() + gizmo_width + separator_width / 2);

    m_separator_toolbar.set_position(top, left);
    m_separator_toolbar.render(*this,GLToolbarItem::SeparatorLine);
}

void GLCanvas3D::_render_separator_toolbar_left() const
{
    if (!m_separator_toolbar.is_enabled())
        return;

    const Size cnv_size = get_canvas_size();
    const float top = 0.5f * (float)cnv_size.get_height();
    const float main_toolbar_left = -0.5f * cnv_size.get_width() + get_main_toolbar_offset();
    const float left = main_toolbar_left + (m_main_toolbar.get_width());

    m_separator_toolbar.set_position(top, left);
    m_separator_toolbar.render(*this,GLToolbarItem::SeparatorLine);
}

void GLCanvas3D::_render_collapse_toolbar() const
{
    auto&      plater              = *AppAdapter::plater();
    const auto sidebar_docking_dir = plater.get_sidebar_docking_state();
    if (sidebar_docking_dir == Sidebar::None) {
        return;
    }

    GLToolbar& collapse_toolbar = plater.get_collapse_toolbar();

    const Size cnv_size = get_canvas_size();
    const float top  = 0.5f * (float)cnv_size.get_height();
    const float left = sidebar_docking_dir == Sidebar::Right ? 0.5f * (float) cnv_size.get_width() - (float) collapse_toolbar.get_width() :
                                                               -0.5f * (float) cnv_size.get_width();

    collapse_toolbar.set_position(top, left);
    collapse_toolbar.render(*this);
}

#if ENABLE_SHOW_CAMERA_TARGET
void GLCanvas3D::_render_camera_target()
{
    static const float half_length = 5.0f;

    glsafe(::glDisable(GL_DEPTH_TEST));
    glsafe(::glLineWidth(2.0f));
    const Vec3f& target = AppAdapter::plater()->get_camera().get_target().cast<float>();
    bool target_changed = !m_camera_target.target.isApprox(target.cast<double>());
    m_camera_target.target = target.cast<double>();

    for (int i = 0; i < 3; ++i) {
        if (!m_camera_target.axis[i].is_initialized() || target_changed) {
            m_camera_target.axis[i].reset();

            GLModel::Geometry init_data;
            init_data.format = { GLModel::Geometry::EPrimitiveType::Lines, GLModel::Geometry::EVertexLayout::P3, GLModel::Geometry::EIndexType::USHORT };
            init_data.color = (i == X) ? ColorRGBA::X() : ((i == Y) ? ColorRGBA::Y() : ColorRGBA::Z());
            init_data.reserve_vertices(2);
            init_data.reserve_indices(2);

            // vertices
            if (i == X) {
                init_data.add_vertex(Vec3f(target.x() - half_length, target.y(), target.z()));
                init_data.add_vertex(Vec3f(target.x() + half_length, target.y(), target.z()));
            }
            else if (i == Y) {
                init_data.add_vertex(Vec3f(target.x(), target.y() - half_length, target.z()));
                init_data.add_vertex(Vec3f(target.x(), target.y() + half_length, target.z()));
            }
            else {
                init_data.add_vertex(Vec3f(target.x(), target.y(), target.z() - half_length));
                init_data.add_vertex(Vec3f(target.x(), target.y(), target.z() + half_length));
            }

            // indices
            init_data.add_line(0, 1);

            m_camera_target.axis[i].init_from(std::move(init_data));
        }
    }

    GLShaderProgram* shader = get_shader("flat");
    if (shader != nullptr) {
        shader->start_using();
        const Camera& camera = AppAdapter::plater()->get_camera();
        shader->set_uniform("view_model_matrix", camera.get_view_matrix());
        shader->set_uniform("projection_matrix", camera.get_projection_matrix());
        for (int i = 0; i < 3; ++i) {
            m_camera_target.axis[i].render();
        }
        shader->stop_using();
    }
}
#endif // ENABLE_SHOW_CAMERA_TARGET

void GLCanvas3D::_render_selection_sidebar_hints()
{
    if (m_selection)
        m_selection->render_sidebar_hints(m_sidebar_field, m_gizmos->get_uniform_scaling());
}

void GLCanvas3D::_update_volumes_hover_state()
{
    if (!m_selection)
        return;

    for (GLVolume* v : m_volumes.volumes) {
        v->hover = GLVolume::HS_None;
    }

    if (m_hover_volume_idxs.empty())
        return;

    bool ctrl_pressed = wxGetKeyState(WXK_CONTROL); // additive select/deselect
    bool shift_pressed = wxGetKeyState(WXK_SHIFT);  // select by rectangle
    bool alt_pressed = wxGetKeyState(WXK_ALT);      // deselect by rectangle

    if (alt_pressed && (shift_pressed || ctrl_pressed)) {
        // illegal combinations of keys
        m_hover_volume_idxs.clear();
        return;
    }

    bool selection_modifiers_only = m_selection->is_empty() || m_selection->is_any_modifier();

    bool hover_modifiers_only = true;
    for (int i : m_hover_volume_idxs) {
        if (!m_volumes.volumes[i]->is_modifier) {
            hover_modifiers_only = false;
            break;
        }
    }

    std::set<std::pair<int, int>> hover_instances;
    for (int i : m_hover_volume_idxs) {
        const GLVolume& v = *m_volumes.volumes[i];
        hover_instances.insert(std::make_pair(v.object_idx(), v.instance_idx()));
    }

    bool hover_from_single_instance = hover_instances.size() == 1;

    if (hover_modifiers_only && !hover_from_single_instance) {
        // do not allow to select volumes from different instances
        m_hover_volume_idxs.clear();
        return;
    }

    for (int i : m_hover_volume_idxs) {
        GLVolume& volume = *m_volumes.volumes[i];
        if (volume.hover != GLVolume::HS_None)
            continue;

        bool deselect = volume.selected && ((ctrl_pressed && !shift_pressed) || alt_pressed);
        // (volume->is_modifier && !selection_modifiers_only && !is_ctrl_pressed) -> allows hovering on selected modifiers belonging to selection of type Instance
        bool select = (!volume.selected || (volume.is_modifier && !selection_modifiers_only && !ctrl_pressed)) && !alt_pressed;

        if (select || deselect) {
            bool as_volume =
                volume.is_modifier && hover_from_single_instance && !ctrl_pressed &&
                (
                (!deselect) ||
                (deselect && !m_selection->is_single_full_instance() && (volume.object_idx() == m_selection->get_object_idx()) && (volume.instance_idx() == m_selection->get_instance_idx()))
                );

            if (as_volume)
                volume.hover = deselect ? GLVolume::HS_Deselect : GLVolume::HS_Select;
            else {
                int object_idx = volume.object_idx();
                int instance_idx = volume.instance_idx();

                for (GLVolume* v : m_volumes.volumes) {
                    if (v->object_idx() == object_idx && v->instance_idx() == instance_idx)
                        v->hover = deselect ? GLVolume::HS_Deselect : GLVolume::HS_Select;
                }
            }
        }
        else if (volume.selected)
            volume.hover = GLVolume::HS_Hover;
    }
}

void GLCanvas3D::_perform_layer_editing_action(wxMouseEvent* evt)
{
    int object_idx_selected = m_layers_editing->last_object_id;
    if (object_idx_selected == -1)
        return;

    // A volume is selected. Test, whether hovering over a layer thickness bar.
    if (evt != nullptr) {
        const Rect& rect = LayersEditing::get_bar_rect_screen(*this);
        float b = rect.get_bottom();
        m_layers_editing->last_z = m_layers_editing->object_max_z() * (b - evt->GetY() - 1.0f) / (b - rect.get_top());
        m_layers_editing->last_action =
            evt->ShiftDown() ? (evt->RightIsDown() ? LAYER_HEIGHT_EDIT_ACTION_SMOOTH : LAYER_HEIGHT_EDIT_ACTION_REDUCE) :
            (evt->RightIsDown() ? LAYER_HEIGHT_EDIT_ACTION_INCREASE : LAYER_HEIGHT_EDIT_ACTION_DECREASE);
    }

    m_layers_editing->adjust_layer_height_profile();
    _refresh_if_shown_on_screen();

    // Automatic action on mouse down with the same coordinate.
    _start_timer();
}

Vec3d GLCanvas3D::_mouse_to_3d(const Point& mouse_pos, float* z)
{
    if (m_canvas == nullptr)
        return Vec3d(DBL_MAX, DBL_MAX, DBL_MAX);

    if (z == nullptr) {
        const SceneRaycaster::HitResult hit = m_scene_raycaster.hit(mouse_pos.cast<double>(), AppAdapter::plater()->get_camera(), nullptr);
        return hit.is_valid() ? hit.position.cast<double>() : _mouse_to_bed_3d(mouse_pos);
    }
    else {
        const Camera& camera = AppAdapter::plater()->get_camera();
        const Vec4i32 viewport(camera.get_viewport().data());
        Vec3d out;
        igl::unproject(Vec3d(mouse_pos.x(), viewport[3] - mouse_pos.y(), *z), camera.get_view_matrix().matrix(), camera.get_projection_matrix().matrix(), viewport, out);
        return out;
    }
}

Vec3d GLCanvas3D::_mouse_to_bed_3d(const Point& mouse_pos)
{
    return mouse_ray(mouse_pos).intersect_plane(0.0);
}

void GLCanvas3D::_start_timer()
{
    m_timer.Start(100, wxTIMER_CONTINUOUS);
}

void GLCanvas3D::_stop_timer()
{
    m_timer.Stop();
}

void GLCanvas3D::_set_warning_notification(EWarning warning, bool state)
{
    enum ErrorType{
        PLATER_WARNING,
        PLATER_ERROR,
        SLICING_SERIOUS_WARNING,
        SLICING_ERROR
    };
    std::string text;
    ErrorType error = ErrorType::PLATER_WARNING;
    const ModelObject* conflictObj=nullptr;
    switch (warning) {
    case EWarning::ObjectOutside:      text = _u8L("An object is layed over the boundary of plate."); break;
    case EWarning::ToolHeightOutside:  text = _u8L("A G-code path goes beyond the max print height."); error = ErrorType::SLICING_ERROR; break;
    case EWarning::ToolpathOutside:    text = _u8L("A G-code path goes beyond the boundary of plate."); error = ErrorType::SLICING_ERROR; break;
    // BBS: remove _u8L() for SLA
    case EWarning::SlaSupportsOutside: text = ("SLA supports outside the print area were detected."); error = ErrorType::PLATER_ERROR; break;
    case EWarning::SomethingNotShown:  text = _u8L("Only the object being edit is visible."); break;
    case EWarning::ObjectClashed:
        text = _u8L("An object is laid over the boundary of plate or exceeds the height limit.\n"
            "Please solve the problem by moving it totally on or off the plate, and confirming that the height is within the build volume.");
        error = ErrorType::PLATER_ERROR;
        break;
    }
    //BBS: this may happened when exit the app, plater is null
    if (!AppAdapter::plater())
        return;

    auto& notification_manager = *get_notification_manager();

    switch (error)
    {
    case PLATER_WARNING:
        if (state)
            notification_manager.push_plater_warning_notification(text);
        else
            notification_manager.close_plater_warning_notification(text);
        break;
    case PLATER_ERROR:
        if (state)
            notification_manager.push_plater_error_notification(text);
        else
            notification_manager.close_plater_error_notification(text);
        break;
    case SLICING_SERIOUS_WARNING:
        if (state)
            notification_manager.push_slicing_serious_warning_notification(text, conflictObj ? std::vector<ModelObject const*>{conflictObj} : std::vector<ModelObject const*>{});
        else
            notification_manager.close_slicing_serious_warning_notification(text);
        break;
    case SLICING_ERROR:
        if (state)
            notification_manager.push_slicing_error_notification(text, conflictObj ? std::vector<ModelObject const*>{conflictObj} : std::vector<ModelObject const*>{});
        else
            notification_manager.close_slicing_error_notification(text);
        break;
    default:
        break;
    }
}

void GLCanvas3D::_on_mouse(wxMouseEvent& evt)
{

}

bool GLCanvas3D::_is_any_volume_outside() const
{
    for (const GLVolume* volume : m_volumes.volumes) {
        if (volume != nullptr && volume->is_outside)
            return true;
    }

    return false;
}

void GLCanvas3D::_update_selection_from_hover()
{
    if (!m_selection)
        return;

    bool ctrl_pressed = wxGetKeyState(WXK_CONTROL);

    if (m_hover_volume_idxs.empty()) {
        if (!ctrl_pressed && (m_rectangle_selection.get_state() == GLSelectionRectangle::Select))
            m_selection->remove_all();

        return;
    }

    GLSelectionRectangle::EState state = m_rectangle_selection.get_state();

    bool hover_modifiers_only = true;
    for (int i : m_hover_volume_idxs) {
        if (!m_volumes.volumes[i]->is_modifier) {
            hover_modifiers_only = false;
            break;
        }
    }

    bool selection_changed = false;
    if (state == GLSelectionRectangle::Select) {
        bool contains_all = true;
        for (int i : m_hover_volume_idxs) {
            if (!m_selection->contains_volume((unsigned int)i)) {
                contains_all = false;
                break;
            }
        }

        // the selection is going to be modified (Add)
        if (!contains_all) {
            AppAdapter::plater()->take_snapshot(std::string("Select by rectangle"), UndoRedo::SnapshotType::Selection);
            selection_changed = true;
        }
    }
    else {
        bool contains_any = false;
        for (int i : m_hover_volume_idxs) {
            if (m_selection->contains_volume((unsigned int)i)) {
                contains_any = true;
                break;
            }
        }

        // the selection is going to be modified (Remove)
        if (contains_any) {
            AppAdapter::plater()->take_snapshot(std::string("Unselect by rectangle"), UndoRedo::SnapshotType::Selection);
            selection_changed = true;
        }
    }

    if (!selection_changed)
        return;

    Plater::SuppressSnapshots suppress(AppAdapter::plater());

    if ((state == GLSelectionRectangle::Select) && !ctrl_pressed)
        m_selection->clear();

    for (int i : m_hover_volume_idxs) {
        if (state == GLSelectionRectangle::Select) {
            if (hover_modifiers_only) {
                const GLVolume& v = *m_volumes.volumes[i];
                m_selection->add_volume(v.object_idx(), v.volume_idx(), v.instance_idx(), false);
            }
            else
                m_selection->add(i, false);
        }
        else
            m_selection->remove(i);
    }

    if (m_selection->is_empty())
        m_gizmos->reset_all_states();
    else
        m_gizmos->refresh_on_off_state();

    m_gizmos->update_data();
    post_event(SimpleEvent(EVT_GLCANVAS_OBJECT_SELECT));
    m_dirty = true;
}

bool GLCanvas3D::_deactivate_arrange_menu()
{
    if (m_main_toolbar.is_item_pressed("arrange")) {
        m_main_toolbar.force_right_action(m_main_toolbar.get_item_id("arrange"), *this);
        return true;
    }

    return false;
}

//BBS: add deactivate orient menu
bool GLCanvas3D::_deactivate_orient_menu()
{
    if (m_main_toolbar.is_item_pressed("orient")) {
        m_main_toolbar.force_right_action(m_main_toolbar.get_item_id("orient"), *this);
        return true;
    }

    return false;
}

//BBS: add deactivate layersediting menu
bool GLCanvas3D::_deactivate_layersediting_menu()
{
    if (m_main_toolbar.is_item_pressed("layersediting")) {
        m_main_toolbar.force_left_action(m_main_toolbar.get_item_id("layersediting"), *this);
        return true;
    }

    return false;
}

bool GLCanvas3D::_deactivate_collapse_toolbar_items()
{
    GLToolbar& collapse_toolbar = AppAdapter::plater()->get_collapse_toolbar();
    if (collapse_toolbar.is_item_pressed("print")) {
        collapse_toolbar.force_left_action(collapse_toolbar.get_item_id("print"), *this);
        return true;
    }

    return false;
}

void GLCanvas3D::highlight_toolbar_item(const std::string& item_name)
{
    GLToolbarItem* item = m_main_toolbar.get_item(item_name);
    if (!item || !item->is_visible())
        return;
    m_toolbar_highlighter.init(item, this);
}

void GLCanvas3D::highlight_gizmo(const std::string& gizmo_name)
{
    GLGizmosManager::EType gizmo = m_gizmos->get_gizmo_from_name(gizmo_name);
    if(gizmo == GLGizmosManager::EType::Undefined)
        return;
    m_gizmo_highlighter.init(m_gizmos, gizmo, this);
}

const Print* GLCanvas3D::fff_print() const
{
    return (m_process == nullptr) ? nullptr : m_process->fff_print();
}


} // namespace GUI
} // namespace Slic3r

#include "PlaterPrivate.hpp"
#include "libslic3r/FileSystem/FileHelp.hpp"

#include "slic3r/Scene/ObjectDataViewModel.hpp"
#include "slic3r/Global/InstanceCheck.hpp"
#include "slic3r/Render/ImGUI/IMSlider.hpp"
#include "slic3r/Slice/SlicingProcessCompletedEvent.hpp"

#include "slic3r/GUI/Frame/View3D.hpp"
#include "slic3r/GUI/Frame/GCodePreview.hpp"

#include "slic3r/Render/SceneRaycaster.hpp"
#include "slic3r/Render/GLCanvas3DFacade.hpp"
#include "slic3r/Render/RenderUtils.hpp"
#include "slic3r/Render/GCodePreviewCanvas.hpp"
#include "slic3r/Render/View3DCanvas.hpp"
#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Slice/GCodeResultWrapper.hpp"

#include "slic3r/Render/PlateBed.hpp"
#include "slic3r/GUI/Frame/OpenGLPanel.hpp"
#include "slic3r/GUI/MainFrame.hpp"

#include "lmwx/interface/framework_interface.h"

namespace Slic3r {
namespace GUI {

namespace {
bool emboss_svg(Plater& plater, const wxString &svg_file, const Vec2d& mouse_drop_position)
{
    return false;
}
}

void plater_get_position(wxWindowBase* child, wxWindowBase* until_parent, int& x, int& y) {
    int res_x = 0, res_y = 0;

    while (child != until_parent && child != nullptr) {
        int _x, _y;
        child->GetPosition(&_x, &_y);
        res_x += _x;
        res_y += _y;

        child = child->GetParent();
    }

    x = res_x;
    y = res_y;
}

Sidebar::priv::priv(Plater *plater)
    : plater(plater)
{

}

Sidebar::priv::~priv()
{
    delete object_settings;
}

void Sidebar::priv::show_preset_comboboxes()
{
    scrolled->GetParent()->Layout();
    scrolled->Refresh();
}

void Sidebar::priv::on_search_update()
{
    m_object_list->assembly_plate_object_name();

    wxString search_text = m_search_bar->GetValue();
    m_object_list->GetModel()->search_object(search_text);
    dia->update_list();
}

void Sidebar::priv::jump_to_object(ObjectDataViewModelNode* item)
{
    m_object_list->selected_object(item);
}

void Sidebar::priv::can_search()
{
    if (m_search_bar->IsShown()) {
        m_search_bar->SetFocus();
    }
}

Plater::priv::priv(Plater *q, MainPanel *main_panel)
    : q(q)
    , main_panel(main_panel)
    //BBS: add bed_exclude_area
    , config(Slic3r::DynamicPrintConfig::new_from_defaults_keys({
        "printable_area", "bed_exclude_area", "bed_custom_texture", "bed_custom_model", "print_sequence",
        "extruder_clearance_radius", "extruder_clearance_height_to_lid", "extruder_clearance_height_to_rod",
		"nozzle_height", "skirt_type", "skirt_loops", "skirt_speed","min_skirt_length", "skirt_distance", "skirt_start_angle",
        "brim_width", "brim_object_gap", "brim_type", "nozzle_diameter", "single_extruder_multi_material", "preferred_orientation",
        "enable_prime_tower", "wipe_tower_x", "wipe_tower_y", "prime_tower_width", "prime_tower_brim_width", "prime_volume",
        "extruder_colour", "filament_colour", "material_colour", "printable_height", "printer_model",
        // These values are necessary to construct SlicingParameters by the Canvas3D variable layer height editor.
        "layer_height", "initial_layer_print_height", "min_layer_height", "max_layer_height",
        "brim_width", "wall_loops", "wall_filament", "sparse_infill_density", "sparse_infill_filament", "top_shell_layers",
        "enable_support", "support_filament", "support_interface_filament",
        "support_top_z_distance", "support_bottom_z_distance", "raft_layers",
        "wipe_tower_rotation_angle", "wipe_tower_cone_angle", "wipe_tower_extra_spacing", "wipe_tower_extra_flow", "wipe_tower_max_purge_speed", "wipe_tower_filament",
        "best_object_pos", "idex_mode","hot_bed_divide"
        }))
    , sidebar(new Sidebar(q))
    , notification_manager(std::make_unique<NotificationManager>(q))
    , m_worker{q, std::make_unique<NotificationProgressIndicator>(notification_manager.get()), "ui_worker"}
    , m_job_prepare_state(Job::JobPrepareState::PREPARE_STATE_DEFAULT)
    , delayed_scene_refresh(false)
    , collapse_toolbar(GLToolbar::Normal, "Collapse")
    //BBS :partplatelist construction
    , partplate_list(this->q, &model)
{
}

void Plater::priv::init(Plater *q, MainPanel *main_panel)
{
    m_is_dark = AppAdapter::app_config()->get("dark_color_mode") == "1";

    m_aui_mgr.SetManagedWindow(q);
    m_aui_mgr.SetDockSizeConstraint(1, 1);
    //m_aui_mgr.GetArtProvider()->SetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE, 0);
    //m_aui_mgr.GetArtProvider()->SetMetric(wxAUI_DOCKART_SASH_SIZE, 2);
    m_aui_mgr.GetArtProvider()->SetMetric(wxAUI_DOCKART_CAPTION_SIZE, 18);
    m_aui_mgr.GetArtProvider()->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE, wxAUI_GRADIENT_NONE);

    this->q->SetFont(Slic3r::GUI::app_normal_font());

    //BBS: use the first partplate's print for background process
    partplate_list.update_slice_context_to_current_plate(background_process);

    // BBS: to be checked. Not follow patch.
    background_process.set_thumbnail_cb([this](const ThumbnailsParams& params) {
        return this->generate_thumbnails(params, Camera::EType::Ortho); 
    });

    this->q->Bind(EVT_SLICING_UPDATE, &priv::on_slicing_update, this);
    this->q->Bind(EVT_REPAIR_MODEL, &priv::on_repair_model, this);
    this->q->Bind(EVT_FILAMENT_COLOR_CHANGED, &priv::on_filament_color_changed, this);
    this->q->Bind(EVT_UPDATE_PLUGINS_WHEN_LAUNCH, &priv::update_plugin_when_launch, this);
    this->q->Bind(EVT_PREVIEW_ONLY_MODE_HINT, &priv::show_preview_only_hint, this);
    this->q->Bind(EVT_GLCANVAS_COLOR_MODE_CHANGED, &priv::on_change_color_mode, this);
    this->q->Bind(wxEVT_SYS_COLOUR_CHANGED, &priv::on_apple_change_color_mode, this);
    this->q->Bind(EVT_CREATE_FILAMENT, &priv::on_create_filament, this);
    this->q->Bind(EVT_MODIFY_FILAMENT, &priv::on_modify_filament, this);
    this->q->Bind(EVT_ADD_CUSTOM_FILAMENT, &priv::on_add_custom_filament, this);
    main_panel->m_tabpanel->Bind(wxEVT_NOTEBOOK_PAGE_CHANGING, &priv::on_tab_selection_changing, this);

    auto* panel_3d = new wxPanel(q);
    view3D = new View3D(panel_3d, &model, config, &background_process, &m_selection);
    //BBS: use partplater's gcode
    preview = new GCodePreview(panel_3d, &model, config, &background_process, &partplate_list, partplate_list.get_current_slice_result_wrapper(), [this]() { schedule_background_process(); });

    {
        View3DCanvas* view3d_canvas = static_cast<View3DCanvas*>(view3D->get_canvas3d());
        m_canvas = new GLCanvas3DFacade(view3d_canvas, preview->get_canvas3d());
        m_gizmos = new GLGizmosManager(m_canvas);
        m_scene_raycaster = view3d_canvas->get_scene_raycaster();
        view3d_canvas->set_gizmos_manager(m_gizmos);
    }

    panels.push_back(view3D);
    panels.push_back(preview);

    this->background_process_timer.SetOwner(this->q, 0);
    this->q->Bind(wxEVT_TIMER, [this](wxTimerEvent &evt)
    {
        if (!this->suppressed_backround_processing_update) {
            // Skip auto update for external gcode to prevent data reset
            PartPlate* current_plate = this->partplate_list.get_curr_plate();
            if (current_plate && current_plate->has_external_gcode()) {
                return;
            }
            this->update_restart_background_process(false, false);
        }
    });

    update();

    // Orca: Make sidebar dockable
    m_aui_mgr.AddPane(sidebar, wxAuiPaneInfo()
                                   .Name("sidebar")
                                   .Left()
                                   .CloseButton(false)
                                   .TopDockable(false)
                                   .BottomDockable(false)
                                   .Floatable(true)
                                   .BestSize(wxSize(42 * app_em_unit(), 90 * app_em_unit())));

    auto* panel_sizer = new wxBoxSizer(wxHORIZONTAL);
    panel_sizer->Add(view3D, 1, wxEXPAND | wxALL, 0);
    panel_sizer->Add(preview, 1, wxEXPAND | wxALL, 0);
    panel_3d->SetSizer(panel_sizer);
    m_aui_mgr.AddPane(panel_3d, wxAuiPaneInfo().Name("main").CenterPane().PaneBorder(false));

    m_default_window_layout = m_aui_mgr.SavePerspective();

    {
        auto& sidebar = m_aui_mgr.GetPane(this->sidebar);

        // Load previous window layout
        const auto cfg    = AppAdapter::app_config();
        wxString   layout = wxString::FromUTF8(cfg->get("window_layout"));
        if (!layout.empty()) {
            m_aui_mgr.LoadPerspective(layout, false);
            sidebar_layout.is_collapsed = !sidebar.IsShown();
        }

        // Keep tracking the current sidebar size, by storing it using `best_size`, which will be stored
        // in the config and re-applied when the app is opened again.
        this->sidebar->Bind(wxEVT_IDLE, [&sidebar, this](wxIdleEvent& e) {
            if (sidebar.IsShown() && sidebar.IsDocked() && sidebar.rect.GetWidth() > 0) {
                sidebar.BestSize(sidebar.rect.GetWidth(), sidebar.best_size.GetHeight());
            }
            e.Skip();
        });

        // Hide sidebar initially, will re-show it after initialization when we got proper window size
        sidebar.Hide();
        m_aui_mgr.Update();
    }

    menus.init(main_panel);


    // Events:

    if (true) {
        // Preset change event
        sidebar->Bind(wxEVT_COMBOBOX, &priv::on_combobox_select, this);
        sidebar->Bind(EVT_OBJ_LIST_OBJECT_SELECT, [this](wxEvent&) { priv::selection_changed(); });
        // BBS: should bind BACKGROUND_PROCESS event to plater
        q->Bind(EVT_SCHEDULE_BACKGROUND_PROCESS, [this](SimpleEvent&) { this->schedule_background_process(); });
        // jump to found option from SearchDialog
        q->Bind(wxCUSTOMEVT_JUMP_TO_OPTION, [this](wxCommandEvent& evt) { sidebar->jump_to_option(evt.GetInt()); });
        q->Bind(wxCUSTOMEVT_JUMP_TO_OBJECT, [this](wxCommandEvent& evt) {
            auto client_data = evt.GetClientData();
            ObjectDataViewModelNode* data = static_cast<ObjectDataViewModelNode*>(client_data);
            sidebar->jump_to_object(data);
            }
        );
    }

    wxGLCanvas* view3D_canvas = view3D->get_wxglcanvas();
    //BBS: GUI refactor
    wxGLCanvas* preview_canvas = preview->get_wxglcanvas();

    if (true) {
        // 3DScene events:
        view3D_canvas->Bind(EVT_GLCANVAS_SCHEDULE_BACKGROUND_PROCESS, [this](SimpleEvent&) {
            this->background_process_timer.Start(500, wxTIMER_ONE_SHOT);
            });
        view3D_canvas->Bind(EVT_GLCANVAS_OBJECT_SELECT, &priv::on_object_select, this);
        view3D_canvas->Bind(EVT_GLCANVAS_RIGHT_CLICK, &priv::on_right_click, this);
        //BBS: add part plate related logic
        view3D_canvas->Bind(EVT_GLCANVAS_PLATE_RIGHT_CLICK, &priv::on_plate_right_click, this);
        view3D_canvas->Bind(EVT_GLCANVAS_REMOVE_OBJECT, [q](SimpleEvent&) { q->remove_selected(); });
        view3D_canvas->Bind(EVT_GLCANVAS_ARRANGE, [this](SimpleEvent& evt) {
            //BBS arrange from EVT set default state.
            this->q->set_prepare_state(Job::PREPARE_STATE_DEFAULT);
            this->q->arrange(); });
        view3D_canvas->Bind(EVT_GLCANVAS_ARRANGE_PARTPLATE, [this](SimpleEvent& evt) {
            //BBS arrange from EVT set default state.
            this->q->set_prepare_state(Job::PREPARE_STATE_MENU);
            this->q->arrange(); });
        view3D_canvas->Bind(EVT_GLCANVAS_ORIENT, [this](SimpleEvent& evt) {
            //BBS orient from EVT set default state.
            this->q->set_prepare_state(Job::PREPARE_STATE_DEFAULT);
            this->q->orient(); });
        view3D_canvas->Bind(EVT_GLCANVAS_ORIENT_PARTPLATE, [this](SimpleEvent& evt) {
            //BBS orient from EVT set default state.
            this->q->set_prepare_state(Job::PREPARE_STATE_MENU);
            this->q->orient(); });
        //BBS
        view3D_canvas->Bind(EVT_GLCANVAS_SELECT_CURR_PLATE_ALL, [this](SimpleEvent&) {this->q->select_curr_plate_all(); });

        view3D_canvas->Bind(EVT_GLCANVAS_SELECT_ALL, [this](SimpleEvent&) { this->q->select_all(); });
        view3D_canvas->Bind(EVT_GLCANVAS_QUESTION_MARK, [](SimpleEvent&) { open_keyboard_shortcuts_dialog(); });
        view3D_canvas->Bind(EVT_GLCANVAS_INSTANCE_MOVED, [this](SimpleEvent&) { update(0, false); });
        view3D_canvas->Bind(EVT_GLCANVAS_FORCE_UPDATE, [this](SimpleEvent&) { update(); });
        view3D_canvas->Bind(EVT_GLCANVAS_INSTANCE_ROTATED, [this](SimpleEvent&) { update(0, false); });
        view3D_canvas->Bind(EVT_GLCANVAS_INSTANCE_SCALED, [this](SimpleEvent&) { update(0, false); });
        view3D_canvas->Bind(EVT_GLCANVAS_ENABLE_ACTION_BUTTONS, [this](Event<bool>& evt) { on_slice_button_status(evt.data); });
        view3D_canvas->Bind(EVT_GLCANVAS_UPDATE_GEOMETRY, &priv::on_update_geometry, this);
        view3D_canvas->Bind(EVT_GLCANVAS_MOUSE_DRAGGING_STARTED, &priv::on_3dcanvas_mouse_dragging_started, this);
        view3D_canvas->Bind(EVT_GLCANVAS_MOUSE_DRAGGING_FINISHED, &priv::on_3dcanvas_mouse_dragging_finished, this);
        view3D_canvas->Bind(EVT_GLCANVAS_TAB, [this](SimpleEvent&) { select_next_view_3D(); });
        view3D_canvas->Bind(EVT_GLCANVAS_RESETGIZMOS, [this](SimpleEvent&) { reset_all_gizmos(); });
        view3D_canvas->Bind(EVT_GLCANVAS_UNDO, [this](SimpleEvent&) { this->undo(); });
        view3D_canvas->Bind(EVT_GLCANVAS_REDO, [this](SimpleEvent&) { this->redo(); });
        view3D_canvas->Bind(EVT_GLCANVAS_COLLAPSE_SIDEBAR, [this](SimpleEvent&) { this->q->collapse_sidebar(!this->q->is_sidebar_collapsed());  });
        view3D_canvas->Bind(EVT_GLCANVAS_RESET_LAYER_HEIGHT_PROFILE, [this](SimpleEvent&) { this->view3D->get_canvas3d()->reset_layer_height_profile(); });
        view3D_canvas->Bind(EVT_GLCANVAS_ADAPTIVE_LAYER_HEIGHT_PROFILE, [this](Event<float>& evt) { this->view3D->get_canvas3d()->adaptive_layer_height_profile(evt.data); });
        view3D_canvas->Bind(EVT_GLCANVAS_SMOOTH_LAYER_HEIGHT_PROFILE, [this](HeightProfileSmoothEvent& evt) { this->view3D->get_canvas3d()->smooth_layer_height_profile(evt.data); });
        view3D_canvas->Bind(EVT_GLCANVAS_RELOAD_FROM_DISK, [this](SimpleEvent&) { this->reload_all_from_disk(); });

        // 3DScene/Toolbar:
        view3D_canvas->Bind(EVT_GLTOOLBAR_ADD, &priv::on_action_add, this);
        view3D_canvas->Bind(EVT_GLTOOLBAR_DELETE, [q](SimpleEvent&) { q->remove_selected(); });
        view3D_canvas->Bind(EVT_GLTOOLBAR_DELETE_ALL, [this](SimpleEvent&) { delete_all_objects_from_model(); });
//        view3D_canvas->Bind(EVT_GLTOOLBAR_DELETE_ALL, [q](SimpleEvent&) { q->reset_with_confirm(); });

        view3D_canvas->Bind(EVT_GLTOOLBAR_ADD_PLATE, &priv::on_action_add_plate, this);
        view3D_canvas->Bind(EVT_GLTOOLBAR_DEL_PLATE, &priv::on_action_del_plate, this);
        view3D_canvas->Bind(EVT_GLTOOLBAR_ORIENT, [this](SimpleEvent&) {
            //BBS arrange from EVT set default state.
            this->q->set_prepare_state(Job::PREPARE_STATE_DEFAULT);
            this->q->orient(); });
        view3D_canvas->Bind(EVT_GLTOOLBAR_ARRANGE, [this](SimpleEvent&) {
            //BBS arrange from EVT set default state.
            this->q->set_prepare_state(Job::PREPARE_STATE_DEFAULT);
            this->q->arrange();
            });
        view3D_canvas->Bind(EVT_GLTOOLBAR_CUT, [q](SimpleEvent&) { q->cut_selection_to_clipboard(); });
        view3D_canvas->Bind(EVT_GLTOOLBAR_COPY, [q](SimpleEvent&) { q->copy_selection_to_clipboard(); });
        view3D_canvas->Bind(EVT_GLTOOLBAR_PASTE, [q](SimpleEvent&) { q->paste_from_clipboard(); });
        view3D_canvas->Bind(EVT_GLTOOLBAR_LAYERSEDITING, &priv::on_action_layersediting, this);
        //BBS: add clone
        view3D_canvas->Bind(EVT_GLTOOLBAR_CLONE, [q](SimpleEvent&) { q->clone_selection(); });
        view3D_canvas->Bind(EVT_GLTOOLBAR_SPLIT_OBJECTS, &priv::on_action_split_objects, this);
        view3D_canvas->Bind(EVT_GLTOOLBAR_SPLIT_VOLUMES, &priv::on_action_split_volumes, this);
        //BBS: GUI refactor: GLToolbar
        view3D_canvas->Bind(EVT_GLTOOLBAR_OPEN_PROJECT, &priv::on_action_open_project, this);
        view3D_canvas->Bind(EVT_GLCANVAS_SWITCH_TO_OBJECT, [main_panel](SimpleEvent&) {
                if (main_panel->m_param_panel) {
                    main_panel->m_param_panel->switch_to_object(false);
                }
            });
        view3D_canvas->Bind(EVT_GLCANVAS_SWITCH_TO_GLOBAL, [main_panel](SimpleEvent&) {
                if (main_panel->m_param_panel) {
                    main_panel->m_param_panel->switch_to_global();
                }
            });
    }

    // Preview events:
    preview->get_wxglcanvas()->Bind(EVT_GLCANVAS_QUESTION_MARK, [](SimpleEvent&) { open_keyboard_shortcuts_dialog(); });

    preview->get_wxglcanvas()->Bind(EVT_GLCANVAS_UPDATE, [this](SimpleEvent &) {
            preview->get_canvas3d()->set_as_dirty();
        });
    if (true) {
        preview->get_wxglcanvas()->Bind(EVT_GLCANVAS_TAB, [this](SimpleEvent&) { select_next_view_3D(); });
        preview->get_wxglcanvas()->Bind(EVT_GLCANVAS_COLLAPSE_SIDEBAR, [this](SimpleEvent&) { this->q->collapse_sidebar(!this->q->is_sidebar_collapsed());  });
        preview->get_wxglcanvas()->Bind(EVT_CUSTOMEVT_TICKSCHANGED, [this](wxCommandEvent& event) {
            Type tick_event_type = (Type)event.GetInt();
            Model& model = AppAdapter::plater()->model();
            //BBS: replace model custom gcode with current plate custom gcode
            model.plates_custom_gcodes[model.curr_plate_index] = preview->get_layers_slider()->GetTicksValues();

            // BBS set to invalid state only
            if (tick_event_type == Type::ToolChange || tick_event_type == Type::Custom || tick_event_type == Type::Template || tick_event_type == Type::PausePrint) {
                PartPlate *plate = this->q->get_partplate_list().get_curr_plate();
                // Protect external gcode: don't mark as invalid when ticks changed
                // External gcode cannot be resliced, should keep displaying
                if (plate && !plate->has_external_gcode()) {
                    plate->update_slice_result_valid_state(false);
                    BOOST_LOG_TRIVIAL(info) << "Tick changed: Plate " << plate->get_index() 
                        << " marked as invalid (will need reslice)";
                } else if (plate) {
                    BOOST_LOG_TRIVIAL(info) << "Tick changed: Plate " << plate->get_index() 
                        << " has external gcode, keeping valid state";
                }
            }
            set_plater_dirty(true);

            preview->on_tick_changed();

            // update slice and print button
            AppAdapter::main_panel()->update_slice_print_status(MainPanel::SlicePrintEventType::eEventSliceUpdate, true, false);
            set_need_update(true);
        });
    }

    if (true) {
        q->Bind(EVT_SLICING_COMPLETED, &priv::on_slicing_completed, this);
        q->Bind(EVT_PROCESS_COMPLETED, &priv::on_process_completed, this);
        q->Bind(EVT_EXPORT_BEGAN, &priv::on_export_began, this);
        q->Bind(EVT_EXPORT_FINISHED, &priv::on_export_finished, this);
        q->Bind(EVT_GLVIEWTOOLBAR_3D, [q](SimpleEvent&) {
            q->select_view3d(); 
        });
        //BBS: set on_slice to false
        q->Bind(EVT_GLVIEWTOOLBAR_PREVIEW, [q](SimpleEvent&) {
            q->select_preview(false); 
        });
        q->Bind(EVT_GLTOOLBAR_SLICE_PLATE, &priv::on_action_slice_plate, this);
        q->Bind(EVT_GLTOOLBAR_SLICE_ALL, &priv::on_action_slice_all, this);
        q->Bind(EVT_GLTOOLBAR_PRINT_PLATE, &priv::on_action_print_plate, this);
        q->Bind(EVT_GLTOOLBAR_SELECT_SLICED_PLATE, &priv::on_action_select_sliced_plate, this);
        q->Bind(EVT_GLTOOLBAR_PRINT_ALL, &priv::on_action_print_all, this);
        q->Bind(EVT_GLTOOLBAR_EXPORT_GCODE, &priv::on_action_export_gcode, this);
        q->Bind(EVT_GLTOOLBAR_SEND_GCODE, &priv::on_action_send_gcode, this);
        q->Bind(EVT_GLTOOLBAR_EXPORT_SLICED_FILE, &priv::on_action_export_sliced_file, this);
        q->Bind(EVT_GLTOOLBAR_EXPORT_ALL_SLICED_FILE, &priv::on_action_export_all_sliced_file, this);
        q->Bind(EVT_GLTOOLBAR_PRINT_MULTI_MACHINE, &priv::on_action_send_to_multi_machine, this);
        q->Bind(EVT_GLCANVAS_PLATE_SELECT, &priv::on_plate_selected, this);
        q->Bind(EVT_IMPORT_MODEL_ID, &priv::on_action_request_model_id, this);
        q->Bind(EVT_PRINT_FINISHED, [q](wxCommandEvent& evt) { q->print_job_finished(evt); });
        q->Bind(EVT_SEND_FINISHED, [q](wxCommandEvent& evt) { q->send_job_finished(evt); });
        q->Bind(EVT_PUBLISH_FINISHED, [q](wxCommandEvent& evt) { q->publish_job_finished(evt);});
        q->Bind(EVT_OPEN_PLATESETTINGSDIALOG, [q](wxCommandEvent& evt) { q->open_platesettings_dialog(evt);});
    }

    // Drop target:
    q->SetDropTarget(new PlaterDropTarget(*main_panel, *q));   // if my understanding is right, wxWindow takes the owenership
    q->Layout();

    apply_color_mode();

    select_view3d();

    // updates camera type from .ini file
    camera.enable_update_config_on_type_change(true);
    // BBS set config
    bool use_perspective_camera = get_config("use_perspective_camera").compare("true") == 0;
    if (use_perspective_camera) {
        camera.set_type(Camera::EType::Perspective);
    } else {
        camera.set_type(Camera::EType::Ortho);
    }

    // Load the 3DConnexion device database.
    mouse3d_controller.load_config(*AppAdapter::app_config());
    // Start the background thread to detect and connect to a HID device (Windows and Linux).
    // Connect to a 3DConnextion driver (OSX).
    mouse3d_controller.init();
#ifdef _WIN32
    // Register an USB HID (Human Interface Device) attach event. evt contains Win32 path to the USB device containing VID, PID and other info.
    // This event wakes up the Mouse3DController's background thread to enumerate HID devices, if the VID of the callback event
    // is one of the 3D Mouse vendors (3DConnexion or Logitech).
    this->q->Bind(EVT_HID_DEVICE_ATTACHED, [this](HIDDeviceAttachedEvent &evt) {
        mouse3d_controller.device_attached(evt.data);
        });
    this->q->Bind(EVT_HID_DEVICE_DETACHED, [this](HIDDeviceAttachedEvent& evt) {
        mouse3d_controller.device_detached(evt.data);
        });
#endif /* _WIN32 */

    if (true) {
        this->q->Bind(EVT_EJECT_DRIVE_NOTIFICAION_CLICKED, [this](EjectDriveNotificationClickedEvent&) { this->q->eject_drive(); });

        this->q->Bind(EVT_EXPORT_GCODE_NOTIFICAION_CLICKED, [this](ExportGcodeNotificationClickedEvent&) {
            this->q->export_gcode(true);
        });

        /* BBS do not handle removeable driver event */
        this->q->Bind(EVT_REMOVABLE_DRIVE_EJECTED, [this](RemovableDriveEjectEvent &evt) {
            if (evt.data.second) {
                // BBS
                //this->show_action_buttons(this->ready_to_slice);
                notification_manager->close_notification_of_type(NotificationType::ExportFinished);
                notification_manager->push_notification(NotificationType::CustomNotification,
                                                        NotificationManager::NotificationLevel::RegularNotificationLevel,
                                                        format(_L("Successfully unmounted. The device %s(%s) can now be safely removed from the computer."), evt.data.first.name, evt.data.first.path)
                    );
            } else {
                notification_manager->push_notification(NotificationType::CustomNotification,
                                                        NotificationManager::NotificationLevel::ErrorNotificationLevel,
                                                        format(_L("Ejecting of device %s(%s) has failed."), evt.data.first.name, evt.data.first.path)
                    );
            }
        });
        this->q->Bind(EVT_REMOVABLE_DRIVES_CHANGED, [this](RemovableDrivesChangedEvent &) {
            // BBS
            //this->show_action_buttons(this->ready_to_slice);
            // Close notification ExportingFinished but only if last export was to removable
            notification_manager->device_ejected();
        });
        // Start the background thread and register this window as a target for update events.
        AppAdapter::gui_app()->removable_drive_manager()->init(this->q);
#ifdef _WIN32
        //Trigger enumeration of removable media on Win32 notification.
        this->q->Bind(EVT_VOLUME_ATTACHED, [this](VolumeAttachedEvent &evt) { AppAdapter::gui_app()->removable_drive_manager()->volumes_changed(); });
        this->q->Bind(EVT_VOLUME_DETACHED, [this](VolumeDetachedEvent &evt) { AppAdapter::gui_app()->removable_drive_manager()->volumes_changed(); });
#endif /* _WIN32 */
    }

    // Initialize the Undo / Redo stack with a first snapshot.
    //this->take_snapshot("New Project", UndoRedo::SnapshotType::ProjectSeparator);
    // Reset the "dirty project" flag.
    m_undo_redo_stack_main.mark_current_as_saved();
    dirty_state.update_from_undo_redo_stack(false);
    //this->take_snapshot("New Project");
    // BBS: save project confirm
    up_to_date(true, false);
    up_to_date(true, true);
    model.set_need_backup();

    // BBS: restore project
    if (true) {
        auto last_backup = AppAdapter::app_config()->get_last_backup_dir();
        this->q->Bind(EVT_RESTORE_PROJECT, [this, last = last_backup](wxCommandEvent& e) {
            std::string last_backup = last;
            std::string originfile;
            if (Slic3r::has_restore_data(last_backup, originfile)) {
                auto result = MessageDialog(this->q, _L("Previous unsaved project detected, do you want to restore it?"), wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Restore"), wxYES_NO | wxYES_DEFAULT | wxCENTRE).ShowModal();
                if (result == wxID_YES) {
                    this->q->load_project(from_path(last_backup), from_path(originfile));
                    Slic3r::backup_soon();
                    return;
                }
            }
            try {
                if (originfile != "<lock>") // see bbs_3mf.cpp for lock detail
                    boost::filesystem::remove_all(last);
            }
            catch (...) {}
            int skip_confirm = e.GetInt();
            this->q->new_project(skip_confirm, true);
            });
        //wxPostEvent(this->q, wxCommandEvent{EVT_RESTORE_PROJECT});
    }

    this->q->Bind(EVT_LOAD_MODEL_OTHER_INSTANCE, [this](LoadFromOtherInstanceEvent& evt) {
        BOOST_LOG_TRIVIAL(trace) << "Received load from other instance event.";
        wxArrayString input_files;
        for (size_t i = 0; i < evt.data.size(); ++i) {
            input_files.push_back(from_u8(evt.data[i].string()));
        }
        AppAdapter::main_panel()->Raise();
        this->q->load_files(input_files);
    });
    
    this->q->Bind(EVT_START_DOWNLOAD_OTHER_INSTANCE, [](StartDownloadOtherInstanceEvent& evt) {
        BOOST_LOG_TRIVIAL(trace) << "Received url from other instance event.";
        AppAdapter::main_panel()->Raise();
        for (size_t i = 0; i < evt.data.size(); ++i) {
            AppAdapter::gui_app()->start_download(evt.data[i]);
        }
       
    });
    this->q->Bind(EVT_INSTANCE_GO_TO_FRONT, [this](InstanceGoToFrontEvent &) {
        bring_instance_forward();
    });
    other_instance_message_handler()->init(this->q);

    update_sidebar(true);
}

Plater::priv::~priv()
{
    if (config != nullptr)
        delete config;
    // Saves the database of visited (already shown) hints into hints.ini.
    notification_manager->deactivate_loaded_hints();
    main_panel->m_tabpanel->Unbind(wxEVT_NOTEBOOK_PAGE_CHANGING, &priv::on_tab_selection_changing, this);


    delete m_gizmos;
}

void Plater::priv::select_printer_preset(const std::string& preset_name)
{
    // BBS:Save the plate parameters before switching
    PartPlateList& old_plate_list = this->partplate_list;
    PartPlate* old_plate = old_plate_list.get_selected_plate();
    Vec3d old_plate_pos = old_plate->get_center_origin();

    // BBS: Save the model in the current platelist
    std::vector<std::vector<int> > plate_object;
    for (size_t i = 0; i < old_plate_list.get_plate_count(); ++i) {
        PartPlate* plate = old_plate_list.get_plate(i);
        std::vector<int> obj_idxs;
        for (int obj_idx = 0; obj_idx < model.objects.size(); obj_idx++) {
            if (plate && plate->contain_instance(obj_idx, 0)) {
                obj_idxs.emplace_back(obj_idx);
            }
        }
        plate_object.emplace_back(obj_idxs);
    }

    //BBS
    //wxWindowUpdateLocker noUpdates1(sidebar->print_panel());
    wxWindowUpdateLocker noUpdates2(sidebar->filament_panel());
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->select_preset(preset_name);

    // update plater with new config
    q->on_config_change(app_preset_bundle()->full_config());

    AppAdapter::obj_list()->update_object_list_by_printer_technology();

    // BBS:Model reset by plate center
    PartPlateList& cur_plate_list = this->partplate_list;
    PartPlate* cur_plate = cur_plate_list.get_curr_plate();
    Vec3d cur_plate_pos = cur_plate->get_center_origin();

    if (old_plate_pos.x() != cur_plate_pos.x() || old_plate_pos.y() != cur_plate_pos.y()) {
        for (int i = 0; i < plate_object.size(); ++i) {
            view3D->select_object_from_idx(plate_object[i]);
            this->sidebar->obj_list()->update_selections();
            view3D->center_selected_plate(i);
        }

        view3D->deselect_all();
    }

    // update slice state and set bedtype default for 3rd-party printer
    auto plate_list = partplate_list.get_plate_list();
    for (auto plate : plate_list) {
         plate->update_slice_result_valid_state(false);
    }
}

void Plater::priv::update(unsigned int flags, bool reload_scene)
{
    unsigned int update_status = 0;
    const bool force_background_processing_restart = (flags & (unsigned int)UpdateParams::FORCE_BACKGROUND_PROCESSING_UPDATE);
    if (force_background_processing_restart)
        // Update the SLAPrint from the current Model, so that the reload_scene()
        // pulls the correct data.
        update_status = this->update_background_process(false);

    //BBS TODO reload_scene
    this->view3D->reload_scene(false, flags & (unsigned int)UpdateParams::FORCE_FULL_SCREEN_REFRESH);
    if (reload_scene)
    {
        this->preview->reload_print();
    }

    if (current_panel && q->is_preview_shown()) {
        q->force_update_all_plate_thumbnails();
        //update_fff_scene_only_shells(true);
    }

    if (force_background_processing_restart)
        this->restart_background_process(update_status);
    else
        this->schedule_background_process();

    update_sidebar();
}

void Plater::priv::select_view(const std::string& direction)
{
    if (current_panel == view3D) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << __LINE__ << "select view3D";
        view3D->select_view(direction);
        AppAdapter::gui_app()->update_ui_from_settings();
    }
    else if (current_panel == preview) {
        BOOST_LOG_TRIVIAL(info) << "select preview";
        preview->select_view(direction);
        AppAdapter::gui_app()->update_ui_from_settings();
    }
}

void Plater::priv::apply_free_camera_correction(bool apply/* = true*/)
{
    bool use_perspective_camera = get_config("use_perspective_camera").compare("true") == 0;
    if (use_perspective_camera)
        camera.set_type(Camera::EType::Perspective);
    else
        camera.set_type(Camera::EType::Ortho);
    if (apply && AppAdapter::app_config()->get_bool("use_free_camera"))
        camera.recover_from_free_camera();
}

void Plater::priv::select_next_view_3D()
{
    
    if (current_panel == view3D)
        AppAdapter::main_panel()->select_tab(size_t(MainPanel::tpPreview));
    else if (current_panel == preview)
        AppAdapter::main_panel()->select_tab(size_t(MainPanel::tp3DEditor));
}

bool Plater::priv::is_preview_shown() const 
{ 
    return current_panel == preview; 
}

bool Plater::priv::is_preview_loaded() const 
{ 
    return preview->is_loaded(); 
}

bool Plater::priv::is_view3D_shown() const 
{ 
    return current_panel == view3D; 
}

bool Plater::priv::are_view3D_labels_shown() const 
{ 
    return (current_panel == view3D) && view3D->get_canvas3d()->are_labels_shown(); 
}

void Plater::priv::show_view3D_labels(bool show) 
{ 
    if (current_panel == view3D) view3D->get_canvas3d()->show_labels(show); 
}

bool Plater::priv::is_view3D_overhang_shown() const 
{ 
    return (current_panel == view3D) && view3D->get_canvas3d()->is_overhang_shown(); 
}

void Plater::priv::show_view3D_overhang(bool show)
{
    if (current_panel == view3D) view3D->get_canvas3d()->show_overhang(show);
}

bool Plater::priv::is_view3D_layers_editing_enabled() const 
{ 
    return (current_panel == view3D) && view3D->get_canvas3d()->is_layers_editing_enabled(); 
}
    

void Plater::priv::enable_sidebar(bool enabled)
{
    sidebar_layout.is_enabled = enabled;
    update_sidebar();
}

void Plater::priv::collapse_sidebar(bool collapse)
{
    sidebar_layout.is_collapsed = collapse;

    // Now update the tooltip in the toolbar.
    std::string new_tooltip = collapse
                              ? _u8L("Expand sidebar")
                              : _u8L("Collapse sidebar");
    new_tooltip += " [Shift+Tab]";
    int id = collapse_toolbar.get_item_id("collapse_sidebar");
    collapse_toolbar.set_tooltip(id, new_tooltip);

    update_sidebar();
}

void Plater::priv::update_sidebar(bool force_update) {
    auto& sidebar = m_aui_mgr.GetPane(this->sidebar);
    if (!sidebar.IsOk() || this->current_panel == nullptr) {
        return;
    }
    bool  needs_update = force_update;

    if (!sidebar_layout.is_enabled) {
        if (sidebar.IsShown()) {
            sidebar.Hide();
            needs_update = true;
        }
    } else {
        // Only hide if collapsed or is floating and is not 3d view
        const bool should_hide = sidebar_layout.is_collapsed || (sidebar.IsFloating() && !sidebar_layout.show);
        const bool should_show = !should_hide;
        if (should_show != sidebar.IsShown()) {
            sidebar.Show(should_show);
            needs_update = true;
        }
    }

    if (needs_update) {
        notification_manager->set_sidebar_collapsed(sidebar.IsShown());
        m_aui_mgr.Update();
    }
}

void Plater::priv::reset_window_layout()
{
    m_aui_mgr.LoadPerspective(m_default_window_layout, false);
    sidebar_layout.is_collapsed = false;
    update_sidebar(true);
}

Sidebar::DockingState Plater::priv::get_sidebar_docking_state() {
    if (!sidebar_layout.is_enabled) {
        return Sidebar::None;
    }

    const auto& sidebar = m_aui_mgr.GetPane(this->sidebar);
    if(sidebar.IsFloating()) {
        return Sidebar::None;
    }

    return sidebar.dock_direction == wxAUI_DOCK_RIGHT ? Sidebar::Right : Sidebar::Left;
}

void Plater::priv::reset_all_gizmos()
{
    m_gizmos->reset_all_states();
}

// Called after the Preferences dialog is closed and the program settings are saved.
// Update the UI based on the current preferences.
void Plater::priv::update_ui_from_settings()
{
    apply_free_camera_correction();

    view3D->get_canvas3d()->update_ui_from_settings();
    preview->get_canvas3d()->update_ui_from_settings();

    sidebar->update_ui_from_settings();
}

std::string Plater::priv::get_config(const std::string &key) const
{
    return AppAdapter::app_config()->get(key);
}

// BBS: backup & restore
std::vector<size_t> Plater::priv::load_files(const std::vector<fs::path>& input_files, LoadStrategy strategy, bool ask_multi)
{
    std::vector<size_t> empty_result;
    bool dlg_cont = true;
    bool is_user_cancel = false;
    bool translate_old = false;
    int current_width, current_depth, current_height;

    if (input_files.empty()) { return std::vector<size_t>(); }
    
    // SoftFever: ugly fix so we can exist pa calib mode
    background_process.fff_print()->calib_mode() = CalibMode::Calib_None;


    // BBS
    int filaments_cnt = config->opt<ConfigOptionStrings>("filament_colour")->values.size();
    bool one_by_one = input_files.size() == 1;
    if (! one_by_one) {
        for (const auto &path : input_files) {
            if (std::regex_match(path.string(), pattern_bundle)) {
                one_by_one = true;
                break;
            }
        }
    }

    bool load_model = strategy & LoadStrategy::LoadModel;
    bool load_config = strategy & LoadStrategy::LoadConfig;
    bool imperial_units = strategy & LoadStrategy::ImperialUnits;
    bool silence = strategy & LoadStrategy::Silence;

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": load_model %1%, load_config %2%, input_files size %3%")%load_model %load_config %input_files.size();

    const auto loading = _L("Loading") + dots;
    ProgressDialog dlg(loading, "", 100, find_toplevel_parent(q), wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_APP_MODAL);
    wxBusyCursor busy;

    auto *new_model = (!load_model || one_by_one) ? nullptr : new Slic3r::Model();
    std::vector<size_t> obj_idxs;

    std::string  designer_model_id;
    std::string  designer_country_code;

    int answer_convert_from_meters          = wxOK_DEFAULT;
    int answer_convert_from_imperial_units  = wxOK_DEFAULT;
    int tolal_model_count                   = 0;

    int progress_percent = 0;
    int total_files = input_files.size();
    const int stage_percent[IMPORT_STAGE_MAX+1] = {
            5,      // IMPORT_STAGE_RESTORE
            10,     // IMPORT_STAGE_OPEN
            30,     // IMPORT_STAGE_READ_FILES
            50,     // IMPORT_STAGE_EXTRACT
            60,     // IMPORT_STAGE_LOADING_OBJECTS
            70,     // IMPORT_STAGE_LOADING_PLATES
            80,     // IMPORT_STAGE_FINISH
            85,     // IMPORT_STAGE_ADD_INSTANCE
            90,      // IMPORT_STAGE_UPDATE_GCODE
            92,     // IMPORT_STAGE_CHECK_MODE_GCODE
            95,     // UPDATE_GCODE_RESULT
            98,     // IMPORT_LOAD_CONFIG
            99,     // IMPORT_LOAD_MODEL_OBJECTS
            100
     };
    const int step_percent[LOAD_STEP_STAGE_NUM+1] = {
            5,     // LOAD_STEP_STAGE_READ_FILE
            30,     // LOAD_STEP_STAGE_GET_SOLID
            60,     // LOAD_STEP_STAGE_GET_MESH
            100
     };

    const float INPUT_FILES_RATIO            = 0.7;
    const float INIT_MODEL_RATIO             = 0.75;
    const float CENTER_AROUND_ORIGIN_RATIO   = 0.8;
    const float LOAD_MODEL_RATIO             = 0.9;

    for (size_t i = 0; i < input_files.size(); ++i) {
        int file_percent = 0;

#ifdef _WIN32
        auto path = input_files[i];
        // On Windows, we swap slashes to back slashes, see GH #6803 as read_from_file() does not understand slashes on Windows thus it assignes full path to names of loaded objects.
        path.make_preferred();
#else  // _WIN32
       // Don't make a copy on Posix. Slash is a path separator, back slashes are not accepted as a substitute.
        const auto &path = input_files[i];
#endif // _WIN32
        const auto filename         = path.filename();
        int  progress_percent = static_cast<int>(100.0f * static_cast<float>(i) / static_cast<float>(input_files.size()));
        const auto real_filename    = (strategy & LoadStrategy::Restore) ? input_files[++i].filename() : filename;
        const auto dlg_info         = _L("Loading file") + ": " + from_path(real_filename);
        BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << boost::format(": load file %1%") % filename;
        dlg_cont = dlg.Update(progress_percent, dlg_info);
        if (!dlg_cont) return empty_result;

        const bool type_3mf = std::regex_match(path.string(), pattern_3mf);
        // const bool type_zip_amf = !type_3mf && std::regex_match(path.string(), pattern_zip_amf);
        const bool type_any_amf = !type_3mf && std::regex_match(path.string(), pattern_any_amf);
        // const bool type_prusa   = std::regex_match(path.string(), pattern_prusa);

        Slic3r::Model model;
        // BBS: add auxiliary files related logic
        bool load_aux = strategy & LoadStrategy::LoadAuxiliary, load_old_project = false;
        if (load_model && load_config && type_3mf) {
            load_aux = true;
            strategy = strategy | LoadStrategy::LoadAuxiliary;
        }
        if (load_config) strategy = strategy | LoadStrategy::CheckVersion;
        bool is_project_file = false;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": is_project_file %1%, type_3mf %2%") % is_project_file % type_3mf;
        try {
            if (type_3mf) {
                DynamicPrintConfig config;
                Semver             file_version;
                {
                    DynamicPrintConfig config_loaded;

                    // BBS: add part plate related logic
                    PlateDataPtrs             plate_data;
                    En3mfType                 en_3mf_file_type = En3mfType::From_BBS;
                    ConfigSubstitutionContext config_substitutions{ForwardCompatibilitySubstitutionRule::Enable};
                    std::vector<Preset *>     project_presets;
                    // BBS: backup & restore
                    q->skip_thumbnail_invalid = true;
                    model = Slic3r::Model::read_from_archive(path.string(), &config_loaded, &config_substitutions, en_3mf_file_type, strategy, &plate_data, &project_presets,
                                                             &file_version,
                                                             [this, &dlg, real_filename, &progress_percent, &file_percent, stage_percent, INPUT_FILES_RATIO, total_files, i,
                                                              &is_user_cancel](int import_stage, int current, int total, bool &cancel) {
                                                                 bool     cont = true;
                                                                 float percent_float = (100.0f * (float)i / (float)total_files) + INPUT_FILES_RATIO * ((float)stage_percent[import_stage] + (float)current * (float)(stage_percent[import_stage + 1] - stage_percent[import_stage]) /(float) total) / (float)total_files;
                                                                 BOOST_LOG_TRIVIAL(trace) << "load_3mf_file: percent(float)=" << percent_float << ", stage = " << import_stage << ", curr = " << current << ", total = " << total;
                                                                 progress_percent = (int)percent_float;
                                                                 wxString msg  = wxString::Format(_L("Loading file: %s"), from_path(real_filename));
                                                                 cont          = dlg.Update(progress_percent, msg);
                                                                 cancel        = !cont;
                                                                 if (cancel)
                                                                     is_user_cancel = cancel;
                                                             });
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__
                                            << boost::format(", plate_data.size %1%, project_preset.size %2%, is_bbs_3mf %3%, file_version %4% \n") % plate_data.size() %
                                                   project_presets.size() % (en_3mf_file_type == En3mfType::From_BBS) % file_version.to_string();

                    // 1. add extruder for prusa model if the number of existing extruders is not enough
                    // 2. add extruder for BBS or Other model if only import geometry
                    if (en_3mf_file_type == En3mfType::From_Prusa || (load_model && !load_config)) {
                        std::set<int> extruderIds;
                        for (ModelObject *o : model.objects) {
                            if (o->config.option("extruder")) extruderIds.insert(o->config.extruder());
                            for (auto volume : o->volumes) {
                                if (volume->config.option("extruder")) extruderIds.insert(volume->config.extruder());
                                for (int extruder : volume->get_extruders()) { extruderIds.insert(extruder); }
                            }
                        }
                        int size = extruderIds.size() == 0 ? 0 : *(extruderIds.rbegin());

                        int filament_size = sidebar->combos_filament().size();
                        while (filament_size < MAXIMUM_EXTRUDER_NUMBER && filament_size < size) {
                            int         filament_count = filament_size + 1;
                            wxColour    new_col        = Plater::get_next_color_for_filament();
                            std::string new_color      = new_col.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
                            app_preset_bundle()->set_num_filaments(filament_count, new_color);
                            AppAdapter::plater()->on_filaments_change(filament_count);
                            ++filament_size;
                        }
                        AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update();
                    }

                    std::string import_project_action = AppAdapter::app_config()->get("import_project_action");
                    LoadType load_type;
                    if (import_project_action.empty())
                        load_type = LoadType::Unknown;
                    else
                        load_type  = static_cast<LoadType>(std::stoi(import_project_action));

                    // BBS: version check
                    Semver app_version = *(Semver::parse(LightMaker_VERSION));
                    if (en_3mf_file_type == En3mfType::From_Prusa) {
                        // do not reset the model config
                        load_config = false;
                        if(load_type != LoadType::LoadGeometry)
                            show_info(q, _L("The 3mf is not supported by OrcaSlicer, load geometry data only."), _L("Load 3mf"));
                    }
                    else if (load_config && (file_version > app_version)) {
                        if (config_substitutions.unrecogized_keys.size() > 0) {
                            wxString text  = wxString::Format(_L("The 3mf's version %s is newer than %s's version %s, Found following keys unrecognized:"),
                                                             file_version.to_string(), std::string(SLIC3R_APP_FULL_NAME), app_version.to_string());
                            text += "\n";
                            bool     first = true;
                            // std::string context = into_u8(text);
                            wxString context = text;
                            // if (AppAdapter::app_config()->get("user_mode") == "develop") {
                            //     for (auto &key : config_substitutions.unrecogized_keys) {
                            //         context += "  -";
                            //         context += key;
                            //         context += ";\n";
                            //         first = false;
                            //     }
                            // }
                            wxString append = _L("You'd better upgrade your software.\n");
                            context += "\n\n";
                            // context += into_u8(append);
                            context += append;
                            show_info(q, context, _L("Newer 3mf version"));
                        }
                        else {
                            //if the minor version is not matched
                            if (file_version.min() != app_version.min()) {
                                wxString text  = wxString::Format(_L("The 3mf's version %s is newer than %s's version %s, Suggest to upgrade your software."),
                                                 file_version.to_string(), std::string(SLIC3R_APP_FULL_NAME), app_version.to_string());
                                text += "\n";
                                show_info(q, text, _L("Newer 3mf version"));
                            }
                        }
                    } 
                    else if (!load_config) {
                        // reset config except color
                        for (ModelObject *model_object : model.objects) {
                            bool has_extruder = model_object->config.has("extruder");
                            int  extruder_id  = -1;
                            // save the extruder information before reset
                            if (has_extruder) { extruder_id = model_object->config.extruder(); }

                            model_object->config.reset();

                            // restore the extruder after reset
                            if (has_extruder) { model_object->config.set("extruder", extruder_id); }

                            // Is there any modifier or advanced config data?
                            for (ModelVolume *model_volume : model_object->volumes) {
                                has_extruder = model_volume->config.has("extruder");
                                if (has_extruder) { extruder_id = model_volume->config.extruder(); }

                                model_volume->config.reset();

                                if (has_extruder) { model_volume->config.set("extruder", extruder_id); }
                            }
                        }
                    }

                    // plate data
                    if (plate_data.size() > 0) {
                        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format(", import 3mf UPDATE_GCODE_RESULT \n");
                        wxString msg = wxString::Format(_L("Loading file: %s"), from_path(real_filename));
                        dlg_cont     = dlg.Update(progress_percent, msg);
                        if (!dlg_cont) {
                            q->skip_thumbnail_invalid = false;
                            return empty_result;
                        }

                        Semver old_version(1, 5, 9);
                        if ((en_3mf_file_type == En3mfType::From_BBS) && (file_version < old_version) && load_model && load_config && !config_loaded.empty()) {
                            translate_old = true;
                            partplate_list.get_plate_size(current_width, current_depth, current_height);
                        }

                        if (load_config) {
                            if (translate_old) {
                                //set the size back
                                partplate_list.reset_size(current_width + Bed3D::Axes::DefaultTipRadius, current_depth + Bed3D::Axes::DefaultTipRadius, current_height, false);
                            }
                            partplate_list.load_from_3mf_structure(plate_data);
                            partplate_list.update_slice_context_to_current_plate(background_process);
                            this->preview->update_gcode_result(partplate_list.get_current_slice_result_wrapper());
                            release_PlateData_list(plate_data);
                            sidebar->obj_list()->reload_all_plates();
                        } else {
                            partplate_list.reload_all_objects();
                        }
                    }

                    // BBS:: project embedded presets
                    if ((project_presets.size() > 0) && load_config) {
                        // load project embedded presets
                        PresetsConfigSubstitutions preset_substitutions;
                        PresetBundle &             preset_bundle = *app_preset_bundle();
                        preset_substitutions                     = preset_bundle.load_project_embedded_presets(project_presets, ForwardCompatibilitySubstitutionRule::Enable);
                        if (!preset_substitutions.empty()) show_substitutions_info(preset_substitutions);
                    }
                    if (project_presets.size() > 0) {
                        for (unsigned int i = 0; i < project_presets.size(); i++) { delete project_presets[i]; }
                        project_presets.clear();
                    }

                    if (load_config && !config_loaded.empty()) {
                        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format(", import 3mf IMPORT_LOAD_CONFIG \n");
                        wxString msg = wxString::Format(_L("Loading file: %s"), from_path(real_filename));
                        dlg_cont     = dlg.Update(progress_percent, msg);
                        if (!dlg_cont) {
                            q->skip_thumbnail_invalid = false;
                            return empty_result;
                        }

                        config.apply(static_cast<const ConfigBase &>(FullPrintConfig::defaults()));
                        // and place the loaded config over the base.
                        config += std::move(config_loaded);
                        std::map<std::string, std::string> validity = config.validate();
                        if (!validity.empty()) {
                            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format("Param values in 3mf error: ");
                            for (std::map<std::string, std::string>::iterator it=validity.begin(); it!=validity.end(); ++it)
                                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format("%1%: %2%")%it->first %it->second;
                            //
                            NotificationManager *notify_manager = q->get_notification_manager();
                            std::string error_message = L("Invalid values found in the 3mf:");
                            error_message += "\n";
                            for (std::map<std::string, std::string>::iterator it=validity.begin(); it!=validity.end(); ++it)
                                error_message += "-" + it->first + ": " + it->second + "\n";
                            error_message += "\n";
                            error_message += L("Please correct them in the param tabs");
                            notify_manager->bbl_show_3mf_warn_notification(error_message);
                        }
                    }
                    if (!config_substitutions.empty()) show_substitutions_info(config_substitutions.substitutions, filename.string());

                    // BBS
                    if (load_model && !load_config) {
                        ;
                    }
                    else {
                        this->model.plates_custom_gcodes = model.plates_custom_gcodes;
                        this->model.design_info = model.design_info;
                        this->model.model_info = model.model_info;
                    }
                }

                if (load_config) {
                    if (!config.empty()) {
                        Preset::normalize(config);
                        PresetBundle *preset_bundle = app_preset_bundle();

                        auto choise = AppAdapter::app_config()->get("no_warn_when_modified_gcodes");
                        if (choise.empty() || choise != "true") {
                            // BBS: first validate the printer
                            // validate the system profiles
                            std::set<std::string> modified_gcodes;
                            int validated = preset_bundle->validate_presets(filename.string(), config, modified_gcodes);
                            if (validated == VALIDATE_PRESETS_MODIFIED_GCODES) {
                                std::string warning_message;
                                warning_message += "\n";
                                for (std::set<std::string>::iterator it=modified_gcodes.begin(); it!=modified_gcodes.end(); ++it)
                                    warning_message += "-" + *it + "\n";
                                warning_message += "\n";
                                //show_info(q, _L("The 3mf has following modified G-codes in filament or printer presets:") + warning_message+ _L("Please confirm that these modified G-codes are safe to prevent any damage to the machine!"), _L("Modified G-codes"));

                                MessageDialog dlg(q, _L("The 3mf has following modified G-codes in filament or printer presets:") + warning_message+ _L("Please confirm that these modified G-codes are safe to prevent any damage to the machine!"), _L("Modified G-codes"));
                                dlg.show_dsa_button();
                                auto  res = dlg.ShowModal();
                                if (dlg.get_checkbox_state())
                                    AppAdapter::app_config()->set("no_warn_when_modified_gcodes", "true");
                            }
                            else if ((validated == VALIDATE_PRESETS_PRINTER_NOT_FOUND) || (validated == VALIDATE_PRESETS_FILAMENTS_NOT_FOUND)) {
                                std::string warning_message;
                                warning_message += "\n";
                                for (std::set<std::string>::iterator it=modified_gcodes.begin(); it!=modified_gcodes.end(); ++it)
                                    warning_message += "-" + *it + "\n";
                                warning_message += "\n";
                                //show_info(q, _L("The 3mf has following customized filament or printer presets:") + warning_message + _L("Please confirm that the G-codes within these presets are safe to prevent any damage to the machine!"), _L("Customized Preset"));
                                MessageDialog dlg(q, _L("The 3mf has following customized filament or printer presets:") + from_u8(warning_message)+ _L("Please confirm that the G-codes within these presets are safe to prevent any damage to the machine!"), _L("Customized Preset"));
                                dlg.show_dsa_button();
                                auto  res = dlg.ShowModal();
                                if (dlg.get_checkbox_state())
                                    AppAdapter::app_config()->set("no_warn_when_modified_gcodes", "true");
                            }
                        }

                        //always load config
                        {
                            // BBS: save the wipe tower pos in file here, will be used later
                            ConfigOptionFloats* wipe_tower_x_opt = config.opt<ConfigOptionFloats>("wipe_tower_x");
                            ConfigOptionFloats* wipe_tower_y_opt = config.opt<ConfigOptionFloats>("wipe_tower_y");
                            std::optional<ConfigOptionFloats>file_wipe_tower_x;
                            std::optional<ConfigOptionFloats>file_wipe_tower_y;
                            if (wipe_tower_x_opt)
                                file_wipe_tower_x = *wipe_tower_x_opt;
                            if (wipe_tower_y_opt)
                                file_wipe_tower_y = *wipe_tower_y_opt;

                            preset_bundle->load_config_model(filename.string(), std::move(config), file_version);

                            ConfigOption* bed_type_opt = preset_bundle->project_config.option("curr_bed_type");
                            if (bed_type_opt != nullptr) {
                                BedType bed_type = (BedType)bed_type_opt->getInt();
                                q->on_bed_type_change(bed_type);
                            }

                            // For exporting from the amf/3mf we shouldn't check printer_presets for the containing information about "Print Host upload"
                            // BBS: add preset combo box re-active logic
                            // currently found only needs re-active here
                            AppAdapter::gui_app()->load_current_presets(false, false);
                            // Update filament colors for the MM-printer profile in the full config
                            // to avoid black (default) colors for Extruders in the ObjectList,
                            // when for extruder colors are used filament colors
                            q->on_filaments_change(preset_bundle->filament_presets.size());
                            is_project_file = true;

                            //BBS: rewrite wipe tower pos stored in 3mf file , the code above should be seriously reconsidered
                            {
                                DynamicConfig& proj_cfg = app_preset_bundle()->project_config;
                                ConfigOptionFloats* wipe_tower_x = proj_cfg.opt<ConfigOptionFloats>("wipe_tower_x");
                                ConfigOptionFloats* wipe_tower_y = proj_cfg.opt<ConfigOptionFloats>("wipe_tower_y");
                                if (file_wipe_tower_x)
                                    *wipe_tower_x = *file_wipe_tower_x;
                                if (file_wipe_tower_y)
                                    *wipe_tower_y = *file_wipe_tower_y;
                            }
                        }
                    }
                    if (!silence) AppAdapter::app_config()->update_config_dir(path.parent_path().string());
                }
            } else {
                // BBS: add plate data related logic
                PlateDataPtrs plate_data;
                // BBS: project embedded settings
                std::vector<Preset *> project_presets;
                bool                  is_xxx;
                Semver                file_version;
                
                //ObjImportColorFn obj_color_fun=nullptr;
                auto obj_color_fun = [this, &path](std::vector<RGBA> &input_colors, bool is_single_color, std::vector<unsigned char> &filament_ids,
                                                   unsigned char &first_extruder_id) {
                    if (!boost::iends_with(path.string(), ".obj")) { return; }
                    const std::vector<std::string> extruder_colours = AppAdapter::plater()->get_extruder_colors_from_plater_config();
                    ObjColorDialog                 color_dlg(nullptr, input_colors, is_single_color, extruder_colours, filament_ids, first_extruder_id);
                    if (color_dlg.ShowModal() != wxID_OK) { 
                        filament_ids.clear();
                    }
                };
                model = Slic3r::Model::read_from_file(
                    path.string(), nullptr, nullptr, strategy, &plate_data, &project_presets, &is_xxx, &file_version, nullptr,
                    [this, &dlg, real_filename, &progress_percent, &file_percent, INPUT_FILES_RATIO, total_files, i, &designer_model_id, &designer_country_code](int current, int total, bool &cancel, std::string &mode_id, std::string &code)
                    {
                            designer_model_id = mode_id;
                            designer_country_code = code;

                            bool     cont = true;
                            float percent_float = (100.0f * (float)i / (float)total_files) + INPUT_FILES_RATIO * 100.0f * ((float)current / (float)total) / (float)total_files;
                            BOOST_LOG_TRIVIAL(trace) << "load_stl_file: percent(float)=" << percent_float << ", curr = " << current << ", total = " << total;
                            progress_percent = (int)percent_float;
                            wxString msg  = wxString::Format(_L("Loading file: %s"), from_path(real_filename));
                            cont          = dlg.Update(progress_percent, msg);
                            cancel        = !cont;
                     },
                    [this, &dlg, real_filename, &progress_percent, &file_percent, step_percent, INPUT_FILES_RATIO, total_files, i](int load_stage, int current, int total, bool &cancel)
                    {
                            bool     cont = true;
                            float percent_float = (100.0f * (float)i / (float)total_files) + INPUT_FILES_RATIO * ((float)step_percent[load_stage] + (float)current * (float)(step_percent[load_stage + 1] - step_percent[load_stage]) / (float)total) / (float)total_files;
                            BOOST_LOG_TRIVIAL(trace) << "load_step_file: percent(float)=" << percent_float << ", stage = " << load_stage << ", curr = " << current << ", total = " << total;
                            progress_percent = (int)percent_float;
                            wxString msg  = wxString::Format(_L("Loading file: %s"), from_path(real_filename));
                            cont          = dlg.Update(progress_percent, msg);
                            cancel        = !cont;
                    },
                    [](int isUtf8StepFile) {
                            if (!isUtf8StepFile) {
                                const auto no_warn = AppAdapter::app_config()->get_bool("step_not_utf8_no_warn");
                                if (!no_warn) {
                                    MessageDialog dlg(nullptr, _L("Name of components inside step file is not UTF8 format!") + "\n\n" + _L("The name may show garbage characters!"),
                                                      wxString(SLIC3R_APP_FULL_NAME " - ") + _L("Attention!"), wxOK | wxICON_INFORMATION);
                                    dlg.show_dsa_button(_L("Remember my choice."));
                                    dlg.ShowModal();
                                    if (dlg.get_checkbox_state()) {
                                        AppAdapter::app_config()->set_bool("step_not_utf8_no_warn", true);
                                    }
                                }
                            }
                        },
                    nullptr, 0, obj_color_fun);


                if (designer_model_id.empty() && boost::algorithm::iends_with(path.string(), ".stl")) {
                    read_binary_stl(path.string(), designer_model_id, designer_country_code);
                }

                if (type_any_amf && is_xxx) imperial_units = true;

                for (auto obj : model.objects) {
                    if (obj->name.empty()) {
                        obj->name = fs::path(obj->input_file).filename().string();
                    }
                    obj->rotate(Geometry::deg2rad(config->opt_float("preferred_orientation")), Axis::Z);
                }

                if (plate_data.size() > 0) {
                    partplate_list.load_from_3mf_structure(plate_data);
                    partplate_list.update_slice_context_to_current_plate(background_process);
                    this->preview->update_gcode_result(partplate_list.get_current_slice_result_wrapper());
                    release_PlateData_list(plate_data);
                    sidebar->obj_list()->reload_all_plates();
                }

                // BBS:: project embedded presets
                if (project_presets.size() > 0) {
                    // load project embedded presets
                    PresetsConfigSubstitutions preset_substitutions;
                    PresetBundle &             preset_bundle = *app_preset_bundle();
                    preset_substitutions                     = preset_bundle.load_project_embedded_presets(project_presets, ForwardCompatibilitySubstitutionRule::Enable);
                    if (!preset_substitutions.empty()) show_substitutions_info(preset_substitutions);

                    for (unsigned int i = 0; i < project_presets.size(); i++) { delete project_presets[i]; }
                    project_presets.clear();
                }
            }
        } catch (const ConfigurationError &e) {
            std::string message = GUI::format(_L("Failed loading file \"%1%\". An invalid configuration was found."), filename.string()) + "\n\n" + e.what();
            GUI::show_error(q, message);
            continue;
        } catch (const std::exception &e) {
            if (!is_user_cancel)
                GUI::show_error(q, e.what());
            continue;
        }

        progress_percent = 100.0f * (float)i / (float)total_files + INIT_MODEL_RATIO * 100.0f / (float)total_files;
        dlg_cont = dlg.Update(progress_percent);
        if (!dlg_cont) {
            q->skip_thumbnail_invalid = false;
            return empty_result;
        }

        if (load_model) {
            // The model should now be initialized
            auto convert_from_imperial_units = [](Model &model, bool only_small_volumes) { model.convert_from_imperial_units(only_small_volumes); };

            // BBS: add load_old_project logic
            if ((!is_project_file) && (!load_old_project)) {
                // if (!is_project_file) {
                if (int deleted_objects = model.removed_objects_with_zero_volume(); deleted_objects > 0) {
                    MessageDialog(q, _L("Objects with zero volume removed"), _L("The volume of the object is zero"), wxICON_INFORMATION | wxOK).ShowModal();
                }
                if (imperial_units)
                    // Convert even if the object is big.
                    convert_from_imperial_units(model, false);
                else if (model.looks_like_saved_in_meters()) {
                    // BBS do not handle look like in meters
                    MessageDialog dlg(q,
                                      format_wxstr(_L("The object from file %s is too small, and maybe in meters or inches.\n Do you want to scale to millimeters?"),
                                                   from_path(filename)),
                                      _L("Object too small"), wxICON_QUESTION | wxYES_NO);
                    int           answer = dlg.ShowModal();
                    if (answer == wxID_YES) model.convert_from_meters(true);
                } else if (model.looks_like_imperial_units()) {
                    // BBS do not handle look like in meters
                    MessageDialog dlg(q,
                                      format_wxstr(_L("The object from file %s is too small, and maybe in meters or inches.\n Do you want to scale to millimeters?"),
                                                   from_path(filename)),
                                      _L("Object too small"), wxICON_QUESTION | wxYES_NO);
                    int           answer = dlg.ShowModal();
                    if (answer == wxID_YES) convert_from_imperial_units(model, true);
                }
            }

             if (!is_project_file && model.looks_like_multipart_object()) {
               MessageDialog msg_dlg(q, _L(
                    "This file contains several objects positioned at multiple heights.\n"
                    "Instead of considering them as multiple objects, should \n"
                    "the file be loaded as a single object having multiple parts?") + "\n",
                    _L("Multi-part object detected"), wxICON_WARNING | wxYES | wxNO);
                if (msg_dlg.ShowModal() == wxID_YES) {
                    model.convert_multipart_object(filaments_cnt);
                }
            }
        }

        progress_percent = 100.0f * (float)i / (float)total_files + CENTER_AROUND_ORIGIN_RATIO * 100.0f / (float)total_files;
        dlg_cont = dlg.Update(progress_percent);
        if (!dlg_cont) {
            q->skip_thumbnail_invalid = false;
            return empty_result;
        }

        int model_idx = 0;
        for (ModelObject *model_object : model.objects) {
            if (!type_3mf && !type_any_amf) model_object->center_around_origin(false);

            // BBS
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format("import 3mf IMPORT_LOAD_MODEL_OBJECTS \n");
            wxString msg = wxString::Format("Loading file: %s", from_path(real_filename));
            model_idx++;
            dlg_cont = dlg.Update(progress_percent, msg);
            if (!dlg_cont) {
                q->skip_thumbnail_invalid = false;
                return empty_result;
            }

            if (!model_object->instances.empty())
                model_object->ensure_on_bed(is_project_file);
        }

        tolal_model_count += model_idx;

        progress_percent = 100.0f * (float)i / (float)total_files + LOAD_MODEL_RATIO * 100.0f / (float)total_files;
        dlg_cont = dlg.Update(progress_percent);
        if (!dlg_cont) {
            q->skip_thumbnail_invalid = false;
            return empty_result;
        }

        if (one_by_one) {
            // BBS: add load_old_project logic
            if (type_3mf && !is_project_file && !load_old_project)
                // if (type_3mf && !is_project_file)
                model.center_instances_around_point(PlateBed::build_volume().bed_center());
            // BBS: add auxiliary files logic
            // BBS: backup & restore
            if (load_aux) {
                q->model().load_from(model);
            }
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format(", before load_model_objects, count %1%")%model.objects.size();
            auto loaded_idxs = load_model_objects(model.objects, is_project_file);
            obj_idxs.insert(obj_idxs.end(), loaded_idxs.begin(), loaded_idxs.end());

            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format(", finished load_model_objects");
            wxString msg = wxString::Format(_L("Loading file: %s"), from_path(real_filename));
            dlg_cont     = dlg.Update(progress_percent, msg);
            if (!dlg_cont) {
                q->skip_thumbnail_invalid = false;
                return empty_result;
            }
        } else {
            // This must be an .stl or .obj file, which may contain a maximum of one volume.
            for (const ModelObject *model_object : model.objects) {
                new_model->add_object(*model_object);

                BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":" << __LINE__ << boost::format(", added object %1%")%model_object->name;
                wxString msg = wxString::Format(_L("Loading file: %s"), from_path(real_filename));
                dlg_cont     = dlg.Update(progress_percent, msg);
                if (!dlg_cont) {
                    q->skip_thumbnail_invalid = false;
                    return empty_result;
                }
            }
        }
    }

    if (new_model != nullptr && new_model->objects.size() > 1) {
        //BBS do not popup this dialog

        if (ask_multi) {
            MessageDialog msg_dlg(q, _L("Load these files as a single object with multiple parts?\n"), _L("Object with multiple parts was detected"),
                                  wxICON_WARNING | wxYES | wxNO);
            if (msg_dlg.ShowModal() == wxID_YES) { new_model->convert_multipart_object(filaments_cnt); }
        }

        auto loaded_idxs = load_model_objects(new_model->objects);
        obj_idxs.insert(obj_idxs.end(), loaded_idxs.begin(), loaded_idxs.end());
    }

    if (new_model) delete new_model;

    //BBS: translate old 3mf to correct positions
    if (translate_old) {
        //translate the objects
        int plate_count = partplate_list.get_plate_count();
        for (int index = 1; index < plate_count; index ++) {
            PartPlate* cur_plate = (PartPlate *)partplate_list.get_plate(index);

            Vec3d cur_origin = cur_plate->get_origin();
            Vec3d new_origin = partplate_list.compute_origin_using_new_size(index, current_width, current_depth);

            cur_plate->translate_all_instance(new_origin - cur_origin);
        }
        m_scene_raycaster->remove_raycasters(SceneRaycaster::EType::Bed);
        partplate_list.reset_size(current_width, current_depth, current_height, true, true);
        partplate_list.register_raycasters_for_picking(m_scene_raycaster);
    }

    //BBS: add gcode loading logic in the end
    q->m_exported_file = false;
    q->skip_thumbnail_invalid = false;
    if (load_model && load_config) {
        if (model.objects.empty()) {
            partplate_list.load_gcode_files();
            PartPlate * first_plate = nullptr, *cur_plate = nullptr;
            int plate_cnt = partplate_list.get_plate_count();
            int index = 0, first_plate_index = 0;
            q->m_valid_plates_count = 0;
            for (index = 0; index < plate_cnt; index ++)
            {
                cur_plate = partplate_list.get_plate(index);
                if (!first_plate && cur_plate->is_slice_result_valid()) {
                    first_plate = cur_plate;
                    first_plate_index = index;
                }
                if (cur_plate->is_slice_result_valid())
                    q->m_valid_plates_count ++;
            }
            if (first_plate&&first_plate->is_slice_result_valid()) {
                q->m_exported_file = true;
                //select plate 0 as default
                q->select_plate(first_plate_index);
                //set to 3d tab
                q->select_preview();
                AppAdapter::main_panel()->select_tab(MainPanel::tpPreview);
            }
            else {
                //set to 3d tab
                q->select_view3d();
                //select plate 0 as default
                q->select_plate(0);
            }
        }
        else {
            //set to 3d tab
            q->select_view3d();
            //select plate 0 as default
            q->select_plate(0);
        }
    }
    else {
        //always set to 3D after loading files
        q->select_view3d();
        AppAdapter::main_panel()->select_tab(MainPanel::tp3DEditor);
    }

    if (load_model) {
        if (!silence) AppAdapter::app_config()->update_skein_dir(input_files[input_files.size() - 1].parent_path().make_preferred().string());
        // XXX: Plater.pm had @loaded_files, but didn't seem to fill them with the filenames...
    }

    // automatic selection of added objects
    if (!obj_idxs.empty() && view3D != nullptr) {
        // update printable state for new volumes on canvas3D
        AppAdapter::plater()->canvas3D()->update_instance_printable_state_for_objects(obj_idxs);

        if (!load_config) {
            m_selection.clear();
            for (size_t idx : obj_idxs) {
                m_selection.add_object((unsigned int)idx, false);
            }
        }
        // BBS: update object list selection
        this->sidebar->obj_list()->update_selections();

        if (m_gizmos->is_enabled())
            update_gizmos_on_off_state();
    }

    GLGizmoSimplify::add_simplify_suggestion_notification(
        obj_idxs, model.objects, *notification_manager);

    //set designer_model_id
    q->model().stl_design_id = designer_model_id;
    q->model().stl_design_country = designer_country_code;
    //if (!designer_model_id.empty() && q->model().stl_design_id.empty() && !designer_country_code.empty()) {
    //    q->model().stl_design_id = designer_model_id;
    //    q->model().stl_design_country = designer_country_code;
    //}
    //else {
    //    q->model().stl_design_id = "";
    //    q->model().stl_design_country = "";
    //}

    if (tolal_model_count <= 0 && !q->m_exported_file) {
        dlg.Hide();
        if (!is_user_cancel) {
            MessageDialog msg(AppAdapter::main_panel(), _L("The file does not contain any geometry data."), _L("Warning"), wxYES | wxICON_WARNING);
            if (msg.ShowModal() == wxID_YES) {}
        }
    }
    return obj_idxs;
}

std::vector<size_t> Plater::priv::load_model_objects(const ModelObjectPtrs& model_objects, bool allow_negative_z, bool split_object)
{
    const Vec3d bed_size = Slic3r::to_3d(PlateBed::build_volume().bounding_volume2d().size(), 1.0) - 2.0 * Vec3d::Ones();

    bool scaled_down = false;
    std::vector<size_t> obj_idxs;
    unsigned int obj_count = model.objects.size();

    ModelInstancePtrs new_instances;
    for (ModelObject *model_object : model_objects) {
        auto *object = model.add_object(*model_object);
        object->sort_volumes(true);
        std::string object_name = object->name.empty() ? fs::path(object->input_file).filename().string() : object->name;
        obj_idxs.push_back(obj_count++);

        if (model_object->instances.empty()) {
            object->center_around_origin();
            new_instances.emplace_back(object->add_instance());
        }

        //BBS: when the object is too large, let the user choose whether to scale it down
        for (size_t i = 0; i < object->instances.size(); ++i) {
            ModelInstance* instance = object->instances[i];
            const Vec3d size = object->instance_bounding_box(i).size();
            const Vec3d ratio = size.cwiseQuotient(bed_size);
            const double max_ratio = std::max(ratio(0), ratio(1));
            if (max_ratio > 10000) {
                MessageDialog dlg(q, _L("Your object appears to be too large, Do you want to scale it down to fit the heat bed automatically?"), _L("Object too large"),
                                  wxICON_QUESTION | wxYES);
                int           answer = dlg.ShowModal();
                // the size of the object is too big -> this could lead to overflow when moving to clipper coordinates,
                // so scale down the mesh
                object->scale_mesh_after_creation(1. / max_ratio);
                object->origin_translation = Vec3d::Zero();
                object->center_around_origin();
                scaled_down = true;
                break;
            }
            else if (max_ratio > 10) {
                MessageDialog dlg(q, _L("Your object appears to be too large, Do you want to scale it down to fit the heat bed automatically?"), _L("Object too large"),
                                  wxICON_QUESTION | wxYES_NO);
                int           answer = dlg.ShowModal();
                if (answer == wxID_YES) {
                    instance->set_scaling_factor(instance->get_scaling_factor() / max_ratio);
                    scaled_down = true;
                }
            }
        }

        object->ensure_on_bed(allow_negative_z);
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format(", loaded objects, begin to auto placement");
    // BBS: find an empty cell to put the copied object
    for (auto& instance : new_instances) {
        auto offset = instance->get_offset();
        auto start_point = PlateBed::build_volume().bounding_volume2d().center();
        const std::vector<Pointfs>& plate_shapes = q->get_partplate_list().get_curr_plate()->get_shape();
        if (!plate_shapes.empty())
            start_point = Slic3r::center_point(plate_shapes.back());

        bool plate_empty = partplate_list.get_curr_plate()->empty();
        Vec3d displacement;
        if (plate_empty)
            displacement = {start_point(0), start_point(1), offset(2)};
        else {
            auto empty_cell = AppAdapter::plater()->canvas3D()->get_nearest_empty_cell({start_point(0), start_point(1)});
            displacement    = {empty_cell.x(), empty_cell.y(), offset(2)};
        }
        instance->set_offset(displacement);
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format(", finished auto placement, before add_objects_to_list");
    notification_manager->close_notification_of_type(NotificationType::UpdatedItemsInfo);

    if (obj_idxs.size() > 1) {
        std::vector<size_t> obj_idxs_1 (obj_idxs.begin(), obj_idxs.end() - 1);

        AppAdapter::obj_list()->add_objects_to_list(obj_idxs_1, false);
        AppAdapter::obj_list()->add_object_to_list(obj_idxs[obj_idxs.size() - 1]);
    }
    else
        AppAdapter::obj_list()->add_objects_to_list(obj_idxs);

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ":" << __LINE__ << boost::format(", after add_objects_to_list");
    update();
    // Update InfoItems in ObjectList after update() to use of a correct value of the GLCanvas3D::is_sinking(),
    // which is updated after a view3D->reload_scene(false, flags & (unsigned int)UpdateParams::FORCE_FULL_SCREEN_REFRESH) call
    for (const size_t idx : obj_idxs)
        AppAdapter::obj_list()->update_info_items(idx);

    object_list_changed();

    this->schedule_background_process();

    return obj_idxs;
}

fs::path Plater::priv::get_export_file_path(Slic3r::FileType file_type)
{
    int obj_idx = m_selection.get_object_idx();

    fs::path output_file;
    if (file_type == FT_3MF)
        // for 3mf take the path from the project filename, if any
        output_file = into_path(get_project_filename(".3mf"));
    else if (file_type == FT_STL) {
        if (obj_idx > 0 && obj_idx < this->model.objects.size() && m_selection.is_single_full_object()) {
            output_file = this->model.objects[obj_idx]->get_export_filename();
        }
        else {
            output_file = into_path(get_project_name());
        }
    }
    //bbs  name the project using the part name
    if (output_file.empty()) {
        if (get_project_name() != _L("Untitled")) {
            output_file = into_path(get_project_name() + ".3mf");
        }
    }

    if (output_file.empty())
    {
        // first try to get the file name from the current selection
        if ((0 <= obj_idx) && (obj_idx < (int)this->model.objects.size()))
            output_file = this->model.objects[obj_idx]->get_export_filename();

        if (output_file.empty())
            // Find the file name of the first printable object.
            output_file = this->model.propose_export_file_name_and_path();

        if (output_file.empty() && !model.objects.empty())
            // Find the file name of the first object.
            output_file = this->model.objects[0]->get_export_filename();

        if (output_file.empty())
            // Use _L("Untitled") name
            output_file = into_path(_L("Untitled"));
    }
    return output_file;
}

wxString Plater::priv::get_export_file(Slic3r::FileType file_type)
{
    wxString wildcard;
    switch (file_type) {
        case FT_STL:
        case FT_AMF:
        case FT_3MF:
        case FT_GCODE:
        case FT_OBJ:
            wildcard = file_wildcards(file_type);
        break;
        default:
            wildcard = file_wildcards(FT_MODEL);
        break;
    }

    fs::path output_file = get_export_file_path(file_type);

    wxString dlg_title;
    switch (file_type) {
        case FT_STL:
        {
            output_file.replace_extension("stl");
            dlg_title = _L("Export STL file:");
            break;
        }
        case FT_AMF:
        {
            // XXX: Problem on OS X with double extension?
            output_file.replace_extension("zip.amf");
            dlg_title = _L("Export AMF file:");
            break;
        }
        case FT_3MF:
        {
            output_file.replace_extension("3mf");
            dlg_title = _L("Save file as:");
            break;
        }
        case FT_OBJ:
        {
            output_file.replace_extension("obj");
            dlg_title = _L("Export OBJ file:");
            break;
        }
        default: break;
    }

    std::string out_dir = (boost::filesystem::path(output_file).parent_path()).string();

    wxFileDialog dlg(q, dlg_title,
        is_shapes_dir(out_dir) ? from_u8(AppAdapter::app_config()->get_last_dir()) : from_path(output_file.parent_path()), from_path(output_file.filename()),
        wildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT | wxPD_APP_MODAL);

    int result = dlg.ShowModal();
    if (result == wxID_CANCEL)
        return "<cancel>";
    if (result != wxID_OK)
        return wxEmptyString;

    wxString out_path = dlg.GetPath();
    fs::path path(into_path(out_path));
#ifdef __WXMSW__
    if (boost::iequals(path.extension().string(), output_file.extension().string()) == false) {
        out_path += output_file.extension().string();
        boost::system::error_code ec;
        if (boost::filesystem::exists(into_u8(out_path), ec)) {
            auto result = MessageBox(q->GetHandle(),
                wxString::Format(_L("The file %s already exists\nDo you want to replace it?"), out_path),
                _L("Confirm Save As"),
                MB_YESNO | MB_ICONWARNING);
            if (result != IDYES)
                return wxEmptyString;
        }
    }
#endif
    AppAdapter::app_config()->update_last_output_dir(path.parent_path().string());

    return out_path;
}

const Selection& Plater::priv::get_selection() const
{
    return m_selection;
}

Selection& Plater::priv::get_selection()
{
    return m_selection;
}

Selection* Plater::priv::get_selection_ptr()
{
    return &m_selection;
}

int Plater::priv::get_selected_object_idx() const
{
    int idx = m_selection.get_object_idx();
    return ((0 <= idx) && (idx < 1000)) ? idx : -1;
}

int Plater::priv::get_selected_volume_idx() const
{
    int idx = m_selection.get_object_idx();
    if ((0 > idx) || (idx > 1000))
        return-1;
    const GLVolume* v = m_selection.get_first_volume();
    if (model.objects[idx]->volumes.size() > 1)
        return v->volume_idx();
    return -1;
}

void Plater::priv::selection_changed()
{
    // if the selection is not valid to allow for layer editing, we need to turn off the tool if it is running
    if (!layers_height_allowed() && view3D->is_layers_editing_enabled()) {
        SimpleEvent evt(EVT_GLTOOLBAR_LAYERSEDITING);
        on_action_layersediting(evt);
    }

    // forces a frame render to update the view (to avoid a missed update if, for example, the context menu appears)
    view3D->render();
}

void Plater::priv::object_list_changed()
{
    const bool model_fits = view3D->get_canvas3d()->check_volumes_outside_state() != ModelInstancePVS_Partly_Outside;

    PartPlate* part_plate = partplate_list.get_curr_plate();

    bool can_slice = !model.objects.empty() && model_fits && part_plate->has_printable_instances();
    main_panel->update_slice_print_status(MainPanel::eEventObjectUpdate, can_slice);

    AppAdapter::gui_app()->params_panel()->notify_object_config_changed();
}

void Plater::priv::select_curr_plate_all()
{
    view3D->select_curr_plate_all();
    this->sidebar->obj_list()->update_selections();
}

void Plater::priv::remove_curr_plate_all()
{
    SingleSnapshot ss(q);
    view3D->remove_curr_plate_all();
    this->sidebar->obj_list()->update_selections();
}

void Plater::priv::select_all()
{
    view3D->select_all();
    this->sidebar->obj_list()->update_selections();
}

void Plater::priv::deselect_all()
{
    view3D->deselect_all();
}

void Plater::priv::exit_gizmo()
{
    view3D->exit_gizmo();
}

void Plater::priv::remove(size_t obj_idx)
{
    if (view3D->is_layers_editing_enabled())
        view3D->enable_layers_editing(false);

    m_worker.cancel_all();
    model.delete_object(obj_idx);
    //BBS: notify partplate the instance removed
    partplate_list.notify_instance_removed(obj_idx, -1);
    update();
    // Delete object from Sidebar list. Do it after update, so that the GLScene selection is updated with the modified model.
    sidebar->obj_list()->delete_object_from_list(obj_idx);
    object_list_changed();
}


bool Plater::priv::delete_object_from_model(size_t obj_idx, bool refresh_immediately)
{
    // check if object isn't cut
    // show warning message that "cut consistancy" will not be supported any more
    ModelObject *obj = model.objects[obj_idx];
    if (obj->is_cut()) {
        InfoDialog dialog(q, _L("Delete object which is a part of cut object"),
                          _L("You try to delete an object which is a part of a cut object.\n"
                             "This action will break a cut correspondence.\n"
                             "After that model consistency can't be guaranteed."),
                          false, wxYES | wxCANCEL | wxCANCEL_DEFAULT | wxICON_WARNING);
        dialog.SetButtonLabel(wxID_YES, _L("Delete"));
        if (dialog.ShowModal() == wxID_CANCEL)
            return false;
    }

    std::string snapshot_label = "Delete Object";
    if (!obj->name.empty())
        snapshot_label += ": " + obj->name;
    Plater::TakeSnapshot snapshot(q, snapshot_label);
    m_worker.cancel_all();

    if (obj->is_cut())
        sidebar->obj_list()->invalidate_cut_info_for_object(obj_idx);

    model.delete_object(obj_idx);
    //BBS: notify partplate the instance removed
    partplate_list.notify_instance_removed(obj_idx, -1);

    //BBS
    if (refresh_immediately) {
        update();
        object_list_changed();
    }

    return true;
}

void Plater::priv::delete_all_objects_from_model()
{
    Plater::TakeSnapshot snapshot(q, "Delete All Objects");

    if (view3D->is_layers_editing_enabled())
        view3D->enable_layers_editing(false);

    reset_gcode_toolpaths();

    view3D->get_canvas3d()->reset_sequential_print_clearance();

    m_worker.cancel_all();

    // Stop and reset the Print content.
    background_process.reset();

    //BBS: update partplate
    partplate_list.clear();

    model.clear_objects();
    update();
    // Delete object from Sidebar list. Do it after update, so that the GLScene selection is updated with the modified model.
    sidebar->obj_list()->delete_all_objects_from_list();
    object_list_changed();

    //BBS
    model.calib_pa_pattern.reset();
    model.plates_custom_gcodes.clear();
}

void Plater::priv::reset(bool apply_presets_change)
{
    Plater::TakeSnapshot snapshot(q, "Reset Project", UndoRedo::SnapshotType::ProjectSeparator);

    clear_warnings();

    set_project_filename("");
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << __LINE__ << " call set_project_filename: empty";

    if (view3D->is_layers_editing_enabled())
        view3D->get_canvas3d()->force_main_toolbar_left_action(view3D->get_canvas3d()->get_main_toolbar_item_id("layersediting"));
    reset_all_gizmos();

    reset_gcode_toolpaths();

    view3D->get_canvas3d()->reset_sequential_print_clearance();

    m_worker.cancel_all();

    //BBS: clear the partplate list's object before object cleared
    partplate_list.reinit();
    partplate_list.update_slice_context_to_current_plate(background_process);
    preview->update_gcode_result(partplate_list.get_current_slice_result_wrapper());

    // Stop and reset the Print content.
    this->background_process.reset();
    model.clear_objects();
    update();

    //BBS
    if (true) {
        // Delete object from Sidebar list. Do it after update, so that the GLScene selection is updated with the modified model.
        sidebar->obj_list()->delete_all_objects_from_list();
        object_list_changed();
    }

    project.reset();

    //BBS: reset all project embedded presets
    app_preset_bundle()->reset_project_embedded_presets();
    if (apply_presets_change)
        AppAdapter::gui_app()->apply_keeped_preset_modifications();
    else
        AppAdapter::gui_app()->load_current_presets(false, false);

    //BBS
    model.calib_pa_pattern.reset();
    model.plates_custom_gcodes.clear();

    // BBS
    m_saved_timestamp = m_backup_timestamp = size_t(-1);

    // Save window layout
    if (sidebar_layout.is_enabled) {
        // Reset show state
        auto& sidebar = m_aui_mgr.GetPane(this->sidebar);
        if (!sidebar_layout.is_collapsed && !sidebar.IsShown()) {
            sidebar.Show();
        }
        auto layout = m_aui_mgr.SavePerspective();
        AppAdapter::app_config()->set("window_layout", layout.utf8_string());
    }
}

void Plater::priv::center_selection()
{
    view3D->center_selected();
}

void Plater::priv::drop_selection()
{
    view3D->drop_selected();
}

void Plater::priv::mirror(Axis axis)
{
    view3D->mirror_selection(axis);
}

void Plater::priv::split_object()
{
    int obj_idx = get_selected_object_idx();
    if (obj_idx == -1)
        return;

    // we clone model object because split_object() adds the split volumes
    // into the same model object, thus causing duplicates when we call load_model_objects()
    Model new_model = model;
    ModelObject* current_model_object = new_model.objects[obj_idx];

    wxBusyCursor wait;
    ModelObjectPtrs new_objects;
    current_model_object->split(&new_objects);
    if (new_objects.size() == 1)
        // #ysFIXME use notification
        Slic3r::GUI::warning_catcher(q, _L("The selected object couldn't be split."));
    else
    {
        // BBS no solid parts removed
        // If we splited object which is contain some parts/modifiers then all non-solid parts (modifiers) were deleted
        //if (current_model_object->volumes.size() > 1 && current_model_object->volumes.size() != new_objects.size())
        //    notification_manager->push_notification(NotificationType::CustomNotification,
        //        NotificationManager::NotificationLevel::PrintInfoNotificationLevel,
        //        _u8L("All non-solid parts (modifiers) were deleted"));

        Plater::TakeSnapshot snapshot(q, "Split to Objects");

        remove(obj_idx);

        // load all model objects at once, otherwise the plate would be rearranged after each one
        // causing original positions not to be kept
        std::vector<size_t> idxs = load_model_objects(new_objects, false, true);

        // select newly added objects
        for (size_t idx : idxs)
        {
            m_selection.add_object((unsigned int)idx, false);
        }
    }
}

void Plater::priv::split_volume()
{
    AppAdapter::obj_list()->split();
}

void Plater::priv::scale_selection_to_fit_print_volume()
{
    m_selection.scale_to_fit_print_volume(PlateBed::build_volume());
}

void Plater::priv::schedule_background_process()
{
    // Trigger the timer event after 0.5s
    this->background_process_timer.Start(500, wxTIMER_ONE_SHOT);
    // Notify the Canvas3D that something has changed, so it may invalidate some of the layer editing stuff.
    this->view3D->get_canvas3d()->set_config(this->config);
}

void Plater::priv::process_validation_warning(StringObjectException const &warning) const
{
    if (warning.string.empty())
        notification_manager->close_notification_of_type(NotificationType::ValidateWarning);
    else {
        std::string text = warning.string;
        auto po = dynamic_cast<PrintObjectBase const *>(warning.object);
        auto mo = po ? po->model_object() : dynamic_cast<ModelObject const *>(warning.object);
        auto action_fn = (mo || !warning.opt_key.empty()) ? [id = mo ? mo->id() : 0, opt = warning.opt_key](wxEvtHandler *) {
		    auto & objects = AppAdapter::gui_app()->model().objects;
		    auto iter = id.id ? std::find_if(objects.begin(), objects.end(), [id](auto o) { return o->id() == id; }) : objects.end();
            if (iter != objects.end()) {
                AppAdapter::main_panel()->select_tab(MainPanel::tp3DEditor);
			    AppAdapter::obj_list()->select_items({{*iter, nullptr}});
            }
            if (!opt.empty()) {
                if (iter != objects.end())
				    AppAdapter::gui_app()->params_panel()->switch_to_object();
                AppAdapter::gui_app()->sidebar().jump_to_option(opt, Preset::TYPE_PRINT, L"");
		    }
		    return false;
	    } : std::function<bool(wxEvtHandler *)>();
        auto hypertext = (mo || !warning.opt_key.empty()) ? _u8L("Jump to") : "";
        if (mo) hypertext += std::string(" [") + mo->name + "]";
        if (!warning.opt_key.empty()) hypertext += std::string(" (") + warning.opt_key + ")";

        notification_manager->push_notification(
            NotificationType::ValidateWarning,
            NotificationManager::NotificationLevel::WarningNotificationLevel,
            _u8L("WARNING:") + "\n" + text, hypertext, action_fn
        );
    }
}


// Update background processing thread from the current config and Model.
// Returns a bitmask of UpdateBackgroundProcessReturnState.
unsigned int Plater::priv::update_background_process(bool force_validation)
{
    // bitmap of enum UpdateBackgroundProcessReturnState
    unsigned int return_state = 0;

    background_process_timer.Stop();
    // Apply new config to the possibly running background task.
    bool               was_running = background_process.running();

    //BBS: update the current print to the current plate
    this->partplate_list.update_slice_context_to_current_plate(background_process);
    this->preview->update_gcode_result(partplate_list.get_current_slice_result_wrapper());

    Print::ApplyStatus invalidated = background_process.apply();

    if (invalidated == Print::APPLY_STATUS_INVALIDATED) {
        //BBS: update current plater's slicer result to invalid
        this->background_process.get_current_plate()->update_slice_result_valid_state(false);

        // Reset preview canvases. If the print has been invalidated, the preview canvases will be cleared.
        // Otherwise they will be just refreshed.
        if (preview != nullptr) {
            // If the preview is not visible, the following line just invalidates the preview,
            // but the G-code paths or SLA preview are calculated first once the preview is made visible.
            
            // Layer 2 protection: use Plate-level external gcode flag
            PartPlate* current_plate = background_process.get_current_plate();
            bool has_external_gcode = current_plate && current_plate->has_external_gcode();
            
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ 
                << " - invalidated"
                << ", plate=" << (current_plate ? current_plate->get_index() : -1)
                << ", has_external_gcode=" << has_external_gcode
                << ", is_slice_result_valid=" << (current_plate ? current_plate->is_slice_result_valid() : false)
                << ", will_reset_toolpaths=" << (!has_external_gcode);
            
            if (!has_external_gcode) {
                reset_gcode_toolpaths();
            }
            preview->reload_print();
        }
        // In FDM mode, we need to reload the 3D scene because of the wipe tower preview box.
        if (config->opt_bool("enable_prime_tower"))
            return_state |= UPDATE_BACKGROUND_PROCESS_REFRESH_SCENE;

        notification_manager->set_slicing_progress_hidden();
    }
    else {
        preview->reload_print();
    }

    if ((invalidated != Print::APPLY_STATUS_UNCHANGED || force_validation) && ! background_process.empty()) {
        //BBS: add is_warning logic
        StringObjectException warning;
        //BBS: refine seq-print logic
        Polygons polygons;
        std::vector<std::pair<Polygon, float>> height_polygons;
        StringObjectException err = background_process.validate(&warning, &polygons, &height_polygons);
        // update string by type
        q->post_process_string_object_exception(err);
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": validate err=%1%, warning=%2%")%err.string%warning.string;

        if (err.string.empty()) {
            this->partplate_list.get_curr_plate()->update_apply_result_invalid(false);
            notification_manager->set_all_slicing_errors_gray(true);
            notification_manager->close_notification_of_type(NotificationType::ValidateError);
            if (invalidated != Print::APPLY_STATUS_UNCHANGED && background_processing_enabled())
                return_state |= UPDATE_BACKGROUND_PROCESS_RESTART;

            // Pass a warning from validation and either show a notification,
            // or hide the old one.
            process_validation_warning(warning);
            view3D->get_canvas3d()->reset_sequential_print_clearance();
            view3D->get_canvas3d()->set_as_dirty();
            view3D->get_canvas3d()->request_extra_frame();
        }
        else {
            this->partplate_list.get_curr_plate()->update_apply_result_invalid(true);
            // The print is not valid.
            // Show error as notification.
            notification_manager->push_validate_error_notification(err);
            //also update the warnings
            process_validation_warning(warning);
            return_state |= UPDATE_BACKGROUND_PROCESS_INVALID;
            if (true) {
                const Print* print = background_process.fff_print();
                //Polygons polygons;
                //if (print->config().print_sequence == PrintSequence::ByObject)
                //    Print::sequential_print_clearance_valid(*print, &polygons);
                view3D->get_canvas3d()->set_sequential_print_clearance_visible(true);
                view3D->get_canvas3d()->set_sequential_print_clearance_render_fill(true);
                view3D->get_canvas3d()->set_sequential_print_clearance_polygons(polygons, height_polygons);
            }
        }
    }

    //actualizate warnings
    if (invalidated != Print::APPLY_STATUS_UNCHANGED || background_process.empty()) {
        if (background_process.empty())
            process_validation_warning({});
        actualize_slicing_warnings(*this->background_process.fff_print());
        actualize_object_warnings(*this->background_process.fff_print());
        show_warning_dialog = false;
        process_completed_with_error = -1;
    }

    if (was_running && ! this->background_process.running() && (return_state & UPDATE_BACKGROUND_PROCESS_RESTART) == 0) {
        if (invalidated != Print::APPLY_STATUS_UNCHANGED || this->background_process.is_internal_cancelled())
        {
            // The background processing was killed and it will not be restarted.
            // Post the "canceled" callback message, so that it will be processed after any possible pending status bar update messages.
            SlicingProcessCompletedEvent evt(EVT_PROCESS_COMPLETED, 0,
                SlicingProcessCompletedEvent::Cancelled, nullptr);
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%, post an EVT_PROCESS_COMPLETED to main, status %2%")%__LINE__ %evt.status();
            wxQueueEvent(q, evt.Clone());
        }
    }

    if ((return_state & UPDATE_BACKGROUND_PROCESS_INVALID) != 0)
    {
        // Validation of the background data failed.
        //BBS: add slice&&print status update logic
        this->main_panel->update_slice_print_status(MainPanel::eEventSliceUpdate, false);

        process_completed_with_error = partplate_list.get_curr_plate_index();
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: set to process_completed_with_error, return_state=%2%")%__LINE__%return_state;
    }
    else
    {
        // Background data is valid.
        if ((return_state & UPDATE_BACKGROUND_PROCESS_RESTART) != 0 ||
            (return_state & UPDATE_BACKGROUND_PROCESS_REFRESH_SCENE) != 0 )
            notification_manager->set_slicing_progress_hidden();

        //BBS: add slice&&print status update logic
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: background data valid, return_state=%2%")%__LINE__%return_state;
        PartPlate* cur_plate = background_process.get_current_plate();
        if (background_process.finished() && cur_plate && cur_plate->is_slice_result_valid())
        {
            //ready_to_slice = false;
            this->main_panel->update_slice_print_status(MainPanel::eEventSliceUpdate, false);
        }
        else if (!background_process.empty() &&
                 !background_process.running()) /* Do not update buttons if background process is running
                                                 * This condition is important for SLA mode especially,
                                                 * when this function is called several times during calculations
                                                 * */
        {
            if (cur_plate->can_slice()) {
                //ready_to_slice = true;
                this->main_panel->update_slice_print_status(MainPanel::eEventSliceUpdate, true);
                process_completed_with_error = -1;
            }
            else {
                //ready_to_slice = false;
                this->main_panel->update_slice_print_status(MainPanel::eEventSliceUpdate, false);
                process_completed_with_error = partplate_list.get_curr_plate_index();
            }
        }
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: exit, return_state=%2%")%__LINE__%return_state;
    return return_state;
}

// Restart background processing thread based on a bitmask of UpdateBackgroundProcessReturnState.
bool Plater::priv::restart_background_process(unsigned int state)
{
    if (!m_worker.is_idle()) {
        // Avoid a race condition
        BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(", Line %1%: ui jobs running, return false")%__LINE__;
        return false;
    }

    if ( ! this->background_process.empty() &&
         (state & priv::UPDATE_BACKGROUND_PROCESS_INVALID) == 0 &&
         ( ((state & UPDATE_BACKGROUND_PROCESS_FORCE_RESTART) != 0 && ! this->background_process.finished()) ||
           (state & UPDATE_BACKGROUND_PROCESS_FORCE_EXPORT) != 0 ||
           (state & UPDATE_BACKGROUND_PROCESS_RESTART) != 0 ) ) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: print is valid, try to start it now")%__LINE__;
        // The print is valid and it can be started.
        if (this->background_process.start()) {
            if (!show_warning_dialog)
                on_slicing_began();
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: start successfully")%__LINE__;
            return true;
        }
    }
    else if (this->background_process.empty()) {
        PartPlate* cur_plate = background_process.get_current_plate();
        if (cur_plate->is_slice_result_valid() && ((state & UPDATE_BACKGROUND_PROCESS_FORCE_RESTART) != 0)) {
            if (this->background_process.start(cur_plate && cur_plate->is_slice_result_valid())) {
                if (!show_warning_dialog)
                    on_slicing_began();
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: start successfully")%__LINE__;
                return true;
            }
        }
    }
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: not started")%__LINE__;
    return false;
}

unsigned int Plater::priv::update_restart_background_process(bool force_update_scene, bool force_update_preview)
{
    // bitmask of UpdateBackgroundProcessReturnState
    unsigned int state = this->update_background_process(false);
    if (force_update_scene || (state & UPDATE_BACKGROUND_PROCESS_REFRESH_SCENE) != 0)
        view3D->reload_scene(false);

    if (force_update_preview)
        this->preview->reload_print();
    this->restart_background_process(state);
    return state;
}

void Plater::priv::update_fff_scene()
{
    if (this->preview != nullptr)
        this->preview->reload_print();
    // In case this was MM print, wipe tower bounding box on 3D tab might need redrawing with exact depth:
    view3D->reload_scene(true);
}

//BBS: add print project related logic
void Plater::priv::update_fff_scene_only_shells(bool only_shells)
{
    if (this->preview != nullptr)
    {
        const Print* current_print = this->background_process.fff_print();
        if (current_print)
        {
            this->preview->load_shells(*current_print);
        }
    }

    if (!only_shells) {
        view3D->reload_scene(true);
    }
}

void Plater::priv::update_sla_scene()
{
    // Update the SLAPrint from the current Model, so that the reload_scene()
    // pulls the correct data.
    delayed_scene_refresh = false;
    this->update_restart_background_process(true, true);
}

bool Plater::priv::replace_volume_with_stl(int object_idx, int volume_idx, const fs::path& new_path, const std::string& snapshot)
{
    const std::string path = new_path.string();
    wxBusyCursor wait;

    Model new_model;
    try {
        new_model = Model::read_from_file(path, nullptr, nullptr, LoadStrategy::AddDefaultInstances | LoadStrategy::LoadModel);
        for (ModelObject* model_object : new_model.objects) {
            model_object->center_around_origin();
            model_object->ensure_on_bed();
        }
    }
    catch (std::exception&) {
        // error while loading
        return false;
    }

    if (new_model.objects.size() > 1 || new_model.objects.front()->volumes.size() > 1) {
        MessageDialog dlg(q, _L("Unable to replace with more than one volume"), _L("Error during replace"), wxOK | wxOK_DEFAULT | wxICON_WARNING);
        dlg.ShowModal();
        return false;
    }

    wxBusyInfo info(_L("Replace from:") + " " + from_u8(path), q->get_current_canvas3D()->get_wxglcanvas());

    if (!snapshot.empty())
        q->take_snapshot(snapshot);

    ModelObject* old_model_object = model.objects[object_idx];
    ModelVolume* old_volume = old_model_object->volumes[volume_idx];

    bool sinking = old_model_object->min_z() < SINKING_Z_THRESHOLD;

    ModelObject* new_model_object = new_model.objects.front();
    old_model_object->add_volume(*new_model_object->volumes.front());
    ModelVolume* new_volume = old_model_object->volumes.back();
    new_volume->set_new_unique_id();
    new_volume->config.apply(old_volume->config);
    new_volume->set_type(old_volume->type());
    new_volume->set_material_id(old_volume->material_id());
    new_volume->set_transformation(old_volume->get_transformation());
    new_volume->translate(new_volume->get_transformation().get_matrix_no_offset() * (new_volume->source.mesh_offset - old_volume->source.mesh_offset));
    assert(!old_volume->source.is_converted_from_inches || !old_volume->source.is_converted_from_meters);
    if (old_volume->source.is_converted_from_inches)
        new_volume->convert_from_imperial_units();
    else if (old_volume->source.is_converted_from_meters)
        new_volume->convert_from_meters();
    new_volume->supported_facets.assign(old_volume->supported_facets);
    new_volume->seam_facets.assign(old_volume->seam_facets);
    new_volume->mmu_segmentation_facets.assign(old_volume->mmu_segmentation_facets);
    std::swap(old_model_object->volumes[volume_idx], old_model_object->volumes.back());
    old_model_object->delete_volume(old_model_object->volumes.size() - 1);
    if (!sinking)
        old_model_object->ensure_on_bed();
    old_model_object->sort_volumes(true);

    // if object has just one volume, rename object too
    if (old_model_object->volumes.size() == 1)
        old_model_object->name = old_model_object->volumes.front()->name;

    // update new name in ObjectList
    sidebar->obj_list()->update_name_in_list(object_idx, volume_idx);
    return true;
}

void Plater::priv::replace_with_stl()
{
    if (!m_gizmos->check_gizmos_closed_except(GLGizmosManager::EType::Undefined))
        return;

    if (m_selection.is_wipe_tower() || m_selection.get_volume_idxs().size() != 1)
        return;

    const GLVolume* v = m_selection.get_first_volume();
    int object_idx = v->object_idx();
    int volume_idx = v->volume_idx();

    // collects paths of files to load

    const ModelObject* object = model.objects[object_idx];
    const ModelVolume* volume = object->volumes[volume_idx];

    fs::path input_path;
    if (!volume->source.input_file.empty() && fs::exists(volume->source.input_file))
        input_path = volume->source.input_file;

    wxString title = _L("Select a new file");
    title += ":";
    wxFileDialog dialog(q, title, "", from_u8(input_path.filename().string()), file_wildcards(FT_MODEL), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;

    fs::path out_path = dialog.GetPath().ToUTF8().data();
    if (out_path.empty()) {
        MessageDialog dlg(q, _L("File for the replace wasn't selected"), _L("Error during replace"), wxOK | wxOK_DEFAULT | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }

    if (!replace_volume_with_stl(object_idx, volume_idx, out_path, "Replace with STL"))
        return;

    // update 3D scene
    update();

    // new GLVolumes have been created at this point, so update their printable state
    for (size_t i = 0; i < model.objects.size(); ++i) {
        view3D->get_canvas3d()->update_instance_printable_state_for_object(i);
    }
}

#if ENABLE_RELOAD_FROM_DISK_REWORK
static std::vector<std::pair<int, int>> reloadable_volumes(const Model &model, const Selection &selection)
{
    std::vector<std::pair<int, int>> ret;
    const std::set<unsigned int> &   selected_volumes_idxs = selection.get_volume_idxs();
    for (unsigned int idx : selected_volumes_idxs) {
        const GLVolume &v     = *selection.get_volume(idx);
        const int       o_idx = v.object_idx();
        if (0 <= o_idx && o_idx < int(model.objects.size())) {
            const ModelObject *obj   = model.objects[o_idx];
            const int          v_idx = v.volume_idx();
            if (0 <= v_idx && v_idx < int(obj->volumes.size())) {
                const ModelVolume *vol = obj->volumes[v_idx];
                if (!vol->source.is_from_builtin_objects && !vol->source.input_file.empty() && !fs::path(vol->source.input_file).extension().string().empty())
                    ret.push_back({o_idx, v_idx});
            }
        }
    }
    return ret;
}
#endif // ENABLE_RELOAD_FROM_DISK_REWORK

void Plater::priv::reload_from_disk()
{
#if ENABLE_RELOAD_FROM_DISK_REWORK
    // collect selected reloadable ModelVolumes
    std::vector<std::pair<int, int>> selected_volumes = reloadable_volumes(model, m_selection);
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " entry, and reloadable volumes number is: " << selected_volumes.size();
    // nothing to reload, return
    if (selected_volumes.empty())
        return;

    std::sort(selected_volumes.begin(), selected_volumes.end(), [](const std::pair<int, int> &v1, const std::pair<int, int> &v2) {
        return (v1.first < v2.first) || (v1.first == v2.first && v1.second < v2.second);
        });
    selected_volumes.erase(std::unique(selected_volumes.begin(), selected_volumes.end(), [](const std::pair<int, int> &v1, const std::pair<int, int> &v2) {
        return (v1.first == v2.first) && (v1.second == v2.second);
        }), selected_volumes.end());
#else
    Plater::TakeSnapshot snapshot(q, "Reload from disk");

    if (m_selection.is_wipe_tower())
        return;

    // struct to hold selected ModelVolumes by their indices
    struct SelectedVolume
    {
        int object_idx;
        int volume_idx;

        // operators needed by std::algorithms
        bool operator < (const SelectedVolume& other) const { return object_idx < other.object_idx || (object_idx == other.object_idx && volume_idx < other.volume_idx); }
        bool operator == (const SelectedVolume& other) const { return object_idx == other.object_idx && volume_idx == other.volume_idx; }
    };
    std::vector<SelectedVolume> selected_volumes;

    // collects selected ModelVolumes
    const std::set<unsigned int>& selected_volumes_idxs = selection.get_volume_idxs();
    for (unsigned int idx : selected_volumes_idxs) {
        const GLVolume* v = selection.get_volume(idx);
        int v_idx = v->volume_idx();
        if (v_idx >= 0) {
            int o_idx = v->object_idx();
            if (0 <= o_idx && o_idx < (int)model.objects.size())
                selected_volumes.push_back({ o_idx, v_idx });
        }
    }
    std::sort(selected_volumes.begin(), selected_volumes.end());
    selected_volumes.erase(std::unique(selected_volumes.begin(), selected_volumes.end()), selected_volumes.end());
#endif // ENABLE_RELOAD_FROM_DISK_REWORK

    // collects paths of files to load
    std::vector<fs::path> input_paths;
    std::vector<fs::path> missing_input_paths;
#if ENABLE_RELOAD_FROM_DISK_REWORK
    std::vector<std::pair<fs::path, fs::path>> replace_paths;
    for (auto [obj_idx, vol_idx] : selected_volumes) {
        const ModelObject *object = model.objects[obj_idx];
        const ModelVolume *volume = object->volumes[vol_idx];
        if (fs::exists(volume->source.input_file))
            input_paths.push_back(volume->source.input_file);
        else {
            // searches the source in the same folder containing the object
            bool found = false;
            if (!object->input_file.empty()) {
                fs::path object_path = fs::path(object->input_file).remove_filename();
                if (!object_path.empty()) {
                    object_path /= fs::path(volume->source.input_file).filename();
                    if (fs::exists(object_path)) {
                        input_paths.push_back(object_path);
                        found = true;
                    }
                }
            }
            if (!found)
                missing_input_paths.push_back(volume->source.input_file);
        }
    }
#else
    std::vector<fs::path> replace_paths;
    for (const SelectedVolume& v : selected_volumes) {
        const ModelObject* object = model.objects[v.object_idx];
        const ModelVolume* volume = object->volumes[v.volume_idx];

        if (!volume->source.input_file.empty()) {
            if (fs::exists(volume->source.input_file))
                input_paths.push_back(volume->source.input_file);
            else {
                // searches the source in the same folder containing the object
                bool found = false;
                if (!object->input_file.empty()) {
                    fs::path object_path = fs::path(object->input_file).remove_filename();
                    if (!object_path.empty()) {
                        object_path /= fs::path(volume->source.input_file).filename();
                        const std::string source_input_file = object_path.string();
                        if (fs::exists(source_input_file)) {
                            input_paths.push_back(source_input_file);
                            found = true;
                        }
                    }
                }
                if (!found)
                    missing_input_paths.push_back(volume->source.input_file);
            }
        }
        else if (!object->input_file.empty() && volume->is_model_part() && !volume->name.empty() && !volume->source.is_from_builtin_objects)
            missing_input_paths.push_back(volume->name);
    }
#endif // ENABLE_RELOAD_FROM_DISK_REWORK

    std::sort(missing_input_paths.begin(), missing_input_paths.end());
    missing_input_paths.erase(std::unique(missing_input_paths.begin(), missing_input_paths.end()), missing_input_paths.end());

    while (!missing_input_paths.empty()) {
        // ask user to select the missing file
        fs::path search = missing_input_paths.back();
        wxString title = _L("Please select a file");
#if defined(__APPLE__)
        title += " (" + from_u8(search.filename().string()) + ")";
#endif // __APPLE__
        title += ":";
        wxFileDialog dialog(q, title, "", from_u8(search.filename().string()), file_wildcards(FT_MODEL), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK)
            return;

        std::string sel_filename_path = dialog.GetPath().ToUTF8().data();
        std::string sel_filename = fs::path(sel_filename_path).filename().string();
        if (boost::algorithm::iequals(search.filename().string(), sel_filename)) {
            input_paths.push_back(sel_filename_path);
            missing_input_paths.pop_back();

            fs::path sel_path = fs::path(sel_filename_path).remove_filename().string();

            std::vector<fs::path>::iterator it = missing_input_paths.begin();
            while (it != missing_input_paths.end()) {
                // try to use the path of the selected file with all remaining missing files
                fs::path repathed_filename = sel_path;
                repathed_filename /= it->filename();
                if (fs::exists(repathed_filename)) {
                    input_paths.push_back(repathed_filename.string());
                    it = missing_input_paths.erase(it);
                }
                else
                    ++it;
            }
        }
        else {
            wxString      message = _L("Do you want to replace it") + " ?";
            MessageDialog dlg(q, message, _L("Message"), wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION);
            if (dlg.ShowModal() == wxID_YES)
#if ENABLE_RELOAD_FROM_DISK_REWORK
                replace_paths.emplace_back(search, sel_filename_path);
#else
                replace_paths.emplace_back(sel_filename_path);
#endif // ENABLE_RELOAD_FROM_DISK_REWORK
            missing_input_paths.pop_back();
        }
    }

    std::sort(input_paths.begin(), input_paths.end());
    input_paths.erase(std::unique(input_paths.begin(), input_paths.end()), input_paths.end());

    std::sort(replace_paths.begin(), replace_paths.end());
    replace_paths.erase(std::unique(replace_paths.begin(), replace_paths.end()), replace_paths.end());

#if ENABLE_RELOAD_FROM_DISK_REWORK
    Plater::TakeSnapshot snapshot(q, "Reload from disk");
#endif // ENABLE_RELOAD_FROM_DISK_REWORK

    std::vector<wxString> fail_list;

    // load one file at a time
    for (size_t i = 0; i < input_paths.size(); ++i) {
        const auto& path = input_paths[i].string();
        auto obj_color_fun = [this, &path](std::vector<RGBA> &input_colors, bool is_single_color, std::vector<unsigned char> &filament_ids, unsigned char &first_extruder_id) {
            if (!boost::iends_with(path, ".obj")) { return; }
            const std::vector<std::string> extruder_colours = AppAdapter::plater()->get_extruder_colors_from_plater_config();
            ObjColorDialog                 color_dlg(nullptr, input_colors, is_single_color, extruder_colours, filament_ids, first_extruder_id);
            if (color_dlg.ShowModal() != wxID_OK) { filament_ids.clear(); }
        };
        wxBusyCursor wait;
        wxBusyInfo info(_L("Reload from:") + " " + from_u8(path), q->get_current_canvas3D()->get_wxglcanvas());

        Model new_model;
        try
        {
            //BBS: add plate data related logic
            PlateDataPtrs plate_data;
            //BBS: project embedded settings
            std::vector<Preset*> project_presets;

            // BBS: backup
            new_model = Model::read_from_file(path, nullptr, nullptr, LoadStrategy::AddDefaultInstances | LoadStrategy::LoadModel, &plate_data, &project_presets, nullptr,
                                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, obj_color_fun);
            for (ModelObject* model_object : new_model.objects)
            {
                model_object->center_around_origin();
                model_object->ensure_on_bed();
            }

            if (plate_data.size() > 0)
            {
                //partplate_list.load_from_3mf_structure(plate_data);
                partplate_list.update_slice_context_to_current_plate(background_process);
                this->preview->update_gcode_result(partplate_list.get_current_slice_result_wrapper());
                release_PlateData_list(plate_data);
                sidebar->obj_list()->reload_all_plates();
            }
        }
        catch (std::exception&)
        {
            // error while loading
            return;
        }

#if ENABLE_RELOAD_FROM_DISK_REWORK
        for (auto [obj_idx, vol_idx] : selected_volumes) {
            ModelObject *old_model_object = model.objects[obj_idx];
            ModelVolume *old_volume       = old_model_object->volumes[vol_idx];

            bool sinking = old_model_object->min_z() < SINKING_Z_THRESHOLD;

            bool has_source = !old_volume->source.input_file.empty() &&
                              boost::algorithm::iequals(fs::path(old_volume->source.input_file).filename().string(), fs::path(path).filename().string());
            bool has_name = !old_volume->name.empty() && boost::algorithm::iequals(old_volume->name, fs::path(path).filename().string());
            if (has_source || has_name) {
                int  new_volume_idx = -1;
                int  new_object_idx = -1;
                bool match_found    = false;
                // take idxs from the matching volume
                if (has_source && old_volume->source.object_idx < int(new_model.objects.size())) {
                    const ModelObject *obj = new_model.objects[old_volume->source.object_idx];
                    if (old_volume->source.volume_idx < int(obj->volumes.size())) {
                        if (obj->volumes[old_volume->source.volume_idx]->source.input_file == old_volume->source.input_file) {
                            new_volume_idx = old_volume->source.volume_idx;
                            new_object_idx = old_volume->source.object_idx;
                            match_found    = true;
                        }
                    }
                }

                if (!match_found && has_name) {
                    // take idxs from the 1st matching volume
                    for (size_t o = 0; o < new_model.objects.size(); ++o) {
                        ModelObject *obj   = new_model.objects[o];
                        bool         found = false;
                        for (size_t v = 0; v < obj->volumes.size(); ++v) {
                            if (obj->volumes[v]->name == old_volume->name) {
                                new_volume_idx = (int) v;
                                new_object_idx = (int) o;
                                found          = true;
                                break;
                            }
                        }
                        if (found) break;
                        // BBS: step model,object loaded as a volume. GUI_ObfectList.cpp load_modifier()
                        if (obj->name == old_volume->name) {
                            new_object_idx = (int) o;
                            break;
                        }
                    }
                }

                if (new_object_idx < 0 || int(new_model.objects.size()) <= new_object_idx) {
                    fail_list.push_back(from_u8(has_source ? old_volume->source.input_file : old_volume->name));
                    continue;
                }
                ModelObject *new_model_object = new_model.objects[new_object_idx];
                if (int(new_model_object->volumes.size()) <= new_volume_idx) {
                    fail_list.push_back(from_u8(has_source ? old_volume->source.input_file : old_volume->name));
                    continue;
                }

                ModelVolume *new_volume = nullptr;
                // BBS: step model
                if (new_volume_idx < 0 && new_object_idx >= 0) {
                    TriangleMesh mesh = new_model_object->mesh();
                    new_volume = old_model_object->add_volume(std::move(mesh));
                    new_volume->name  = new_model_object->name;
                    new_volume->source.input_file = new_model_object->input_file;
                }else {
                    new_volume = old_model_object->add_volume(*new_model_object->volumes[new_volume_idx]);
                    // new_volume = old_model_object->volumes.back();
                }
                
                new_volume->set_new_unique_id();
                new_volume->config.apply(old_volume->config);
                new_volume->set_type(old_volume->type());
                new_volume->set_material_id(old_volume->material_id());

                new_volume->source.mesh_offset = old_volume->source.mesh_offset;
                new_volume->set_transformation(old_volume->get_transformation());

                new_volume->source.object_idx = old_volume->source.object_idx;
                new_volume->source.volume_idx = old_volume->source.volume_idx;
                assert(!old_volume->source.is_converted_from_inches || !old_volume->source.is_converted_from_meters);
                if (old_volume->source.is_converted_from_inches)
                    new_volume->convert_from_imperial_units();
                else if (old_volume->source.is_converted_from_meters)
                    new_volume->convert_from_meters();
                std::swap(old_model_object->volumes[vol_idx], old_model_object->volumes.back());
                old_model_object->delete_volume(old_model_object->volumes.size() - 1);
                if (!sinking) old_model_object->ensure_on_bed();
                old_model_object->sort_volumes(AppAdapter::app_config()->get("order_volumes") == "1");

                // Fix warning icon in object list
                AppAdapter::obj_list()->update_item_error_icon(obj_idx, vol_idx);
            }
        }
#else
        // update the selected volumes whose source is the current file
        for (const SelectedVolume& sel_v : selected_volumes) {
            ModelObject* old_model_object = model.objects[sel_v.object_idx];
            ModelVolume* old_volume = old_model_object->volumes[sel_v.volume_idx];

            bool sinking = old_model_object->bounding_box().min.z() < SINKING_Z_THRESHOLD;

            bool has_source = !old_volume->source.input_file.empty() && boost::algorithm::iequals(fs::path(old_volume->source.input_file).filename().string(), fs::path(path).filename().string());
            bool has_name = !old_volume->name.empty() && boost::algorithm::iequals(old_volume->name, fs::path(path).filename().string());
            if (has_source || has_name) {
                int new_volume_idx = -1;
                int new_object_idx = -1;
//                if (has_source) {
//                    // take idxs from source
//                    new_volume_idx = old_volume->source.volume_idx;
//                    new_object_idx = old_volume->source.object_idx;
//                }
//                else {
                    // take idxs from the 1st matching volume
                    for (size_t o = 0; o < new_model.objects.size(); ++o) {
                        ModelObject* obj = new_model.objects[o];
                        bool found = false;
                        for (size_t v = 0; v < obj->volumes.size(); ++v) {
                            if (obj->volumes[v]->name == old_volume->name) {
                                new_volume_idx = (int)v;
                                new_object_idx = (int)o;
                                found = true;
                                break;
                            }
                        }
                        if (found)
                            break;
                    }
//                }

                if (new_object_idx < 0 || int(new_model.objects.size()) <= new_object_idx) {
                    fail_list.push_back(from_u8(has_source ? old_volume->source.input_file : old_volume->name));
                    continue;
                }
                ModelObject* new_model_object = new_model.objects[new_object_idx];
                if (new_volume_idx < 0 || int(new_model_object->volumes.size()) <= new_volume_idx) {
                    fail_list.push_back(from_u8(has_source ? old_volume->source.input_file : old_volume->name));
                    continue;
                }

                old_model_object->add_volume(*new_model_object->volumes[new_volume_idx]);
                ModelVolume* new_volume = old_model_object->volumes.back();
                new_volume->set_new_unique_id();
                new_volume->config.apply(old_volume->config);
                new_volume->set_type(old_volume->type());
                new_volume->set_material_id(old_volume->material_id());
                new_volume->set_transformation(old_volume->get_transformation());
                new_volume->translate(new_volume->get_transformation().get_matrix_no_offset() * (new_volume->source.mesh_offset - old_volume->source.mesh_offset));
                new_volume->source.object_idx = old_volume->source.object_idx;
                new_volume->source.volume_idx = old_volume->source.volume_idx;
                assert(! old_volume->source.is_converted_from_inches || ! old_volume->source.is_converted_from_meters);
                if (old_volume->source.is_converted_from_inches)
                    new_volume->convert_from_imperial_units();
                else if (old_volume->source.is_converted_from_meters)
                    new_volume->convert_from_meters();
                std::swap(old_model_object->volumes[sel_v.volume_idx], old_model_object->volumes.back());
                old_model_object->delete_volume(old_model_object->volumes.size() - 1);
                if (!sinking)
                    old_model_object->ensure_on_bed();
                old_model_object->sort_volumes(true);
            }
        }
#endif // ENABLE_RELOAD_FROM_DISK_REWORK
    }

#if ENABLE_RELOAD_FROM_DISK_REWORK
    for (auto [src, dest] : replace_paths) {
        for (auto [obj_idx, vol_idx] : selected_volumes) {
            if (boost::algorithm::iequals(model.objects[obj_idx]->volumes[vol_idx]->source.input_file, src.string()))
                // When an error occurs, either the dest parsing error occurs, or the number of objects in the dest is greater than 1 and cannot be replaced, and cannot be replaced in this loop.
                if (!replace_volume_with_stl(obj_idx, vol_idx, dest, "")) break;
        }
    }
#else
    for (size_t i = 0; i < replace_paths.size(); ++i) {
        const auto& path = replace_paths[i].string();
        for (const SelectedVolume& sel_v : selected_volumes) {
            ModelObject* old_model_object = model.objects[sel_v.object_idx];
            ModelVolume* old_volume = old_model_object->volumes[sel_v.volume_idx];
            bool has_source = !old_volume->source.input_file.empty() && boost::algorithm::iequals(fs::path(old_volume->source.input_file).filename().string(), fs::path(path).filename().string());
            if (!replace_volume_with_stl(sel_v.object_idx, sel_v.volume_idx, path, "")) {
                fail_list.push_back(from_u8(has_source ? old_volume->source.input_file : old_volume->name));
            }
        }
    }
#endif // ENABLE_RELOAD_FROM_DISK_REWORK

    if (!fail_list.empty()) {
        wxString message = _L("Unable to reload:") + "\n";
        for (const wxString& s : fail_list) {
            message += s + "\n";
        }
        MessageDialog dlg(q, message, _L("Error during reload"), wxOK | wxOK_DEFAULT | wxICON_WARNING);
        dlg.ShowModal();
    }

    // update 3D scene
    update();

    // new GLVolumes have been created at this point, so update their printable state
    for (size_t i = 0; i < model.objects.size(); ++i) {
        view3D->get_canvas3d()->update_instance_printable_state_for_object(i);
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " finish.";
}

void Plater::priv::reload_all_from_disk()
{
    if (model.objects.empty())
        return;

    Plater::TakeSnapshot snapshot(q, "Reload all");
    Plater::SuppressSnapshots suppress(q);

    Selection::IndicesList curr_idxs = m_selection.get_volume_idxs();
    // reload from disk uses selection
    select_all();
    reload_from_disk();
    // restore previous selection
    m_selection.clear();
    for (unsigned int idx : curr_idxs) {
        m_selection.add(idx, false);
    }
}

void Plater::priv::select_view3d(bool no_slice)
{
    select_view_3D("3D", no_slice);
}

void Plater::priv::select_preview(bool no_slice)
{
    select_view_3D("Preview", no_slice);
}

void Plater::priv::select_view_3D(const std::string& name, bool no_slice)
{
    if (name == "3D") {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << __LINE__ << "select view3D";
        if (q->using_exported_file()) {
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format("goto preview page when loading gcode/exported_3mf");
        }
        set_current_panel(view3D, no_slice);
    }
    else if (name == "Preview") {
        BOOST_LOG_TRIVIAL(info) << "select preview";
        set_current_panel(preview, no_slice);
    }

    //BBS update selection
    AppAdapter::obj_list()->update_selections();
    selection_changed();

    apply_free_camera_correction(false);
}

void Plater::priv::set_current_panel(wxPanel* panel, bool no_slice)
{
    if (std::find(panels.begin(), panels.end(), panel) == panels.end())
        return;

    this->enable_sidebar(true);

    panel->Show(); // to reduce flickering when changing view, first set as visible the new current panel
    if (current_panel)
        current_panel->detach();

    current_panel = static_cast<OpenGLPanel*>(panel);
    
    if (current_panel)
        current_panel->attach();

    if (current_panel == preview) 
        do_reslice(no_slice);

    update_sidebar(true);

    if (notification_manager != nullptr)
        notification_manager->set_in_preview(true);
    current_panel->SetFocusFromKbd();

}

// BBS
void Plater::priv::on_combobox_select(wxCommandEvent &evt)
{
    PlaterPresetComboBox* preset_combo_box = dynamic_cast<PlaterPresetComboBox*>(evt.GetEventObject());
    if (preset_combo_box) {
        this->on_select_preset(evt);
    }
    else {
        this->on_select_bed_type(evt);
    }
}

void Plater::priv::on_select_bed_type(wxCommandEvent &evt)
{
    ComboBox* combo = static_cast<ComboBox*>(evt.GetEventObject());
    int selection = combo->GetSelection();
    std::string bed_type_name = print_config_def.get("curr_bed_type")->enum_values[selection];

    PresetBundle& preset_bundle = *app_preset_bundle();
    DynamicPrintConfig& proj_config = app_preset_bundle()->project_config;
    const t_config_enum_values* keys_map = print_config_def.get("curr_bed_type")->enum_keys_map;

    if (keys_map) {
        BedType new_bed_type = btCount;
        for (auto item : *keys_map) {
            if (item.first == bed_type_name) {
                new_bed_type = (BedType)item.second;
                break;
            }
        }

        if (new_bed_type != btCount) {
            BedType old_bed_type = proj_config.opt_enum<BedType>("curr_bed_type");
            if (old_bed_type != new_bed_type) {
                proj_config.set_key_value("curr_bed_type", new ConfigOptionEnum<BedType>(new_bed_type));

                AppAdapter::plater()->update_project_dirty_from_presets();

                // update plater with new config
                q->on_config_change(app_preset_bundle()->full_config());

                // update app_config
                AppConfig* app_config = AppAdapter::app_config();
                app_config->set("curr_bed_type", std::to_string(int(new_bed_type)));
                app_config->set_printer_setting(app_preset_bundle()->printers.get_selected_preset_name(),
                                                "curr_bed_type", std::to_string(int(new_bed_type)));

                //update slice status
                auto plate_list = partplate_list.get_plate_list();
                for (auto plate : plate_list) {
                    if (plate->get_bed_type() == btDefault) {
                        plate->update_slice_result_valid_state(false);
                    }
                }

                // update render
                view3D->get_canvas3d()->render();
                preview->msw_rescale();
            }
        }
    }
}

void Plater::priv::on_select_preset(wxCommandEvent &evt)
{
    PlaterPresetComboBox* combo = static_cast<PlaterPresetComboBox*>(evt.GetEventObject());
    Preset::Type preset_type    = combo->get_type();

    // Under OSX: in case of use of a same names written in different case (like "ENDER" and "Ender"),
    // m_presets_choice->GetSelection() will return first item, because search in PopupListCtrl is case-insensitive.
    // So, use GetSelection() from event parameter
    int selection = evt.GetSelection();

    auto marker = reinterpret_cast<size_t>(combo->GetClientData(selection));
    auto idx = combo->get_filament_idx();

    // BBS:Save the plate parameters before switching
    PartPlateList& old_plate_list = this->partplate_list;
    PartPlate* old_plate = old_plate_list.get_selected_plate();
    Vec3d old_plate_pos = old_plate->get_center_origin();

    // BBS: Save the model in the current platelist
    std::vector<std::vector<int> > plate_object;
    for (size_t i = 0; i < old_plate_list.get_plate_count(); ++i) {
        PartPlate* plate = old_plate_list.get_plate(i);
        std::vector<int> obj_idxs;
        for (int obj_idx = 0; obj_idx < model.objects.size(); obj_idx++) {
            if (plate && plate->contain_instance(obj_idx, 0)) {
                obj_idxs.emplace_back(obj_idx);
            }
        }
        plate_object.emplace_back(obj_idxs);
    }

    bool flag = is_support_filament(idx);
    //! Because of The MSW and GTK version of wxBitmapComboBox derived from wxComboBox,
    //! but the OSX version derived from wxOwnerDrawnCombo.
    //! So, to get selected string we do
    //!     combo->GetString(combo->GetSelection())
    //! instead of
    //!     combo->GetStringSelection().ToUTF8().data());

    std::string preset_name = app_preset_bundle()->get_preset_name_by_alias(preset_type,
        Preset::remove_suffix_modified(combo->GetString(selection).ToUTF8().data()));

    if (preset_type == Preset::TYPE_FILAMENT) {
        app_preset_bundle()->set_filament_preset(idx, preset_name);
        AppAdapter::plater()->update_project_dirty_from_presets();
        app_preset_bundle()->export_selections(*AppAdapter::app_config());
        sidebar->update_dynamic_filament_list();
        bool flag_is_change = is_support_filament(idx);
        if (flag != flag_is_change) {
            sidebar->auto_calc_flushing_volumes(idx);
        }
    }
    bool select_preset = !combo->selection_is_changed_according_to_physical_printers();
    // TODO: ?
    if (preset_type == Preset::TYPE_FILAMENT && sidebar->is_multifilament()) {
        // Only update the plater UI for the 2nd and other filaments.
        combo->update();
    }
    else if (select_preset) {
        if (preset_type == Preset::TYPE_PRINTER) {
            PhysicalPrinterCollection& physical_printers = app_preset_bundle()->physical_printers;
            if(combo->is_selected_physical_printer())
                preset_name = physical_printers.get_selected_printer_preset_name();
            else
                physical_printers.unselect_printer();
        }
        //BBS
        //wxWindowUpdateLocker noUpdates1(sidebar->print_panel());
        wxWindowUpdateLocker noUpdates2(sidebar->filament_panel());
        AppAdapter::gui_app()->get_tab(preset_type)->select_preset(preset_name);
    }

    // update plater with new config
    q->on_config_change(app_preset_bundle()->full_config());
    if (preset_type == Preset::TYPE_PRINTER) {
    /* Settings list can be changed after printer preset changing, so
     * update all settings items for all item had it.
     * Furthermore, Layers editing is implemented only for FFF printers
     * and for SLA presets they should be deleted
     */
        AppAdapter::obj_list()->update_object_list_by_printer_technology();

        // BBS:Model reset by plate center
        PartPlateList& cur_plate_list = this->partplate_list;
        PartPlate* cur_plate = cur_plate_list.get_curr_plate();
        Vec3d cur_plate_pos = cur_plate->get_center_origin();

        if (old_plate_pos.x() != cur_plate_pos.x() || old_plate_pos.y() != cur_plate_pos.y()) {
            for (int i = 0; i < plate_object.size(); ++i) {
                view3D->select_object_from_idx(plate_object[i]);
                this->sidebar->obj_list()->update_selections();
                view3D->center_selected_plate(i);
            }

            view3D->deselect_all();
        }
    }

#ifdef __WXMSW__
    // From the Win 2004 preset combobox lose a focus after change the preset selection
    // and that is why the up/down arrow doesn't work properly
    // So, set the focus to the combobox explicitly
    combo->SetFocus();
#endif
    if (preset_type == Preset::TYPE_FILAMENT && AppAdapter::app_config()->get("auto_calculate_when_filament_change") == "true") {
        AppAdapter::plater()->sidebar().auto_calc_flushing_volumes(idx);
    }

    // BBS: log modify of filament selection
    Slic3r::put_other_changes();

    // update slice state and set bedtype default for 3rd-party printer
    auto plate_list = partplate_list.get_plate_list();
    for (auto plate : plate_list) {
         plate->update_slice_result_valid_state(false);
    }
}

void Plater::priv::on_slicing_update(SlicingStatusEvent &evt)
{
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": event_type %1%, percent %2%, text %3%") % evt.GetEventType() % evt.status.percent % evt.status.text;
    //BBS: add slice project logic
    std::string title_text = _u8L("Slicing");
    evt.status.text = title_text + evt.status.text;
    if (evt.status.percent >= 0) {
         if (!m_worker.is_idle()) {
            // Avoid a race condition
            return;
        }

        notification_manager->set_slicing_progress_percentage(evt.status.text, (float)evt.status.percent / 100.0f);

        // update slicing percent
        PartPlateList& plate_list = AppAdapter::plater()->get_partplate_list();
        //slicing parallel, only update if percent is greater than before
        if (evt.status.percent > plate_list.get_curr_plate()->get_slicing_percent())
            plate_list.get_curr_plate()->update_slicing_percent(evt.status.percent);
    }

    if (evt.status.flags & (PrintBase::SlicingStatus::RELOAD_SCENE | PrintBase::SlicingStatus::RELOAD_SLA_SUPPORT_POINTS)) {
        //BBS: add slice project logic, only display shells at the beginning
        if (!m_slice_all || (m_cur_slice_plate == (partplate_list.get_plate_count() - 1))) {
            //this->update_fff_scene();
            this->update_fff_scene_only_shells();
        }
    } else if(evt.status.flags & PrintBase::SlicingStatus::RELOAD_SLA_PREVIEW) {
        // Update the SLA preview. Only called if not RELOAD_SLA_SUPPORT_POINTS, as the block above will refresh the preview anyways.
        this->preview->reload_print();
    }

    if (evt.status.flags & (PrintBase::SlicingStatus::UPDATE_PRINT_STEP_WARNINGS | PrintBase::SlicingStatus::UPDATE_PRINT_OBJECT_STEP_WARNINGS)) {
        // Update notification center with warnings of object_id and its warning_step.
        ObjectID object_id = evt.status.warning_object_id;
        int warning_step = evt.status.warning_step;
        PrintStateBase::StateWithWarnings state;
        ModelObject const * model_object = nullptr;

        //BBS: add partplate related logic, use the print in background process
        // if (evt.status.flags & PrintBase::SlicingStatus::UPDATE_PRINT_STEP_WARNINGS) {
        //     state = this->background_process.m_fff_print->step_state_with_warnings(static_cast<PrintStep>(warning_step));
        // } else {
        //     const PrintObject *print_object = this->background_process.m_fff_print->get_object(object_id);
        //     if (print_object) {
        //         state = print_object->step_state_with_warnings(static_cast<PrintObjectStep>(warning_step));
        //         model_object = print_object->model_object();
        //     }
        // }
        // Now process state.warnings.
        for (auto const& warning : state.warnings) {
            if (warning.current) {
                NotificationManager::NotificationLevel notif_level = NotificationManager::NotificationLevel::WarningNotificationLevel;
                if (evt.status.message_type == PrintStateBase::SlicingNotificationType::SlicingReplaceInitEmptyLayers || evt.status.message_type == PrintStateBase::SlicingNotificationType::SlicingEmptyGcodeLayers) {
                    notif_level = NotificationManager::NotificationLevel::SeriousWarningNotificationLevel;
                }
                notification_manager->push_slicing_warning_notification(warning.message, false, model_object, object_id, warning_step, warning.message_id, notif_level);
                add_warning(warning, object_id.id);
            }
        }
    }
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format("exit.");
}

void Plater::priv::on_slicing_completed(wxCommandEvent & evt)
{
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": event_type %1%, string %2%") % evt.GetEventType() % evt.GetString();
    //BBS: add slice project logic
    if (m_slice_all && (m_cur_slice_plate < (partplate_list.get_plate_count() - 1))) {
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format("slicing all, finished plate %1%, will continue next.")%m_cur_slice_plate;
        return;
    }

    if (view3D->is_dragging()) // updating scene now would interfere with the gizmo dragging
        delayed_scene_refresh = true;
    else {
        //BBS: only reload shells
        this->update_fff_scene_only_shells(false);
        //this->update_fff_scene();
    }
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format("exit.");
}

void Plater::priv::on_export_began(wxCommandEvent& evt)
{
    if (show_warning_dialog)
        warnings_dialog();
}

void Plater::priv::on_export_finished(wxCommandEvent& evt)
{
}

void Plater::priv::on_slicing_began()
{
    clear_warnings();
    notification_manager->close_notification_of_type(NotificationType::SignDetected);
    notification_manager->close_notification_of_type(NotificationType::ExportFinished);
    bool is_first_plate = m_cur_slice_plate == 0;
    bool slice_all = m_slice_all;
    bool need_change_dailytips = !(slice_all && !is_first_plate);
    notification_manager->set_slicing_progress_began();
    notification_manager->update_slicing_notif_dailytips(need_change_dailytips);
}
void Plater::priv::add_warning(const Slic3r::PrintStateBase::Warning& warning, size_t oid)
{
    for (auto& it : current_warnings) {
        if (warning.message_id == it.first.message_id) {
            if (warning.message_id != 0 || (warning.message_id == 0 && warning.message == it.first.message))
            {
                if (warning.message_id != 0)
                    it.first.message = warning.message;
                return;
            }
        }
    }
    current_warnings.emplace_back(std::pair<Slic3r::PrintStateBase::Warning, size_t>(warning, oid));
}
void Plater::priv::actualize_slicing_warnings(const PrintBase &print)
{
    std::vector<ObjectID> ids = print.print_object_ids();
    if (ids.empty()) {
        clear_warnings();
        return;
    }
    ids.emplace_back(print.id());
    std::sort(ids.begin(), ids.end());
    notification_manager->remove_slicing_warnings_of_released_objects(ids);
    notification_manager->set_all_slicing_warnings_gray(true);
}
void Plater::priv::actualize_object_warnings(const PrintBase& print)
{
    std::vector<ObjectID> ids;
    for (const ModelObject* object : print.model().objects )
    {
        ids.push_back(object->id());
    }
    std::sort(ids.begin(), ids.end());
    notification_manager->remove_simplify_suggestion_of_released_objects(ids);
}
void Plater::priv::clear_warnings()
{
    notification_manager->close_slicing_errors_and_warnings();
    this->current_warnings.clear();
}
bool Plater::priv::warnings_dialog()
{
    if (current_warnings.empty())
        return true;
    std::string text = _u8L("There are warnings after slicing models:") + "\n";
    for (auto const& it : current_warnings) {
        size_t next_n = it.first.message.find_first_of('\n', 0);
        text += "\n";
        if (next_n != std::string::npos)
            text += it.first.message.substr(0, next_n);
        else
            text += it.first.message;
    }
    //text += "\n\nDo you still wish to export?";
    MessageDialog msg_window(this->q, from_u8(text), _L("warnings"), wxOK);
    const auto    res = msg_window.ShowModal();
    return res == wxID_OK;

}

//BBS: add project slice logic
void Plater::priv::on_process_completed(SlicingProcessCompletedEvent &evt)
{
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": enter, m_ignore_event %1%, status %2%")%m_ignore_event %evt.status();
    //BBS:ignore cancel event for some special case
    if (m_ignore_event)
    {
        m_ignore_event = false;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": ignore this event %1%") % evt.status();
        return;
    }
    //BBS: add project slice logic
    bool is_finished = !m_slice_all || (m_cur_slice_plate == (partplate_list.get_plate_count() - 1));

    //BBS: slice .gcode.3mf file related logic, assign is_finished again
    bool only_has_gcode_need_preview = false;
    auto plate_list = this->partplate_list.get_plate_list();
    bool has_print_instances = false;
    for (auto plate : plate_list)
        has_print_instances = has_print_instances || plate->has_printable_instances();
    if (this->model.objects.empty() && !has_print_instances)
        only_has_gcode_need_preview = true;
    if (only_has_gcode_need_preview && m_slice_all_only_has_gcode) {
        is_finished = (m_cur_slice_plate == (partplate_list.get_plate_count() - 1));
        if (is_finished)
            m_slice_all_only_has_gcode = false;
    }

    // Stop the background task, wait until the thread goes into the "Idle" state.
    // At this point of time the thread should be either finished or canceled,
    // so the following call just confirms, that the produced data were consumed.
    this->background_process.stop();
    notification_manager->set_slicing_progress_export_possible();

    // This bool stops showing export finished notification even when process_completed_with_error is false
    bool has_error = false;
    if (evt.error()) {
        auto message = evt.format_error_message();
        if (evt.critical_error()) {
            if (q->m_tracking_popup_menu) {
                // We don't want to pop-up a message box when tracking a pop-up menu.
                // We postpone the error message instead.
                q->m_tracking_popup_menu_error_message = message.first;
            } else {
                show_error(q, message.first, message.second.size() != 0 && message.second[0] != 0);
                notification_manager->set_slicing_progress_hidden();
            }
        } else {
            std::vector<const ModelObject *> ptrs;
            for (auto oid : message.second)
            {
                // const PrintObject *print_object = this->background_process.m_fff_print->get_object(ObjectID(oid));
                // if (print_object) { ptrs.push_back(print_object->model_object()); }
            }
            notification_manager->push_slicing_error_notification(message.first, ptrs);
        }
        if (evt.invalidate_plater())
        {
            process_completed_with_error = partplate_list.get_curr_plate_index();;
        }
        has_error = true;
        is_finished = true;
    }
    if (evt.cancelled()) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", cancel event, status: %1%") % evt.status();
        this->notification_manager->set_slicing_progress_canceled(_u8L("Slicing Canceled"));
        is_finished = true;
    }

    //BBS: set the current plater's slice result to valid
    if (!this->background_process.empty())
        this->background_process.get_current_plate()->update_slice_result_valid_state(evt.success());

    //BBS: update the action button according to the current plate's status
    bool ready_to_slice = !this->partplate_list.get_curr_plate()->is_slice_result_valid();

    // refresh preview
    if (view3D->is_dragging()) // updating scene now would interfere with the gizmo dragging
        delayed_scene_refresh = true;
    else {
        if (is_finished)
            this->update_fff_scene();
    }

    //BBS: add slice&&print status update logic
    if (evt.cancelled()) {
        ready_to_slice = true;
    } else {
        if (exporting_status != ExportingStatus::NOT_EXPORTING && !has_error) {
            notification_manager->stop_delayed_notifications_of_type(NotificationType::ExportOngoing);
            notification_manager->close_notification_of_type(NotificationType::ExportOngoing);
        }

        // BBS, Generate calibration thumbnail for current plate
        if (!has_error && preview) {
            // generate bbox data
            PlateBBoxData* plate_bbox_data = &partplate_list.get_curr_plate()->cali_bboxes_data;
            *plate_bbox_data = generate_first_layer_bbox();
        }
    }

    exporting_status = ExportingStatus::NOT_EXPORTING;

    if (is_finished)
    {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":finished, reload print soon");
        m_is_slicing = false;
        this->preview->reload_print(false);

        q->SetDropTarget(new PlaterDropTarget(*main_panel, *q));
    }
    else
    {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":slicing all, plate %1% finished, start next slice...")%m_cur_slice_plate;
        m_cur_slice_plate++;

        q->Freeze();
        q->select_plate(m_cur_slice_plate);
        partplate_list.select_plate_view();
        int ret = q->start_next_slice();
        if (ret) {
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":slicing all, plate %1% can not be sliced, will stop")%m_cur_slice_plate;
            m_is_slicing = false;
        }
        //not the last plate
        update_fff_scene_only_shells();
        q->Thaw();
    }
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(", exit.");
}

void Plater::priv::on_action_add(SimpleEvent&)
{
    if (q != nullptr) {
        //q->add_model();
        //BBS open file in toolbar add
        q->add_file();
    }
}

//BBS: add plate from toolbar
void Plater::priv::on_action_add_plate(SimpleEvent&)
{
    if (q != nullptr) {
        take_snapshot("add partplate");
        this->partplate_list.create_plate();
        int new_plate = this->partplate_list.get_plate_count() - 1;
        this->partplate_list.select_plate(new_plate);
        update();

        // BBS set default view
        q->get_camera().requires_zoom_to_plate = REQUIRES_ZOOM_TO_ALL_PLATE;
    }
}

//BBS: remove plate from toolbar
void Plater::priv::on_action_del_plate(SimpleEvent&)
{
    if (q != nullptr) {
        q->delete_plate();
    }
}

//BBS: GUI refactor: GLToolbar
void Plater::priv::on_action_open_project(SimpleEvent&)
{
    if (q != nullptr) {
        q->load_project();
    }
}

//BBS: GUI refactor: slice plate
void Plater::priv::on_action_slice_plate(SimpleEvent&)
{
    if (q != nullptr) {
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":received slice plate event\n" ;
        m_slice_all = false;
        q->reslice();
        q->select_preview();
    }
}

//BBS: GUI refactor: slice all
void Plater::priv::on_action_slice_all(SimpleEvent&)
{
    if (q != nullptr) {
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":received slice project event\n" ;
        m_slice_all = true;
        m_slice_all_only_has_gcode = true;
        m_cur_slice_plate = 0;
        //select plate
        q->select_plate(m_cur_slice_plate);
        q->reslice();
        q->select_preview();
        //BBS: wish to select all plates stats item
        preview->get_canvas3d()->_update_select_plate_toolbar_stats_item(true);
    }
}

void Plater::priv::on_action_print_plate(SimpleEvent&)
{
}

void Plater::priv::on_action_send_to_multi_machine(SimpleEvent&)
{
}

void Plater::priv::on_tab_selection_changing(wxBookCtrlEvent& e)
{
    const int new_sel = e.GetSelection();
    sidebar_layout.show = new_sel == MainPanel::tp3DEditor || new_sel == MainPanel::tpPreview;
    update_sidebar();
}

void Plater::priv::update_gizmos_on_off_state()
{
    view3D->set_as_dirty();
    m_gizmos->update_data();
    m_gizmos->refresh_on_off_state();
}
void Plater::priv::on_action_select_sliced_plate(wxCommandEvent &evt)
{
    if (q != nullptr) {
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":received select sliced plate event\n" ;
    }
    q->select_sliced_plate(evt.GetInt());
}

void Plater::priv::on_action_print_all(SimpleEvent&)
{
}

void Plater::priv::on_action_export_gcode(SimpleEvent&)
{
    if (q != nullptr) {
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":received export gcode event\n" ;
        q->export_gcode(false);
    }
}

void Plater::priv::on_action_send_gcode(SimpleEvent&)
{
    PartPlate* part_plate = partplate_list.get_curr_plate();
    GCodeResultWrapper* gcode_result = nullptr;
    part_plate->get_print(&gcode_result, NULL);
    std::vector<std::string> files = gcode_result->get_area_gcode_paths();

    FrameworkContext* context = AppAdapter::inst()->m_context;
    if(context)
    {
        context->broadcast_cmd("send_gcode", files);

        std::vector<std::string> args;
        args.push_back("LightCluster");
        context->execute_cmd("run_module", args);
    }
}

void Plater::priv::on_action_export_sliced_file(SimpleEvent&)
{
    if (q != nullptr) {
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":received export sliced file event\n" ;
        q->export_gcode_3mf();
    }
}

void Plater::priv::on_action_export_all_sliced_file(SimpleEvent &)
{
    if (q != nullptr) {
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":received export all sliced file event\n";
        q->export_gcode_3mf(true);
    }
}

//BBS: add plate select logic
void Plater::priv::on_plate_selected(SimpleEvent&)
{
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":received plate selected event\n" ;
    sidebar->obj_list()->on_plate_selected(partplate_list.get_curr_plate_index());
}

void Plater::priv::on_action_request_model_id(wxCommandEvent& evt)
{
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ":received import model id event\n" ;
    if (q != nullptr) {
        q->import_model_id(evt.GetString());
    }
}

//BBS: add slice button status update logic
void Plater::priv::on_slice_button_status(bool enable)
{
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ": enable = "<<enable<<"\n";
    if (!background_process.running())
        main_panel->update_slice_print_status(MainPanel::eEventObjectUpdate, enable);
}

void Plater::priv::on_action_split_objects(SimpleEvent&)
{
    split_object();
}

void Plater::priv::on_action_split_volumes(SimpleEvent&)
{
    split_volume();
}

void Plater::priv::on_object_select(SimpleEvent& evt)
{
    AppAdapter::obj_list()->update_selections();
    selection_changed();
}

//BBS: repair model through netfabb
void Plater::priv::on_repair_model(wxCommandEvent &event)
{
    AppAdapter::obj_list()->fix_through_netfabb();
}

void Plater::priv::on_filament_color_changed(wxCommandEvent &event)
{
    //q->update_all_plate_thumbnails(true);
    //q->get_preview_canvas3D()->update_plate_thumbnails();
    int modify_id = event.GetInt();

    auto& ams_multi_color_filment = app_preset_bundle()->ams_multi_color_filment;
    if (modify_id >= 0 && modify_id < ams_multi_color_filment.size())
        ams_multi_color_filment[modify_id].clear();

    if (AppAdapter::app_config()->get("auto_calculate") == "true") {
        sidebar->auto_calc_flushing_volumes(modify_id);
    }
}

void Plater::priv::update_plugin_when_launch(wxCommandEvent &event)
{
    std::string data_dir_str = user_data_dir();
    boost::filesystem::path data_dir_path(data_dir_str);
    auto cache_folder = data_dir_path / "ota";
    std::string changelog_file = cache_folder.string() + "/network_plugins.json";

    UpdatePluginDialog dlg(AppAdapter::main_panel());
    dlg.update_info(changelog_file);
    auto result = dlg.ShowModal();

    auto app_config = AppAdapter::app_config();
    if (!app_config) return;

    if (result == wxID_OK) {
        app_config->set("update_network_plugin", "true");
    }
    else if (result == wxID_NO) {
        app_config->set("update_network_plugin", "false");
    }
}

void Plater::priv::show_preview_only_hint(wxCommandEvent &event)
{
    notification_manager->bbl_show_preview_only_notification(into_u8(_L("Preview only mode:\nThe loaded file contains gcode only, Can not enter the Prepare page")));
}

void Plater::priv::on_apple_change_color_mode(wxSysColourChangedEvent& evt) {
    m_is_dark = wxSystemSettings::GetAppearance().IsDark();
    if (view3D->get_canvas3d() && view3D->get_canvas3d()->is_initialized()) {
        view3D->get_canvas3d()->on_change_color_mode(m_is_dark);
        preview->get_canvas3d()->on_change_color_mode(m_is_dark);
    }

    apply_color_mode();
}

void Plater::priv::on_change_color_mode(SimpleEvent& evt) {
    m_is_dark = AppAdapter::app_config()->get("dark_color_mode") == "1";
    view3D->get_canvas3d()->on_change_color_mode(m_is_dark);
    preview->get_canvas3d()->on_change_color_mode(m_is_dark);

    apply_color_mode();
}

void Plater::priv::apply_color_mode()
{
    const bool is_dark         = dark_mode();
    wxColour   orca_color      = wxColour(59, 68, 70);//wxColour(ColorRGBA::ORCA().r_uchar(), ColorRGBA::ORCA().g_uchar(), ColorRGBA::ORCA().b_uchar());
    orca_color                 = is_dark ? StateColor::darkModeColorFor(orca_color) : StateColor::lightModeColorFor(orca_color);
    wxColour sash_color = is_dark ? wxColour(38, 46, 48) : wxColour(206, 206, 206);
    m_aui_mgr.GetArtProvider()->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR, sash_color);
    m_aui_mgr.GetArtProvider()->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, *wxWHITE);
    m_aui_mgr.GetArtProvider()->SetColour(wxAUI_DOCKART_SASH_COLOUR, sash_color);
    m_aui_mgr.GetArtProvider()->SetColour(wxAUI_DOCKART_BORDER_COLOUR, is_dark ? *wxBLACK : wxColour(165, 165, 165));
}

void Plater::priv::show_right_click_menu(Vec2d mouse_position, wxMenu *menu)
{
    // BBS: GUI refactor: move sidebar to the left
    int x, y;
    plater_get_position(current_panel, AppAdapter::main_panel(), x, y);
    wxPoint position(static_cast<int>(mouse_position.x() + x), static_cast<int>(mouse_position.y() + y));
#ifdef __linux__
    // For some reason on Linux the menu isn't displayed if position is
    // specified (even though the position is sane).
    position = wxDefaultPosition;
#endif
    GLCanvas3D &canvas = *q->canvas3D();
    canvas.apply_retina_scale(mouse_position);
    canvas.set_popup_menu_position(mouse_position);
    q->PopupMenu(menu, position);
    canvas.clear_popup_menu_position();
}

void Plater::priv::on_right_click(RBtnEvent& evt)
{
    int obj_idx = get_selected_object_idx();

    wxMenu* menu = nullptr;

    if (obj_idx == -1) { // no one or several object are selected
        if (evt.data.second) { // right button was clicked on empty space
            if (!m_selection.is_empty()) // several objects are selected in 3DScene
                return;
            menu = menus.default_menu();
        }
        else {
            menu = menus.multi_selection_menu();
        }
    }
    else {
        // If in 3DScene is(are) selected volume(s), but right button was clicked on empty space
        if (evt.data.second)
            return;

        // Each context menu respects to the selected item in ObjectList,
        // so this selection should be updated before menu agyuicreation
        AppAdapter::obj_list()->update_selections();

        {
            // show "Object menu" for each one or several FullInstance instead of FullObject
            const bool is_some_full_instances = m_selection.is_single_full_instance() ||
                                                m_selection.is_single_full_object() ||
                                                m_selection.is_multiple_full_instance();
            const bool is_part = m_selection.is_single_volume() || m_selection.is_single_modifier();

            if (is_some_full_instances)
                menu = menus.object_menu();
            else if (is_part) {
                const GLVolume* gl_volume = m_selection.get_first_volume();
                const ModelVolume *model_volume = get_model_volume(*gl_volume, m_selection.get_model()->objects);
                menu = (model_volume != nullptr && model_volume->is_text()) ? menus.text_part_menu() :
                        (model_volume != nullptr && model_volume->is_svg()) ? menus.svg_part_menu() : 
                    menus.part_menu();
            } else
                menu = menus.multi_selection_menu();
            
        }
    }

    if (q != nullptr && menu) {
        show_right_click_menu(evt.data.first, menu);
    }
}

//BBS: add part plate related logic
void Plater::priv::on_plate_right_click(RBtnPlateEvent& evt)
{
    wxMenu *menu = menus.plate_menu();
    show_right_click_menu(evt.data.first, menu);
}

void Plater::priv::on_update_geometry(Vec3dsEvent<2>&)
{
    // TODO
}

void Plater::priv::on_3dcanvas_mouse_dragging_started(SimpleEvent&)
{
    view3D->get_canvas3d()->reset_sequential_print_clearance();
}

// Update the scene from the background processing,
// if the update message was received during mouse manipulation.
void Plater::priv::on_3dcanvas_mouse_dragging_finished(SimpleEvent&)
{
    if (delayed_scene_refresh) {
        delayed_scene_refresh = false;
        update_sla_scene();
    }

    //partplate_list.reload_all_objects();
}

//BBS: add plate id for thumbnail generate param
void Plater::priv::generate_thumbnail(ThumbnailData& data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params, Camera::EType camera_type, bool use_top_view, bool for_picking, bool ban_light)
{
    view3D->get_canvas3d()->render_thumbnail(data, w, h, thumbnail_params, camera_type, use_top_view, for_picking, ban_light);
}

//BBS: add plate id for thumbnail generate param
ThumbnailsList Plater::priv::generate_thumbnails(const ThumbnailsParams& params, Camera::EType camera_type)
{
    ThumbnailsList thumbnails;
    for (const Vec2d& size : params.sizes) {
        thumbnails.push_back(ThumbnailData());
        Point isize(size); // round to ints
        generate_thumbnail(thumbnails.back(), isize.x(), isize.y(), params, camera_type);
        if (!thumbnails.back().is_valid())
            thumbnails.pop_back();
    }
    return thumbnails;
}

void Plater::priv::generate_calibration_thumbnail(ThumbnailData& data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params)
{
    preview->get_canvas3d()->render_calibration_thumbnail(data, w, h, thumbnail_params);
}

PlateBBoxData Plater::priv::generate_first_layer_bbox()
{
    PlateBBoxData bboxdata;
    // std::vector<BBoxData>& id_bboxes = bboxdata.bbox_objs;
    // BoundingBoxf bbox_all;
    // auto                   print = this->background_process.m_fff_print;
    // auto curr_plate = this->partplate_list.get_curr_plate();
    // auto curr_plate_seq = curr_plate->get_real_print_seq();
    // bboxdata.is_seq_print = (curr_plate_seq == PrintSequence::ByObject);
    // bboxdata.first_extruder = print->get_tool_ordering().first_extruder();
    // bboxdata.bed_type       = bed_type_to_gcode_string(print->config().curr_bed_type.value);
    // // get nozzle diameter
    // auto opt_nozzle_diameters = print->config().option<ConfigOptionFloats>("nozzle_diameter");
    // if (opt_nozzle_diameters != nullptr)
    //     bboxdata.nozzle_diameter = float(opt_nozzle_diameters->get_at(bboxdata.first_extruder));

    // auto objects = print->objects();
    // auto orig = this->partplate_list.get_curr_plate()->get_origin();
    // Vec2d orig2d = { orig[0], orig[1] };

    // BBoxData data;
    // for (auto obj : objects)
    // {
    //     auto bb_scaled = obj->get_first_layer_bbox(data.area, data.layer_height, data.name);
    //     auto bb = unscaled(bb_scaled);
    //     bb.min -= orig2d;
    //     bb.max -= orig2d;
    //     bbox_all.merge(bb);
    //     data.area *= (SCALING_FACTOR * SCALING_FACTOR); // unscale area
    //     data.id = obj->id().id;
    //     data.bbox = { bb.min.x(),bb.min.y(),bb.max.x(),bb.max.y() };
    //     id_bboxes.emplace_back(data);
    // }

    // // add wipe tower bounding box
    // if (print->has_wipe_tower()) {
    //     auto   wt_corners = print->first_layer_wipe_tower_corners();
    //     // when loading gcode.3mf, wipe tower info may not be correct
    //     if (!wt_corners.empty()) {
    //         BoundingBox bb_scaled = {wt_corners[0], wt_corners[2]};
    //         auto        bb        = unscaled(bb_scaled);
    //         bb.min -= orig2d;
    //         bb.max -= orig2d;
    //         bbox_all.merge(bb);
    //         data.name = "wipe_tower";
    //         data.id   = partplate_list.get_curr_plate()->get_index() + 1000;
    //         data.bbox = {bb.min.x(), bb.min.y(), bb.max.x(), bb.max.y()};
    //         id_bboxes.emplace_back(data);
    //     }
    // }

    // bboxdata.bbox_all = { bbox_all.min.x(),bbox_all.min.y(),bbox_all.max.x(),bbox_all.max.y() };
    return bboxdata;
}

wxString Plater::priv::get_project_filename(const wxString& extension) const
{
    if (m_project_name.empty())
        return "";
    else {
        auto full_filename = m_project_folder / std::string((m_project_name + extension).mb_str(wxConvUTF8));
        return m_project_folder.empty() ? "" : from_path(full_filename);
    }
}

wxString Plater::priv::get_export_gcode_filename(const wxString& extension, bool only_filename, bool export_all) const
{
    std::string plate_index_str;
    auto plate_name = partplate_list.get_curr_plate()->get_plate_name();
    if (!plate_name.empty())
        plate_index_str = (boost::format("_%1%") % plate_name).str();
    else if (partplate_list.get_plate_count() > 1)
        plate_index_str = (boost::format("_plate_%1%") % std::to_string(partplate_list.get_curr_plate_index() + 1)).str();

    if (!m_project_folder.empty()) {
        if (!only_filename) {
            if (export_all) {
                auto full_filename = m_project_folder / std::string((m_project_name + extension).mb_str(wxConvUTF8));
                return from_path(full_filename);
            } else {
                auto full_filename = m_project_folder / std::string((m_project_name + from_u8(plate_index_str) + extension).mb_str(wxConvUTF8));
                return from_path(full_filename);
            }
        } else {
            if (export_all)
                return m_project_name + extension;
            else
                return m_project_name + from_u8(plate_index_str) + extension;
        }
    } else {
        if (only_filename) {
            if(m_project_name == _L("Untitled"))
                return wxString(fs::path(model.objects.front()->name).replace_extension().c_str()) + from_u8(plate_index_str) + extension;

            if (export_all)
                return m_project_name + extension;
            else
                return m_project_name + from_u8(plate_index_str) + extension;
        }
        else
            return "";
    }
}

wxString Plater::priv::get_project_name()
{
    return m_project_name;
}

//BBS
void Plater::priv::set_project_name(const wxString& project_name)
{
    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << __LINE__ << " project is:" << project_name;
    m_project_name = project_name;
    //update topbar title

    AppAdapter::main_panel()->set_title(m_project_name);
}

void Plater::priv::update_title_dirty_status()
{
    if (m_project_name.empty())
        return;

    wxString title;
    if (is_project_dirty())
        title = "*" + m_project_name;
    else
        title = m_project_name;

    AppAdapter::main_panel()->set_title(title);  
}

void Plater::priv::set_project_filename(const wxString& filename)
{
    boost::filesystem::path full_path = into_path(filename);
    boost::filesystem::path ext = full_path.extension();

    full_path.replace_extension("");

    m_project_folder = full_path.parent_path();
    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << __LINE__ << " project folder is:" << m_project_folder.string();

    //BBS
    wxString project_name = from_u8(full_path.filename().string());
    set_project_name(project_name);
    // record filename for hint when open exported file/.gcode
    if (q->m_exported_file)
        q->m_preview_only_filename = std::string((project_name + ".3mf").mb_str());

    AppAdapter::main_panel()->update_title();

    if (!m_project_folder.empty())
        AppAdapter::main_panel()->add_to_recent_projects(filename);
}

void Plater::priv::init_notification_manager()
{
    if (!notification_manager)
        return;
    notification_manager->init();

    auto cancel_callback = [this]() {
        if (this->background_process.idle())
            return false;
        this->background_process.stop();
        return true;
    };
    notification_manager->init_slicing_progress_notification(cancel_callback);
    notification_manager->set_fff(true);
    notification_manager->init_progress_indicator();
}

void Plater::priv::set_current_canvas_as_dirty()
{
    if (current_panel == view3D)
        view3D->set_as_dirty();
    else if (current_panel == preview)
        preview->set_as_dirty();
}

GLCanvas3D* Plater::priv::get_current_canvas3D(bool exclude_preview)
{
    if (current_panel == view3D)
        return view3D->get_canvas3d();
    else if (!exclude_preview && (current_panel == preview))
        return preview->get_canvas3d();
    else //BBS default set to view3D
        return view3D->get_canvas3d();
}

void Plater::priv::unbind_canvas_event_handlers()
{
    if (view3D != nullptr)
        view3D->get_canvas3d()->unbind_event_handlers();

    if (preview != nullptr)
        preview->get_canvas3d()->unbind_event_handlers();

}

void Plater::priv::reset_canvas_volumes()
{
    if (view3D != nullptr)
        view3D->get_canvas3d()->reset_volumes();

    if (preview != nullptr)
        preview->get_canvas3d()->reset_volumes();
}

bool Plater::priv::init_collapse_toolbar()
{
    if (collapse_toolbar.get_items_count() > 0)
        // already initialized
        return true;

    BackgroundTexture::Metadata background_data;
    background_data.filename = m_is_dark ? "toolbar_background_dark.png" : "toolbar_background.png";
    background_data.left = 16;
    background_data.top = 16;
    background_data.right = 16;
    background_data.bottom = 16;

    if (!collapse_toolbar.init(background_data))
        return false;

    collapse_toolbar.set_layout_type(GLToolbar::Layout::Vertical);
    collapse_toolbar.set_horizontal_orientation(GLToolbar::Layout::HO_Right);
    collapse_toolbar.set_vertical_orientation(GLToolbar::Layout::VO_Top);
    collapse_toolbar.set_border(4.0f);
    collapse_toolbar.set_separator_size(4);
    collapse_toolbar.set_gap_size(2);

    collapse_toolbar.del_all_item();

    GLToolbarItem::Data item;

    item.name = "collapse_sidebar";
    // set collapse svg name
    item.icon_filename = "collapse.svg";
    item.sprite_id = 0;
    item.left.action_callback = []() {
        AppAdapter::plater()->collapse_sidebar(!AppAdapter::plater()->is_sidebar_collapsed());
    };

    if (!collapse_toolbar.add_item(item))
        return false;

    // Now "collapse" sidebar to current state. This is done so the tooltip
    // is updated before the toolbar is first used.
    AppAdapter::plater()->collapse_sidebar(AppAdapter::plater()->is_sidebar_collapsed());
    return true;
}

void Plater::priv::update_preview_bottom_toolbar()
{
    ;
}

void Plater::priv::reset_gcode_toolpaths()
{
    preview->get_canvas3d()->reset_gcode_toolpaths();
}

bool Plater::priv::can_set_instance_to_object() const
{
    const int obj_idx = get_selected_object_idx();
    return 0 <= obj_idx && obj_idx < (int)model.objects.size() && model.objects[obj_idx]->instances.size() > 1;
}

bool Plater::priv::can_split(bool to_objects) const
{
    return sidebar->obj_list()->is_splittable(to_objects);
}

bool Plater::priv::can_fillcolor() const
{
    //BBS TODO
    return true;
}

bool Plater::priv::can_scale_to_print_volume() const
{
    const BuildVolume_Type type = PlateBed::build_volume().type();
    return !sidebar->obj_list()->has_selected_cut_object()
        && !m_selection.is_empty()
        && (type == BuildVolume_Type::Rectangle || type == BuildVolume_Type::Circle);
}

Bed3D* Plater::priv::get_bed()
{
    //return &bed;
    return NULL;
}

bool Plater::priv::can_mirror() const
{
    return !sidebar->obj_list()->has_selected_cut_object()
        && m_selection.is_from_single_instance();
}

bool Plater::priv::can_replace_with_stl() const
{
    return !sidebar->obj_list()->has_selected_cut_object()
        && m_selection.get_volume_idxs().size() == 1;
}

bool Plater::priv::can_reload_from_disk() const
{
    if (sidebar->obj_list()->has_selected_cut_object())
        return false;

#if ENABLE_RELOAD_FROM_DISK_REWORK
    // collect selected reloadable ModelVolumes
    std::vector<std::pair<int, int>> selected_volumes = reloadable_volumes(model, m_selection);
    // nothing to reload, return
    if (selected_volumes.empty())
        return false;
#else
    // struct to hold selected ModelVolumes by their indices
    struct SelectedVolume
    {
        int object_idx;
        int volume_idx;

        // operators needed by std::algorithms
        bool operator < (const SelectedVolume& other) const { return (object_idx < other.object_idx) || ((object_idx == other.object_idx) && (volume_idx < other.volume_idx)); }
        bool operator == (const SelectedVolume& other) const { return (object_idx == other.object_idx) && (volume_idx == other.volume_idx); }
    };
    std::vector<SelectedVolume> selected_volumes;

    // collects selected ModelVolumes
    const std::set<unsigned int>& selected_volumes_idxs = m_selection.get_volume_idxs();
    for (unsigned int idx : selected_volumes_idxs) {
        const GLVolume* v = m_selection.get_volume(idx);
        int v_idx = v->volume_idx();
        if (v_idx >= 0) {
            int o_idx = v->object_idx();
            if (0 <= o_idx && o_idx < (int)model.objects.size())
                selected_volumes.push_back({ o_idx, v_idx });
        }
    }
#endif // ENABLE_RELOAD_FROM_DISK_REWORK

#if ENABLE_RELOAD_FROM_DISK_REWORK
    std::sort(selected_volumes.begin(), selected_volumes.end(), [](const std::pair<int, int> &v1, const std::pair<int, int> &v2) {
        return (v1.first < v2.first) || (v1.first == v2.first && v1.second < v2.second);
        });
    selected_volumes.erase(std::unique(selected_volumes.begin(), selected_volumes.end(), [](const std::pair<int, int> &v1, const std::pair<int, int> &v2) {
        return (v1.first == v2.first) && (v1.second == v2.second);
        }), selected_volumes.end());

    // collects paths of files to load
    std::vector<fs::path> paths;
    for (auto [obj_idx, vol_idx] : selected_volumes) {
        paths.push_back(model.objects[obj_idx]->volumes[vol_idx]->source.input_file);
    }
#else
    std::sort(selected_volumes.begin(), selected_volumes.end());
    selected_volumes.erase(std::unique(selected_volumes.begin(), selected_volumes.end()), selected_volumes.end());

    // collects paths of files to load
    std::vector<fs::path> paths;
    for (const SelectedVolume& v : selected_volumes) {
        const ModelObject* object = model.objects[v.object_idx];
        const ModelVolume* volume = object->volumes[v.volume_idx];
        if (!volume->source.input_file.empty())
            paths.push_back(volume->source.input_file);
        else if (!object->input_file.empty() && !volume->name.empty() && !volume->source.is_from_builtin_objects)
            paths.push_back(volume->name);
    }
#endif // ENABLE_RELOAD_FROM_DISK_REWORK
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

    return !paths.empty();
}

void Plater::priv::update_bed_shape()
{
    std::string texture_filename;
    auto bundle = app_preset_bundle();
    if (bundle != nullptr) {
        const Preset* curr = &bundle->printers.get_selected_preset();
        if (curr->is_system)
            texture_filename = PresetUtils::system_printer_bed_texture(*curr);
        else {
            auto *printer_model = curr->config.opt<ConfigOptionString>("printer_model");
            if (printer_model != nullptr && ! printer_model->value.empty()) {
                texture_filename = bundle->get_texture_for_printer_model(printer_model->value);
            }
        }
    }

    std::vector<Pointfs> shapes;
    std::vector<Pointfs> exclude_shapes;
    get_bed_shapes_from_config(*config, shapes, exclude_shapes);

    set_bed_shape(shapes,
        exclude_shapes,
        config->option<ConfigOptionFloat>("printable_height")->value,
        config->option<ConfigOptionString>("bed_custom_texture")->value.empty() ? texture_filename : config->option<ConfigOptionString>("bed_custom_texture")->value,
        config->option<ConfigOptionString>("bed_custom_model")->value);    
}

void Plater::priv::on_bed_updated()
{
    int area_count = platepart_area_count_from_config(*config);
    partplate_list.set_plate_area_count(area_count);
}

void Plater::priv::set_bed_shape(const std::vector<Pointfs> & shapes, const std::vector<Pointfs>& exclude_areas, const double printable_height, const std::string& custom_texture, const std::string& custom_model, bool force_as_custom)
{
    Pointfs allShap;
    for(auto & shape : shapes)
    {
         allShap.insert(allShap.end(), shape.begin(), shape.end());
    }
    BoundingBoxf bed_size = get_extents(allShap);

    if (bed_size.size().maxCoeff() <= LARGE_BED_THRESHOLD)
        SCALING_FACTOR = SCALING_FACTOR_INTERNAL;
    else
        SCALING_FACTOR = SCALING_FACTOR_INTERNAL_LARGE_PRINTER;
    
    //BBS: add shape position
    Vec2d shape_position = partplate_list.get_current_shape_position();
    bool new_shape = PlateBed::set_shape(shapes, printable_height, custom_model, force_as_custom, shape_position);

    float prev_height_lid, prev_height_rod;
    partplate_list.get_height_limits(prev_height_lid, prev_height_rod);
    double height_to_lid = config->opt_float("extruder_clearance_height_to_lid");
    double height_to_rod = config->opt_float("extruder_clearance_height_to_rod");

    std::vector<Pointfs> prev_exclude_areas = partplate_list.get_exclude_area();
    new_shape |= (height_to_lid != prev_height_lid) || (height_to_rod != prev_height_rod) || (prev_exclude_areas != exclude_areas);
    if (!new_shape && PlateBed::get_logo_texture_filename() != custom_texture) {
        PlateBed::update_logo_texture_filename(custom_texture);
    }
    if (new_shape) {
        if (view3D) view3D->bed_shape_changed();
        if (preview) preview->bed_shape_changed();

        Vec3d max = PlateBed::extended_bounding_box().max;
        Vec3d min = PlateBed::extended_bounding_box().min;
        double z = config->opt_float("printable_height");

        partplate_list.reset_size(max.x() - min.x() - Bed3D::Axes::DefaultTipRadius, max.y() - min.y() - Bed3D::Axes::DefaultTipRadius, z);
        partplate_list.set_shapes(shapes, exclude_areas, custom_texture, height_to_lid, height_to_rod);

        Vec2d new_shape_position = partplate_list.get_current_shape_position();
        if (shape_position != new_shape_position)
            PlateBed::set_shape(shapes, printable_height, custom_model, force_as_custom, new_shape_position);
    }
}

bool Plater::priv::can_delete() const
{
    return !m_selection.is_empty() && !m_selection.is_wipe_tower();
}

bool Plater::priv::can_delete_all() const
{
    return !model.objects.empty();
}

bool Plater::priv::can_add_plate() const
{
    return q->get_partplate_list().get_plate_count() < PartPlateList::MAX_PLATES_COUNT;
}

bool Plater::priv::can_delete_plate() const
{
    return q->get_partplate_list().get_plate_count() > 1;
}

bool Plater::priv::can_fix_through_netfabb() const
{
    std::vector<int> obj_idxs, vol_idxs;
    sidebar->obj_list()->get_selection_indexes(obj_idxs, vol_idxs);

#if FIX_THROUGH_NETFABB_ALWAYS
    // Fixing always.
    return ! obj_idxs.empty() || ! vol_idxs.empty();
#else // FIX_THROUGH_NETFABB_ALWAYS
    // Fixing only if the model is not manifold.
    if (vol_idxs.empty()) {
        for (auto obj_idx : obj_idxs)
            if (model.objects[obj_idx]->get_repaired_errors_count() > 0)
                return true;
        return false;
    }

    int obj_idx = obj_idxs.front();
    for (auto vol_idx : vol_idxs)
        if (model.objects[obj_idx]->get_repaired_errors_count(vol_idx) > 0)
            return true;
    return false;
#endif // FIX_THROUGH_NETFABB_ALWAYS
}

bool Plater::priv::can_simplify() const
{
    // is object for simplification selected
    if (get_selected_object_idx() < 0) return false;
    // is already opened?
    if (m_gizmos->get_current_type() ==
        GLGizmosManager::EType::Simplify)
        return false;
    return true;
}

bool Plater::priv::can_increase_instances() const
{
    if (!m_worker.is_idle()
     || m_gizmos->is_in_editing_mode())
            return false;

    int obj_idx = get_selected_object_idx();
    return (0 <= obj_idx) && (obj_idx < (int)model.objects.size())
        && !sidebar->obj_list()->has_selected_cut_object()
        && std::all_of(model.objects[obj_idx]->instances.begin(), model.objects[obj_idx]->instances.end(), [](auto& inst) {return inst->printable; });
}

bool Plater::priv::can_decrease_instances() const
{
    if (!m_worker.is_idle()
     || m_gizmos->is_in_editing_mode())
            return false;

    int obj_idx = get_selected_object_idx();
    return (0 <= obj_idx) && (obj_idx < (int)model.objects.size()) && (model.objects[obj_idx]->instances.size() > 1)
        && !sidebar->obj_list()->has_selected_cut_object();
}

bool Plater::priv::can_split_to_objects() const
{
    return q->can_split(true);
}

bool Plater::priv::can_split_to_volumes() const
{
    return q->can_split(false);
}

bool Plater::priv::can_arrange() const
{
    return !model.objects.empty() && m_worker.is_idle();
}

void Plater::priv::do_reslice(bool no_slice) 
{
    bool model_fits = this->view3D->get_canvas3d()->check_volumes_outside_state() != ModelInstancePVS_Partly_Outside;
    PartPlate* current_plate = this->partplate_list.get_curr_plate();
    
    // Layer 1 protection: if plate has external gcode, don't trigger reslice
    if (current_plate && current_plate->has_external_gcode()) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ 
            << " - Plate " << current_plate->get_index() 
            << " has external gcode, skip reslice";
        return;
    }
    
    if (current_plate->is_slice_result_valid())
        return;

    bool only_has_gcode_need_preview = false;
    bool current_has_print_instances = current_plate->has_printable_instances();
    if (current_plate->is_slice_result_valid() && this->model.objects.empty() && !current_has_print_instances)
        only_has_gcode_need_preview = true;

    if (!no_slice && !this->model.objects.empty() && model_fits && current_has_print_instances)
    {
        //if already running in background, not relice here
        //BBS: add more judge for slicing
        if (!this->background_process.running() && !this->m_is_slicing)
        {
            this->m_slice_all = false;
            this->q->reslice();
        }
        else {
            //reset current plate to the slicing plate
            int plate_index = this->background_process.get_current_plate()->get_index();
            this->partplate_list.select_plate(plate_index);
        }
    }
    else if (only_has_gcode_need_preview)
    {
        this->m_slice_all = false;
        this->q->reslice();
    }
    //BBS: process empty plate, reset previous toolpath
    else
    {
        //if (!this->m_slice_all)
        if (!current_has_print_instances)
            reset_gcode_toolpaths();
        //this->q->refresh_print();
        if (!preview->get_canvas3d()->is_initialized())
        {
            preview->get_canvas3d()->render(true);
        }
    }

    // keeps current gcode preview, if any
    if (this->m_slice_all) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": slicing all, just reload shells");
        this->update_fff_scene_only_shells();
    }
    else {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": single slice, reload print");
        if (model_fits)
            this->preview->reload_print(true);
        else
            this->update_fff_scene_only_shells();
    }

    preview->set_as_dirty();
}

bool Plater::priv::layers_height_allowed() const
{
    int obj_idx = get_selected_object_idx();
    return 0 <= obj_idx && obj_idx < (int)model.objects.size() && model.objects[obj_idx]->max_z() > SINKING_Z_THRESHOLD && view3D->is_layers_editing_allowed();
}

bool Plater::priv::can_layers_editing() const
{
    return layers_height_allowed();
}

void Plater::priv::on_action_layersediting(SimpleEvent&)
{
    view3D->enable_layers_editing(!view3D->is_layers_editing_enabled());
    notification_manager->set_move_from_overlay(view3D->is_layers_editing_enabled());
}

void Plater::priv::on_create_filament(SimpleEvent &)
{
    CreateFilamentPresetDialog dlg(AppAdapter::main_panel());
    int res = dlg.ShowModal();
    if (wxID_OK == res) {
        AppAdapter::main_panel()->update_side_preset_ui();
        update_ui_from_settings();
        sidebar->update_all_preset_comboboxes();
        CreatePresetSuccessfulDialog success_dlg(AppAdapter::main_panel(), SuccessType::FILAMENT);
        int                          res = success_dlg.ShowModal();
    }
}

void Plater::priv::on_modify_filament(SimpleEvent &evt)
{
    Filamentinformation *filament_info = static_cast<Filamentinformation *>(evt.GetEventObject());
    int                 res;
    std::shared_ptr<Preset> need_edit_preset;
    {
        EditFilamentPresetDialog dlg(AppAdapter::main_panel(), filament_info);
        res = dlg.ShowModal();
        need_edit_preset = dlg.get_need_edit_preset();
    }
    AppAdapter::main_panel()->update_side_preset_ui();
    update_ui_from_settings();
    sidebar->update_all_preset_comboboxes();
    if (wxID_EDIT == res) {
        Tab *tab = AppAdapter::gui_app()->get_tab(Preset::Type::TYPE_FILAMENT);
        //tab->restore_last_select_item();
        if (tab == nullptr) { return; }
        // Popup needs to be called before "restore_last_select_item", otherwise the page may not be updated
        AppAdapter::gui_app()->params_dialog()->Popup();
        tab->restore_last_select_item();
        // Opening Studio and directly accessing the Filament settings interface through the edit preset button will not take effect and requires manual settings.
        tab->set_just_edit(true);
        tab->select_preset(need_edit_preset->name);
        // when some preset have modified, if the printer is not need_edit_preset_name compatible printer, the preset will jump to other preset, need select again
        if (!need_edit_preset->is_compatible) tab->select_preset(need_edit_preset->name);
    }

}

void Plater::priv::on_add_filament(SimpleEvent &evt) {
    sidebar->add_filament();
}

void Plater::priv::on_delete_filament(SimpleEvent &evt) {
    sidebar->delete_filament();
}

void Plater::priv::on_add_custom_filament(ColorEvent &evt)
{
    sidebar->add_custom_filament(evt.data);
}

void Plater::priv::enter_gizmos_stack()
{
    assert(m_undo_redo_stack_active == &m_undo_redo_stack_main);
    if (m_undo_redo_stack_active == &m_undo_redo_stack_main) {
        m_undo_redo_stack_active = &m_undo_redo_stack_gizmos;
        assert(m_undo_redo_stack_active->empty());
        // Take the initial snapshot of the gizmos.
        // Not localized on purpose, the text will never be shown to the user.
        this->take_snapshot(std::string("Gizmos-Initial"));
    }
}

bool Plater::priv::leave_gizmos_stack()
{
    bool changed = false;
    assert(m_undo_redo_stack_active == &m_undo_redo_stack_gizmos);
    if (m_undo_redo_stack_active == &m_undo_redo_stack_gizmos) {
        assert(! m_undo_redo_stack_active->empty());
        changed = m_undo_redo_stack_gizmos.has_undo_snapshot();
        m_undo_redo_stack_active->clear();
        m_undo_redo_stack_active = &m_undo_redo_stack_main;
    }
    return changed;
}

int Plater::priv::get_active_snapshot_index()
{
    const size_t active_snapshot_time = this->undo_redo_stack().active_snapshot_time();
    const std::vector<UndoRedo::Snapshot>& ss_stack = this->undo_redo_stack().snapshots();
    const auto it = std::lower_bound(ss_stack.begin(), ss_stack.end(), UndoRedo::Snapshot(active_snapshot_time));
    return it - ss_stack.begin();
}

void Plater::priv::take_snapshot(const std::string& snapshot_name, const UndoRedo::SnapshotType snapshot_type)
{
    if (m_prevent_snapshots > 0)
        return;
    assert(m_prevent_snapshots >= 0);
    // BBS: single snapshot
    if (m_single && !m_single->check(snapshot_modifies_project(snapshot_type) && (snapshot_name.empty() || snapshot_name.back() != '!')))
        return;
    UndoRedo::SnapshotData snapshot_data;
    snapshot_data.snapshot_type      = snapshot_type;
    snapshot_data.printer_technology = ptFFF;
    if (this->view3D->is_layers_editing_enabled())
        snapshot_data.flags |= UndoRedo::SnapshotData::VARIABLE_LAYER_EDITING_ACTIVE;
    if (this->sidebar->obj_list()->is_selected(itSettings)) {
        snapshot_data.flags |= UndoRedo::SnapshotData::SELECTED_SETTINGS_ON_SIDEBAR;
        snapshot_data.layer_range_idx = this->sidebar->obj_list()->get_selected_layers_range_idx();
    }
    else if (this->sidebar->obj_list()->is_selected(itLayer)) {
        snapshot_data.flags |= UndoRedo::SnapshotData::SELECTED_LAYER_ON_SIDEBAR;
        snapshot_data.layer_range_idx = this->sidebar->obj_list()->get_selected_layers_range_idx();
    }
    else if (this->sidebar->obj_list()->is_selected(itLayerRoot))
        snapshot_data.flags |= UndoRedo::SnapshotData::SELECTED_LAYERROOT_ON_SIDEBAR;

    // If SLA gizmo is active, ask it if it wants to trigger support generation
    // on loading this snapshot.
    if (m_gizmos->wants_reslice_supports_on_undo())
        snapshot_data.flags |= UndoRedo::SnapshotData::RECALCULATE_SLA_SUPPORTS;

    //FIXME updating the Wipe tower config values at the ModelWipeTower from the Print config.
    // This is a workaround until we refactor the Wipe Tower position / orientation to live solely inside the Model, not in the Print config.
    // BBS: add partplate logic
    if (true) {
        const DynamicPrintConfig& config = app_preset_bundle()->prints.get_edited_preset().config;
        const DynamicPrintConfig& proj_cfg = app_preset_bundle()->project_config;
        const ConfigOptionFloats* tower_x_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_x");
        const ConfigOptionFloats* tower_y_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_y");
        assert(tower_x_opt->values.size() == tower_y_opt->values.size());
        model.wipe_tower.positions.clear();
        model.wipe_tower.positions.resize(tower_x_opt->values.size());
        for (int plate_idx = 0; plate_idx < tower_x_opt->values.size(); plate_idx++) {
            ModelWipeTower& tower = model.wipe_tower;

            tower.positions[plate_idx] = Vec2d(tower_x_opt->get_at(plate_idx), tower_y_opt->get_at(plate_idx));
            tower.rotation = proj_cfg.opt_float("wipe_tower_rotation_angle");
        }
    }

    if (snapshot_type == UndoRedo::SnapshotType::ProjectSeparator)
        this->undo_redo_stack().clear();

    this->undo_redo_stack().take_snapshot(snapshot_name, model, m_selection, *m_gizmos, partplate_list, snapshot_data);
    if (snapshot_type == UndoRedo::SnapshotType::LeavingGizmoWithAction) {
        // Filter all but the last UndoRedo::SnapshotType::GizmoAction in a row between the last UndoRedo::SnapshotType::EnteringGizmo and UndoRedo::SnapshotType::LeavingGizmoWithAction.
        // The remaining snapshot will be renamed to a more generic name,
        // depending on what gizmo is being left.
        if (m_gizmos->get_current() != nullptr) {
            std::string new_name = m_gizmos->get_current()->get_action_snapshot_name();
            this->undo_redo_stack().reduce_noisy_snapshots(new_name);
        }
    } else if (snapshot_type == UndoRedo::SnapshotType::ProjectSeparator) {
        // Reset the "dirty project" flag.
        m_undo_redo_stack_main.mark_current_as_saved();
    }
    //BBS: add PartPlateList as the paremeter for take_snapshot
    this->undo_redo_stack().release_least_recently_used();

    dirty_state.update_from_undo_redo_stack(m_undo_redo_stack_main.project_modified());

    // Save the last active preset name of a particular printer technology.
    m_last_fff_printer_profile_name = app_preset_bundle()->printers.get_selected_preset_name();
    BOOST_LOG_TRIVIAL(info) << "Undo / Redo snapshot taken: " << snapshot_name << ", Undo / Redo stack memory: " << Slic3r::format_memsize_MB(this->undo_redo_stack().memsize()) << log_memory_info();
}

void Plater::priv::undo()
{
    const std::vector<UndoRedo::Snapshot> &snapshots = this->undo_redo_stack().snapshots();
    auto it_current = std::lower_bound(snapshots.begin(), snapshots.end(), UndoRedo::Snapshot(this->undo_redo_stack().active_snapshot_time()));
    // BBS: undo-redo until modify record
    while (--it_current != snapshots.begin() && !snapshot_modifies_project(*it_current));
    if (it_current == snapshots.begin()) return;
    this->undo_redo_to(it_current);
}

void Plater::priv::redo()
{
    const std::vector<UndoRedo::Snapshot> &snapshots = this->undo_redo_stack().snapshots();
    auto it_current = std::lower_bound(snapshots.begin(), snapshots.end(), UndoRedo::Snapshot(this->undo_redo_stack().active_snapshot_time()));
    // BBS: undo-redo until modify record
    while (it_current != snapshots.end() && !snapshot_modifies_project(*it_current++));
    if (it_current != snapshots.end()) {
        while (it_current != snapshots.end() && !snapshot_modifies_project(*it_current++));
        this->undo_redo_to(--it_current);
    }
}

void Plater::priv::undo_redo_to(size_t time_to_load)
{
    const std::vector<UndoRedo::Snapshot> &snapshots = this->undo_redo_stack().snapshots();
    auto it_current = std::lower_bound(snapshots.begin(), snapshots.end(), UndoRedo::Snapshot(time_to_load));
    assert(it_current != snapshots.end());
    this->undo_redo_to(it_current);
}

// BBS: check need save or backup
bool Plater::priv::up_to_date(bool saved, bool backup)
{
    size_t& last_time = backup ? m_backup_timestamp : m_saved_timestamp;
    if (saved) {
        last_time = undo_redo_stack_main().active_snapshot_time();
        if (!backup)
            undo_redo_stack_main().mark_current_as_saved();
        return true;
    }
    else {
        return !undo_redo_stack_main().has_real_change_from(last_time);
    }
}

void Plater::priv::undo_redo_to(std::vector<UndoRedo::Snapshot>::const_iterator it_snapshot)
{
    // Make sure that no updating function calls take_snapshot until we are done.
    SuppressSnapshots snapshot_supressor(q);

    bool 				temp_snapshot_was_taken 	= this->undo_redo_stack().temp_snapshot_active();

    // Save the last active preset name of a particular printer technology.
     m_last_fff_printer_profile_name  = app_preset_bundle()->printers.get_selected_preset_name();
    //FIXME updating the Wipe tower config values at the ModelWipeTower from the Print config.
    // This is a workaround until we refactor the Wipe Tower position / orientation to live solely inside the Model, not in the Print config.
    // BBS: add partplate logic
    if (true) {
        const DynamicPrintConfig& config = app_preset_bundle()->prints.get_edited_preset().config;
        const DynamicPrintConfig& proj_cfg = app_preset_bundle()->project_config;
        const ConfigOptionFloats* tower_x_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_x");
        const ConfigOptionFloats* tower_y_opt = proj_cfg.option<ConfigOptionFloats>("wipe_tower_y");
        assert(tower_x_opt->values.size() == tower_y_opt->values.size());
        model.wipe_tower.positions.clear();
        model.wipe_tower.positions.resize(tower_x_opt->values.size());
        for (int plate_idx = 0; plate_idx < tower_x_opt->values.size(); plate_idx++) {
            ModelWipeTower& tower = model.wipe_tower;

            tower.positions[plate_idx] = Vec2d(tower_x_opt->get_at(plate_idx), tower_y_opt->get_at(plate_idx));
            tower.rotation = proj_cfg.opt_float("wipe_tower_rotation_angle");
        }
    }
    const int layer_range_idx = it_snapshot->snapshot_data.layer_range_idx;
    // Flags made of Snapshot::Flags enum values.
    unsigned int new_flags = it_snapshot->snapshot_data.flags;
    UndoRedo::SnapshotData top_snapshot_data;
    top_snapshot_data.printer_technology = ptFFF;
    if (this->view3D->is_layers_editing_enabled())
        top_snapshot_data.flags |= UndoRedo::SnapshotData::VARIABLE_LAYER_EDITING_ACTIVE;
    if (this->sidebar->obj_list()->is_selected(itSettings)) {
        top_snapshot_data.flags |= UndoRedo::SnapshotData::SELECTED_SETTINGS_ON_SIDEBAR;
        top_snapshot_data.layer_range_idx = this->sidebar->obj_list()->get_selected_layers_range_idx();
    }
    else if (this->sidebar->obj_list()->is_selected(itLayer)) {
        top_snapshot_data.flags |= UndoRedo::SnapshotData::SELECTED_LAYER_ON_SIDEBAR;
        top_snapshot_data.layer_range_idx = this->sidebar->obj_list()->get_selected_layers_range_idx();
    }
    else if (this->sidebar->obj_list()->is_selected(itLayerRoot))
        top_snapshot_data.flags |= UndoRedo::SnapshotData::SELECTED_LAYERROOT_ON_SIDEBAR;
    bool   		 new_variable_layer_editing_active = (new_flags & UndoRedo::SnapshotData::VARIABLE_LAYER_EDITING_ACTIVE) != 0;
    bool         new_selected_settings_on_sidebar  = (new_flags & UndoRedo::SnapshotData::SELECTED_SETTINGS_ON_SIDEBAR) != 0;
    bool         new_selected_layer_on_sidebar     = (new_flags & UndoRedo::SnapshotData::SELECTED_LAYER_ON_SIDEBAR) != 0;
    bool         new_selected_layerroot_on_sidebar = (new_flags & UndoRedo::SnapshotData::SELECTED_LAYERROOT_ON_SIDEBAR) != 0;

    if (m_gizmos->wants_reslice_supports_on_undo())
        top_snapshot_data.flags |= UndoRedo::SnapshotData::RECALCULATE_SLA_SUPPORTS;

    // Disable layer editing before the Undo / Redo jump.
    if (!new_variable_layer_editing_active && view3D->is_layers_editing_enabled())
        view3D->get_canvas3d()->force_main_toolbar_left_action(view3D->get_canvas3d()->get_main_toolbar_item_id("layersediting"));

    // Make a copy of the snapshot, undo/redo could invalidate the iterator
    const UndoRedo::Snapshot snapshot_copy = *it_snapshot;
    // Do the jump in time.
    if (it_snapshot->timestamp < this->undo_redo_stack().active_snapshot_time() ?
        this->undo_redo_stack().undo(model, m_selection, *m_gizmos, this->partplate_list, top_snapshot_data, it_snapshot->timestamp) :
        this->undo_redo_stack().redo(model, *m_gizmos, this->partplate_list, it_snapshot->timestamp)) {

        if (true) {
            const DynamicPrintConfig& config = app_preset_bundle()->prints.get_edited_preset().config;
            const DynamicPrintConfig& proj_cfg = app_preset_bundle()->project_config;
            ConfigOptionFloats* tower_x_opt = const_cast<ConfigOptionFloats*>(proj_cfg.option<ConfigOptionFloats>("wipe_tower_x"));
            ConfigOptionFloats* tower_y_opt = const_cast<ConfigOptionFloats*>(proj_cfg.option<ConfigOptionFloats>("wipe_tower_y"));
            // BBS: don't support wipe tower rotation
            //double current_rotation = proj_cfg.opt_float("wipe_tower_rotation_angle");
            bool need_update = false;
            if (tower_x_opt->values.size() != model.wipe_tower.positions.size()) {
                tower_x_opt->clear();
                ConfigOptionFloat default_tower_x(40.f);
                tower_x_opt->resize(model.wipe_tower.positions.size(), &default_tower_x);
                need_update = true;
            }

            if (tower_y_opt->values.size() != model.wipe_tower.positions.size()) {
                tower_y_opt->clear();
                ConfigOptionFloat default_tower_y(200.f);
                tower_y_opt->resize(model.wipe_tower.positions.size(), &default_tower_y);
                need_update = true;
            }

            for (int plate_idx = 0; plate_idx < model.wipe_tower.positions.size(); plate_idx++) {
                if (Vec2d(tower_x_opt->get_at(plate_idx), tower_y_opt->get_at(plate_idx)) != model.wipe_tower.positions[plate_idx]) {
                    ConfigOptionFloat tower_x_new(model.wipe_tower.positions[plate_idx].x());
                    ConfigOptionFloat tower_y_new(model.wipe_tower.positions[plate_idx].y());
                    tower_x_opt->set_at(&tower_x_new, plate_idx, 0);
                    tower_y_opt->set_at(&tower_y_new, plate_idx, 0);
                    need_update = true;
                    break;
                }
            }

            if (need_update) {
                // update print to current plate (preview->m_process)
                this->partplate_list.update_slice_context_to_current_plate(this->background_process);
                this->preview->update_gcode_result(this->partplate_list.get_current_slice_result_wrapper());
                this->update();
            }
        }
        // set selection mode for ObjectList on sidebar
        this->sidebar->obj_list()->set_selection_mode(new_selected_settings_on_sidebar  ? ObjectList::SELECTION_MODE::smSettings :
                                                      new_selected_layer_on_sidebar     ? ObjectList::SELECTION_MODE::smLayer :
                                                      new_selected_layerroot_on_sidebar ? ObjectList::SELECTION_MODE::smLayerRoot :
                                                                                          ObjectList::SELECTION_MODE::smUndef);
        if (new_selected_settings_on_sidebar || new_selected_layer_on_sidebar)
            this->sidebar->obj_list()->set_selected_layers_range_idx(layer_range_idx);

        this->update_after_undo_redo(snapshot_copy, temp_snapshot_was_taken);
        // Enable layer editing after the Undo / Redo jump.
        if (!view3D->is_layers_editing_enabled() && this->layers_height_allowed() && new_variable_layer_editing_active)
            view3D->get_canvas3d()->force_main_toolbar_left_action(view3D->get_canvas3d()->get_main_toolbar_item_id("layersediting"));
    }

    dirty_state.update_from_undo_redo_stack(m_undo_redo_stack_main.project_modified());
    update_title_dirty_status();
}

void Plater::priv::update_after_undo_redo(const UndoRedo::Snapshot& snapshot, bool /* temp_snapshot_was_taken */)
{
    m_selection.clear();
    
    // Update volumes from the deserializd model, always stop / update the background processing (for both the SLA and FFF technologies).
    this->update((unsigned int)UpdateParams::FORCE_BACKGROUND_PROCESSING_UPDATE);

    this->undo_redo_stack().release_least_recently_used();

    m_selection.set_deserialized(GUI::Selection::EMode(this->undo_redo_stack().selection_deserialized().mode), this->undo_redo_stack().selection_deserialized().volumes_and_instances);
    m_gizmos->update_after_undo_redo(snapshot);

    AppAdapter::obj_list()->update_after_undo_redo();

    if (app_get_mode() == comSimple && model_has_advanced_features(this->model)) {
        // If the user jumped to a snapshot that require user interface with advanced features, switch to the advanced mode without asking.
        // There is a little risk of surprising the user, as he already must have had the advanced or advanced mode active for such a snapshot to be taken.
        Slic3r::GUI::AppAdapter::gui_app()->save_mode(comAdvanced);
        view3D->set_as_dirty();
    }

    BOOST_LOG_TRIVIAL(info) << "Undo / Redo snapshot reloaded. Undo / Redo stack memory: " << Slic3r::format_memsize_MB(this->undo_redo_stack().memsize()) << log_memory_info();
}

void Plater::priv::bring_instance_forward() const
{
#ifdef __APPLE__
    other_instance_message_handler()->bring_instance_forward();
    return;
#endif //__APPLE__

    wxFrame* mainframe = AppAdapter::mainframe();

    if (mainframe == nullptr) {
        BOOST_LOG_TRIVIAL(debug) << "Couldnt bring instance forward - mainframe is null";
        return;
    }
    BOOST_LOG_TRIVIAL(debug) << "Orca Slicer window going forward";
    //this code maximize app window on Fedora
    {
        mainframe->Iconize(false);
        if (mainframe->IsMaximized())
            mainframe->Maximize(true);
        else
            mainframe->Maximize(false);
    }
    //this code maximize window on Ubuntu
    {
        mainframe->Restore();
        AppAdapter::app()->GetTopWindow()->SetFocus();  // focus on my window
        AppAdapter::app()->GetTopWindow()->Raise();  // bring window to front
        AppAdapter::app()->GetTopWindow()->Show(true); // show the window
    }
}

//BBS: popup object table
bool Plater::priv::PopupObjectTable(int object_id, int volume_id, const wxPoint& position)
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" enter, create ObjectTableDialog");
    int max_width{1920}, max_height{1080};

    max_width = q->GetMaxWidth();
    max_height = q->GetMaxHeight();
    ObjectTableDialog table_dialog(q, q, &model, wxSize(max_width, max_height));
    //m_popup_table = new ObjectTableDialog(q, q,  &model);

    wxRect rect = sidebar->GetRect();
    wxPoint pos = sidebar->ClientToScreen(wxPoint(rect.x, rect.y));

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": show ObjectTableDialog");
    table_dialog.Popup(object_id, volume_id, pos);

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" finished, will destroy ObjectTableDialog");
    return true;
}


void Plater::priv::record_start_print_preset(std::string action) 
{
}

bool PlaterDropTarget::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &filenames)
{
#ifdef WIN32
    // hides the system icon
    this->MSWUpdateDragImageOnLeave();
#endif // WIN32

    m_main_panel.Raise();
    
    // Check if dropping gcode files
    bool is_gcode = false;
    for (const auto& filename : filenames) {
        wxString ext = filename.AfterLast('.');
        ext.MakeLower();
        if (ext == "gcode" || ext == "g") {
            is_gcode = true;
            break;
        }
    }
    
    // Only switch to 3D Editor if not dropping gcode files
    if (!is_gcode) {
        m_main_panel.select_tab(size_t(MainPanel::tp3DEditor));
        m_plater.select_view3d();
    }

    // When only one .svg file is dropped on scene
    if (filenames.size() == 1) {
        const wxString &filename = filenames.Last();
        const wxString  file_extension = filename.substr(filename.length() - 4);
        if (file_extension.CmpNoCase(".svg") == 0) {
            // BBS: GUI refactor: move sidebar to the left
            const wxPoint offset  = m_plater.GetPosition() + m_plater.p->current_panel->GetPosition();
            Vec2d mouse_position(x - offset.x, y - offset.y);
            // Scale for retina displays
            const GLCanvas3D *canvas = m_plater.canvas3D();
            canvas->apply_retina_scale(mouse_position);
            return emboss_svg(m_plater, filename, mouse_position);
        }
    }
    bool res = m_plater.load_files(filenames);
    m_main_panel.update_title();
    return res;
}

}
}
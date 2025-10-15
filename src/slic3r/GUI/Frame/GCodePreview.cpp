#include "GCodePreview.hpp"

#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"

#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/GUI/Frame/OpenGLWindow.hpp"
#include "slic3r/Slice/BackgroundSlicingProcess.hpp"
#include "slic3r/Slice/GCodeResultWrapper.hpp"

#include "slic3r/Theme/AppColor.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Render/GCodeViewer.hpp"
#include "slic3r/Render/PlateBed.hpp"

#include "slic3r/Render/ImGUI/IMSlider.hpp"
#include "slic3r/Render/GCodePreviewCanvas.hpp"

namespace Slic3r { 
namespace GUI {

GCodePreview::GCodePreview(
    wxWindow* parent, Model* model, DynamicPrintConfig* config, 
    BackgroundSlicingProcess* process, PartPlateList* plate_list, GCodeResultWrapper* gcode_result, std::function<void()> schedule_background_process_func)
    : OpenGLPanel(parent)
    , m_config(config)
    , m_process(process)
    , m_gcode_result(gcode_result)
    , m_schedule_background_process(schedule_background_process_func)
{
    if (init(parent, model, plate_list))
        load_print();
}

void GCodePreview::update_gcode_result(GCodeResultWrapper* gcode_result)
{
    m_gcode_result = gcode_result;

    return;
}

bool GCodePreview::init(wxWindow* parent, Model* model, PartPlateList* plate_list)
{
    // to match the background of the sliders
#ifdef _WIN32
    UpdateDarkUI(this);
#else
    SetBackgroundColour(GetParent()->GetBackgroundColour());
#endif // _WIN32

    m_canvas = new GCodePreviewCanvas(m_opengl_window);
    m_canvas->set_context(raw_context());
    m_canvas->allow_multisample(can_multisample());
    m_canvas->set_config(m_config);
    m_canvas->set_model(model);
    m_canvas->set_process(m_process);
    m_canvas->set_plate_list(plate_list);
    m_canvas->set_type(GLCanvas3D::ECanvasType::CanvasPreview);
    m_canvas->enable_legend_texture(true);
    m_canvas->enable_dynamic_background(true);
    //BBS: GUI refactor: GLToolbar
    m_canvas->enable_select_plate_toolbar(true);

    m_opengl_window->Bind(wxEVT_KEY_DOWN, &GCodePreview::update_layers_slider_from_canvas, this);

    return true;
}

GCodePreview::~GCodePreview()
{
    if (m_canvas != nullptr)
        delete m_canvas;
}

wxGLCanvas* GCodePreview::get_wxglcanvas() 
{ 
    return raw_canvas(); 
}

GCodePreviewCanvas* GCodePreview::get_canvas3d() 
{ 
    return m_canvas; 
}

void GCodePreview::set_as_dirty()
{
    if (m_canvas != nullptr)
        m_canvas->set_as_dirty();
}

void GCodePreview::bed_shape_changed()
{
    if (m_canvas != nullptr)
        m_canvas->bed_shape_changed();
}

void GCodePreview::select_view(const std::string& direction)
{
    m_canvas->select_view(direction);
}

void GCodePreview::set_drop_target(wxDropTarget* target)
{
    if (target != nullptr)
        SetDropTarget(target);
}

//BBS: add only gcode mode
void GCodePreview::load_print(bool keep_z_range, bool only_gcode)
{
    load_print_as_fff(keep_z_range, only_gcode);
    Layout();
}

//BBS: add only gcode mode
void GCodePreview::reload_print(bool keep_volumes, bool only_gcode)
{
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(" %1%: enter, keep_volumes %2%")%__LINE__ %keep_volumes;
#ifdef __linux__
    // We are getting mysterious crashes on Linux in gtk due to OpenGL context activation GH #1874 #1955.
    // So we are applying a workaround here: a delayed release of OpenGL vertex buffers.
    if (!IsShown())
    {
        m_volumes_cleanup_required = !keep_volumes;
        return;
    }
#endif /* __linux__ */
    if (
#ifdef __linux__
        m_volumes_cleanup_required ||
#endif /* __linux__ */
        !keep_volumes)
    {
        m_canvas->reset_volumes();
        //BBS: add m_loaded_print logic
        //m_loaded = false;
        m_loaded_print = nullptr;
#ifdef __linux__
        m_volumes_cleanup_required = false;
#endif /* __linux__ */
    }

    load_print(false, only_gcode);
    m_only_gcode = only_gcode;
}

//BBS: add only gcode mode
void GCodePreview::refresh_print()
{
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(" %1%: enter, current m_loaded_print %2%")%__LINE__ %m_loaded_print;
    //BBS: add m_loaded_print logic
    //m_loaded = false;
    m_loaded_print = nullptr;

    if (!IsShown())
        return;

    load_print(true, m_only_gcode);
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(" %1%: exit")%__LINE__;
}

//BBS: always load shell at preview
void GCodePreview::load_shells(const Print& print, bool force_previewing)
{
    m_canvas->load_shells(print, force_previewing);
}

void GCodePreview::msw_rescale()
{
    // rescale warning legend on the canvas
    get_canvas3d()->msw_rescale();

    // rescale legend
    refresh_print();
}

void GCodePreview::sys_color_changed()
{
}

void GCodePreview::on_tick_changed()
{
    m_keep_current_preview_type = false;
    reload_print(false);
}

IMSlider* GCodePreview::get_layers_slider() 
{
    return m_canvas->get_gcode_viewer()->get_layers_slider();
}

void GCodePreview::show_sliders(bool show)
{
    show_moves_sliders(show);
    show_layers_sliders(show);
}

void GCodePreview::show_moves_sliders(bool show)
{
}

void GCodePreview::show_layers_sliders(bool show)
{
}

void GCodePreview::check_layers_slider_values(std::vector<CustomGCode::Item>& ticks_from_model, const std::vector<double>& layers_z)
{
    // All ticks that would end up outside the slider range should be erased.
    // TODO: this should be placed into more appropriate part of code,
    // this function is e.g. not called when the last object is deleted
    unsigned int old_size = ticks_from_model.size();
    ticks_from_model.erase(std::remove_if(ticks_from_model.begin(), ticks_from_model.end(),
                     [layers_z](CustomGCode::Item val)
        {
            auto it = std::lower_bound(layers_z.begin(), layers_z.end(), val.print_z - epsilon());
            return it == layers_z.end();
        }),
        ticks_from_model.end());
    if (ticks_from_model.size() != old_size)
        m_schedule_background_process();
}

// Find an index of a value in a sorted vector, which is in <z-eps, z+eps>.
// Returns -1 if there is no such member.
static int find_close_layer_idx(const std::vector<double> &zs, double &z, double eps)
{
    if (zs.empty()) return -1;
    auto it_h = std::lower_bound(zs.begin(), zs.end(), z);
    if (it_h == zs.end()) {
        auto it_l = it_h;
        --it_l;
        if (z - *it_l < eps) return int(zs.size() - 1);
    } else if (it_h == zs.begin()) {
        if (*it_h - z < eps) return 0;
    } else {
        auto it_l = it_h;
        --it_l;
        double dist_l = z - *it_l;
        double dist_h = *it_h - z;
        if (std::min(dist_l, dist_h) < eps) { return (dist_l < dist_h) ? int(it_l - zs.begin()) : int(it_h - zs.begin()); }
    }
    return -1;
}

void GCodePreview::update_layers_slider_mode()
{
    //    true  -> single-extruder printer profile OR
    //             multi-extruder printer profile , but whole model is printed by only one extruder
    //    false -> multi-extruder printer profile , and model is printed by several extruders
    bool one_extruder_printed_model = true;
    bool can_change_color = true;
    // extruder used for whole model for multi-extruder printer profile
    int only_extruder = -1;

    // BBS
    if (preset_filaments_cnt() > 1) {
        //const ModelObjectPtrs& objects = AppAdapter::plater()->model().objects;
        auto plate_extruders = AppAdapter::plater()->get_partplate_list().get_curr_plate()->get_extruders_without_support();
        for (auto extruder : plate_extruders) {
            if (extruder != plate_extruders[0])
                can_change_color = false;
        }
        // check if whole model uses just only one extruder
        if (!plate_extruders.empty()) {
            //const int extruder = objects[0]->config.has("extruder") ? objects[0]->config.option("extruder")->getInt() : 0;
            only_extruder = plate_extruders[0];
            //    auto is_one_extruder_printed_model = [objects, extruder]() {
            //        for (ModelObject *object : objects) {
            //            if (object->config.has("extruder") && object->config.option("extruder")->getInt() != extruder) /*return false*/;

            //            for (ModelVolume *volume : object->volumes)
            //                if ((volume->config.has("extruder") && volume->config.option("extruder")->getInt() != extruder) || !volume->mmu_segmentation_facets.empty()) return false;

            //            for (const auto &range : object->layer_config_ranges)
            //                if (range.second.has("extruder") && range.second.option("extruder")->getInt() != extruder) return false;
            //        }
            //        return true;
            //    };

            //    if (is_one_extruder_printed_model())
            //        only_extruder = extruder;
            //    else
            //        one_extruder_printed_model = false;
        }
    }

    IMSlider *m_layers_slider = m_canvas->get_gcode_viewer()->get_layers_slider();
    m_layers_slider->SetModeAndOnlyExtruder(one_extruder_printed_model, only_extruder, can_change_color);
}

void GCodePreview::update_layers_slider_from_canvas(wxKeyEvent &event)
{
    if (event.HasModifiers()) {
        event.Skip();
        return;
    }

    const auto key = event.GetKeyCode();

    IMSlider *m_layers_slider = m_canvas->get_gcode_viewer()->get_layers_slider();
    IMSlider *m_moves_slider  = m_canvas->get_gcode_viewer()->get_moves_slider();
    if (key == 'L') {
        if(!m_layers_slider->switch_one_layer_mode())
            event.Skip();
        m_canvas->set_as_dirty();
    }
    /*else if (key == WXK_SHIFT)
        m_layers_slider->UseDefaultColors(false);*/
    else
        event.Skip();
}

void GCodePreview::update_layers_slider(const std::vector<double>& layers_z, bool keep_z_range)
{
    IMSlider *m_layers_slider = m_canvas->get_gcode_viewer()->get_layers_slider();
    // Save the initial slider span.
    double z_low     = m_layers_slider->GetLowerValueD();
    double z_high    = m_layers_slider->GetHigherValueD();
    bool   was_empty = m_layers_slider->GetMaxValue() == 0;

    bool force_sliders_full_range = was_empty;
    if (!keep_z_range) {
        bool span_changed = layers_z.empty() || std::abs(layers_z.back() - m_layers_slider->GetMaxValueD()) > epsilon() /*1e-6*/;
        force_sliders_full_range |= span_changed;
    }
    bool snap_to_min = force_sliders_full_range || m_layers_slider->is_lower_at_min();
    bool snap_to_max = force_sliders_full_range || m_layers_slider->is_higher_at_max();

    // Detect and set manipulation mode for double slider
    update_layers_slider_mode();

    Plater* plater = AppAdapter::plater();
    //BBS: replace model custom gcode with current plate custom gcode
    CustomGCode::Info ticks_info_from_curr_plate = plater->model().get_curr_plate_custom_gcodes();
    check_layers_slider_values(ticks_info_from_curr_plate.gcodes, layers_z);

    // first of all update extruder colors to avoid crash, when we are switching printer preset from MM to SM
    m_layers_slider->SetExtruderColors(plater->get_extruder_colors_from_plater_config());
    m_layers_slider->SetSliderValues(layers_z);
    assert(m_layers_slider->GetMinValue() == 0);
    m_layers_slider->SetMaxValue(layers_z.empty() ? 0 : layers_z.size() - 1);

    int idx_low  = 0;
    int idx_high = m_layers_slider->GetMaxValue();
    if (!layers_z.empty()) {
        if (!snap_to_min) {
            int idx_new = find_close_layer_idx(layers_z, z_low, epsilon() /*1e-6*/);
            if (idx_new != -1) idx_low = idx_new;
        }
        if (!snap_to_max) {
            int idx_new = find_close_layer_idx(layers_z, z_high, epsilon() /*1e-6*/);
            if (idx_new != -1) idx_high = idx_new;
        }
    }
    m_layers_slider->SetSelectionSpan(idx_low, idx_high);

    auto curr_plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
    auto curr_print_seq = curr_plate->get_real_print_seq();
    bool sequential_print = (curr_print_seq == PrintSequence::ByObject);
    m_layers_slider->SetDrawMode(sequential_print);
    
    m_layers_slider->SetTicksValues(ticks_info_from_curr_plate);

    auto print_mode_stat = m_gcode_result->print_statistics().modes.front();
    m_layers_slider->SetLayersTimes(print_mode_stat.layers_times, print_mode_stat.time);
}

//BBS: add only gcode mode
void GCodePreview::load_print_as_fff(bool keep_z_range, bool only_gcode)
{
    if (AppAdapter::mainframe() == nullptr || AppAdapter::gui_app()->is_recreating_gui())
        // avoid processing while mainframe is being constructed
        return;

    //BBS: add m_loaded_print logic
    GCodeResultWrapper* gcode_result_wrapper = m_process->gcode_result_wrapper();
    const Print *print = m_process->fff_print();
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(" %1%: previous print %2%, new print %3%")%__LINE__ %m_loaded_print %print;
    if ((m_loaded_print&&(m_loaded_print == print))) {
        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(" %1%: already loaded before, return directly")%__LINE__;
        return;
    }

    // we require that there's at least one object and the posSlice step
    // is performed on all of them(this ensures that _shifted_copies was
    // populated and we know the number of layers)
    bool has_layers = false;
    //BBS: always load shell at preview
    load_shells(*print, true);
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(" %1%: print: %2%, gcode_result %3%, check started")%__LINE__ %print %m_gcode_result;
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: print is step done, posSlice %2%, posSupportMaterial %3%, psGCodeExport %4%") % __LINE__ % print->is_step_done(posSlice) %print->is_step_done(posSupportMaterial) % print->is_step_done(psGCodeExport);
    
    //BBS: support preview gcode directly even if no slicing
    bool directly_preview = false; 
    for (int i = 0, count = gcode_result_wrapper->size(); i < count; ++i)
    {
        Print* _print = gcode_result_wrapper->get_print(i);
        if (!has_layers)
        {
            if (_print->is_step_done(posSlice)) {
                for (const PrintObject* print_object : _print->objects())
                    if (!print_object->layers().empty()) {
                        has_layers = true;
                        break;
                    }
            }
            if (_print->is_step_done(posSupportMaterial)) {
                for (const PrintObject* print_object : _print->objects())
                    if (!print_object->support_layers().empty()) {
                        has_layers = true;
                        break;
                    }
            }
        }
        if (!directly_preview)
        {
            directly_preview = _print->is_step_done(psGCodeExport);
        }
        if (has_layers && directly_preview)
            break;
    }
    
    bool valid = m_gcode_result->is_valid();
    directly_preview = directly_preview && valid;
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": directly_preview: %1%, gcode_result moves %2%, has_layers %3%") % directly_preview % m_gcode_result->moves().size() % has_layers;
    if (!has_layers && !directly_preview) {
        show_sliders(false);
        render_update();
        return;
    }
    //BBS: for direct preview, don't keep z range
    else if (directly_preview && !has_layers)
        keep_z_range = false;

    EViewType gcode_view_type = (EViewType)m_canvas->get_gcode_view_preview_type();
    bool gcode_preview_data_valid = valid;

    // Collect colors per extruder.
    std::vector<std::string> colors;
    std::vector<CustomGCode::Item> color_print_values = {};
    // set color print values, if it si selected "ColorPrint" view type
    if (gcode_view_type == EViewType::ColorPrint) {
        colors = AppAdapter::plater()->get_colors_for_color_print();

        if (!gcode_preview_data_valid) {
            color_print_values = AppAdapter::plater()->model().get_curr_plate_custom_gcodes().gcodes;
            colors.push_back("#808080"); // gray color for pause print or custom G-code
        }
    }
    else if (gcode_preview_data_valid || gcode_view_type == EViewType::Tool) {
        colors = AppAdapter::plater()->get_extruder_colors_from_plater_config();
        color_print_values.clear();
    }

    std::vector<double> zs;

    if (IsShown()) {
        m_canvas->set_selected_extruder(0);
        bool is_slice_result_valid = AppAdapter::plater()->get_partplate_list().get_curr_plate()->is_slice_result_valid();
        if (gcode_preview_data_valid && (is_slice_result_valid || only_gcode)) {
            // Load the real G-code preview.
            //BBS: add more log
            BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": will load gcode_preview from result, moves count %1%") % m_gcode_result->moves().size();
            //BBS: add only gcode mode
            m_canvas->load_gcode_preview(m_gcode_result, colors, only_gcode);
            //BBS show sliders
            show_moves_sliders();

            //Orca: keep shell preview on but make it more transparent
            m_canvas->set_shells_on_previewing(true);
            m_canvas->set_shell_transparence();
            Refresh();
            zs = m_canvas->get_gcode_layers_zs();
            //BBS: add m_loaded_print logic
            //m_loaded = true;
            m_loaded_print = print;
        }
        else if (true) {
            // Load the initial preview based on slices, not the final G-code.
            //BBS: only display shell before slicing result out
            //m_canvas->load_preview(colors, color_print_values);
            show_moves_sliders(false);
            Refresh();
            //zs = m_canvas->get_volumes_print_zs(true);
        }

        if (!zs.empty() && !m_keep_current_preview_type) {
            unsigned int number_extruders = (unsigned int)print->extruders().size();
            std::vector<CustomGCode::Item> gcodes = AppAdapter::plater()->model().get_curr_plate_custom_gcodes().gcodes;
            const wxString choice = !gcodes.empty() ?
                _L("Multicolor Print") :
                (number_extruders > 1) ? _L("Filaments") : _L("Line Type");
        }

        if (zs.empty()) {
            // all layers filtered out
            //BBS
            show_layers_sliders(false);
            render_update();
        } else
            update_layers_slider(zs, keep_z_range);
    }
}

void GCodePreview::attach() 
{

    m_canvas->reset_select_plate_toolbar_selection();
    m_canvas->bind_event_handlers();
    m_canvas->render();
    Show();
    m_canvas->enable_select_plate_toolbar(true); // call after render for update toolbar
    m_canvas->reset_old_size();

}

void GCodePreview::detach() 
{
    m_canvas->enable_select_plate_toolbar(false);
    m_canvas->unbind_event_handlers();
    Hide();

}

void GCodePreview::render_impl()
{



}
}
}
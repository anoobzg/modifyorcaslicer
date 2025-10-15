#include "GCodePreviewCanvas.hpp"
#include "GCodeViewer.hpp"
#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Slice/BackgroundSlicingProcess.hpp"
#include "slic3r/GUI/Event/UserGLToolBarEvent.hpp"
#include "slic3r/GUI/Event/UserCanvasEvent.hpp"
#include "slic3r/Render/RenderUtils.hpp"
#include "slic3r/Slice/GCodeResultWrapper.hpp"
#include "slic3r/Render/ImGUI/IMSlider.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/Render/PlateBed.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Scene/NotificationManager.hpp"
#include "libslic3r/PresetBundle.hpp"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui/imgui_internal.h>
#include <imguizmo/ImGuizmo.h>
#include <wx/glcanvas.h>


namespace Slic3r {
namespace GUI {

GCodePreviewCanvas::GCodePreviewCanvas(wxGLCanvas* canvas)
    :GLCanvas3D(canvas)
{
    m_gcode_viewer = new GCodeViewer;
}

GCodePreviewCanvas::~GCodePreviewCanvas()
{

}

void GCodePreviewCanvas::set_process(BackgroundSlicingProcess* process)
{
    m_process = process;
}

void GCodePreviewCanvas::set_plate_list(PartPlateList* plate_list)
{
    m_plate_list = plate_list;
}

//BBS: GUI refactor: GLToolbar adjust
//when rendering, {0, 0} is at the center, {-0.5, 0.5} at the left-up
void GCodePreviewCanvas::_render_imgui_select_plate_toolbar()
{
   if (!m_sel_plate_toolbar.is_enabled()) {
       if (!m_render_preview)
           m_render_preview = true;
       return;
   }

   IMToolbarItem* all_plates_stats_item = m_sel_plate_toolbar.m_all_plates_stats_item;

   for (int i = 0; i < m_plate_list->get_plate_count(); i++) {
       if (i < m_sel_plate_toolbar.m_items.size()) {
           if (i == m_plate_list->get_curr_plate_index() && !all_plates_stats_item->selected)
               m_sel_plate_toolbar.m_items[i]->selected = true;
           else
               m_sel_plate_toolbar.m_items[i]->selected = false;

           m_sel_plate_toolbar.m_items[i]->percent = m_plate_list->get_plate(i)->get_slicing_percent();

           if (m_plate_list->get_plate(i)->is_slice_result_valid()) {
               if (m_plate_list->get_plate(i)->is_slice_result_ready_for_print())
                   m_sel_plate_toolbar.m_items[i]->slice_state = IMToolbarItem::SliceState::SLICED;
               else
                   m_sel_plate_toolbar.m_items[i]->slice_state = IMToolbarItem::SliceState::SLICE_FAILED;
           }
           else {
               if (!m_plate_list->get_plate(i)->can_slice())
                   m_sel_plate_toolbar.m_items[i]->slice_state = IMToolbarItem::SliceState::SLICE_FAILED;
               else {
                   if (m_plate_list->get_plate(i)->get_slicing_percent() < 0.0f)
                       m_sel_plate_toolbar.m_items[i]->slice_state = IMToolbarItem::SliceState::UNSLICED;
                   else
                       m_sel_plate_toolbar.m_items[i]->slice_state = IMToolbarItem::SliceState::SLICING;
               }
           }
       }
   }
   if (m_sel_plate_toolbar.show_stats_item) {
       all_plates_stats_item->percent = 0.0f;

       size_t sliced_plates_cnt = 0;
       for (auto plate : m_plate_list->get_nonempty_plate_list()) {
           if (plate->is_slice_result_valid() && plate->is_slice_result_ready_for_print())
               sliced_plates_cnt++;
       }
       all_plates_stats_item->percent = (float)(sliced_plates_cnt) / (float)(m_plate_list->get_nonempty_plate_list().size()) * 100.0f;

       if (all_plates_stats_item->percent == 0.0f)
           all_plates_stats_item->slice_state = IMToolbarItem::SliceState::UNSLICED;
       else if (sliced_plates_cnt == m_plate_list->get_nonempty_plate_list().size())
           all_plates_stats_item->slice_state = IMToolbarItem::SliceState::SLICED;
       else if (all_plates_stats_item->percent < 100.0f)
           all_plates_stats_item->slice_state = IMToolbarItem::SliceState::SLICING;

       for (auto toolbar_item : m_sel_plate_toolbar.m_items) {
           if(toolbar_item->slice_state == IMToolbarItem::SliceState::SLICE_FAILED) {
               all_plates_stats_item->slice_state = IMToolbarItem::SliceState::SLICE_FAILED;
               all_plates_stats_item->selected = false;
               break;
           }
       }

       // Changing parameters does not invalid all plates, need extra logic to validate
       bool gcode_result_valid = true;
       for (auto gcode_result : m_plate_list->get_nonempty_plates_slice_results()) {
           if (gcode_result->moves.size() == 0) {
               gcode_result_valid = false;
           }
       }
       if (all_plates_stats_item->selected && all_plates_stats_item->slice_state == IMToolbarItem::SliceState::SLICED && gcode_result_valid) {
           m_gcode_viewer->render_all_plates_stats(m_plate_list->get_nonempty_plates_slice_results());
           m_render_preview = false;
       }
       else{
           m_gcode_viewer->render_all_plates_stats(m_plate_list->get_nonempty_plates_slice_results(), false);
           m_render_preview = true;
       }
   }else
       m_render_preview = true;

   // places the toolbar on the top_left corner of the 3d scene
#if ENABLE_RETINA_GL
   float f_scale  = m_retina_helper->get_scale_factor();
#else
   float f_scale  = 1.0;
#endif
   Size cnv_size = get_canvas_size();
   auto canvas_w = float(cnv_size.get_width());
   auto canvas_h = float(cnv_size.get_height());

   bool is_hovered = false;

   m_sel_plate_toolbar.set_icon_size(100.0f * f_scale, 100.0f * f_scale);

   float button_width = m_sel_plate_toolbar.icon_width;
   float button_height = m_sel_plate_toolbar.icon_height;

   float frame_padding = 1.0f * f_scale;
   float margin_size = 4.0f * f_scale;
   float button_margin = frame_padding;

   const float y_offset = is_collapse_toolbar_on_left() ? (get_collapse_toolbar_height() + 5) : 0;
   // Make sure the window does not overlap the 3d navigator
   auto window_height_max = canvas_h - y_offset;
   if (show_3d_navigator()) {
       float sc = get_scale();
#ifdef WIN32
       const int dpi = get_dpi_for_window(AppAdapter::app()->GetTopWindow());
       sc *= (float) dpi / (float) DPI_DEFAULT;
#endif // WIN32
       window_height_max -= (128 * sc + 5);
   }

   ImGuiWrapper& imgui = global_im_gui();
   int item_count = m_sel_plate_toolbar.m_items.size() + (m_sel_plate_toolbar.show_stats_item ? 1 : 0);
   bool show_scroll = item_count * (button_height + frame_padding * 2.0f + button_margin) - button_margin + 22.0f * f_scale > window_height_max ? true: false;
   show_scroll = m_sel_plate_toolbar.is_display_scrollbar && show_scroll;
   float window_height = std::min(item_count * (button_height + (frame_padding + margin_size) * 2.0f + button_margin) - button_margin + 28.0f * f_scale, window_height_max);
   float window_width = m_sel_plate_toolbar.icon_width + margin_size * 2 + (show_scroll ? 28.0f * f_scale : 20.0f * f_scale);

   ImVec4 window_bg = ImVec4(0.82f, 0.82f, 0.82f, 0.5f);
   ImVec4 button_active = ImGuiWrapper::COL_ORCA; // ORCA: Use orca color for selected sliced plate border 
   ImVec4 button_hover = ImVec4(0.67f, 0.67f, 0.67, 1.0f);
   ImVec4 scroll_col = ImVec4(0.77f, 0.77f, 0.77f, 1.0f);
   //ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 1.0f));
   //use white text as the background switch to black
   ImGui::PushStyleColor(ImGuiCol_Text, m_is_dark ? ImVec4(.9f, .9f, .9f, 1) : ImVec4(.3f, .3f, .3f, 1)); // ORCA Plate number text > Add support for dark mode
   ImGui::PushStyleColor(ImGuiCol_WindowBg, window_bg);
   ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.f, 0.f, 0.f, 0.f)); // ORCA using background color with opacity creates a second color. This prevents secondary color 
   ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, scroll_col);
   ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, scroll_col);
   ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, scroll_col);
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_active);
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_hover);

   ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

   imgui.set_next_window_pos(canvas_w * 0, canvas_h * 0 + y_offset, ImGuiCond_Always, 0, 0);
   imgui.set_next_window_size(window_width, window_height, ImGuiCond_Always);

   if (show_scroll)
       imgui.begin(_L("Select Plate"), ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
   else
       imgui.begin(_L("Select Plate"), ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
   ImGui::SetWindowFontScale(1.2f);

   ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f * f_scale);

   ImVec2 size = ImVec2(button_width, button_height); // Size of the image we want to make visible
   ImVec4 bg_col = ImVec4(128.0f, 128.0f, 128.0f, 0.0f);
   ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);               // No tint
   ImVec2 margin = ImVec2(margin_size, margin_size);

   if(m_sel_plate_toolbar.show_stats_item)
   {
       // draw image
       ImVec2 button_start_pos = ImGui::GetCursorScreenPos();

       if (all_plates_stats_item->selected) {
           ImGui::PushStyleColor(ImGuiCol_Button, button_active);
           ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_active);
           ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_active);
       }
       else {
           ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(128.0f, 128.0f, 128.0f, 0.0f));
           if (all_plates_stats_item->slice_state == IMToolbarItem::SliceState::SLICE_FAILED) {
               ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_Button));
               ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_Button));
           }
           else {
               ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_hover);
               ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_hover);
           }
       }

       ImVec4 text_clr;
       ImTextureID btn_texture_id;
       if (all_plates_stats_item->slice_state == IMToolbarItem::SliceState::UNSLICED || all_plates_stats_item->slice_state == IMToolbarItem::SliceState::SLICING || all_plates_stats_item->slice_state == IMToolbarItem::SliceState::SLICE_FAILED)
       {
           text_clr       = ImVec4(0.0f, 150.f / 255.0f, 136.0f / 255, 0.2f); // ORCA: All plates slicing NOT complete - Text color
           btn_texture_id = (ImTextureID)(intptr_t)(all_plates_stats_item->image_texture_transparent.get_id());
       }
       else
       {
           text_clr       = ImGuiWrapper::COL_ORCA; // ORCA: All plates slicing complete - Text color
           btn_texture_id = (ImTextureID)(intptr_t)(all_plates_stats_item->image_texture.get_id());
       }

       if (ImGui::ImageButton2(btn_texture_id, size, {0,0}, {1,1}, frame_padding, bg_col, tint_col, margin)) {
           if (all_plates_stats_item->slice_state != IMToolbarItem::SliceState::SLICE_FAILED) {
               if (m_process && !m_process->running()) {
                   for (int i = 0; i < m_sel_plate_toolbar.m_items.size(); i++) {
                       m_sel_plate_toolbar.m_items[i]->selected = false;
                   }
                   all_plates_stats_item->selected = true;
                   AppAdapter::plater()->update(true, true);
                   wxCommandEvent evt = wxCommandEvent(EVT_GLTOOLBAR_SLICE_ALL);
                   wxPostEvent(AppAdapter::plater(), evt);
               }
           }
       }

       ImGui::PopStyleColor(3);

       ImVec2 start_pos = ImVec2(button_start_pos.x + frame_padding + margin.x, button_start_pos.y + frame_padding + margin.y);
       if (all_plates_stats_item->slice_state == IMToolbarItem::SliceState::UNSLICED) {
           ImVec2 size = ImVec2(button_width, button_height);
           ImVec2 end_pos = ImVec2(start_pos.x + size.x, start_pos.y + size.y);
           ImGui::GetWindowDrawList()->AddRectFilled(start_pos, end_pos, IM_COL32(0, 0, 0, 80));
       }
       else if (all_plates_stats_item->slice_state == IMToolbarItem::SliceState::SLICING) {
           ImVec2 size = ImVec2(button_width, button_height * all_plates_stats_item->percent / 100.0f);
           ImVec2 rect_start_pos = ImVec2(start_pos.x, start_pos.y + size.y);
           ImVec2 rect_end_pos = ImVec2(start_pos.x + button_width, start_pos.y + button_height);
           ImGui::GetWindowDrawList()->AddRectFilled(start_pos, rect_end_pos, IM_COL32(0, 0, 0, 10));
           ImGui::GetWindowDrawList()->AddRectFilled(rect_start_pos, rect_end_pos, IM_COL32(0, 0, 0, 80));
       }
       else if (all_plates_stats_item->slice_state == IMToolbarItem::SliceState::SLICE_FAILED) {
           ImVec2 size = ImVec2(button_width, button_height);
           ImVec2 end_pos = ImVec2(start_pos.x + size.x, start_pos.y + size.y);
           ImGui::GetWindowDrawList()->AddRectFilled(start_pos, end_pos, IM_COL32(40, 1, 1, 64));
           ImGui::GetWindowDrawList()->AddRect(start_pos, end_pos, IM_COL32(208, 27, 27, 255), 0.0f, 0, 1.0f);
       }
       else if (all_plates_stats_item->slice_state == IMToolbarItem::SliceState::SLICED) {
           ImVec2 size = ImVec2(button_width, button_height);
           ImVec2 end_pos = ImVec2(start_pos.x + size.x, start_pos.y + size.y);
           ImGui::GetWindowDrawList()->AddRectFilled(start_pos, end_pos, IM_COL32(0, 0, 0, 10));
       }

       // draw text
       GImGui->FontSize = 15.0f;
       ImGui::PushStyleColor(ImGuiCol_Text, text_clr);
       ImVec2 text_size = ImGui::CalcTextSize(("All Plates"));
       ImVec2 text_start_pos = ImVec2(start_pos.x + (button_width - text_size.x) / 2, start_pos.y + 3.0f * button_height / 5.0f);
       ImGui::RenderText(text_start_pos, ("All Plates"));
       text_size = ImGui::CalcTextSize(("Stats"));
       text_start_pos = ImVec2(start_pos.x + (button_width - text_size.x) / 2, text_start_pos.y + ImGui::GetTextLineHeight());
       ImGui::RenderText(text_start_pos, ("Stats"));
       ImGui::PopStyleColor();
       ImGui::SetWindowFontScale(1.2f);
   }

   for (int i = 0; i < m_sel_plate_toolbar.m_items.size(); i++) {
       IMToolbarItem* item = m_sel_plate_toolbar.m_items[i];

       // draw image
       ImVec2 button_start_pos = ImGui::GetCursorScreenPos();
       ImGui::PushID(i);
       ImVec2 uv0 = ImVec2(0.0f, 1.0f);    // UV coordinates for lower-left
       ImVec2 uv1 = ImVec2(1.0f, 0.0f);    // UV coordinates in our texture

       auto button_pos = ImGui::GetCursorPos();
       ImGui::SetCursorPos(button_pos + margin);

       ImGui::Image(item->texture_id, size, uv0, uv1, tint_col);

       ImGui::SetCursorPos(button_pos);

       // invisible button
       auto button_size = size + margin + margin + ImVec2(2 * frame_padding, 2 * frame_padding);
       ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f * f_scale);
       ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.0f, .0f, .0f, .0f));
       ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.0f, .0f, .0f, .0f));
       ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.0f, .0f, .0f, .0f));
       if (item->selected) {
           ImGui::PushStyleColor(ImGuiCol_Border, button_active);
       }
       else {
           // Translate window pos to abs pos, also account for the window scrolling
           auto hover_rect = button_pos + ImGui::GetWindowPos() - ImGui::GetCurrentWindow()->Scroll;
           if (ImGui::IsMouseHoveringRect(hover_rect, hover_rect + button_size)) {
               ImGui::PushStyleColor(ImGuiCol_Border, button_hover);
           }
           else {
               ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.0f, .0f, .0f, .0f));
           }
       }
       if(ImGui::Button("##invisible_button", button_size)){
           if (m_process && !m_process->running()) {
               all_plates_stats_item->selected = false;
               item->selected = true;
               // begin to slicing plate
               if (item->slice_state != IMToolbarItem::SliceState::SLICED)
                   AppAdapter::plater()->update(true, true);
               wxCommandEvent* evt = new wxCommandEvent(EVT_GLTOOLBAR_SELECT_SLICED_PLATE);
               evt->SetInt(i);
               wxQueueEvent(AppAdapter::plater(), evt);
           }
       }
       ImGui::PopStyleColor(4);
       ImGui::PopStyleVar();

       ImVec2 start_pos = ImVec2(button_start_pos.x + frame_padding + margin.x, button_start_pos.y + frame_padding + margin.y);
       if (item->slice_state == IMToolbarItem::SliceState::UNSLICED) {
           ImVec2 size = ImVec2(button_width, button_height);
           ImVec2 end_pos = ImVec2(start_pos.x + size.x, start_pos.y + size.y);
           ImGui::GetWindowDrawList()->AddRectFilled(start_pos, end_pos, IM_COL32(0, 0, 0, 80));
       } else if (item->slice_state == IMToolbarItem::SliceState::SLICING) {
           ImVec2 size = ImVec2(button_width, button_height * item->percent / 100.0f);
           ImVec2 rect_start_pos = ImVec2(start_pos.x, start_pos.y + size.y);
           ImVec2 rect_end_pos = ImVec2(start_pos.x + button_width, start_pos.y + button_height);
           ImGui::GetWindowDrawList()->AddRectFilled(start_pos, rect_end_pos, IM_COL32(0, 0, 0, 10));
           ImGui::GetWindowDrawList()->AddRectFilled(rect_start_pos, rect_end_pos, IM_COL32(0, 0, 0, 80));
       } else if (item->slice_state == IMToolbarItem::SliceState::SLICE_FAILED) {
           ImVec2 size    = ImVec2(button_width, button_height);
           ImVec2 end_pos = ImVec2(start_pos.x + size.x, start_pos.y + size.y);
           ImGui::GetWindowDrawList()->AddRectFilled(start_pos, end_pos, IM_COL32(40, 1, 1, 64));
           ImGui::GetWindowDrawList()->AddRect(start_pos, end_pos, IM_COL32(208, 27, 27, 255), 0.0f, 0, 1.0f);
       } else if (item->slice_state == IMToolbarItem::SliceState::SLICED) {
           ImVec2 size = ImVec2(button_width, button_height);
           ImVec2 end_pos = ImVec2(start_pos.x + size.x, start_pos.y + size.y);
           ImGui::GetWindowDrawList()->AddRectFilled(start_pos, end_pos, IM_COL32(0, 0, 0, 10));
       }

       // draw text
       ImVec2 text_start_pos = ImVec2(start_pos.x + 4.0f, start_pos.y + 2.0f); // ORCA move close to corner to prevent overlapping with preview
       ImGui::RenderText(text_start_pos, std::to_string(i + 1).c_str());

       ImGui::PopID();
   }
   ImGui::SetWindowFontScale(1.0f);
   ImGui::PopStyleColor(8);
   ImGui::PopStyleVar(5);

   if (ImGui::IsWindowHovered() || is_hovered) {
       m_sel_plate_toolbar.is_display_scrollbar = true;
   } else {
       m_sel_plate_toolbar.is_display_scrollbar = false;
   }

   imgui.end();
   m_sel_plate_toolbar.is_render_finish = true;
}


void GCodePreviewCanvas::_set_warning_notification_if_needed(EWarning warning)
{
    _set_current();
    bool show = false;
    // if (!m_volumes.empty()) {
    //    show = _is_any_volume_outside();
    //    show &= m_gcode_viewer->has_data() && m_gcode_viewer->is_contained_in_bed() && m_gcode_viewer->m_conflict_result.has_value();
    // } else {
       unsigned int max_z_layer = m_gcode_viewer->get_layers_z_range().back();
       if (warning == EWarning::ToolHeightOutside)  // check if max z_layer height exceed max print height
           show = m_gcode_viewer->has_data() && (m_gcode_viewer->get_layers_zs()[max_z_layer] - m_gcode_viewer->get_max_print_height() >= 1e-6);
       else if (warning == EWarning::ToolpathOutside) { // check if max x,y coords exceed bed area
           show = m_gcode_viewer->has_data() && !m_gcode_viewer->is_contained_in_bed() &&
                   (m_gcode_viewer->get_max_print_height() -m_gcode_viewer->get_layers_zs()[max_z_layer] >= 1e-6);
       }
       else if (warning == EWarning::GCodeConflict)
           show = m_gcode_viewer->has_data() && m_gcode_viewer->is_contained_in_bed() && m_gcode_viewer->m_conflict_result.has_value();
    // }

    _set_warning_notification(warning, show);
}

void GCodePreviewCanvas::_set_warning_notification(EWarning warning, bool state)
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
    case EWarning::GCodeConflict: {
       static std::string prevConflictText;
       text  = prevConflictText;
       error = ErrorType::SLICING_SERIOUS_WARNING;
       if (!m_gcode_viewer->m_conflict_result) { break; }
       std::string objName1 = m_gcode_viewer->m_conflict_result.value()._objName1;
       std::string objName2 = m_gcode_viewer->m_conflict_result.value()._objName2;
       double      height   = m_gcode_viewer->m_conflict_result.value()._height;
       int         layer    = m_gcode_viewer->m_conflict_result.value().layer;
       text = (boost::format(_u8L("Conflicts of gcode paths have been found at layer %d, z = %.2lf mm. Please separate the conflicted objects farther (%s <-> %s).")) % layer %
               height % objName1 % objName2)
                  .str();
       prevConflictText        = text;
       const PrintObject *obj2 = reinterpret_cast<const PrintObject *>(m_gcode_viewer->m_conflict_result.value()._obj2);
       conflictObj             = obj2->model_object();
       break;
    }
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


/* gcode_viewer */
void GCodePreviewCanvas::set_gcode_view_preview_type(int type) 
{ 
    return m_gcode_viewer->set_view_type((EViewType)type);
}

int GCodePreviewCanvas::get_gcode_view_preview_type() const 
{ 
    return (int)m_gcode_viewer->get_view_type(); 
}

void GCodePreviewCanvas::set_shells_on_previewing(bool is_preview) 
{ 
    m_gcode_viewer->set_shells_on_preview(is_preview); 
}

bool GCodePreviewCanvas::is_gcode_legend_enabled() const 
{ 
    return m_gcode_viewer->is_legend_enabled(); 
}

int GCodePreviewCanvas::get_gcode_view_type() const 
{ 
    return (int)m_gcode_viewer->get_view_type(); 
}

std::vector<CustomGCode::Item>& GCodePreviewCanvas::get_custom_gcode_per_print_z() 
{ 
    return m_gcode_viewer->get_custom_gcode_per_print_z(); 
}

const std::vector<double>& GCodePreviewCanvas::get_gcode_layers_zs() const
{
    return m_gcode_viewer->get_layers_zs();
}

size_t GCodePreviewCanvas::get_gcode_extruders_count() 
{ 
    return m_gcode_viewer->get_extruders_count(); 
}

GCodeViewer* GCodePreviewCanvas::get_gcode_viewer() 
{
     return m_gcode_viewer; 
}

void GCodePreviewCanvas::init_gcode_viewer(ConfigOptionMode mode, Slic3r::PresetBundle* preset_bundle) 
{ 
    m_gcode_viewer->init(mode, preset_bundle); 
}

void GCodePreviewCanvas::reset_gcode_toolpaths() 
{ 
    m_gcode_viewer->reset(); 
}

void GCodePreviewCanvas::update_gcode_sequential_view_current(unsigned int first, unsigned int last) 
{ 
    m_gcode_viewer->update_sequential_view_current(first, last); 
}

void GCodePreviewCanvas::load_gcode_preview(GCodeResultWrapper* gcode_result, const std::vector<std::string>& str_tool_colors, bool only_gcode)
{
    if (!gcode_result || !gcode_result->is_valid())
        return;

    PartPlateList& partplate_list = AppAdapter::plater()->get_partplate_list();
    PartPlate* plate = partplate_list.get_curr_plate();
    const std::vector<BoundingBoxf3>& exclude_bounding_box = plate->get_exclude_areas();
     
    
    //BBS: init is called in GLCanvas3D.render()
    //when load gcode directly, it is too late
    m_gcode_viewer->init((ConfigOptionMode)app_get_mode(), app_preset_bundle());
    
    auto& gcode_result_list = gcode_result->get_const_all_result();
    m_gcode_viewer->load(gcode_result_list, *this->fff_print(), AppAdapter::plater()->build_volume(), PlateBed::sub_build_volume(),  exclude_bounding_box,
        (ConfigOptionMode)app_get_mode(), only_gcode);

    m_gcode_viewer->get_moves_slider()->SetHigherValue(m_gcode_viewer->get_moves_slider()->GetMaxValue());

    //BBS: always load shell at preview, do this in load_shells
    //m_gcode_viewer->update_shells_color_by_extruder(m_config);
    _set_warning_notification_if_needed(EWarning::ToolHeightOutside);
    _set_warning_notification_if_needed(EWarning::ToolpathOutside);
    _set_warning_notification_if_needed(EWarning::GCodeConflict);

    m_gcode_viewer->refresh(*(gcode_result->get_result()), str_tool_colors);
    set_as_dirty();
    request_extra_frame();
}

void GCodePreviewCanvas::refresh_gcode_preview_render_paths()
{
    m_gcode_viewer->refresh_render_paths();
    set_as_dirty();
    request_extra_frame();
}


void GCodePreviewCanvas::load_shells(const Print& print, bool force_previewing)
{
    if (m_initialized)
    {
        m_gcode_viewer->load_shells(print, m_initialized, force_previewing);
        m_gcode_viewer->update_shells_color_by_extruder(m_config);
    }
}

void GCodePreviewCanvas::set_shell_transparence(float alpha){
    m_gcode_viewer->set_shell_transparency(alpha);

}

void GCodePreviewCanvas::render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params)
{
    //load current plate gcode
    m_gcode_viewer->render_calibration_thumbnail(thumbnail_data, w, h, thumbnail_params,
        AppAdapter::plater()->get_partplate_list());
}

bool GCodePreviewCanvas::has_toolpaths_to_export() const
{
    return m_gcode_viewer->can_export_toolpaths();
}

void GCodePreviewCanvas::export_toolpaths_to_obj(const char* filename) const
{
    m_gcode_viewer->export_toolpaths_to_obj(filename);
}

void GCodePreviewCanvas::key_handle(wxKeyEvent& evt)
{
    const int keyCode = evt.GetKeyCode();
    IMSlider *m_layers_slider = get_gcode_viewer()->get_layers_slider();
    IMSlider *m_moves_slider  = get_gcode_viewer()->get_moves_slider();
    int increment = (evt.CmdDown() || evt.ShiftDown()) ? 5 : 1;
    if ((evt.CmdDown() || evt.ShiftDown()) && evt.GetKeyCode() == 'G') {
        m_layers_slider->show_go_to_layer(true);
    }
    else if (keyCode == WXK_UP || keyCode == WXK_DOWN) {
        int new_pos;
        if (m_layers_slider->GetSelection() == ssHigher) {
            new_pos = keyCode == WXK_UP ? m_layers_slider->GetHigherValue() + increment : m_layers_slider->GetHigherValue() - increment;
            m_layers_slider->SetHigherValue(new_pos);
            m_moves_slider->SetHigherValue(m_moves_slider->GetMaxValue());
        }
        else if (m_layers_slider->GetSelection() == ssLower) {
            new_pos = keyCode == WXK_UP ? m_layers_slider->GetLowerValue() + increment : m_layers_slider->GetLowerValue() - increment;
            m_layers_slider->SetLowerValue(new_pos);
        }
    } else if (keyCode == WXK_LEFT) {
        if (m_moves_slider->GetHigherValue() == m_moves_slider->GetMinValue() && (m_layers_slider->GetHigherValue() > m_layers_slider->GetMinValue())) {
            m_layers_slider->SetHigherValue(m_layers_slider->GetHigherValue() - 1);
            m_moves_slider->SetHigherValue(m_moves_slider->GetMaxValue());
        } else {
            m_moves_slider->SetHigherValue(m_moves_slider->GetHigherValue() - increment);
        }
    } else if (keyCode == WXK_RIGHT) {
        if (m_moves_slider->GetHigherValue() == m_moves_slider->GetMaxValue() && (m_layers_slider->GetHigherValue() < m_layers_slider->GetMaxValue())) {
            m_layers_slider->SetHigherValue(m_layers_slider->GetHigherValue() + 1);
            m_moves_slider->SetHigherValue(m_moves_slider->GetMinValue());
        } else {
            m_moves_slider->SetHigherValue(m_moves_slider->GetHigherValue() + increment);
        }
    } else if (keyCode == WXK_HOME || keyCode == WXK_END) {
        const int new_pos = keyCode == WXK_HOME ? m_moves_slider->GetMinValue() : m_moves_slider->GetMaxValue();
        m_moves_slider->SetHigherValue(new_pos);
        m_moves_slider->set_as_dirty();
    }

    if (m_layers_slider->is_dirty() && m_layers_slider->is_one_layer())
        m_layers_slider->SetLowerValue(m_layers_slider->GetHigherValue());

    m_dirty = true;
}

//BBS: GUI refactor: add canvas size as parameters
void GCodePreviewCanvas::_render_gcode(int canvas_width, int canvas_height)
{
    // if (!m_main_toolbar.is_enabled())
    m_gcode_viewer->init((ConfigOptionMode)app_get_mode(), app_preset_bundle());
    
    m_gcode_viewer->add_slider_fresh_callback([&](IMSlider *layers_slider, IMSlider *moves_slider)
    {
        if (layers_slider->is_need_post_tick_event())
        {
            auto evt = new wxCommandEvent(EVT_CUSTOMEVT_TICKSCHANGED, m_canvas->GetId());
            evt->SetInt((int)layers_slider->get_post_tick_event_type());
            wxPostEvent(m_canvas, *evt);
            layers_slider->reset_post_tick_event();
        }

        if (layers_slider->is_dirty())
        {
            set_volumes_z_range({layers_slider->GetLowerValueD(), layers_slider->GetHigherValueD()});
            // if (m_gcode_viewer->has_data())
            // {
            //     m_gcode_viewer->set_layers_z_range({static_cast<unsigned int>(layers_slider->GetLowerValue()), static_cast<unsigned int>(layers_slider->GetHigherValue())});
            // }
            layers_slider->set_as_dirty(false);
            post_event(SimpleEvent(EVT_GLCANVAS_UPDATE));
            //m_gcode_viewer->update_marker_curr_move();
        }

        if (moves_slider->is_dirty())
        {
            moves_slider->set_as_dirty(false);
            ////m_gcode_viewer->update_sequential_view_current((moves_slider->GetLowerValueD() - 1.0), static_cast<unsigned int>(moves_slider->GetHigherValueD() - 1.0));
            post_event(SimpleEvent(EVT_GLCANVAS_UPDATE));
            //m_gcode_viewer->update_marker_curr_move();
        }
      });

    m_gcode_viewer->render(canvas_width, canvas_height, SLIDER_RIGHT_MARGIN * GCODE_VIEWER_SLIDER_SCALE);
    m_gcode_viewer->refresh();
    // IMSlider *layers_slider = m_gcode_viewer->get_layers_slider();
    // IMSlider *moves_slider  = m_gcode_viewer->get_moves_slider();

    // if (layers_slider->is_need_post_tick_event()) {
    //     auto evt = new wxCommandEvent(EVT_CUSTOMEVT_TICKSCHANGED, m_canvas->GetId());
    //     evt->SetInt((int)layers_slider->get_post_tick_event_type());
    //     wxPostEvent(m_canvas, *evt);
    //     layers_slider->reset_post_tick_event();
    // }

    // if (layers_slider->is_dirty()) {
    //     set_volumes_z_range({layers_slider->GetLowerValueD(), layers_slider->GetHigherValueD()});
    //     if (m_gcode_viewer->has_data()) {
    //         m_gcode_viewer->set_layers_z_range({static_cast<unsigned int>(layers_slider->GetLowerValue()), static_cast<unsigned int>(layers_slider->GetHigherValue())});
    //     }
    //     layers_slider->set_as_dirty(false);
    //     post_event(SimpleEvent(EVT_GLCANVAS_UPDATE));
    //     m_gcode_viewer->update_marker_curr_move();
    // }

    // if (moves_slider->is_dirty()) {
    //     moves_slider->set_as_dirty(false);
    //     m_gcode_viewer->update_sequential_view_current((moves_slider->GetLowerValueD() - 1.0), static_cast<unsigned int>(moves_slider->GetHigherValueD() - 1.0));
    //     post_event(SimpleEvent(EVT_GLCANVAS_UPDATE));
    //     m_gcode_viewer->update_marker_curr_move();
    // }

}

void GCodePreviewCanvas::enable_legend_texture(bool enable)
{
    m_gcode_viewer->enable_legend(enable);
}

void GCodePreviewCanvas::zoom_to_gcode()
{
    _zoom_to_box(m_gcode_viewer->get_paths_bounding_box(), 1.05);
}

// const Print* GCodePreviewCanvas::fff_print() const
// {
//     return (m_process == nullptr) ? nullptr : m_process->fff_print();
// }

void GCodePreviewCanvas::_on_mouse(wxMouseEvent& evt)
{
    if (!m_initialized || !_set_current())
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

    // for (GLVolume* volume : m_volumes.volumes) {
    //     volume->force_sinking_contours = false;
    // }

    if (m_mouse.drag.move_requires_threshold && m_mouse.is_move_start_threshold_position_2D_defined() && m_mouse.is_move_threshold_met(pos)) {
        m_mouse.drag.move_requires_threshold = false;
        m_mouse.set_move_start_threshold_position_2D_as_invalid();
    }

    if (evt.ButtonDown() && wxWindow::FindFocus() != m_canvas)
        // Grab keyboard focus on any mouse click event.
        m_canvas->SetFocus();

    m_event_manager.dispatchEvent(evt, this);
}

void GCodePreviewCanvas::reload_scene(bool refresh_immediately, bool force_full_scene_refresh) 
{
    if (m_canvas == nullptr || m_config == nullptr || m_model == nullptr)
        return;

    if (!m_initialized)
        return;
    
    _set_current();

    m_hover_volume_idxs.clear();

    std::vector<size_t> instance_ids_selected;
    std::vector<size_t> map_glvolume_old_to_new(m_volumes.volumes.size(), size_t(-1));
  
    std::vector<GLVolume*> glvolumes_new;
    glvolumes_new.reserve(m_volumes.volumes.size());

    m_reload_delayed = !m_canvas->IsShown() && !refresh_immediately && !force_full_scene_refresh;

    // BBS: support wipe tower for multi-plates
    PartPlateList& ppl = AppAdapter::plater()->get_partplate_list();
    int n_plates = ppl.get_plate_count();

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


    if (m_reload_delayed)
        return;

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
            }
        }
    }

    _set_warning_notification_if_needed(EWarning::GCodeConflict);
   
    refresh_camera_scene_box();

    m_dirty = true;
}

void GCodePreviewCanvas::render(bool only_init) 
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
    bool only_current = false, show_axes = true;

    _render_bed(camera.get_view_matrix(), camera.get_projection_matrix(), !camera.is_looking_downward(), show_axes);

    int hover_id = (m_hover_plate_idxs.size() > 0) ? m_hover_plate_idxs.front() : -1;
    _render_platelist(camera.get_view_matrix(), camera.get_projection_matrix(), !camera.is_looking_downward(), only_current, true, hover_id);
    // BBS: GUI refactor: add canvas size as parameters
    _render_gcode(cnv_size.get_width(), cnv_size.get_height());


    // we need to set the mouse's scene position here because the depth buffer
    // could be invalidated by the following gizmo render methods
    // this position is used later into on_mouse() to drag the objects
    if (m_picking_enabled)
        m_mouse.scene_position = _mouse_to_3d(m_mouse.position.cast<coord_t>());


    glsafe(::glDisable(GL_DEPTH_TEST));
    _render_collapse_toolbar();
    _render_imgui_select_plate_toolbar();
    _render_3d_navigator();


#if ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW
    if (AppAdapter::plater()->is_view3D_shown())
        AppAdapter::plater()->render_project_state_debug_window();
#endif // ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW

#if ENABLE_CAMERA_STATISTICS
    camera.debug_render();
#endif // ENABLE_CAMERA_STATISTICS

    //AppAdapter::plater()->get_mouse3d_controller().render_settings_dialog(*this);

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
    //AppAdapter::plater()->get_dailytips()->render();  

    global_im_gui().render();

    m_canvas->SwapBuffers();
    m_render_stats.increment_fps_counter();
}

}
}
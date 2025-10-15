#include "GCodePanel.hpp"
#include "GCodeDefine.hpp"
#include "GCodeViewerData.hpp"
#include "GCodeViewInstance.hpp"

#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"
#include "slic3r/Render/ImGUI/IMSlider.hpp"
#include "slic3r/Render/GLCanvas3D.hpp"
#include "slic3r/Config/AppPreset.hpp"
#include "libslic3r/AppConfig.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include <imgui/imgui_internal.h>
#include "slic3r/GUI/Gizmos/GizmoObjectManipulation.hpp"

namespace Slic3r {
namespace GUI {
namespace GCode {

GCodePanel::GCodePanel()
{
    m_moves_slider  = new IMSlider(0, 0, 0, 100, wxSL_HORIZONTAL);
    m_layers_slider = new IMSlider(0, 0, 0, 100, wxSL_VERTICAL);
}

GCodePanel::~GCodePanel()
{
    delete m_moves_slider;
    delete m_layers_slider;
}

void GCodePanel::set_refresh_func(std::function<void(bool)>& func)
{
    m_refresh_func = func;
}

void GCodePanel::set_select_part_func(std::function<void(int)>& func)
{
    m_select_part_func = func;
}

void GCodePanel::set_select_mode_func(std::function<void(int)>& func)
{
    m_select_mode_func = func;
}

void GCodePanel::set_schedule_background_process(std::function<void()>& func)
{
    m_schedule_background_process = func;
}

void GCodePanel::set_instance(std::shared_ptr<GCodeViewInstance> instance)
{
    m_data = instance->data();
}

void GCodePanel::render(float &legend_height, int canvas_width, int canvas_height, int right_margin)
{
    if (!m_data)
        return;

    render_legend(legend_height, canvas_width, canvas_height, right_margin);
    render_sliders(canvas_width, canvas_height);
}

void GCodePanel::render_all_plates_stats(const std::vector<const GCodeProcessorResult*>& gcode_result_list, bool show /*= true*/) 
{
    if (!show)
        return;

    if (!m_data)
        return;
        
    for (auto gcode_result : gcode_result_list) {
        if (gcode_result->moves.size() == 0)
            return;
    }
    ImGuiWrapper& imgui = global_im_gui();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 10.0 * m_scale));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.00f, 0.68f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.00f, 0.68f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.42f, 0.42f, 0.42f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.93f, 0.93f, 0.93f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.93f, 0.93f, 0.93f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(340.f * m_scale * imgui.scaled(1.0f / 15.0f), 0));

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), 0, ImVec2(0.5f, 0.5f));
    ImGui::Begin(_L("Statistics of All Plates").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    std::vector<float> filament_diameters = gcode_result_list.front()->filament_diameters;
    std::vector<float> filament_densities = gcode_result_list.front()->filament_densities;
    std::vector<ColorRGBA> filament_colors;
    decode_colors(AppAdapter::plater()->get_extruder_colors_from_plater_config(), filament_colors);

    for (int i = 0; i < filament_colors.size(); i++) { 
        filament_colors[i] = adjust_color_for_rendering(filament_colors[i]);
    }

    bool imperial_units = AppAdapter::app_config()->get("use_inches") == "1";
    float window_padding = 4.0f * m_scale;
    const float icon_size = ImGui::GetTextLineHeight() * 0.7;
    std::map<std::string, float> offsets;
    std::map<int, double> model_volume_of_extruders_all_plates; // map<extruder_idx, volume>
    std::map<int, double> flushed_volume_of_extruders_all_plates; // map<extruder_idx, flushed volume>
    std::map<int, double> wipe_tower_volume_of_extruders_all_plates; // map<extruder_idx, flushed volume>
    std::map<int, double> support_volume_of_extruders_all_plates; // map<extruder_idx, flushed volume>
    std::vector<double> model_used_filaments_m_all_plates;
    std::vector<double> model_used_filaments_g_all_plates;
    std::vector<double> flushed_filaments_m_all_plates;
    std::vector<double> flushed_filaments_g_all_plates;
    std::vector<double> wipe_tower_used_filaments_m_all_plates;
    std::vector<double> wipe_tower_used_filaments_g_all_plates;
    std::vector<double> support_used_filaments_m_all_plates;
    std::vector<double> support_used_filaments_g_all_plates;
    float total_time_all_plates = 0.0f;
    float total_cost_all_plates = 0.0f;
    bool show_detailed_statistics_page = false;
    struct ColumnData {
        enum {
            Model = 1,
            Flushed = 2,
            WipeTower = 4,
            Support = 1 << 3,
        };
    };
    int displayed_columns = 0;
    auto max_width = [](const std::vector<std::string>& items, const std::string& title, float extra_size = 0.0f) {
        float ret = ImGui::CalcTextSize(title.c_str()).x;
        for (const std::string& item : items) {
            ret = std::max(ret, extra_size + ImGui::CalcTextSize(item.c_str()).x);
        }
        return ret;
    };
    auto calculate_offsets = [max_width, window_padding](const std::vector<std::pair<std::string, std::vector<std::string>>>& title_columns, float extra_size = 0.0f) {
        const ImGuiStyle& style = ImGui::GetStyle();
        std::vector<float> offsets;
        offsets.push_back(max_width(title_columns[0].second, title_columns[0].first, extra_size) + 3.0f * style.ItemSpacing.x + style.WindowPadding.x);
        for (size_t i = 1; i < title_columns.size() - 1; i++)
            offsets.push_back(offsets.back() + max_width(title_columns[i].second, title_columns[i].first) + style.ItemSpacing.x);
        if (title_columns.back().first == _u8L("Display"))
            offsets.back() = ImGui::GetWindowWidth() - ImGui::CalcTextSize(_u8L("Display").c_str()).x - ImGui::GetFrameHeight() / 2 - 2 * window_padding;

        float average_col_width = ImGui::GetWindowWidth() / static_cast<float>(title_columns.size());
        std::vector<float> ret;
        ret.push_back(0);
        for (size_t i = 1; i < title_columns.size(); i++) {
            ret.push_back(std::max(offsets[i - 1], i * average_col_width));
        }

        return ret;
    };
    auto append_item = [icon_size, &imgui, imperial_units, &window_padding, &draw_list, this](const ColorRGBA& color, const std::vector<std::pair<std::string, float>>& columns_offsets)
    {
        // render icon
        ImVec2 pos = ImVec2(ImGui::GetCursorScreenPos().x + window_padding * 3, ImGui::GetCursorScreenPos().y);

        draw_list->AddRectFilled({ pos.x + 1.0f * m_scale, pos.y + 3.0f * m_scale }, { pos.x + icon_size - 1.0f * m_scale, pos.y + icon_size + 1.0f * m_scale },
            ImGuiWrapper::to_ImU32(color));

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20.0 * m_scale, 6.0 * m_scale));

        // render selectable
        ImGui::Dummy({ 0.0, 0.0 });
        ImGui::SameLine();

        // render column item
        {
            float dummy_size = ImGui::GetStyle().ItemSpacing.x + icon_size;
            ImGui::SameLine(dummy_size);
            imgui.text(columns_offsets[0].first);

            for (auto i = 1; i < columns_offsets.size(); i++) {
                ImGui::SameLine(columns_offsets[i].second);
                imgui.text(columns_offsets[i].first);
            }
        }

        ImGui::PopStyleVar(1);
    };
    auto append_headers = [&imgui](const std::vector<std::pair<std::string, float>>& title_offsets) {
        for (size_t i = 0; i < title_offsets.size(); i++) {
            ImGui::SameLine(title_offsets[i].second);
            imgui.bold_text(title_offsets[i].first);
        }
        ImGui::Separator();
    };
    auto get_used_filament_from_volume = [this, imperial_units, &filament_diameters, &filament_densities](double volume, int extruder_id) {
        double koef = imperial_units ? 1.0 / GizmoObjectManipulation::in_to_mm : 0.001;
        std::pair<double, double> ret = { koef * volume / (PI * sqr(0.5 * filament_diameters[extruder_id])),
                                            volume * filament_densities[extruder_id] * 0.001 };
        return ret;
    };

    ImGui::Dummy({ window_padding, window_padding });
    ImGui::SameLine();
    //// title and item data
    //{
    //    PartPlateList& plate_list = AppAdapter::plater()->get_partplate_list();
    //    for (auto plate : plate_list.get_nonempty_plate_list())
    //    {
    //        auto plate_print_statistics = plate->get_slice_result()->print_statistics;
    //        auto plate_extruders = plate->get_extruders(true);
    //        for (size_t extruder_id : plate_extruders) {
    //            extruder_id -= 1;
    //            if (plate_print_statistics.model_volumes_per_extruder.find(extruder_id) == plate_print_statistics.model_volumes_per_extruder.end())
    //                model_volume_of_extruders_all_plates[extruder_id] += 0;
    //            else {
    //                double model_volume = plate_print_statistics.model_volumes_per_extruder.at(extruder_id);
    //                model_volume_of_extruders_all_plates[extruder_id] += model_volume;
    //            }
    //            if (plate_print_statistics.flush_per_filament.find(extruder_id) == plate_print_statistics.flush_per_filament.end())
    //                flushed_volume_of_extruders_all_plates[extruder_id] += 0;
    //            else {
    //                double flushed_volume = plate_print_statistics.flush_per_filament.at(extruder_id);
    //                flushed_volume_of_extruders_all_plates[extruder_id] += flushed_volume;
    //            }
    //            if (plate_print_statistics.wipe_tower_volumes_per_extruder.find(extruder_id) == plate_print_statistics.wipe_tower_volumes_per_extruder.end())
    //                wipe_tower_volume_of_extruders_all_plates[extruder_id] += 0;
    //            else {
    //                double wipe_tower_volume = plate_print_statistics.wipe_tower_volumes_per_extruder.at(extruder_id);
    //                wipe_tower_volume_of_extruders_all_plates[extruder_id] += wipe_tower_volume;
    //            }
    //            if (plate_print_statistics.support_volumes_per_extruder.find(extruder_id) == plate_print_statistics.support_volumes_per_extruder.end())
    //                support_volume_of_extruders_all_plates[extruder_id] += 0;
    //            else {
    //                double support_volume = plate_print_statistics.support_volumes_per_extruder.at(extruder_id);
    //                support_volume_of_extruders_all_plates[extruder_id] += support_volume;
    //            }
    //        }
    //        const PrintEstimatedStatistics::Mode& plate_time_mode = plate_print_statistics.modes[static_cast<size_t>(m_data->m_time_estimate_mode)];
    //        total_time_all_plates += plate_time_mode.time;
    //        
    //        Print     *print;
    //        plate->get_print((PrintBase **) &print, nullptr, nullptr);
    //        total_cost_all_plates += print->print_statistics().total_cost;
    //    }
    //   
    //    for (auto it = model_volume_of_extruders_all_plates.begin(); it != model_volume_of_extruders_all_plates.end(); it++) {
    //        auto [model_used_filament_m, model_used_filament_g] = get_used_filament_from_volume(it->second, it->first);
    //        if (model_used_filament_m != 0.0 || model_used_filament_g != 0.0)
    //            displayed_columns |= ColumnData::Model;
    //        model_used_filaments_m_all_plates.push_back(model_used_filament_m);
    //        model_used_filaments_g_all_plates.push_back(model_used_filament_g);
    //    }
    //    for (auto it = flushed_volume_of_extruders_all_plates.begin(); it != flushed_volume_of_extruders_all_plates.end(); it++) {
    //        auto [flushed_filament_m, flushed_filament_g] = get_used_filament_from_volume(it->second, it->first);
    //        if (flushed_filament_m != 0.0 || flushed_filament_g != 0.0)
    //            displayed_columns |= ColumnData::Flushed;
    //        flushed_filaments_m_all_plates.push_back(flushed_filament_m);
    //        flushed_filaments_g_all_plates.push_back(flushed_filament_g);
    //    }
    //    for (auto it = wipe_tower_volume_of_extruders_all_plates.begin(); it != wipe_tower_volume_of_extruders_all_plates.end(); it++) {
    //        auto [wipe_tower_filament_m, wipe_tower_filament_g] = get_used_filament_from_volume(it->second, it->first);
    //        if (wipe_tower_filament_m != 0.0 || wipe_tower_filament_g != 0.0)
    //            displayed_columns |= ColumnData::WipeTower;
    //        wipe_tower_used_filaments_m_all_plates.push_back(wipe_tower_filament_m);
    //        wipe_tower_used_filaments_g_all_plates.push_back(wipe_tower_filament_g);
    //    }
    //    for (auto it = support_volume_of_extruders_all_plates.begin(); it != support_volume_of_extruders_all_plates.end(); it++) {
    //        auto [support_filament_m, support_filament_g] = get_used_filament_from_volume(it->second, it->first);
    //        if (support_filament_m != 0.0 || support_filament_g != 0.0)
    //            displayed_columns |= ColumnData::Support;
    //        support_used_filaments_m_all_plates.push_back(support_filament_m);
    //        support_used_filaments_g_all_plates.push_back(support_filament_g);
    //    }

    //    char buff[64];
    //    double longest_str = 0.0;
    //    for (auto i : model_used_filaments_g_all_plates) {
    //        if (i > longest_str)
    //            longest_str = i;
    //    }
    //    ::sprintf(buff, "%.2f", longest_str);

    //    std::vector<std::pair<std::string, std::vector<std::string>>> title_columns;
    //    if (displayed_columns & ColumnData::Model) {
    //        title_columns.push_back({ _u8L("Filament"), {""} });
    //        title_columns.push_back({ _u8L("Model"), {buff} });
    //    }
    //    if (displayed_columns & ColumnData::Support) {
    //        title_columns.push_back({ _u8L("Support"), {buff} });
    //    }
    //    if (displayed_columns & ColumnData::Flushed) {
    //        title_columns.push_back({ _u8L("Flushed"), {buff} });
    //    }
    //    if (displayed_columns & ColumnData::WipeTower) {
    //        title_columns.push_back({ _u8L("Tower"), {buff} });
    //    }
    //    if ((displayed_columns & ~ColumnData::Model) > 0) {
    //        title_columns.push_back({ _u8L("Total"), {buff} });
    //    }
    //    auto offsets_ = calculate_offsets(title_columns, icon_size);
    //    std::vector<std::pair<std::string, float>> title_offsets;
    //    for (int i = 0; i < offsets_.size(); i++) {
    //        title_offsets.push_back({ title_columns[i].first, offsets_[i] });
    //        offsets[title_columns[i].first] = offsets_[i];
    //    }
    //    append_headers(title_offsets);
    //}

    // item
    {
        size_t i = 0;
        for (auto it = model_volume_of_extruders_all_plates.begin(); it != model_volume_of_extruders_all_plates.end(); it++) {
            if (i < model_used_filaments_m_all_plates.size() && i < model_used_filaments_g_all_plates.size()) {
                std::vector<std::pair<std::string, float>> columns_offsets;
                columns_offsets.push_back({ std::to_string(it->first + 1), offsets[_u8L("Filament")]});

                char buf[64];
                double unit_conver = imperial_units ? GizmoObjectManipulation::oz_to_g : 1.0;

                float column_sum_m = 0.0f;
                float column_sum_g = 0.0f;
                if (displayed_columns & ColumnData::Model) {
                    if ((displayed_columns & ~ColumnData::Model) > 0)
                        ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", model_used_filaments_m_all_plates[i], model_used_filaments_g_all_plates[i] / unit_conver);
                    else
                        ::sprintf(buf, imperial_units ? "%.2f in    %.2f oz" : "%.2f m    %.2f g", model_used_filaments_m_all_plates[i], model_used_filaments_g_all_plates[i] / unit_conver);
                    columns_offsets.push_back({ buf, offsets[_u8L("Model")] });
                    column_sum_m += model_used_filaments_m_all_plates[i];
                    column_sum_g += model_used_filaments_g_all_plates[i];
                }
                if (displayed_columns & ColumnData::Support) {
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", support_used_filaments_m_all_plates[i], support_used_filaments_g_all_plates[i] / unit_conver);
                    columns_offsets.push_back({ buf, offsets[_u8L("Support")] });
                    column_sum_m += support_used_filaments_m_all_plates[i];
                    column_sum_g += support_used_filaments_g_all_plates[i];
                }
                if (displayed_columns & ColumnData::Flushed) {
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", flushed_filaments_m_all_plates[i], flushed_filaments_g_all_plates[i] / unit_conver);
                    columns_offsets.push_back({ buf, offsets[_u8L("Flushed")] });
                    column_sum_m += flushed_filaments_m_all_plates[i];
                    column_sum_g += flushed_filaments_g_all_plates[i];
                }
                if (displayed_columns & ColumnData::WipeTower) {
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", wipe_tower_used_filaments_m_all_plates[i], wipe_tower_used_filaments_g_all_plates[i] / unit_conver);
                    columns_offsets.push_back({ buf, offsets[_u8L("Tower")] });
                    column_sum_m += wipe_tower_used_filaments_m_all_plates[i];
                    column_sum_g += wipe_tower_used_filaments_g_all_plates[i];
                }
                if ((displayed_columns & ~ColumnData::Model) > 0) {
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", column_sum_m, column_sum_g / unit_conver);
                    columns_offsets.push_back({ buf, offsets[_u8L("Total")] });
                }

                append_item(filament_colors[it->first], columns_offsets);
            }
            i++;
        }

        ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * 0.1));
        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        imgui.title(_u8L("Total Estimation"));

        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        imgui.text(_u8L("Total time") + ":");
        ImGui::SameLine();
        imgui.text(short_time(get_time_dhms(total_time_all_plates)));

        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        imgui.text(_u8L("Total cost") + ":");
        ImGui::SameLine();
        char buf[64];
        ::sprintf(buf, "%.2f", total_cost_all_plates);
        imgui.text(buf);
    }
    ImGui::End();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);
    return;
}

void GCodePanel::render_gcode_window(float top, float bottom, float right) const
{
    if (!m_data || !m_data->has_data())
        return;

    uint64_t curr_line_id = m_data->m_sequential_view.gcode_ids[m_data->m_sequential_view.current.last];
    GCodeFile* file = &m_data->m_sequential_view.file;
    // Orca: truncate long lines(>55 characters), add "..." at the end
    auto update_lines = [this, file](uint64_t start_id, uint64_t end_id) {
        std::vector<Line> ret;
        ret.reserve(end_id - start_id + 1);
        for (uint64_t id = start_id; id <= end_id; ++id) {
            // read line from file
            const size_t start        = id == 1 ? 0 : file->m_lines_ends[id - 2];
            const size_t original_len = file->m_lines_ends[id - 1] - start;
            const size_t len          = std::min(original_len, (size_t) 55);
            std::string  gline(file->m_file.data() + start, len);

            // If original line is longer than 55 characters, truncate and append "..."
            if (original_len > 55)
                gline = gline.substr(0, 52) + "...";

            std::string command, parameters, comment;
            // extract comment
            std::vector<std::string> tokens;
            boost::split(tokens, gline, boost::is_any_of(";"), boost::token_compress_on);
            command = tokens.front();
            if (tokens.size() > 1)
                comment = ";" + tokens.back();

            // extract gcode command and parameters
            if (!command.empty()) {
                boost::split(tokens, command, boost::is_any_of(" "), boost::token_compress_on);
                command = tokens.front();
                if (tokens.size() > 1) {
                    for (size_t i = 1; i < tokens.size(); ++i) {
                        parameters += " " + tokens[i];
                    }
                }
            }
            ret.push_back({command, parameters, comment});
        }
        return ret;
    };

    static const ImVec4 LINE_NUMBER_COLOR    = ImGuiWrapper::COL_ORANGE_LIGHT;
    static const ImVec4 SELECTION_RECT_COLOR = ImGuiWrapper::COL_ORANGE_DARK;
    static const ImVec4 COMMAND_COLOR        = {0.8f, 0.8f, 0.0f, 1.0f};
    static const ImVec4 PARAMETERS_COLOR     = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const ImVec4 COMMENT_COLOR        = { 0.7f, 0.7f, 0.7f, 1.0f };

    //if (!show_gcode_window() || file.m_filename.empty() || file.m_lines_ends.empty() || curr_line_id == 0)
    if (file->m_filename.empty() || file->m_lines_ends.empty() || curr_line_id == 0)
        return;

    // window height
    const float wnd_height = bottom - top;

    // number of visible lines
    const float text_height = ImGui::CalcTextSize("0").y;
    const ImGuiStyle& style = ImGui::GetStyle();
    const uint64_t lines_count = static_cast<uint64_t>((wnd_height - 2.0f * style.WindowPadding.y + style.ItemSpacing.y) / (text_height + style.ItemSpacing.y));

    if (lines_count == 0)
        return;

    // visible range
    const uint64_t half_lines_count = lines_count / 2;
    uint64_t start_id = (curr_line_id >= half_lines_count) ? curr_line_id - half_lines_count : 0;
    uint64_t end_id = start_id + lines_count - 1;
    if (end_id >= static_cast<uint64_t>(file->m_lines_ends.size())) {
        end_id = static_cast<uint64_t>(file->m_lines_ends.size()) - 1;
        start_id = end_id - lines_count + 1;
    }

    // updates list of lines to show, if needed
    if (file->m_selected_line_id != curr_line_id || file->m_last_lines_size != end_id - start_id + 1) {
        try
        {
            *const_cast<std::vector<Line>*>(&file->m_lines) = update_lines(start_id, end_id);
        }
        catch (...)
        {
            BOOST_LOG_TRIVIAL(error) << "Error while loading from file " << file->m_filename << ". Cannot show G-code window.";
            return;
        }
        *const_cast<uint64_t*>(&file->m_selected_line_id) = curr_line_id;
        *const_cast<size_t*>(&file->m_last_lines_size) = file->m_lines.size();
    }

    // line number's column width
    const float id_width = ImGui::CalcTextSize(std::to_string(end_id).c_str()).x;

    ImGuiWrapper& imgui = global_im_gui();

    //BBS: GUI refactor: move to right
    //imgui.set_next_window_pos(0.0f, top, ImGuiCond_Always, 0.0f, 0.0f);
    imgui.set_next_window_pos(right, top, ImGuiCond_Always, 1.0f, 0.0f);
    imgui.set_next_window_size(0.0f, wnd_height, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::SetNextWindowBgAlpha(0.8f);
    imgui.begin(std::string("G-code"), ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    // center the text in the window by pushing down the first line
    const float f_lines_count = static_cast<float>(lines_count);
    ImGui::SetCursorPosY(0.5f * (wnd_height - f_lines_count * text_height - (f_lines_count - 1.0f) * style.ItemSpacing.y));

    // render text lines
    for (uint64_t id = start_id; id <= end_id; ++id) {
        const Line& line = file->m_lines[id - start_id];

        // rect around the current selected line
        if (id == curr_line_id) {
            //BBS: GUI refactor: move to right
            const float pos_y = ImGui::GetCursorScreenPos().y;
            const float pos_x = ImGui::GetCursorScreenPos().x;
            const float half_ItemSpacing_y = 0.5f * style.ItemSpacing.y;
            const float half_ItemSpacing_x = 0.5f * style.ItemSpacing.x;
            //ImGui::GetWindowDrawList()->AddRect({ half_padding_x, pos_y - half_ItemSpacing_y },
            //    { ImGui::GetCurrentWindow()->Size.x - half_padding_x, pos_y + text_height + half_ItemSpacing_y },
            //    ImGui::GetColorU32(SELECTION_RECT_COLOR));
            ImGui::GetWindowDrawList()->AddRect({ pos_x - half_ItemSpacing_x, pos_y - half_ItemSpacing_y },
                { right - half_ItemSpacing_x, pos_y + text_height + half_ItemSpacing_y },
                ImGui::GetColorU32(SELECTION_RECT_COLOR));
        }

        // render line number
        const std::string id_str = std::to_string(id);
        // spacer to right align text
        ImGui::Dummy({ id_width - ImGui::CalcTextSize(id_str.c_str()).x, text_height });
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, LINE_NUMBER_COLOR);
        imgui.text(id_str);
        ImGui::PopStyleColor();

        if (!line.command.empty() || !line.comment.empty())
            ImGui::SameLine();

        // render command
        if (!line.command.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, COMMAND_COLOR);
            imgui.text(line.command);
            ImGui::PopStyleColor();
        }

        // render parameters
        if (!line.parameters.empty()) {
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, PARAMETERS_COLOR);
            imgui.text(line.parameters);
            ImGui::PopStyleColor();
        }

        // render comment
        if (!line.comment.empty()) {
            if (!line.command.empty())
                ImGui::SameLine(0.0f, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, COMMENT_COLOR);
            imgui.text(line.comment);
            ImGui::PopStyleColor();
        }
    }

    imgui.end();
    ImGui::PopStyleVar();
}

void GCodePanel::render_group_window(float top, float bottom, float right, int count)
{
    ImGui::SetNextWindowPos(ImVec2(right - 180, top)); 
    ImGui::SetNextWindowSize(ImVec2(180, bottom - top));
    
    ImGui::Begin("分区窗口", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    
    ImGui::Text("分区");
    
    float buttonWidth = (ImGui::GetWindowWidth() - (count + 1) * ImGui::GetStyle().ItemSpacing.x) / count;
    
    for (int i = 0; i < count; ++i) {
        // 设置按钮宽度
        ImGui::PushItemWidth(buttonWidth);
        
        // 创建按钮，标签为数字
        char buttonLabel[16];
        sprintf(buttonLabel, "%d", i + 1);
        if (ImGui::Button(buttonLabel)) {
            if (m_select_part_func)
                m_select_part_func(i);
        }
        
        ImGui::PopItemWidth();
    
        if (i != count - 1) {
            ImGui::SameLine();
        }
    }
    
    ImGui::Spacing(); // 增加间距

    ImGui::End();
}

IMSlider* GCodePanel::get_moves_slider() 
{ 
    return m_moves_slider; 
}

IMSlider* GCodePanel::get_layers_slider() 
{ 
    return m_layers_slider; 
}

void GCodePanel::check_layers_slider_values(std::vector<CustomGCode::Item>& ticks_from_model, const std::vector<double>& layers_z)
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
        if (m_schedule_background_process)
            m_schedule_background_process();
}

bool GCodePanel::switch_one_layer_mode()
{
    return m_layers_slider->switch_one_layer_mode();
}

void GCodePanel::_update_layers_slider_mode()
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
            only_extruder = plate_extruders[0];
        }
    }

    m_layers_slider->SetModeAndOnlyExtruder(one_extruder_printed_model, only_extruder, can_change_color);
}

void GCodePanel::update_layers_slider(const std::vector<double>& layers_z, bool keep_z_range)
{
    if (!m_data)
        return;

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
    _update_layers_slider_mode();

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

    auto print_mode_stat = m_data->m_print_statistics.modes.front();
    m_layers_slider->SetLayersTimes(print_mode_stat.layers_times, print_mode_stat.time);
}

void GCodePanel::render_legend(float &legend_height, int canvas_width, int canvas_height, int right_margin)
{
    if (!m_legend_enabled)
        return;

    const Size cnv_size = AppAdapter::plater()->get_current_canvas3D()->get_canvas_size();

    ImGuiWrapper& imgui = global_im_gui();

    //BBS: GUI refactor: move to the right
    imgui.set_next_window_pos(float(canvas_width - right_margin * m_scale), 0.0f, ImGuiCond_Always, 1.0f, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0,0.0));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f,1.0f,1.0f,0.6f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.00f, 0.59f, 0.53f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.00f, 0.59f, 0.53f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.42f, 0.42f, 0.42f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.93f, 0.93f, 0.93f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.93f, 0.93f, 0.93f, 1.00f));
    ImGui::SetNextWindowBgAlpha(0.8f);
    const float max_height = 0.75f * static_cast<float>(cnv_size.get_height());
    const float child_height = 0.3333f * max_height;
    ImGui::SetNextWindowSizeConstraints({ 0.0f, 0.0f }, { -1.0f, max_height });
    imgui.begin(std::string("Legend"), ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    enum class EItemType : unsigned char
    {
        Rect,
        Circle,
        Hexagon,
        Line,
        None
    };

    const PrintEstimatedStatistics::Mode& time_mode = m_data->m_print_statistics.modes[static_cast<size_t>(m_data->m_time_estimate_mode)];
    //BBS
    /*bool show_estimated_time = time_mode.time > 0.0f && (m_data->m_view_type == EViewType::FeatureType ||
        (m_data->m_view_type == EViewType::ColorPrint && !time_mode.custom_gcode_times.empty()));*/
    bool show_estimated = time_mode.time > 0.0f && (m_data->m_view_type == EViewType::FeatureType || m_data->m_view_type == EViewType::ColorPrint);

    const float icon_size = ImGui::GetTextLineHeight() * 0.7;
    //BBS GUI refactor
    //const float percent_bar_size = 2.0f * ImGui::GetTextLineHeight();
    const float percent_bar_size = 0;

    bool imperial_units = AppAdapter::app_config()->get("use_inches") == "1";
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos_rect = ImGui::GetCursorScreenPos();
    float window_padding = 4.0f * m_scale;

    draw_list->AddRectFilled(ImVec2(pos_rect.x,pos_rect.y - ImGui::GetStyle().WindowPadding.y),
        ImVec2(pos_rect.x + ImGui::GetWindowWidth() + ImGui::GetFrameHeight(),pos_rect.y + ImGui::GetFrameHeight() + window_padding * 2.5),
        ImGui::GetColorU32(ImVec4(0,0,0,0.3)));

    auto append_item = [icon_size, &imgui, imperial_units, &window_padding, &draw_list, this](
        EItemType type,
        const ColorRGBA& color,
        const std::vector<std::pair<std::string, float>>& columns_offsets,
        bool checkbox = true,
        bool visible = true,
        std::function<void()> callback = nullptr)
    {
        // render icon
        ImVec2 pos = ImVec2(ImGui::GetCursorScreenPos().x + window_padding * 3, ImGui::GetCursorScreenPos().y);
        switch (type) {
        default:
        case EItemType::Rect: {
            draw_list->AddRectFilled({ pos.x + 1.0f * m_scale, pos.y + 3.0f * m_scale }, { pos.x + icon_size - 1.0f * m_scale, pos.y + icon_size + 1.0f * m_scale },
                                     ImGuiWrapper::to_ImU32(color));
            break;
        }
        case EItemType::Circle: {
            ImVec2 center(0.5f * (pos.x + pos.x + icon_size), 0.5f * (pos.y + pos.y + icon_size + 5.0f));
            draw_list->AddCircleFilled(center, 0.5f * icon_size, ImGuiWrapper::to_ImU32(color), 16);
            break;
        }
        case EItemType::Hexagon: {
            ImVec2 center(0.5f * (pos.x + pos.x + icon_size), 0.5f * (pos.y + pos.y + icon_size + 5.0f));
            draw_list->AddNgonFilled(center, 0.5f * icon_size, ImGuiWrapper::to_ImU32(color), 6);
            break;
        }
        case EItemType::Line: {
            draw_list->AddLine({ pos.x + 1, pos.y + icon_size + 2 }, { pos.x + icon_size - 1, pos.y + 4 }, ImGuiWrapper::to_ImU32(color), 3.0f);
            break;
        case EItemType::None:
            break;
        }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20.0 * m_scale, 6.0 * m_scale));

        // BBS render selectable
        ImGui::Dummy({ 0.0, 0.0 });
        ImGui::SameLine();
        if (callback) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f * m_scale);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0 * m_scale, 0.0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.00f, 0.68f, 0.26f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.00f, 0.68f, 0.26f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_BorderActive, ImVec4(0.00f, 0.59f, 0.53f, 1.00f));
            float max_height = 0.f;
            for (auto column_offset : columns_offsets) {
                if (ImGui::CalcTextSize(column_offset.first.c_str()).y > max_height)
                    max_height = ImGui::CalcTextSize(column_offset.first.c_str()).y;
            }
            bool b_menu_item = ImGui::BBLMenuItem(("##" + columns_offsets[0].first).c_str(), nullptr, false, true, max_height);
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            if (b_menu_item)
                callback();
            if (checkbox) {
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(_u8L("Display").c_str()).x / 2 - ImGui::GetFrameHeight() / 2 - 2 * window_padding);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0, 0.0));
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.00f, 0.59f, 0.53f, 1.00f));
                ImGui::Checkbox(("##" + columns_offsets[0].first).c_str(), &visible);
                ImGui::PopStyleColor(1);
                ImGui::PopStyleVar(1);
            }
        }

        // BBS render column item
        {
            if(callback && !checkbox && !visible)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(172 / 255.0f, 172 / 255.0f, 172 / 255.0f, 1.00f));
            float dummy_size = type == EItemType::None ? window_padding * 3 : ImGui::GetStyle().ItemSpacing.x + icon_size;
            ImGui::SameLine(dummy_size);
            imgui.text(columns_offsets[0].first);

            for (auto i = 1; i < columns_offsets.size(); i++) {
                ImGui::SameLine(columns_offsets[i].second);
                imgui.text(columns_offsets[i].first);
            }
            if (callback && !checkbox && !visible)
                ImGui::PopStyleColor(1);
        }

        ImGui::PopStyleVar(1);

    };

    auto append_range = [append_item](const Extrusions::Range& range, unsigned int decimals) {
        auto append_range_item = [append_item](int i, float value, unsigned int decimals) {
            char buf[1024];
            ::sprintf(buf, "%.*f", decimals, value);
            append_item(EItemType::Rect, Range_Colors[i], { { buf , 0} });
        };

        if (range.count == 1)
            // single item use case
            append_range_item(0, range.min, decimals);
        else if (range.count == 2) {
            append_range_item(static_cast<int>(Range_Colors.size()) - 1, range.max, decimals);
            append_range_item(0, range.min, decimals);
        }
        else {
            const float step_size = range.step_size();
            for (int i = static_cast<int>(Range_Colors.size()) - 1; i >= 0; --i) {
                append_range_item(i, range.get_value_at_step(i), decimals);
            }
        }
    };

    auto append_headers = [&imgui, window_padding](const std::vector<std::pair<std::string, float>>& title_offsets) {
        for (size_t i = 0; i < title_offsets.size(); i++) {
            ImGui::SameLine(title_offsets[i].second);
            imgui.bold_text(title_offsets[i].first);
        }
        // Ensure right padding
        ImGui::SameLine();
        ImGui::Dummy({window_padding, 1});
        ImGui::Separator();
    };

    auto max_width = [](const std::vector<std::string>& items, const std::string& title, float extra_size = 0.0f) {
        float ret = ImGui::CalcTextSize(title.c_str()).x;
        for (const std::string& item : items) {
            ret = std::max(ret, extra_size + ImGui::CalcTextSize(item.c_str()).x);
        }
        return ret;
    };

    auto calculate_offsets = [&imgui, max_width, window_padding](const std::vector<std::pair<std::string, std::vector<std::string>>>& title_columns, float extra_size = 0.0f) {
            const ImGuiStyle& style = ImGui::GetStyle();
            std::vector<float> offsets;
            offsets.push_back(max_width(title_columns[0].second, title_columns[0].first, extra_size) + 3.0f * style.ItemSpacing.x);
            for (size_t i = 1; i < title_columns.size() - 1; i++)
                offsets.push_back(offsets.back() + max_width(title_columns[i].second, title_columns[i].first) + style.ItemSpacing.x);
            if (title_columns.back().first == _u8L("Display")) {
                const auto preferred_offset = ImGui::GetWindowWidth() - ImGui::CalcTextSize(_u8L("Display").c_str()).x - ImGui::GetFrameHeight() / 2 - 2 * window_padding - ImGui::GetStyle().ScrollbarSize;
                if (preferred_offset > offsets.back()) {
                    offsets.back() = preferred_offset;
                }
            }

            float average_col_width = ImGui::GetWindowWidth() / static_cast<float>(title_columns.size());
            std::vector<float> ret;
            ret.push_back(0);
            for (size_t i = 1; i < title_columns.size(); i++) {
                ret.push_back(std::max(offsets[i - 1], i * average_col_width));
            }

            return ret;
    };

    auto color_print_ranges = [this](unsigned char extruder_id, const std::vector<CustomGCode::Item>& custom_gcode_per_print_z) {
        std::vector<std::pair<ColorRGBA, std::pair<double, double>>> ret;
        ret.reserve(custom_gcode_per_print_z.size());

        for (const auto& item : custom_gcode_per_print_z) {
            if (extruder_id + 1 != static_cast<unsigned char>(item.extruder))
                continue;

            if (item.type != ColorChange)
                continue;

            const std::vector<double> zs = m_data->m_layers.get_zs();
            auto lower_b = std::lower_bound(zs.begin(), zs.end(), item.print_z - EPSILON);
            if (lower_b == zs.end())
                continue;

            const double current_z = *lower_b;
            const double previous_z = (lower_b == zs.begin()) ? 0.0 : *(--lower_b);

            // to avoid duplicate values, check adding values
            if (ret.empty() || !(ret.back().second.first == previous_z && ret.back().second.second == current_z))
            {
                ColorRGBA color;
                decode_color(item.color, color);
                ret.push_back({ color, { previous_z, current_z } });
            }
        }

        return ret;
    };

    auto upto_label = [](double z) {
        char buf[64];
        ::sprintf(buf, "%.2f", z);
        return _u8L("up to") + " " + std::string(buf) + " " + _u8L("mm");
    };

    auto above_label = [](double z) {
        char buf[64];
        ::sprintf(buf, "%.2f", z);
        return _u8L("above") + " " + std::string(buf) + " " + _u8L("mm");
    };

    auto fromto_label = [](double z1, double z2) {
        char buf1[64];
        ::sprintf(buf1, "%.2f", z1);
        char buf2[64];
        ::sprintf(buf2, "%.2f", z2);
        return _u8L("from") + " " + std::string(buf1) + " " + _u8L("to") + " " + std::string(buf2) + " " + _u8L("mm");
    };

    auto role_time_and_percent = [time_mode](ExtrusionRole role) {
        auto it = std::find_if(time_mode.roles_times.begin(), time_mode.roles_times.end(), [role](const std::pair<ExtrusionRole, float>& item) { return role == item.first; });
        return (it != time_mode.roles_times.end()) ? std::make_pair(it->second, it->second / time_mode.time) : std::make_pair(0.0f, 0.0f);
    };

    auto move_time_and_percent = [time_mode](EMoveType move_type) {
        auto it = std::find_if(time_mode.moves_times.begin(), time_mode.moves_times.end(), [move_type](const std::pair<EMoveType, float>& item) { return move_type == item.first; });
        return (it != time_mode.moves_times.end()) ? std::make_pair(it->second, it->second / time_mode.time) : std::make_pair(0.0f, 0.0f);
    };

    auto used_filament_per_role = [this, imperial_units](ExtrusionRole role) {
        auto it = m_data->m_print_statistics.used_filaments_per_role.find(role);
        if (it == m_data->m_print_statistics.used_filaments_per_role.end())
            return std::make_pair(0.0, 0.0);

        double koef        = imperial_units ? GizmoObjectManipulation::in_to_mm / 1000.0 : 1.0;
        double unit_conver = imperial_units ? GizmoObjectManipulation::oz_to_g : 1;
        return std::make_pair(it->second.first / koef, it->second.second / unit_conver);
    };

    // get used filament (meters and grams) from used volume in respect to the active extruder
    auto get_used_filament_from_volume = [this, imperial_units](double volume, int extruder_id) {
        double koef = imperial_units ? 1.0 / GizmoObjectManipulation::in_to_mm : 0.001;
        std::pair<double, double> ret = { koef * volume / (PI * sqr(0.5 * m_data->m_filament_diameters[extruder_id])),
                                          volume * m_data->m_filament_densities[extruder_id] * 0.001 };
        return ret;
    };

    //BBS display Color Scheme
    ImGui::Dummy({ window_padding, window_padding });
    ImGui::Dummy({ window_padding, window_padding });
    ImGui::SameLine();
    std::wstring btn_name;
    if (m_data->m_fold)
        btn_name = ImGui::UnfoldButtonIcon;
    else
        btn_name = ImGui::FoldButtonIcon;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.59f, 0.53f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.59f, 0.53f, 0.78f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    //ImGui::PushItemWidth(
    float button_width = 34.0f;
    if (ImGui::Button(into_u8(btn_name).c_str(), ImVec2(button_width, 0))) {
        m_data->m_fold = !m_data->m_fold;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(1);
    ImGui::SameLine();
    imgui.bold_text(_u8L("Color Scheme"));
    push_combo_style();

    ImGui::SameLine();
    const char* view_type_value = m_data->view_type_items_str[m_data->m_view_type_sel].c_str();
    ImGuiComboFlags flags = 0;
    if (ImGui::BBLBeginCombo("", view_type_value, flags)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        for (int i = 0; i < m_data->view_type_items_str.size(); i++) {
            const bool is_selected = (m_data->m_view_type_sel == i);
            if (ImGui::BBLSelectable(m_data->view_type_items_str[i].c_str(), is_selected)) {
                m_data->m_fold = false;
                m_data->m_view_type_sel = i;
                m_data->set_view_type(m_data->view_type_items[m_data->m_view_type_sel]);
                m_data->reset_visible(m_data->view_type_items[m_data->m_view_type_sel]);
                // update buffers' render paths
                if (m_refresh_func)
                    m_refresh_func(true);
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::PopStyleVar(1);
        ImGui::EndCombo();
    }
    pop_combo_style();
    ImGui::SameLine();
    ImGui::Dummy({ window_padding, window_padding });

    if (m_data->m_fold) {
        legend_height = ImGui::GetStyle().WindowPadding.y + ImGui::GetFrameHeight() + window_padding * 2.5;
        imgui.end();
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
        return;
    }

    // data used to properly align items in columns when showing time
    std::vector<float> offsets;
    std::vector<std::string> labels;
    std::vector<std::string> times;
    std::string travel_time;
    std::vector<std::string> percents;
    std::vector<std::string> used_filaments_length;
    std::vector<std::string> used_filaments_weight;
    std::string travel_percent;
    std::vector<double> model_used_filaments_m;
    std::vector<double> model_used_filaments_g;
    double total_model_used_filament_m = 0, total_model_used_filament_g = 0;
    std::vector<double> flushed_filaments_m;
    std::vector<double> flushed_filaments_g;
    double total_flushed_filament_m = 0, total_flushed_filament_g = 0;
    std::vector<double> wipe_tower_used_filaments_m;
    std::vector<double> wipe_tower_used_filaments_g;
    double total_wipe_tower_used_filament_m = 0, total_wipe_tower_used_filament_g = 0;
    std::vector<double> support_used_filaments_m;
    std::vector<double> support_used_filaments_g;
    double total_support_used_filament_m = 0, total_support_used_filament_g = 0;
    struct ColumnData {
        enum {
            Model = 1,
            Flushed = 2,
            WipeTower = 4,
            Support = 1 << 3,
        };
    };
    int displayed_columns = 0;
    std::map<std::string, float> color_print_offsets;
    // const PrintStatistics& ps = AppAdapter::plater()->get_partplate_list().get_current_fff_print().print_statistics();
    PrintStatistics ps;
    
    double koef = imperial_units ? GizmoObjectManipulation::in_to_mm : 1000.0;
    double unit_conver = imperial_units ? GizmoObjectManipulation::oz_to_g : 1;


    // used filament statistics
    for (size_t extruder_id : m_data->m_extruder_ids) {
        if (m_data->m_print_statistics.model_volumes_per_extruder.find(extruder_id) == m_data->m_print_statistics.model_volumes_per_extruder.end()) {
            model_used_filaments_m.push_back(0.0);
            model_used_filaments_g.push_back(0.0);
        }
        else {
            double volume = m_data->m_print_statistics.model_volumes_per_extruder.at(extruder_id);
            auto [model_used_filament_m, model_used_filament_g] = get_used_filament_from_volume(volume, extruder_id);
            model_used_filaments_m.push_back(model_used_filament_m);
            model_used_filaments_g.push_back(model_used_filament_g);
            total_model_used_filament_m += model_used_filament_m;
            total_model_used_filament_g += model_used_filament_g;
            displayed_columns |= ColumnData::Model;
        }
    }

    for (size_t extruder_id : m_data->m_extruder_ids) {
        if (m_data->m_print_statistics.wipe_tower_volumes_per_extruder.find(extruder_id) == m_data->m_print_statistics.wipe_tower_volumes_per_extruder.end()) {
            wipe_tower_used_filaments_m.push_back(0.0);
            wipe_tower_used_filaments_g.push_back(0.0);
        }
        else {
            double volume = m_data->m_print_statistics.wipe_tower_volumes_per_extruder.at(extruder_id);
            auto [wipe_tower_used_filament_m, wipe_tower_used_filament_g] = get_used_filament_from_volume(volume, extruder_id);
            wipe_tower_used_filaments_m.push_back(wipe_tower_used_filament_m);
            wipe_tower_used_filaments_g.push_back(wipe_tower_used_filament_g);
            total_wipe_tower_used_filament_m += wipe_tower_used_filament_m;
            total_wipe_tower_used_filament_g += wipe_tower_used_filament_g;
            displayed_columns |= ColumnData::WipeTower;
        }
    }

    for (size_t extruder_id : m_data->m_extruder_ids) {
        if (m_data->m_print_statistics.flush_per_filament.find(extruder_id) == m_data->m_print_statistics.flush_per_filament.end()) {
            flushed_filaments_m.push_back(0.0);
            flushed_filaments_g.push_back(0.0);
        }
        else {
            double volume = m_data->m_print_statistics.flush_per_filament.at(extruder_id);
            auto [flushed_filament_m, flushed_filament_g] = get_used_filament_from_volume(volume, extruder_id);
            flushed_filaments_m.push_back(flushed_filament_m);
            flushed_filaments_g.push_back(flushed_filament_g);
            total_flushed_filament_m += flushed_filament_m;
            total_flushed_filament_g += flushed_filament_g;
            displayed_columns |= ColumnData::Flushed;
        }
    }

    for (size_t extruder_id : m_data->m_extruder_ids) {
        if (m_data->m_print_statistics.support_volumes_per_extruder.find(extruder_id) == m_data->m_print_statistics.support_volumes_per_extruder.end()) {
            support_used_filaments_m.push_back(0.0);
            support_used_filaments_g.push_back(0.0);
        }
        else {
            double volume = m_data->m_print_statistics.support_volumes_per_extruder.at(extruder_id);
            auto [used_filament_m, used_filament_g] = get_used_filament_from_volume(volume, extruder_id);
            support_used_filaments_m.push_back(used_filament_m);
            support_used_filaments_g.push_back(used_filament_g);
            total_support_used_filament_m += used_filament_m;
            total_support_used_filament_g += used_filament_g;
            displayed_columns |= ColumnData::Support;
        }
    }


    // extrusion paths section -> title
    ImGui::Dummy({ window_padding, window_padding });
    ImGui::SameLine();
    switch (m_data->m_view_type)
    {
    case EViewType::FeatureType:
    {
        // calculate offsets to align time/percentage data
        char buffer[64];
        for (size_t i = 0; i < m_data->m_roles.size(); ++i) {
            ExtrusionRole role = m_data->m_roles[i];
            if (role < erCount) {
                labels.push_back(_u8L(ExtrusionEntity::role_to_string(role)));
                auto [time, percent] = role_time_and_percent(role);
                times.push_back((time > 0.0f) ? short_time(get_time_dhms(time)) : "");
                if (percent == 0)
                    ::sprintf(buffer, "0%%");
                else
                    percent > 0.001 ? ::sprintf(buffer, "%.1f%%", percent * 100) : ::sprintf(buffer, "<0.1%%");
                percents.push_back(buffer);

                auto [model_used_filament_m, model_used_filament_g] = used_filament_per_role(role);
                ::sprintf(buffer, imperial_units ? "%.2f in" : "%.2f m", model_used_filament_m);
                used_filaments_length.push_back(buffer);
                ::sprintf(buffer, imperial_units ? "%.2f oz" : "%.2f g", model_used_filament_g);
                used_filaments_weight.push_back(buffer);
            }
        }

        //BBS: get travel time and percent
        {
            auto [time, percent] = move_time_and_percent(EMoveType::Travel);
            travel_time = (time > 0.0f) ? short_time(get_time_dhms(time)) : "";
            if (percent == 0)
                ::sprintf(buffer, "0%%");
            else
                percent > 0.001 ? ::sprintf(buffer, "%.1f%%", percent * 100) : ::sprintf(buffer, "<0.1%%");
            travel_percent = buffer;
        }

        offsets = calculate_offsets({ {_u8L("Line Type"), labels}, {_u8L("Time"), times}, {_u8L("Percent"), percents}, {"", used_filaments_length}, {"", used_filaments_weight}, {_u8L("Display"), {""}}}, icon_size);
        append_headers({{_u8L("Line Type"), offsets[0]}, {_u8L("Time"), offsets[1]}, {_u8L("Percent"), offsets[2]}, {_u8L("Used filament"), offsets[3]}, {_u8L("Display"), offsets[5]}});
        break;
    }
    case EViewType::Height:         { imgui.title(_u8L("Layer Height (mm)")); break; }
    case EViewType::Width:          { imgui.title(_u8L("Line Width (mm)")); break; }
    case EViewType::Feedrate:
    {
        imgui.title(_u8L("Speed (mm/s)"));
        break;
    }

    case EViewType::FanSpeed:       { imgui.title(_u8L("Fan Speed (%)")); break; }
    case EViewType::Temperature:    { imgui.title(_u8L("Temperature (°C)")); break; }
    case EViewType::VolumetricRate: { imgui.title(_u8L("Volumetric flow rate (mm³/s)")); break; }
    case EViewType::LayerTime:      { imgui.title(_u8L("Layer Time")); break; }
    case EViewType::LayerTimeLog:   { imgui.title(_u8L("Layer Time (log)")); break; }

    case EViewType::Tool:
    {
        // calculate used filaments data
        for (size_t extruder_id : m_data->m_extruder_ids) {
            if (m_data->m_print_statistics.model_volumes_per_extruder.find(extruder_id) == m_data->m_print_statistics.model_volumes_per_extruder.end())
                continue;
            double volume = m_data->m_print_statistics.model_volumes_per_extruder.at(extruder_id);

            auto [model_used_filament_m, model_used_filament_g] = get_used_filament_from_volume(volume, extruder_id);
            model_used_filaments_m.push_back(model_used_filament_m);
            model_used_filaments_g.push_back(model_used_filament_g);
        }

        offsets = calculate_offsets({ { "Extruder NNN", {""}}}, icon_size);
        append_headers({ {_u8L("Filament"), offsets[0]}, {_u8L("Used filament"), offsets[1]} });
        break;
    }
    case EViewType::ColorPrint:
    {
        std::vector<std::string> total_filaments;
        char buffer[64];
        ::sprintf(buffer, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", ps.total_used_filament / /*1000*/koef, ps.total_weight / unit_conver);
        total_filaments.push_back(buffer);


        std::vector<std::pair<std::string, std::vector<std::string>>> title_columns;
        if (displayed_columns & ColumnData::Model) {
            title_columns.push_back({ _u8L("Filament"), {""} });
            title_columns.push_back({ _u8L("Model"), total_filaments });
        }
        if (displayed_columns & ColumnData::Support) {
            title_columns.push_back({ _u8L("Support"), total_filaments });
        }
        if (displayed_columns & ColumnData::Flushed) {
            title_columns.push_back({ _u8L("Flushed"), total_filaments });
        }
        if (displayed_columns & ColumnData::WipeTower) {
            title_columns.push_back({ _u8L("Tower"), total_filaments });
        }
        if ((displayed_columns & ~ColumnData::Model) > 0) {
            title_columns.push_back({ _u8L("Total"), total_filaments });
        }
        auto offsets_ = calculate_offsets(title_columns, icon_size);
        std::vector<std::pair<std::string, float>> title_offsets;
        for (int i = 0; i < offsets_.size(); i++) {
            title_offsets.push_back({ title_columns[i].first, offsets_[i] });
            color_print_offsets[title_columns[i].first] = offsets_[i];
        }
        append_headers(title_offsets);

        break;
    }
    default: { break; }
    }

    auto append_option_item = [this, append_item](EMoveType type, std::vector<float> offsets) {
        auto append_option_item_with_type = [this, offsets, append_item](EMoveType type, const ColorRGBA& color, const std::string& label, bool visible) {
            append_item(EItemType::Rect, color, {{ label , offsets[0] }}, true, visible, [this, type, visible]() {
                m_data->m_buffers[buffer_id(type)].visible = !m_data->m_buffers[buffer_id(type)].visible;
                //// update buffers' render paths
                if (m_refresh_func)
                    m_refresh_func(true);
                });
        };
        const bool visible = m_data->m_buffers[buffer_id(type)].visible;
        if (type == EMoveType::Travel) {
            //BBS: only display travel time in FeatureType view
            append_option_item_with_type(type, Travel_Colors[0], _u8L("Travel"), visible);
        }
        else if (type == EMoveType::Seam)
            append_option_item_with_type(type, Options_Colors[(int)EOptionsColors::Seams], _u8L("Seams"), visible);
        else if (type == EMoveType::Retract)
            append_option_item_with_type(type, Options_Colors[(int)EOptionsColors::Retractions], _u8L("Retract"), visible);
        else if (type == EMoveType::Unretract)
            append_option_item_with_type(type, Options_Colors[(int)EOptionsColors::Unretractions], _u8L("Unretract"), visible);
        else if (type == EMoveType::Tool_change)
            append_option_item_with_type(type, Options_Colors[(int)EOptionsColors::ToolChanges], _u8L("Filament Changes"), visible);
        else if (type == EMoveType::Wipe)
            append_option_item_with_type(type, Wipe_Color, _u8L("Wipe"), visible);
    };

    // extrusion paths section -> items
    switch (m_data->m_view_type)
    {
    case EViewType::FeatureType:
    {
        for (size_t i = 0; i < m_data->m_roles.size(); ++i) {
            ExtrusionRole role = m_data->m_roles[i];
            if (role >= erCount)
                continue;
            const bool visible = m_data->is_visible(role);
            std::vector<std::pair<std::string, float>> columns_offsets;
            columns_offsets.push_back({ labels[i], offsets[0] });
            columns_offsets.push_back({ times[i], offsets[1] });
            columns_offsets.push_back({percents[i], offsets[2]});
            columns_offsets.push_back({used_filaments_length[i], offsets[3]});
            columns_offsets.push_back({used_filaments_weight[i], offsets[4]});
            append_item(EItemType::Rect, Extrusion_Role_Colors[static_cast<unsigned int>(role)], columns_offsets,
                true, visible, [this, role, visible]() {
                    m_data->m_extrusions.role_visibility_flags = visible ? m_data->m_extrusions.role_visibility_flags & ~(1 << role) : m_data->m_extrusions.role_visibility_flags | (1 << role);
                    //// update buffers' render paths
                    if (m_refresh_func)
                        m_refresh_func(true);
                });
        }

        for(auto item : m_data->options_items) {
            if (item != EMoveType::Travel) {
                append_option_item(item, offsets);
            } else {
                //BBS: show travel time in FeatureType view
                const bool visible = m_data->m_buffers[buffer_id(item)].visible;
                std::vector<std::pair<std::string, float>> columns_offsets;
                columns_offsets.push_back({ _u8L("Travel"), offsets[0] });
                columns_offsets.push_back({ travel_time, offsets[1] });
                columns_offsets.push_back({ travel_percent, offsets[2] });
                append_item(EItemType::Rect, Travel_Colors[0], columns_offsets, true, visible, [this, item, visible]() {
                        m_data->m_buffers[buffer_id(item)].visible = !m_data->m_buffers[buffer_id(item)].visible;
                        //// update buffers' render paths
                        if (m_refresh_func)
                            m_refresh_func(true);
                    });
            }
        }
        break;
    }
    case EViewType::Height:         { append_range(m_data->m_extrusions.ranges.height, 2); break; }
    case EViewType::Width:          { append_range(m_data->m_extrusions.ranges.width, 2); break; }
    case EViewType::Feedrate:       {
        append_range(m_data->m_extrusions.ranges.feedrate, 0);
        ImGui::Spacing();
        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        offsets = calculate_offsets({ { _u8L("Options"), { _u8L("Travel")}}, { _u8L("Display"), {""}} }, icon_size);
        append_headers({ {_u8L("Options"), offsets[0] }, { _u8L("Display"), offsets[1]} });
        const bool travel_visible = m_data->m_buffers[buffer_id(EMoveType::Travel)].visible;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 3.0f));
        append_item(EItemType::None, Travel_Colors[0], { {_u8L("travel"), offsets[0] }}, true, travel_visible, [this, travel_visible]() {
            m_data->m_buffers[buffer_id(EMoveType::Travel)].visible = !m_data->m_buffers[buffer_id(EMoveType::Travel)].visible;
            // update buffers' render paths, and update m_data->m_tools.m_tool_colors and m_data->m_extrusions.ranges
            if (m_refresh_func)
                m_refresh_func(false);
            });
        ImGui::PopStyleVar(1);
        break;
    }
    case EViewType::FanSpeed:       { append_range(m_data->m_extrusions.ranges.fan_speed, 0); break; }
    case EViewType::Temperature:    { append_range(m_data->m_extrusions.ranges.temperature, 0); break; }
    case EViewType::LayerTime:      { append_range(m_data->m_extrusions.ranges.layer_duration, true); break; }
    case EViewType::LayerTimeLog:   { append_range(m_data->m_extrusions.ranges.layer_duration_log, true); break; }
    case EViewType::VolumetricRate: { append_range(m_data->m_extrusions.ranges.volumetric_rate, 2); break; }
    case EViewType::Tool:
    {
        // shows only extruders actually used
        char buf[64];
        size_t i = 0;
        for (unsigned char extruder_id : m_data->m_extruder_ids) {
            ::sprintf(buf, imperial_units ? "%.2f in    %.2f g" : "%.2f m    %.2f g", model_used_filaments_m[i], model_used_filaments_g[i]);
            append_item(EItemType::Rect, m_data->m_tools.m_tool_colors[extruder_id], { { _u8L("Extruder") + " " + std::to_string(extruder_id + 1), offsets[0]}, {buf, offsets[1]} });
            i++;
        }
        break;
    }
    case EViewType::ColorPrint:
    {
        //BBS: replace model custom gcode with current plate custom gcode
        const std::vector<CustomGCode::Item>& custom_gcode_per_print_z = AppAdapter::plater()->model().get_curr_plate_custom_gcodes().gcodes;
        size_t total_items = 1;
        // BBS: no ColorChange type, use ToolChange
        //for (size_t extruder_id : m_data->m_extruder_ids) {
        //    total_items += color_print_ranges(extruder_id, custom_gcode_per_print_z).size();
        //}

        const bool need_scrollable = static_cast<float>(total_items) * (icon_size + ImGui::GetStyle().ItemSpacing.y) > child_height;

        // add scrollable region, if needed
        if (need_scrollable)
            ImGui::BeginChild("color_prints", { -1.0f, child_height }, false);

        // shows only extruders actually used
        size_t i = 0;
        for (auto extruder_idx : m_data->m_extruder_ids) {
            const bool filament_visible = m_data->m_tools.m_tool_visibles[extruder_idx];
            if (i < model_used_filaments_m.size() && i < model_used_filaments_g.size()) {
                std::vector<std::pair<std::string, float>> columns_offsets;
                columns_offsets.push_back({ std::to_string(extruder_idx + 1), color_print_offsets[_u8L("Filament")]});

                char buf[64];
                float column_sum_m = 0.0f;
                float column_sum_g = 0.0f;
                if (displayed_columns & ColumnData::Model) {
                    if ((displayed_columns & ~ColumnData::Model) > 0)
                        ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", model_used_filaments_m[i], model_used_filaments_g[i] / unit_conver);
                    else
                        ::sprintf(buf, imperial_units ? "%.2f in    %.2f oz" : "%.2f m    %.2f g", model_used_filaments_m[i], model_used_filaments_g[i] / unit_conver);
                    columns_offsets.push_back({ buf, color_print_offsets[_u8L("Model")] });
                    column_sum_m += model_used_filaments_m[i];
                    column_sum_g += model_used_filaments_g[i];
                }
                if (displayed_columns & ColumnData::Support) {
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", support_used_filaments_m[i], support_used_filaments_g[i] / unit_conver);
                    columns_offsets.push_back({ buf, color_print_offsets[_u8L("Support")] });
                    column_sum_m += support_used_filaments_m[i];
                    column_sum_g += support_used_filaments_g[i];
                }
                if (displayed_columns & ColumnData::Flushed) {
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", flushed_filaments_m[i], flushed_filaments_g[i] / unit_conver);
                    columns_offsets.push_back({ buf, color_print_offsets[_u8L("Flushed")]});
                    column_sum_m += flushed_filaments_m[i];
                    column_sum_g += flushed_filaments_g[i];
                }
                if (displayed_columns & ColumnData::WipeTower) {
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", wipe_tower_used_filaments_m[i], wipe_tower_used_filaments_g[i] / unit_conver);
                    columns_offsets.push_back({ buf, color_print_offsets[_u8L("Tower")] });
                    column_sum_m += wipe_tower_used_filaments_m[i];
                    column_sum_g += wipe_tower_used_filaments_g[i];
                }
                if ((displayed_columns & ~ColumnData::Model) > 0) {
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", column_sum_m, column_sum_g / unit_conver);
                    columns_offsets.push_back({ buf, color_print_offsets[_u8L("Total")] });
                }

                append_item(EItemType::Rect, m_data->m_tools.m_tool_colors[extruder_idx], columns_offsets, false, filament_visible, [this, extruder_idx]() {
                        m_data->m_tools.m_tool_visibles[extruder_idx] = !m_data->m_tools.m_tool_visibles[extruder_idx];
                        // update buffers' render paths
                        if (m_refresh_func)
                            m_refresh_func(true);
                    });
            }
            i++;
        }
        
        if (need_scrollable)
            ImGui::EndChild();

        // Sum of all rows
        char buf[64];
        if (m_data->m_extruder_ids.size() > 1) {
            // Separator
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            const ImRect separator(ImVec2(window->Pos.x + window_padding * 3, window->DC.CursorPos.y), ImVec2(window->Pos.x + window->Size.x - window_padding * 3, window->DC.CursorPos.y + 1.0f));
            ImGui::ItemSize(ImVec2(0.0f, 0.0f));
            const bool item_visible = ImGui::ItemAdd(separator, 0);
            window->DrawList->AddLine(separator.Min, ImVec2(separator.Max.x, separator.Min.y), ImGui::GetColorU32(ImGuiCol_Separator));

            std::vector<std::pair<std::string, float>> columns_offsets;
            columns_offsets.push_back({ _u8L("Total"), color_print_offsets[_u8L("Filament")]});
            if (displayed_columns & ColumnData::Model) {
                if ((displayed_columns & ~ColumnData::Model) > 0)
                    ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", total_model_used_filament_m, total_model_used_filament_g / unit_conver);
                else
                    ::sprintf(buf, imperial_units ? "%.2f in    %.2f oz" : "%.2f m    %.2f g", total_model_used_filament_m, total_model_used_filament_g / unit_conver);
                columns_offsets.push_back({ buf, color_print_offsets[_u8L("Model")] });
            }
            if (displayed_columns & ColumnData::Support) {
                ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", total_support_used_filament_m, total_support_used_filament_g / unit_conver);
                columns_offsets.push_back({ buf, color_print_offsets[_u8L("Support")] });
            }
            if (displayed_columns & ColumnData::Flushed) {
                ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", total_flushed_filament_m, total_flushed_filament_g / unit_conver);
                columns_offsets.push_back({ buf, color_print_offsets[_u8L("Flushed")] });
            }
            if (displayed_columns & ColumnData::WipeTower) {
                ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", total_wipe_tower_used_filament_m, total_wipe_tower_used_filament_g / unit_conver);
                columns_offsets.push_back({ buf, color_print_offsets[_u8L("Tower")] });
            }
            if ((displayed_columns & ~ColumnData::Model) > 0) {
                ::sprintf(buf, imperial_units ? "%.2f in\n%.2f oz" : "%.2f m\n%.2f g", total_model_used_filament_m + total_support_used_filament_m + total_flushed_filament_m + total_wipe_tower_used_filament_m, 
                    (total_model_used_filament_g + total_support_used_filament_g + total_flushed_filament_g + total_wipe_tower_used_filament_g) / unit_conver);
                columns_offsets.push_back({ buf, color_print_offsets[_u8L("Total")] });
            }
            append_item(EItemType::None, m_data->m_tools.m_tool_colors[0], columns_offsets);
        }

        //BBS display filament change times
        ImGui::Dummy({window_padding, window_padding});
        ImGui::SameLine();
        imgui.text(_u8L("Filament change times") + ":");
        ImGui::SameLine();
        ::sprintf(buf, "%d", m_data->m_print_statistics.total_filamentchanges);
        imgui.text(buf);

        //BBS display cost
        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        imgui.text(_u8L("Cost")+":");
        ImGui::SameLine();
        ::sprintf(buf, "%.2f", ps.total_cost);
        imgui.text(buf);

        break;
    }
    default: { break; }
    }

    // partial estimated printing time section
    if (m_data->m_view_type == EViewType::ColorPrint) {
        using Times = std::pair<float, float>;
        using TimesList = std::vector<std::pair<CustomGCode::Type, Times>>;

        // helper structure containig the data needed to render the time items
        struct PartialTime
        {
            enum class EType : unsigned char
            {
                Print,
                ColorChange,
                Pause
            };
            EType type;
            int extruder_id;
            ColorRGBA color1;
            ColorRGBA color2;
            Times times;
            std::pair<double, double> used_filament {0.0f, 0.0f};
        };
        using PartialTimes = std::vector<PartialTime>;

        auto generate_partial_times = [this, get_used_filament_from_volume](const TimesList& times, const std::vector<double>& used_filaments) {
            PartialTimes items;

            //BBS: replace model custom gcode with current plate custom gcode
            std::vector<CustomGCode::Item> custom_gcode_per_print_z = AppAdapter::plater()->model().get_curr_plate_custom_gcodes().gcodes;
            std::vector<ColorRGBA> last_color(m_data->m_extruders_count);
            for (size_t i = 0; i < m_data->m_extruders_count; ++i) {
                last_color[i] = m_data->m_tools.m_tool_colors[i];
            }
            int last_extruder_id = 1;
            int color_change_idx = 0;
            for (const auto& time_rec : times) {
                switch (time_rec.first)
                {
                case CustomGCode::PausePrint: {
                    auto it = std::find_if(custom_gcode_per_print_z.begin(), custom_gcode_per_print_z.end(), [time_rec](const CustomGCode::Item& item) { return item.type == time_rec.first; });
                    if (it != custom_gcode_per_print_z.end()) {
                        items.push_back({ PartialTime::EType::Print, it->extruder, last_color[it->extruder - 1], ColorRGBA::BLACK(), time_rec.second });
                        items.push_back({ PartialTime::EType::Pause, it->extruder, ColorRGBA::BLACK(), ColorRGBA::BLACK(), time_rec.second });
                        custom_gcode_per_print_z.erase(it);
                    }
                    break;
                }
                case CustomGCode::ColorChange: {
                    auto it = std::find_if(custom_gcode_per_print_z.begin(), custom_gcode_per_print_z.end(), [time_rec](const CustomGCode::Item& item) { return item.type == time_rec.first; });
                    if (it != custom_gcode_per_print_z.end()) {
                        items.push_back({ PartialTime::EType::Print, it->extruder, last_color[it->extruder - 1], ColorRGBA::BLACK(), time_rec.second, get_used_filament_from_volume(used_filaments[color_change_idx++], it->extruder - 1) });
                        ColorRGBA color;
                        decode_color(it->color, color);
                        items.push_back({ PartialTime::EType::ColorChange, it->extruder, last_color[it->extruder - 1], color, time_rec.second });
                        last_color[it->extruder - 1] = color;
                        last_extruder_id = it->extruder;
                        custom_gcode_per_print_z.erase(it);
                    }
                    else
                        items.push_back({ PartialTime::EType::Print, last_extruder_id, last_color[last_extruder_id - 1], ColorRGBA::BLACK(), time_rec.second, get_used_filament_from_volume(used_filaments[color_change_idx++], last_extruder_id - 1) });

                    break;
                }
                default: { break; }
                }
            }

            return items;
        };

        auto append_color_change = [&imgui](const ColorRGBA& color1, const ColorRGBA& color2, const std::array<float, 4>& offsets, const Times& times) {
            imgui.text(_u8L("Color change"));
            ImGui::SameLine();

            float icon_size = ImGui::GetTextLineHeight();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            pos.x -= 0.5f * ImGui::GetStyle().ItemSpacing.x;

            draw_list->AddRectFilled({ pos.x + 1.0f, pos.y + 1.0f }, { pos.x + icon_size - 1.0f, pos.y + icon_size - 1.0f },
                ImGuiWrapper::to_ImU32(color1));
            pos.x += icon_size;
            draw_list->AddRectFilled({ pos.x + 1.0f, pos.y + 1.0f }, { pos.x + icon_size - 1.0f, pos.y + icon_size - 1.0f },
                ImGuiWrapper::to_ImU32(color2));

            ImGui::SameLine(offsets[0]);
            imgui.text(short_time(get_time_dhms(times.second - times.first)));
        };

        auto append_print = [&imgui, imperial_units](const ColorRGBA& color, const std::array<float, 4>& offsets, const Times& times, std::pair<double, double> used_filament) {
            imgui.text(_u8L("Print"));
            ImGui::SameLine();

            float icon_size = ImGui::GetTextLineHeight();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            pos.x -= 0.5f * ImGui::GetStyle().ItemSpacing.x;

            draw_list->AddRectFilled({ pos.x + 1.0f, pos.y + 1.0f }, { pos.x + icon_size - 1.0f, pos.y + icon_size - 1.0f },
                ImGuiWrapper::to_ImU32(color));

            ImGui::SameLine(offsets[0]);
            imgui.text(short_time(get_time_dhms(times.second)));
            ImGui::SameLine(offsets[1]);
            imgui.text(short_time(get_time_dhms(times.first)));
            if (used_filament.first > 0.0f) {
                char buffer[64];
                ImGui::SameLine(offsets[2]);
                ::sprintf(buffer, imperial_units ? "%.2f in" : "%.2f m", used_filament.first);
                imgui.text(buffer);

                ImGui::SameLine(offsets[3]);
                ::sprintf(buffer, "%.2f g", used_filament.second);
                imgui.text(buffer);
            }
        };

        PartialTimes partial_times = generate_partial_times(time_mode.custom_gcode_times, m_data->m_print_statistics.volumes_per_color_change);
        if (!partial_times.empty()) {
            labels.clear();
            times.clear();

            for (const PartialTime& item : partial_times) {
                switch (item.type)
                {
                case PartialTime::EType::Print:       { labels.push_back(_u8L("Print")); break; }
                case PartialTime::EType::Pause:       { labels.push_back(_u8L("Pause")); break; }
                case PartialTime::EType::ColorChange: { labels.push_back(_u8L("Color change")); break; }
                }
                times.push_back(short_time(get_time_dhms(item.times.second)));
            }

            std::string longest_used_filament_string;
            for (const PartialTime& item : partial_times) {
                if (item.used_filament.first > 0.0f) {
                    char buffer[64];
                    ::sprintf(buffer, imperial_units ? "%.2f in" : "%.2f m", item.used_filament.first);
                    if (::strlen(buffer) > longest_used_filament_string.length())
                        longest_used_filament_string = buffer;
                }
            }
        }
    }

    // travel paths section
    if (m_data->m_buffers[buffer_id(EMoveType::Travel)].visible) {
        switch (m_data->m_view_type)
        {
        case EViewType::Feedrate:
        case EViewType::Tool:
        case EViewType::ColorPrint: {
            break;
        }
        default: {
            break;
        }
        }
    }


    auto any_option_available = [this]() {
        auto available = [this](EMoveType type) {
            const TBuffer& buffer = m_data->m_buffers[buffer_id(type)];
            return buffer.visible && buffer.has_data();
        };

        return available(EMoveType::Color_change) ||
            available(EMoveType::Custom_GCode) ||
            available(EMoveType::Pause_Print) ||
            available(EMoveType::Retract) ||
            available(EMoveType::Tool_change) ||
            available(EMoveType::Unretract) ||
            available(EMoveType::Seam);
    };


    // settings section
    bool has_settings = false;
    has_settings |= !m_data->m_settings_ids.print.empty();
    has_settings |= !m_data->m_settings_ids.printer.empty();
    bool has_filament_settings = true;
    has_filament_settings &= !m_data->m_settings_ids.filament.empty();
    for (const std::string& fs : m_data->m_settings_ids.filament) {
        has_filament_settings &= !fs.empty();
    }
    has_settings |= has_filament_settings;
    //BBS: add only gcode mode

    bool show_settings = m_data->m_only_gcode_in_preview;
    show_settings &= (m_data->m_view_type == EViewType::FeatureType || m_data->m_view_type == EViewType::Tool);
    show_settings &= has_settings;
    if (show_settings) {
        auto calc_offset = [this]() {
            float ret = 0.0f;
            if (!m_data->m_settings_ids.printer.empty())
                ret = std::max(ret, ImGui::CalcTextSize((_u8L("Printer") + std::string(":")).c_str()).x);
            if (!m_data->m_settings_ids.print.empty())
                ret = std::max(ret, ImGui::CalcTextSize((_u8L("Print settings") + std::string(":")).c_str()).x);
            if (!m_data->m_settings_ids.filament.empty()) {
                for (unsigned char i : m_data->m_extruder_ids) {
                    ret = std::max(ret, ImGui::CalcTextSize((_u8L("Filament") + " " + std::to_string(i + 1) + ":").c_str()).x);
                }
            }
            if (ret > 0.0f)
                ret += 2.0f * ImGui::GetStyle().ItemSpacing.x;
            return ret;
        };

        ImGui::Spacing();
        imgui.title(_u8L("Settings"));

        float offset = calc_offset();

        if (!m_data->m_settings_ids.printer.empty()) {
            imgui.text(_u8L("Printer") + ":");
            ImGui::SameLine(offset);
            imgui.text(m_data->m_settings_ids.printer);
        }
        if (!m_data->m_settings_ids.print.empty()) {
            imgui.text(_u8L("Print settings") + ":");
            ImGui::SameLine(offset);
            imgui.text(m_data->m_settings_ids.print);
        }
        if (!m_data->m_settings_ids.filament.empty()) {
            for (unsigned char i : m_data->m_extruder_ids) {
                if (i < static_cast<unsigned char>(m_data->m_settings_ids.filament.size()) && !m_data->m_settings_ids.filament[i].empty()) {
                    std::string txt = _u8L("Filament");
                    txt += (m_data->m_extruder_ids.size() == 1) ? ":" : " " + std::to_string(i + 1);
                    imgui.text(txt);
                    ImGui::SameLine(offset);
                    imgui.text(m_data->m_settings_ids.filament[i]);
                }
            }
        }
    }
    // Custom g-code overview
    std::vector<CustomGCode::Item> custom_gcode_per_print_z = AppAdapter::plater()->model().get_curr_plate_custom_gcodes().gcodes;
    if (custom_gcode_per_print_z.size() != 0) {
        float max_len = window_padding + 2 * ImGui::GetStyle().ItemSpacing.x;
        ImGui::Spacing();
        // Title Line
        std::string cgcode_title_str       = _u8L("Custom g-code");
        std::string cgcode_layer_str       = _u8L("Layer");
        std::string cgcode_time_str        =  _u8L("Time");
        // Types of custom gcode
        std::string cgcode_pause_str = _u8L("Pause");
        std::string cgcode_template_str= _u8L("Template");
        std::string cgcode_toolchange_str = _u8L("ToolChange");
        std::string cgcode_custom_str = _u8L("Custom");
        std::string cgcode_unknown_str = _u8L("Unknown");

        // Get longest String
        max_len += std::max(ImGui::CalcTextSize(cgcode_title_str.c_str()).x,
                                              std::max(ImGui::CalcTextSize(cgcode_pause_str.c_str()).x,
                                                       std::max(ImGui::CalcTextSize(cgcode_template_str.c_str()).x,
                                                                std::max(ImGui::CalcTextSize(cgcode_toolchange_str.c_str()).x,
                                                                         std::max(ImGui::CalcTextSize(cgcode_custom_str.c_str()).x,
                                                                                  ImGui::CalcTextSize(cgcode_unknown_str.c_str()).x))))

        );
       
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * 0.1));
        ImGui::Dummy({window_padding, window_padding});
        ImGui::SameLine();
        imgui.title(cgcode_title_str,true);
        ImGui::SameLine(max_len);
        imgui.title(cgcode_layer_str, true);
        ImGui::SameLine(max_len*1.5);
        imgui.title(cgcode_time_str, false);

        for (Slic3r::CustomGCode::Item custom_gcode : custom_gcode_per_print_z) {
            ImGui::Dummy({window_padding, window_padding});
            ImGui::SameLine();

            switch (custom_gcode.type) {
            case PausePrint: imgui.text(cgcode_pause_str); break;
            case Template: imgui.text(cgcode_template_str); break;
            case ToolChange: imgui.text(cgcode_toolchange_str); break;
            case Custom: imgui.text(cgcode_custom_str); break;
            default: imgui.text(cgcode_unknown_str); break;
            }
            ImGui::SameLine(max_len);
            char buf[64];
            int  layer = m_data->m_layers.get_l_at(custom_gcode.print_z);
            ::sprintf(buf, "%d",layer );
            imgui.text(buf);
            ImGui::SameLine(max_len * 1.5);
            
            std::vector<float> layer_times = m_data->m_print_statistics.modes[static_cast<size_t>(PrintEstimatedStatistics::ETimeMode::Normal)].layers_times;
            float custom_gcode_time = 0;
            if (layer > 0)
            {
                for (int i = 0; i < layer-1; i++) {
                    custom_gcode_time += layer_times[i];
                }
            }
            imgui.text(short_time(get_time_dhms(custom_gcode_time)));

        }
    }


    // total estimated printing time section
    if (show_estimated) {
        ImGui::Spacing();
        std::string time_title = m_data->m_view_type == EViewType::FeatureType ? _u8L("Total Estimation") : _u8L("Time Estimation");
        auto can_show_mode_button = [this](PrintEstimatedStatistics::ETimeMode mode) {
            bool show = false;
            if (m_data->m_print_statistics.modes.size() > 1 && m_data->m_print_statistics.modes[static_cast<size_t>(mode)].roles_times.size() > 0) {
                for (size_t i = 0; i < m_data->m_print_statistics.modes.size(); ++i) {
                    if (i != static_cast<size_t>(mode) &&
                        m_data->m_print_statistics.modes[i].time > 0.0f &&
                        short_time(get_time_dhms(m_data->m_print_statistics.modes[static_cast<size_t>(mode)].time)) != short_time(get_time_dhms(m_data->m_print_statistics.modes[i].time))) {
                        show = true;
                        break;
                    }
                }
            }
            return show;
        };

        if (can_show_mode_button(m_data->m_time_estimate_mode)) {
            switch (m_data->m_time_estimate_mode)
            {
            case PrintEstimatedStatistics::ETimeMode::Normal: { time_title += " [" + _u8L("Normal mode") + "]"; break; }
            default: { assert(false); break; }
            }
        }
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * 0.1));
        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        imgui.title(time_title);
        std::string total_filament_str = _u8L("Total Filament");
        std::string model_filament_str = _u8L("Model Filament");
        std::string cost_str = _u8L("Cost");
        std::string prepare_str = _u8L("Prepare time");
        std::string print_str = _u8L("Model printing time");
        std::string total_str = _u8L("Total time");

        float max_len = window_padding + 2 * ImGui::GetStyle().ItemSpacing.x;
        if (time_mode.layers_times.empty())
            max_len += ImGui::CalcTextSize(total_str.c_str()).x;
        else {
            if (m_data->m_view_type == EViewType::FeatureType)
                max_len += std::max(ImGui::CalcTextSize(cost_str.c_str()).x,
                    std::max(ImGui::CalcTextSize(print_str.c_str()).x,
                        std::max(std::max(ImGui::CalcTextSize(prepare_str.c_str()).x, ImGui::CalcTextSize(total_str.c_str()).x),
                            std::max(ImGui::CalcTextSize(total_filament_str.c_str()).x, ImGui::CalcTextSize(model_filament_str.c_str()).x))));
            else
                max_len += std::max(ImGui::CalcTextSize(print_str.c_str()).x,
                    (std::max(ImGui::CalcTextSize(prepare_str.c_str()).x, ImGui::CalcTextSize(total_str.c_str()).x)));
        }

        if (m_data->m_view_type == EViewType::FeatureType) {
            //BBS display filament cost
            ImGui::Dummy({ window_padding, window_padding });
            ImGui::SameLine();
            imgui.text(total_filament_str + ":");
            ImGui::SameLine(max_len);
            //BBS: use current plater's print statistics
            bool imperial_units = AppAdapter::app_config()->get("use_inches") == "1";
            char buf[64];
            ::sprintf(buf, imperial_units ? "%.2f in" : "%.2f m", ps.total_used_filament / koef);
            imgui.text(buf);
            ImGui::SameLine();
            ::sprintf(buf, imperial_units ? "  %.2f oz" : "  %.2f g", ps.total_weight / unit_conver);
            imgui.text(buf);

            ImGui::Dummy({ window_padding, window_padding });
            ImGui::SameLine();
            imgui.text(model_filament_str + ":");
            ImGui::SameLine(max_len);
            auto exlude_m = total_support_used_filament_m + total_flushed_filament_m + total_wipe_tower_used_filament_m;
            auto exlude_g = total_support_used_filament_g + total_flushed_filament_g + total_wipe_tower_used_filament_g;
            ::sprintf(buf, imperial_units ? "%.2f in" : "%.2f m", ps.total_used_filament / koef - exlude_m);
            imgui.text(buf);
            ImGui::SameLine();
            ::sprintf(buf, imperial_units ? "  %.2f oz" : "  %.2f g", (ps.total_weight - exlude_g) / unit_conver);
            imgui.text(buf);

            //BBS: display cost of filaments
            ImGui::Dummy({ window_padding, window_padding });
            ImGui::SameLine();
            imgui.text(cost_str + ":");
            ImGui::SameLine(max_len);
            ::sprintf(buf, "%.2f", ps.total_cost);
            imgui.text(buf);
        }

            auto role_time = [time_mode](ExtrusionRole role) {
            auto it = std::find_if(time_mode.roles_times.begin(), time_mode.roles_times.end(), [role](const std::pair<ExtrusionRole, float>& item) { return role == item.first; });
                return (it != time_mode.roles_times.end()) ? it->second : 0.0f;
            };
        //BBS: start gcode is mostly same with prepeare time
        if (time_mode.prepare_time != 0.0f) {
            ImGui::Dummy({ window_padding, window_padding });
            ImGui::SameLine();
            imgui.text(prepare_str + ":");
            ImGui::SameLine(max_len);
            imgui.text(short_time(get_time_dhms(time_mode.prepare_time)));
        }
        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        imgui.text(print_str + ":");
        ImGui::SameLine(max_len);
        imgui.text(short_time(get_time_dhms(time_mode.time - time_mode.prepare_time)));
        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        imgui.text(total_str + ":");
        ImGui::SameLine(max_len);
        imgui.text(short_time(get_time_dhms(time_mode.time)));

        auto show_mode_button = [this, &imgui, can_show_mode_button](const wxString& label, PrintEstimatedStatistics::ETimeMode mode) {
            if (can_show_mode_button(mode)) {
                if (imgui.button(label)) {
                    m_data->m_time_estimate_mode = mode;
                    imgui.set_requires_extra_frame();
                }
            }
        };

        switch (m_data->m_time_estimate_mode) {
        case PrintEstimatedStatistics::ETimeMode::Normal: {
            show_mode_button(_L("Switch to silent mode"), PrintEstimatedStatistics::ETimeMode::Stealth);
            break;
        }
        case PrintEstimatedStatistics::ETimeMode::Stealth: {
            show_mode_button(_L("Switch to normal mode"), PrintEstimatedStatistics::ETimeMode::Normal);
            break;
        }
        default : { assert(false); break; }
        }
    }

    if (m_data->m_view_type == EViewType::ColorPrint) {
        ImGui::Spacing();
        ImGui::Dummy({ window_padding, window_padding });
        ImGui::SameLine();
        offsets = calculate_offsets({ { _u8L("Options"), { ""}}, { _u8L("Display"), {""}} }, icon_size);
        append_headers({ {_u8L("Options"), offsets[0] }, { _u8L("Display"), offsets[1]} });
        for (auto item : m_data->options_items)
            append_option_item(item, offsets);
    }

    legend_height = ImGui::GetCurrentWindow()->Size.y;
    ImGui::Dummy({ window_padding, window_padding});
    imgui.end();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(2);

}

void GCodePanel::push_combo_style()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0,8.0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_BorderActive, ImVec4(0.00f, 0.59f, 0.53f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.00f, 0.59f, 0.53f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.00f, 0.59f, 0.53f, 1.0f));
}
void GCodePanel::pop_combo_style()
{
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(8);
}

void GCodePanel::render_sliders(int canvas_width, int canvas_height)
{
    m_moves_slider->render(canvas_width, canvas_height);
    m_layers_slider->render(canvas_width, canvas_height);
}

void GCodePanel::enable_moves_slider(bool enable) const
{
    bool render_as_disabled = !enable;
    if (m_moves_slider != nullptr && m_moves_slider->is_rendering_as_disabled() != render_as_disabled) {
       m_moves_slider->set_render_as_disabled(render_as_disabled);
       m_moves_slider->set_as_dirty();
    }
}

void GCodePanel::update_moves_slider(bool set_to_max)
{
    const GCodeViewerState &view = m_data->m_sequential_view;
    if (view.gcode_ids.empty())
        return;
    //// this should not be needed, but it is here to try to prevent rambling crashes on Mac Asan
    if (view.endpoints.last < view.endpoints.first) return;

    std::vector<double> values(view.endpoints.last - view.endpoints.first + 1);
    std::vector<double> alternate_values(view.endpoints.last - view.endpoints.first + 1);
    unsigned int        count = 0;
    for (unsigned int i = view.endpoints.first; i <= view.endpoints.last; ++i) {
       values[count] = static_cast<double>(i + 1);
       if (view.gcode_ids[i] > 0) alternate_values[count] = static_cast<double>(view.gcode_ids[i]);
       ++count;
    }

    bool keep_min = m_moves_slider->GetActiveValue() == m_moves_slider->GetMinValue();

    m_moves_slider->SetSliderValues(values);
    m_moves_slider->SetSliderAlternateValues(alternate_values);
    m_moves_slider->SetMaxValue(view.endpoints.last - view.endpoints.first);
    m_moves_slider->SetSelectionSpan(view.current.first - view.endpoints.first, view.current.last - view.endpoints.first);
    if (set_to_max)
       m_moves_slider->SetHigherValue(keep_min ? m_moves_slider->GetMinValue() : m_moves_slider->GetMaxValue());
}

void GCodePanel::update_layers_slider_mode()
{
    //    true  -> single-extruder printer profile OR
    //             multi-extruder printer profile , but whole model is printed by only one extruder
    //    false -> multi-extruder printer profile , and model is printed by several extruders
    bool one_extruder_printed_model = true;

    // extruder used for whole model for multi-extruder printer profile
    int only_extruder = -1;

    // BBS
    if (preset_filaments_cnt() > 1) {
        const ModelObjectPtrs &objects = AppAdapter::plater()->model().objects;

        // check if whole model uses just only one extruder
        if (!objects.empty()) {
            const int extruder = objects[0]->config.has("extruder") ? objects[0]->config.option("extruder")->getInt() : 0;

            auto is_one_extruder_printed_model = [objects, extruder]() {
                for (ModelObject *object : objects) {
                    if (object->config.has("extruder") && object->config.option("extruder")->getInt() != extruder) return false;

                    for (ModelVolume *volume : object->volumes)
                        if ((volume->config.has("extruder") && volume->config.option("extruder")->getInt() != extruder) || !volume->mmu_segmentation_facets.empty()) return false;

                    for (const auto &range : object->layer_config_ranges)
                        if (range.second.has("extruder") && range.second.option("extruder")->getInt() != extruder) return false;
                }
                return true;
            };

            if (is_one_extruder_printed_model())
                only_extruder = extruder;
            else
                one_extruder_printed_model = false;
        }
    }

    // TODO m_gcode_viewer_data->m_layers_slider->SetModeAndOnlyExtruder(one_extruder_printed_model, only_extruder);
}

};
};
};
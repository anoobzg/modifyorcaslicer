#include "Plater.hpp"
#include "PlaterPrivate.hpp"

#include "slic3r/Scene/ObjectDataViewModel.hpp"

#include "slic3r/GUI/Config/GUI_ObjectSettings.hpp"
#include "slic3r/GUI/Config/GUI_ObjectLayers.hpp"

#include "slic3r/Slice/GCodeResultWrapper.hpp"
#include "slic3r/Slice/SlicingProcessCompletedEvent.hpp"

#include "slic3r/GUI/Frame/View3D.hpp"
#include "slic3r/GUI/Frame/GCodePreview.hpp"
#include "slic3r/GUI/Config/PrinterPresetDialog.hpp"
#include "slic3r/Config/AppConfig.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"
#include "slic3r/Slice/GCodeImportExporter.hpp"
#include "slic3r/Net/Http.hpp"

#include "slic3r/Render/GCodePreviewCanvas.hpp"
#include "slic3r/Render/GLCanvas3DFacade.hpp"
#include "slic3r/Render/PlateBed.hpp"

// Multi-Nozzle Calibration feature
#include "libslic3r/Config/CalibrationConfig.hpp"
#include "libslic3r/Point.hpp"
#include "fullcontrol/multi_nozzle_calibration/multi_nozzle_calibration.h"
#include <fstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "libslic3r/GCode/GCodeProcessor.hpp"

#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/MainPanel.hpp"

static const std::pair<unsigned int, unsigned int> THUMBNAIL_SIZE_3MF = { 512, 512 };

namespace Slic3r {
namespace GUI {

void Plater::show_illegal_characters_warning(wxWindow* parent)
{
    show_error(parent, _L("Invalid name, the following characters are not allowed:") + " <>:/\\|?*\"");
}

enum SlicedInfoIdx
{
    siFilament_m,
    siFilament_mm3,
    siFilament_g,
    siMateril_unit,
    siCost,
    siEstimatedTime,
    siWTNumbetOfToolchanges,
    siCount
};

enum class LoadFilesType {
    NoFile,
    Single3MF,
    SingleOther,
    Multiple3MF,
    MultipleOther,
    Multiple3MFOther,
};

class SlicedInfo : public wxStaticBoxSizer
{
public:
    SlicedInfo(wxWindow *parent);
    void SetTextAndShow(SlicedInfoIdx idx, const wxString& text, const wxString& new_label="");

private:
    std::vector<std::pair<wxStaticText*, wxStaticText*>> info_vec;
};

SlicedInfo::SlicedInfo(wxWindow *parent) :
    wxStaticBoxSizer(new wxStaticBox(parent, wxID_ANY, _L("Sliced Info")), wxVERTICAL)
{
    GetStaticBox()->SetFont(app_bold_font());
    UpdateDarkUI(GetStaticBox());

    auto *grid_sizer = new wxFlexGridSizer(2, 5, 15);
    grid_sizer->SetFlexibleDirection(wxVERTICAL);

    info_vec.reserve(siCount);

    auto init_info_label = [this, parent, grid_sizer](wxString text_label) {
        auto *text = new wxStaticText(parent, wxID_ANY, text_label);
        text->SetForegroundColour(*wxBLACK);
        text->SetFont(app_small_font());
        auto info_label = new wxStaticText(parent, wxID_ANY, "N/A");
        info_label->SetForegroundColour(*wxBLACK);
        info_label->SetFont(app_small_font());
        grid_sizer->Add(text, 0);
        grid_sizer->Add(info_label, 0);
        info_vec.push_back(std::pair<wxStaticText*, wxStaticText*>(text, info_label));
    };

    init_info_label(_L("Used Filament (m)"));
    init_info_label(_L("Used Filament (mm³)"));
    init_info_label(_L("Used Filament (g)"));
    init_info_label(_L("Used Materials"));
    init_info_label(_L("Cost"));
    init_info_label(_L("Estimated time"));
    init_info_label(_L("Filament changes"));

    Add(grid_sizer, 0, wxEXPAND);
    this->Show(false);
}

void SlicedInfo::SetTextAndShow(SlicedInfoIdx idx, const wxString& text, const wxString& new_label/*=""*/)
{
    const bool show = text != "N/A";
    if (show)
        info_vec[idx].second->SetLabelText(text);
    if (!new_label.IsEmpty())
        info_vec[idx].first->SetLabelText(new_label);
    info_vec[idx].first->Show(show);
    info_vec[idx].second->Show(show);
}

static wxString temp_dir;

// Sidebar / private

enum class ActionButtonType : int {
    abReslice,
    abExport,
    abSendGCode
};

int SidebarProps::TitlebarMargin() { return 8; }  // Use as side margins on titlebar. Has less margin on sides to create separation with its content
int SidebarProps::ContentMargin()  { return 12; } // Use as side margins contents of title
int SidebarProps::IconSpacing()    { return 10; } // Use on main elements
int SidebarProps::ElementSpacing() { return 5; }  // Use if elements has relation between them like edit button for combo box etc.

#ifdef _WIN32
using wxRichToolTipPopup = wxCustomBackgroundWindow<wxPopupTransientWindow>;
static wxRichToolTipPopup* get_rtt_popup(wxButton* btn)
{
    auto children = btn->GetChildren();
    for (auto child : children)
        if (child->IsShown())
            return dynamic_cast<wxRichToolTipPopup*>(child);
    return nullptr;
}

void Sidebar::priv::show_rich_tip(const wxString& tooltip, wxButton* btn)
{
    if (tooltip.IsEmpty())
        return;
    wxRichToolTip tip(tooltip, "");
    tip.SetIcon(wxICON_NONE);
    tip.SetTipKind(wxTipKind_BottomRight);
    tip.SetTitleFont(app_normal_font());
    tip.SetBackgroundColour(get_window_default_clr());

    tip.ShowFor(btn);
    // Every call of the ShowFor() creates new RichToolTip and show it.
    // Every one else are hidden.
    // So, set a text color just for the shown rich tooltip
    if (wxRichToolTipPopup* popup = get_rtt_popup(btn)) {
        auto children = popup->GetChildren();
        for (auto child : children) {
            child->SetForegroundColour(get_label_clr_default());
            // we neen just first text line for out rich tooltip
            return;
        }
    }
}

void Sidebar::priv::hide_rich_tip(wxButton* btn)
{
    if (wxRichToolTipPopup* popup = get_rtt_popup(btn))
        popup->Dismiss();
}
#endif

// Sidebar / public

struct DynamicFilamentList : DynamicList
{
    std::vector<std::pair<wxString, wxBitmap *>> items;

    void apply_on(Choice *c) override
    {
        if (items.empty())
            update(true);
        auto cb = dynamic_cast<ComboBox *>(c->window);
        auto n  = cb->GetSelection();
        cb->Clear();
        cb->Append(_L("Default"));
        for (auto i : items) {
            cb->Append(i.first, *i.second);
        }
        if (n < cb->GetCount())
            cb->SetSelection(n);
    }
    wxString get_value(int index) override
    {
        wxString str;
        str << index;
        return str;
    }
    int index_of(wxString value) override
    {
        long n = 0;
        return (value.ToLong(&n) && n <= items.size()) ? int(n) : -1;
    }
    void update(bool force = false)
    {
        items.clear();
        if (!force && m_choices.empty())
            return;
        auto icons = get_extruder_color_icons(true);
        auto presets = app_preset_bundle()->filament_presets;
        for (int i = 0; i < presets.size(); ++i) {
            wxString str;
            std::string type;
            app_preset_bundle()->filaments.find_preset(presets[i])->get_filament_type(type);
            str << type;
            items.push_back({str, icons[i]});
        }
        DynamicList::update();
    }
};

struct DynamicFilamentList1Based : DynamicFilamentList
{
    void apply_on(Choice *c) override
    {
        if (items.empty())
            update(true);
        auto cb = dynamic_cast<ComboBox *>(c->window);
        auto n  = cb->GetSelection();
        cb->Clear();
        for (auto i : items) {
            cb->Append(i.first, *i.second);
        }
        if (n < cb->GetCount())
            cb->SetSelection(n);
    }
    wxString get_value(int index) override
    {
        wxString str;
        str << index+1;
        return str;
    }
    int index_of(wxString value) override
    {
        long n = 0;
        if(!value.ToLong(&n))
            return -1;
        --n;
        return (n >= 0 && n <= items.size()) ? int(n) : -1;
    }
    void update(bool force = false)
    {
        items.clear();
        if (!force && m_choices.empty())
            return;
        auto icons = get_extruder_color_icons(true);
        auto presets = app_preset_bundle()->filament_presets;
        for (int i = 0; i < presets.size(); ++i) {
            wxString str;
            std::string type;
            app_preset_bundle()->filaments.find_preset(presets[i])->get_filament_type(type);
            str << type;
            items.push_back({str, icons[i]});
        }
        DynamicList::update();
    }

};


static DynamicFilamentList dynamic_filament_list;
static DynamicFilamentList1Based dynamic_filament_list_1_based;

Sidebar::Sidebar(Plater *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(42 * app_em_unit(), -1)), p(new priv(parent))
{
    Choice::register_dynamic_list("support_filament", &dynamic_filament_list);
    Choice::register_dynamic_list("support_interface_filament", &dynamic_filament_list);
    Choice::register_dynamic_list("wall_filament", &dynamic_filament_list_1_based);
    Choice::register_dynamic_list("sparse_infill_filament", &dynamic_filament_list_1_based);
    Choice::register_dynamic_list("solid_infill_filament", &dynamic_filament_list_1_based);
    Choice::register_dynamic_list("wipe_tower_filament", &dynamic_filament_list);

    p->scrolled = new wxPanel(this);
    p->scrolled->SetBackgroundColour(*wxBLACK);

    SetFont(app_normal_font());
#ifndef __APPLE__
#ifdef _WIN32
    UpdateDarkUI(this);
    UpdateDarkUI(p->scrolled);
#else
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif
#endif

    int em = app_em_unit();
    //BBS refine layout and styles
    // Sizer in the scrolled area
    auto* scrolled_sizer = m_scrolled_sizer = new wxBoxSizer(wxVERTICAL);
    p->scrolled->SetSizer(scrolled_sizer);

    wxColour title_bg = wxColour(248, 248, 248);
    wxColour inactive_text = wxColour(86, 86, 86);
    wxColour active_text = wxColour(0, 0, 0);
    wxColour static_line_col = wxColour(166, 169, 170);

#ifdef __WINDOWS__
    p->scrolled->SetDoubleBuffered(true);
#endif //__WINDOWS__

    // add printer
    {
        /***************** 1. create printer title bar    **************/
        // 1.1 create title bar resources
        p->m_panel_printer_title = new StaticBox(p->scrolled, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_NONE);
        p->m_panel_printer_title->SetBackgroundColor(title_bg);
        p->m_panel_printer_title->SetBackgroundColor2(0xF1F1F1);

        p->m_printer_icon = new ScalableButton(p->m_panel_printer_title, wxID_ANY, "printer");
        p->m_text_printer_settings = new Label(p->m_panel_printer_title, _L("Printer"), LB_PROPAGATE_MOUSE_EVENT);

        p->m_printer_icon->Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
            });


        p->m_printer_setting = new ScalableButton(p->m_panel_printer_title, wxID_ANY, "settings");
        p->m_printer_setting->Bind(wxEVT_BUTTON, [this](wxCommandEvent &e) {
            AppAdapter::gui_app()->run_wizard(ConfigWizard::RR_USER, ConfigWizard::SP_PRINTERS);
            });

        wxBoxSizer* h_sizer_title = new wxBoxSizer(wxHORIZONTAL);
        h_sizer_title->Add(p->m_printer_icon, 0, wxALIGN_CENTRE | wxLEFT, FromDIP(SidebarProps::TitlebarMargin()));
        h_sizer_title->AddSpacer(FromDIP(SidebarProps::ElementSpacing()));
        h_sizer_title->Add(p->m_text_printer_settings, 0, wxALIGN_CENTER);
        h_sizer_title->AddStretchSpacer();
        h_sizer_title->Add(p->m_printer_setting, 0, wxALIGN_CENTER);
        h_sizer_title->AddSpacer(FromDIP(SidebarProps::TitlebarMargin()));
        h_sizer_title->SetMinSize(-1, 3 * em);

        p->m_panel_printer_title->SetSizer(h_sizer_title);
        p->m_panel_printer_title->Layout();

        // add printer title
        scrolled_sizer->Add(p->m_panel_printer_title, 0, wxEXPAND | wxALL, 0);
        p->m_panel_printer_title->Bind(wxEVT_LEFT_UP, [this] (auto & e) {
            if (p->m_panel_printer_content->GetMaxHeight() == 0)
                p->m_panel_printer_content->SetMaxSize({-1, -1});
            else
                p->m_panel_printer_content->SetMaxSize({-1, 0});
            m_scrolled_sizer->Layout();
        });

        // add spliter 2
        auto spliter_2 = new StaticLine(p->scrolled);
        spliter_2->SetLineColour("#CECECE");
        scrolled_sizer->Add(spliter_2, 0, wxEXPAND);


        /*************************** 2. add printer content ************************/
        p->m_panel_printer_content = new wxPanel(p->scrolled, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        p->m_panel_printer_content->SetBackgroundColour(wxColour(255, 255, 255));

        PlaterPrinterPresetComboBox* combo_printer = new PlaterPrinterPresetComboBox(p->m_panel_printer_content);
        ScalableButton* edit_btn = new ScalableButton(p->m_panel_printer_content, wxID_ANY, "edit");
        edit_btn->SetToolTip(_L("Click to edit preset"));
        edit_btn->Bind(wxEVT_BUTTON, [this, combo_printer](wxCommandEvent){
                p->combo_printer->switch_to_tab();
                // PrinterPresetDialog dlg(AppAdapter::main_panel());
                // dlg.Popup();
            });
        // combo_printer->edit_btn = edit_btn;
        p->combo_printer = combo_printer;

        wxBoxSizer* vsizer_printer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* hsizer_printer = new wxBoxSizer(wxHORIZONTAL);

        vsizer_printer->AddSpacer(FromDIP(16));
        hsizer_printer->Add(combo_printer, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(SidebarProps::ContentMargin()));
        hsizer_printer->Add(edit_btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(SidebarProps::ElementSpacing()));
        hsizer_printer->AddSpacer(FromDIP(SidebarProps::ContentMargin()));
        vsizer_printer->Add(hsizer_printer, 0, wxEXPAND, 0);

        // Bed type selection
        wxBoxSizer* bed_type_sizer = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText* bed_type_title = new wxStaticText(p->m_panel_printer_content, wxID_ANY, _L("Bed type"));
        //bed_type_title->SetBackgroundColour();
        bed_type_title->Wrap(-1);
        bed_type_title->SetFont(Font::Body_14);
        m_bed_type_list = new ComboBox(p->m_panel_printer_content, wxID_ANY, wxString(""), wxDefaultPosition, {-1, FromDIP(30)}, 0, nullptr, wxCB_READONLY);
        const ConfigOptionDef* bed_type_def = print_config_def.get("curr_bed_type");
        if (bed_type_def && bed_type_def->enum_keys_map) {
            for (auto item : bed_type_def->enum_labels) {
                m_bed_type_list->AppendString(_L(item));
            }
        }

        bed_type_title->Bind(wxEVT_ENTER_WINDOW, [bed_type_title, this](wxMouseEvent &e) {
            e.Skip();
            auto font = bed_type_title->GetFont();
            font.SetUnderlined(true);
            bed_type_title->SetFont(font);
            SetCursor(wxCURSOR_HAND);
        });
        bed_type_title->Bind(wxEVT_LEAVE_WINDOW, [bed_type_title, this](wxMouseEvent &e) {
            e.Skip();
            auto font = bed_type_title->GetFont();
            font.SetUnderlined(false);
            bed_type_title->SetFont(font);
            SetCursor(wxCURSOR_ARROW);
        });
        bed_type_title->Bind(wxEVT_LEFT_UP, [bed_type_title, this](wxMouseEvent &e) {
            wxLaunchDefaultBrowser("https://github.com/SoftFever/OrcaSlicer/wiki/bed-types");
        });

        AppConfig *app_config = AppAdapter::app_config();
        std::string str_bed_type = app_config->get("curr_bed_type");
        int bed_type_value = atoi(str_bed_type.c_str());
        // hotfix: btDefault is added as the first one in BedType, and app_config should not be btDefault
        if (bed_type_value == 0) {
            app_config->set("curr_bed_type", "1");
            bed_type_value = 1;
        }

        int bed_type_idx = bed_type_value - 1;
        m_bed_type_list->Select(bed_type_idx);
        bed_type_sizer->Add(bed_type_title, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(SidebarProps::ContentMargin()));
        bed_type_sizer->Add(m_bed_type_list, 1, wxLEFT | wxEXPAND, FromDIP(SidebarProps::ElementSpacing()));
        bed_type_sizer->AddSpacer(FromDIP(SidebarProps::ContentMargin()));
        vsizer_printer->Add(bed_type_sizer, 0, wxEXPAND | wxTOP, FromDIP(5));
        vsizer_printer->AddSpacer(FromDIP(16));

        auto& project_config = app_preset_bundle()->project_config;
        BedType bed_type = (BedType)bed_type_value;
        project_config.set_key_value("curr_bed_type", new ConfigOptionEnum<BedType>(bed_type));

        p->m_panel_printer_content->SetSizer(vsizer_printer);
        p->m_panel_printer_content->Layout();
        scrolled_sizer->Add(p->m_panel_printer_content, 0, wxEXPAND, 0);
    }

    {
        // add filament title
        p->m_panel_filament_title = new StaticBox(p->scrolled, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_NONE);
        p->m_panel_filament_title->SetBackgroundColor(title_bg);
        p->m_panel_filament_title->SetBackgroundColor2(0xF1F1F1);
        p->m_panel_filament_title->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &e) {
            if (e.GetPosition().x > (p->m_flushing_volume_btn->IsShown()
                    ? p->m_flushing_volume_btn->GetPosition().x : (p->m_bpButton_add_filament->GetPosition().x - FromDIP(30)))) // ORCA exclude area of del button from titlebar collapse/expand feature to fix undesired collapse when user spams del filament button 
                return;
            if (p->m_panel_filament_content->GetMaxHeight() == 0)
                p->m_panel_filament_content->SetMaxSize({-1, -1});
            else
                p->m_panel_filament_content->SetMaxSize({-1, 0});
            m_scrolled_sizer->Layout();
        });

        wxBoxSizer* bSizer39;
        bSizer39 = new wxBoxSizer( wxHORIZONTAL );
        p->m_filament_icon = new ScalableButton(p->m_panel_filament_title, wxID_ANY, "filament");
        p->m_staticText_filament_settings = new Label(p->m_panel_filament_title, _L("Filament"), LB_PROPAGATE_MOUSE_EVENT);
        bSizer39->Add(p->m_filament_icon, 0, wxALIGN_CENTER | wxLEFT, FromDIP(SidebarProps::TitlebarMargin()));
        bSizer39->AddSpacer(FromDIP(SidebarProps::ElementSpacing()));
        bSizer39->Add( p->m_staticText_filament_settings, 0, wxALIGN_CENTER );
        bSizer39->Add(FromDIP(10), 0, 0, 0, 0);
        bSizer39->SetMinSize(-1, FromDIP(30));

        p->m_panel_filament_title->SetSizer( bSizer39 );
        p->m_panel_filament_title->Layout();
        auto spliter_1 = new StaticLine(p->scrolled);
        spliter_1->SetLineColour("#A6A9AA");
        scrolled_sizer->Add(spliter_1, 0, wxEXPAND);
        scrolled_sizer->Add(p->m_panel_filament_title, 0, wxEXPAND | wxALL, 0);
        auto spliter_2 = new StaticLine(p->scrolled);
        spliter_2->SetLineColour("#CECECE");
        scrolled_sizer->Add(spliter_2, 0, wxEXPAND);

        bSizer39->AddStretchSpacer(1);

        // BBS
        // add wiping dialog
        //wiping_dialog_button->SetFont(app_normal_font());
        p->m_flushing_volume_btn = new Button(p->m_panel_filament_title, _L("Flushing volumes"));
        p->m_flushing_volume_btn->SetFont(Font::Body_10);
        p->m_flushing_volume_btn->SetPaddingSize(wxSize(FromDIP(8),FromDIP(3)));
        p->m_flushing_volume_btn->SetCornerRadius(FromDIP(8));

        StateColor flush_bg_col(std::pair<wxColour, int>(wxColour("#BFE1DE"), StateColor::Pressed), // ORCA
                                std::pair<wxColour, int>(wxColour(238, 238, 238), StateColor::Hovered),
                                std::pair<wxColour, int>(wxColour(238, 238, 238), StateColor::Normal));

        StateColor flush_fg_col(std::pair<wxColour, int>(wxColour(107, 107, 106), StateColor::Pressed),
                                std::pair<wxColour, int>(wxColour(107, 107, 106), StateColor::Hovered),
                                std::pair<wxColour, int>(wxColour(107, 107, 106), StateColor::Normal));

        StateColor flush_bd_col(std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Pressed),
                                std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Hovered),
                                std::pair<wxColour, int>(wxColour(172, 172, 172), StateColor::Normal));

        p->m_flushing_volume_btn->SetBackgroundColor(flush_bg_col);
        p->m_flushing_volume_btn->SetBorderColor(flush_bd_col);
        p->m_flushing_volume_btn->SetTextColor(flush_fg_col);
        p->m_flushing_volume_btn->SetFocus();
        p->m_flushing_volume_btn->SetId(wxID_RESET);
        p->m_flushing_volume_btn->Rescale();

        p->m_flushing_volume_btn->Bind(wxEVT_BUTTON, ([parent](wxCommandEvent &e)
            {
                auto& project_config = app_preset_bundle()->project_config;
                const std::vector<double>& init_matrix = (project_config.option<ConfigOptionFloats>("flush_volumes_matrix"))->values;
                const std::vector<double>& init_extruders = (project_config.option<ConfigOptionFloats>("flush_volumes_vector"))->values;
                ConfigOptionFloat* flush_multi_opt = project_config.option<ConfigOptionFloat>("flush_multiplier");
                float flush_multiplier = flush_multi_opt ? flush_multi_opt->getFloat() : 1.f;

                const std::vector<std::string> extruder_colours = AppAdapter::plater()->get_extruder_colors_from_plater_config();
                const auto& full_config = app_preset_bundle()->full_config();
                const auto& extra_flush_volumes = get_min_flush_volumes(full_config);
                WipingDialog dlg(parent, cast<float>(init_matrix), cast<float>(init_extruders), extruder_colours, extra_flush_volumes, flush_multiplier);
                if (dlg.ShowModal() == wxID_OK) {
                    std::vector<float> matrix = dlg.get_matrix();
                    std::vector<float> extruders = dlg.get_extruders();
                    (project_config.option<ConfigOptionFloats>("flush_volumes_matrix"))->values = std::vector<double>(matrix.begin(), matrix.end());
                    (project_config.option<ConfigOptionFloats>("flush_volumes_vector"))->values = std::vector<double>(extruders.begin(), extruders.end());
                    (project_config.option<ConfigOptionFloat>("flush_multiplier"))->set(new ConfigOptionFloat(dlg.get_flush_multiplier()));

                    app_preset_bundle()->export_selections(*AppAdapter::app_config());

                    AppAdapter::plater()->update_project_dirty_from_presets();
                    wxPostEvent(parent, SimpleEvent(EVT_SCHEDULE_BACKGROUND_PROCESS, parent));
                }
            }));

        bSizer39->Add(p->m_flushing_volume_btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(5));
        bSizer39->Hide(p->m_flushing_volume_btn);

        ScalableButton* add_btn = new ScalableButton(p->m_panel_filament_title, wxID_ANY, "add_filament");
        add_btn->SetToolTip(_L("Add one filament"));
        add_btn->Bind(wxEVT_BUTTON, [this, scrolled_sizer](wxCommandEvent& e){
            // Orca: limit filament choices to MAXIMUM_EXTRUDER_NUMBER
            if (p->combos_filament.size() >= MAXIMUM_EXTRUDER_NUMBER)
                return;

            int filament_count = p->combos_filament.size() + 1;
            wxColour new_col = Plater::get_next_color_for_filament();
            std::string new_color = new_col.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
            app_preset_bundle()->set_num_filaments(filament_count, new_color);
            AppAdapter::plater()->on_filaments_change(filament_count);
            AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update();
            app_preset_bundle()->export_selections(*AppAdapter::app_config());
            auto_calc_flushing_volumes(filament_count - 1);
        });
        p->m_bpButton_add_filament = add_btn;

        // ORCA Moved add button after delete button to prevent add button position change when remove icon automatically hidden

        ScalableButton* del_btn = new ScalableButton(p->m_panel_filament_title, wxID_ANY, "delete_filament");
        del_btn->SetToolTip(_L("Remove last filament"));
        del_btn->Bind(wxEVT_BUTTON, [this, scrolled_sizer](wxCommandEvent &e) {
            if (p->combos_filament.size() <= 1)
                return;

            size_t filament_count = p->combos_filament.size() - 1;
            if (app_preset_bundle()->is_the_only_edited_filament(filament_count) || (filament_count == 1)) {
                AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->select_preset(app_preset_bundle()->filament_presets[0], false, "", true);
            }

            if (p->editing_filament >= filament_count) {
                p->editing_filament = -1;
            }

            app_preset_bundle()->set_num_filaments(filament_count);
            AppAdapter::plater()->on_filaments_change(filament_count);
            AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update();
            app_preset_bundle()->export_selections(*AppAdapter::app_config());
        });
        p->m_bpButton_del_filament = del_btn;

        bSizer39->Add(del_btn, 0, wxALIGN_CENTER | wxLEFT, FromDIP(SidebarProps::IconSpacing()));
        bSizer39->Add(add_btn, 0, wxALIGN_CENTER | wxLEFT, FromDIP(SidebarProps::IconSpacing())); // ORCA Moved add button after delete button to prevent add button position change when remove icon automatically hidden
        bSizer39->AddSpacer(FromDIP(20));

        if (p->combos_filament.size() <= 1) { // ORCA Fix Flushing button and Delete filament button not hidden on launch while only 1 filament exist
            bSizer39->Hide(p->m_flushing_volume_btn);
            bSizer39->Hide(p->m_bpButton_del_filament); // ORCA: Hide delete filament button if there is only one filament
        }

        ams_btn = new ScalableButton(p->m_panel_filament_title, wxID_ANY, "ams_fila_sync", wxEmptyString, wxDefaultSize, wxDefaultPosition,
                                                    wxBU_EXACTFIT | wxNO_BORDER, false, 18);
        ams_btn->SetToolTip(_L("Synchronize filament list from AMS"));
        ams_btn->Bind(wxEVT_BUTTON, [this, scrolled_sizer](wxCommandEvent &e) {
            sync_ams_list();
        });
        p->m_bpButton_ams_filament = ams_btn;

        bSizer39->Add(ams_btn, 0, wxALIGN_CENTER | wxLEFT, FromDIP(SidebarProps::IconSpacing()));
        //bSizer39->Add(FromDIP(10), 0, 0, 0, 0 );

        ScalableButton* set_btn = new ScalableButton(p->m_panel_filament_title, wxID_ANY, "settings");
        set_btn->SetToolTip(_L("Set filaments to use"));
        set_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &e) {
            p->editing_filament = -1;
            AppAdapter::gui_app()->run_wizard(ConfigWizard::RR_USER, ConfigWizard::SP_FILAMENTS);
            });
        p->m_bpButton_set_filament = set_btn;

        bSizer39->Add(set_btn, 0, wxALIGN_CENTER | wxLEFT, FromDIP(SidebarProps::IconSpacing()));
        bSizer39->AddSpacer(FromDIP(SidebarProps::TitlebarMargin()));

        // add filament content
        p->m_panel_filament_content = new wxPanel( p->scrolled, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
        p->m_panel_filament_content->SetBackgroundColour( wxColour( 255, 255, 255 ) );

        // BBS:  filament double columns
        p->sizer_filaments = new wxBoxSizer(wxHORIZONTAL);
        p->sizer_filaments->Add(new wxBoxSizer(wxVERTICAL), 1, wxEXPAND);
        p->sizer_filaments->Add(new wxBoxSizer(wxVERTICAL), 1, wxEXPAND);

        p->combos_filament.push_back(nullptr);

        /* first filament item */
        p->combos_filament[0] = new PlaterPresetComboBox(p->m_panel_filament_content, Preset::TYPE_FILAMENT);
        auto combo_and_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
        // BBS:  filament double columns
        combo_and_btn_sizer->AddSpacer(FromDIP(SidebarProps::ContentMargin()));
        if (p->combos_filament[0]->clr_picker) {
            p->combos_filament[0]->clr_picker->SetLabel("1");
            combo_and_btn_sizer->Add(p->combos_filament[0]->clr_picker, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT,FromDIP(SidebarProps::ElementSpacing()) - FromDIP(2)); // ElementSpacing - 2 (from combo box))
        }
        combo_and_btn_sizer->Add(p->combos_filament[0], 1, wxALL | wxEXPAND, FromDIP(2))->SetMinSize({-1, FromDIP(30) });

        ScalableButton* edit_btn = new ScalableButton(p->m_panel_filament_content, wxID_ANY, "edit");
        edit_btn->SetBackgroundColour(wxColour(255, 255, 255));
        edit_btn->SetToolTip(_L("Click to edit preset"));

        PlaterPresetComboBox* combobox = p->combos_filament[0];
        edit_btn->Bind(wxEVT_BUTTON, [this, combobox](wxCommandEvent)
            {
                p->editing_filament = 0;
                combobox->switch_to_tab();
            });
        combobox->edit_btn = edit_btn;

        combo_and_btn_sizer->Add(edit_btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(SidebarProps::ElementSpacing()) - FromDIP(2)); // ElementSpacing - 2 (from combo box))
        combo_and_btn_sizer->AddSpacer(FromDIP(SidebarProps::ContentMargin()));

        p->combos_filament[0]->set_filament_idx(0);
        p->sizer_filaments->GetItem((size_t)0)->GetSizer()->Add(combo_and_btn_sizer, 1, wxEXPAND);

        //bSizer_filament_content->Add(p->sizer_filaments, 1, wxALIGN_CENTER | wxALL);
        wxSizer *sizer_filaments2 = new wxBoxSizer(wxVERTICAL);
        sizer_filaments2->AddSpacer(FromDIP(16));
        sizer_filaments2->Add(p->sizer_filaments, 0, wxEXPAND, 0);
        sizer_filaments2->AddSpacer(FromDIP(16));
        p->m_panel_filament_content->SetSizer(sizer_filaments2);
        p->m_panel_filament_content->Layout();
        scrolled_sizer->Add(p->m_panel_filament_content, 0, wxEXPAND, 0);
    }

    {
        //add project title
        auto params_panel = ((MainPanel*)parent->GetParent())->m_param_panel;
        if (params_panel) {
            params_panel->get_top_panel()->Reparent(p->scrolled);
            auto spliter_1 = new StaticLine(p->scrolled);
            spliter_1->SetLineColour("#A6A9AA");
            scrolled_sizer->Add(spliter_1, 0, wxEXPAND);
            scrolled_sizer->Add(params_panel->get_top_panel(), 0, wxEXPAND);
            auto spliter_2 = new StaticLine(p->scrolled);
            spliter_2->SetLineColour("#CECECE");
            scrolled_sizer->Add(spliter_2, 0, wxEXPAND);
        }

        //add project content
        p->sizer_params = new wxBoxSizer(wxVERTICAL);

        p->m_search_bar = new wxSearchCtrl(p->scrolled, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        p->m_search_bar->ShowSearchButton(true);
        p->m_search_bar->ShowCancelButton(true);
        p->m_search_bar->SetDescriptiveText(_L("Search plate, object and part."));

        p->m_search_bar->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent&) {
            this->p->on_search_update();
            wxPoint pos = this->p->m_search_bar->ClientToScreen(wxPoint(0, 0));
            pos.y += this->p->m_search_bar->GetRect().height;
            p->dia->SetPosition(pos);
            p->dia->Popup();
            });
        p->m_search_bar->Bind(wxEVT_COMMAND_TEXT_UPDATED, [this](wxCommandEvent&) {
            this->p->on_search_update();
            });
        p->m_search_bar->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) {
            p->dia->Dismiss();
            e.Skip();
            });

        p->m_object_list = new ObjectList(p->scrolled);

        p->sizer_params->Add(p->m_search_bar, 0, wxALL | wxEXPAND, 0);
        p->sizer_params->Add(p->m_object_list, 1, wxEXPAND | wxTOP, 0);
        scrolled_sizer->Add(p->sizer_params, 2, wxEXPAND | wxLEFT, 0);
        p->m_object_list->Hide();
        p->m_search_bar->Hide();
        // Frequently Object Settings
        p->object_settings = new ObjectSettings(p->scrolled);

        p->dia = new Search::SearchObjectDialog(p->m_object_list, p->m_search_bar);

        if (params_panel) {
            params_panel->Reparent(p->scrolled);
            scrolled_sizer->Add(params_panel, 3, wxEXPAND);
        }
    }

    p->object_layers = new ObjectLayers(p->scrolled);
    p->object_layers->Hide();
    // p->sizer_params->Add(p->object_layers->get_sizer(), 0, wxEXPAND | wxTOP, 0);

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(p->scrolled, 1, wxEXPAND);
    SetSizer(sizer);
}

Sidebar::~Sidebar() {}

void Sidebar::create_printer_preset()
{
    CreatePrinterPresetDialog dlg(AppAdapter::mainframe());
    int                       res = dlg.ShowModal();
    if (wxID_OK == res) {
        AppAdapter::main_panel()->update_side_preset_ui();
        update_ui_from_settings();
        update_all_preset_comboboxes();
        AppAdapter::gui_app()->load_current_presets();
        CreatePresetSuccessfulDialog success_dlg(AppAdapter::mainframe(), SuccessType::PRINTER);
        int                          res = success_dlg.ShowModal();
        if (res == wxID_OK) {
            // p->editing_filament = -1;
            // if (p->combo_printer->switch_to_tab())
            // p->editing_filament = 0;
        }
    }
}

void Sidebar::init_filament_combo(PlaterPresetComboBox **combo, const int filament_idx)
{
    *combo = new PlaterPresetComboBox(p->m_panel_filament_content, Slic3r::Preset::TYPE_FILAMENT);
    (*combo)->set_filament_idx(filament_idx);

    auto combo_and_btn_sizer = new wxBoxSizer(wxHORIZONTAL);

    // BBS:  filament double columns
    if ((filament_idx % 2) == 0) // Dont add right column item. this one create equal spacing on left, right & middle
        combo_and_btn_sizer->AddSpacer(FromDIP((filament_idx % 2) == 0 ? 12 : 3)); // Content Margin

    (*combo)->clr_picker->SetLabel(wxString::Format("%d", filament_idx + 1));
    combo_and_btn_sizer->Add((*combo)->clr_picker, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(SidebarProps::ElementSpacing()) - FromDIP(2)); // ElementSpacing - 2 (from combo box))
    combo_and_btn_sizer->Add(*combo, 1, wxALL | wxEXPAND, FromDIP(2))->SetMinSize({-1, FromDIP(30)});

    ScalableButton* edit_btn = new ScalableButton(p->m_panel_filament_content, wxID_ANY, "edit");
    edit_btn->SetToolTip(_L("Click to edit preset"));

    PlaterPresetComboBox* combobox = (*combo);
    edit_btn->Bind(wxEVT_BUTTON, [this, combobox, filament_idx](wxCommandEvent)
        {
            p->editing_filament = -1;
            if (combobox->switch_to_tab())
                p->editing_filament = filament_idx; // sync with TabPresetComboxBox's m_filament_idx
        });
    combobox->edit_btn = edit_btn;

    combo_and_btn_sizer->Add(edit_btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(SidebarProps::ElementSpacing()) - FromDIP(2)); // ElementSpacing - 2 (from combo box))

    combo_and_btn_sizer->AddSpacer(FromDIP(SidebarProps::ContentMargin()));

    // BBS:  filament double columns
    auto side = filament_idx % 2;
    auto /***/sizer_filaments = this->p->sizer_filaments->GetItem(side)->GetSizer();
    if (side == 1 && filament_idx > 1) sizer_filaments->Remove(filament_idx / 2);
    sizer_filaments->Add(combo_and_btn_sizer, 1, wxEXPAND);
    if (side == 0) {
        sizer_filaments = this->p->sizer_filaments->GetItem(1)->GetSizer();
        sizer_filaments->AddStretchSpacer(1);
    }
}

void Sidebar::remove_unused_filament_combos(const size_t current_extruder_count)
{
    if (current_extruder_count >= p->combos_filament.size())
        return;
    while (p->combos_filament.size() > current_extruder_count) {
        const int last = p->combos_filament.size() - 1;
        auto sizer_filaments = this->p->sizer_filaments->GetItem(last % 2)->GetSizer();
        sizer_filaments->Remove(last / 2);
        (*p->combos_filament[last]).Destroy();
        p->combos_filament.pop_back();
    }
    // BBS:  filament double columns
    auto sizer_filaments0 = this->p->sizer_filaments->GetItem((size_t)0)->GetSizer();
    auto sizer_filaments1 = this->p->sizer_filaments->GetItem(1)->GetSizer();
    if (current_extruder_count < 2) {
        sizer_filaments1->Clear();
    } else {
        size_t c0 = sizer_filaments0->GetChildren().GetCount();
        size_t c1 = sizer_filaments1->GetChildren().GetCount();
        if (c0 < c1)
            sizer_filaments1->Remove(c1 - 1);
        else if (c0 > c1)
            sizer_filaments1->AddStretchSpacer(1);
    }
}

void Sidebar::update_all_preset_comboboxes()
{
    PresetBundle &preset_bundle = *app_preset_bundle();
    const auto print_tech = preset_bundle.printers.get_edited_preset().printer_technology();

    auto p_mainframe = AppAdapter::main_panel();
    auto cfg = preset_bundle.printers.get_edited_preset().config;

    {
        ams_btn->Hide();
        auto print_btn_type = MainPanel::PrintSelectType::eExportGcode;
        wxString url = cfg.opt_string("print_host_webui").empty() ? cfg.opt_string("print_host") : cfg.opt_string("print_host_webui");
        wxString apikey;
        if(url.empty())
            url = wxString::Format("file://%s/web/orca/missing_connection.html", from_u8(resources_dir()));
        else {
            if (!url.Lower().starts_with("http"))
                url = wxString::Format("http://%s", url);
            const auto host_type = cfg.option<ConfigOptionEnum<PrintHostType>>("host_type")->value;
            if (cfg.has("printhost_apikey") && (host_type != htSimplyPrint))
                apikey = cfg.opt_string("printhost_apikey");
            print_btn_type = MainPanel::PrintSelectType::eSendGcode;
        }

        p_mainframe->set_print_button_to_default(print_btn_type);

    }

    if (cfg.opt_bool("pellet_modded_printer")) {
		p->m_staticText_filament_settings->SetLabel(_L("Pellets"));
        p->m_filament_icon->SetBitmap_("pellets");
    } else {
		p->m_staticText_filament_settings->SetLabel(_L("Filament"));
        p->m_filament_icon->SetBitmap_("filament");
    }

    show_SEMM_buttons(cfg.opt_bool("single_extruder_multi_material"));

    //p->m_staticText_filament_settings->Update();

    if (cfg.opt_bool("support_multi_bed_types")) {
        m_bed_type_list->Enable();
        // Orca: don't update bed type if loading project
        if (!p->plater->is_loading_project()) {
            auto str_bed_type = AppAdapter::app_config()->get_printer_setting(app_preset_bundle()->printers.get_selected_preset_name(),
                                                                           "curr_bed_type");
            if (!str_bed_type.empty()) {
                int bed_type_value = atoi(str_bed_type.c_str());
                if (bed_type_value <= 0 || bed_type_value >= btCount) {
                    bed_type_value = preset_bundle.printers.get_edited_preset().get_default_bed_type(&preset_bundle);
                }

                m_bed_type_list->SelectAndNotify(bed_type_value - 1);
            } else {
                BedType bed_type = preset_bundle.printers.get_edited_preset().get_default_bed_type(&preset_bundle);
                m_bed_type_list->SelectAndNotify((int) bed_type - 1);
            }
        }
    } else {
        // m_bed_type_list->SelectAndNotify(btPEI - 1);
        BedType bed_type = preset_bundle.printers.get_edited_preset().get_default_bed_type(&preset_bundle);
        m_bed_type_list->SelectAndNotify((int) bed_type - 1);
        m_bed_type_list->Disable();
    }

    if (print_tech == ptFFF) {
        for (PlaterPresetComboBox* cb : p->combos_filament)
            cb->update();
    }

    if (p->combo_printer)
        p->combo_printer->update();

    p_mainframe->m_tabpanel->SetSelection(p_mainframe->m_tabpanel->GetSelection());
}

void Sidebar::update_presets(Preset::Type preset_type)
{
    PresetBundle &preset_bundle = *app_preset_bundle();
    const auto print_tech = preset_bundle.printers.get_edited_preset().printer_technology();

    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": enter, preset_type %1%")%preset_type;
    switch (preset_type) {
    case Preset::TYPE_FILAMENT:
    {
        // BBS
        const size_t filament_cnt = p->combos_filament.size();
        const std::string &name = preset_bundle.filaments.get_selected_preset_name();
        if (p->editing_filament >= 0) {
            preset_bundle.set_filament_preset(p->editing_filament, name);
        } else if (filament_cnt == 1) {
            // Single filament printer, synchronize the filament presets.
            Preset *preset = preset_bundle.filaments.find_preset(name, false);
            if (preset) {
                if (preset->is_compatible) preset_bundle.set_filament_preset(0, name);
            }

        }

        for (size_t i = 0; i < filament_cnt; i++)
            p->combos_filament[i]->update();

        update_dynamic_filament_list();
        break;
    }

    case Preset::TYPE_PRINT:
        //AppAdapter::main_panel()->m_param_panel;
        //p->combo_print->update();
        {
        Tab* print_tab = AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT);
        if (print_tab) {
            print_tab->get_combo_box()->update();
        }
        break;
        }
    case Preset::TYPE_SLA_PRINT:
        ;// p->combo_sla_print->update();
        break;

    case Preset::TYPE_SLA_MATERIAL:
        ;// p->combo_sla_material->update();
        break;

    case Preset::TYPE_PRINTER:
    {
        update_all_preset_comboboxes();
        p->show_preset_comboboxes();

        /* update bed shape */
        Tab* printer_tab = AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER);
        if (printer_tab) {
            printer_tab->update();
            printer_tab->on_preset_loaded();
        }

        Preset& printer_preset = app_preset_bundle()->printers.get_edited_preset();
        if (auto printer_structure_opt = printer_preset.config.option<ConfigOptionEnum<PrinterStructure>>("printer_structure")) {
            AppAdapter::plater()->get_current_canvas3D()->get_arrange_settings().align_to_y_axis = (printer_structure_opt->value == PrinterStructure::psI3);
        }
        else
            AppAdapter::plater()->get_current_canvas3D()->get_arrange_settings().align_to_y_axis = false;

        break;
    }

    default: break;
    }

    // Synchronize config.ini with the current selections.
    app_preset_bundle()->export_selections(*AppAdapter::app_config());

    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": exit.");
}

//BBS
void Sidebar::update_presets_from_to(Slic3r::Preset::Type preset_type, std::string from, std::string to)
{
    PresetBundle &preset_bundle = *app_preset_bundle();

    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": enter, preset_type %1%, from %2% to %3%")%preset_type %from %to;

    switch (preset_type) {
    case Preset::TYPE_FILAMENT:
    {
        const size_t filament_cnt = p->combos_filament.size();
        for (auto it = preset_bundle.filament_presets.begin(); it != preset_bundle.filament_presets.end(); it++)
        {
            if ((*it).compare(from) == 0) {
                (*it) = to;
            }
        }
        for (size_t i = 0; i < filament_cnt; i++)
            p->combos_filament[i]->update();
        break;
    }

    default: break;
    }

    // Synchronize config.ini with the current selections.
    app_preset_bundle()->export_selections(*AppAdapter::app_config());

    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": exit!");
}

void Sidebar::change_top_border_for_mode_sizer(bool increase_border)
{
}

void Sidebar::msw_rescale()
{
    SetMinSize(wxSize(42 * app_em_unit(), -1));
    p->m_panel_printer_title->GetSizer()->SetMinSize(-1, 3 * app_em_unit());
    p->m_panel_filament_title->GetSizer()
        ->SetMinSize(-1, 3 * app_em_unit());
    p->m_printer_icon->msw_rescale();
    p->m_printer_setting->msw_rescale();
    p->m_filament_icon->msw_rescale();
    p->m_bpButton_add_filament->msw_rescale();
    p->m_bpButton_del_filament->msw_rescale();
    p->m_bpButton_ams_filament->msw_rescale();
    p->m_bpButton_set_filament->msw_rescale();
    p->m_flushing_volume_btn->Rescale();
    //BBS
    m_bed_type_list->Rescale();
    m_bed_type_list->SetMinSize({-1, 3 * app_em_unit()});

    p->combo_printer->msw_rescale();
    for (PlaterPresetComboBox* combo : p->combos_filament)
        combo->msw_rescale();

    p->object_settings->msw_rescale();
    p->scrolled->Layout();

    p->searcher.dlg_msw_rescale();
}

void Sidebar::sys_color_changed()
{
    wxWindowUpdateLocker noUpdates(this);

    p->m_printer_icon->msw_rescale();
    p->m_printer_setting->msw_rescale();
    p->m_filament_icon->msw_rescale();
    p->m_bpButton_add_filament->msw_rescale();
    p->m_bpButton_del_filament->msw_rescale();
    p->m_bpButton_ams_filament->msw_rescale();
    p->m_bpButton_set_filament->msw_rescale();
    p->m_flushing_volume_btn->Rescale();

    p->object_settings->sys_color_changed();

    p->combo_printer->sys_color_changed();
    for (PlaterPresetComboBox* combo : p->combos_filament)
        combo->sys_color_changed();

    // BBS
    obj_list()->sys_color_changed();
    obj_layers()->sys_color_changed();

    p->scrolled->Layout();

    p->searcher.dlg_sys_color_changed();
}

void Sidebar::search()
{
    p->searcher.search();
}

void Sidebar::jump_to_option(const std::string& opt_key, Preset::Type type, const std::wstring& category)
{
    //const Search::Option& opt = p->searcher.get_option(opt_key, type);
    if (type == Preset::TYPE_PRINT) {
        auto tab = dynamic_cast<TabPrintModel*>(AppAdapter::gui_app()->params_panel()->get_current_tab());
        if (tab && tab->has_key(opt_key)) {
            tab->activate_option(opt_key, category);
            return;
        }
        AppAdapter::gui_app()->params_panel()->switch_to_global();
    }
    AppAdapter::gui_app()->get_tab(type)->activate_option(opt_key, category);
}

void Sidebar::jump_to_option(size_t selected)
{
    const Search::Option& opt = p->searcher.get_option(selected);
    jump_to_option(opt.opt_key(), opt.type, opt.category);
}

// BBS. Move logic from Plater::on_extruders_change() to Sidebar::on_filaments_change().
void Sidebar::on_filaments_change(size_t num_filaments)
{
    auto& choices = combos_filament();

    if (num_filaments == choices.size())
        return;

    if (choices.size() == 1 || num_filaments == 1)
        choices[0]->GetDropDown().Invalidate();

    wxWindowUpdateLocker noUpdates_scrolled_panel(this);

    size_t i = choices.size();
    while (i < num_filaments)
    {
        PlaterPresetComboBox* choice/*{ nullptr }*/;
        init_filament_combo(&choice, i);
        int last_selection = choices.back()->GetSelection();
        choices.push_back(choice);

        // initialize selection
        choice->update();
        choice->SetSelection(last_selection);
        ++i;
    }

    // remove unused choices if any
    remove_unused_filament_combos(num_filaments);

    auto sizer = p->m_panel_filament_title->GetSizer();
    if (p->m_flushing_volume_btn != nullptr && sizer != nullptr) {
        if (num_filaments > 1) {
            sizer->Show(p->m_flushing_volume_btn);
            sizer->Show(p->m_bpButton_del_filament); // ORCA: Show delete filament button if multiple filaments
        } else {
            sizer->Hide(p->m_flushing_volume_btn);
            sizer->Hide(p->m_bpButton_del_filament); // ORCA: Hide delete filament button if there is only one filament
        }
    }

    Layout();
    p->m_panel_filament_title->Refresh();
    update_ui_from_settings();
    update_dynamic_filament_list();
}

void Sidebar::add_filament() {
    if (p->combos_filament.size() >= MAXIMUM_EXTRUDER_NUMBER) return;
    wxColour    new_col        = Plater::get_next_color_for_filament();
    add_custom_filament(new_col);
}

void Sidebar::delete_filament() {
    if (p->combos_filament.size() <= 1) return;

    size_t filament_count = p->combos_filament.size() - 1;
    if (app_preset_bundle()->is_the_only_edited_filament(filament_count) || (filament_count == 1)) {
        AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->select_preset(app_preset_bundle()->filament_presets[0], false, "", true);
    }

    if (p->editing_filament >= filament_count) {
        p->editing_filament = -1;
    }

    app_preset_bundle()->set_num_filaments(filament_count);
    AppAdapter::plater()->on_filaments_change(filament_count);
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update();
    app_preset_bundle()->export_selections(*AppAdapter::app_config());
}

void Sidebar::add_custom_filament(wxColour new_col) {
    if (p->combos_filament.size() >= MAXIMUM_EXTRUDER_NUMBER) return;

    int         filament_count = p->combos_filament.size() + 1;
    std::string new_color      = new_col.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    app_preset_bundle()->set_num_filaments(filament_count, new_color);
    AppAdapter::plater()->on_filaments_change(filament_count);
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update();
    app_preset_bundle()->export_selections(*AppAdapter::app_config());
    auto_calc_flushing_volumes(filament_count - 1);
}

void Sidebar::on_bed_type_change(BedType bed_type)
{
    // btDefault option is not included in global bed type setting
    int sel_idx = (int)bed_type - 1;
    if (m_bed_type_list != nullptr)
        m_bed_type_list->SetSelection(sel_idx);
}

void Sidebar::sync_ams_list()
{
}

void Sidebar::show_SEMM_buttons(bool bshow)
{
    if(p->m_bpButton_add_filament)
        p->m_bpButton_add_filament->Show(bshow);
    if (p->m_bpButton_del_filament && p->combos_filament.size() > 1)
        p->m_bpButton_del_filament->Show(bshow);
    if (p->m_flushing_volume_btn && p->combos_filament.size() > 1)
        p->m_flushing_volume_btn->Show(bshow);
    Layout();
}

void Sidebar::update_dynamic_filament_list()
{
    dynamic_filament_list.update();
    dynamic_filament_list_1_based.update();
}

ObjectList* Sidebar::obj_list()
{
    // BBS
    //return obj_list();
    return p->m_object_list;
}

ObjectSettings* Sidebar::obj_settings()
{
    return p->object_settings;
}

ObjectLayers* Sidebar::obj_layers()
{
    return p->object_layers;
}

wxPanel* Sidebar::scrolled_panel()
{
    return p->scrolled;
}

wxPanel* Sidebar::print_panel()
{
    return p->m_panel_print_content;
}

wxPanel* Sidebar::filament_panel()
{
    return p->m_panel_filament_content;
}

ConfigOptionsGroup* Sidebar::og_freq_chng_params(const bool is_fff)
{
    return NULL;
}

wxButton* Sidebar::get_wiping_dialog_button()
{
    return NULL;
}

bool Sidebar::show_reslice(bool show)          const { return p->btn_reslice->Show(show); }
bool Sidebar::show_export(bool show)           const { return p->btn_export_gcode->Show(show); }
bool Sidebar::show_send(bool show)             const { return p->btn_send_gcode->Show(show); }
bool Sidebar::show_export_removable(bool show) const { return p->btn_export_gcode_removable->Show(show); }

bool Sidebar::is_multifilament()
{
    return p->combos_filament.size() > 1;
}

static std::vector<Search::InputInfo> get_search_inputs(ConfigOptionMode mode)
{
    std::vector<Search::InputInfo> ret {};

    auto& tabs_list = AppAdapter::gui_app()->tabs_list;
    auto print_tech = app_preset_bundle()->printers.get_selected_preset().printer_technology();
    for (auto tab : tabs_list)
        if (tab->supports_printer_technology(print_tech))
            ret.emplace_back(Search::InputInfo {tab->get_config(), tab->type(), mode});

    return ret;
}

void Sidebar::update_searcher()
{
    p->searcher.init(get_search_inputs(m_mode));
}

void Sidebar::update_mode()
{
    m_mode = (ConfigOptionMode)app_get_mode();

    //BBS: remove print related combos
    update_searcher();

    wxWindowUpdateLocker noUpdates(this);

    obj_list()->unselect_objects();
    obj_list()->update_selections();

    Layout();
}

bool Sidebar::is_collapsed() { return p->plater->is_sidebar_collapsed(); }

void Sidebar::collapse(bool collapse) { p->plater->collapse_sidebar(collapse); }

void Sidebar::show_mode_sizer(bool show)
{
    //p->mode_sizer->Show(show);
}

void Sidebar::update_ui_from_settings()
{
    // BBS
    //p->object_manipulation->update_ui_from_settings();
    // update Cut gizmo, if it's open
    p->plater->update_gizmos_on_off_state();
    p->plater->set_current_canvas_as_dirty();
    p->plater->get_current_canvas3D()->request_extra_frame();
}

bool Sidebar::show_object_list(bool show) const
{
    p->m_search_bar->Show(show);
    if (!p->m_object_list->Show(show))
        return false;
    if (!show)
        p->object_layers->Show(false);
    else
        p->m_object_list->part_selection_changed();
    p->scrolled->Layout();
    return true;
}

void Sidebar::finish_param_edit() { p->editing_filament = -1; }

std::vector<PlaterPresetComboBox*>& Sidebar::combos_filament()
{
    return p->combos_filament;
}

Search::OptionsSearcher& Sidebar::get_searcher()
{
    return p->searcher;
}

std::string& Sidebar::get_search_line()
{
    return p->searcher.search_string();
}

void Sidebar::auto_calc_flushing_volumes(const int modify_id)
{
    auto preset_bundle = app_preset_bundle();
    auto& project_config = preset_bundle->project_config;
    auto& printer_config = preset_bundle->printers.get_edited_preset().config;
    const auto& full_config = app_preset_bundle()->full_config();
    auto& ams_multi_color_filament = preset_bundle->ams_multi_color_filment;
    auto& ams_filament_list = preset_bundle->filament_ams_list;

    const std::vector<double>& init_matrix = (project_config.option<ConfigOptionFloats>("flush_volumes_matrix"))->values;
    const std::vector<double>& init_extruders = (project_config.option<ConfigOptionFloats>("flush_volumes_vector"))->values;

    const std::vector<int>&   min_flush_volumes= get_min_flush_volumes(full_config);

    ConfigOptionFloat* flush_multi_opt = project_config.option<ConfigOptionFloat>("flush_multiplier");
    float flush_multiplier = flush_multi_opt ? flush_multi_opt->getFloat() : 1.f;
    std::vector<double> matrix = init_matrix;
    int m_max_flush_volume = Slic3r::g_max_flush_volume;
    unsigned int m_number_of_extruders = (int)(sqrt(init_matrix.size()) + 0.001);

    const std::vector<std::string> extruder_colours = AppAdapter::plater()->get_extruder_colors_from_plater_config();
    std::vector<std::vector<wxColour>> multi_colours;

    // Support for multi-color filament
    for (int i = 0; i < extruder_colours.size(); ++i) {
        std::vector<wxColour> single_filament;
        if (i < ams_multi_color_filament.size()) {
            if (!ams_multi_color_filament[i].empty()) {
                std::vector<std::string> colors = ams_multi_color_filament[i];
                for (int j = 0; j < colors.size(); ++j) {
                    single_filament.push_back(wxColour(colors[j]));
                }
                multi_colours.push_back(single_filament);
                continue;
            }
        }

        single_filament.push_back(wxColour(extruder_colours[i]));
        multi_colours.push_back(single_filament);
    }

    if (modify_id >= 0 && modify_id < multi_colours.size()) {
        for (int i = 0; i < multi_colours.size(); ++i) {
            // from to modify
            int from_idx = i;
            if (from_idx != modify_id) {
                Slic3r::FlushVolCalculator calculator(min_flush_volumes[from_idx], m_max_flush_volume);
                int flushing_volume = 0;
                bool is_from_support = is_support_filament(from_idx);
                bool is_to_support = is_support_filament(modify_id);
                if (is_to_support) {
                    flushing_volume = Slic3r::g_flush_volume_to_support;
                }
                else {
                    for (int j = 0; j < multi_colours[from_idx].size(); ++j) {
                        const wxColour& from = multi_colours[from_idx][j];
                        for (int k = 0; k < multi_colours[modify_id].size(); ++k) {
                            const wxColour& to = multi_colours[modify_id][k];
                            int volume = calculator.calc_flush_vol(from.Alpha(), from.Red(), from.Green(), from.Blue(), to.Alpha(), to.Red(), to.Green(), to.Blue());
                            flushing_volume = std::max(flushing_volume, volume);
                        }
                    }
                    if (is_from_support)
                        flushing_volume = std::max(flushing_volume, Slic3r::g_min_flush_volume_from_support);
                }
                matrix[m_number_of_extruders * from_idx + modify_id] = flushing_volume;
            }

            // modify to to
            int to_idx = i;
            if (to_idx != modify_id) {
                Slic3r::FlushVolCalculator calculator(min_flush_volumes[modify_id], m_max_flush_volume);
                bool is_from_support = is_support_filament(modify_id);
                bool is_to_support = is_support_filament(to_idx);
                int flushing_volume = 0;
                if (is_to_support) {
                    flushing_volume = Slic3r::g_flush_volume_to_support;
                }
                else {
                    for (int j = 0; j < multi_colours[modify_id].size(); ++j) {
                        const wxColour& from = multi_colours[modify_id][j];
                        for (int k = 0; k < multi_colours[to_idx].size(); ++k) {
                            const wxColour& to = multi_colours[to_idx][k];
                            int volume = calculator.calc_flush_vol(from.Alpha(), from.Red(), from.Green(), from.Blue(), to.Alpha(), to.Red(), to.Green(), to.Blue());
                            flushing_volume = std::max(flushing_volume, volume);
                        }
                    }
                    if (is_from_support)
                        flushing_volume = std::max(flushing_volume, Slic3r::g_min_flush_volume_from_support);

                    matrix[m_number_of_extruders * modify_id + to_idx] = flushing_volume;
                }
            }
        }
    }
    (project_config.option<ConfigOptionFloats>("flush_volumes_matrix"))->values = std::vector<double>(matrix.begin(), matrix.end());

    app_preset_bundle()->export_selections(*AppAdapter::app_config());

    AppAdapter::plater()->update_project_dirty_from_presets();
    wxPostEvent(this, SimpleEvent(EVT_SCHEDULE_BACKGROUND_PROCESS, this));
}

void Sidebar::jump_to_object(ObjectDataViewModelNode* item)
{
    p->jump_to_object(item);
}

void Sidebar::can_search()
{
    p->can_search();
}

const std::regex Plater::priv::pattern_bundle(".*[.](amf|amf[.]xml|zip[.]amf|3mf)", std::regex::icase);
const std::regex Plater::priv::pattern_3mf(".*3mf", std::regex::icase);
const std::regex Plater::priv::pattern_zip_amf(".*[.]zip[.]amf", std::regex::icase);
const std::regex Plater::priv::pattern_any_amf(".*[.](amf|amf[.]xml|zip[.]amf)", std::regex::icase);
const std::regex Plater::priv::pattern_prusa(".*bbl", std::regex::icase);

wxColour Plater::get_next_color_for_filament()
{
    static int curr_color_filamenet = 0;
    // refs to https://www.ebaomonthly.com/window/photo/lesson/colorList.htm
    wxColour colors[FILAMENT_SYSTEM_COLORS_NUM] = {
        // ORCA updated all color palette
        wxColour("#00C1AE"),
        wxColour("#F4E2C1"),
        wxColour("#ED1C24"),
        wxColour("#00FF7F"),
        wxColour("#F26722"),
        wxColour("#FFEB31"),
        wxColour("#7841CE"),
        wxColour("#115877"),
        wxColour("#ED1E79"),
        wxColour("#2EBDEF"),
        wxColour("#345B2F"),
        wxColour("#800080"),
        wxColour("#FA8173"),
        wxColour("#800000"),
        wxColour("#F7B763"),
        wxColour("#A4C41E"),
    };
    return colors[curr_color_filamenet++ % FILAMENT_SYSTEM_COLORS_NUM];
}

wxString Plater::get_slice_warning_string(GCodeProcessorResult::SliceWarning& warning)
{
    if (warning.msg == BED_TEMP_TOO_HIGH_THAN_FILAMENT) {
        return _L("The current hot bed temperature is relatively high. The nozzle may be clogged when printing this filament in a closed enclosure. Please open the front door and/or remove the upper glass.");
    } else if (warning.msg == NOZZLE_HRC_CHECKER) {
        return _L("The nozzle hardness required by the filament is higher than the default nozzle hardness of the printer. Please replace the hardened nozzle or filament, otherwise, the nozzle will be attrited or damaged.");
    } else if (warning.msg == NOT_SUPPORT_TRADITIONAL_TIMELAPSE) {
        return _L("Enabling traditional timelapse photography may cause surface imperfections. It is recommended to change to smooth mode.");
    } else if (warning.msg == NOT_GENERATE_TIMELAPSE) {
        return wxString();
    }
    else {
        return wxString(warning.msg);
    }
}

void Plater::find_new_position(const ModelInstancePtrs &instances)
{
    arrangement::ArrangePolygons movable, fixed;
    arrangement::ArrangeParams arr_params = init_arrange_params(this);

    for (const ModelObject *mo : p->model.objects)
        for (ModelInstance *inst : mo->instances) {
            auto it = std::find(instances.begin(), instances.end(), inst);
            arrangement::ArrangePolygon arrpoly;
            inst->get_arrange_polygon(&arrpoly);

            if (it == instances.end())
                fixed.emplace_back(std::move(arrpoly));
            else {
                arrpoly.setter = [it](const arrangement::ArrangePolygon &p) {
                    if (p.is_arranged() && p.bed_idx == 0) {
                        Vec2d t = p.translation.cast<double>();
                        (*it)->apply_arrange_result(t, p.rotation);
                    }
                };
                movable.emplace_back(std::move(arrpoly));
            }
        }

    if (auto wt = get_wipe_tower_arrangepoly(*this))
        fixed.emplace_back(*wt);

    arrangement::arrange(movable, fixed, this->build_volume().polygon(), arr_params);

    for (auto & m : movable)
        m.apply();
}

void Plater::orient()
{
    auto &w = get_ui_job_worker();
    if (w.is_idle()) {
        p->take_snapshot(_u8L("Orient"));
        replace_job(w, std::make_unique<OrientJob>());
    }
}

//BBS: add job state related functions
void Plater::set_prepare_state(int state)
{
    p->m_job_prepare_state = state;
}

int Plater::get_prepare_state()
{
    return p->m_job_prepare_state;
}

void Sidebar::set_btn_label(const ActionButtonType btn_type, const wxString& label) const
{
    switch (btn_type)
    {
        case ActionButtonType::abReslice:   p->btn_reslice->SetLabelText(label);        break;
        case ActionButtonType::abExport:    p->btn_export_gcode->SetLabelText(label);   break;
        case ActionButtonType::abSendGCode: /*p->btn_send_gcode->SetLabelText(label);*/     break;
    }
}

// Plater / Public

Plater::Plater(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, app_min_size())
    , p(nullptr)
{

}

void Plater::init()
{
    p.reset(new priv(this, AppAdapter::main_panel()));
    p->init(p->q, AppAdapter::main_panel());
    // Initialization performed in the private c-tor
    enable_wireframe(true);
    
}

void Plater::create_printer_preset()
{
    p->sidebar->create_printer_preset();
}

void Plater::select_printer_preset(const std::string& preset_name)
{
    p->select_printer_preset(preset_name);
}

bool Plater::Show(bool show)
{
    if (AppAdapter::main_panel())
        AppAdapter::main_panel()->show_option(show);
    return wxPanel::Show(show);
}

bool Plater::is_project_dirty() const { return p->is_project_dirty(); }
bool Plater::is_presets_dirty() const { return p->is_presets_dirty(); }
void Plater::set_plater_dirty(bool is_dirty) { p->set_plater_dirty(is_dirty); }
void Plater::update_project_dirty_from_presets() { p->update_project_dirty_from_presets(); }
int  Plater::save_project_if_dirty(const wxString& reason) { return p->save_project_if_dirty(reason); }
void Plater::reset_project_dirty_after_save() { p->reset_project_dirty_after_save(); }
void Plater::reset_project_dirty_initial_presets() { p->reset_project_dirty_initial_presets(); }
#if ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW
void Plater::render_project_state_debug_window() const { p->render_project_state_debug_window(); }
#endif // ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW

Sidebar&        Plater::sidebar()           { return *p->sidebar; }
const Model&    Plater::model() const       { return p->model; }
Model&          Plater::model()             { return p->model; }

Bed3D* Plater::bed()
{
    return p->get_bed();
}

bool Plater::is_normal_devide_mode()
{
    return is_normal_mode_config(*p->config);
}

GLVolumeCollection::ERenderMode Plater::render_mode()
{
    return is_mirror_mode_config(*p->config) ? GLVolumeCollection::ERenderMode::Mirror :
        ( is_copy_mode_config(*p->config) ? GLVolumeCollection::ERenderMode::Copy :
            GLVolumeCollection::ERenderMode::Normal );
}

int Plater::new_project(bool skip_confirm, bool silent, const wxString& project_name)
{
    bool transfer_preset_changes = false;
    // BBS: save confirm
    auto check = [&transfer_preset_changes](bool yes_or_no) {
        wxString header = _L("Some presets are modified.") + "\n" +
            (yes_or_no ? _L("You can keep the modified presets to the new project or discard them") :
                _L("You can keep the modified presets to the new project, discard or save changes as new presets."));
        int act_buttons = ActionButtons::KEEP | ActionButtons::REMEMBER_CHOISE;
        if (!yes_or_no)
            act_buttons |= ActionButtons::SAVE;
        return AppAdapter::gui_app()->check_and_keep_current_preset_changes(_L("Creating a new project"), header, act_buttons, &transfer_preset_changes);
    };
    int result;
    if (!skip_confirm && (result = close_with_confirm(check)) == wxID_CANCEL)
        return wxID_CANCEL;

    set_using_exported_file(false);
    m_loading_project = false;
    get_notification_manager()->bbl_close_plateinfo_notification();
    get_notification_manager()->bbl_close_preview_only_notification();
    get_notification_manager()->bbl_close_3mf_warn_notification();
    get_notification_manager()->close_notification_of_type(NotificationType::PlaterError);
    get_notification_manager()->close_notification_of_type(NotificationType::PlaterWarning);
    get_notification_manager()->close_notification_of_type(NotificationType::SlicingError);
    get_notification_manager()->close_notification_of_type(NotificationType::SlicingSeriousWarning);
    get_notification_manager()->close_notification_of_type(NotificationType::SlicingWarning);

    if (!silent)
        AppAdapter::main_panel()->select_tab(MainPanel::tp3DEditor);

    Model m;
    model().load_from(m); // new id avoid same path name
    
    reset(transfer_preset_changes);
    reset_project_dirty_after_save();
    reset_project_dirty_initial_presets();
    AppAdapter::gui_app()->update_saved_preset_from_current_preset();
    update_project_dirty_from_presets();

    //reset project
    p->project.reset();
    //set project name
    if (project_name.empty())
        p->set_project_name(_L("Untitled"));
    else
        p->set_project_name(project_name);

    Plater::TakeSnapshot snapshot(this, "New Project", UndoRedo::SnapshotType::ProjectSeparator);

    //select first plate
    get_partplate_list().select_plate(0);
    SimpleEvent event(EVT_GLCANVAS_PLATE_SELECT);
    p->on_plate_selected(event);

    AppAdapter::app_config()->update_last_backup_dir(model().get_backup_path());

    // BBS set default view and zoom
    select_view3d();

    p->select_view("topfront");
    p->camera.requires_zoom_to_bed = true;
    enable_sidebar(true);

    up_to_date(true, false);
    up_to_date(true, true);
    return wxID_YES;
}

LoadType determine_load_type(std::string filename, std::string override_setting = "");

// BBS: FIXME, missing resotre logic
void Plater::load_project(wxString const& filename2,
    wxString const& originfile)
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "filename is: " << filename2 << "and originfile is: " << originfile;
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__;
    auto filename = filename2;
    auto check = [&filename, this] (bool yes_or_no) {
        if (!yes_or_no && !AppAdapter::gui_app()->check_and_save_current_preset_changes(_L("Load project"),
                _L("Some presets are modified.")))
            return false;
        if (filename.empty()) {
            // Ask user for a project file name.
            filename = choose_project_name(this);
        }
        return !filename.empty();
    };

    // BSS: save project, force close
    int result;
    if ((result = close_with_confirm(check)) == wxID_CANCEL) {
        return;
    }

    // BBS
    if (m_loading_project) {
        BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": current loading other project, return directly");
        return;
    }
    else
        m_loading_project = true;

    set_using_exported_file(false);
    get_notification_manager()->bbl_close_plateinfo_notification();
    get_notification_manager()->bbl_close_preview_only_notification();
    get_notification_manager()->bbl_close_3mf_warn_notification();
    get_notification_manager()->close_notification_of_type(NotificationType::PlaterError);
    get_notification_manager()->close_notification_of_type(NotificationType::PlaterWarning);
    get_notification_manager()->close_notification_of_type(NotificationType::SlicingError);
    get_notification_manager()->close_notification_of_type(NotificationType::SlicingSeriousWarning);
    get_notification_manager()->close_notification_of_type(NotificationType::SlicingWarning);

    auto path     = into_path(filename);

    auto strategy = LoadStrategy::LoadModel | LoadStrategy::LoadConfig;
    if (originfile == "<silence>") {
        strategy = strategy | LoadStrategy::Silence;
    } else if (originfile == "<loadall>") {
        // Do nothing
    } else if (originfile != "-") {
        strategy = strategy | LoadStrategy::Restore;
    } else {
        switch (determine_load_type(filename.ToStdString())) {
            case LoadType::OpenProject: break; // Do nothing
            case LoadType::LoadGeometry:; strategy = LoadStrategy::LoadModel; break;
            default: return; // User cancelled
        }
    }
    bool load_restore = strategy & LoadStrategy::Restore;

    // Take the Undo / Redo snapshot.
    reset();

    Plater::TakeSnapshot snapshot(this, "Load Project", UndoRedo::SnapshotType::ProjectSeparator);

    std::vector<fs::path> input_paths;
    input_paths.push_back(path);
    if (strategy & LoadStrategy::Restore)
        input_paths.push_back(into_u8(originfile));

    std::vector<size_t> res = load_files(input_paths, strategy);

    reset_project_dirty_initial_presets();
    update_project_dirty_from_presets();
    app_preset_bundle()->export_selections(*AppAdapter::app_config());

    // if res is empty no data has been loaded
    if (!res.empty() && (load_restore || !(strategy & LoadStrategy::Silence))) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << __LINE__ << " call set_project_filename: " << load_restore ? originfile : filename;
        p->set_project_filename(load_restore ? originfile : filename);
        if (load_restore && originfile.IsEmpty()) {
        p->set_project_name(_L("Untitled"));
        }

    } else {
        if (using_exported_file()) {
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << __LINE__ << " using ecported set project filename: " << filename;
            p->set_project_filename(filename);
        }

    }

    if (!m_exported_file) {
        p->select_view("topfront");
        p->camera.requires_zoom_to_plate = REQUIRES_ZOOM_TO_ALL_PLATE;
        AppAdapter::main_panel()->select_tab(MainPanel::tp3DEditor);
    }
    else {
        p->partplate_list.select_plate_view();
    }

    enable_sidebar(true);

    AppAdapter::app_config()->update_last_backup_dir(model().get_backup_path());
    if (load_restore && !originfile.empty()) {
        AppAdapter::app_config()->update_skein_dir(into_path(originfile).parent_path().string());
        AppAdapter::app_config()->update_config_dir(into_path(originfile).parent_path().string());
    }

    if (!load_restore)
        up_to_date(true, false);
    else
        p->dirty_state.update_from_undo_redo_stack(true);
    up_to_date(true, true);

    AppAdapter::gui_app()->params_panel()->switch_to_object_if_has_object_configs();

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << __LINE__ << " load project done";
    m_loading_project = false;
}

// BBS: save logic
int Plater::save_project(bool saveAs)
{
    //if (up_to_date(false, false)) // should we always save
    //    return;
    auto filename = get_project_filename(".3mf");
    if (!saveAs && filename.IsEmpty())
        saveAs = true;
    if (saveAs)
        filename = p->get_export_file(FT_3MF);
    if (filename.empty())
        return wxID_NO;
    if (filename == "<cancel>")
        return wxID_CANCEL;

    //BBS export 3mf without gcode
    if (export_3mf(into_path(filename), SaveStrategy::SplitModel | SaveStrategy::ShareMesh | SaveStrategy::FullPathSources) < 0) {
        MessageDialog(this, _L("Failed to save the project.\nPlease check whether the folder exists online or if other programs open the project file."),
            _L("Save project"), wxOK | wxICON_WARNING).ShowModal();
        return wxID_CANCEL;
    }

    Slic3r::remove_backup(model(), false);

    p->set_project_filename(filename);
    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << __LINE__ << " call set_project_filename: " << filename;

    up_to_date(true, false);
    up_to_date(true, true);

    AppAdapter::gui_app()->update_saved_preset_from_current_preset();
    reset_project_dirty_after_save();

    update_title_dirty_status();
    return wxID_YES;
}

//BBS import model by model id
void Plater::import_model_id(wxString download_info)
{
    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << __LINE__ << " download info: " << download_info;

    wxString download_origin_url = download_info;
    wxString download_url;
    wxString filename;
    wxString separator = "&name=";

    try
    {
        size_t namePos = download_info.Find(separator);
        if (namePos != wxString::npos) {
            download_url = download_info.Mid(0, namePos);
            filename = download_info.Mid(namePos + separator.Length());

        }
        else {
            fs::path download_path = fs::path(download_origin_url.wx_str());
            download_url = download_origin_url;
            filename = download_path.filename().string();
        }

    }
    catch (const std::exception&)
    {
        //wxString sError = error.what();
    }

    bool download_ok = false;
    int retry_count = 0;
    const int max_retries = 3;

    /* jump to 3D eidtor */
    AppAdapter::main_panel()->select_tab((size_t)MainPanel::TabPosition::tp3DEditor);

    /* prepare progress dialog */
    bool cont = true;
    bool cont_dlg = true;
    bool cancel = false;
    wxString msg;
    wxString dlg_title = _L("Importing Model");

    int percent = 0;
    ProgressDialog dlg(dlg_title,
        wxString(' ', 100) + "\n\n\n\n",
        100,    // range
        this,   // parent
        wxPD_CAN_ABORT |
        wxPD_APP_MODAL |
        wxPD_AUTO_HIDE |
        wxPD_SMOOTH);

    boost::filesystem::path target_path;

    //reset params
    p->project.reset();

    /* prepare project and profile */
    boost::thread import_thread = Slic3r::create_thread([&percent, &cont, &cancel, &retry_count, max_retries, &msg, &target_path, &download_ok, download_url, &filename] {

        int res = 0;
        std::string http_body;

        msg = _L("prepare 3mf file...");

        //gets the number of files with the same name
        std::vector<wxString>   vecFiles;
        bool                    is_already_exist = false;


        target_path = fs::path(AppAdapter::app_config()->get("download_path"));

        try
        {
            vecFiles.clear();
            wxString extension = fs::path(filename.wx_str()).extension().c_str();


            //check file suffix
            if (!extension.Contains(".3mf")) {
                msg = _L("Download failed, unknown file format.");
                return;
            }

            auto name = filename.substr(0, filename.length() - extension.length() - 1);

            for (const auto& iter : boost::filesystem::directory_iterator(target_path))
            {
                if (boost::filesystem::is_directory(iter.path()))
                    continue;

                wxString sFile = iter.path().filename().string().c_str();
                if (strstr(sFile.c_str(), name.c_str()) != NULL) {
                    vecFiles.push_back(sFile);
                }

                if (sFile == filename) is_already_exist = true;
            }
        }
        catch (const std::exception&)
        {
            //wxString sError = error.what();
        }

        //update filename
        if (is_already_exist && vecFiles.size() >= 1) {
            wxString extension = fs::path(filename.wx_str()).extension().c_str();
            wxString name = filename.substr(0, filename.length() - extension.length());
            filename = wxString::Format("%s(%d)%s", name, vecFiles.size() + 1, extension).ToStdString();
        }


        msg = _L("downloading project ...");

        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        std::string unique = to_string(uuid).substr(0, 6);

        if (filename.empty()) {
            filename = "untitled.3mf";
        }

        //target_path /= (boost::format("%1%_%2%.3mf") % filename % unique).str();
        target_path /= fs::path(filename.wc_str());

        fs::path tmp_path = target_path;
        tmp_path += format(".%1%", ".download");

        auto filesize = 0;
        bool size_limit = false;
        auto http = Http::get(download_url.ToStdString());

        while (cont && retry_count < max_retries) {
            retry_count++;
            http.on_progress([&percent, &cont, &msg, &filesize, &size_limit](Http::Progress progress, bool& cancel) {

                    if (!cont) cancel = true;
                    if (progress.dltotal != 0) {

                        if (filesize == 0) {
                            filesize = progress.dltotal;
                            double megabytes = static_cast<double>(progress.dltotal) / (1024 * 1024);
                            //The maximum size of a 3mf file is 500mb
                            if (megabytes > 500) {
                                cont = false;
                                size_limit = true;
                            }
                        }
                        percent = progress.dlnow * 100 / progress.dltotal;
                    }

                    if (size_limit) {
                        msg = _L("Download failed, File size exception.");
                    }
                    else {
                        msg = wxString::Format(_L("Project downloaded %d%%"), percent);
                    }
                })
                .on_error([&msg, &cont, &retry_count, max_retries](std::string body, std::string error, unsigned http_status) {
                    (void)body;
                    BOOST_LOG_TRIVIAL(error) << format("Error getting: `%1%`: HTTP %2%, %3%",
                        body,
                        http_status,
                        error);

                    if (retry_count == max_retries) {
                        msg = _L("Importing to Orca Slicer failed. Please download the file and manually import it.");
                        cont = false;
                    }
                })
                .on_complete([&cont, &download_ok, tmp_path, target_path](std::string body, unsigned /* http_status */) {
                        fs::fstream file(tmp_path, std::ios::out | std::ios::binary | std::ios::trunc);
                        file.write(body.c_str(), body.size());
                        file.close();
                        fs::rename(tmp_path, target_path);
                        cont = false;
                        download_ok = true;
                }).perform_sync();

                // for break while
                //cont = false;
        }

    });

    while (cont && cont_dlg) {
        wxMilliSleep(50);
        cont_dlg = dlg.Update(percent, msg);
        if (!cont_dlg) {
            cont = cont_dlg;
            cancel = true;
        }

        if (download_ok)
            break;
    }

    if (import_thread.joinable())
        import_thread.join();

    dlg.Hide();
    dlg.Close();
    if (download_ok) {
        BOOST_LOG_TRIVIAL(trace) << "import_model_id: target_path = " << target_path.string();
        /* load project */
        // Orca: If download is a zip file, treat it as if file has been drag and dropped on the plater
        if (target_path.extension() == ".zip")
            this->load_files(wxArrayString(1, target_path.string()));
        else
            this->load_project(target_path.wstring());
        /*BBS set project info after load project, project info is reset in load project */
        //p->project.project_model_id = model_id;
        //p->project.project_design_id = design_id;
        AppConfig* config = AppAdapter::app_config();
        if (config) {
            p->project.project_country_code = config->get_country_code();
        }

        // show save new project
        p->set_project_filename(target_path.wstring());
        p->notification_manager->push_import_finished_notification(target_path.string(), target_path.parent_path().string(), false);
    }
    else {
        if (!msg.empty()) {
            MessageDialog msg_wingow(nullptr, msg, wxEmptyString, wxICON_WARNING | wxOK);
            msg_wingow.SetSize(wxSize(FromDIP(480), -1));
            msg_wingow.ShowModal();
        }
        return;
    }
}

// BBS: save logic
bool Plater::up_to_date(bool saved, bool backup)
{
    if (saved) {
        Slic3r::clear_other_changes(backup);
        return p->up_to_date(saved, backup);
    }
    return p->model.objects.empty() || (p->up_to_date(saved, backup) &&
                                        !Slic3r::has_other_changes(backup));
}

void Plater::add_model(bool imperial_units, std::string fname)
{
    wxArrayString input_files;

    std::vector<fs::path> paths;
    if (fname.empty()) {
        input_files = choose_model_name(this);
        if (input_files.empty())
            return;

        for (const auto& file : input_files)
            paths.emplace_back(into_path(file));
    }
    else {
        paths.emplace_back(fname);
    }

    std::string snapshot_label;
    assert(! paths.empty());
    if (paths.size() == 1) {
        snapshot_label = "Import Object";
        snapshot_label += ": ";
        snapshot_label += encode_path(paths.front().filename().string().c_str());
    } else {
        snapshot_label = "Import Objects";
        snapshot_label += ": ";
        snapshot_label += paths.front().filename().string().c_str();
        for (size_t i = 1; i < paths.size(); ++ i) {
            snapshot_label += ", ";
            snapshot_label += encode_path(paths[i].filename().string().c_str());
        }
    }

    Plater::TakeSnapshot snapshot(this, snapshot_label);

    // BBS: check file types
    auto loadfiles_type  = LoadFilesType::NoFile;
    auto amf_files_count = get_3mf_file_count(paths);

    if (paths.size() > 1 && amf_files_count < paths.size()) { loadfiles_type = LoadFilesType::Multiple3MFOther; }
    if (paths.size() > 1 && amf_files_count == paths.size()) { loadfiles_type = LoadFilesType::Multiple3MF; }
    if (paths.size() > 1 && amf_files_count == 0) { loadfiles_type = LoadFilesType::MultipleOther; }
    if (paths.size() == 1 && amf_files_count == 1) { loadfiles_type = LoadFilesType::Single3MF; };
    if (paths.size() == 1 && amf_files_count == 0) { loadfiles_type = LoadFilesType::SingleOther; };

    bool ask_multi = false;

    if (loadfiles_type == LoadFilesType::MultipleOther)
        ask_multi = true;

    auto strategy = LoadStrategy::LoadModel;
    if (imperial_units) strategy = strategy | LoadStrategy::ImperialUnits;
    if (!load_files(paths, strategy, ask_multi).empty()) {

        if (get_project_name() == _L("Untitled") && paths.size() > 0) {
            boost::filesystem::path full_path(paths[0].string());
            p->set_project_name(from_u8(full_path.stem().string()));
        }

        AppAdapter::main_panel()->update_title();
    }
}

void Plater::cut_horizontal(size_t obj_idx, size_t instance_idx, double z, ModelObjectCutAttributes attributes)
{
    wxCHECK_RET(obj_idx < p->model.objects.size(), "obj_idx out of bounds");
    auto *object = p->model.objects[obj_idx];

    wxCHECK_RET(instance_idx < object->instances.size(), "instance_idx out of bounds");

    if (! attributes.has(ModelObjectCutAttribute::KeepUpper) && ! attributes.has(ModelObjectCutAttribute::KeepLower))
        return;

    wxBusyCursor wait;

    const Vec3d instance_offset = object->instances[instance_idx]->get_offset();
    Cut         cut(object, instance_idx, Geometry::translation_transform(z * Vec3d::UnitZ() - instance_offset), attributes);
    const auto  new_objects = cut.perform_with_plane();

    apply_cut_object_to_model(obj_idx, new_objects);
}

// Adjust settings for flowrate calibration
// For linear mode, pass 1 means normal version while pass 2 mean "for perfectionists" version
void adjust_settings_for_flowrate_calib(ModelObjectPtrs& objects, bool linear, int pass)
{
auto print_config = &app_preset_bundle()->prints.get_edited_preset().config;
    auto printerConfig = &app_preset_bundle()->printers.get_edited_preset().config;
    auto filament_config = &app_preset_bundle()->filaments.get_edited_preset().config;

    /// --- scale ---
    // model is created for a 0.4 nozzle, scale z with nozzle size.
    const ConfigOptionFloats* nozzle_diameter_config = printerConfig->option<ConfigOptionFloats>("nozzle_diameter");
    assert(nozzle_diameter_config->values.size() > 0);
    float nozzle_diameter = nozzle_diameter_config->values[0];
    float xyScale = nozzle_diameter / 0.6;
    //scale z to have 10 layers
    // 2 bottom, 5 top, 3 sparse infill
    double first_layer_height = print_config->option<ConfigOptionFloat>("initial_layer_print_height")->value;
    double layer_height = nozzle_diameter / 2.0; // prefer 0.2 layer height for 0.4 nozzle
    first_layer_height = std::max(first_layer_height, layer_height);

    float zscale = (first_layer_height + 9 * layer_height) / 2;
    // only enlarge
    if (xyScale > 1.2) {
        for (auto _obj : objects)
            _obj->scale(xyScale, xyScale, zscale); 
    }
    else {
        for (auto _obj : objects)
            _obj->scale(1, 1, zscale);
    }

    auto cur_flowrate = filament_config->option<ConfigOptionFloats>("filament_flow_ratio")->get_at(0);
    Flow infill_flow = Flow(nozzle_diameter * 1.2f, layer_height, nozzle_diameter);
    double filament_max_volumetric_speed = filament_config->option<ConfigOptionFloats>("filament_max_volumetric_speed")->get_at(0);
    double max_infill_speed;
    if (linear)
        max_infill_speed = filament_max_volumetric_speed /
                           (infill_flow.mm3_per_mm() * (cur_flowrate + (pass == 2 ? 0.035 : 0.05)) / cur_flowrate);
    else
        max_infill_speed = filament_max_volumetric_speed / (infill_flow.mm3_per_mm() * (pass == 1 ? 1.2 : 1));
    double internal_solid_speed = std::floor(std::min(print_config->opt_float("internal_solid_infill_speed"), max_infill_speed));
    double top_surface_speed = std::floor(std::min(print_config->opt_float("top_surface_speed"), max_infill_speed));

    // adjust parameters
    for (auto _obj : objects) {
        _obj->ensure_on_bed();
        _obj->config.set_key_value("wall_loops", new ConfigOptionInt(1));
        _obj->config.set_key_value("only_one_wall_top", new ConfigOptionBool(true));
        _obj->config.set_key_value("thick_internal_bridges", new ConfigOptionBool(false));
        _obj->config.set_key_value("enable_extra_bridge_layer", new ConfigOptionEnum<EnableExtraBridgeLayer>(eblDisabled));
        _obj->config.set_key_value("internal_bridge_density", new ConfigOptionPercent(100));
        _obj->config.set_key_value("sparse_infill_density", new ConfigOptionPercent(35));
        _obj->config.set_key_value("min_width_top_surface", new ConfigOptionFloatOrPercent(100,true));
        _obj->config.set_key_value("bottom_shell_layers", new ConfigOptionInt(2));
        _obj->config.set_key_value("top_shell_layers", new ConfigOptionInt(5));
        _obj->config.set_key_value("top_shell_thickness", new ConfigOptionFloat(0));
        _obj->config.set_key_value("bottom_shell_thickness", new ConfigOptionFloat(0));
        _obj->config.set_key_value("detect_thin_wall", new ConfigOptionBool(true));
        _obj->config.set_key_value("filter_out_gap_fill", new ConfigOptionFloat(0));
        _obj->config.set_key_value("sparse_infill_pattern", new ConfigOptionEnum<InfillPattern>(ipRectilinear));
        _obj->config.set_key_value("top_surface_line_width", new ConfigOptionFloatOrPercent(nozzle_diameter * 1.2f, false));
        _obj->config.set_key_value("internal_solid_infill_line_width", new ConfigOptionFloatOrPercent(nozzle_diameter * 1.2f, false));
        _obj->config.set_key_value("top_surface_pattern", new ConfigOptionEnum<InfillPattern>(ipMonotonic));
        _obj->config.set_key_value("top_solid_infill_flow_ratio", new ConfigOptionFloat(1.0f));
        _obj->config.set_key_value("infill_direction", new ConfigOptionFloat(45));
        _obj->config.set_key_value("solid_infill_direction", new ConfigOptionFloat(135));
        _obj->config.set_key_value("rotate_solid_infill_direction", new ConfigOptionBool(true));
        _obj->config.set_key_value("ironing_type", new ConfigOptionEnum<IroningType>(IroningType::NoIroning));
        _obj->config.set_key_value("internal_solid_infill_speed", new ConfigOptionFloat(internal_solid_speed));
        _obj->config.set_key_value("top_surface_speed", new ConfigOptionFloat(top_surface_speed));
        _obj->config.set_key_value("seam_slope_type", new ConfigOptionEnum<SeamScarfType>(SeamScarfType::None));
        print_config->set_key_value("max_volumetric_extrusion_rate_slope", new ConfigOptionFloat(0));

        // extract flowrate from name, filename format: flowrate_xxx
        std::string obj_name = _obj->name;
        assert(obj_name.length() > 9);
        obj_name = obj_name.substr(9);
        if (obj_name[0] == 'm')
            obj_name[0] = '-';
        // Orca: force set locale to C to avoid parsing error
        const std::string _loc = std::setlocale(LC_NUMERIC, nullptr);
        std::setlocale(LC_NUMERIC,"C");
        auto              modifier  = 1.0f;
        try {
            modifier = stof(obj_name);
        } catch (...) {
        }
        // restore locale
        std::setlocale(LC_NUMERIC, _loc.c_str());

        if(linear)
            _obj->config.set_key_value("print_flow_ratio", new ConfigOptionFloat((cur_flowrate + modifier)/cur_flowrate));
        else
            _obj->config.set_key_value("print_flow_ratio", new ConfigOptionFloat(1.0f + modifier/100.f));

    }

    print_config->set_key_value("layer_height", new ConfigOptionFloat(layer_height));
    print_config->set_key_value("alternate_extra_wall", new ConfigOptionBool(false));
    print_config->set_key_value("initial_layer_print_height", new ConfigOptionFloat(first_layer_height));
    print_config->set_key_value("reduce_crossing_wall", new ConfigOptionBool(true));


    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->reload_config();
}

BuildVolume_Type Plater::get_build_volume_type() const { return PlateBed::build_volume().type(); }

void Plater::import_zip_archive()
{
    wxString input_file = choose_zip_name(this);
    if (input_file.empty())
        return;

    wxArrayString arr;
    arr.Add(input_file);
    load_files(arr);
}

void Plater::extract_config_from_project()
{
    wxString input_file = choose_project_name(this);

    if (! input_file.empty())
        load_files({ into_path(input_file) }, LoadStrategy::LoadConfig);
}

void Plater::load_gcode()
{
    // Ask user for a gcode file name.
    wxString input_file = choose_gcode_name(this);
    // And finally load the gcode file.
    load_gcode(input_file);
}

//BBS: remove GCodeViewer as seperate APP logic
void Plater::load_gcode(const wxString& filename)
{
    if (!is_gcode_file(into_u8(filename))) {
        return;
    }
        
    // Only skip if it's duplicate AND already showing in preview
    if (m_last_loaded_gcode == filename && is_preview_shown()) {
        return;
    }
   
    m_last_loaded_gcode = filename;

    // Create a new project when load_gcode
    if (new_project(false, true) != wxID_YES) {
        return;
    }
    
    // Clear camera zoom flags set by new_project to prevent interference
    p->camera.requires_zoom_to_bed = false;
    p->camera.requires_zoom_to_plate = REQUIRES_ZOOM_TO_PLATE_IDLE;

    try {
        GCodeResultWrapper* result_wrapper = p->partplate_list.get_current_slice_result_wrapper();
        if (!result_wrapper) {
            show_error(this, _L("Failed to initialize gcode result container"));
            return;
        }
        
        GCodeResult* current_result = result_wrapper->get_result();
        Print* current_print = result_wrapper->get_print();

        if (!current_result || !current_print) {
            show_error(this, _L("Failed to initialize print objects"));
            return;
        }

        wxBusyCursor wait;

        GCodeProcessor processor;
        processor.process_file(filename.ToUTF8().data());
        
        // Check IDEX mode
        const GCodeProcessorResult& processor_result = processor.get_result();
        IdexMode idex_mode = processor_result.idex_mode;
        BedDivide hot_bed_divide = processor_result.hot_bed_divide;
            
        bool is_idex_mode = (idex_mode == IdexMode_Copy || idex_mode == IdexMode_Mirror);
        
        if (is_idex_mode) {
            DynamicPrintConfig& proj_config = app_preset_bundle()->project_config;
            proj_config.set_key_value("hot_bed_divide", new ConfigOptionEnum<BedDivide>(Four_Areas));
            proj_config.set_key_value("idex_mode", new ConfigOptionEnum<IdexMode>(idex_mode));
            
            p->config->set_key_value("hot_bed_divide", new ConfigOptionEnum<BedDivide>(Four_Areas));
            p->config->set_key_value("idex_mode", new ConfigOptionEnum<IdexMode>(idex_mode));
            
            p->update_bed_shape();
            p->partplate_list.set_plate_area_count(2);
            
            current_result = result_wrapper->get_result(0);
            current_print = result_wrapper->get_print(0);
        }
        else if (idex_mode == IdexMode_Pack && hot_bed_divide == Four_Areas) {
            DynamicPrintConfig& proj_config = app_preset_bundle()->project_config;
            proj_config.set_key_value("hot_bed_divide", new ConfigOptionEnum<BedDivide>(hot_bed_divide));
            proj_config.set_key_value("idex_mode", new ConfigOptionEnum<IdexMode>(idex_mode));
            
            p->config->set_key_value("hot_bed_divide", new ConfigOptionEnum<BedDivide>(hot_bed_divide));
            p->config->set_key_value("idex_mode", new ConfigOptionEnum<IdexMode>(idex_mode));
        }
        
        *current_result = std::move(processor.extract_result());
        current_result->filename = into_u8(filename);
        
        // IDEX mode: adjust coordinates to top-left platform
        if (is_idex_mode) {
            PartPlate* current_plate = p->partplate_list.get_curr_plate();
            if (current_plate) {
                const std::vector<Pointfs>& bed_shapes = current_plate->get_shape();
                if (bed_shapes.size() == 4) {
                    const Pointfs& top_left_shape = bed_shapes[3];
                    if (!top_left_shape.empty()) {
                        Vec3d offset(top_left_shape[0].x(), top_left_shape[0].y(), 0);
                        for (auto& move : current_result->moves) {
                            move.position.x() += offset.x();
                            move.position.y() += offset.y();
                            move.position.z() += offset.z();
                            if (move.is_arc_move_with_interpolation_points()) {
                                for (auto& pt : move.interpolation_points) {
                                    pt.x() += offset.x();
                                    pt.y() += offset.y();
                                    pt.z() += offset.z();
                                }
                            }
                        }
                    }
                }
            }
        }

        BedType bed_type = current_result->bed_type;
        if (bed_type != BedType::btCount) {
            DynamicPrintConfig& proj_config = app_preset_bundle()->project_config;
            proj_config.set_key_value("curr_bed_type", new ConfigOptionEnum<BedType>(bed_type));
            on_bed_type_change(bed_type);
        }

        current_print->apply(this->model(), app_preset_bundle()->full_config());

        //BBS: add cost info when drag in gcode
        auto& ps = current_result->print_statistics;
        double total_cost = 0.0;
        for (auto volume : ps.total_volumes_per_extruder) {
            size_t extruder_id = volume.first;
            double density = current_result->filament_densities.at(extruder_id);
            double cost = current_result->filament_costs.at(extruder_id);
            double weight = volume.second * density * 0.001;
            total_cost += weight * cost * 0.001;
        }
        current_print->print_statistics().total_cost = total_cost;

        current_print->set_gcode_file_ready();
        
        // Copy gcode file to expected temporary location for export
        std::vector<std::string> area_paths = result_wrapper->get_area_gcode_paths();
        if (!area_paths.empty()) {
            try {
                boost::filesystem::create_directories(boost::filesystem::path(area_paths[0]).parent_path());
                boost::filesystem::copy_file(
                    boost::filesystem::path(into_u8(filename)),
                    boost::filesystem::path(area_paths[0]),
                    boost::filesystem::copy_option::overwrite_if_exists
                );
            } catch (const std::exception&) {
                // Ignore copy errors
            }
        }
        
        // Mark plate status
        PartPlate* current_plate = p->partplate_list.get_curr_plate();
        if (current_plate) {
            current_plate->set_has_external_gcode(true);
            current_plate->update_slice_result_valid_state(true);
        }
        
        m_exported_file = true;

        // Switch to preview tab
        AppAdapter::main_panel()->select_tab(MainPanel::tpPreview);
        select_preview(true);
        
        // Reload preview and set camera zoom
        p->preview->reload_print(false, true);
        p->camera.requires_zoom_to_plate = REQUIRES_ZOOM_TO_CUR_PLATE;
        p->preview->get_canvas3d()->set_as_dirty();

        if (p->preview->get_canvas3d()->get_gcode_layers_zs().empty()) {
            MessageDialog(this, _L("The selected file") + ":\n" + filename + "\n" + _L("does not contain valid gcode."),
                wxString(GCODEVIEWER_APP_NAME) + " - " + _L("Error occurs while loading G-code file"), wxCLOSE | wxICON_WARNING | wxCENTRE).ShowModal();
            set_project_filename(DEFAULT_PROJECT_NAME);
        }
        else {
            set_project_filename(filename);
        }

        p->view3D->get_canvas3d()->remove_raycasters_for_picking(SceneRaycaster::EType::Bed);
    }
    catch (const std::exception& ex) {
        show_error(this, wxString::Format(_L("Error loading gcode file: %s"), ex.what()));
    }
    catch (...) {
        show_error(this, _L("Unknown error occurred while loading gcode file"));
    }
}

void Plater::load_gcodes(const std::vector<wxString>& filenames)
{
    // BSS: create a new project when load_gcode, force close previous one
    if (new_project(false, true) != wxID_YES)
        return;

    GCodeResultWrapper* result_wrapper = p->partplate_list.get_current_slice_result_wrapper();
    Print* current_print = result_wrapper->get_print();

    wxBusyCursor wait;

    result_wrapper->resize(filenames.size());
    for (int i = 0, count = filenames.size(); i < count; ++i)
    {
        wxString file = filenames[i];
        if (!is_gcode_file(into_u8(file))
            || (m_last_loaded_gcode == file)
            )
            continue;

        m_last_loaded_gcode = file;

        GCodeResult* current_result = result_wrapper->get_result(i);

        // process gcode
        GCodeProcessor processor;
        try
        {
            processor.process_file(file.ToUTF8().data());
        }
        catch (const std::exception& ex)
        {
            show_error(this, ex.what());
            return;
        }
        *current_result = std::move(processor.extract_result());
        current_result->filename = file.ToStdString();

        BedType bed_type = current_result->bed_type;
        if (bed_type != BedType::btCount) {
            DynamicPrintConfig& proj_config = app_preset_bundle()->project_config;
            proj_config.set_key_value("curr_bed_type", new ConfigOptionEnum<BedType>(bed_type));
            on_bed_type_change(bed_type);
        }

        current_print->apply(this->model(), app_preset_bundle()->full_config());

        //BBS: add cost info when drag in gcode
        auto& ps = current_result->print_statistics;
        double total_cost = 0.0;
        for (auto volume : ps.total_volumes_per_extruder) {
            size_t extruder_id = volume.first;
            double density = current_result->filament_densities.at(extruder_id);
            double cost = current_result->filament_costs.at(extruder_id);
            double weight = volume.second * density * 0.001;
            total_cost += weight * cost * 0.001;
        }
        current_print->print_statistics().total_cost = total_cost;

    }
    current_print->set_gcode_file_ready();
    
    // Copy gcode files to expected temporary locations for export
    std::vector<std::string> area_paths = result_wrapper->get_area_gcode_paths();
    for (size_t i = 0; i < filenames.size() && i < area_paths.size(); ++i) {
        try {
            boost::filesystem::create_directories(boost::filesystem::path(area_paths[i]).parent_path());
            boost::filesystem::copy_file(
                boost::filesystem::path(into_u8(filenames[i])),
                boost::filesystem::path(area_paths[i]),
                boost::filesystem::copy_option::overwrite_if_exists
            );
        } catch (const std::exception&) {
            // Ignore copy errors
        }
    }
    
    // Mark plate status
    PartPlate* current_plate = p->partplate_list.get_curr_plate();
    if (current_plate) {
        current_plate->set_has_external_gcode(true);
        current_plate->update_slice_result_valid_state(true);
    }
    
    m_exported_file = true;

    // Switch to preview tab
    AppAdapter::main_panel()->select_tab(MainPanel::tpPreview);
    select_preview(true);

    // Reload preview and set camera zoom
    p->preview->reload_print(false, true);
    p->camera.requires_zoom_to_plate = REQUIRES_ZOOM_TO_CUR_PLATE;
    p->preview->get_canvas3d()->set_as_dirty();

    wxString filename = filenames.front();
    auto canvas = p->preview->get_canvas3d();
    if (canvas->get_gcode_layers_zs().empty()) {
        MessageDialog(this, _L("The selected file") + ":\n" + filename + "\n" + _L("does not contain valid gcode."),
            wxString(GCODEVIEWER_APP_NAME) + " - " + _L("Error occurs while loading G-code file"), wxCLOSE | wxICON_WARNING | wxCENTRE).ShowModal();
        set_project_filename(DEFAULT_PROJECT_NAME);
    } else {
        set_project_filename(filename);
    }

    p->view3D->get_canvas3d()->remove_raycasters_for_picking(SceneRaycaster::EType::Bed);
}

void Plater::reload_gcode_from_disk()
{
    wxString filename(m_last_loaded_gcode);
    m_last_loaded_gcode.clear();
    load_gcode(filename);
}

void Plater::refresh_print()
{
    p->preview->refresh_print();
}

// BBS
wxString Plater::get_project_name()
{
    return p->get_project_name();
}

void Plater::update_all_plate_thumbnails(bool force_update)
{
    for (int i = 0; i < get_partplate_list().get_plate_count(); i++) {
        PartPlate* plate = get_partplate_list().get_plate(i);
        ThumbnailsParams thumbnail_params = { {}, false, true, true, true, i};
        if (force_update || !plate->thumbnail_data.is_valid()) {
            get_view3D_canvas3D()->render_thumbnail(plate->thumbnail_data, plate->plate_thumbnail_width, plate->plate_thumbnail_height, thumbnail_params, Camera::EType::Ortho);
        }
        if (force_update || !plate->no_light_thumbnail_data.is_valid()) {
            get_view3D_canvas3D()->render_thumbnail(plate->no_light_thumbnail_data, plate->plate_thumbnail_width, plate->plate_thumbnail_height, thumbnail_params,
                                                    Camera::EType::Ortho,false,false,true);
        }
    }
}

//invalid all plate's thumbnails
void Plater::invalid_all_plate_thumbnails()
{
    if (using_exported_file() || skip_thumbnail_invalid)
        return;
    BOOST_LOG_TRIVIAL(info) << "thumb: invalid all";
    for (int i = 0; i < get_partplate_list().get_plate_count(); i++) {
        PartPlate* plate = get_partplate_list().get_plate(i);
        plate->thumbnail_data.reset();
        plate->no_light_thumbnail_data.reset();
    }
}

void Plater::force_update_all_plate_thumbnails()
{
    if (using_exported_file() || skip_thumbnail_invalid) {
    }
    else {
        invalid_all_plate_thumbnails();
        update_all_plate_thumbnails(true);
    }
    get_preview_canvas3D()->update_plate_thumbnails();
}

// BBS: backup
std::vector<size_t> Plater::load_files(const std::vector<fs::path>& input_files, LoadStrategy strategy, bool ask_multi) {
    //BBS: wish to reset state when load a new file
    p->m_slice_all_only_has_gcode = false;
    //BBS: wish to reset all plates stats item selected state when load a new file
    p->preview->get_canvas3d()->reset_select_plate_toolbar_selection();
    return p->load_files(input_files, strategy, ask_multi);
}

// To be called when providing a list of files to the GUI slic3r on command line.
std::vector<size_t> Plater::load_files(const std::vector<std::string>& input_files, LoadStrategy strategy,  bool ask_multi)
{
    std::vector<fs::path> paths;
    paths.reserve(input_files.size());
    for (const std::string& path : input_files)
        paths.emplace_back(path);
    return p->load_files(paths, strategy, ask_multi);
}

bool Plater::preview_zip_archive(const boost::filesystem::path& archive_path)
{
    //std::vector<fs::path> unzipped_paths;
    std::vector<fs::path> non_project_paths;
    std::vector<fs::path> project_paths;
    try
    {
        mz_zip_archive archive;
        mz_zip_zero_struct(&archive);

        if (!open_zip_reader(&archive, archive_path.string())) {
            // TRN %1% is archive path
            std::string err_msg = GUI::format(_u8L("Loading of a ZIP archive on path %1% has failed."), archive_path.string());
            throw Slic3r::FileIOError(err_msg);
        }
        mz_uint num_entries = mz_zip_reader_get_num_files(&archive);
        mz_zip_archive_file_stat stat;
        // selected_paths contains paths and its uncompressed size. The size is used to distinguish between files with same path.
        std::vector<std::pair<fs::path, size_t>> selected_paths;
        FileArchiveDialog dlg(static_cast<wxWindow*>(AppAdapter::main_panel()), &archive, selected_paths);
        if (dlg.ShowModal() == wxID_OK)
        {
            std::string archive_path_string = archive_path.string();
            archive_path_string = archive_path_string.substr(0, archive_path_string.size() - 4);
            fs::path archive_dir(wxStandardPaths::Get().GetTempDir().utf8_str().data());

            for (auto& path_w_size : selected_paths) {
                const fs::path& path = path_w_size.first;
                size_t size = path_w_size.second;
                // find path in zip archive
                for (mz_uint i = 0; i < num_entries; ++i) {
                    if (mz_zip_reader_file_stat(&archive, i, &stat)) {
                        if (size != stat.m_uncomp_size) // size must fit
                            continue;
                        wxString wname = boost::nowide::widen(stat.m_filename);
                        std::string name = boost::nowide::narrow(wname);
                        fs::path archive_path(name);

                        std::string extra(1024, 0);
                        size_t extra_size = mz_zip_reader_get_filename_from_extra(&archive, i, extra.data(), extra.size());
                        if (extra_size > 0) {
                            archive_path = fs::path(extra.substr(0, extra_size));
                            name = archive_path.string();
                        }

                        if (archive_path.empty())
                            continue;
                        if (path != archive_path)
                            continue;
                        // decompressing
                        try
                        {
                            std::replace(name.begin(), name.end(), '\\', '/');
                            // rename if file exists
                            std::string filename = path.filename().string();
                            std::string extension = path.extension().string();
                            std::string just_filename = filename.substr(0, filename.size() - extension.size());
                            std::string final_filename = just_filename;

                            size_t version = 0;
                            while (fs::exists(archive_dir / (final_filename + extension)))
                            {
                                ++version;
                                final_filename = just_filename + "(" + std::to_string(version) + ")";
                            }
                            filename = final_filename + extension;
                            fs::path final_path = archive_dir / filename;
                            std::string buffer((size_t)stat.m_uncomp_size, 0);
                            // Decompress action. We already has correct file index in stat structure.
                            mz_bool res = mz_zip_reader_extract_to_mem(&archive, stat.m_file_index, (void*)buffer.data(), (size_t)stat.m_uncomp_size, 0);
                            if (res == 0) {
                                // TRN: First argument = path to file, second argument = error description
                                wxString error_log = GUI::format_wxstr(_L("Failed to unzip file to %1%: %2%"), final_path.string(), mz_zip_get_error_string(mz_zip_get_last_error(&archive)));
                                BOOST_LOG_TRIVIAL(error) << error_log;
                                show_error(nullptr, error_log);
                                break;
                            }
                            // write buffer to file
                            fs::fstream file(final_path, std::ios::out | std::ios::binary | std::ios::trunc);
                            file.write(buffer.c_str(), buffer.size());
                            file.close();
                            if (!fs::exists(final_path)) {
                                wxString error_log = GUI::format_wxstr(_L("Failed to find unzipped file at %1%. Unzipping of file has failed."), final_path.string());
                                BOOST_LOG_TRIVIAL(error) << error_log;
                                show_error(nullptr, error_log);
                                break;
                            }
                            BOOST_LOG_TRIVIAL(info) << "Unzipped " << final_path;
                            if (!boost::algorithm::iends_with(filename, ".3mf") && !boost::algorithm::iends_with(filename, ".amf")) {
                                non_project_paths.emplace_back(final_path);
                                break;
                            }
                            // if 3mf - read archive headers to find project file
                            if (/*(boost::algorithm::iends_with(filename, ".3mf") && !is_project_3mf(final_path.string())) ||*/
                                (boost::algorithm::iends_with(filename, ".amf") && !boost::algorithm::iends_with(filename, ".zip.amf"))) {
                                non_project_paths.emplace_back(final_path);
                                break;
                            }

                            project_paths.emplace_back(final_path);
                            break;
                        }
                        catch (const std::exception& e)
                        {
                            // ensure the zip archive is closed and rethrow the exception
                            close_zip_reader(&archive);
                            throw Slic3r::FileIOError(e.what());
                        }
                    }
                }
            }
            close_zip_reader(&archive);
            if (non_project_paths.size() + project_paths.size() != selected_paths.size())
                BOOST_LOG_TRIVIAL(error) << "Decompresing of archive did not retrieve all files. Expected files: "
                                         << selected_paths.size()
                                         << " Decopressed files: "
                                         << non_project_paths.size() + project_paths.size();
        } else {
            close_zip_reader(&archive);
            return false;
        }

    }
    catch (const Slic3r::FileIOError& e) {
        // zip reader should be already closed or not even opened
        GUI::show_error(this, e.what());
        return false;
    }
    // none selected
    if (project_paths.empty() && non_project_paths.empty())
    {
        return false;
    }

    // 1 project file and some models - behave like drag n drop of 3mf and then load models
    if (project_paths.size() == 1)
    {
        wxArrayString aux;
        aux.Add(from_u8(project_paths.front().string()));
        bool loaded3mf = load_files(aux);
        load_files(non_project_paths, LoadStrategy::LoadModel);
        boost::system::error_code ec;
        if (loaded3mf) {
            fs::remove(project_paths.front(), ec);
            if (ec)
                BOOST_LOG_TRIVIAL(error) << ec.message();
        }
        for (const fs::path& path : non_project_paths) {
            // Delete file from temp file (path variable), it will stay only in app memory.
            boost::system::error_code ec;
            fs::remove(path, ec);
            if (ec)
                BOOST_LOG_TRIVIAL(error) << ec.message();
        }
        return true;
    }

    // load all projects and all models as geometry
    load_files(project_paths, LoadStrategy::LoadModel);
    load_files(non_project_paths, LoadStrategy::LoadModel);


    for (const fs::path& path : project_paths) {
        // Delete file from temp file (path variable), it will stay only in app memory.
        boost::system::error_code ec;
        fs::remove(path, ec);
        if (ec)
            BOOST_LOG_TRIVIAL(error) << ec.message();
    }
    for (const fs::path& path : non_project_paths) {
        // Delete file from temp file (path variable), it will stay only in app memory.
        boost::system::error_code ec;
        fs::remove(path, ec);
        if (ec)
            BOOST_LOG_TRIVIAL(error) << ec.message();
    }

    return true;
}

class RadioBox;

//BBS: remove GCodeViewer as seperate APP logic
bool Plater::load_files(const wxArrayString& filenames)
{
    const std::regex pattern_drop(".*[.](stp|step|stl|oltp|obj|amf|3mf|svg|zip)", std::regex::icase);
    const std::regex pattern_gcode_drop(".*[.](gcode|g)", std::regex::icase);

    std::vector<fs::path> normal_paths;
    std::vector<fs::path> gcode_paths;
    std::vector<wxString> gcode_paths_wx;

    for (const auto& filename : filenames) {
        fs::path path(into_path(filename));
        if (std::regex_match(path.string(), pattern_drop))
            normal_paths.push_back(std::move(path));
        else if (std::regex_match(path.string(), pattern_gcode_drop))
        {
            gcode_paths.push_back(path);
            gcode_paths_wx.push_back(from_path(path));
        }
        else
            continue;
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": normal_paths %1%, gcode_paths %2%")%normal_paths.size() %gcode_paths.size();
    if (normal_paths.empty() && gcode_paths.empty()) {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << boost::format(": can not find valid path, return directly");
        // Likely no supported files
        return false;
    }
    else if (normal_paths.empty()){
        if (gcode_paths.size() > 1)
            load_gcodes(gcode_paths_wx);
        else 
            load_gcode(from_path(gcode_paths.front()));
        return true;
    }

    if (!gcode_paths.empty()) {
        show_info(this, _L("G-code files can not be loaded with models together!"), _L("G-code loading"));
        return false;
    }

    //// other files
    std::string snapshot_label;
    assert(!normal_paths.empty());
    if (normal_paths.size() == 1) {
        snapshot_label = "Load File";
        snapshot_label += ": ";
        snapshot_label += encode_path(normal_paths.front().filename().string().c_str());
    } else {
        snapshot_label = "Load Files";
        snapshot_label += ": ";
        snapshot_label += encode_path(normal_paths.front().filename().string().c_str());
        for (size_t i = 1; i < normal_paths.size(); ++i) {
            snapshot_label += ", ";
            snapshot_label += encode_path(normal_paths[i].filename().string().c_str());
        }
    }

    // BBS: check file types
    std::sort(normal_paths.begin(), normal_paths.end(), [](fs::path obj1, fs::path obj2) { return obj1.filename().string() < obj2.filename().string(); });

    auto loadfiles_type  = LoadFilesType::NoFile;
    auto amf_files_count = get_3mf_file_count(normal_paths);

    if (normal_paths.size() > 1 && amf_files_count < normal_paths.size()) { loadfiles_type = LoadFilesType::Multiple3MFOther; }
    if (normal_paths.size() > 1 && amf_files_count == normal_paths.size()) { loadfiles_type = LoadFilesType::Multiple3MF; }
    if (normal_paths.size() > 1 && amf_files_count == 0) { loadfiles_type = LoadFilesType::MultipleOther; }
    if (normal_paths.size() == 1 && amf_files_count == 1) { loadfiles_type = LoadFilesType::Single3MF; };
    if (normal_paths.size() == 1 && amf_files_count == 0) { loadfiles_type = LoadFilesType::SingleOther; };

    auto first_file = std::vector<fs::path>{};
    auto tmf_file   = std::vector<fs::path>{};
    auto other_file = std::vector<fs::path>{};
    auto res        = true;

    if (this->m_exported_file) {
        if ((loadfiles_type == LoadFilesType::SingleOther)
            || (loadfiles_type == LoadFilesType::MultipleOther)) {
            show_info(this, _L("Can not add models when in preview mode!"), _L("Add Models"));
            return false;
        }
    }

    // Orca: Iters through given paths and imports files from zip then remove zip from paths
    // returns true if zip files were found
    auto handle_zips = [this](std::vector<fs::path>& paths) { // NOLINT(*-no-recursion) - Recursion is intended and should be managed properly
        bool res = false;
        for (auto it = paths.begin(); it != paths.end();) {
            if (boost::algorithm::iends_with(it->string(), ".zip")) {
                res = true;
                preview_zip_archive(*it);
                it = paths.erase(it);
            } else
                it++;
        }
        return res;
    };

    switch (loadfiles_type) {
    case LoadFilesType::Single3MF:
        open_3mf_file(normal_paths[0]);
        break;

    case LoadFilesType::SingleOther: {
        Plater::TakeSnapshot snapshot(this, snapshot_label);
        if (handle_zips(normal_paths)) return true;
        if (load_files(normal_paths, LoadStrategy::LoadModel, false).empty()) { res = false; }
        break;
    }
    case LoadFilesType::Multiple3MF:
        first_file = std::vector<fs::path>{normal_paths[0]};
        for (auto i = 0; i < normal_paths.size(); i++) {
            if (i > 0) { other_file.push_back(normal_paths[i]); }
        };

        open_3mf_file(first_file[0]);
        if (load_files(other_file, LoadStrategy::LoadModel).empty()) {  res = false;  }
        break;

    case LoadFilesType::MultipleOther: {
        Plater::TakeSnapshot snapshot(this, snapshot_label);
        if (handle_zips(normal_paths)) {
            if (normal_paths.empty()) return true;
        }
        if (load_files(normal_paths, LoadStrategy::LoadModel, true).empty()) { res = false; }
        break;
    }

    case LoadFilesType::Multiple3MFOther:
        for (const auto &path : normal_paths) {
            if (boost::iends_with(path.filename().string(), ".3mf")){
                if (first_file.size() <= 0)
                    first_file.push_back(path);
                else
                    tmf_file.push_back(path);
            } else {
                other_file.push_back(path);
            }
        }

        open_3mf_file(first_file[0]);
        if (load_files(tmf_file, LoadStrategy::LoadModel).empty()) {  res = false;  }
        if (res && handle_zips(other_file)) {
            if (normal_paths.empty()) return true;
        }
        if (load_files(other_file, LoadStrategy::LoadModel, false).empty()) {  res = false;  }
        break;
    default: break;
    }

    return res;
}

LoadType determine_load_type(std::string filename, std::string override_setting)
{
    std::string setting;

    if (override_setting != "") {
        setting = override_setting;
    } else {
        setting = AppAdapter::app_config()->get(SETTING_PROJECT_LOAD_BEHAVIOUR);
    }

    if (setting == OPTION_PROJECT_LOAD_BEHAVIOUR_LOAD_GEOMETRY) {
        return LoadType::LoadGeometry;
    } else if (setting == OPTION_PROJECT_LOAD_BEHAVIOUR_ALWAYS_ASK) {
        ProjectDropDialog dlg(filename);
        if (dlg.ShowModal() == wxID_OK) {
            int      choice    = dlg.get_action();
            LoadType load_type = static_cast<LoadType>(choice);
            AppAdapter::app_config()->set("import_project_action", std::to_string(choice));

            // BBS: jump to plater panel
            AppAdapter::main_panel()->select_tab(MainPanel::tp3DEditor);
            return load_type;
        }

        return LoadType::Unknown; // Cancel
    } else {
        return LoadType::OpenProject;
    }
}

bool Plater::open_3mf_file(const fs::path &file_path)
{
    std::string filename = encode_path(file_path.filename().string().c_str());
    if (!boost::algorithm::iends_with(filename, ".3mf")) {
        return false;
    }

    bool not_empty_plate = !model().objects.empty();
    bool load_setting_ask_when_relevant = AppAdapter::app_config()->get(SETTING_PROJECT_LOAD_BEHAVIOUR) == OPTION_PROJECT_LOAD_BEHAVIOUR_ASK_WHEN_RELEVANT;
    LoadType load_type = determine_load_type(filename, (not_empty_plate && load_setting_ask_when_relevant) ? OPTION_PROJECT_LOAD_BEHAVIOUR_ALWAYS_ASK : "");

    if (load_type == LoadType::Unknown) return false;

    switch (load_type) {
        case LoadType::OpenProject: {
            load_project(from_path(file_path), "<loadall>");
            break;
        }
        case LoadType::LoadGeometry: {
            Plater::TakeSnapshot snapshot(this, "Import Object");
            load_files({file_path}, LoadStrategy::LoadModel);
            break;
        }
        case LoadType::LoadConfig: {
            load_files({file_path}, LoadStrategy::LoadConfig);
            break;
        }
        case LoadType::Unknown: {
            assert(false);
            break;
        }
    }

    return true;
}

int Plater::get_3mf_file_count(std::vector<fs::path> paths)
{
    auto count = 0;
    for (const auto &path : paths) {
        if (boost::iends_with(path.filename().string(), ".3mf")) {
            count++;
        }
    }
    return count;
}

void Plater::add_file()
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << __LINE__ << " entry";
    wxArrayString input_files = choose_model_name(this);
    if (input_files.empty()) return;

    std::vector<fs::path> paths;
    for (const auto &file : input_files) paths.emplace_back(into_path(file));

    std::string snapshot_label;
    assert(!paths.empty());

    snapshot_label = "Import Objects";
    snapshot_label += ": ";
    snapshot_label += encode_path(paths.front().filename().string().c_str());
    for (size_t i = 1; i < paths.size(); ++i) {
        snapshot_label += ", ";
        snapshot_label += encode_path(paths[i].filename().string().c_str());
    }

    // BBS: check file types
    auto loadfiles_type  = LoadFilesType::NoFile;
    auto amf_files_count = get_3mf_file_count(paths);

    if (paths.size() > 1 && amf_files_count < paths.size()) { loadfiles_type = LoadFilesType::Multiple3MFOther; }
    if (paths.size() > 1 && amf_files_count == paths.size()) { loadfiles_type = LoadFilesType::Multiple3MF; }
    if (paths.size() > 1 && amf_files_count == 0) { loadfiles_type = LoadFilesType::MultipleOther; }
    if (paths.size() == 1 && amf_files_count == 1) { loadfiles_type = LoadFilesType::Single3MF; };
    if (paths.size() == 1 && amf_files_count == 0) { loadfiles_type = LoadFilesType::SingleOther; };

    auto first_file = std::vector<fs::path>{};
    auto tmf_file   = std::vector<fs::path>{};
    auto other_file = std::vector<fs::path>{};

    switch (loadfiles_type)
    {
    case LoadFilesType::Single3MF:
        open_3mf_file(paths[0]);
    	break;

    case LoadFilesType::SingleOther: {
        Plater::TakeSnapshot snapshot(this, snapshot_label);
        if (!load_files(paths, LoadStrategy::LoadModel, false).empty()) {
            if (get_project_name() == _L("Untitled") && paths.size() > 0) {
                boost::filesystem::path full_path(paths[0].string());
                p->set_project_name(from_u8(full_path.stem().string()));
            }
            AppAdapter::main_panel()->update_title();
        }
        break;
    }
    case LoadFilesType::Multiple3MF:
        first_file = std::vector<fs::path>{paths[0]};
        for (auto i = 0; i < paths.size(); i++) {
            if (i > 0) { other_file.push_back(paths[i]); }
        };

        open_3mf_file(first_file[0]);
        if (!load_files(other_file, LoadStrategy::LoadModel).empty()) { AppAdapter::main_panel()->update_title(); }
        break;

    case LoadFilesType::MultipleOther: {
        Plater::TakeSnapshot snapshot(this, snapshot_label);
        if (!load_files(paths, LoadStrategy::LoadModel, true).empty()) {
            if (get_project_name() == _L("Untitled") && paths.size() > 0) {
                boost::filesystem::path full_path(paths[0].string());
                p->set_project_name(from_u8(full_path.stem().string()));
            }
            AppAdapter::main_panel()->update_title();
        }
        break;
    }
    case LoadFilesType::Multiple3MFOther:
        for (const auto &path : paths) {
            if (boost::iends_with(path.filename().string(), ".3mf")) {
                if (first_file.size() <= 0)
                    first_file.push_back(path);
                else
                    tmf_file.push_back(path);
            } else {
                other_file.push_back(path);
            }
        }

        open_3mf_file(first_file[0]);
        load_files(tmf_file, LoadStrategy::LoadModel);
        if (!load_files(other_file, LoadStrategy::LoadModel, false).empty()) { AppAdapter::main_panel()->update_title();}
        break;
    default:break;
    }
}

void Plater::update(bool conside_update_flag, bool force_background_processing_update)
{
    unsigned int flag = force_background_processing_update ? (unsigned int)Plater::priv::UpdateParams::FORCE_BACKGROUND_PROCESSING_UPDATE : 0;
    if (conside_update_flag) {
        if (need_update()) {
            p->update(flag);
            p->set_need_update(false);
        }
    }
    else
        p->update(flag);
}

void Plater::object_list_changed() { p->object_list_changed(); }

Worker &Plater::get_ui_job_worker() { return p->m_worker; }

const Worker &Plater::get_ui_job_worker() const { return p->m_worker; }

void Plater::update_ui_from_settings() { p->update_ui_from_settings(); }

void Plater::select_view(const std::string& direction) { p->select_view(direction); }

void Plater::select_view3d(bool no_slice)
{
    p->select_view3d(no_slice);
}

void Plater::select_preview(bool no_slice)
{
    p->select_preview(no_slice);
}

void Plater::reload_paint_after_background_process_apply() {
    p->preview->set_reload_paint_after_background_process_apply(true);
}

bool Plater::is_preview_shown() const { return p->is_preview_shown(); }
bool Plater::is_preview_loaded() const { return p->is_preview_loaded(); }
bool Plater::is_view3D_shown() const { return p->is_view3D_shown(); }

bool Plater::are_view3D_labels_shown() const { return p->are_view3D_labels_shown(); }
void Plater::show_view3D_labels(bool show) { p->show_view3D_labels(show); }

bool Plater::is_view3D_overhang_shown() const { return p->is_view3D_overhang_shown(); }
void Plater::show_view3D_overhang(bool show)  {  p->show_view3D_overhang(show); }

bool Plater::is_sidebar_enabled() const { return p->sidebar_layout.is_enabled; }
void Plater::enable_sidebar(bool enabled) { p->enable_sidebar(enabled); }
bool Plater::is_sidebar_collapsed() const { return p->sidebar_layout.is_collapsed; }
void Plater::collapse_sidebar(bool collapse) { p->collapse_sidebar(collapse); }
Sidebar::DockingState Plater::get_sidebar_docking_state() const { return p->get_sidebar_docking_state(); }

void Plater::reset_window_layout() { p->reset_window_layout(); }

//BBS
void Plater::select_curr_plate_all() { p->select_curr_plate_all(); }
void Plater::remove_curr_plate_all() { p->remove_curr_plate_all(); }

void Plater::select_all() { p->select_all(); }
void Plater::deselect_all() { p->deselect_all(); }
void Plater::exit_gizmo() { p->exit_gizmo(); }

void Plater::remove(size_t obj_idx) { p->remove(obj_idx); }
void Plater::reset(bool apply_presets_change) { p->reset(apply_presets_change); }
void Plater::reset_with_confirm()
{
    if (p->model.objects.empty() || MessageDialog(static_cast<wxWindow *>(this), _L("All objects will be removed, continue?"),
                                                  wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Delete all"), wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxCENTRE)
                                            .ShowModal() == wxID_YES) {
        reset();
        // BBS: jump to plater panel
        AppAdapter::main_panel()->select_tab(size_t(0));
    }
}

// BBS: save logic
int GUI::Plater::close_with_confirm(std::function<bool(bool)> second_check)
{
    if (up_to_date(false, false)) {
        if (second_check && !second_check(false)) return wxID_CANCEL;
        model().set_backup_path("");
        return wxID_NO;
    }

    MessageDialog dlg(static_cast<wxWindow*>(this), _L("The current project has unsaved changes, save it before continue?"),
        wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Save"), wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxCENTRE);
    dlg.show_dsa_button(_L("Remember my choice."));
    auto choise = AppAdapter::app_config()->get("save_project_choise");
    auto result = choise.empty() ? dlg.ShowModal() : choise == "yes" ? wxID_YES : wxID_NO;
    if (result == wxID_CANCEL)
        return result;
    else {
        if (dlg.get_checkbox_state())
            AppAdapter::app_config()->set("save_project_choise", result == wxID_YES ? "yes" : "no");
        if (result == wxID_YES) {
            result = save_project();
            if (result == wxID_CANCEL) {
                if (choise.empty())
                    return result;
                else
                    result = wxID_NO;
            }
        }
    }

    if (second_check && !second_check(result == wxID_YES)) return wxID_CANCEL;

    model().set_backup_path("");
    up_to_date(true, false);
    up_to_date(true, true);

    return result;
}

//BBS: trigger a restore project event
void Plater::trigger_restore_project(int skip_confirm)
{
    auto evt = new wxCommandEvent(EVT_RESTORE_PROJECT, this->GetId());
    evt->SetInt(skip_confirm);
    wxQueueEvent(this, evt);
    //wxPostEvent(this, *evt);
}

//BBS
bool Plater::delete_object_from_model(size_t obj_idx, bool refresh_immediately) { return p->delete_object_from_model(obj_idx, refresh_immediately); }

//BBS: delete all from model
void Plater::delete_all_objects_from_model()
{
    p->delete_all_objects_from_model();
}

void Plater::set_selected_visible(bool visible)
{
    if (p->get_selection().is_empty())
        return;

    Plater::TakeSnapshot snapshot(this, "Set Selected Objects Visible in AssembleView");
    get_ui_job_worker().cancel_all();

    p->m_canvas->set_selected_visible(visible);
}


void Plater::remove_selected()
{
    /*if (p->get_selection().is_empty())
        return;*/
    if (p->get_selection().is_empty())
        return;

    // BBS: check before deleting object
    if (!p->can_delete())
        return;

    Plater::TakeSnapshot snapshot(this, "Delete Selected Objects");
    get_ui_job_worker().cancel_all();

    //BBS delete current selected
    // p->view3D->delete_selected();
    p->view3D->delete_selected();
}

static long GetNumberFromUser(  const wxString& msg,
                                const wxString& prompt,
                                const wxString& title,
                                long value,
                                long min,
                                long max,
                                wxWindow* parent)
{
#ifdef _WIN32
    wxNumberEntryDialog dialog(parent, msg, prompt, title, value, min, max, wxDefaultPosition);
    UpdateDlgDarkUI(&dialog);
    if (dialog.ShowModal() == wxID_OK)
        return dialog.GetValue();

    return -1;
#else
    return wxGetNumberFromUser(msg, prompt, title, value, min, max, parent);
#endif
}

void Plater::fill_bed_with_instances()
{
    auto &w = get_ui_job_worker();
    if (w.is_idle()) {
        p->take_snapshot(_u8L("Arrange"));
        replace_job(w, std::make_unique<FillBedJob>());
    }
}

bool Plater::is_selection_empty() const
{
    return p->get_selection().is_empty() || p->get_selection().is_wipe_tower();
}

void Plater::scale_selection_to_fit_print_volume()
{
    p->scale_selection_to_fit_print_volume();
}

void Plater::convert_unit(ConversionType conv_type)
{
    std::vector<int> obj_idxs, volume_idxs;
    AppAdapter::obj_list()->get_selection_indexes(obj_idxs, volume_idxs);
    if (obj_idxs.empty() && volume_idxs.empty())
        return;

    TakeSnapshot snapshot(this, conv_type == ConversionType::CONV_FROM_INCH  ? "Convert from imperial units" :
                                conv_type == ConversionType::CONV_TO_INCH    ? "Revert conversion from imperial units" :
                                conv_type == ConversionType::CONV_FROM_METER ? "Convert from meters" : "Revert conversion from meters");
    wxBusyCursor wait;

    ModelObjectPtrs objects;
    std::reverse(obj_idxs.begin(), obj_idxs.end());
    for (int obj_idx : obj_idxs) {
        ModelObject *object = p->model.objects[obj_idx];
        object->convert_units(objects, conv_type, volume_idxs);
        remove(obj_idx);
    }
    std::reverse(objects.begin(), objects.end());
    p->load_model_objects(objects);

    Selection& selection = p->get_selection();
    size_t last_obj_idx = p->model.objects.size() - 1;

    if (volume_idxs.empty()) {
        for (size_t i = 0; i < objects.size(); ++i)
            selection.add_object((unsigned int)(last_obj_idx - i), i == 0);
    }
    else {
        for (int vol_idx : volume_idxs)
            selection.add_volume(last_obj_idx, vol_idx, 0, false);
    }
}

void Plater::apply_cut_object_to_model(size_t obj_idx, const ModelObjectPtrs& new_objects)
{
    model().delete_object(obj_idx);
    sidebar().obj_list()->delete_object_from_list(obj_idx);

    // suppress to call selection update for Object List to avoid call of early Gizmos on/off update
    p->load_model_objects(new_objects, false, false);

    // now process all updates of the 3d scene
    update();
    // Update InfoItems in ObjectList after update() to use of a correct value of the GLCanvas3D::is_sinking(),
    // which is updated after a view3D->reload_scene(false, flags & (unsigned int)UpdateParams::FORCE_FULL_SCREEN_REFRESH) call
    for (size_t idx = 0; idx < p->model.objects.size(); idx++)
        AppAdapter::obj_list()->update_info_items(idx);

    Selection& selection = p->get_selection();
    size_t last_id = p->model.objects.size() - 1;
    for (size_t i = 0; i < new_objects.size(); ++i)
        selection.add_object((unsigned int)(last_id - i), i == 0);
}

void Plater::export_gcode(bool prefer_removable)
{
    GCodeExportParam param;
    PartPlate* pp = p->partplate_list.get_curr_plate();

    ExportResult result = export_gcode_from_part_plate(pp, param);

    if(result.success)
    {
        std::string last_output_path = result.last_output_path;
        std::string last_output_dir_path = result.last_output_dir_path;

        if (result.is_removable_path) {
            p->notification_manager->push_exporting_finished_notification(last_output_path, last_output_dir_path,
                // Don't offer the "Eject" button on ChromeOS, the Linux side has no control over it.
                platform_flavor() != PlatformFlavor::LinuxOnChromium);
            AppAdapter::gui_app()->removable_drive_manager()->set_exporting_finished(true);
        }else
        {
            p->notification_manager->push_exporting_finished_notification(last_output_path, last_output_dir_path, false);
        }
    }else{

    }
}

//BBS export gcode 3mf to file
void Plater::export_gcode_3mf(bool export_all)
{
    if (p->model.objects.empty())
        return;

    if (p->process_completed_with_error == p->partplate_list.get_curr_plate_index())
        return;

    //calc default_output_file, get default output file from background process
    fs::path default_output_file;
    AppConfig& appconfig = *AppAdapter::app_config();
    std::string start_dir;
    try {
        // Update the background processing, so that the placeholder parser will get the correct values for the ouput file template.
        // Also if there is something wrong with the current configuration, a pop-up dialog will be shown and the export will not be performed.
        unsigned int state = this->p->update_restart_background_process(false, false);
        if (state & priv::UPDATE_BACKGROUND_PROCESS_INVALID)
            return;
        // default_output_file = this->p->background_process.output_filepath_for_project("");
    }
    catch (const Slic3r::PlaceholderParserError& ex) {
        // Show the error with monospaced font.
        show_error(this, ex.what(), true);
        return;
    }
    catch (const std::exception& ex) {
        show_error(this, ex.what(), false);
        return;
    }
    default_output_file.replace_extension(".gcode.3mf");
    default_output_file = fs::path(Slic3r::fold_utf8_to_ascii(default_output_file.string()));

    //Get a last save path
    start_dir = appconfig.get_last_output_dir(default_output_file.parent_path().string(), false);

    fs::path output_path;
    {
        std::string ext = default_output_file.extension().string();
        wxFileDialog dlg(this, _L("Save Sliced file as:"),
            start_dir,
            from_path(default_output_file.filename()),
            file_wildcards(FT_GCODE_3MF, ""),
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT
        );
        if (dlg.ShowModal() == wxID_OK) {
            output_path = into_path(dlg.GetPath());
            ext = output_path.extension().string();
            if (ext != ".3mf")
                output_path = output_path.string() + ".3mf";
        }
    }

    if (!output_path.empty()) {
        //BBS do not set to removable media path
        bool path_on_removable_media = false;
        p->notification_manager->new_export_began(path_on_removable_media);
        p->exporting_status = path_on_removable_media ? ExportingStatus::EXPORTING_TO_REMOVABLE : ExportingStatus::EXPORTING_TO_LOCAL;
        //BBS do not save last output path
        std::string last_output_path = output_path.string();
        std::string last_output_dir_path = output_path.parent_path().string();
        int plate_idx = get_partplate_list().get_curr_plate_index();
        if (export_all)
            plate_idx = PLATE_ALL_IDX;
        export_3mf(output_path, SaveStrategy::Silence | SaveStrategy::SplitModel | SaveStrategy::WithGcode | SaveStrategy::SkipModel, plate_idx); // BBS: silence

        RemovableDriveManager& removable_drive_manager = *AppAdapter::gui_app()->removable_drive_manager();


        bool on_removable = removable_drive_manager.is_path_on_removable_drive(last_output_dir_path);


        // update last output dir
        appconfig.update_last_output_dir(output_path.parent_path().string(), false);
        p->notification_manager->push_exporting_finished_notification(output_path.string(), last_output_dir_path, on_removable);
    }
}

void Plater::send_gcode_finish(wxString name)
{
    auto out_str = GUI::format(_L("The file %s has been sent to the printer's storage space and can be viewed on the printer."), name);
    p->notification_manager->push_exporting_finished_notification(out_str, "", false);
}

void Plater::export_core_3mf()
{
    wxString path = p->get_export_file(FT_3MF);
    if (path.empty()) { return; }
    const std::string path_u8 = into_u8(path);
    export_3mf(path_u8, SaveStrategy::Silence);
}

// BBS export with/without boolean, however, stil merge mesh
#define EXPORT_WITH_BOOLEAN 0
void Plater::export_stl(bool extended, bool selection_only, bool multi_stls)
{
    if (p->model.objects.empty()) { return; }

    wxString path;
    if (multi_stls) {
        wxDirDialog dlg(this, _L("Choose a directory"), from_u8(AppAdapter::app_config()->get_last_dir()),
                        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK) {
            path = dlg.GetPath() + "/";
        }
    } else {
        path = p->get_export_file(FT_STL);
    }
    if (path.empty()) { return; }
    const std::string path_u8 = into_u8(path);

    wxBusyCursor wait;
    const auto& selection = p->get_selection();
    const auto obj_idx = selection.get_object_idx();

#if EXPORT_WITH_BOOLEAN
    if (selection_only && (obj_idx == -1 || selection.is_wipe_tower()))
        return;
#else
    // BBS support selecting multiple objects
    if (selection_only && selection.is_wipe_tower()) return;

    // BBS
    if (selection_only) {
        // only support selection single full object and mulitiple full object
        if (!selection.is_single_full_object() && !selection.is_multiple_full_object()) return;
    }
#endif

    std::function<TriangleMesh(const ModelObject& mo, int instance_id)>
        mesh_to_export;

#if EXPORT_WITH_BOOLEAN
    mesh_to_export = [this](const ModelObject& mo, int instance_id) {return Plater::combine_mesh_fff(mo, instance_id,
        [this](const std::string& msg) {return get_notification_manager()->push_plater_error_notification(msg); }); };
#else
    mesh_to_export = mesh_to_export_fff_no_boolean;
#endif

    auto get_save_file = [](std::string const & dir, std::string const & name) {
        auto path = dir + name + ".stl";
        int n = 1;
        while (boost::filesystem::exists(path))
            path = dir + name + "(" + std::to_string(n++) + ").stl";
        return path;
    };

    TriangleMesh mesh;
    if (selection_only) {
        if (selection.is_single_full_object()) {
            const auto obj_idx = selection.get_object_idx();
            const ModelObject* model_object = p->model.objects[obj_idx];
            if (selection.get_mode() == Selection::Instance)
                mesh = mesh_to_export(*model_object, (model_object->instances.size() > 1) ? -1 : selection.get_instance_idx());
            else {
                const GLVolume* volume = selection.get_first_volume();
                mesh = model_object->volumes[volume->volume_idx()]->mesh();
                mesh.transform(volume->get_volume_transformation().get_matrix(), true);
            }

            if (model_object->instances.size() == 1) mesh.translate(-model_object->origin_translation.cast<float>());
        }
        else if (selection.is_multiple_full_object() && !multi_stls) {
            const std::set<std::pair<int, int>>& instances_idxs = p->get_selection().get_selected_object_instances();
            for (const std::pair<int, int>& i : instances_idxs) {
                ModelObject* object = p->model.objects[i.first];
                mesh.merge(mesh_to_export(*object, i.second));
            }
        }
        else if (selection.is_multiple_full_object() && multi_stls) {
            const std::set<std::pair<int, int>> &instances_idxs = p->get_selection().get_selected_object_instances();
            for (const std::pair<int, int> &i : instances_idxs) {
                ModelObject *object = p->model.objects[i.first];
                auto mesh = mesh_to_export(*object, i.second);
                mesh.translate(-object->origin_translation.cast<float>());

                Slic3r::store_stl(get_save_file(path_u8, object->name).c_str(), &mesh, true);
            }
            return;
        }
    }
    else if (!multi_stls) {
        for (const ModelObject* o : p->model.objects) {
            mesh.merge(mesh_to_export(*o, -1));
        }
    } else {
        for (const ModelObject* o : p->model.objects) {
            auto mesh = mesh_to_export(*o, -1);
            mesh.translate(-o->origin_translation.cast<float>());
            Slic3r::store_stl(get_save_file(path_u8, o->name).c_str(), &mesh, true);
        }
        return;
    }

    Slic3r::store_stl(path_u8.c_str(), &mesh, true);
}

namespace {
std::string get_file_name(const std::string &file_path)
{
    size_t pos_last_delimiter = file_path.find_last_of("/\\");
    size_t pos_point          = file_path.find_last_of('.');
    size_t offset             = pos_last_delimiter + 1;
    size_t count              = pos_point - pos_last_delimiter - 1;
    return file_path.substr(offset, count);
}
using SvgFile = EmbossShape::SvgFile;
using SvgFiles = std::vector<SvgFile*>;
std::string create_unique_3mf_filepath(const std::string &file, const SvgFiles svgs)
{
    // const std::string MODEL_FOLDER = "3D/"; // copy from file 3mf.cpp
    std::string path_in_3mf = "3D/" + file + ".svg";
    size_t suffix_number = 0;
    bool is_unique = false;
    do{
        is_unique = true;
        path_in_3mf = "3D/" + file + ((suffix_number++)? ("_" + std::to_string(suffix_number)) : "") + ".svg";
        for (SvgFile *svgfile : svgs) {
            if (svgfile->path_in_3mf.empty())
                continue;
            if (svgfile->path_in_3mf.compare(path_in_3mf) == 0) {
                is_unique = false;
                break;
            }
        } 
    } while (!is_unique);
    return path_in_3mf;
}

bool set_by_local_path(SvgFile &svg, const SvgFiles& svgs)
{
    // Try to find already used svg file
    for (SvgFile *svg_ : svgs) {
        if (svg_->path_in_3mf.empty())
            continue;
        if (svg.path.compare(svg_->path) == 0) {
            svg.path_in_3mf = svg_->path_in_3mf;
            return true;
        }
    }
    return false;
}

/// <summary>
/// Function to secure private data before store to 3mf
/// </summary>
/// <param name="model">Data(also private) to clean before publishing</param>
void publish(Model &model, SaveStrategy strategy) {

    // SVG file publishing
    bool exist_new = false;
    SvgFiles svgfiles;
    for (ModelObject *object: model.objects){
        for (ModelVolume *volume : object->volumes) {
            if (!volume->emboss_shape.has_value())
                continue;
            if (volume->text_configuration.has_value())
                continue; // text dosen't have svg path

            assert(volume->emboss_shape->svg_file.has_value());
            if (!volume->emboss_shape->svg_file.has_value())
                continue;

            SvgFile* svg = &(*volume->emboss_shape->svg_file);
            if (svg->path_in_3mf.empty())
                exist_new = true;
            svgfiles.push_back(svg);
        }
    }

    // Orca: don't show this in silence mode
    if (exist_new && !(strategy & SaveStrategy::Silence)) {
        MessageDialog dialog(nullptr,
                             _L("Are you sure you want to store original SVGs with their local paths into the 3MF file?\n"
                                "If you hit 'NO', all SVGs in the project will not be editable any more."),
                             _L("Private protection"), wxYES_NO | wxICON_QUESTION);
        if (dialog.ShowModal() == wxID_NO){
            for (ModelObject *object : model.objects) 
                for (ModelVolume *volume : object->volumes)
                    if (volume->emboss_shape.has_value())
                        volume->emboss_shape.reset();
        }
    }

    for (SvgFile* svgfile : svgfiles){
        if (!svgfile->path_in_3mf.empty())
            continue; // already suggested path (previous save)

        // create unique name for svgs, when local path differ
        std::string filename = "unknown";
        if (!svgfile->path.empty()) {
            if (set_by_local_path(*svgfile, svgfiles))
                continue;
            // check whether original filename is already in:
            filename = get_file_name(svgfile->path);
        }
        svgfile->path_in_3mf = create_unique_3mf_filepath(filename, svgfiles);        
    }
}
}

// BBS: backup
int Plater::export_3mf(const boost::filesystem::path& output_path, SaveStrategy strategy, int export_plate_idx, Export3mfProgressFn proFn)
{
    int ret = 0;
    if (output_path.empty())
        return -1;

    bool export_config = true;
    wxString path = from_path(output_path);

    if (!path.Lower().EndsWith(".3mf"))
        return -1;

    // take care about private data stored into .3mf
    // modify model
    publish(p->model, strategy);

    DynamicPrintConfig cfg = app_preset_bundle()->full_config_secure();
    const std::string path_u8 = into_u8(path);
    wxBusyCursor wait;

    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << boost::format(": path=%1%, backup=%2%, export_plate_idx=%3%, SaveStrategy=%4%")
        %output_path.string()%(strategy & SaveStrategy::Backup)%export_plate_idx %(unsigned int)strategy;

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": path=%1%, backup=%2%, export_plate_idx=%3%, SaveStrategy=%4%")
        % std::string("") % (strategy & SaveStrategy::Backup) % export_plate_idx % (unsigned int)strategy;

    //BBS: add plate logic for thumbnail generate
    std::vector<ThumbnailData*> thumbnails;
    std::vector<ThumbnailData*> no_light_thumbnails;
    std::vector<ThumbnailData*> calibration_thumbnails;
    std::vector<ThumbnailData*> top_thumbnails;
    std::vector<ThumbnailData*> picking_thumbnails;
    std::vector<PlateBBoxData*> plate_bboxes;
    // BBS: backup
    if (!(strategy & SaveStrategy::Backup)) {
        for (int i = 0; i < p->partplate_list.get_plate_count(); i++) {
            ThumbnailData* thumbnail_data = &p->partplate_list.get_plate(i)->thumbnail_data;
            if (p->partplate_list.get_plate(i)->thumbnail_data.is_valid() &&  using_exported_file()) {
                //no need to generate thumbnail
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": non need to re-generate thumbnail for gcode/exported mode of plate %1%")%i;
            }
            else {
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": re-generate thumbnail for plate %1%") % i;
                const ThumbnailsParams thumbnail_params = { {}, false, true, true, true, i };
                p->generate_thumbnail(p->partplate_list.get_plate(i)->thumbnail_data, THUMBNAIL_SIZE_3MF.first, THUMBNAIL_SIZE_3MF.second,
                                    thumbnail_params, Camera::EType::Ortho);
            }
            thumbnails.push_back(thumbnail_data);

            ThumbnailData *no_light_thumbnail_data = &p->partplate_list.get_plate(i)->no_light_thumbnail_data;
            if (p->partplate_list.get_plate(i)->no_light_thumbnail_data.is_valid() && using_exported_file()) {
                // no need to generate thumbnail
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": non need to re-generate thumbnail for gcode/exported mode of plate %1%") % i;
            } else {
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": re-generate thumbnail for plate %1%") % i;
                const ThumbnailsParams thumbnail_params = {{}, false, true, true, true, i};
                p->generate_thumbnail(p->partplate_list.get_plate(i)->no_light_thumbnail_data, THUMBNAIL_SIZE_3MF.first, THUMBNAIL_SIZE_3MF.second,
                    thumbnail_params, Camera::EType::Ortho,false,false,true);
            }
            no_light_thumbnails.push_back(no_light_thumbnail_data);
            //ThumbnailData* calibration_data = &p->partplate_list.get_plate(i)->cali_thumbnail_data;
            //calibration_thumbnails.push_back(calibration_data);
            PlateBBoxData* plate_bbox_data = &p->partplate_list.get_plate(i)->cali_bboxes_data;
            plate_bboxes.push_back(plate_bbox_data);

            //generate top and picking thumbnails
            ThumbnailData* top_thumbnail = &p->partplate_list.get_plate(i)->top_thumbnail_data;
            if (top_thumbnail->is_valid() &&  using_exported_file()) {
                //no need to generate thumbnail
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": non need to re-generate top_thumbnail for gcode/exported mode of plate %1%")%i;
            }
            else {
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": re-generate top_thumbnail for plate %1%") % i;
                const ThumbnailsParams thumbnail_params = { {}, false, true, false, true, i };
                p->generate_thumbnail(p->partplate_list.get_plate(i)->top_thumbnail_data, THUMBNAIL_SIZE_3MF.first, THUMBNAIL_SIZE_3MF.second,
                                    thumbnail_params, Camera::EType::Ortho, true, false);
            }
            top_thumbnails.push_back(top_thumbnail);

            ThumbnailData* picking_thumbnail = &p->partplate_list.get_plate(i)->pick_thumbnail_data;
            if (picking_thumbnail->is_valid() &&  using_exported_file()) {
                //no need to generate thumbnail
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": non need to re-generate pick_thumbnail for gcode/exported mode of plate %1%")%i;
            }
            else {
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": re-generate pick_thumbnail for plate %1%") % i;
                const ThumbnailsParams thumbnail_params = { {}, false, true, false, true, i };
                p->generate_thumbnail(p->partplate_list.get_plate(i)->pick_thumbnail_data, THUMBNAIL_SIZE_3MF.first, THUMBNAIL_SIZE_3MF.second,
                                    thumbnail_params, Camera::EType::Ortho, true, true);
            }
            picking_thumbnails.push_back(picking_thumbnail);
        }

        if (p->partplate_list.get_curr_plate()->is_slice_result_valid()) {
            //BBS generate BBS calibration thumbnails
            int index = p->partplate_list.get_curr_plate_index();
            //ThumbnailData* calibration_data = calibration_thumbnails[index];
            //const ThumbnailsParams calibration_params = { {}, false, true, true, true, p->partplate_list.get_curr_plate_index() };
            //p->generate_calibration_thumbnail(*calibration_data, PartPlate::cali_thumbnail_width, PartPlate::cali_thumbnail_height, calibration_params);
            if (using_exported_file()) {
                //do nothing
            }
            else
                *plate_bboxes[index] = p->generate_first_layer_bbox();
        }
    }

    //BBS: add bbs 3mf logic
    PlateDataPtrs plate_data_list;
    p->partplate_list.store_to_3mf_structure(plate_data_list, (strategy & SaveStrategy::WithGcode || strategy & SaveStrategy::WithSliceInfo), export_plate_idx);

    // BBS: backup
    PresetBundle& preset_bundle = *app_preset_bundle();
    std::vector<Preset*> project_presets = preset_bundle.get_current_project_embedded_presets();

    StoreParams store_params;
    store_params.path  = path_u8.c_str();
    store_params.model = &p->model;
    store_params.plate_data_list = plate_data_list;
    store_params.export_plate_idx = export_plate_idx;
    store_params.project_presets = project_presets;
    store_params.config = export_config ? &cfg : nullptr;
    store_params.thumbnail_data = thumbnails;
    store_params.no_light_thumbnail_data  = no_light_thumbnails;
    store_params.top_thumbnail_data = top_thumbnails;
    store_params.pick_thumbnail_data = picking_thumbnails;
    store_params.calibration_thumbnail_data = calibration_thumbnails;
    store_params.proFn = proFn;
    store_params.id_bboxes = plate_bboxes;//BBS
    store_params.project = &p->project;
    store_params.strategy = strategy | SaveStrategy::Zip64;


    // get type and color for platedata
    auto* filament_color = dynamic_cast<const ConfigOptionStrings*>(cfg.option("filament_colour"));
    auto* nozzle_diameter_option = dynamic_cast<const ConfigOptionFloats*>(cfg.option("nozzle_diameter"));
    auto* filament_id_opt = dynamic_cast<const ConfigOptionStrings*>(cfg.option("filament_ids"));
    std::string nozzle_diameter_str;
    if (nozzle_diameter_option)
        nozzle_diameter_str = nozzle_diameter_option->serialize();

    std::string printer_model_id = preset_bundle.printers.get_edited_preset().get_printer_type(&preset_bundle);

    for (int i = 0; i < plate_data_list.size(); i++) {
        PlateData *plate_data = plate_data_list[i];
        plate_data->printer_model_id = printer_model_id;
        plate_data->nozzle_diameters = nozzle_diameter_str;
        for (auto it = plate_data->slice_filaments_info.begin(); it != plate_data->slice_filaments_info.end(); it++) {
            std::string display_filament_type;
            it->type  = cfg.get_filament_type(display_filament_type, it->id);
            it->filament_id = filament_id_opt ? filament_id_opt->get_at(it->id) : "";
            it->color = filament_color ? filament_color->get_at(it->id) : "#FFFFFF";
            // save filament info used in curr plate
            int index = p->partplate_list.get_curr_plate_index();
            if (store_params.id_bboxes.size() > index) {
                store_params.id_bboxes[index]->filament_ids.push_back(it->id);
                store_params.id_bboxes[index]->filament_colors.push_back(it->color);
            }
        }
    }

    // handle Design Info
    bool has_design_info = false;
    ModelDesignInfo designInfo;
    if (p->model.design_info != nullptr) {
        if (!p->model.design_info->Designer.empty()) {
            BOOST_LOG_TRIVIAL(trace) << "design_info, found designer = " << p->model.design_info->Designer;
            has_design_info = true;
        }
    }

    bool store_result = Slic3r::store_bbs_3mf(store_params);
    // reset designed info
    if (!has_design_info)
        p->model.design_info = nullptr;

    if (store_result) {
        if (!(store_params.strategy & SaveStrategy::Silence)) {
            // Success
            p->set_project_filename(path);
            BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << __LINE__ << " call set_project_filename: " << path;
        }
    }
    else {
        ret = -1;
    }

    if (project_presets.size() > 0)
    {
        for (unsigned int i = 0; i < project_presets.size(); i++)
        {
            delete project_presets[i];
        }
        project_presets.clear();
    }

    release_PlateData_list(plate_data_list);

    for (unsigned int i = 0; i < calibration_thumbnails.size(); i++)
    {
        //release the data here, as it will always be generated when export
        calibration_thumbnails[i]->reset();
    }
    for (unsigned int i = 0; i < no_light_thumbnails.size(); i++) {
        // release the data here, as it will always be generated when export
        no_light_thumbnails[i]->reset();
    }
    for (unsigned int i = 0; i < top_thumbnails.size(); i++)
    {
        //release the data here, as it will always be generated when export
        top_thumbnails[i]->reset();
    }
    top_thumbnails.clear();
    for (unsigned int i = 0; i < picking_thumbnails.size(); i++)
    {
        //release the data here, as it will always be generated when export
        picking_thumbnails[i]->reset();;
    }
    picking_thumbnails.clear();

    return ret;
}

void Plater::publish_project()
{
    return;
}


void Plater::reload_from_disk()
{
    p->reload_from_disk();
}

void Plater::replace_with_stl()
{
    p->replace_with_stl();
}

void Plater::reload_all_from_disk()
{
    p->reload_all_from_disk();
}

bool Plater::has_toolpaths_to_export() const
{
    return  p->preview->get_canvas3d()->has_toolpaths_to_export();
}

void Plater::export_toolpaths_to_obj() const
{
    if ((printer_technology() != ptFFF) || !is_preview_loaded())
        return;

    wxString path = p->get_export_file(FT_OBJ);
    if (path.empty())
        return;

    wxBusyCursor wait;
    p->preview->get_canvas3d()->export_toolpaths_to_obj(into_u8(path).c_str());
}

//BBS: add multiple plate reslice logic
void Plater::reslice()
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: enter, process_completed_with_error=%2%")%__LINE__ %p->process_completed_with_error;
    // There is "invalid data" button instead "slice now"
    if (p->process_completed_with_error == p->partplate_list.get_curr_plate_index())
    {
        BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": process_completed_with_error, return directly");
        reset_gcode_toolpaths();
        return;
    }

    // In case SLA gizmo is in editing mode, refuse to continue
    // and notify user that he should leave it first.
    if (p->m_gizmos->is_in_editing_mode(true))
        return;
    
    // Stop the running (and queued) UI jobs and only proceed if they actually
    // get stopped.
    unsigned timeout_ms = 10000;
    if (!stop_queue(this->get_ui_job_worker(), timeout_ms)) {
        BOOST_LOG_TRIVIAL(error) << "Could not stop UI job within "
                                 << timeout_ms << " milliseconds timeout!";
        return;
    }

    // Orca: regenerate CalibPressureAdvancePattern custom G-code to apply changes
    if (model().calib_pa_pattern) {
        _calib_pa_pattern_gen_gcode();
    }

    // When user manually triggers reslice, clear external gcode flag from current plate
    PartPlate* current_plate = p->partplate_list.get_curr_plate();
    if (current_plate && current_plate->has_external_gcode()) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ 
            << " - Clearing external gcode flag for plate " << current_plate->get_index()
            << " (user initiated reslice)";
        current_plate->clear_external_gcode();
    }

    //FIXME Don't reslice if export of G-code or sending to OctoPrint is running.
    // bitmask of UpdateBackgroundProcessReturnState
    unsigned int state = this->p->update_background_process(true);
    if (state & priv::UPDATE_BACKGROUND_PROCESS_REFRESH_SCENE)
        this->p->view3D->reload_scene(false);

    // Only restarts if the state is valid.
    //BBS: jusdge the result
    bool result = this->p->restart_background_process(state | priv::UPDATE_BACKGROUND_PROCESS_FORCE_RESTART);
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(", Line %1%: restart background,state=%2%, result=%3%")%__LINE__%state %result;
    if ((state & priv::UPDATE_BACKGROUND_PROCESS_INVALID) != 0)
    {
        //BBS: add logs
        BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": state %1% is UPDATE_BACKGROUND_PROCESS_INVALID, can not slice") % state;
        p->update_fff_scene_only_shells();
        return;
    }

    if ((!result) && p->m_slice_all && (p->m_cur_slice_plate < (p->partplate_list.get_plate_count() - 1)))
    {
        //slice next
        BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << boost::format(": in slicing all, current plate %1% already sliced, skip to next") % p->m_cur_slice_plate ;
        SlicingProcessCompletedEvent evt(EVT_PROCESS_COMPLETED, 0,
            SlicingProcessCompletedEvent::Finished, nullptr);
        // Post the "complete" callback message, so that it will slice the next plate soon
        wxQueueEvent(this, evt.Clone());
        p->m_is_slicing = true;
        if (p->m_cur_slice_plate == 0)
            reset_gcode_toolpaths();
        return;
    }

    if (result) {
        p->m_is_slicing = true;
    }

    bool clean_gcode_toolpaths = true;
    // BBS
    if (p->background_process.running())
    {
        //p->ready_to_slice = false;
        p->main_panel->update_slice_print_status(MainPanel::eEventSliceUpdate, false);
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": background process is running, m_is_slicing is true");
    }
    else if (!p->background_process.empty() && !p->background_process.idle()) {
        //p->show_action_buttons(true);
        //p->ready_to_slice = true;
        p->main_panel->update_slice_print_status(MainPanel::eEventSliceUpdate, true);
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": background process changes to not_idle, set ready_to_slice back to true");
    }
    else {
        //BBS: add reset logic for empty plate
        PartPlate * current_plate = p->background_process.get_current_plate();

        if (!current_plate->has_printable_instances()) {
            clean_gcode_toolpaths = true;
            current_plate->update_slice_result_valid_state(false);
        }
        else {
            clean_gcode_toolpaths = false;
            current_plate->update_slice_result_valid_state(true);
        }
        p->main_panel->update_slice_print_status(MainPanel::eEventSliceUpdate, false);
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": background process in idle state, use previous result, clean_gcode_toolpaths=%1%")%clean_gcode_toolpaths;
    }

    if (clean_gcode_toolpaths)
        reset_gcode_toolpaths();

    p->preview->reload_print(!clean_gcode_toolpaths);

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": finished, started slicing for plate %1%") % p->partplate_list.get_curr_plate_index();
}

//BBS: add project slicing related logic
int Plater::start_next_slice()
{
    unsigned int state = this->p->update_background_process(true);
    if (state & priv::UPDATE_BACKGROUND_PROCESS_REFRESH_SCENE)
        this->p->view3D->reload_scene(false);

    if (!p->partplate_list.get_curr_plate()->can_slice()) {
        p->process_completed_with_error = p->partplate_list.get_curr_plate_index();
        return -1;
    }

    // Only restarts if the state is valid.
    bool result = this->p->restart_background_process(state | priv::UPDATE_BACKGROUND_PROCESS_FORCE_RESTART);
    if (!result)
    {
        //slice next
        SlicingProcessCompletedEvent evt(EVT_PROCESS_COMPLETED, 0,
                SlicingProcessCompletedEvent::Finished, nullptr);
        // Post the "complete" callback message, so that it will slice the next plate soon
        wxQueueEvent(this, evt.Clone());
    }
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": restart_background_process returns %1%")%result;

    return 0;
}

void Plater::print_job_finished(wxCommandEvent &evt)
{
}

void Plater::send_job_finished(wxCommandEvent& evt)
{
}

void Plater::publish_job_finished(wxCommandEvent &evt)
{
}

// Called when the Eject button is pressed.
void Plater::eject_drive()
{
	wxBusyCursor wait;
	AppAdapter::gui_app()->removable_drive_manager()->eject_drive();
}

void Plater::take_snapshot(const std::string &snapshot_name) { p->take_snapshot(snapshot_name); }
//void Plater::take_snapshot(const wxString &snapshot_name) { p->take_snapshot(snapshot_name); }
void Plater::take_snapshot(const std::string &snapshot_name, UndoRedo::SnapshotType snapshot_type) { p->take_snapshot(snapshot_name, snapshot_type); }
//void Plater::take_snapshot(const wxString &snapshot_name, UndoRedo::SnapshotType snapshot_type) { p->take_snapshot(snapshot_name, snapshot_type); }
void Plater::suppress_snapshots() { p->suppress_snapshots(); }
void Plater::allow_snapshots() { p->allow_snapshots(); }
// BBS: single snapshot
void Plater::single_snapshots_enter(SingleSnapshot *single)
{
    p->single_snapshots_enter(single);
}
void Plater::single_snapshots_leave(SingleSnapshot *single)
{
    p->single_snapshots_leave(single);
}
void Plater::undo() { p->undo(); }
void Plater::redo() { p->redo(); }
void Plater::undo_to(int selection)
{
    if (selection == 0) {
        p->undo();
        return;
    }

    const int idx = p->get_active_snapshot_index() - selection - 1;
    p->undo_redo_to(p->undo_redo_stack().snapshots()[idx].timestamp);
}
void Plater::redo_to(int selection)
{
    if (selection == 0) {
        p->redo();
        return;
    }

    const int idx = p->get_active_snapshot_index() + selection + 1;
    p->undo_redo_to(p->undo_redo_stack().snapshots()[idx].timestamp);
}
bool Plater::undo_redo_string_getter(const bool is_undo, int idx, const char** out_text)
{
    const std::vector<UndoRedo::Snapshot>& ss_stack = p->undo_redo_stack().snapshots();
    const int idx_in_ss_stack = p->get_active_snapshot_index() + (is_undo ? -(++idx) : idx);

    if (0 < idx_in_ss_stack && (size_t)idx_in_ss_stack < ss_stack.size() - 1) {
        *out_text = ss_stack[idx_in_ss_stack].name.c_str();
        return true;
    }

    return false;
}

void Plater::undo_redo_topmost_string_getter(const bool is_undo, std::string& out_text)
{
    const std::vector<UndoRedo::Snapshot>& ss_stack = p->undo_redo_stack().snapshots();
    const int idx_in_ss_stack = p->get_active_snapshot_index() + (is_undo ? -1 : 0);

    if (0 < idx_in_ss_stack && (size_t)idx_in_ss_stack < ss_stack.size() - 1) {
        out_text = ss_stack[idx_in_ss_stack].name;
        return;
    }

    out_text = "";
}

bool Plater::search_string_getter(int idx, const char** label, const char** tooltip)
{
    const Search::OptionsSearcher& search_list = p->sidebar->get_searcher();

    if (0 <= idx && (size_t)idx < search_list.size()) {
        search_list[idx].get_marked_label_and_tooltip(label, tooltip);
        return true;
    }

    return false;
}

// BBS.
void Plater::on_filaments_change(size_t num_filaments)
{
    // only update elements in plater
    update_filament_colors_in_full_config();
    sidebar().on_filaments_change(num_filaments);
    sidebar().obj_list()->update_objects_list_filament_column(num_filaments);

    Slic3r::GUI::PartPlateList &plate_list = get_partplate_list();
    for (int i = 0; i < plate_list.get_plate_count(); ++i) {
        PartPlate* part_plate = plate_list.get_plate(i);
        part_plate->update_first_layer_print_sequence(num_filaments);
    }

    for (ModelObject* mo : AppAdapter::gui_app()->model().objects) {
        for (ModelVolume* mv : mo->volumes) {
            mv->update_extruder_count(num_filaments);
        }
    }
}

void Plater::on_bed_type_change(BedType bed_type)
{
    sidebar().on_bed_type_change(bed_type);
}

bool Plater::update_filament_colors_in_full_config()
{
    DynamicPrintConfig& project_config = app_preset_bundle()->project_config;
    ConfigOptionStrings* color_opt = project_config.option<ConfigOptionStrings>("filament_colour");

    p->config->option<ConfigOptionStrings>("filament_colour")->values = color_opt->values;
    return true;
}

void Plater::config_change_notification(const DynamicPrintConfig &config, const std::string& key)
{
    GLCanvas3D* view3d_canvas = get_view3D_canvas3D();
    if (key == std::string("print_sequence")) {
        auto seq_print = config.option<ConfigOptionEnum<PrintSequence>>("print_sequence");
        if (seq_print && view3d_canvas && view3d_canvas->is_initialized() && view3d_canvas->is_rendering_enabled()) {
            NotificationManager* notify_manager = get_notification_manager();
            if (seq_print->value == PrintSequence::ByObject) {
                std::string info_text = _u8L("Print By Object: \nSuggest to use auto-arrange to avoid collisions when printing.");
                notify_manager->bbl_show_seqprintinfo_notification(info_text);
            }
            else
                notify_manager->bbl_close_seqprintinfo_notification();
        }
    }
    // notification for more options
}

void Plater::on_config_change(const DynamicPrintConfig &config)
{
    bool update_scheduled = false;
    bool bed_shape_changed = false;
    //bool print_sequence_changed = false;
    t_config_option_keys diff_keys = p->config->diff(config);
    for (auto opt_key : diff_keys) {
        if (opt_key == "filament_colour") {
            update_scheduled = true; // update should be scheduled (for update 3DScene) #2738

            if (update_filament_colors_in_full_config()) {
                p->sidebar->obj_list()->update_filament_colors();
                p->sidebar->update_dynamic_filament_list();
                continue;
            }
        }
        if (opt_key == "material_colour") {
            update_scheduled = true; // update should be scheduled (for update 3DScene)
        }

        p->config->set_key_value(opt_key, config.option(opt_key)->clone());
        if (opt_key == "printer_technology") {
            this->set_printer_technology(config.opt_enum<PrinterTechnology>(opt_key));
            // print technology is changed, so we should to update a search list
            p->sidebar->update_searcher();
            p->reset_gcode_toolpaths();
            p->view3D->get_canvas3d()->reset_sequential_print_clearance();
            //BBS: invalid all the slice results
            p->partplate_list.invalid_all_slice_result();
        }
        //BBS: add bed_exclude_area
        else if (opt_key == "printable_area" || opt_key == "bed_exclude_area"
            || opt_key == "bed_custom_texture" || opt_key == "bed_custom_model"
            || opt_key == "extruder_clearance_height_to_lid"
            || opt_key == "extruder_clearance_height_to_rod") {
            bed_shape_changed = true;
            update_scheduled = true;
        }
        else if (opt_key == "bed_shape" || opt_key == "bed_custom_texture" || opt_key == "bed_custom_model") {
            bed_shape_changed = true;
            update_scheduled = true;
        }
        else if (boost::starts_with(opt_key, "enable_prime_tower") ||
            boost::starts_with(opt_key, "prime_tower") ||
            boost::starts_with(opt_key, "wipe_tower") ||
            opt_key == "filament_minimal_purge_on_wipe_tower" ||
            opt_key == "single_extruder_multi_material" ||
            // BBS
            opt_key == "prime_volume") {
            update_scheduled = true;
        }
        else if(opt_key == "extruder_colour") {
            update_scheduled = true;
            //p->sidebar->obj_list()->update_extruder_colors();
        }
        else if (opt_key == "printable_height") {
            bed_shape_changed = true;
            update_scheduled = true;
        }
        else if (opt_key == "print_sequence") {
            update_scheduled = true;
            //print_sequence_changed = true;
        }
        else if (opt_key == "printer_model" || opt_key == "hot_bed_divide") {
            p->reset_gcode_toolpaths();
            // update to force bed selection(for texturing)
            bed_shape_changed = true;
            update_scheduled = true;
        }
        // Orca: update when *_filament changed
        else if (opt_key == "support_interface_filament" || opt_key == "support_filament" || opt_key == "wall_filament" ||
                 opt_key == "sparse_infill_filament" || opt_key == "solid_infill_filament") {
            update_scheduled = true;
        }
    }

    if (bed_shape_changed)
    {
        set_bed_shape();
        on_bed_updated();
    }

    config_change_notification(config, std::string("print_sequence"));

    if (update_scheduled)
        update();

    if (p->main_panel->is_loaded()) {
        this->p->schedule_background_process();
        update_title_dirty_status();
    }
}

void Plater::set_bed_shape() const
{
    p->update_bed_shape();
}

void Plater::on_bed_updated()
{
    p->on_bed_updated();
}

void Plater::force_print_bed_update()
{
    p->config->opt_string("printer_model", true) = "bbl_empty";
}

// Get vector of extruder colors considering filament color, if extruder color is undefined.
std::vector<std::string> Plater::get_extruder_colors_from_plater_config() const
{
    const Slic3r::DynamicPrintConfig* config = &app_preset_bundle()->project_config;
    std::vector<std::string> filament_colors;
    if (!config->has("filament_colour")) // in case of a SLA print
        return filament_colors;

    filament_colors = (config->option<ConfigOptionStrings>("filament_colour"))->values;
    return filament_colors;
}

/* Get vector of colors used for rendering of a Preview scene in "Color print" mode
 * It consists of extruder colors and colors, saved in model.custom_gcode_per_print_z
 */
std::vector<std::string> Plater::get_colors_for_color_print() const
{
    std::vector<std::string> colors = get_extruder_colors_from_plater_config();

    //BBS
    colors.reserve(colors.size() + p->model.get_curr_plate_custom_gcodes().gcodes.size());
    for (const CustomGCode::Item& code : p->model.get_curr_plate_custom_gcodes().gcodes) {
        if (code.type == CustomGCode::ColorChange)
            colors.emplace_back(code.color);
    }

    return colors;
}

wxWindow* Plater::get_select_machine_dialog()
{
    return nullptr;
}

void Plater::update_print_error_info(int code, std::string msg, std::string extra)
{
}

wxString Plater::get_project_filename(const wxString& extension) const
{
    return p->get_project_filename(extension);
}

wxString Plater::get_export_gcode_filename(const wxString & extension, bool only_filename, bool export_all) const
{
    return p->get_export_gcode_filename(extension, only_filename, export_all);
}

void Plater::set_project_filename(const wxString& filename)
{
    p->set_project_filename(filename);
}

const Selection &Plater::get_selection() const
{
    return p->get_selection();
}

Selection& Plater::get_selection() 
{
    return p->get_selection();
}

Selection* Plater::get_selection_ptr()
{
    return p->get_selection_ptr();
}

GLGizmosManager* Plater::get_gizmos_manager()
{
    return p->m_gizmos;
}
    
SceneRaycaster* Plater::get_scene_raycaster()
{
    return p->m_scene_raycaster;
}

int Plater::get_selected_object_idx()
{
    return p->get_selected_object_idx();
}

bool Plater::is_single_full_object_selection() const
{
    return p->get_selection().is_single_full_object();
}

GLCanvas3D* Plater::canvas3D()
{
    // BBS modify view3D->get_canvas3d() to current canvas
    return p->get_current_canvas3D();
}

const GLCanvas3D* Plater::canvas3D() const
{
    // BBS modify view3D->get_canvas3d() to current canvas
    return p->get_current_canvas3D();
}

GLCanvas3D* Plater::get_view3D_canvas3D()
{
    return p->view3D->get_canvas3d();
}

GCodePreviewCanvas* Plater::get_preview_canvas3D()
{
    return p->preview->get_canvas3d();
}

GLCanvas3DFacade* Plater::canvas_facade()
{
    return p->m_canvas;
}

GLCanvas3D* Plater::get_current_canvas3D(bool exclude_preview)
{
    return p->get_current_canvas3D(exclude_preview);
}

void Plater::arrange()
{
    auto &w = get_ui_job_worker();
    if (w.is_idle()) {
        p->take_snapshot(_u8L("Arrange"));
        replace_job(w, std::make_unique<ArrangeJob>());
    }
}

void Plater::set_current_canvas_as_dirty()
{
    p->set_current_canvas_as_dirty();
}

void Plater::unbind_canvas_event_handlers()
{
    p->unbind_canvas_event_handlers();
}

void Plater::reset_canvas_volumes()
{
    p->reset_canvas_volumes();
}

PrinterTechnology Plater::printer_technology() const
{
    return ptFFF;
}

const DynamicPrintConfig * Plater::config() const { return p->config; }

bool Plater::set_printer_technology(PrinterTechnology printer_technology)
{
    p->label_btn_export = L("Export G-code");
    p->label_btn_send   = L("Send G-code");

    p->sidebar->get_searcher().set_printer_technology(ptFFF);

    p->notification_manager->set_fff(true);
    p->notification_manager->set_slicing_progress_hidden();

    return true;
}

void Plater::clear_before_change_mesh(int obj_idx)
{
    ModelObject* mo = model().objects[obj_idx];

    // If there are custom supports/seams/mmu segmentation, remove them. Fixed mesh
    // may be different and they would make no sense.
    bool paint_removed = false;
    for (ModelVolume* mv : mo->volumes) {
        paint_removed |= ! mv->supported_facets.empty() || ! mv->seam_facets.empty() || ! mv->mmu_segmentation_facets.empty();
        mv->supported_facets.reset();
        mv->seam_facets.reset();
        mv->mmu_segmentation_facets.reset();
    }
    if (paint_removed) {
        // snapshot_time is captured by copy so the lambda knows where to undo/redo to.
        get_notification_manager()->push_notification(
                    NotificationType::CustomSupportsAndSeamRemovedAfterRepair,
                    NotificationManager::NotificationLevel::PrintInfoNotificationLevel,
                    _u8L("Custom supports and color painting were removed before repairing."));
    }
}

void Plater::changed_mesh(int obj_idx)
{
    update();
    p->object_list_changed();
    p->schedule_background_process();
}

void Plater::changed_object(ModelObject &object){
    assert(object.get_model() == &p->model); // is object from same model?
    object.invalidate_bounding_box();

    // recenter and re - align to Z = 0
    object.ensure_on_bed(true);

    p->view3D->reload_scene(false);
    // update print
    p->schedule_background_process();

}

void Plater::changed_object(int obj_idx)
{
    if (obj_idx < 0)
        return;
    ModelObject *object = p->model.objects[obj_idx];
    if (object == nullptr)
        return;
    changed_object(*object);
}

void Plater::changed_objects(const std::vector<size_t>& object_idxs)
{
    if (object_idxs.empty())
        return;

    for (size_t obj_idx : object_idxs) {
        if (obj_idx < p->model.objects.size()) {
            if (p->model.objects[obj_idx]->min_z() >= SINKING_Z_THRESHOLD)
                // re - align to Z = 0
                p->model.objects[obj_idx]->ensure_on_bed();
        }
    }

    p->view3D->reload_scene(false);
    p->view3D->get_canvas3d()->update_instance_printable_state_for_objects(object_idxs);

    // update print
    this->p->schedule_background_process();
}

void Plater::schedule_background_process(bool schedule/* = true*/)
{
    if (schedule)
        this->p->schedule_background_process();

    this->p->suppressed_backround_processing_update = false;
}

bool Plater::is_background_process_update_scheduled() const
{
    return this->p->background_process_timer.IsRunning();
}

void Plater::suppress_background_process(const bool stop_background_process)
{
    if (stop_background_process)
        this->p->background_process_timer.Stop();

    this->p->suppressed_backround_processing_update = true;
}

void Plater::center_selection()     { p->center_selection(); }
void Plater::drop_selection()       { p->drop_selection(); }
void Plater::mirror(Axis axis)      { p->mirror(axis); }
void Plater::split_object()         { p->split_object(); }
void Plater::split_volume()         { p->split_volume(); }
void Plater::optimize_rotation()
{
    auto &w = get_ui_job_worker();
    if (w.is_idle()) {
        p->take_snapshot(_u8L("Optimize Rotation"));
        replace_job(w, std::make_unique<OrientJob>());
    }
}
void Plater::update_menus()         { p->menus.update(); }
// BBS
//void Plater::show_action_buttons(const bool ready_to_slice) const   { p->show_action_buttons(ready_to_slice); }

void Plater::fill_color(int extruder_id)
{
    if (can_fillcolor()) {
        p->get_selection().fill_color(extruder_id);
    }
}

//BBS
void Plater::cut_selection_to_clipboard()
{
    Plater::TakeSnapshot snapshot(this, "Cut Selected Objects");
    if (can_cut_to_clipboard() && !p->sidebar->obj_list()->cut_to_clipboard()) {
        p->get_selection().cut_to_clipboard();
    }
}

void Plater::copy_selection_to_clipboard()
{
    // At first try to copy selected values to the ObjectList's clipboard
    // to check if Settings or Layers are selected in the list
    // and then copy to 3DCanvas's clipboard if not
    if (can_copy_to_clipboard() && !p->sidebar->obj_list()->copy_to_clipboard())
        p->get_selection().copy_to_clipboard();
}

void Plater::paste_from_clipboard()
{
    if (!can_paste_from_clipboard())
        return;

    Plater::TakeSnapshot snapshot(this, "Paste From Clipboard");

    // At first try to paste values from the ObjectList's clipboard
    // to check if Settings or Layers were copied
    // and then paste from the 3DCanvas's clipboard if not
    if (!p->sidebar->obj_list()->paste_from_clipboard())
        p->get_selection().paste_from_clipboard();
}

//BBS: add clone
void Plater::clone_selection()
{
    if (is_selection_empty())
        return;
    long res = wxGetNumberFromUser("",
        _L("Clone"),
        _L("Number of copies:"),
        1, 0, 1000, this);
    wxString msg;
    if (res == -1) {
        msg = _L("Invalid number");
        return;
    }
    Selection& selection = p->get_selection();
    selection.clone(res);
    if (AppAdapter::app_config()->get("auto_arrange") == "true") {
        this->set_prepare_state(Job::PREPARE_STATE_MENU);
        this->arrange();
    }
}

std::vector<Vec2f> Plater::get_empty_cells(const Vec2f step)
{
    PartPlate* plate = AppAdapter::plater()->get_partplate_list().get_curr_plate();
    BoundingBoxf3 build_volume = plate->get_build_volume();
    Vec2d vmin(build_volume.min.x(), build_volume.min.y()), vmax(build_volume.max.x(), build_volume.max.y());
    BoundingBoxf bbox(vmin, vmax);
    std::vector<Vec2f> cells;
    auto min_x = step(0)/2;// start_point.x() - step(0) * int((start_point.x() - bbox.min.x()) / step(0));
    auto min_y = step(1)/2;// start_point.y() - step(1) * int((start_point.y() - bbox.min.y()) / step(1));
    auto& exclude_box3s = plate->get_exclude_areas();
    std::vector<BoundingBoxf> exclude_boxs;
    for (auto& box : exclude_box3s) {
        Vec2d vmin(box.min.x(), box.min.y()), vmax(box.max.x(), box.max.y());
        exclude_boxs.emplace_back(vmin, vmax);
    }
    for (float x = min_x + bbox.min.x(); x < bbox.max.x() - step(0) / 2; x += step(0))
        for (float y = min_y + bbox.min.y(); y < bbox.max.y() - step(1) / 2; y += step(1)) {
            bool in_exclude = false;
            BoundingBoxf cell(Vec2d(x - step(0) / 2, y - step(1) / 2), Vec2d(x + step(0) / 2, y + step(1) / 2));
            for (auto& box : exclude_boxs) {
                if (box.overlap(cell)) {
                    in_exclude = true;
                    break;
                }
            }
            if(in_exclude)
                continue;
            cells.emplace_back(x, y);
        }
    return cells;
}

void Plater::search(bool plater_is_active, Preset::Type type, wxWindow *tag, TextInput *etag, wxWindow *stag)
{
    if (plater_is_active) {
        if (is_preview_shown())
            return;
        // plater should be focused for correct navigation inside search window
        this->SetFocus();

        wxKeyEvent evt;
#ifdef __APPLE__
        evt.m_keyCode = 'f';
#else /* __APPLE__ */
        evt.m_keyCode = WXK_CONTROL_F;
#endif /* __APPLE__ */
        evt.SetControlDown(true);
        canvas3D()->on_char(evt);
    }
    else
        p->sidebar->get_searcher().show_dialog(type, tag, etag, stag);
}

void Plater::msw_rescale()
{
    p->preview->msw_rescale();

    p->view3D->get_canvas3d()->msw_rescale();

    p->sidebar->msw_rescale();

    p->menus.msw_rescale();

    Layout();
    GetParent()->Layout();
}

void Plater::sys_color_changed()
{
    p->preview->sys_color_changed();
    p->sidebar->sys_color_changed();
    p->menus.sys_color_changed();

    Layout();
    GetParent()->Layout();
}

bool Plater::init_collapse_toolbar()
{
    return p->init_collapse_toolbar();
}

const Camera& Plater::get_camera() const
{
    return p->camera;
}

Camera& Plater::get_camera()
{
    return p->camera;
}

Camera* Plater::get_camera_ptr()
{
    return &p->camera;
}

//BBS: partplate list related functions
PartPlateList& Plater::get_partplate_list()
{
    return p->partplate_list;
}

void Plater::apply_background_progress()
{
    PartPlate* part_plate = p->partplate_list.get_curr_plate();

    //always apply the current plate's print
    Print::ApplyStatus invalidated = p->background_process.apply();

    if (invalidated & PrintBase::APPLY_STATUS_INVALIDATED)
    {
        part_plate->update_slice_result_valid_state(false);
        //p->ready_to_slice = true;
        p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, true);
    }
}

//BBS: select Plate
int Plater::select_plate(int plate_index, bool need_slice)
{
    int ret;
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: plate %2%, need_slice %3% ")%__LINE__ %plate_index  %need_slice;
    take_snapshot("select partplate!");
    ret = p->partplate_list.select_plate(plate_index);
    if (!ret) {
        if (is_view3D_shown())
            AppAdapter::plater()->canvas3D()->render();
    }

    if ((!ret) && (p->background_process.can_switch_print()))
    {
        //select successfully
        p->partplate_list.update_slice_context_to_current_plate(p->background_process);
        p->preview->update_gcode_result(p->partplate_list.get_current_slice_result_wrapper());

        PartPlate* part_plate = p->partplate_list.get_curr_plate();
        bool result_valid = part_plate->is_slice_result_valid();

        GCodeResultWrapper* gcode_result = nullptr;
        part_plate->get_print(&gcode_result, NULL);

        Print::ApplyStatus invalidated = p->background_process.apply();
        bool model_fits, validate_err;

        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: plate %2%, after apply, invalidated= %3%, previous result_valid %4% ")%__LINE__ %plate_index  %invalidated %result_valid;
        if (result_valid)
        {
            if (is_preview_shown())
            {
                if (need_slice) { //from preview's thumbnail
                    if ((invalidated & PrintBase::APPLY_STATUS_INVALIDATED) || (gcode_result->moves().empty())) {
                        if (invalidated & PrintBase::APPLY_STATUS_INVALIDATED)
                            part_plate->update_slice_result_valid_state(false);
                        p->process_completed_with_error = -1;
                        p->m_slice_all = false;
                        reset_gcode_toolpaths();
                        reslice();
                    }
                    else {
                        validate_current_plate(model_fits, validate_err);
                        //just refresh_print
                        refresh_print();
                        p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, false, true);
                    }
                }
                else {// from multiple slice's next
                    //do nothing
                }
            }
            else
            {
                validate_current_plate(model_fits, validate_err);
                if (invalidated & PrintBase::APPLY_STATUS_INVALIDATED)
                {
                    part_plate->update_slice_result_valid_state(false);
                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, true);
                }
                else
                {
                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, false);

                    refresh_print();
                }
            }
        }
        else
        {
            validate_current_plate(model_fits, validate_err);
            if (model_fits && !validate_err) {
                p->process_completed_with_error = -1;
            }
            else {
                p->process_completed_with_error = p->partplate_list.get_curr_plate_index();
            }
            if (is_preview_shown())
            {
                if (need_slice)
                {
                    p->m_slice_all = false;
                    reset_gcode_toolpaths();
                    if (model_fits && !validate_err)
                        reslice();
                    else {
                        p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, false);
                        p->update_fff_scene_only_shells();
                    }
                }
                else {
                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, false);
                    refresh_print();
                }
            }
            else
            {
                if (model_fits && part_plate->has_printable_instances())
                {
                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, true);
                }
                else
                {
                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, false);
                }
            }
        }
    }

    SimpleEvent event(EVT_GLCANVAS_PLATE_SELECT);
    p->on_plate_selected(event);

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: plate %2%, return %3%")%__LINE__ %plate_index %ret;
    return ret;
}

int Plater::select_sliced_plate(int plate_index)
{
    int ret = 0;
    BOOST_LOG_TRIVIAL(info) << "select_sliced_plate plate_idx=" << plate_index;

    Freeze();
    ret = select_plate(plate_index, true);
    if (ret)
    {
        BOOST_LOG_TRIVIAL(error) << "select_plate error for plate_idx=" << plate_index;
        Thaw();
        return -1;
    }
    p->partplate_list.select_plate_view();
    Thaw();

    return ret;
}

void Plater::validate_current_plate(bool& model_fits, bool& validate_error)
{
    model_fits = p->view3D->get_canvas3d()->check_volumes_outside_state() != ModelInstancePVS_Partly_Outside;
    validate_error = false;
    if (true) {
        std::string plater_text = _u8L("An object is laid over the boundary of plate or exceeds the height limit.\n"
                    "Please solve the problem by moving it totally on or off the plate, and confirming that the height is within the build volume.");;
        StringObjectException warning;
        Polygons polygons;
        std::vector<std::pair<Polygon, float>> height_polygons;
        StringObjectException err = p->background_process.validate(&warning, &polygons, &height_polygons);
        // update string by type
        post_process_string_object_exception(err);
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": validate err=%1%, warning=%2%, model_fits %3%")%err.string%warning.string %model_fits;

        if (err.string.empty()) {
            p->partplate_list.get_curr_plate()->update_apply_result_invalid(false);
            p->notification_manager->set_all_slicing_errors_gray(true);
            p->notification_manager->close_notification_of_type(NotificationType::ValidateError);

            // Pass a warning from validation and either show a notification,
            // or hide the old one.
            p->process_validation_warning(warning);
            p->view3D->get_canvas3d()->reset_sequential_print_clearance();
            p->view3D->get_canvas3d()->set_as_dirty();
            p->view3D->get_canvas3d()->request_extra_frame();
        }
        else {
            // The print is not valid.
            p->partplate_list.get_curr_plate()->update_apply_result_invalid(true);
            // Show error as notification.
            p->notification_manager->push_validate_error_notification(err);
            p->process_validation_warning(warning);
            //model_fits = false;
            validate_error = true;
            p->view3D->get_canvas3d()->set_sequential_print_clearance_visible(true);
            p->view3D->get_canvas3d()->set_sequential_print_clearance_render_fill(true);
            p->view3D->get_canvas3d()->set_sequential_print_clearance_polygons(polygons, height_polygons);
        }

        if (!model_fits) {
            p->notification_manager->push_plater_error_notification(plater_text);
        }
        else {
            p->notification_manager->close_plater_error_notification(plater_text);
        }
    }

    PartPlate* part_plate = p->partplate_list.get_curr_plate();
    part_plate->update_slice_ready_status(model_fits);

    return;
}

void Plater::open_platesettings_dialog(wxCommandEvent& evt) {
    int plate_index = evt.GetInt();
    PlateSettingsDialog dlg(this, _L("Plate Settings"), evt.GetString() == "only_layer_sequence");
    PartPlate* curr_plate = p->partplate_list.get_curr_plate();
    dlg.sync_bed_type(curr_plate->get_bed_type());

    auto curr_print_seq = curr_plate->get_print_seq();
    if (curr_print_seq != PrintSequence::ByDefault) {
        dlg.sync_print_seq(int(curr_print_seq) + 1);
    }
    else
        dlg.sync_print_seq(0);

    auto first_layer_print_seq = curr_plate->get_first_layer_print_sequence();
    if (first_layer_print_seq.empty())
        dlg.sync_first_layer_print_seq(0);
    else
        dlg.sync_first_layer_print_seq(1, curr_plate->get_first_layer_print_sequence());

    auto other_layers_print_seq = curr_plate->get_other_layers_print_sequence();
    if (other_layers_print_seq.empty())
        dlg.sync_other_layers_print_seq(0, {});
    else {
        dlg.sync_other_layers_print_seq(1, curr_plate->get_other_layers_print_sequence());
    }

    dlg.sync_spiral_mode(curr_plate->get_spiral_vase_mode(), !curr_plate->has_spiral_mode_config());

    dlg.Bind(EVT_SET_BED_TYPE_CONFIRM, [this, plate_index, &dlg](wxCommandEvent& e) {
        PartPlate* curr_plate = p->partplate_list.get_curr_plate();
        BedType old_bed_type = curr_plate->get_bed_type();
        auto bt_sel = BedType(dlg.get_bed_type_choice());
        if (old_bed_type != bt_sel) {
            curr_plate->set_bed_type(bt_sel);
            update_project_dirty_from_presets();
            set_plater_dirty(true);
        }
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format("select bed type %1% for plate %2% at plate side") % bt_sel % plate_index;

        if (dlg.get_first_layer_print_seq_choice() != 0)
            curr_plate->set_first_layer_print_sequence(dlg.get_first_layer_print_seq());
        else
            curr_plate->set_first_layer_print_sequence({});

        if (dlg.get_other_layers_print_seq_choice() != 0)
            curr_plate->set_other_layers_print_sequence(dlg.get_other_layers_print_seq_infos());
        else
            curr_plate->set_other_layers_print_sequence({});

        int ps_sel = dlg.get_print_seq_choice();
        if (ps_sel != 0)
            curr_plate->set_print_seq(PrintSequence(ps_sel - 1));
        else
            curr_plate->set_print_seq(PrintSequence::ByDefault);

        int spiral_sel = dlg.get_spiral_mode_choice();
        if (spiral_sel == 1) {
            curr_plate->set_spiral_vase_mode(true, false);
        }
        else if (spiral_sel == 2) {
            curr_plate->set_spiral_vase_mode(false, false);
        }
        else {
            curr_plate->set_spiral_vase_mode(false, true);
        }

        update_project_dirty_from_presets();
        set_plater_dirty(true);
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format("select print sequence %1% for plate %2% at plate side") % ps_sel % plate_index;
        auto plate_config = *(curr_plate->config());
        AppAdapter::plater()->config_change_notification(plate_config, std::string("print_sequence"));
        update();
        AppAdapter::obj_list()->update_selections();
        });
    dlg.set_plate_name(from_u8(curr_plate->get_plate_name()));

    dlg.ShowModal();
    curr_plate->set_plate_name(dlg.get_plate_name().ToUTF8().data());
}

//BBS: select Plate by hover_id
int Plater::select_plate_by_hover_id(int hover_id, bool right_click, bool isModidyPlateName)
{
    int ret;
    int action, plate_index;

    plate_index = hover_id / PartPlate::GRABBER_COUNT;
    action      = isModidyPlateName ? PartPlate::PLATE_NAME_HOVER_ID : hover_id % PartPlate::GRABBER_COUNT;

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": enter, hover_id %1%, plate_index %2%, action %3%")%hover_id % plate_index %action;
    if (action == 0)
    {
        //select plate
        ret = p->partplate_list.select_plate(plate_index);
        if (!ret) {
            SimpleEvent event(EVT_GLCANVAS_PLATE_SELECT);
            p->on_plate_selected(event);
        }
        if ((!ret)&&(p->background_process.can_switch_print()))
        {
            //select successfully
            p->partplate_list.update_slice_context_to_current_plate(p->background_process);
            p->preview->update_gcode_result(p->partplate_list.get_current_slice_result_wrapper());

            PartPlate* part_plate = p->partplate_list.get_curr_plate();
            bool result_valid = part_plate->is_slice_result_valid();

            Print::ApplyStatus invalidated = p->background_process.apply();
            bool model_fits, validate_err;
            validate_current_plate(model_fits, validate_err);

            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: after apply, invalidated= %2%, previous result_valid %3% ")%__LINE__ % invalidated %result_valid;
            if (result_valid)
            {
                if (invalidated & PrintBase::APPLY_STATUS_INVALIDATED)
                {
                    part_plate->update_slice_result_valid_state(false);

                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, true);
                }
                else
                {
                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, false);

                    refresh_print();
                }
            }
            else
            {
                //check inside status
                if (model_fits && !validate_err){
                    p->process_completed_with_error = -1;
                }
                else {
                    p->process_completed_with_error = p->partplate_list.get_curr_plate_index();
                }

                if (model_fits && part_plate->has_printable_instances())
                {
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": will set can_slice to true");
                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, true);
                }
                else
                {
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": will set can_slice to false, has_printable_instances %1%")%part_plate->has_printable_instances();
                    p->main_panel->update_slice_print_status(MainPanel::eEventPlateUpdate, false);
                }
            }
        }
    }
    else if ((action == 1)&&(!right_click))
    {
        ret = delete_plate(plate_index);
    }
    else if ((action == 2)&&(!right_click))
    {
        ret = select_plate(plate_index);
        if (!ret)
        {
            set_prepare_state(Job::PREPARE_STATE_MENU);
            orient();
        }
        else
        {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "can not select plate %1%" << plate_index;
            ret = -1;
        }
    }
    else if ((action == 3)&&(!right_click))
    {
        ret = select_plate(plate_index);
        if (!ret)
        {
            if (last_arrange_job_is_finished()) {
                set_prepare_state(Job::PREPARE_STATE_MENU);
                arrange();
            }
        }
        else
        {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "can not select plate %1%" << plate_index;
            ret = -1;
        }
    }
    else if ((action == 4)&&(!right_click))
    {
        //lock the plate
        take_snapshot("lock partplate");
        ret = p->partplate_list.lock_plate(plate_index, !p->partplate_list.is_locked(plate_index));
    }
    else if ((action == 5)&&(!right_click))
    {
        // set the plate type
        ret = select_plate(plate_index);
        if (!ret) {
            wxCommandEvent evt(EVT_OPEN_PLATESETTINGSDIALOG);
            evt.SetInt(plate_index);
            evt.SetEventObject(this);
            wxPostEvent(this, evt);

            this->schedule_background_process();
        } else {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "can not select plate %1%" << plate_index;
            ret = -1;
        }
    }
    else if ((action == 6) && (!right_click)) {
        // set the plate type
        ret = select_plate(plate_index);
        if (!ret) {
            PlateNameEditDialog dlg(this, wxID_ANY, _L("Edit Plate Name"));
            PartPlate *         curr_plate = p->partplate_list.get_curr_plate();

            wxString curr_plate_name = from_u8(curr_plate->get_plate_name());
            dlg.set_plate_name(curr_plate_name);

            int result=dlg.ShowModal();
            if (result == wxID_YES) {
                wxString dlg_plate_name = dlg.get_plate_name();
                curr_plate->set_plate_name(dlg_plate_name.ToUTF8().data());
            }
        } else {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "can not select plate %1%" << plate_index;
            ret = -1;
        }
    } else if ((action == 7) && (!right_click)) {
        // move plate to the front
        take_snapshot("move plate to the front");
        ret = p->partplate_list.move_plate_to_index(plate_index,0);
        p->partplate_list.update_slice_context_to_current_plate(p->background_process);
        p->preview->update_gcode_result(p->partplate_list.get_current_slice_result_wrapper());
        p->sidebar->obj_list()->reload_all_plates();
        p->partplate_list.update_plates();
        update();
        p->partplate_list.select_plate(0);
    }

    else
    {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << "invalid action %1%, with right_click=%2%" << action << right_click;
        ret = -1;
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: return %2%")%__LINE__ % ret;
    return ret;
}

int Plater::duplicate_plate(int plate_index)
{
    int index = plate_index, ret;
    if (plate_index == -1)
        index = p->partplate_list.get_curr_plate_index();

    ret = p->partplate_list.duplicate_plate(index);

    //need to call update
    update();
    return ret;
}

//BBS: delete the plate, index= -1 means the current plate
int Plater::delete_plate(int plate_index)
{
    int index = plate_index, ret;

    if (plate_index == -1)
        index = p->partplate_list.get_curr_plate_index();

    take_snapshot("delete partplate");
    ret = p->partplate_list.delete_plate(index);

    //BBS: update the current print to the current plate
    p->partplate_list.update_slice_context_to_current_plate(p->background_process);
    p->preview->update_gcode_result(p->partplate_list.get_current_slice_result_wrapper());
    p->sidebar->obj_list()->reload_all_plates();

    //need to call update
    update();
    return ret;
}

//BBS: is the background process slicing currently
bool Plater::is_background_process_slicing() const
{
    return p->m_is_slicing;
}

//BBS: update slicing context
void Plater::update_slicing_context_to_current_partplate()
{
    p->partplate_list.update_slice_context_to_current_plate(p->background_process);
    p->preview->update_gcode_result(p->partplate_list.get_current_slice_result_wrapper());
}

//BBS: show object info
void Plater::show_object_info()
{
    NotificationManager *notify_manager = get_notification_manager();
    const Selection& selection = get_selection();
    int selCount = selection.get_volume_idxs().size();
    ModelObjectPtrs objects = model().objects;
    int obj_idx = selection.get_object_idx();
    std::string info_text;

    if (selCount > 1 && !selection.is_single_full_object()) {
        notify_manager->bbl_close_objectsinfo_notification();
        if (selection.get_mode() == Selection::EMode::Volume) {
            info_text += (boost::format(_utf8(L("Number of currently selected parts: %1%\n"))) % selCount).str();
        } else if (selection.get_mode() == Selection::EMode::Instance) {
            int content_count = selection.get_content().size();
            info_text += (boost::format(_utf8(L("Number of currently selected objects: %1%\n"))) % content_count).str();
        }
        notify_manager->bbl_show_objectsinfo_notification(info_text, false, !(p->current_panel == p->view3D));
        return;
    }
    else if (objects.empty() || (obj_idx < 0) || (obj_idx >= objects.size()) ||
        objects[obj_idx]->volumes.empty() ||// hack to avoid crash when deleting the last object on the bed
        (selection.is_single_full_object() && objects[obj_idx]->instances.size()> 1) ||
        !(selection.is_single_full_instance() || selection.is_single_volume()))
    {
        notify_manager->bbl_close_objectsinfo_notification();
        return;
    }

    const ModelObject* model_object = objects[obj_idx];
    int inst_idx = selection.get_instance_idx();
    if ((inst_idx < 0) || (inst_idx >= model_object->instances.size()))
    {
        notify_manager->bbl_close_objectsinfo_notification();
        return;
    }
    bool imperial_units = AppAdapter::app_config()->get("use_inches") == "1";
    double koef = imperial_units ? GizmoObjectManipulation::mm_to_in : 1.0f;

    ModelVolume* vol = nullptr;
    Transform3d t;
    int face_count;
    Vec3d size;
    if (selection.is_single_volume()) {
        std::vector<int> obj_idxs, vol_idxs;
        AppAdapter::obj_list()->get_selection_indexes(obj_idxs, vol_idxs);
        if (vol_idxs.size() != 1)
        {
            //corner case when merge/split/remove
            return;
        }
        vol = model_object->volumes[vol_idxs[0]];
        t = model_object->instances[inst_idx]->get_matrix() * vol->get_matrix();
        info_text += (boost::format(_utf8(L("Part name: %1%\n"))) % vol->name).str();
        face_count = static_cast<int>(vol->mesh().facets_count());
        size = vol->get_convex_hull().transformed_bounding_box(t).size();
    }
    else {
        info_text += (boost::format(_utf8(L("Object name: %1%\n"))) % model_object->name).str();
        face_count = static_cast<int>(model_object->facets_count());
        size = model_object->instance_convex_hull_bounding_box(inst_idx).size();
    }

    if (imperial_units)
        info_text += (boost::format(_utf8(L("Size: %1% x %2% x %3% in\n"))) %(size(0)*koef) %(size(1)*koef) %(size(2)*koef)).str();
    else
        info_text += (boost::format(_utf8(L("Size: %1% x %2% x %3% mm\n"))) %size(0) %size(1) %size(2)).str();

    const TriangleMeshStats& stats = vol ? vol->mesh().stats() : model_object->get_object_stl_stats();
    double volume_val = stats.volume;
    if (vol)
        volume_val *= std::fabs(t.matrix().block(0, 0, 3, 3).determinant());
    volume_val = volume_val * pow(koef,3);
    if (imperial_units)
        info_text += (boost::format(_utf8(L("Volume: %1% in³\n"))) %volume_val).str();
    else
        info_text += (boost::format(_utf8(L("Volume: %1% mm³\n"))) %volume_val).str();
    info_text += (boost::format(_utf8(L("Triangles: %1%\n"))) %face_count).str();

    wxString info_manifold;
    int non_manifold_edges = 0;
    auto mesh_errors = p->sidebar->obj_list()->get_mesh_errors_info(&info_manifold, &non_manifold_edges);

    #ifndef __WINDOWS__
    if (non_manifold_edges > 0) {
        info_manifold += into_u8("\n" + _L("Tips:") + "\n" +_L("\"Fix Model\" feature is currently only on Windows. Please repair the model on Orca Slicer(windows) or CAD softwares."));
    }
    #endif //APPLE & LINUX

    info_manifold = "<Error>" + info_manifold + "</Error>";
    info_text += into_u8(info_manifold);
    notify_manager->bbl_show_objectsinfo_notification(info_text, is_windows10()&&(non_manifold_edges > 0), !(p->current_panel == p->view3D));
}

void Plater::post_process_string_object_exception(StringObjectException &err)
{
    PresetBundle* preset_bundle = app_preset_bundle();
    if (err.type == StringExceptionType::STRING_EXCEPT_FILAMENT_NOT_MATCH_BED_TYPE) {
        try {
            int extruder_id = atoi(err.params[2].c_str()) - 1;
            if (extruder_id < preset_bundle->filament_presets.size()) {
                std::string filament_name = preset_bundle->filament_presets[extruder_id];
                for (auto filament_it = preset_bundle->filaments.begin(); filament_it != preset_bundle->filaments.end(); filament_it++) {
                    if (filament_it->name == filament_name) {
                        if (filament_it->is_system) {
                            filament_name = filament_it->alias;
                        } else {
                            auto preset = preset_bundle->filaments.get_preset_base(*filament_it);
                            if (preset && !preset->alias.empty()) {
                                filament_name = preset->alias;
                            } else {
                                char target = '@';
                                size_t pos    = filament_name.find(target);
                                if (pos != std::string::npos) {
                                    filament_name = filament_name.substr(0, pos - 1);
                                }
                            }
                        }
                        break;
                    }
                }
                err.string = format(_L("Plate %d: %s is not suggested to be used to print filament %s(%s). If you still want to do this printing, please set this filament's bed temperature to non-zero."),
                             err.params[0], err.params[1], err.params[2], filament_name);
                err.string += "\n";
            }
        } catch (...) {
            ;
        }
    }

    return;
}

#if ENABLE_ENVIRONMENT_MAP
void Plater::init_environment_texture()
{
    if (p->environment_texture.get_id() == 0)
        p->environment_texture.load_from_file(resources_dir() + "/images/Pmetal_001.png", false, GLTexture::SingleThreaded, false);
}

unsigned int Plater::get_environment_texture_id() const
{
    return p->environment_texture.get_id();
}
#endif // ENABLE_ENVIRONMENT_MAP

const BuildVolume& Plater::build_volume() const
{
    return PlateBed::build_volume();
}

const GLToolbar& Plater::get_collapse_toolbar() const
{
    return p->collapse_toolbar;
}

GLToolbar& Plater::get_collapse_toolbar()
{
    return p->collapse_toolbar;
}

void Plater::update_preview_bottom_toolbar()
{
    p->update_preview_bottom_toolbar();
}

void Plater::reset_gcode_toolpaths()
{
    //BBS: add some logs
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": reset the gcode viewer's toolpaths");
    p->reset_gcode_toolpaths();
}

const Mouse3DController& Plater::get_mouse3d_controller() const
{
    return p->mouse3d_controller;
}

Mouse3DController& Plater::get_mouse3d_controller()
{
    return p->mouse3d_controller;
}

NotificationManager * Plater::get_notification_manager()
{
    return p->notification_manager.get();
}

DailyTipsWindow* Plater::get_dailytips() const
{
    static DailyTipsWindow* dailytips_win = new DailyTipsWindow();
    return dailytips_win;
}

const NotificationManager * Plater::get_notification_manager() const
{
    return p->notification_manager.get();
}

void Plater::init_notification_manager()
{
    p->init_notification_manager();
}

void Plater::set_notification_manager()
{
    p->notification_manager->set_slicing_progress_began();
}

void Plater::show_status_message(std::string s)
{
    BOOST_LOG_TRIVIAL(trace) << "show_status_message:" << s;
}

bool Plater::can_delete() const { return p->can_delete(); }
bool Plater::can_delete_all() const { return p->can_delete_all(); }
bool Plater::can_add_model() const { return !is_background_process_slicing(); }
bool Plater::can_add_plate() const { return !is_background_process_slicing() && p->can_add_plate(); }
bool Plater::can_delete_plate() const { return p->can_delete_plate(); }
bool Plater::can_increase_instances() const { return p->can_increase_instances(); }
bool Plater::can_decrease_instances() const { return p->can_decrease_instances(); }
bool Plater::can_set_instance_to_object() const { return p->can_set_instance_to_object(); }
bool Plater::can_fix_through_netfabb() const { return p->can_fix_through_netfabb(); }
bool Plater::can_simplify() const { return p->can_simplify(); }
bool Plater::can_split_to_objects() const { return p->can_split_to_objects(); }
bool Plater::can_split_to_volumes() const { return p->can_split_to_volumes(); }
bool Plater::can_arrange() const { return p->can_arrange(); }
bool Plater::can_layers_editing() const { return p->can_layers_editing(); }
bool Plater::can_paste_from_clipboard() const
{
    if (!IsShown() || !p->is_view3D_shown()) return false;

    const Selection& selection = p->get_selection();
    const Selection::Clipboard& clipboard = selection.get_clipboard();

    if (clipboard.is_empty() && p->sidebar->obj_list()->clipboard_is_empty())
        return false;

    if ((app_preset_bundle()->printers.get_edited_preset().printer_technology() == ptSLA) && !clipboard.is_sla_compliant())
        return false;

    Selection::EMode mode = clipboard.get_mode();
    if ((mode == Selection::Volume) && !selection.is_from_single_instance())
        return false;

    if ((mode == Selection::Instance) && (selection.get_mode() != Selection::Instance))
        return false;

    return true;
}

//BBS support cut
bool Plater::can_cut_to_clipboard() const
{
    if (is_selection_empty())
        return false;
    return true;
}

bool Plater::can_copy_to_clipboard() const
{
    if (!IsShown() || !p->is_view3D_shown())
        return false;

    if (is_selection_empty())
        return false;

    const Selection& selection = p->get_selection();
    if ((app_preset_bundle()->printers.get_edited_preset().printer_technology() == ptSLA) && !selection.is_sla_compliant())
        return false;

    return true;
}

bool Plater::can_undo() const { return IsShown() && p->is_view3D_shown() && p->undo_redo_stack().has_undo_snapshot(); }
bool Plater::can_redo() const { return IsShown() && p->is_view3D_shown() && p->undo_redo_stack().has_redo_snapshot(); }
bool Plater::can_reload_from_disk() const { return p->can_reload_from_disk(); }
//BBS
bool Plater::can_fillcolor() const { return p->can_fillcolor(); }
bool Plater::can_replace_with_stl() const { return p->can_replace_with_stl(); }
bool Plater::can_mirror() const { return p->can_mirror(); }
bool Plater::can_split(bool to_objects) const { return p->can_split(to_objects); }
bool Plater::can_scale_to_print_volume() const { return p->can_scale_to_print_volume(); }

const UndoRedo::Stack& Plater::undo_redo_stack_main() const { return p->undo_redo_stack_main(); }
void Plater::clear_undo_redo_stack_main() { p->undo_redo_stack_main().clear(); }
void Plater::enter_gizmos_stack() { p->enter_gizmos_stack(); }
bool Plater::leave_gizmos_stack() { return p->leave_gizmos_stack(); } // BBS: return false if not changed
bool Plater::inside_snapshot_capture() { return p->inside_snapshot_capture(); }

void Plater::toggle_show_wireframe()
{
    p->show_wireframe = !p->show_wireframe;
}

bool Plater::is_show_wireframe() const
{
    return p->show_wireframe;
}

void Plater::enable_wireframe(bool status)
{
    p->wireframe_enabled = status;
}

bool Plater::is_wireframe_enabled() const
{
    return p->wireframe_enabled;
}

// Wrapper around wxWindow::PopupMenu to suppress error messages popping out while tracking the popup menu.
bool Plater::PopupMenu(wxMenu *menu, const wxPoint& pos)
{
    // Don't want to wake up and trigger reslicing while tracking the pop-up menu.
    SuppressBackgroundProcessingUpdate sbpu;
    // When tracking a pop-up menu, postpone error messages from the slicing result.
    m_tracking_popup_menu = true;
    bool out = AppAdapter::main_panel()->PopupMenu(menu, pos);
    m_tracking_popup_menu = false;
    if (! m_tracking_popup_menu_error_message.empty()) {
        // Don't know whether the CallAfter is necessary, but it should not hurt.
        // The menus likely sends out some commands, so we may be safer if the dialog is shown after the menu command is processed.
        wxString message = std::move(m_tracking_popup_menu_error_message);
        wxTheApp->CallAfter([message, this]() { show_error(this, message); });
        m_tracking_popup_menu_error_message.clear();
    }
    return out;
}
void Plater::bring_instance_forward()
{
    p->bring_instance_forward();
}

bool Plater::need_update() const
{
    return p->need_update();
}

void Plater::set_need_update(bool need_update)
{
    p->set_need_update(need_update);
}

// BBS
//BBS: add popup logic for table object
bool Plater::PopupObjectTable(int object_id, int volume_id, const wxPoint& position)
{
    return p->PopupObjectTable(object_id, volume_id, position);
}

bool Plater::PopupObjectTableBySelection()
{
    wxDataViewItem item;
    int obj_idx, vol_idx;
    const wxPoint pos = wxPoint(0, 0);  //Fake position
    AppAdapter::obj_list()->get_selected_item_indexes(obj_idx, vol_idx, item);
    return p->PopupObjectTable(obj_idx, vol_idx, pos);
}

void Plater::update_title_dirty_status()
{
    p->update_title_dirty_status();
}

void Plater::update_gizmos_on_off_state()
{
    p->update_gizmos_on_off_state();
}

void Plater::reset_all_gizmos()
{
    p->reset_all_gizmos();
}

wxMenu* Plater::plate_menu()            { return p->menus.plate_menu();             }
wxMenu* Plater::object_menu()           { return p->menus.object_menu();            }
wxMenu* Plater::part_menu()             { return p->menus.part_menu();              }
wxMenu* Plater::text_part_menu()        { return p->menus.text_part_menu();         }
wxMenu* Plater::svg_part_menu()         { return p->menus.svg_part_menu();          }
wxMenu* Plater::sla_object_menu()       { return p->menus.sla_object_menu();        }
wxMenu* Plater::default_menu()          { return p->menus.default_menu();           }
wxMenu* Plater::instance_menu()         { return p->menus.instance_menu();          }
wxMenu* Plater::layer_menu()            { return p->menus.layer_menu();             }
wxMenu* Plater::multi_selection_menu()  { return p->menus.multi_selection_menu();   }
int     Plater::GetPlateIndexByRightMenuInLeftUI() { return p->m_is_RightClickInLeftUI; }
void    Plater::SetPlateIndexByRightMenuInLeftUI(int index) { p->m_is_RightClickInLeftUI = index; }
SuppressBackgroundProcessingUpdate::SuppressBackgroundProcessingUpdate() :
    m_was_scheduled(AppAdapter::plater()->is_background_process_update_scheduled())
{
    AppAdapter::plater()->suppress_background_process(m_was_scheduled);
}

SuppressBackgroundProcessingUpdate::~SuppressBackgroundProcessingUpdate()
{
    AppAdapter::plater()->schedule_background_process(m_was_scheduled);
}


void Plater::calib_pa(const Calib_Params& params)
{
    const auto calib_pa_name = wxString::Format(L"Pressure Advance Test");
    new_project(false, false, calib_pa_name);
    AppAdapter::main_panel()->select_tab(size_t(MainPanel::tp3DEditor));
    switch (params.mode) {
        case CalibMode::Calib_PA_Line:
            add_model(false, Slic3r::resources_dir() + "/calib/pressure_advance/pressure_advance_test.stl");
            break;
        case CalibMode::Calib_PA_Pattern:
            _calib_pa_pattern(params);
            break;
        case CalibMode::Calib_PA_Tower:
            _calib_pa_tower(params);
            break;
        default: break;
    }

    p->background_process.fff_print()->set_calib_params(params);
}

void Plater::calib_flowrate(bool is_linear, int pass) {
    if (pass != 1 && pass != 2)
        return;
    wxString calib_name;
    if (is_linear) {
        calib_name = L"Orca YOLO Flow Calibration";
        if (pass == 2)
            calib_name += L" - Perfectionist version";
    } else
        calib_name = wxString::Format(L"Flowrate Test - Pass%d", pass);

    if (new_project(false, false, calib_name) == wxID_CANCEL)
        return;

    AppAdapter::main_panel()->select_tab(size_t(MainPanel::tp3DEditor));

    if (is_linear) {
        if (pass == 1)
            add_model(false,
                      (boost::filesystem::path(Slic3r::resources_dir()) / "calib" / "filament_flow" / "Orca-LinearFlow.3mf").string());
        else
            add_model(false,
                      (boost::filesystem::path(Slic3r::resources_dir()) / "calib" / "filament_flow" / "Orca-LinearFlow_fine.3mf").string());
    } else {
        if (pass == 1)
            add_model(false,
                      (boost::filesystem::path(Slic3r::resources_dir()) / "calib" / "filament_flow" / "flowrate-test-pass1.3mf").string());
        else
            add_model(false,
                      (boost::filesystem::path(Slic3r::resources_dir()) / "calib" / "filament_flow" / "flowrate-test-pass2.3mf").string());
    }

    adjust_settings_for_flowrate_calib(model().objects, is_linear, pass);
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->reload_config();
}

void Plater::calib_temp(const Calib_Params& params) {
    const auto calib_temp_name = wxString::Format(L"Nozzle temperature test");
    new_project(false, false, calib_temp_name);
    AppAdapter::main_panel()->select_tab(size_t(MainPanel::tp3DEditor));
    if (params.mode != CalibMode::Calib_Temp_Tower)
        return;
    
    add_model(false, Slic3r::resources_dir() + "/calib/temperature_tower/temperature_tower.stl");
    auto filament_config = &app_preset_bundle()->filaments.get_edited_preset().config;
    auto start_temp = lround(params.start);
    filament_config->set_key_value("nozzle_temperature_initial_layer", new ConfigOptionInts(1,(int)start_temp));
    filament_config->set_key_value("nozzle_temperature", new ConfigOptionInts(1,(int)start_temp));
    model().objects[0]->config.set_key_value("brim_type", new ConfigOptionEnum<BrimType>(btOuterOnly));
    model().objects[0]->config.set_key_value("brim_width", new ConfigOptionFloat(5.0));
    model().objects[0]->config.set_key_value("brim_object_gap", new ConfigOptionFloat(0.0));
    model().objects[0]->config.set_key_value("alternate_extra_wall", new ConfigOptionBool(false));
    model().objects[0]->config.set_key_value("seam_slope_type", new ConfigOptionEnum<SeamScarfType>(SeamScarfType::None));

    changed_objects({ 0 });
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->reload_config();

    // cut upper
    auto obj_bb = model().objects[0]->bounding_box_exact();
    auto block_count = lround((350 - params.end) / 5 + 1);
    if(block_count > 0){
        // add EPSILON offset to avoid cutting at the exact location where the flat surface is
        auto new_height = block_count * 10.0 + EPSILON;
        if (new_height < obj_bb.size().z()) {
            cut_horizontal(0, 0, new_height, ModelObjectCutAttribute::KeepLower);
        }
    }
    
    // cut bottom
    obj_bb = model().objects[0]->bounding_box_exact();
    block_count = lround((350 - params.start) / 5);
    if(block_count > 0){
        auto new_height = block_count * 10.0 + EPSILON;
        if (new_height < obj_bb.size().z()) {
            cut_horizontal(0, 0, new_height, ModelObjectCutAttribute::KeepUpper);
        }
    }
    
    p->background_process.fff_print()->set_calib_params(params);
}

void Plater::calib_max_vol_speed(const Calib_Params& params)
{
    const auto calib_vol_speed_name = wxString::Format(L"Max volumetric speed test");
    new_project(false, false, calib_vol_speed_name);
    AppAdapter::main_panel()->select_tab(size_t(MainPanel::tp3DEditor));
    if (params.mode != CalibMode::Calib_Vol_speed_Tower)
        return;

    add_model(false, Slic3r::resources_dir() + "/calib/volumetric_speed/SpeedTestStructure.step");

    auto print_config = &app_preset_bundle()->prints.get_edited_preset().config;
    auto filament_config = &app_preset_bundle()->filaments.get_edited_preset().config;
    auto printer_config = &app_preset_bundle()->printers.get_edited_preset().config;
    auto obj = model().objects[0];
    auto& obj_cfg = obj->config;

    auto bed_shape = printer_config->option<ConfigOptionPoints>("printable_area")->values;
    BoundingBoxf bed_ext = get_extents(bed_shape);
    auto scale_obj = (bed_ext.size().x() - 10) / obj->bounding_box_exact().size().x();
    if (scale_obj < 1.0)
        obj->scale(scale_obj, 1, 1);

    const ConfigOptionFloats* nozzle_diameter_config = printer_config->option<ConfigOptionFloats>("nozzle_diameter");
    assert(nozzle_diameter_config->values.size() > 0);
    double nozzle_diameter = nozzle_diameter_config->values[0];
    double line_width = nozzle_diameter * 1.75;
    double layer_height = nozzle_diameter * 0.8;

    auto max_lh = printer_config->option<ConfigOptionFloats>("max_layer_height");
    if (max_lh->values[0] < layer_height)
        max_lh->values[0] = { layer_height };

    filament_config->set_key_value("filament_max_volumetric_speed", new ConfigOptionFloats { 200 });
    filament_config->set_key_value("slow_down_layer_time", new ConfigOptionFloats{0.0});
    
    obj_cfg.set_key_value("enable_overhang_speed", new ConfigOptionBool { false });
    obj_cfg.set_key_value("wall_loops", new ConfigOptionInt(1));
    obj_cfg.set_key_value("alternate_extra_wall", new ConfigOptionBool(false));
    obj_cfg.set_key_value("top_shell_layers", new ConfigOptionInt(0));
    obj_cfg.set_key_value("bottom_shell_layers", new ConfigOptionInt(0));
    obj_cfg.set_key_value("sparse_infill_density", new ConfigOptionPercent(0));
    obj_cfg.set_key_value("overhang_reverse", new ConfigOptionBool(false));
    obj_cfg.set_key_value("outer_wall_line_width", new ConfigOptionFloatOrPercent(line_width, false));
    obj_cfg.set_key_value("layer_height", new ConfigOptionFloat(layer_height));
    obj_cfg.set_key_value("brim_type", new ConfigOptionEnum<BrimType>(btOuterAndInner));
    obj_cfg.set_key_value("brim_width", new ConfigOptionFloat(5.0));
    obj_cfg.set_key_value("brim_object_gap", new ConfigOptionFloat(0.0));
    print_config->set_key_value("timelapse_type", new ConfigOptionEnum<TimelapseType>(tlTraditional));
    print_config->set_key_value("spiral_mode", new ConfigOptionBool(true));
    print_config->set_key_value("max_volumetric_extrusion_rate_slope", new ConfigOptionFloat(0));

    changed_objects({ 0 });
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->reload_config();

    //  cut upper
    auto obj_bb = obj->bounding_box_exact();
    auto height = (params.end - params.start + 1) / params.step;
    if (height < obj_bb.size().z()) {
        cut_horizontal(0, 0, height, ModelObjectCutAttribute::KeepLower);
    }

    auto new_params = params;
    auto mm3_per_mm = Flow(line_width, layer_height, nozzle_diameter).mm3_per_mm() *
                      filament_config->option<ConfigOptionFloats>("filament_flow_ratio")->get_at(0);
    new_params.end = params.end / mm3_per_mm;
    new_params.start = params.start / mm3_per_mm;
    new_params.step = params.step / mm3_per_mm;


    p->background_process.fff_print()->set_calib_params(new_params);
}

void Plater::calib_retraction(const Calib_Params& params)
{
    const auto calib_retraction_name = wxString::Format(L"Retraction test");
    new_project(false, false, calib_retraction_name);
    AppAdapter::main_panel()->select_tab(size_t(MainPanel::tp3DEditor));
    if (params.mode != CalibMode::Calib_Retraction_tower)
        return;

    add_model(false, Slic3r::resources_dir() + "/calib/retraction/retraction_tower.stl");

    auto print_config = &app_preset_bundle()->prints.get_edited_preset().config;
    auto filament_config = &app_preset_bundle()->filaments.get_edited_preset().config;
    auto printer_config = &app_preset_bundle()->printers.get_edited_preset().config;
    auto obj = model().objects[0];

    double layer_height = 0.2;

    auto max_lh = printer_config->option<ConfigOptionFloats>("max_layer_height");
    if (max_lh->values[0] < layer_height)
        max_lh->values[0] = { layer_height };

    obj->config.set_key_value("wall_loops", new ConfigOptionInt(2));
    obj->config.set_key_value("top_shell_layers", new ConfigOptionInt(0));
    obj->config.set_key_value("bottom_shell_layers", new ConfigOptionInt(3));
    obj->config.set_key_value("sparse_infill_density", new ConfigOptionPercent(0));
    print_config->set_key_value("initial_layer_print_height", new ConfigOptionFloat(layer_height));
    obj->config.set_key_value("layer_height", new ConfigOptionFloat(layer_height));
    obj->config.set_key_value("alternate_extra_wall", new ConfigOptionBool(false));

    changed_objects({ 0 });

    //  cut upper
    auto obj_bb = obj->bounding_box_exact();
    auto height = 1.0 + 0.4 + ((params.end - params.start)) / params.step;
    if (height < obj_bb.size().z()) {
        cut_horizontal(0, 0, height, ModelObjectCutAttribute::KeepLower);
    }

    p->background_process.fff_print()->set_calib_params(params);
}

void Plater::calib_VFA(const Calib_Params& params)
{
    const auto calib_vfa_name = wxString::Format(L"VFA test");
    new_project(false, false, calib_vfa_name);
    AppAdapter::main_panel()->select_tab(size_t(MainPanel::tp3DEditor));
    if (params.mode != CalibMode::Calib_VFA_Tower)
        return;

    add_model(false, Slic3r::resources_dir() + "/calib/vfa/VFA.stl");
    auto print_config = &app_preset_bundle()->prints.get_edited_preset().config;
    auto filament_config = &app_preset_bundle()->filaments.get_edited_preset().config;
    filament_config->set_key_value("slow_down_layer_time", new ConfigOptionFloats { 0.0 });
    filament_config->set_key_value("filament_max_volumetric_speed", new ConfigOptionFloats { 200 });
    print_config->set_key_value("enable_overhang_speed", new ConfigOptionBool { false });
    print_config->set_key_value("timelapse_type", new ConfigOptionEnum<TimelapseType>(tlTraditional));
    print_config->set_key_value("wall_loops", new ConfigOptionInt(1));
    print_config->set_key_value("alternate_extra_wall", new ConfigOptionBool(false));
    print_config->set_key_value("top_shell_layers", new ConfigOptionInt(0));
    print_config->set_key_value("bottom_shell_layers", new ConfigOptionInt(1));
    print_config->set_key_value("sparse_infill_density", new ConfigOptionPercent(0));
    print_config->set_key_value("overhang_reverse", new ConfigOptionBool(false));
    print_config->set_key_value("detect_thin_wall", new ConfigOptionBool(false));
    print_config->set_key_value("spiral_mode", new ConfigOptionBool(true));
    model().objects[0]->config.set_key_value("brim_type", new ConfigOptionEnum<BrimType>(btOuterOnly));
    model().objects[0]->config.set_key_value("brim_width", new ConfigOptionFloat(3.0));
    model().objects[0]->config.set_key_value("brim_object_gap", new ConfigOptionFloat(0.0));

    changed_objects({ 0 });
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update_ui_from_settings();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->update_ui_from_settings();

    // cut upper
    auto obj_bb = model().objects[0]->bounding_box_exact();
    auto height = 5 * ((params.end - params.start) / params.step + 1);
    if (height < obj_bb.size().z()) {
        cut_horizontal(0, 0, height, ModelObjectCutAttribute::KeepLower);
    }

    p->background_process.fff_print()->set_calib_params(params);
}

void Plater::_calib_pa_pattern(const Calib_Params& params)
{
    std::vector<double> speeds{params.speeds};
    std::vector<double> accels{params.accelerations};
    std::vector<size_t> object_idxs{};
    /* Set common parameters */
    DynamicPrintConfig& printer_config = app_preset_bundle()->printers.get_edited_preset().config;
    DynamicPrintConfig& print_config = app_preset_bundle()->prints.get_edited_preset().config;
    auto filament_config = &app_preset_bundle()->filaments.get_edited_preset().config;
    double nozzle_diameter = printer_config.option<ConfigOptionFloats>("nozzle_diameter")->get_at(0);
    filament_config->set_key_value("filament_retract_when_changing_layer", new ConfigOptionBoolsNullable{false});
    filament_config->set_key_value("filament_wipe", new ConfigOptionBoolsNullable{false});
    printer_config.set_key_value("wipe", new ConfigOptionBools{false});
    printer_config.set_key_value("retract_when_changing_layer", new ConfigOptionBools{false});

    //Orca: find acceleration to use in the test
    auto accel = print_config.option<ConfigOptionFloat>("outer_wall_acceleration")->value; // get the outer wall acceleration
    if (accel == 0) // if outer wall accel isnt defined, fall back to inner wall accel
        accel = print_config.option<ConfigOptionFloat>("inner_wall_acceleration")->value;
    if (accel == 0) // if inner wall accel is not defined fall back to default accel
        accel = print_config.option<ConfigOptionFloat>("default_acceleration")->value;
    // Orca: Set all accelerations except first layer, as the first layer accel doesnt affect the PA test since accel
    // is set to the travel accel before printing the pattern.
    if (accels.empty()) {
        accels.assign({accel});
        const auto msg{_L("INFO:") + "\n" +
                       _L("No accelerations provided for calibration. Use default acceleration value ") + std::to_string(long(accel)) + _L("mm/s²")};
        get_notification_manager()->push_notification(msg.ToStdString());
    } else {
        // set max acceleration in case of batch mode to get correct test pattern size
        accel = *std::max_element(accels.begin(), accels.end());
    }
    print_config.set_key_value( "outer_wall_acceleration", new ConfigOptionFloat(accel));
    print_config.set_key_value( "print_sequence", new ConfigOptionEnum(PrintSequence::ByLayer));
    
    //Orca: find jerk value to use in the test
    if(print_config.option<ConfigOptionFloat>("default_jerk")->value > 0){ // we have set a jerk value
        auto jerk = print_config.option<ConfigOptionFloat>("outer_wall_jerk")->value; // get outer wall jerk
        if (jerk == 0) // if outer wall jerk is not defined, get inner wall jerk
            jerk = print_config.option<ConfigOptionFloat>("inner_wall_jerk")->value;
        if (jerk == 0) // if inner wall jerk is not defined, get the default jerk
            jerk = print_config.option<ConfigOptionFloat>("default_jerk")->value;
        
        //Orca: Set jerk values. Again first layer jerk should not matter as it is reset to the travel jerk before the
        // first PA pattern is printed.
        print_config.set_key_value( "default_jerk", new ConfigOptionFloat(jerk));
        print_config.set_key_value( "outer_wall_jerk", new ConfigOptionFloat(jerk));
        print_config.set_key_value( "inner_wall_jerk", new ConfigOptionFloat(jerk));
        print_config.set_key_value( "top_surface_jerk", new ConfigOptionFloat(jerk));
        print_config.set_key_value( "infill_jerk", new ConfigOptionFloat(jerk));
        print_config.set_key_value( "travel_jerk", new ConfigOptionFloat(jerk));
    }
    
    for (const auto& opt : SuggestedConfigCalibPAPattern().float_pairs) {
        print_config.set_key_value(
            opt.first,
            new ConfigOptionFloat(opt.second)
        );
    }

    for (const auto& opt : SuggestedConfigCalibPAPattern().nozzle_ratio_pairs) {
        print_config.set_key_value(
            opt.first,
            new ConfigOptionFloatOrPercent(nozzle_diameter * opt.second / 100, false)
        );
    }

    for (const auto& opt : SuggestedConfigCalibPAPattern().int_pairs) {
        print_config.set_key_value(
            opt.first,
            new ConfigOptionInt(opt.second)
        );
    }

    print_config.set_key_value(
        SuggestedConfigCalibPAPattern().brim_pair.first,
        new ConfigOptionEnum<BrimType>(SuggestedConfigCalibPAPattern().brim_pair.second)
    );

    // Orca: Set the outer wall speed to the optimal speed for the test, cap it with max volumetric speed
    if (speeds.empty()) {
        double speed = CalibPressureAdvance::find_optimal_PA_speed(
            app_preset_bundle()->full_config(),
            (fabs(print_config.get_abs_value("line_width", nozzle_diameter)) <= DBL_EPSILON) ?
                (nozzle_diameter * 1.125) :
                print_config.get_abs_value("line_width", nozzle_diameter),
            print_config.get_abs_value("layer_height"), 0);
        print_config.set_key_value("outer_wall_speed", new ConfigOptionFloat(speed));

        speeds.assign({speed});
        const auto msg{_L("INFO:") + "\n" +
                       _L("No speeds provided for calibration. Use default optimal speed ") + std::to_string(long(speed)) + _L("mm/s")};
        get_notification_manager()->push_notification(msg.ToStdString());
    } else if (speeds.size() == 1) {
        // If we have single value provided, set speed using global configuration.
        // per-object config is not set in this case
        print_config.set_key_value("outer_wall_speed", new ConfigOptionFloat(speeds.front()));
    }

    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->reload_config();

    const DynamicPrintConfig full_config = app_preset_bundle()->full_config();
    PresetBundle* preset_bundle = app_preset_bundle();
    auto cur_plate = get_partplate_list().get_plate(0);

    // add "handle" cube
    sidebar().obj_list()->load_generic_subobject("Cube", ModelVolumeType::INVALID);
    auto *cube = model().objects[0];

    CalibPressureAdvancePattern pa_pattern(
        params,
        full_config,
        false,
        *cube,
        cur_plate->get_origin()
    );

    /* Having PA pattern configured, we could make a set of polygons resembling N test patterns.
     * We'll arrange this set of polygons, so we would know position of each test pattern and
     * could position test cubes later on
     *
     * We'll take advantage of already existing cube: scale it up to test pattern size to use
     * as a reference for objects arrangement. Polygon is slightly oversized to add spaces between patterns.
     * That arrangement will be used to place 'handle cubes' for each test. */
    auto cube_bb = cube->raw_bounding_box();
    cube->scale((pa_pattern.print_size_x() + 4) / cube_bb.size().x(),
                (pa_pattern.print_size_y() + 4) / cube_bb.size().y(),
                pa_pattern.max_layer_z() / cube_bb.size().z());

    arrangement::ArrangePolygons arranged_items;
    {
        arrangement::ArrangeParams ap;
        Points bedpts = arrangement::get_shrink_bedpts(&full_config, ap);

        for(size_t i = 0; i < speeds.size() * accels.size(); i++) {
            arrangement::ArrangePolygon p;
            cube->instances[0]->get_arrange_polygon(&p);
            p.bed_idx = 0;
            arranged_items.emplace_back(p);
        }

        arrangement::arrange(arranged_items, bedpts, ap);
    }

    /* scale cube back to the size of test pattern 'handle' */
    cube_bb = cube->raw_bounding_box();
    cube->scale(pa_pattern.handle_xy_size() / cube_bb.size().x(),
                pa_pattern.handle_xy_size() / cube_bb.size().y(),
                pa_pattern.max_layer_z() / cube_bb.size().z());

    /* Set speed and acceleration on per-object basis and arrange anchor object on the plates.
     * Test gcode will be genecated during plate slicing */
    for(size_t test_idx = 0; test_idx < arranged_items.size(); test_idx++) {
        const auto &ai = arranged_items[test_idx];
        size_t plate_idx = arranged_items[test_idx].bed_idx;
        auto tspd = speeds[test_idx % speeds.size()];
        auto tacc = accels[test_idx / speeds.size()];

        /* make an own copy of anchor cube for each test */
        auto obj = test_idx == 0 ? cube : model().add_object(*cube);
        auto obj_idx = std::distance(model().objects.begin(), std::find(model().objects.begin(), model().objects.end(), obj));
        obj->name.assign(std::string("pa_pattern_") + std::to_string(int(tspd)) + std::string("_") + std::to_string(int(tacc)));

        auto &obj_config = obj->config;
        if (speeds.size() > 1)
            obj_config.set_key_value("outer_wall_speed", new ConfigOptionFloat(tspd));
        if (accels.size() > 1)
            obj_config.set_key_value("outer_wall_acceleration", new ConfigOptionFloat(tacc));

        auto cur_plate = get_partplate_list().get_plate(plate_idx);
        if (!cur_plate) {
            plate_idx = get_partplate_list().create_plate();
            cur_plate = get_partplate_list().get_plate(plate_idx);
        }

        object_idxs.emplace_back(obj_idx);
        get_partplate_list().add_to_plate(obj_idx, 0, plate_idx);
        const Vec3d obj_offset{unscale<double>(ai.translation(X)),
                               unscale<double>(ai.translation(Y)),
                               0};
        obj->instances[0]->set_offset(cur_plate->get_origin() + obj_offset + pa_pattern.handle_pos_offset());
        obj->ensure_on_bed();

        if (obj_idx == 0)
            sidebar().obj_list()->update_name_for_items();
        else
            sidebar().obj_list()->add_object_to_list(obj_idx);
    }

    model().calib_pa_pattern = std::make_unique<CalibPressureAdvancePattern>(pa_pattern);
    changed_objects(object_idxs);
}

void Plater::_calib_pa_pattern_gen_gcode()
{
    if (!model().calib_pa_pattern)
        return;

    auto cur_plate = get_partplate_list().get_curr_plate();
    if (cur_plate->empty())
        return;

    /* Container to store custom g-codes genereted by the test generator.
     * We'll store gcode for all tests on a single plate here. Once the plate handling is done,
     * all the g-codes will be merged into a single one on per-layer basis */
    std::vector<CustomGCode::Info> mgc;
    PresetBundle *preset_bundle = app_preset_bundle();

    /* iterate over all cubes on current plate and generate gcode for them */
    for (auto obj : cur_plate->get_objects_on_this_plate()) {
        auto gcode = model().calib_pa_pattern->generate_custom_gcodes(
                                preset_bundle->full_config(),
                                false,
                                *obj,
                                cur_plate->get_origin()
        );
        mgc.emplace_back(gcode);
    }

    // move first item into model custom gcode
    auto &pcgc = model().plates_custom_gcodes[get_partplate_list().get_curr_plate_index()];
    pcgc = std::move(mgc[0]);
    mgc.erase(mgc.begin());

    // concat layer gcodes for each test
    for (size_t i = 0; i < pcgc.gcodes.size(); i++) {
        for (auto &gc : mgc) {
            pcgc.gcodes[i].extra += gc.gcodes[i].extra;
        }
    }
}

void Plater::_calib_pa_tower(const Calib_Params& params) {
    add_model(false, Slic3r::resources_dir() + "/calib/pressure_advance/tower_with_seam.stl");

    auto& print_config = app_preset_bundle()->prints.get_edited_preset().config;
    auto printer_config = &app_preset_bundle()->printers.get_edited_preset().config;
    auto filament_config = &app_preset_bundle()->filaments.get_edited_preset().config;

    const double nozzle_diameter = printer_config->option<ConfigOptionFloats>("nozzle_diameter")->get_at(0);

    filament_config->set_key_value("slow_down_layer_time", new ConfigOptionFloats{ 1.0f });


    auto& obj_cfg = model().objects[0]->config;

    obj_cfg.set_key_value("alternate_extra_wall", new ConfigOptionBool(false));
    auto full_config = app_preset_bundle()->full_config();
    auto wall_speed = CalibPressureAdvance::find_optimal_PA_speed(
        full_config, full_config.get_abs_value("line_width", nozzle_diameter),
        full_config.get_abs_value("layer_height"), 0);
    obj_cfg.set_key_value("outer_wall_speed", new ConfigOptionFloat(wall_speed));
    obj_cfg.set_key_value("inner_wall_speed", new ConfigOptionFloat(wall_speed));
    obj_cfg.set_key_value("seam_position", new ConfigOptionEnum<SeamPosition>(spRear));
    obj_cfg.set_key_value("wall_loops", new ConfigOptionInt(2));
    obj_cfg.set_key_value("top_shell_layers", new ConfigOptionInt(0));
    obj_cfg.set_key_value("bottom_shell_layers", new ConfigOptionInt(0));
    obj_cfg.set_key_value("sparse_infill_density", new ConfigOptionPercent(0));
    obj_cfg.set_key_value("brim_type", new ConfigOptionEnum<BrimType>(btEar));
    obj_cfg.set_key_value("brim_object_gap", new ConfigOptionFloat(.0f));
    obj_cfg.set_key_value("brim_ears_max_angle", new ConfigOptionFloat(135.f));
    obj_cfg.set_key_value("brim_width", new ConfigOptionFloat(6.f));
    obj_cfg.set_key_value("seam_slope_type", new ConfigOptionEnum<SeamScarfType>(SeamScarfType::None));
    print_config.set_key_value("max_volumetric_extrusion_rate_slope", new ConfigOptionFloat(0));

    changed_objects({ 0 });
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->update_dirty();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_FILAMENT)->reload_config();
    AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER)->reload_config();

    auto new_height = std::ceil((params.end - params.start) / params.step) + 1;
    auto obj_bb = model().objects[0]->bounding_box_exact();
    if (new_height < obj_bb.size().z()) {
        cut_horizontal(0, 0, new_height, ModelObjectCutAttribute::KeepLower);
    }

    _calib_pa_select_added_objects();
}

void Plater::_calib_pa_select_added_objects() {
    // update printable state for new volumes on canvas3D
    AppAdapter::plater()->canvas3D()->update_instance_printable_state_for_objects({0});

    Selection& selection = p->get_selection();
    selection.clear();
    selection.add_object(0, false);

    // BBS: update object list selection
    p->sidebar->obj_list()->update_selections();
    selection.notify_instance_update(-1, -1);
    if (p->m_gizmos->is_enabled()) {
        p->update_gizmos_on_off_state();
    }
}

void Plater::calib_multi_nozzle(const Calib_Params& params)
{
    if (params.mode != CalibMode::Calib_Multi_Nozzle)
        return;
    
    const auto calib_multi_nozzle_name = wxString::Format(L"Multi-Nozzle Calibration");
    if (new_project(false, true, calib_multi_nozzle_name) != wxID_YES) {
        return;
    }

    try {
        // Get print configuration
        auto print_config = &app_preset_bundle()->prints.get_edited_preset().config;
        auto filament_config = &app_preset_bundle()->filaments.get_edited_preset().config;
        auto printer_config = &app_preset_bundle()->printers.get_edited_preset().config;
        
        // Check if configuration is valid
        if (!print_config || !filament_config || !printer_config) {
            wxMessageBox(_L("Failed to get configuration from preset bundle."),
                        _L("Configuration Error"), wxOK | wxICON_ERROR);
            return;
        }

        // ========== Prepare fullcontrol parameters ==========
        fullcontrol::MultiNozzleParams fc_params;
        
        // Get rectangle parameters from dialog
        fc_params.rect_length = params.start;   // Length (default 10)
        fc_params.rect_width = params.end;      // Width (default 2)
        fc_params.rect_spacing = params.step;   // Spacing (default 20)
        
        // Get print height
        if (!params.accelerations.empty()) {
            fc_params.total_height = params.accelerations[0];
        }
        
        // Get print bed size
        const ConfigOptionPoints* bed_shape_opt = printer_config->option<ConfigOptionPoints>("printable_area");
        if (!bed_shape_opt || bed_shape_opt->values.empty()) {
            wxMessageBox(_L("Failed to get bed shape from printer configuration."),
                        _L("Configuration Error"), wxOK | wxICON_ERROR);
            return;
        }
        
        // Calculate print bed boundaries
        BoundingBoxf bed_bbox;
        for (const Vec2d& pt : bed_shape_opt->values) {
            bed_bbox.merge(pt);
            fc_params.bed_shape.emplace_back(pt.x(), pt.y());
        }
        
        fc_params.bed_width = bed_bbox.size().x();
        fc_params.bed_height = bed_bbox.size().y();
        fc_params.bed_min_x = bed_bbox.min.x();
        fc_params.bed_min_y = bed_bbox.min.y();
        
        // Get nozzle diameter
        const ConfigOptionFloats* nozzle_diameter_config = printer_config->option<ConfigOptionFloats>("nozzle_diameter");
        if (!nozzle_diameter_config || nozzle_diameter_config->values.empty()) {
            wxMessageBox(_L("Failed to get nozzle diameter from printer configuration."),
                        _L("Configuration Error"), wxOK | wxICON_ERROR);
            return;
        }
        fc_params.nozzle_diameter = nozzle_diameter_config->values[0];
        
        // Get filament diameter
        if (auto filament_diam_opt = filament_config->option<ConfigOptionFloats>("filament_diameter")) {
            if (!filament_diam_opt->values.empty()) {
                fc_params.filament_diameter = filament_diam_opt->values[0];
            }
        }
        
        // Get retraction parameters
        if (auto retract_opt = printer_config->option<ConfigOptionFloats>("retraction_length")) {
            if (!retract_opt->values.empty()) {
                fc_params.retract_length = retract_opt->values[0];
            }
        }
        if (auto retract_speed_opt = printer_config->option<ConfigOptionFloats>("retraction_speed")) {
            if (!retract_speed_opt->values.empty()) {
                fc_params.retract_speed = retract_speed_opt->values[0];  // mm/s
            }
        }
        if (auto z_hop_opt = printer_config->option<ConfigOptionFloats>("retract_lift_above")) {
            if (!z_hop_opt->values.empty() && z_hop_opt->values[0] > 0) {
                fc_params.z_hop = z_hop_opt->values[0];
            }
        }
        
        // Get temperature settings
        if (auto temp_opt = filament_config->option<ConfigOptionInts>("nozzle_temperature")) {
            if (!temp_opt->values.empty()) {
                fc_params.nozzle_temp = temp_opt->values[0];
            }
        }
        if (auto bed_temp_opt = print_config->option<ConfigOptionInts>("bed_temperature")) {
            if (!bed_temp_opt->values.empty()) {
                fc_params.bed_temp = bed_temp_opt->values[0];
            }
        }
        
        // Get filament colors
        const ConfigOptionStrings* filament_colors_opt = filament_config->option<ConfigOptionStrings>("filament_colour");
        if (filament_colors_opt && !filament_colors_opt->values.empty()) {
            for (size_t i = 0; i < 4 && i < filament_colors_opt->values.size(); ++i) {
                std::string color = filament_colors_opt->values[i];
                if (!color.empty() && color[0] != '#') {
                    color = "#" + color;
                }
                fc_params.filament_colors.push_back(color);
            }
        }
        
        fullcontrol::MultiNozzleCalibration calibrator(fc_params);
        std::string gcode = calibrator.generateGCode();
        
        BOOST_LOG_TRIVIAL(info) << "GCode generated: " << gcode.size() << " bytes";
        
        // Build gcode file path - use temp directory to match normal slicing workflow
        boost::filesystem::path temp_path = boost::filesystem::temp_directory_path();
        temp_path /= "multi_nozzle_calibration.gcode";
        std::string gcode_file_path = temp_path.string();
        
        BOOST_LOG_TRIVIAL(info) << "Gcode will be saved to: " << gcode_file_path;
        
        // Save gcode to file
        std::ofstream gcode_file(gcode_file_path, std::ios::binary);
        if (!gcode_file.is_open()) {
            wxMessageBox(_L("Failed to save GCode file."), _L("Error"), wxOK | wxICON_ERROR);
            return;
        }
        gcode_file << gcode;
        gcode_file.flush();
        gcode_file.close();
        
        // Load generated gcode to preview
        wxString wx_gcode_path = wxString::FromUTF8(gcode_file_path);
        load_gcode(wx_gcode_path);
        
        // Show success message
        int num_layers = static_cast<int>(fc_params.total_height / fc_params.layer_height);
        double quadrant_width = fc_params.bed_width / 2.0;
        double quadrant_height = fc_params.bed_height / 2.0;
        double total_width_per_quad = fc_params.rect_length + fc_params.rect_spacing + fc_params.rect_width;
        double total_height_per_quad = std::max(fc_params.rect_width, fc_params.rect_spacing + fc_params.rect_length);
    } catch (const std::exception& e) {
        wxMessageBox(wxString::Format(_L("Multi-Nozzle calibration failed: %s"), e.what()),
                    _L("Error"), wxOK | wxICON_ERROR);
    }
}


}}    // namespace Slic3r::GUI

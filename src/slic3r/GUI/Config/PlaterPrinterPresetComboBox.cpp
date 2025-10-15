#include "PlaterPrinterPresetComboBox.hpp"

#include "libslic3r/PresetBundle.hpp"

#include "slic3r/Config/AppPreset.hpp"
#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/Utils/AppWx.hpp"
#include "slic3r/Global/AppModule.hpp"
#include "slic3r/Config/ConfigUtils.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"

#include "slic3r/GUI/Tab.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/Dialog/ParamsDialog.hpp"

const size_t LABEL_ITEM_PRINTER = 1;
const size_t LABEL_ITEM_WIZARD_PRINTERS = 2;
const size_t LABEL_ITEM_WIZARD_ADD_PRINTERS = 3;

namespace Slic3r {
namespace GUI {

PlaterPrinterPresetComboBox::PlaterPrinterPresetComboBox(wxWindow *parent) 
    :ConfigComboBox(parent, wxSize(25 * app_em_unit(), 30 * app_em_unit() / 10))
{
    GetDropDown().SetUseContentWidth(true,true);
}

PlaterPrinterPresetComboBox::~PlaterPrinterPresetComboBox()
{
}

void PlaterPrinterPresetComboBox::OnSelect(wxCommandEvent &evt)
{
    PresetBundle*       preset_bundle = app_preset_bundle();
    PresetCollection*   printers_collection = &preset_bundle->printers;

    auto selected_item = evt.GetSelection();

    size_t marker = client_data(selected_item);

    if (marker == LABEL_ITEM_WIZARD_ADD_PRINTERS)
    {
        this->SetSelection(m_last_selected);
        plater_create_printer_preset();
        evt.StopPropagation();
        return;
    }

    if (marker == LABEL_ITEM_WIZARD_PRINTERS)
    {
        this->SetSelection(m_last_selected);
        run_printer_model_wizard();
        evt.StopPropagation();
        return;        
    }

    if (marker == LABEL_ITEM_PRINTER)
    {
        this->SetSelection(m_last_selected);
        evt.StopPropagation();
        return;
    }

    if (m_last_selected != selected_item || printers_collection->current_is_dirty()) 
    {
        m_last_selected = selected_item;

        std::string preset_name = preset_bundle->get_preset_name_by_alias(Preset::TYPE_PRINTER,
            Preset::remove_suffix_modified(this->GetString(m_last_selected).ToUTF8().data()));

        plater_select_printer_preset(preset_name);
    }

    evt.StopPropagation();
#ifdef __WXMSW__
    // From the Win 2004 preset combobox lose a focus after change the preset selection
    // and that is why the up/down arrow doesn't work properly
    // So, set the focus to the combobox explicitly
    this->SetFocus();
#endif
}

bool PlaterPrinterPresetComboBox::switch_to_tab()
{
    Tab* tab = AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER);
    if (!tab)
        return false;

    //BBS  Select NoteBook Tab params
    if (tab->GetParent() == AppAdapter::gui_app()->params_panel())
        AppAdapter::main_panel()->select_tab(MainPanel::tp3DEditor);
    else {
        AppAdapter::gui_app()->params_dialog()->Popup();
        tab->OnActivate();
    }
    tab->restore_last_select_item();

    return true;
}

void PlaterPrinterPresetComboBox::update()
{
    // Otherwise fill in the list from scratch.
    this->Freeze();
    this->Clear();
    invalidate_selection();

    PresetBundle*       preset_bundle = app_preset_bundle();
    PresetCollection*   printers_collection = &preset_bundle->printers;

    const Preset* selected_filament_preset = nullptr;
    std::string filament_color;

    bool has_selection = printers_collection->get_selected_idx() != size_t(-1);
    const Preset* selected_preset = has_selection ? &printers_collection->get_selected_preset() : nullptr;
    // Show wide icons if the currently selected preset is not compatible with the current printer,
    // and draw a red flag in front of the selected preset.
    bool wide_icons = selected_preset && !selected_preset->is_compatible;

    std::map<wxString, wxBitmap*> nonsys_presets;
    //BBS: add project embedded presets logic
    std::map<wxString, wxBitmap*>  project_embedded_presets;
    std::map<wxString, wxBitmap *> system_presets;
    std::map<wxString, wxString>   preset_descriptions;

    //BBS:  move system to the end
    wxString selected_system_preset;
    wxString selected_user_preset;
    wxString tooltip;

    auto get_preset_name = [](const Preset& preset) -> wxString 
    {
        return from_u8(preset.label(false));
    };

    auto get_tooltip = [](const Preset &preset) -> wxString
    {
        return from_u8(preset.name);
    };

    const std::deque<Preset>& presets = printers_collection->get_presets();

    for (size_t i = presets.front().is_visible ? 0 : printers_collection->num_default_presets(); i < presets.size(); ++i)
    {
        const Preset& preset = presets[i];
        bool is_selected =  preset_bundle->physical_printers.has_selection() ? false : i == printers_collection->get_selected_idx();

        if (!preset.is_visible || (!preset.is_compatible && !is_selected))
            continue;

        bool single_bar = false;

        wxBitmap* bmp = get_printer_preset_bmp(preset);
        assert(bmp);

        const wxString name = get_preset_name(preset);
        preset_descriptions.emplace(name, from_u8(preset.description));

        if (preset.is_default || preset.is_system) {
            //BBS: move system to the end
            system_presets.emplace(name, bmp);
            if (is_selected) {
                tooltip = get_tooltip(preset);
                selected_system_preset = name;
            }
        }
        //BBS: add project embedded preset logic
        else if (preset.is_project_embedded)
        {
            project_embedded_presets.emplace(name, bmp);
            if (is_selected) {
                selected_user_preset = name;
                tooltip = wxString::FromUTF8(preset.name.c_str());
            }
        }
        else
        {
            nonsys_presets.emplace(name, bmp);
            if (is_selected) {
                selected_user_preset = name;
                //BBS set tooltip
                tooltip = get_tooltip(preset);
            }
        }
    }

    //BBS: add project embedded preset logic
    if (!project_embedded_presets.empty())
    {
        set_label_marker(Append(separator(L("Project-inside presets")), wxNullBitmap));
        for (std::map<wxString, wxBitmap*>::iterator it = project_embedded_presets.begin(); it != project_embedded_presets.end(); ++it) {
            SetItemTooltip(Append(it->first, *it->second), preset_descriptions[it->first]);
            validate_selection(it->first == selected_user_preset);
        }
    }
    if (!nonsys_presets.empty())
    {
        set_label_marker(Append(separator(L("User presets")), wxNullBitmap));
        for (std::map<wxString, wxBitmap*>::iterator it = nonsys_presets.begin(); it != nonsys_presets.end(); ++it) {
            SetItemTooltip(Append(it->first, *it->second), preset_descriptions[it->first]);
            validate_selection(it->first == selected_user_preset);
        }
    }
    //BBS: move system to the end
    if (!system_presets.empty())
    {
        set_label_marker(Append(separator(L("System presets")), wxNullBitmap));
        for (std::map<wxString, wxBitmap*>::iterator it = system_presets.begin(); it != system_presets.end(); ++it) {
            SetItemTooltip(Append(it->first, *it->second), preset_descriptions[it->first]);
            validate_selection(it->first == selected_system_preset);
        }
    }

    wxBitmap* bmp = get_preset_bmp("edit_preset_list", wide_icons, "edit_uni");
    assert(bmp);

    set_label_marker(Append(separator(L("Select/Remove printers(system presets)")), *bmp), LABEL_ITEM_WIZARD_PRINTERS);
    set_label_marker(Append(separator(L("Create printer")), *bmp), LABEL_ITEM_WIZARD_ADD_PRINTERS);

    update_selection();
    Thaw();

    if (!tooltip.IsEmpty()) {
#ifdef __WXMSW__
        // From the Windows 2004 the tooltip for preset combobox doesn't work after next call of SetTooltip()
        // (There was an issue, when tooltip doesn't appears after changing of the preset selection)
        // But this workaround seems to work: We should to kill tooltip and than set new tooltip value
        // See, https://groups.google.com/g/wx-users/c/mOEe3fgHrzk
        SetToolTip(NULL);
#endif
        SetToolTip(tooltip);
    }

#ifdef __WXMSW__
    // Use this part of code just on Windows to avoid of some layout issues on Linux
    // Update control min size after rescale (changed Display DPI under MSW)
    if (GetMinWidth() != 10 * m_em_unit)
        SetMinSize(wxSize(10 * m_em_unit, GetSize().GetHeight()));
#endif //__WXMSW__
}

void PlaterPrinterPresetComboBox::msw_rescale()
{
    ConfigComboBox::msw_rescale();
    SetMinSize({-1, 30 * m_em_unit / 10});
}

}
}
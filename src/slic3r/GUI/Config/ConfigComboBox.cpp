#include "ConfigComboBox.hpp"
#include "slic3r/Theme/AppColor.hpp"

namespace Slic3r{
namespace GUI {

ConfigComboBox::ConfigComboBox(wxWindow* parent, const wxSize& size)
    : ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, size, 0, nullptr, wxCB_READONLY)
    , m_em_unit(em_unit(this))
    , m_last_selected(wxNOT_FOUND)
{
    Bind(wxEVT_COMBOBOX, &ConfigComboBox::OnSelect, this);
}

ConfigComboBox::~ConfigComboBox()
{

}

void ConfigComboBox::set_label_marker(int item, size_t label_item_type)
{
    this->SetClientData(item, (void*)label_item_type);
}

size_t ConfigComboBox::client_data(int item)
{
    size_t marker_ptr = reinterpret_cast<size_t>(this->GetClientData(item));
    return marker_ptr;
}

void ConfigComboBox::invalidate_selection()
{
    m_last_selected = INT_MAX; // this value means that no one item is selected
}

void ConfigComboBox::validate_selection(bool predicate/*=false*/)
{
    // just in case: mark m_last_selected as a first added element
    if (predicate || m_last_selected == INT_MAX)
        m_last_selected = GetCount() - 1;
}

void ConfigComboBox::update_selection()
{
    /* If selected_preset_item is still equal to INT_MAX, it means that
     * there is no presets added to the list.
     * So, select last combobox item ("Add/Remove preset")
     */
    validate_selection();

    SetSelection(m_last_selected);
#ifdef __WXMSW__
    // From the Windows 2004 the tooltip for preset combobox doesn't work after next call of SetTooltip()
    // (There was an issue, when tooltip doesn't appears after changing of the preset selection)
    // But this workaround seems to work: We should to kill tooltip and than set new tooltip value
    SetToolTip(NULL);
#endif
    SetToolTip(GetString(m_last_selected));

// A workaround for a set of issues related to text fitting into gtk widgets:
#if defined(__WXGTK20__) || defined(__WXGTK3__)
    GList* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(m_widget));

    // 'cells' contains the GtkCellRendererPixBuf for the icon,
    // 'cells->next' contains GtkCellRendererText for the text we need to ellipsize
    if (!cells || !cells->next) return;

    auto cell = static_cast<GtkCellRendererText *>(cells->next->data);

    if (!cell) return;

    g_object_set(G_OBJECT(cell), "ellipsize", PANGO_ELLIPSIZE_END, (char*)NULL);

    // Only the list of cells must be freed, the renderer isn't ours to free
    g_list_free(cells);
#endif
}

void ConfigComboBox::update()
{
    // this->update(into_u8(this->GetString(this->GetSelection())));
}

void ConfigComboBox::msw_rescale()
{
    m_em_unit = em_unit(this);
    // Rescale();

    // m_bitmapIncompatible.msw_rescale();
    // m_bitmapCompatible.msw_rescale();

    // // parameters for an icon's drawing
    // fill_width_height();

    // // update the control to redraw the icons
    // update();
}

void ConfigComboBox::sys_color_changed()
{
    UpdateDarkUI(this);
    msw_rescale();
}

void ConfigComboBox::OnSelect(wxCommandEvent& evt)
{
    // Under OSX: in case of use of a same names written in different case (like "ENDER" and "Ender")
    // m_presets_choice->GetSelection() will return first item, because search in PopupListCtrl is case-insensitive.
    // So, use GetSelection() from event parameter
    auto selected_item = evt.GetSelection();

    // auto marker = reinterpret_cast<Marker>(this->GetClientData(selected_item));
    // if (marker >= LABEL_ITEM_DISABLED && marker < LABEL_ITEM_MAX)
    //     this->SetSelection(m_last_selected);
    // else if (on_selection_changed && (m_last_selected != selected_item || m_collection->current_is_dirty())) {
    //     m_last_selected = selected_item;
    //     on_selection_changed(selected_item);
    //     evt.StopPropagation();
    // }
    evt.Skip();
}

}
}
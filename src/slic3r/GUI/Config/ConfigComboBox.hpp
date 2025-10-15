#pragma once
#include "slic3r/GUI/Widgets/ComboBox.hpp"

namespace Slic3r {
namespace GUI {

class ConfigComboBox : public ComboBox
{
public:
    ConfigComboBox(wxWindow* parent, const wxSize& size);
    virtual ~ConfigComboBox();

    virtual void update();
    virtual void msw_rescale();    
    virtual void sys_color_changed();
    virtual void OnSelect(wxCommandEvent& evt);

    void set_label_marker(int item, size_t label_item_type = 1);  // must start from 1

    void invalidate_selection();
    void validate_selection(bool predicate = false);
    void update_selection();
protected:
    size_t client_data(int item);
protected:
    typedef std::size_t Marker;

    int m_em_unit;
    int m_last_selected;
};

}
}
#pragma once 
#include "slic3r/GUI/Config/ConfigComboBox.hpp"

namespace Slic3r {
namespace GUI {

class PlaterPrinterPresetComboBox : public ConfigComboBox
{
public:
    PlaterPrinterPresetComboBox(wxWindow *parent);
    ~PlaterPrinterPresetComboBox();

    bool switch_to_tab();

    void update() override;
    void msw_rescale() override;
    void OnSelect(wxCommandEvent& evt) override;
};

}
}
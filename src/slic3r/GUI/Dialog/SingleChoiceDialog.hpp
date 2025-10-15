#ifndef slic3r_GUI_SingleChoice_hpp_
#define slic3r_GUI_SingleChoice_hpp_

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"

namespace Slic3r { namespace GUI {

class SingleChoiceDialog : public DPIDialog
{
public:
    SingleChoiceDialog(const wxString &message, const wxString &caption, const wxArrayString &choices, int initialSelectionwx, wxWindow *parent = nullptr);
    ~SingleChoiceDialog();

    int       GetSingleChoiceIndex();
    ComboBox *GetTypeComboBox() { return type_comboBox; };

    void on_dpi_changed(const wxRect &suggested_rect) override;

protected:
    ComboBox *type_comboBox   = nullptr;
    Button *  m_button_ok     = nullptr;
    Button *  m_button_cancel = nullptr;
};

}} // namespace Slic3r::GUI

#endif
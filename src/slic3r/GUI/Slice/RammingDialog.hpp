#ifndef _WIPE_TOWER_DIALOG_H_
#define _WIPE_TOWER_DIALOG_H_

namespace Slic3r {
namespace GUI {

class Button;
class Label;
class RammingChart;
class RammingPanel : public wxPanel {
public:
    RammingPanel(wxWindow* parent);
    RammingPanel(wxWindow* parent,const std::string& data);
    std::string get_parameters();

private:
    RammingChart* m_chart = nullptr;
    wxSpinCtrl* m_widget_volume = nullptr;
    wxSpinCtrl* m_widget_ramming_line_width_multiplicator = nullptr;
    wxSpinCtrl* m_widget_ramming_step_multiplicator = nullptr;
    wxSpinCtrlDouble* m_widget_time = nullptr;
    int m_ramming_step_multiplicator;
    int m_ramming_line_width_multiplicator;
      
    void line_parameters_changed();
};


class RammingDialog : public wxDialog {
public:
    RammingDialog(wxWindow* parent,const std::string& parameters);    
    std::string get_parameters() { return m_output_data; }
private:
    RammingPanel* m_panel_ramming = nullptr;
    std::string m_output_data;
};

}
}
#endif  // _WIPE_TOWER_DIALOG_H_

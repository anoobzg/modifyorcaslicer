#pragma once 
#include "slic3r/GUI/GUI_Utils.hpp"

namespace Slic3r {
namespace GUI {

enum class LoadType : unsigned char
{
    Unknown,
    OpenProject,
    LoadGeometry,
    LoadConfig
};

class RadioBox;
class StaticBox;
class Button;
class ProjectDropRadioSelector
{
public:
    int       m_select_id;
    int       m_groupid;
    RadioBox *m_radiobox;
};
WX_DECLARE_LIST(ProjectDropRadioSelector, ProjectDropRadioSelectorList);

class ProjectDropDialog : public DPIDialog
{
private:
    wxColour          m_def_color = wxColour(255, 255, 255);
    ProjectDropRadioSelectorList m_radio_group;
    int               m_action{1};
    bool              m_remember_choice{false};

public:
    ProjectDropDialog(const std::string &filename, wxWindow * window = nullptr);

    wxPanel *     m_top_line;
    wxStaticText *m_fname_title;
    wxStaticText *m_fname_f;
    wxStaticText *m_fname_s;
    StaticBox * m_panel_select;
    Button *    m_confirm;
    Button *    m_cancel;


    void      select_radio(int index);
    void      on_select_radio(wxMouseEvent &event);
    void      on_select_ok(wxMouseEvent &event);
    void      on_select_cancel(wxMouseEvent &event);

    int       get_select_radio(int groupid);
    int       get_action() const { return m_action; }
    void      set_action(int index) { m_action = index; }

    wxBoxSizer *create_remember_checkbox(wxString title, wxWindow* parent, wxString tooltip);
    wxBoxSizer *create_item_radiobox(wxString title, wxWindow *parent, int select_id, int groupid);

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
};

}
}
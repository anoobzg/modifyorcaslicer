#include "ProjectDropDialog.hpp"

#include "libslic3r/FileSystem/DataDir.hpp"

#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/Theme/AppColor.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/GUI.hpp"

#include "slic3r/GUI/Widgets/StaticBox.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/RadioBox.hpp"
#include "slic3r/GUI/Widgets/CheckBox.hpp"

#include <wx/listimpl.cpp>

#define PROJECT_DROP_DIALOG_SELECT_PLANE_SIZE wxSize(FromDIP(350), FromDIP(120))
#define PROJECT_DROP_DIALOG_BUTTON_SIZE wxSize(FromDIP(60), FromDIP(24))

namespace Slic3r {
namespace GUI {

WX_DEFINE_LIST(ProjectDropRadioSelectorList);

ProjectDropDialog::ProjectDropDialog(const std::string &filename, wxWindow * window)
    : DPIDialog(window,
                wxID_ANY,
                from_u8((boost::format(_utf8(L("Drop project file")))).str()),
                wxDefaultPosition,
                wxDefaultSize,
                wxCAPTION | wxCLOSE_BOX)
{
    // def setting
    SetBackgroundColour(m_def_color);

    // icon
    std::string icon_path = (boost::format("%1%/images/OrcaSlicerTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    wxBoxSizer *m_sizer_main = new wxBoxSizer(wxVERTICAL);

    m_top_line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_top_line->SetBackgroundColour(wxColour(166, 169, 170));

    m_sizer_main->Add(m_top_line, 0, wxEXPAND, 0);

    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, 20);

    wxBoxSizer *m_sizer_name = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *m_sizer_fline = new wxBoxSizer(wxHORIZONTAL);

    m_fname_title = new wxStaticText(this, wxID_ANY, _L("Please select an action"), wxDefaultPosition, wxDefaultSize, 0);
    m_fname_title->Wrap(-1);
    m_fname_title->SetFont(Font::Body_13);
    m_fname_title->SetForegroundColour(wxColour(107, 107, 107));
    m_fname_title->SetBackgroundColour(wxColour(255, 255, 255));

    m_sizer_fline->Add(m_fname_title, 0, wxALL, 0);
    m_sizer_fline->Add(0, 0, 0, wxEXPAND | wxLEFT, 5);

    m_fname_f = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    m_fname_f->SetFont(Font::Head_13);
    m_fname_f->Wrap(-1);
    m_fname_f->SetForegroundColour(wxColour(38, 46, 48));

    m_sizer_fline->Add(m_fname_f, 1, wxALL, 0);

    m_sizer_name->Add(m_sizer_fline, 1, wxEXPAND, 0);

    m_fname_s = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    m_fname_s->SetFont(Font::Head_13);
    m_fname_s->Wrap(-1);
    m_fname_s->SetForegroundColour(wxColour(38, 46, 48));

    m_sizer_name->Add(m_fname_s, 1, wxALL, 0);

    m_sizer_main->Add(m_sizer_name, 1, wxEXPAND | wxLEFT | wxRIGHT, 40);

    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, 5);

    m_panel_select = new StaticBox(this, wxID_ANY, wxDefaultPosition, PROJECT_DROP_DIALOG_SELECT_PLANE_SIZE);
    StateColor box_colour(std::pair<wxColour, int>(wxColour("#F8F8F8"), StateColor::Normal));
    StateColor box_border_colour(std::pair<wxColour, int>(wxColour(*wxWHITE), StateColor::Normal));

    m_panel_select->SetBackgroundColor(box_colour);
    m_panel_select->SetBorderColor(box_border_colour);
    m_panel_select->SetCornerRadius(5);

    wxBoxSizer *m_sizer_select_h = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer *m_sizer_select_v = new wxBoxSizer(wxVERTICAL);


    auto select_f = create_item_radiobox(_L("Open as project"), m_panel_select, 1, 0);
    auto select_s = create_item_radiobox(_L("Import geometry only"), m_panel_select, 2, 0);
    //auto select_t = create_item_radiobox(_L("Import presets only"), m_panel_select,3, 0);

    m_sizer_select_v->Add(select_f, 0, wxEXPAND, 5);
    m_sizer_select_v->Add(select_s, 0, wxEXPAND, 5);
    //m_sizer_select_v->Add(select_t, 0, wxEXPAND, 5);
    select_radio(2);

    m_sizer_select_h->Add(m_sizer_select_v, 0, wxALIGN_CENTER | wxLEFT, 22);

    m_panel_select->SetSizer(m_sizer_select_h);
    m_panel_select->Layout();
    m_sizer_main->Add(m_panel_select, 0, wxEXPAND | wxLEFT | wxRIGHT, 40);

    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, 10);

    wxBoxSizer *m_sizer_bottom = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *m_sizer_left = new wxBoxSizer(wxHORIZONTAL);

    auto dont_show_again = create_remember_checkbox(_L("Remember my choice."), this, _L("This option can be changed later in preferences, under 'Load Behaviour'."));
    m_sizer_left->Add(dont_show_again, 0, wxALL, 5);

    m_sizer_bottom->Add(m_sizer_left, 0, wxEXPAND, 5);
    m_sizer_bottom->Add(0, 0, 1, wxEXPAND, 5);

    wxBoxSizer *m_sizer_right  = new wxBoxSizer(wxHORIZONTAL);

    m_confirm = new Button(this, _L("OK"));
    StateColor btn_bg_green(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed), std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                            std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));

    m_confirm->SetBackgroundColor(btn_bg_green);
    m_confirm->SetBorderColor(wxColour(0, 150, 136));
    m_confirm->SetTextColor(wxColour("#FFFFFE"));
    m_confirm->SetSize(PROJECT_DROP_DIALOG_BUTTON_SIZE);
    m_confirm->SetMinSize(PROJECT_DROP_DIALOG_BUTTON_SIZE);
    m_confirm->SetCornerRadius(FromDIP(12));
    m_confirm->Bind(wxEVT_LEFT_DOWN, &ProjectDropDialog::on_select_ok, this);
    m_sizer_right->Add(m_confirm, 0, wxALL, 5);

    m_cancel = new Button(this, _L("Cancel"));
    m_cancel->SetTextColor(wxColour(107, 107, 107));
    m_cancel->SetSize(PROJECT_DROP_DIALOG_BUTTON_SIZE);
    m_cancel->SetMinSize(PROJECT_DROP_DIALOG_BUTTON_SIZE);
    m_cancel->SetCornerRadius(FromDIP(12));
    m_cancel->Bind(wxEVT_LEFT_DOWN, &ProjectDropDialog::on_select_cancel, this);
    m_sizer_right->Add(m_cancel, 0, wxALL, 5);

    m_sizer_bottom->Add( m_sizer_right, 0, wxEXPAND, 5 );
    m_sizer_main->Add(m_sizer_bottom, 0, wxEXPAND | wxLEFT | wxRIGHT, 40);
    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, 20);

    SetSizer(m_sizer_main);
    Layout();
    Fit();
    Centre(wxBOTH);


    auto limit_width   = m_fname_f->GetSize().GetWidth() - 2;
    auto current_width = 0;
    auto cut_index     = 0;
    auto fstring       = wxString("");
    auto bstring       = wxString("");

    //auto file_name = from_u8(filename.c_str());
    auto file_name = wxString(filename);
    for (int x = 0; x < file_name.length(); x++) {
        current_width += m_fname_s->GetTextExtent(file_name[x]).GetWidth();
        cut_index = x;

        if (current_width > limit_width) {
            bstring += file_name[x];
        } else {
            fstring += file_name[x];
        }
    }

    m_fname_f->SetLabel(fstring);
    m_fname_s->SetLabel(bstring);

    UpdateDlgDarkUI(this);
}

wxBoxSizer *ProjectDropDialog ::create_item_radiobox(wxString title, wxWindow *parent, int select_id, int groupid)
{
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    auto radiobox =  new RadioBox(parent);

    radiobox->SetBackgroundColour(wxColour(248,248,248));
    sizer->Add(radiobox, 0, wxALL, 5);
    sizer->Add(0, 0, 0, wxEXPAND | wxLEFT, 5);
    auto text = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, 0);
    text->Wrap(-1);
    text->SetForegroundColour(wxColour(107, 107, 107));
    text->SetBackgroundColour(wxColour(248,248,248));
    sizer->Add(text, 0, wxALL, 5);

    radiobox->Bind(wxEVT_LEFT_DOWN, &ProjectDropDialog::on_select_radio, this);
    text->Bind(wxEVT_LEFT_DOWN, [this, radiobox](auto &e) {
        e.SetId(radiobox->GetId());
        on_select_radio(e);
    });

    ProjectDropRadioSelector *rs = new ProjectDropRadioSelector;
    rs->m_groupid     = groupid;
    rs->m_radiobox    = radiobox;
    rs->m_select_id   = select_id;
    m_radio_group.Append(rs);

    return sizer;
}
wxBoxSizer *ProjectDropDialog::create_remember_checkbox(wxString title, wxWindow *parent, wxString tooltip)
{
    wxBoxSizer *m_sizer_checkbox = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_checkbox->Add(0, 0, 0, wxEXPAND | wxLEFT, 5);

    auto checkbox = new CheckUIBox(parent);
    checkbox->SetValue(m_remember_choice);
    checkbox->SetToolTip(tooltip);
    m_sizer_checkbox->Add(checkbox, 0, wxALIGN_CENTER, 0);
    m_sizer_checkbox->Add(0, 0, 0, wxEXPAND | wxLEFT, 8);

    auto checkbox_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxSize(-1, -1), 0);
    checkbox_title->SetForegroundColour(wxColour(144,144,144));
    checkbox_title->SetFont(Font::Body_13);
    checkbox_title->Wrap(-1);
    checkbox_title->SetToolTip(tooltip);
    m_sizer_checkbox->Add(checkbox_title, 0, wxALIGN_CENTER | wxALL, 3);

    checkbox->Bind(wxEVT_TOGGLEBUTTON, [this, checkbox](wxCommandEvent &e) {
        m_remember_choice = checkbox->GetValue();
        e.Skip();
    });

    return m_sizer_checkbox;
}

void ProjectDropDialog::select_radio(int index)
{
    m_action                         = index;
    ProjectDropRadioSelectorList::compatibility_iterator it = m_radio_group.GetFirst();
    auto                     groupid = 0;

    while (it) {
        ProjectDropRadioSelector *rs = it->GetData();
        if (rs->m_select_id == index) groupid = rs->m_groupid;
        it = it->GetNext();
    }

    it = m_radio_group.GetFirst();
    while (it) {
        ProjectDropRadioSelector *rs = it->GetData();
        if (rs->m_groupid == groupid && rs->m_select_id == index) rs->m_radiobox->SetValue(true);
        if (rs->m_groupid == groupid && rs->m_select_id != index) rs->m_radiobox->SetValue(false);
        it = it->GetNext();
    }
}

int ProjectDropDialog::get_select_radio(int groupid)
{
    ProjectDropRadioSelectorList::compatibility_iterator it = m_radio_group.GetFirst();
    while (it) {
        ProjectDropRadioSelector *rs = it->GetData();
        if (rs->m_groupid == groupid && rs->m_radiobox->GetValue()) { return rs->m_select_id; }
        it = it->GetNext();
    }

    return 0;
}
void ProjectDropDialog::on_select_radio(wxMouseEvent &event)
{
    ProjectDropRadioSelectorList::compatibility_iterator it = m_radio_group.GetFirst();
    auto                     groupid = 0;

    while (it) {
        ProjectDropRadioSelector *rs = it->GetData();
        if (rs->m_radiobox->GetId() == event.GetId()) groupid = rs->m_groupid;
        it = it->GetNext();
    }

    it = m_radio_group.GetFirst();
    while (it) {
        ProjectDropRadioSelector *rs = it->GetData();
        if (rs->m_groupid == groupid && rs->m_radiobox->GetId() == event.GetId()) {
            set_action(rs->m_select_id);
            rs->m_radiobox->SetValue(true);
        }


        if (rs->m_groupid == groupid && rs->m_radiobox->GetId() != event.GetId()) rs->m_radiobox->SetValue(false);
        it = it->GetNext();
    }
}

void ProjectDropDialog::on_select_ok(wxMouseEvent &event)
{
    if (m_remember_choice) {
        LoadType load_type = static_cast<LoadType>(get_action());
        switch (load_type)
        {
            case LoadType::OpenProject:
                app_set_load_behaviour_load_all();
                break;
            case LoadType::LoadGeometry:
                app_set_load_behaviour_load_geometry();
                break;
        }
    }

    EndModal(wxID_OK);
}

void ProjectDropDialog::on_select_cancel(wxMouseEvent &event)
{
    EndModal(wxID_CANCEL);
}

void ProjectDropDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    m_confirm->SetMinSize(PROJECT_DROP_DIALOG_BUTTON_SIZE);
    m_cancel->SetMinSize(PROJECT_DROP_DIALOG_BUTTON_SIZE);
    Fit();
    Refresh();
}

}
}
#include "RammingDialog.hpp"

#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/Theme/AppColor.hpp"

// #include "slic3r/Theme/BitmapCache.hpp"
// #include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Dialog/MsgDialog.hpp"

#include "RammingChart.hpp"
// #include "libslic3r/Color.hpp"
// #include "slic3r/GUI/Widgets/Button.hpp"
// #include "slic3r/Theme/ColorSpaceConvert.hpp"
// #include "slic3r/GUI/MainPanel.hpp"
// #include "slic3r/Theme/AppColor.hpp"
// #include "libslic3r/Config.hpp"
// #include "libslic3r/FileSystem/DataDir.hpp"
// #include "slic3r/Config/AppPreset.hpp"
namespace Slic3r {
namespace GUI {

int scale(const int val) { return val * app_em_unit() / 10; }
int ITEM_WIDTH() { return scale(30); }
static const wxColour g_text_color = wxColour(107, 107, 107, 255);


static void update_ui(wxWindow* window)
{
    Slic3r::GUI::UpdateDarkUI(window);
}

RammingDialog::RammingDialog(wxWindow* parent,const std::string& parameters)
: wxDialog(parent, wxID_ANY, _(L("Ramming customization")), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE/* | wxRESIZE_BORDER*/)
{
    update_ui(this);
    m_panel_ramming  = new RammingPanel(this,parameters);

    // Not found another way of getting the background colours of RammingDialog, RammingPanel and Chart correct than setting
    // them all explicitely. Reading the parent colour yielded colour that didn't really match it, no wxSYS_COLOUR_... matched
    // colour used for the dialog. Same issue (and "solution") here : https://forums.wxwidgets.org/viewtopic.php?f=1&t=39608
    // Whoever can fix this, feel free to do so.
#ifndef _WIN32
    this->           SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));
    m_panel_ramming->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));
#endif
    m_panel_ramming->Show(true);
    this->Show();

    auto main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(m_panel_ramming, 1, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 5);
    main_sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, 10);
    SetSizer(main_sizer);
    main_sizer->SetSizeHints(this);

    update_ui(static_cast<wxButton*>(this->FindWindowById(wxID_OK, this)));
    update_ui(static_cast<wxButton*>(this->FindWindowById(wxID_CANCEL, this)));

    this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) { EndModal(wxCANCEL); });

    this->Bind(wxEVT_BUTTON,[this](wxCommandEvent&) {
        m_output_data = m_panel_ramming->get_parameters();
        EndModal(wxID_OK);
        },wxID_OK);
    this->Show();
//    wxMessageDialog dlg(this, _(L("Ramming denotes the rapid extrusion just before a tool change in a single-extruder MM printer. Its purpose is to "
    Slic3r::GUI::MessageDialog dlg(this, _(L("Ramming denotes the rapid extrusion just before a tool change in a single-extruder MM printer. Its purpose is to "
        "properly shape the end of the unloaded filament so it does not prevent insertion of the new filament and can itself "
        "be reinserted later. This phase is important and different materials can require different extrusion speeds to get "
        "the good shape. For this reason, the extrusion rates during ramming are adjustable.\n\nThis is an expert-level "
        "setting, incorrect adjustment will likely lead to jams, extruder wheel grinding into filament etc.")), _(L("Warning")), wxOK | wxICON_EXCLAMATION);// .ShowModal();
    dlg.ShowModal();
}


#ifdef _WIN32
#define style wxSP_ARROW_KEYS | wxBORDER_SIMPLE
#else 
#define style wxSP_ARROW_KEYS
#endif



RammingPanel::RammingPanel(wxWindow* parent, const std::string& parameters)
: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize/*,wxPoint(50,50), wxSize(800,350),wxBORDER_RAISED*/)
{
    update_ui(this);
	auto sizer_chart = new wxBoxSizer(wxVERTICAL);
	auto sizer_param = new wxBoxSizer(wxVERTICAL);

	std::stringstream stream{ parameters };
	stream >> m_ramming_line_width_multiplicator >> m_ramming_step_multiplicator;
	int ramming_speed_size = 0;
	float dummy = 0.f;
	while (stream >> dummy)
		++ramming_speed_size;
	stream.clear();
	stream.get();

	std::vector<std::pair<float, float>> buttons;
	float x = 0.f;
	float y = 0.f;
	while (stream >> x >> y)
		buttons.push_back(std::make_pair(x, y));

	m_chart = new RammingChart(this, wxRect(scale(10),scale(10),scale(480),scale(360)), buttons, ramming_speed_size, 0.25f, scale(10));
#ifdef _WIN32
    update_ui(m_chart);
#else
    m_chart->SetBackgroundColour(parent->GetBackgroundColour()); // see comment in RammingDialog constructor
#endif
 	sizer_chart->Add(m_chart, 0, wxALL, 5);

    m_widget_time						= new wxSpinCtrlDouble(this,wxID_ANY,wxEmptyString,wxDefaultPosition,wxSize(ITEM_WIDTH()*2.5, -1),style,0.,5.0,3.,0.5);        
    m_widget_volume							  = new wxSpinCtrl(this,wxID_ANY,wxEmptyString,wxDefaultPosition,wxSize(ITEM_WIDTH()*2.5, -1),style,0,10000,0);        
    m_widget_ramming_line_width_multiplicator = new wxSpinCtrl(this,wxID_ANY,wxEmptyString,wxDefaultPosition,wxSize(ITEM_WIDTH()*2.5, -1),style,10,200,100);        
    m_widget_ramming_step_multiplicator		  = new wxSpinCtrl(this,wxID_ANY,wxEmptyString,wxDefaultPosition,wxSize(ITEM_WIDTH()*2.5, -1),style,10,200,100);

#ifdef _WIN32
    update_ui(m_widget_time->GetText());
    update_ui(m_widget_volume);
    update_ui(m_widget_ramming_line_width_multiplicator);
    update_ui(m_widget_ramming_step_multiplicator);
#endif

	auto gsizer_param = new wxFlexGridSizer(2, 5, 15);
	gsizer_param->Add(new wxStaticText(this, wxID_ANY, wxString(_(L("Total ramming time")) + " (" + _(L("s")) + "):")), 0, wxALIGN_CENTER_VERTICAL);
	gsizer_param->Add(m_widget_time);
	gsizer_param->Add(new wxStaticText(this, wxID_ANY, wxString(_(L("Total rammed volume")) + " (" + _(L("mm")) + wxString("³):", wxConvUTF8))), 0, wxALIGN_CENTER_VERTICAL);
	gsizer_param->Add(m_widget_volume);
	gsizer_param->AddSpacer(20);
	gsizer_param->AddSpacer(20);
	gsizer_param->Add(new wxStaticText(this, wxID_ANY, wxString(_(L("Ramming line width")) + " (%):")), 0, wxALIGN_CENTER_VERTICAL);
	gsizer_param->Add(m_widget_ramming_line_width_multiplicator);
	gsizer_param->Add(new wxStaticText(this, wxID_ANY, wxString(_(L("Ramming line spacing")) + " (%):")), 0, wxALIGN_CENTER_VERTICAL);
	gsizer_param->Add(m_widget_ramming_step_multiplicator);

	sizer_param->Add(gsizer_param, 0, wxTOP, scale(10));

    m_widget_time->SetValue(m_chart->get_time());
    m_widget_time->SetDigits(2);
    m_widget_volume->SetValue(m_chart->get_volume());
    m_widget_volume->Disable();
    m_widget_ramming_line_width_multiplicator->SetValue(m_ramming_line_width_multiplicator);
    m_widget_ramming_step_multiplicator->SetValue(m_ramming_step_multiplicator);        
    
    m_widget_ramming_step_multiplicator->Bind(wxEVT_TEXT,[this](wxCommandEvent&) { line_parameters_changed(); });
    m_widget_ramming_line_width_multiplicator->Bind(wxEVT_TEXT,[this](wxCommandEvent&) { line_parameters_changed(); });

	auto sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(sizer_chart, 0, wxALL, 5);
	sizer->Add(sizer_param, 0, wxALL, 10);

	sizer->SetSizeHints(this);
	SetSizer(sizer);

    m_widget_time->Bind(wxEVT_TEXT,[this](wxCommandEvent&) {m_chart->set_xy_range(m_widget_time->GetValue(),-1);});
    m_widget_time->Bind(wxEVT_CHAR,[](wxKeyEvent&){});      // do nothing - prevents the user to change the value
    m_widget_volume->Bind(wxEVT_CHAR,[](wxKeyEvent&){});    // do nothing - prevents the user to change the value   
    Bind(EVT_WIPE_TOWER_RammingChart_CHANGED, [this](wxCommandEvent&) {m_widget_volume->SetValue(m_chart->get_volume()); m_widget_time->SetValue(m_chart->get_time());} );
    Refresh(true); // erase background
}

void RammingPanel::line_parameters_changed() {
    m_ramming_line_width_multiplicator = m_widget_ramming_line_width_multiplicator->GetValue();
    m_ramming_step_multiplicator = m_widget_ramming_step_multiplicator->GetValue();
}

std::string RammingPanel::get_parameters()
{
    std::vector<float> speeds = m_chart->get_ramming_speed(0.25f);
    std::vector<std::pair<float,float>> buttons = m_chart->get_buttons();
    std::stringstream stream;
    stream << m_ramming_line_width_multiplicator << " " << m_ramming_step_multiplicator;
    for (const float& speed_value : speeds)
        stream << " " << speed_value;
    stream << "|";    
    for (const auto& button : buttons)
        stream << " " << button.first << " " << button.second;
    return stream.str();
}

}
}
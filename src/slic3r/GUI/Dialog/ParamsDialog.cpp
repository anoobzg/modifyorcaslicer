#include "ParamsDialog.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/Config/ParamsPanel.hpp"
#include "slic3r/GUI/AppAdapter.hpp"
#include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/Tab.hpp"
#include "slic3r/Theme/AppColor.hpp"
#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/Event/UserPlaterEvent.hpp"
#include "slic3r/GUI/Frame/Plater.hpp"

namespace pt = boost::property_tree;
typedef pt::ptree JSON;

namespace Slic3r { 
namespace GUI {


ParamsDialog::ParamsDialog(wxWindow * parent)
	: DPIDialog(parent, wxID_ANY,  "", wxDefaultPosition,
		wxDefaultSize, wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER)
{
	m_panel = new ParamsPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT | wxTAB_TRAVERSAL);
	auto* topsizer = new wxBoxSizer(wxVERTICAL);
	topsizer->Add(m_panel, 1, wxALL | wxEXPAND, 0, NULL);

	SetSizerAndFit(topsizer);
	SetSize({75 * em_unit(), 60 * em_unit()});

	Layout();
	Center();
    Bind(wxEVT_SHOW, [this](auto &event) {
        if (IsShown()) {
            m_winDisabler = new wxWindowDisabler(this);
        } else {
            delete m_winDisabler;
            m_winDisabler = nullptr;
        }
    });
	Bind(wxEVT_CLOSE_WINDOW, [this](auto& event) {

        Hide();
        if (!m_editing_filament_id.empty()) {
            Filamentinformation *filament_info = new Filamentinformation();
            filament_info->filament_id        = m_editing_filament_id;
            wxQueueEvent(AppAdapter::plater(), new SimpleEvent(EVT_MODIFY_FILAMENT, filament_info));
            m_editing_filament_id.clear();
        }

        AppAdapter::gui_app()->sidebar().finish_param_edit();
    });

    //UpdateDlgDarkUI(this);
}

void ParamsDialog::Popup()
{
    UpdateDlgDarkUI(this);
#ifdef __WIN32__
    Reparent(AppAdapter::main_panel());
#endif
    Center();
    if (m_panel && m_panel->get_current_tab()) {
        bool just_edit = false;
        if (!m_editing_filament_id.empty()) just_edit = true;
        dynamic_cast<Tab *>(m_panel->get_current_tab())->set_just_edit(just_edit);
    }
    Show();
}

void ParamsDialog::on_dpi_changed(const wxRect &suggested_rect)
{
	Fit();
	SetSize({75 * em_unit(), 60 * em_unit()});
	m_panel->msw_rescale();
	Refresh();
}

} // namespace GUI
} // namespace Slic3r

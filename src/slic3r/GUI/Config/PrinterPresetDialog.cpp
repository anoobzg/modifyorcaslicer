#include "PrinterPresetDialog.hpp"

#include "slic3r/Theme/AppColor.hpp"

#include "slic3r/GUI/I18N.hpp"

#include "slic3r/GUI/AppAdapter.hpp"
// #include "slic3r/GUI/MainPanel.hpp"
#include "slic3r/GUI/Tab.hpp"
// #include "libslic3r/Utils.hpp"
// #include "slic3r/GUI/Event/UserPlaterEvent.hpp"
// #include "slic3r/GUI/Frame/Plater.hpp"

namespace pt = boost::property_tree;
typedef pt::ptree JSON;

namespace Slic3r { 
namespace GUI {

PrinterPresetPanel::PrinterPresetPanel( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name )
    : wxPanel( parent, id, pos, size, style, name )
{
    // BBS: new layout
    SetBackgroundColour(*wxWHITE);
#if __WXOSX__
    m_top_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_top_sizer->SetSizeHints(this);
    this->SetSizer(m_top_sizer);

    // Create additional panel to Fit() it from OnActivate()
    // It's needed for tooltip showing on OSX
    m_tmp_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT | wxTAB_TRAVERSAL);
    auto  sizer = new wxBoxSizer(wxHORIZONTAL);
    m_tmp_panel->SetSizer(sizer);
    m_tmp_panel->Layout();

#else
    m_top_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_top_sizer->SetSizeHints(this);
    this->SetSizer(m_top_sizer);
#endif //__WXOSX__

    // Initialize the page.
#if __WXOSX__
    wxPanel* page_parent = m_tmp_panel;
#else
    wxPanel* page_parent = this;
#endif

    // BBS: fix scroll to tip view
    class PageScrolledWindow : public wxScrolledWindow
    {
    public:
        PageScrolledWindow(wxWindow *parent)
            : wxScrolledWindow(parent,
                               wxID_ANY,
                               wxDefaultPosition,
                               wxDefaultSize,
                               wxVSCROLL) // hide hori-bar will cause hidden field mis-position
        {
            // ShowScrollBar(GetHandle(), SB_BOTH, FALSE);
            Bind(wxEVT_SCROLL_CHANGED, [this](auto &e) {
                wxWindow *child = dynamic_cast<wxWindow *>(e.GetEventObject());
                if (child != this)
                    EnsureVisible(child);
            });
        }
        virtual bool ShouldScrollToChildOnFocus(wxWindow *child)
        {
            EnsureVisible(child);
            return false;
        }
        void EnsureVisible(wxWindow* win)
        {
            const wxRect viewRect(m_targetWindow->GetClientRect());
            const wxRect winRect(m_targetWindow->ScreenToClient(win->GetScreenPosition()), win->GetSize());
            if (viewRect.Contains(winRect)) {
                return;
            }
            if (winRect.GetWidth() > viewRect.GetWidth() || winRect.GetHeight() > viewRect.GetHeight()) {
                return;
            }
            int stepx, stepy;
            GetScrollPixelsPerUnit(&stepx, &stepy);

            int startx, starty;
            GetViewStart(&startx, &starty);
            // first in vertical direction:
            if (stepy > 0) {
                int diff = 0;

                if (winRect.GetTop() < 0) {
                    diff = winRect.GetTop();
                } else if (winRect.GetBottom() > viewRect.GetHeight()) {
                    diff = winRect.GetBottom() - viewRect.GetHeight() + 1;
                    // round up to next scroll step if we can't get exact position,
                    // so that the window is fully visible:
                    diff += stepy - 1;
                }
                starty = (starty * stepy + diff) / stepy;
            }
            // then horizontal:
            if (stepx > 0) {
                int diff = 0;
                if (winRect.GetLeft() < 0) {
                    diff = winRect.GetLeft();
                } else if (winRect.GetRight() > viewRect.GetWidth()) {
                    diff = winRect.GetRight() - viewRect.GetWidth() + 1;
                    // round up to next scroll step if we can't get exact position,
                    // so that the window is fully visible:
                    diff += stepx - 1;
                }
                startx = (startx * stepx + diff) / stepx;
            }
            Scroll(startx, starty);
        }
    };

    m_page_view = new PageScrolledWindow(page_parent);
    m_page_view->SetBackgroundColour(*wxBLACK);
    m_page_sizer = new wxBoxSizer(wxVERTICAL);

    m_page_view->SetSizer(m_page_sizer);
    m_page_view->SetScrollbars(1, 20, 1, 2);

    // Bind(wxEVT_TOGGLEBUTTON, &PrinterPresetPanel::OnToggled, this); // For Tab's mode switch
}

PrinterPresetPanel::~PrinterPresetPanel()
{
    // // BBS: fix double destruct of OG_CustomCtrl
    // Tab* cur_tab = dynamic_cast<Tab*> (m_current_tab);
    // if (cur_tab)
    //     cur_tab->clear_pages();
}

void PrinterPresetPanel::create_layout()
{
#ifdef __WINDOWS__
    this->SetDoubleBuffered(true);
    m_page_view->SetDoubleBuffered(true);
#endif //__WINDOWS__

    m_left_sizer = new wxBoxSizer( wxVERTICAL );
    // BBS: new layout
    m_left_sizer->SetMinSize( wxSize(40 * em_unit(this), -1 ) );

    if (m_tab_printer) {
        m_left_sizer->Add( m_tab_printer, 1, wxEXPAND );
    }

    m_top_sizer->Add(m_left_sizer, 1, wxEXPAND);

    m_left_sizer->AddSpacer(6 * em_unit(this) / 10);

#if __WXOSX__
    m_left_sizer->Add(m_tmp_panel, 1, wxEXPAND | wxALL, 0);
    m_tmp_panel->GetSizer()->Add( m_page_view, 1, wxEXPAND );
#else
    m_left_sizer->Add( m_page_view, 1, wxEXPAND );
#endif

    this->Layout();
}

void PrinterPresetPanel::rebuild_panels()
{
    refresh_tabs();

    m_tab_printer->Reparent(this);
    // m_tab_printer->OnActivate();
    // m_tab_printer->Show(true);

    free_sizers();
    create_layout();
}

void PrinterPresetPanel::refresh_tabs()
{
    m_tab_printer = static_cast<TabPrinter*>(AppAdapter::gui_app()->get_tab(Preset::TYPE_PRINTER));
}

// void PrinterPresetPanel::clear_page()
// {
//     if (m_page_sizer)
//         m_page_sizer->Clear(true);
// }


// void PrinterPresetPanel::OnActivate()
// {
//     if (m_current_tab == NULL)
//     {
//         //the first time
//         BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": first time opened, set current tab to print");
//         // BBS: open/close tab
//         //m_current_tab = m_tab_print;
//         set_active_tab(m_tab_print ? m_tab_print : m_tab_filament);
//     }
//     Tab* cur_tab = dynamic_cast<Tab *> (m_current_tab);
//     if (cur_tab)
//         cur_tab->OnActivate();
// }

// void PrinterPresetPanel::OnToggled(wxCommandEvent& event)
// {
//     if (m_mode_region && m_mode_region->GetId() == event.GetId()) {
//         wxWindowUpdateLocker locker(GetParent());
//         set_active_tab(nullptr);
//         event.Skip();
//         return;
//     }

//     if (wxID_ABOUT != event.GetId()) {
//         return;
//     }

//     // this is from tab's mode switch
//     bool value = dynamic_cast<SwitchButton*>(event.GetEventObject())->GetValue();
//     int mode_id;

//     BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": Advanced mode toogle to %1%") % value;

//     if (value)
//     {
//         //m_mode_region->SetBitmap(m_toggle_on_icon);
//         mode_id = comAdvanced;
//     }
//     else
//     {
//         //m_mode_region->SetBitmap(m_toggle_off_icon);
//         mode_id = comSimple;
//     }

//     Slic3r::GUI::AppAdapter::gui_app()->save_mode(mode_id);
// }

// // This is special, DO NOT call it from outer except from Tab
// void PrinterPresetPanel::set_active_tab(wxPanel* tab)
// {
//     Tab* cur_tab = dynamic_cast<Tab *> (tab);

//     if (cur_tab == nullptr) {
//         if (!m_mode_region->GetValue()) {
//             cur_tab = (Tab*) m_tab_print;
//         } else if (m_tab_print_part && ((TabPrintModel*) m_tab_print_part)->has_model_config()) {
//             cur_tab = (Tab*) m_tab_print_part;
//         } else if (m_tab_print_layer && ((TabPrintModel*)m_tab_print_layer)->has_model_config()) {
//             cur_tab = (Tab*)m_tab_print_layer;
//         } else if (m_tab_print_object && ((TabPrintModel*) m_tab_print_object)->has_model_config()) {
//             cur_tab = (Tab*) m_tab_print_object;
//         } else if (m_tab_print_plate && ((TabPrintPlate*)m_tab_print_plate)->has_model_config()) {
//             cur_tab = (Tab*)m_tab_print_plate;
//         }
//         Show(cur_tab != nullptr);
//         AppAdapter::gui_app()->sidebar().show_object_list(m_mode_region->GetValue());
//         if (m_current_tab == cur_tab)
//             return;
//         if (cur_tab)
//             cur_tab->restore_last_select_item();
//         return;
//     }

//     m_current_tab = tab;
//     BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": set current to %1%, type=%2%") % cur_tab % cur_tab?cur_tab->type():-1;
//     update_mode();

//     // BBS: open/close tab
//     for (auto t : std::vector<std::pair<wxPanel*, wxStaticLine*>>({
//             {m_tab_print, m_staticline_print},
//             {m_tab_print_object, m_staticline_print_object},
//             {m_tab_print_part, m_staticline_print_part},
//             {m_tab_print_layer, nullptr},
//             {m_tab_print_plate, nullptr},
//             {m_tab_filament, m_staticline_filament},
//             {m_tab_printer, m_staticline_printer}})) {
//         if (!t.first) continue;
//         t.first->Show(tab == t.first);
//         if (!t.second) continue;
//         t.second->Show(tab == t.first);
//         //m_left_sizer->GetItem(t)->SetProportion(tab == t ? 1 : 0);
//     }
//     m_left_sizer->Layout();
//     if (auto dialog = dynamic_cast<wxDialog*>(GetParent())) {
//         wxString title = cur_tab->type() == Preset::TYPE_FILAMENT ? _L("Material settings") : _L("Printer settings");
//         dialog->SetTitle(title);
//     }

//     auto tab_print = dynamic_cast<Tab *>(m_tab_print);
//     if (cur_tab == m_tab_print) {
//         if (tab_print)
//             tab_print->toggle_line("print_flow_ratio", false);
//     } else {
//         if (tab_print)
//             tab_print->toggle_line("print_flow_ratio", false);
//     }
// }

// bool PrinterPresetPanel::is_active_and_shown_tab(wxPanel* tab)
// {
//     if (m_current_tab == tab)
//         return true;
//     else
//         return false;
// }

// void PrinterPresetPanel::update_mode()
// {
//     int app_mode = app_get_mode();
//     SwitchButton * mode_view = m_current_tab ? dynamic_cast<Tab*>(m_current_tab)->m_mode_view : nullptr;
//     if (mode_view == nullptr) mode_view = m_mode_view;
//     if (mode_view == nullptr) return;

//     //BBS: disable the mode tab and return directly when enable develop mode
//     if (app_mode == comDevelop)
//     {
//         mode_view->Disable();
//         return;
//     }
//     if (!mode_view->IsEnabled())
//         mode_view->Enable();

//     if (app_mode == comAdvanced)
//     {
//         mode_view->SetValue(true);
//     }
//     else
//     {
//         mode_view->SetValue(false);
//     }
// }

void PrinterPresetPanel::msw_rescale()
{
    // if (m_process_icon) m_process_icon->msw_rescale();
    // if (m_setting_btn) m_setting_btn->msw_rescale();
    // if (m_search_btn) m_search_btn->msw_rescale();
    // if (m_compare_btn) m_compare_btn->msw_rescale();
    // if (m_tips_arrow) m_tips_arrow->msw_rescale();
    // m_left_sizer->SetMinSize(wxSize(40 * em_unit(this), -1));
    // if (m_mode_region)
    //     ((SwitchButton* )m_mode_region)->Rescale();
    // if (m_mode_view)
    //     ((SwitchButton* )m_mode_view)->Rescale();
    // for (auto tab : {m_tab_print, m_tab_print_plate, m_tab_print_object, m_tab_print_part, m_tab_print_layer, m_tab_filament, m_tab_printer}) {
    //     if (tab) dynamic_cast<Tab*>(tab)->msw_rescale();
    // }
    // //((Button*)m_export_to_file)->Rescale();
    // //((Button*)m_import_from_file)->Rescale();
}

// void PrinterPresetPanel::switch_to_global()
// {
//     m_mode_region->SetValue(false);
//     set_active_tab(nullptr);
// }

// void PrinterPresetPanel::switch_to_object(bool with_tips)
// {
//     m_mode_region->SetValue(true);
//     set_active_tab(nullptr);
//     if (with_tips) {
//         m_highlighter.init(std::pair(m_tips_arrow, &m_tips_arror_blink), nullptr);
//         m_highlighter.blink();
//     }
// }

// void PrinterPresetPanel::notify_object_config_changed()
// {
//     auto & model = AppAdapter::gui_app()->model();
//     bool has_config = false;
//     for (auto obj : model.objects) {
//         if (!obj->config.empty()) {
//             SettingsFactory::Bundle cat_options = SettingsFactory::get_bundle(&obj->config.get(), true);
//             if (cat_options.size() > 0) {
//                 has_config = true;
//                 break;
//             }
//         }
//         for (auto volume : obj->volumes) {
//             if (!volume->config.empty()) {
//                 SettingsFactory::Bundle cat_options = SettingsFactory::get_bundle(&volume->config.get(), true);
//                 if (cat_options.size() > 0) {
//                     has_config = true;
//                     break;
//                 }
//             }
//         }
//         if (has_config) break;
//     }
//     if (has_config == m_has_object_config) return;
//     m_has_object_config = has_config;
//     if (has_config)
//         m_mode_region->SetTextColor2(StateColor(std::pair{0xfffffe, (int) StateColor::Checked}, std::pair{get_label_clr_modified(), 0}));
//     else
//         m_mode_region->SetTextColor2(StateColor());
//     m_mode_region->Rescale();
// }

// void PrinterPresetPanel::switch_to_object_if_has_object_configs()
// {
//     if (m_has_object_config)
//         m_mode_region->SetValue(true);
//     set_active_tab(nullptr);
// }

void PrinterPresetPanel::free_sizers()
{
    if (m_top_sizer)
    {
        m_top_sizer->Clear(false);
    }

    m_left_sizer = nullptr;
    //m_right_sizer = nullptr;
    //m_print_sizer = nullptr;
    //m_filament_sizer = nullptr;
    //m_printer_sizer = nullptr;
    // m_button_sizer = nullptr;
}

// void PrinterPresetPanel::delete_subwindows()
// {
//     if (m_title_label)
//     {
//         delete m_title_label;
//         m_title_label = nullptr;
//     }

//     if (m_mode_region)
//     {
//         delete m_mode_region;
//         m_mode_region = nullptr;
//     }

//     if (m_mode_view)
//     {
//         delete m_mode_view;
//         m_mode_view = nullptr;
//     }

//     if (m_title_view)
//     {
//         delete m_title_view;
//         m_title_view = nullptr;
//     }

//     if (m_search_btn)
//     {
//         delete m_search_btn;
//         m_search_btn = nullptr;
//     }

//     if (m_staticline_print)
//     {
//         delete m_staticline_print;
//         m_staticline_print = nullptr;
//     }

//     if (m_staticline_print_part)
//     {
//         delete m_staticline_print_part;
//         m_staticline_print_part = nullptr;
//     }

//     if (m_staticline_print_object)
//     {
//         delete m_staticline_print_object;
//         m_staticline_print_object = nullptr;
//     }

//     if (m_staticline_filament)
//     {
//         delete m_staticline_filament;
//         m_staticline_filament = nullptr;
//     }

//     if (m_staticline_printer)
//     {
//         delete m_staticline_printer;
//         m_staticline_printer = nullptr;
//     }

//     if (m_export_to_file)
//     {
//         delete m_export_to_file;
//         m_export_to_file = nullptr;
//     }

//     if (m_import_from_file)
//     {
//         delete m_import_from_file;
//         m_import_from_file = nullptr;
//     }

//     if (m_page_view)
//     {
//         delete m_page_view;
//         m_page_view = nullptr;
//     }
// }

PrinterPresetDialog::PrinterPresetDialog(wxWindow * parent)
	: DPIDialog(parent, wxID_ANY,  "", wxDefaultPosition,
		wxDefaultSize, wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER)
{
	m_panel = new PrinterPresetPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT | wxTAB_TRAVERSAL);
	auto* topsizer = new wxBoxSizer(wxVERTICAL);
	topsizer->Add(m_panel, 1, wxALL | wxEXPAND, 0, NULL);

	SetSizerAndFit(topsizer);
	SetSize({75 * em_unit(), 60 * em_unit()});

	Layout();
	Center();
}

void PrinterPresetDialog::Popup()
{
    m_panel->rebuild_panels();

    UpdateDlgDarkUI(this);
    Center();
    ShowModal();
}

void PrinterPresetDialog::on_dpi_changed(const wxRect &suggested_rect)
{
	Fit();
	SetSize({75 * em_unit(), 60 * em_unit()});
	m_panel->msw_rescale();
	Refresh();
}

} // namespace GUI
} // namespace Slic3r

#pragma once

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/wxExtensions.hpp"

namespace Slic3r { 
namespace GUI {

class TabPrinter;
class PrinterPresetPanel : public wxPanel
{
// #if __WXOSX__
//     wxWindow*            m_tmp_panel;
//     int                 m_size_move = -1;
// #endif // __WXOSX__

private:
    void free_sizers();
    void delete_subwindows();
    void refresh_tabs();

protected:
    wxBoxSizer* m_top_sizer { nullptr };
    wxBoxSizer* m_left_sizer { nullptr };
    // wxBoxSizer* m_mode_sizer { nullptr };
    // // // BBS: new layout
    // ScalableButton* m_process_icon{ nullptr };
    // wxStaticText* m_title_label { nullptr };
    // SwitchButton* m_mode_region { nullptr };
    // ScalableButton *m_tips_arrow{nullptr};
    // bool m_tips_arror_blink{false};
    // wxStaticText* m_title_view { nullptr };
    // SwitchButton* m_mode_view { nullptr };
    // //wxBitmapButton* m_search_button { nullptr };
    // wxStaticLine* m_staticline_print { nullptr };
    // //wxBoxSizer* m_print_sizer { nullptr };
    // wxPanel* m_tab_print { nullptr };
    // wxPanel* m_tab_print_plate { nullptr };
    // wxPanel* m_tab_print_object { nullptr };
    // wxStaticLine* m_staticline_print_object { nullptr };
    // wxPanel* m_tab_print_part { nullptr };
    // wxPanel* m_tab_print_layer { nullptr };
    // wxStaticLine* m_staticline_print_part { nullptr };
    // wxStaticLine* m_staticline_filament { nullptr };
    // //wxBoxSizer* m_filament_sizer { nullptr };
    // wxPanel* m_tab_filament { nullptr };
    // wxStaticLine* m_staticline_printer { nullptr };
    // //wxBoxSizer* m_printer_sizer { nullptr };
    TabPrinter* m_tab_printer { nullptr };
    // //wxStaticLine* m_staticline_buttons { nullptr };
    // // BBS: new layout
    // wxBoxSizer* m_button_sizer { nullptr };
    // wxWindow* m_export_to_file { nullptr };
    // wxWindow* m_import_from_file { nullptr };
    // //wxStaticLine* m_staticline_middle{ nullptr };
    // //wxBoxSizer* m_right_sizer { nullptr };
    wxScrolledWindow* m_page_view { nullptr };
    wxBoxSizer* m_page_sizer { nullptr };

    // ScalableButton*		m_setting_btn { nullptr };
    // ScalableButton*		m_search_btn { nullptr };
    // ScalableButton*		m_compare_btn { nullptr };

    // wxBitmap m_toggle_on_icon;
    // wxBitmap m_toggle_off_icon;

    // wxPanel* m_current_tab { nullptr };

    // bool m_has_object_config { false };

    // struct Highlighter
    // {
    //     void set_timer_owner(wxEvtHandler *owner, int timerid = wxID_ANY);
    //     void init(std::pair<wxWindow *, bool *>, wxWindow *parent = nullptr);
    //     void blink();
    //     void invalidate();

    // private:
    //     wxWindow *      m_bitmap{nullptr};
    //     bool *         m_show_blink_ptr{nullptr};
    //     int            m_blink_counter{0};
    //     wxTimer        m_timer;
    //     wxWindow *      m_parent { nullptr };
    // } m_highlighter;

    // void OnToggled(wxCommandEvent& event);

public:
    PrinterPresetPanel( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 1800,1080 ), long style = wxTAB_TRAVERSAL, const wxString& type = wxEmptyString );
    ~PrinterPresetPanel();

    void rebuild_panels();
    void create_layout();
    // //clear the right page
    // void clear_page();
    // void OnActivate();
    // void set_active_tab(wxPanel*tab);
    // bool is_active_and_shown_tab(wxPanel*tab);
    // void update_mode();
    void msw_rescale();
    // void switch_to_global();
    // void switch_to_object(bool with_tips = false);

    // void notify_object_config_changed();
    // void switch_to_object_if_has_object_configs();

    // StaticBox* get_top_panel() { return m_top_panel; }

    // wxPanel* filament_panel() { return m_tab_filament; }

    // wxScrolledWindow* get_paged_view() { return m_page_view;}
    // wxPanel*    get_current_tab() { return m_current_tab; }

};

class PrinterPresetDialog : public DPIDialog
{
public:
    PrinterPresetDialog(wxWindow * parent);

    void Popup();
protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    PrinterPresetPanel * m_panel;
};

} // namespace GUI
} // namespace Slic3r


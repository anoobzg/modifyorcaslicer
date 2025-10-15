#ifndef slic3r_MainFrame_hpp_
#define slic3r_MainFrame_hpp_


#include <wx/frame.h>
#ifdef __APPLE__
#include <wx/taskbar.h>
#endif // __APPLE__

#include <string>
#include <map>

#include "GUI_Utils.hpp"
#include <boost/property_tree/ptree_fwd.hpp>

class BBLTopbar;
namespace Slic3r {

namespace GUI
{

class Notebook;
class Plater;
class MainFrame;
class MainPanel;
class MultiNozzle_Calibration_Dlg;

class MainFrame : public DPIFrame
// class MainFrame : public wxFrame
{
protected:
    virtual void on_dpi_changed(const wxRect &suggested_rect) override;
    virtual void on_sys_color_changed() override;

#ifdef __WIN32__
    WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

public:
    MainFrame();
    ~MainFrame() = default;

    void        setup_context(MainPanel* panel);
	void 		shutdown();
    void        update_title();
    void        set_title(const wxString& title);
    
    void        init_menubar_as_editor();
    wxMenu*     generate_help_menu();
    // Open item in menu by menu and item name (in actual language)
    void        open_menubar_item(const wxString& menu_name,const wxString& item_name);

#ifndef __APPLE__
    // BBS. Replace title bar and menu bar with top bar.
    BBLTopbar*            m_topbar{ nullptr };
#else
    wxPanel*              panel_topbar {nullptr};
#endif

    MainPanel* m_panel;
    Plater*    m_plater{ nullptr };

    wxMenuBar*  m_menubar{ nullptr };
    wxMenu *    m_calib_menu{nullptr};

    wxMenuItem* m_menu_item_reslice_now { nullptr };
    
    // Multi-Nozzle Calibration dialog
    MultiNozzle_Calibration_Dlg* m_multi_nozzle_calib_dlg{ nullptr };
};

} // GUI
} //Slic3r

#endif // slic3r_MainFrame_hpp_

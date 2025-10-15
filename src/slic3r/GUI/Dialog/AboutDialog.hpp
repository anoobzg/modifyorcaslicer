#ifndef slic3r_GUI_AboutDialog_hpp_
#define slic3r_GUI_AboutDialog_hpp_

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/wxExtensions.hpp"

#include <wx/html/htmlwin.h>

namespace Slic3r { 
namespace GUI {

class AboutDialogLogo : public wxPanel
{
public:
    AboutDialogLogo(wxWindow* parent);
    
private:
    ScalableBitmap logo;
    void onRepaint(wxEvent &event);
};



class CopyrightsDialog : public DPIDialog
{
public:
    CopyrightsDialog(wxWindow* parent);
    ~CopyrightsDialog() {}

    struct Entry {
        Entry(const std::string &lib_name, const std::string &copyright, const std::string &link) : 
            lib_name(lib_name), copyright(copyright), link(link) {}

        std::string     lib_name;
        std::string     copyright;
        std::string   	link;
    };

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    
private:
    wxHtmlWindow*   m_html;
    std::vector<Entry> m_entries;

    void onLinkClicked(wxHtmlLinkEvent &event);
    void onCloseDialog(wxEvent &);

    void fill_entries();
    wxString get_html_text();
};



class AboutDialog : public DPIDialog
{
    ScalableBitmap  m_logo_bitmap;
    wxHtmlWindow*   m_html;
    wxStaticBitmap* m_logo;
    int             m_copy_rights_btn_id { wxID_ANY };
    int             m_copy_version_btn_id { wxID_ANY };
public:
    AboutDialog(wxWindow* parent);

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    
private:
    void onLinkClicked(wxHtmlLinkEvent &event);
    void onCloseDialog(wxEvent &);
    void onCopyrightBtn(wxEvent &);
    void onCopyToClipboard(wxEvent&);
};

} // namespace GUI
} // namespace Slic3r

#endif

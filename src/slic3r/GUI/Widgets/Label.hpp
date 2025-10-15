#ifndef slic3r_GUI_Label_hpp_
#define slic3r_GUI_Label_hpp_

#include <wx/stattext.h>

#define LB_HYPERLINK 0x0020
#define LB_PROPAGATE_MOUSE_EVENT 0x0040
#define LB_AUTO_WRAP 0x0080

namespace Slic3r {
namespace GUI {

class Label : public wxStaticText
{
public:
    Label(wxWindow *parent, wxString const &text = {}, long style = 0);

	Label(wxWindow *parent, wxFont const &font, wxString const &text = {}, long style = 0);

    void SetLabel(const wxString& label) override;

    void SetWindowStyleFlag(long style) override;

	void Wrap(int width);

private:
	void OnSize(wxSizeEvent & evt);

private:
    wxFont m_font;
    wxColour m_color;
	wxString m_text;
	bool m_skip_size_evt = false;

public:
    static wxSize split_lines(wxDC &dc, int width, const wxString &text, wxString &multiline_text);
};

}
}
#endif // !slic3r_GUI_Label_hpp_

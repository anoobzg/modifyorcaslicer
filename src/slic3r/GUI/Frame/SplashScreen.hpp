#pragma once
#include <wx/splash.h>

namespace Slic3r {
namespace GUI {
 
struct ConstantText
{
    wxString title;
    wxString version;
    wxString credits;

    wxFont   title_font;
    wxFont   version_font;
    wxFont   credits_font;
    wxFont   based_on_font;

    void init(wxFont init_font);
};

class SplashScreen : public wxSplashScreen
{
public:
    SplashScreen(const wxBitmap& bitmap, wxWindow* window, long splashStyle, int milliseconds, wxPoint pos = wxDefaultPosition);
    virtual ~SplashScreen();

    void SetText(const wxString& text);
    void Decorate(wxBitmap& bmp);
    static wxBitmap MakeBitmap();
    void set_bitmap(wxBitmap& bmp);
    void scale_bitmap(wxBitmap& bmp, float scale);
    void scale_font(wxFont& font, float scale);
protected:
    wxStaticText* m_staticText_slicer_name;
    wxStaticText* m_staticText_slicer_version;
    wxStaticBitmap* m_bitmap;
    wxStaticText* m_staticText_loading;

    wxBitmap    m_main_bitmap;
    wxFont      m_action_font;
    int         m_action_line_y_position;
    float       m_scale {1.0};

    ConstantText m_constant_text;
};

}
}
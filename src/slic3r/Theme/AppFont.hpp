#pragma once

#include "slic3r/Theme/Font.hpp"

namespace Slic3r {
namespace GUI {

class AppFont {
public:
    AppFont();
    ~AppFont();

    void init_app_fonts();
    void update_app_fonts(int em_unit = 10);

    wxFont		    m_small_font;
    wxFont		    m_bold_font;
	wxFont			m_normal_font;
	wxFont			m_code_font;
    wxFont		    m_link_font;

    int             m_em_unit; // width of a "m"-symbol in pixels for current system font
                               // Note: for 100% Scale m_em_unit = 10 -> it's a good enough coefficient for a size setting of controls

};

void init_app_font();
void update_app_font(int em_unit);

const wxFont&   app_small_font();
const wxFont&   app_bold_font();
const wxFont&   app_normal_font();
const wxFont&   app_code_font();
const wxFont&   app_link_font();

int             app_em_unit();
wxSize          app_min_size();

wxFont* find_font(const std::string& text_str, int max_size = 32);

}
}
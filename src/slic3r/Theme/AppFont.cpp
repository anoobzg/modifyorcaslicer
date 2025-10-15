#include "AppFont.hpp"
#include "Font.hpp"

namespace Slic3r {
namespace GUI {

AppFont::AppFont()
{

}

AppFont::~AppFont()
{

}

void AppFont::init_app_fonts()
{
    // BBS: modify font
    m_small_font = Font::Body_10;
    m_bold_font = Font::Body_10.Bold();
    m_normal_font = Font::Body_10;

#ifdef __WXMAC__
    m_small_font.SetPointSize(11);
    m_bold_font.SetPointSize(13);
#endif /*__WXMAC__*/

    // wxSYS_OEM_FIXED_FONT and wxSYS_ANSI_FIXED_FONT use the same as
    // DEFAULT in wxGtk. Use the TELETYPE family as a work-around
    m_code_font = wxFont(wxFontInfo().Family(wxFONTFAMILY_TELETYPE));
    m_code_font.SetPointSize(m_small_font.GetPointSize());
}

void AppFont::update_app_fonts(int em_unit)
{
    m_normal_font   = Font::Body_14; // BBS: larger font size
    m_small_font    = m_normal_font;
    m_bold_font     = m_normal_font.Bold();
    m_link_font     = m_bold_font.Underlined();
    m_em_unit       = em_unit;
    m_code_font.SetPointSize(m_small_font.GetPointSize());        
}

AppFont global_font;
void init_app_font()
{
    initSysFont();
    global_font.init_app_fonts();
}

void update_app_font(int em_unit)
{
    global_font.update_app_fonts(em_unit);    
}

const wxFont&   app_small_font()
{
    return global_font.m_small_font;
}

const wxFont&   app_bold_font()
{
    return global_font.m_bold_font;
}

const wxFont&   app_normal_font()
{
    return global_font.m_normal_font;
}

const wxFont&   app_code_font()
{
    return global_font.m_code_font;
}

const wxFont&   app_link_font()
{
    return global_font.m_link_font;
}

int app_em_unit()
{
    return global_font.m_em_unit;
}

wxSize app_min_size()
{
    return wxSize(76*app_em_unit(), 49 * app_em_unit());
}

// get text extent with wxMemoryDC
void get_text_extent(const wxString &msg, wxCoord &w, wxCoord &h, wxFont *font)
{
  wxMemoryDC memDC;
  if (font)
    memDC.SetFont(*font);
  memDC.GetTextExtent(msg, &w, &h);
}


wxFont* find_font(const std::string& text_str, int max_size)
{
  auto is_font_suitable = [](std::string str, wxFont &font, int max_size) {
    wxString msg(str);
	wxCoord w, h;
	get_text_extent(msg, w, h, &font);

    if (w <= max_size)
      return true;
    else
      return false;
  };
  wxFont *font = nullptr;
  if (is_font_suitable(text_str, Font::Head_24, max_size))
    font = &Font::Head_24;
  else if (is_font_suitable(text_str, Font::Head_20, max_size))
    font = &Font::Head_20;
  else if (is_font_suitable(text_str, Font::Head_18, max_size))
    font = &Font::Head_18;
  else if (is_font_suitable(text_str, Font::Head_16, max_size))
    font = &Font::Head_16;
  else if (is_font_suitable(text_str, Font::Head_14, max_size))
    font = &Font::Head_14;
  else
    font = &Font::Head_12;

  return font;
}

}
}
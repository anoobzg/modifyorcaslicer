#include "SplashScreen.hpp"

#include "slic3r/Theme/AppFont.hpp"
#include "slic3r/Theme/AppColor.hpp"
#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/Theme/BitmapCache.hpp"

#include <wx/fontutil.h>

namespace Slic3r {
namespace GUI {

    void ConstantText::init(wxFont init_font)
    {
        // dynamically get the version to display
        version = format_display_version();

        // credits infornation
        credits = "";

        //title_font    = Font::Head_16;
        version_font  = Font::Body_13;
        based_on_font = Font::Body_8;
        credits_font  = Font::Body_8;
    }

    SplashScreen::SplashScreen(const wxBitmap& bitmap, wxWindow* window, long splashStyle, int milliseconds, wxPoint pos)
        : wxSplashScreen(bitmap, splashStyle, milliseconds, window, wxID_ANY, wxDefaultPosition, wxDefaultSize,
#ifdef __APPLE__
            wxBORDER_NONE | wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP
#else
            wxBORDER_NONE | wxFRAME_NO_TASKBAR
#endif // !__APPLE__
        )
    {
        int init_dpi = get_dpi_for_window(this);
        this->SetPosition(pos);
        this->CenterOnScreen();
        int new_dpi = get_dpi_for_window(this);

        m_scale = (float)(new_dpi) / (float)(init_dpi);

        m_main_bitmap = bitmap;

        scale_bitmap(m_main_bitmap, m_scale);

        // init constant texts and scale fonts
        m_constant_text.init(Font::Body_16);

		// ORCA scale all fonts with monitor scale
        scale_font(m_constant_text.version_font,	m_scale * 2);
        scale_font(m_constant_text.based_on_font,	m_scale * 1.5f);
        scale_font(m_constant_text.credits_font,	m_scale * 2);

        // this font will be used for the action string
        m_action_font = m_constant_text.credits_font;

        // draw logo and constant info text
        Decorate(m_main_bitmap);
        UpdateFrameDarkUI(this);
    }

    SplashScreen::~SplashScreen()
    {

    }

    void SplashScreen::SetText(const wxString& text)
    {
        set_bitmap(m_main_bitmap);
        if (!text.empty()) {
            wxBitmap bitmap(m_main_bitmap);

            wxMemoryDC memDC;
            memDC.SelectObject(bitmap);
            memDC.SetFont(m_action_font);
            memDC.SetTextForeground(StateColor::darkModeColorFor(wxColour(144, 144, 144)));
            int width = bitmap.GetWidth();
            int text_height = memDC.GetTextExtent(text).GetHeight();
            int text_width = memDC.GetTextExtent(text).GetWidth();
            wxRect text_rect(wxPoint(0, m_action_line_y_position), wxPoint(width, m_action_line_y_position + text_height));
            memDC.DrawLabel(text, text_rect, wxALIGN_CENTER);

            memDC.SelectObject(wxNullBitmap);
            set_bitmap(bitmap);
#ifdef __WXOSX__
            // without this code splash screen wouldn't be updated under OSX
            wxYield();
#endif
        }
    }

    void SplashScreen::Decorate(wxBitmap& bmp)
    {
        if (!bmp.IsOk())
            return;

		bool is_dark = app_get_bool("dark_color_mode");

        // use a memory DC to draw directly onto the bitmap
        wxMemoryDC memDc(bmp);
        
        int width = bmp.GetWidth();
		int height = bmp.GetHeight();

		// Logo
        BitmapCache bmp_cache;
        wxBitmap logo_bmp = *bmp_cache.load_svg(is_dark ? "splash_logo_dark" : "splash_logo", width, height);  // use with full width & height
        memDc.DrawBitmap(logo_bmp, 0, 0, true);

        // Version
        memDc.SetFont(m_constant_text.version_font);
        memDc.SetTextForeground(StateColor::darkModeColorFor(wxColor(134, 134, 134)));
        wxSize version_ext = memDc.GetTextExtent(m_constant_text.version);
        wxRect version_rect(
			wxPoint(0, int(height * 0.70)),
			wxPoint(width, int(height * 0.70) + version_ext.GetHeight())
		);
        memDc.DrawLabel(m_constant_text.version, version_rect, wxALIGN_CENTER);

        // Dynamic Text
        m_action_line_y_position = int(height * 0.83);

		// Based on Text
        memDc.SetFont(m_constant_text.based_on_font);
        auto bs_version = wxString::Format("Based on PrusaSlicer and BambuStudio").ToStdString();
        wxSize based_on_ext = memDc.GetTextExtent(bs_version);
        wxRect based_on_rect(
			wxPoint(0, height - based_on_ext.GetHeight() * 2),
            wxPoint(width, height - based_on_ext.GetHeight())
		);
        memDc.DrawLabel(bs_version, based_on_rect, wxALIGN_CENTER);
    }

    wxBitmap SplashScreen::MakeBitmap()
    {
        int width = FromDIP(480, nullptr);
        int height = FromDIP(480, nullptr);

        wxImage image(width, height);
        wxBitmap new_bmp(image);

        wxMemoryDC memDC;
        memDC.SelectObject(new_bmp);
        memDC.SetBrush(StateColor::darkModeColorFor(*wxWHITE));
        memDC.DrawRectangle(-1, -1, width + 2, height + 2);
        memDC.DrawBitmap(new_bmp, 0, 0, true);
        return new_bmp;
    }

    void SplashScreen::set_bitmap(wxBitmap& bmp)
    {
        m_window->SetBitmap(bmp);
        m_window->Refresh();
        m_window->Update();
    }

    void SplashScreen::scale_bitmap(wxBitmap& bmp, float scale)
    {
        if (scale == 1.0)
            return;

        wxImage image = bmp.ConvertToImage();
        if (!image.IsOk() || image.GetWidth() == 0 || image.GetHeight() == 0)
            return;

        int width   = int(scale * image.GetWidth());
        int height  = int(scale * image.GetHeight());
        image.Rescale(width, height, wxIMAGE_QUALITY_BILINEAR);

        bmp = wxBitmap(std::move(image));
    }

    void SplashScreen::scale_font(wxFont& font, float scale)
    {
#ifdef __WXMSW__
        // Workaround for the font scaling in respect to the current active display,
        // not for the primary display, as it's implemented in Font.cpp
        // See https://github.com/wxWidgets/wxWidgets/blob/master/src/msw/font.cpp
        // void wxNativeFontInfo::SetFractionalPointSize(float pointSizeNew)
        wxNativeFontInfo nfi= *font.GetNativeFontInfo();
        float pointSizeNew  = scale * font.GetPointSize();
        nfi.lf.lfHeight     = nfi.GetLogFontHeightAtPPI(pointSizeNew, get_dpi_for_window(this));
        nfi.pointSize       = pointSizeNew;
        font = wxFont(nfi);
#else
        font.Scale(scale);
#endif //__WXMSW__
    }


}
}
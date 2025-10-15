#include "Font.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"

namespace Slic3r {
namespace GUI {

namespace Font {
    wxFont Head_48;
    wxFont Head_32;
    wxFont Head_24;
    wxFont Head_20;
    wxFont Head_18;
    wxFont Head_16;
    wxFont Head_15;
    wxFont Head_14;
    wxFont Head_13;
    wxFont Head_12;
    wxFont Head_11;
    wxFont Head_10;

    wxFont Body_16;
    wxFont Body_15;
    wxFont Body_14;
    wxFont Body_13;
    wxFont Body_12;
    wxFont Body_11;
    wxFont Body_10;
    wxFont Body_9;
    wxFont Body_8;
}

wxFont sysFont(int size, bool bold)
{
//#ifdef __linux__
//    return wxFont{};
//#endif
#ifndef __APPLE__
    size = size * 4 / 5;
#endif

    wxString face = "HarmonyOS Sans SC";

    // Check if the current locale is Korean
    if (wxLocale::GetSystemLanguage() == wxLANGUAGE_KOREAN) {
        face = "NanumGothic";
    }

    wxFont font{size, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL, false, face};
    font.SetFaceName(face);
    if (!font.IsOk()) {
      BOOST_LOG_TRIVIAL(warning) << boost::format("Can't find %1% font") % face;
      font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
      BOOST_LOG_TRIVIAL(warning) << boost::format("Use system font instead: %1%") % font.GetFaceName();
      if (bold)
        font.MakeBold();
      font.SetPointSize(size);
    }
    return font;
}

void initSysFont()
{
#if defined(__linux__) || defined(_WIN32)
    const std::string &resource_path = Slic3r::resources_dir();
    wxString font_path = wxString::FromUTF8(resource_path + "/fonts/HarmonyOS_Sans_SC_Bold.ttf");
    bool result = wxFont::AddPrivateFont(font_path);
    BOOST_LOG_TRIVIAL(info) << boost::format("add font of HarmonyOS_Sans_SC_Bold returns %1%")%result;

    font_path = wxString::FromUTF8(resource_path + "/fonts/HarmonyOS_Sans_SC_Regular.ttf");
    result = wxFont::AddPrivateFont(font_path);
    BOOST_LOG_TRIVIAL(info) << boost::format("add font of HarmonyOS_Sans_SC_Regular returns %1%")%result;

    // Adding NanumGothic Regular and Bold
    font_path = wxString::FromUTF8(resource_path + "/fonts/NanumGothic-Regular.ttf");
    result = wxFont::AddPrivateFont(font_path);
    BOOST_LOG_TRIVIAL(info) << boost::format("add font of NanumGothic-Regular returns %1%")%result;

    font_path = wxString::FromUTF8(resource_path + "/fonts/NanumGothic-Bold.ttf");
    result = wxFont::AddPrivateFont(font_path);
    BOOST_LOG_TRIVIAL(info) << boost::format("add font of NanumGothic-Bold returns %1%")%result;
#endif
    Font::Head_48 = sysFont(48, true);
    Font::Head_32 = sysFont(32, true);
    Font::Head_24 = sysFont(24, true);
    Font::Head_20 = sysFont(20, true);
    Font::Head_18 = sysFont(18, true);
    Font::Head_16 = sysFont(16, true);
    Font::Head_15 = sysFont(15, true);
    Font::Head_14 = sysFont(14, true);
    Font::Head_13 = sysFont(13, true);
    Font::Head_12 = sysFont(12, true);
    Font::Head_11 = sysFont(11, true);
    Font::Head_10 = sysFont(10, true);

    Font::Body_16 = sysFont(16, false);
    Font::Body_15 = sysFont(15, false);
    Font::Body_14 = sysFont(14, false);
    Font::Body_13 = sysFont(13, false);
    Font::Body_12 = sysFont(12, false);
    Font::Body_11 = sysFont(11, false);
    Font::Body_10 = sysFont(10, false);
    Font::Body_9  = sysFont(9, false);
    Font::Body_8  = sysFont(8, false);
}

}
}

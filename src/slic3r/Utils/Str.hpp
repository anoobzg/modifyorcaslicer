#pragma once
#include <vector>
#include <string>

namespace Slic3r {
namespace GUI {
    std::vector<std::string> split_str(std::string src, std::string separator);

    std::string     format_IP(const std::string& ip);

    char            from_hex(char ch);
    std::string     url_encode(std::string value);
    std::string     url_decode(std::string value);

    std::string convert_studio_language_to_api(std::string lang_code);

    // escaping of path string according to 
    // https://cgit.freedesktop.org/xdg/xdg-specs/tree/desktop-entry/desktop-entry-spec.xml
    std::string escape_path_string_xdg(const std::string& str);

    std::string decode(std::string const& extra, std::string const& path = {});

    wxString        filter_string(wxString str);

    static wxString dots("...", wxConvUTF8);
}
}
#include "Str.hpp"

#include "libslic3r/Utils.hpp"
#include "slic3r/Net/Http.hpp"

namespace Slic3r {
namespace GUI {
    std::vector<std::string> split_str(std::string src, std::string separator)
    {
        std::string::size_type pos;
        std::vector<std::string> result;
        src += separator;
        int size = src.size();

        for (int i = 0; i < size; i++)
        {
            pos = src.find(separator, i);
            if (pos < size)
            {
                std::string s = src.substr(i, pos - i);
                result.push_back(s);
                i = pos + separator.size() - 1;
            }
        }
        return result;
    }
    
    std::string format_IP(const std::string& ip)
    {
        std::string format_ip = ip;
        size_t pos_st = 0;
        size_t pos_en = 0;

        for (int i = 0; i < 2; i++) {
            pos_en = format_ip.find('.', pos_st + 1);
            if (pos_en == std::string::npos) {
                return ip;
            }
            format_ip.replace(pos_st, pos_en - pos_st, "***");
            pos_st = pos_en + 1;
        }

        return format_ip;
    }

    char from_hex(char ch) {
        return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
    }

    std::string url_decode(std::string value) {
        return Http::url_decode(value);
    }

    std::string url_encode(std::string value) {
        return Http::url_encode(value);
    }

    std::string convert_studio_language_to_api(std::string lang_code)
    {
        boost::replace_all(lang_code, "_", "-");
        return lang_code;

        /*if (lang_code == "zh_CN")
            return "zh-hans";
        else if (lang_code == "zh_TW")
            return "zh-hant";
        else
            return "en";*/
    }

    std::string escape_path_string_xdg(const std::string& str)
    {
        // The buffer needs to be bigger if escaping <,>,&
        std::vector<char> out(str.size() * 4, 0);
        char *outptr = out.data();
        for (size_t i = 0; i < str.size(); ++ i) {
            char c = str[i];
            // must be escaped
            if (c == '\"') { //double quote 
                (*outptr ++) = '\\';
                (*outptr ++) = '\"';
            } else if (c == '`') {  // backtick character
                (*outptr ++) = '\\';
                (*outptr ++) = '`';
            } else if (c == '$') { // dollar sign
                (*outptr ++) = '\\';
                (*outptr ++) = '$';
            } else if (c == '\\') { // backslash character
                (*outptr ++) = '\\';
                (*outptr ++) = '\\';
                (*outptr ++) = '\\';
                (*outptr ++) = '\\';
            //  Reserved characters   
            // At Ubuntu, all these characters must NOT be escaped for desktop integration to work
            /*
            } else if (c == ' ') { // space
                (*outptr ++) = '\\';
                (*outptr ++) = ' ';
            } else if (c == '\t') { // tab
                (*outptr ++) = '\\';
                (*outptr ++) = '\t';
            } else if (c == '\n') { // newline
                (*outptr ++) = '\\';
                (*outptr ++) = '\n';
            } else if (c == '\'') { // single quote
                (*outptr ++) = '\\';
                (*outptr ++) = '\'';
            } else if (c == '>') { // greater-than sign
                (*outptr ++) = '\\';
                (*outptr ++) = '&';
                (*outptr ++) = 'g';
                (*outptr ++) = 't';
                (*outptr ++) = ';';
            } else if (c == '<') { //less-than sign
                (*outptr ++) = '\\';
                (*outptr ++) = '&';
                (*outptr ++) = 'l';
                (*outptr ++) = 't';
                (*outptr ++) = ';'; 
            }  else if (c == '~') { // tilde
                (*outptr ++) = '\\';
                (*outptr ++) = '~';
            } else if (c == '|') { // vertical bar 
                (*outptr ++) = '\\';
                (*outptr ++) = '|';
            } else if (c == '&') { // ampersand
                (*outptr ++) = '\\';
                (*outptr ++) = '&';
                (*outptr ++) = 'a';
                (*outptr ++) = 'm';
                (*outptr ++) = 'p';
                (*outptr ++) = ';';
            } else if (c == ';') { // semicolon
                (*outptr ++) = '\\';
                (*outptr ++) = ';';
            } else if (c == '*') { //asterisk
                (*outptr ++) = '\\';
                (*outptr ++) = '*';
            } else if (c == '?') { // question mark
                (*outptr ++) = '\\';
                (*outptr ++) = '?';
            } else if (c == '#') { // hash mark
                (*outptr ++) = '\\';
                (*outptr ++) = '#';
            } else if (c == '(') { // parenthesis
                (*outptr ++) = '\\';
                (*outptr ++) = '(';
            } else if (c == ')') {
                (*outptr ++) = '\\';
                (*outptr ++) = ')';
            */
            } else
                (*outptr ++) = c;
        }
        return std::string(out.data(), outptr - out.data());
    }

    std::string decode(std::string const& extra, std::string const& path) {
        char const* p = extra.data();
        char const* e = p + extra.length();
        while (p + 4 < e) {
            boost::uint16_t len = ((boost::uint16_t)p[2]) | ((boost::uint16_t)p[3] << 8);
            if (p[0] == '\x75' && p[1] == '\x70' && len >= 5 && p + 4 + len < e && p[4] == '\x01') {
                return std::string(p + 9, p + 4 + len);
            }
            else {
                p += 4 + len;
            }
        }
        return Slic3r::decode_path(path.c_str());
    }

    wxString filter_string(wxString str)
    {
        std::string result = str.utf8_string();
        std::string input = str.utf8_string();


        std::regex domainRegex(R"(([a-zA-Z0-9.-]+\.[a-zA-Z]{2,}(?:\.[a-zA-Z]{2,})?))");
        std::sregex_iterator it(input.begin(), input.end(), domainRegex);
        std::sregex_iterator end;

        while (it != end) {
            std::smatch match = *it;
            std::string domain = match.str();
            result.replace(match.position(), domain.length(), "[***]");
            ++it;
        }

        return wxString::FromUTF8(result);
    }
}
}
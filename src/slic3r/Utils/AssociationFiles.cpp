#include "AssociationFiles.hpp"

#if WIN32 || __WXMSW__
#include <Windows.h>
#include <shlobj.h>
#include <wx/msw/registry.h>
#endif

#include <boost/dll/runtime_symbol_info.hpp>
#if ENABLE_THUMBNAIL_GENERATOR_DEBUG
#include <boost/beast/core/detail/base64.hpp>
#include <boost/nowide/fstream.hpp>
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG

#include "slic3r/Config/AppConfig.hpp"

namespace Slic3r {
namespace GUI {

#ifdef __WXMSW__
static bool set_into_win_registry(HKEY hkeyHive, const wchar_t* pszVar, const wchar_t* pszValue)
{
    // see as reference: https://stackoverflow.com/questions/20245262/c-program-needs-an-file-association
    wchar_t szValueCurrent[1000];
    DWORD dwType;
    DWORD dwSize = sizeof(szValueCurrent);

    int iRC = ::RegGetValueW(hkeyHive, pszVar, nullptr, RRF_RT_ANY, &dwType, szValueCurrent, &dwSize);

    bool bDidntExist = iRC == ERROR_FILE_NOT_FOUND;

    if ((iRC != ERROR_SUCCESS) && !bDidntExist)
        // an error occurred
        return false;

    if (!bDidntExist) {
        if (dwType != REG_SZ)
            // invalid type
            return false;

        if (::wcscmp(szValueCurrent, pszValue) == 0)
            // value already set
            return false;
    }

    DWORD dwDisposition;
    HKEY hkey;
    iRC = ::RegCreateKeyExW(hkeyHive, pszVar, 0, 0, 0, KEY_ALL_ACCESS, nullptr, &hkey, &dwDisposition);
    bool ret = false;
    if (iRC == ERROR_SUCCESS) {
        iRC = ::RegSetValueExW(hkey, L"", 0, REG_SZ, (BYTE*)pszValue, (::wcslen(pszValue) + 1) * sizeof(wchar_t));
        if (iRC == ERROR_SUCCESS)
            ret = true;
    }

    RegCloseKey(hkey);
    return ret;
}

static bool del_win_registry(HKEY hkeyHive, const wchar_t *pszVar, const wchar_t *pszValue)
{
    wchar_t szValueCurrent[1000];
    DWORD   dwType;
    DWORD   dwSize = sizeof(szValueCurrent);

    int iRC = ::RegGetValueW(hkeyHive, pszVar, nullptr, RRF_RT_ANY, &dwType, szValueCurrent, &dwSize);

    bool bDidntExist = iRC == ERROR_FILE_NOT_FOUND;

    if ((iRC != ERROR_SUCCESS) && !bDidntExist)
        return false;

    if (!bDidntExist) {
        iRC      = ::RegDeleteKeyExW(hkeyHive, pszVar, KEY_ALL_ACCESS, 0);
        if (iRC == ERROR_SUCCESS) {
            return true;
        }
    }

    return false;
}

#endif // __WXMSW__

void associate_files(std::wstring extend)
{
#ifdef WIN32
    wchar_t app_path[MAX_PATH];
    ::GetModuleFileNameW(nullptr, app_path, sizeof(app_path));

    std::wstring prog_path = L"\"" + std::wstring(app_path) + L"\"";
    std::wstring prog_id = L" Orca.Slicer.1";
    std::wstring prog_desc = L"OrcaSlicer";
    std::wstring prog_command = prog_path + L" \"%1\"";
    std::wstring reg_base = L"Software\\Classes";
    std::wstring reg_extension = reg_base + L"\\." + extend;
    std::wstring reg_prog_id = reg_base + L"\\" + prog_id;
    std::wstring reg_prog_id_command = reg_prog_id + L"\\Shell\\Open\\Command";

    bool is_new = false;
    is_new |= set_into_win_registry(HKEY_CURRENT_USER, reg_extension.c_str(), prog_id.c_str());
    is_new |= set_into_win_registry(HKEY_CURRENT_USER, reg_prog_id.c_str(), prog_desc.c_str());
    is_new |= set_into_win_registry(HKEY_CURRENT_USER, reg_prog_id_command.c_str(), prog_command.c_str());
    if (is_new)
        // notify Windows only when any of the values gets changed
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#endif // WIN32
}

void disassociate_files(std::wstring extend)
{
#ifdef WIN32
    wchar_t app_path[MAX_PATH];
    ::GetModuleFileNameW(nullptr, app_path, sizeof(app_path));

    std::wstring prog_path = L"\"" + std::wstring(app_path) + L"\"";
    std::wstring prog_id = L" Orca.Slicer.1";
    std::wstring prog_desc = L"OrcaSlicer";
    std::wstring prog_command = prog_path + L" \"%1\"";
    std::wstring reg_base = L"Software\\Classes";
    std::wstring reg_extension = reg_base + L"\\." + extend;
    std::wstring reg_prog_id = reg_base + L"\\" + prog_id;
    std::wstring reg_prog_id_command = reg_prog_id + L"\\Shell\\Open\\Command";

    bool is_new = false;
    is_new |= del_win_registry(HKEY_CURRENT_USER, reg_extension.c_str(), prog_id.c_str());

    bool is_associate_3mf  = app_get_bool("associate_3mf");
    bool is_associate_stl  = app_get_bool("associate_stl");
    bool is_associate_step = app_get_bool("associate_step");
    if (!is_associate_3mf && !is_associate_stl && !is_associate_step)
    {
        is_new |= del_win_registry(HKEY_CURRENT_USER, reg_prog_id.c_str(), prog_desc.c_str());
        is_new |= del_win_registry(HKEY_CURRENT_USER, reg_prog_id_command.c_str(), prog_command.c_str());
    }

    if (is_new)
       ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#endif // WIN32
}

bool check_url_association(std::wstring url_prefix, std::wstring& reg_bin)
{
    reg_bin = L"";
#ifdef WIN32
    wxRegKey key_full(wxRegKey::HKCU, "Software\\Classes\\" + url_prefix + "\\shell\\open\\command");
    if (!key_full.Exists()) {
        return false;
    }
    reg_bin = key_full.QueryDefaultValue().ToStdWstring();

    boost::filesystem::path binary_path(boost::filesystem::canonical(boost::dll::program_location()));
    std::wstring key_string = L"\"" + binary_path.wstring() + L"\" \"%1\"";
    return key_string == reg_bin;
#else
    return false;
#endif // WIN32
}

void associate_url(std::wstring url_prefix)
{
#ifdef WIN32
    boost::filesystem::path binary_path(boost::filesystem::canonical(boost::dll::program_location()));
    // the path to binary needs to be correctly saved in string with respect to localized characters
    wxString wbinary = wxString::FromUTF8(binary_path.string());
    std::string binary_string = (boost::format("%1%") % wbinary).str();
    BOOST_LOG_TRIVIAL(info) << "Downloader registration: Path of binary: " << binary_string;

    std::string key_string = "\"" + binary_string + "\" \"%1\"";

    wxRegKey key_first(wxRegKey::HKCU, "Software\\Classes\\" + url_prefix);
    wxRegKey key_full(wxRegKey::HKCU, "Software\\Classes\\" + url_prefix + "\\shell\\open\\command");
    if (!key_first.Exists()) {
        key_first.Create(false);
    }
    key_first.SetValue("URL Protocol", "");

    if (!key_full.Exists()) {
        key_full.Create(false);
    }
    key_full = key_string;
#elif defined(__linux__) && defined(SLIC3R_DESKTOP_INTEGRATION)
    DesktopIntegrationDialog::perform_downloader_desktop_integration(boost::nowide::narrow(url_prefix));
#endif // WIN32
}

void disassociate_url(std::wstring url_prefix)
{
#ifdef WIN32
    wxRegKey key_full(wxRegKey::HKCU, "Software\\Classes\\" + url_prefix + "\\shell\\open\\command");
    if (!key_full.Exists()) {
        return;
    }
    key_full = "";
#endif // WIN32
}

#if ENABLE_THUMBNAIL_GENERATOR_DEBUG
void gcode_thumbnails_debug()
{
    const std::string BEGIN_MASK = "; thumbnail begin";
    const std::string END_MASK = "; thumbnail end";
    std::string gcode_line;
    bool reading_image = false;
    unsigned int width = 0;
    unsigned int height = 0;

    wxFileDialog dialog(GetTopWindow(), _L("Select a G-code file:"), "", "", "G-code files (*.gcode)|*.gcode;*.GCODE;", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;

    std::string in_filename = into_u8(dialog.GetPath());
    std::string out_path = boost::filesystem::path(in_filename).remove_filename().append(L"thumbnail").string();

    boost::nowide::ifstream in_file(in_filename.c_str());
    std::vector<std::string> rows;
    std::string row;
    if (in_file.good())
    {
        while (std::getline(in_file, gcode_line))
        {
            if (in_file.good())
            {
                if (boost::starts_with(gcode_line, BEGIN_MASK))
                {
                    reading_image = true;
                    gcode_line = gcode_line.substr(BEGIN_MASK.length() + 1);
                    std::string::size_type x_pos = gcode_line.find('x');
                    std::string width_str = gcode_line.substr(0, x_pos);
                    width = (unsigned int)::atoi(width_str.c_str());
                    std::string height_str = gcode_line.substr(x_pos + 1);
                    height = (unsigned int)::atoi(height_str.c_str());
                    row.clear();
                }
                else if (reading_image && boost::starts_with(gcode_line, END_MASK))
                {
                    std::string out_filename = out_path + std::to_string(width) + "x" + std::to_string(height) + ".png";
                    boost::nowide::ofstream out_file(out_filename.c_str(), std::ios::binary);
                    if (out_file.good())
                    {
                        std::string decoded;
                        decoded.resize(boost::beast::detail::base64::decoded_size(row.size()));
                        decoded.resize(boost::beast::detail::base64::decode((void*)&decoded[0], row.data(), row.size()).first);

                        out_file.write(decoded.c_str(), decoded.size());
                        out_file.close();
                    }

                    reading_image = false;
                    width = 0;
                    height = 0;
                    rows.clear();
                } else if (reading_image)
                    row += gcode_line.substr(2);
            }
        }

        in_file.close();
    }
}
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG

#ifdef __APPLE__
// This callback is called from wxEntry()->wxApp::CallOnInit()->NSApplication run
// that is, before GUI_App::OnInit(), so we have a chance to switch GUI_App
// to a G-code viewer.
void GUI_App::OSXStoreOpenFiles(const wxArrayString &fileNames)
{
    wxApp::OSXStoreOpenFiles(fileNames);
}

void GUI_App::MacOpenURL(const wxString& url)
{
    if (url.empty())
        return;
    start_download(boost::nowide::narrow(url));
}

// wxWidgets override to get an event on open files.
void GUI_App::MacOpenFiles(const wxArrayString &fileNames)
{
    bool single_instance = app_config->get("app", "single_instance") == "true";
    if (m_post_initialized && !single_instance) {
        bool has3mf = false;
        std::vector<wxString> names;
        for (auto & n : fileNames) {
            has3mf |= n.EndsWith(".3mf");
            names.push_back(n);
        }
        if (has3mf) {
            start_new_slicer(names);
            return;
        }
    }
    std::vector<std::string> files;
    std::vector<wxString>    gcode_files;
    std::vector<wxString>    non_gcode_files;
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", open files, size " << fileNames.size();
    for (const auto& filename : fileNames) {
        if (is_gcode_file(into_u8(filename)))
            gcode_files.emplace_back(filename);
        else {
            files.emplace_back(into_u8(filename));
            non_gcode_files.emplace_back(filename);
        }
    }

    {
        if (! files.empty()) {
            if (m_post_initialized) {
                wxArrayString input_files;
                for (size_t i = 0; i < non_gcode_files.size(); ++i) {
                    input_files.push_back(non_gcode_files[i]);
                }
                this->plater()->load_files(input_files);
            } else {
                for (size_t i = 0; i < files.size(); ++i) {
                    this->init_params->input_files.emplace_back(files[i]);
                }
            }
        } else {
            if (m_post_initialized) {
                this->plater()->load_gcode(gcode_files.front());
            } else {
                this->init_params->input_gcode = true;
                this->init_params->input_files = { into_u8(gcode_files.front()) };
            }
        }
        /*for (const wxString &filename : gcode_files)
            start_new_gcodeviewer(&filename);*/
    }
}

#endif /* __APPLE */

}
}
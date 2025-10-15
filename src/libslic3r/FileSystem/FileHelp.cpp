#include "FileHelp.hpp"
#include "libslic3r/Utils.hpp"

using namespace std::literals;

namespace Slic3r {

namespace Utils {
bool is_file_too_large(std::string file_path, bool &try_ok)
{
    try {
        uintmax_t fileSizeBytes = boost::filesystem::file_size(file_path);
        double    fileSizeMB    = static_cast<double>(fileSizeBytes) / 1024 / 1024;
        try_ok                  = true;
        if (fileSizeMB > STL_SVG_MAX_FILE_SIZE_MB) { return true; }
    } catch (boost::filesystem::filesystem_error &e) {
        try_ok = false;
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " error message: " << e.what();
    }
    return false;
}

void slash_to_back_slash(std::string &file_path) {
    std::regex regex("\\\\");
    file_path = std::regex_replace(file_path, regex, "/");
}

bool load_file_content(const std::string& jPath, std::string& content)
{
    try {
        boost::nowide::ifstream t(jPath);
        std::stringstream buffer;
        buffer << t.rdbuf();
        content = buffer.str();
    }
    catch (std::exception &e)
    {
        return false;
    }

    return true;
}

void StrReplace(std::string &strBase, std::string strSrc, std::string strDes)
{
    int pos    = 0;
    int srcLen = strSrc.size();
    int desLen = strDes.size();
    pos = strBase.find(strSrc, pos);
    while ((pos != std::string::npos)) {
        strBase.replace(pos, srcLen, strDes);
        pos = strBase.find(strSrc, (pos + desLen));
    }
}

} // Utils

struct FileWildcards {
    std::string_view              title;
    std::vector<std::string_view> file_extensions;
};

static const FileWildcards file_wildcards_by_type[FT_SIZE] = {
    /* FT_STEP */    { "STEP files"sv,      { ".stp"sv, ".step"sv } },
    /* FT_STL */     { "STL files"sv,       { ".stl"sv } },
    /* FT_OBJ */     { "OBJ files"sv,       { ".obj"sv } },
    /* FT_AMF */     { "AMF files"sv,       { ".amf"sv, ".zip.amf"sv, ".xml"sv } },
    /* FT_3MF */     { "3MF files"sv,       { ".3mf"sv } },
    /* FT_GCODE_3MF */ {"Gcode 3MF files"sv, {".gcode.3mf"sv}},
    /* FT_GCODE */   { "G-code files"sv,    { ".gcode"sv} },
#ifdef __APPLE__
    /* FT_MODEL */
    {"Supported files"sv, {".3mf"sv, ".stl"sv, ".oltp"sv, ".stp"sv, ".step"sv, ".svg"sv, ".amf"sv, ".obj"sv, ".usd"sv, ".usda"sv, ".usdc"sv, ".usdz"sv, ".abc"sv, ".ply"sv}},
#else
    /* FT_MODEL */
    {"Supported files"sv, {".3mf"sv, ".stl"sv, ".oltp"sv, ".stp"sv, ".step"sv, ".svg"sv, ".amf"sv, ".obj"sv}},
#endif
    /* FT_ZIP */     { "ZIP files"sv,       { ".zip"sv } },
    /* FT_PROJECT */ { "Project files"sv,   { ".3mf"sv} },
    /* FT_GALLERY */ { "Known files"sv,     { ".stl"sv, ".obj"sv } },

    /* FT_INI */     { "INI files"sv,       { ".ini"sv } },
    /* FT_SVG */     { "SVG files"sv,       { ".svg"sv } },
    /* FT_TEX */     { "Texture"sv,         { ".png"sv, ".svg"sv } },
    /* FT_SL1 */     { "Masked SLA files"sv, { ".sl1"sv, ".sl1s"sv } },
};

// This function produces a Win32 file dialog file template mask to be consumed by wxWidgets on all platforms.
// The function accepts a custom extension parameter. If the parameter is provided, the custom extension
// will be added as a fist to the list. This is important for a "file save" dialog on OSX, which strips
// an extension from the provided initial file name and substitutes it with the default extension (the first one in the template).
std::string file_wildcards(FileType file_type, const std::string &custom_extension)
{
    const FileWildcards& data = file_wildcards_by_type[file_type];
    std::string title;
    std::string mask;
    std::string custom_ext_lower;

    if (! custom_extension.empty()) {
        // Generate an extension into the title mask and into the list of extensions.
        custom_ext_lower = boost::to_lower_copy(custom_extension);
        const std::string custom_ext_upper = boost::to_upper_copy(custom_extension);
        if (custom_ext_lower == custom_extension) {
            // Add a lower case version.
            title = std::string("*") + custom_ext_lower;
            mask = title;
            // Add an upper case version.
            mask  += ";*";
            mask  += custom_ext_upper;
        } else if (custom_ext_upper == custom_extension) {
            // Add an upper case version.
            title = std::string("*") + custom_ext_upper;
            mask = title;
            // Add a lower case version.
            mask += ";*";
            mask += custom_ext_lower;
        } else {
            // Add the mixed case version only.
            title = std::string("*") + custom_extension;
            mask = title;
        }
    }

    for (const std::string_view &ext : data.file_extensions)
        // Only add an extension if it was not added first as the custom extension.
        if (ext != custom_ext_lower) {
            if (title.empty()) {
                title = "*";
                title += ext;
                mask  = title;
            } else {
                title += ", *";
                title += ext;
                mask  += ";*";
                mask  += ext;
            }
            mask += ";*";
            mask += boost::to_upper_copy(std::string(ext));
        }
    return (boost::format("%1% (%2%)|%3%")%data.title%title%mask).str();
}

void read_binary_stl(const std::string& filename, std::string& model_id, std::string& code) 
{
    std::ifstream file( encode_path(filename.c_str()), std::ios::binary);
    if (!file) {
        return;
    }

    try {
        // Read the first 80 bytes
        char data[80];
        file.read(data, 80);
        if (!file) {
            file.close();
            return;
        }

        if (data[0] == '\0' || data[0] == ' ') {
            file.close();
            return;
        }

        char magic[2] = { data[0], data[1] };
        if (magic[0] != 'M' || magic[1] != 'W') {
            file.close();
            return;
        }

        if (data[2] != ' ') {
            file.close();
            return;
        }

        char protocol_version[3] = { data[3], data[4], data[5] };

        //version
        if (protocol_version[0] != '1' || protocol_version[1] != '.' || protocol_version[2] != '0') {
            file.close();
            return;
        }

        std::vector<char*> tokens;
        std::istringstream iss(data);
        std::string token;
        while (std::getline(iss, token, ' ')) {
            char* tokenPtr = new char[token.length() + 1];
            std::strcpy(tokenPtr, token.c_str());
            tokens.push_back(tokenPtr);
        }

        //model id
        if (tokens.size() < 4) {
            file.close();
            return;
        }

        model_id = tokens[2];
        code = tokens[3];
        file.close();
    }
    catch (...) {
    }
    return;
}

} // namespace Slic3r

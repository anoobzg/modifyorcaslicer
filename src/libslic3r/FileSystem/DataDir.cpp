#include "DataDir.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>

#if defined(_WIN32)
    #include <shlobj.h>
    #include <windows.h>
#elif defined(__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
    #include <sys/syslimits.h>
#else // Linux/Unix
    #include <cstdlib>
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
#endif

namespace Slic3r {
std::string os_user_data_dir(const char* folder_name)
{
    std::string path;

    // Windows
#if defined(_WIN32)
    PWSTR wpath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &wpath))) {
        char cpath[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, cpath, MAX_PATH, NULL, NULL);
        path = cpath;
        CoTaskMemFree(wpath);
    }
    
    // macOS
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    CFURLRef appSupportURL = CFBundleCopyApplicationSupportDirectory(CFBundleGetMainBundle());
    if (CFURLGetFileSystemRepresentation(appSupportURL, true, (UInt8*)buffer, PATH_MAX)) {
        path = buffer;
    }
    CFRelease(appSupportURL);
    
// Linux/Unix
#else
    const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
    if (xdgDataHome) {
        path = xdgDataHome;
    } else {
        const char* homeDir = std::getenv("HOME");
        if (!homeDir) {
            struct passwd* pw = getpwuid(getuid());
            homeDir = pw ? pw->pw_dir : "";
        }
        path = std::string(homeDir) + "/.local/share";
    }
#endif

    std::string real_path = (boost::format("%1%/%2%") % path % (folder_name ? folder_name : "os_user_data_dir")).str();
    return real_path;  
}

static std::string g_user_data_dir;

void set_user_data_dir(const std::string &dir)
{
    g_user_data_dir = dir;
    if (!g_user_data_dir.empty() && !boost::filesystem::exists(g_user_data_dir)) {
       boost::filesystem::create_directory(g_user_data_dir);
    }
}

const std::string& user_data_dir()
{
    return g_user_data_dir;
}

//BBS: add temporary dir
static std::string g_temporary_dir;
void set_temporary_dir(const std::string &dir)
{
    g_temporary_dir = dir;
}

const std::string& temporary_dir()
{
    return g_temporary_dir;
}

static std::string g_var_dir;

void set_var_dir(const std::string &dir)
{
    g_var_dir = dir;
}

const std::string& var_dir()
{
    return g_var_dir;
}

std::string var(const std::string &file_name)
{
    boost::system::error_code ec;
    if (boost::filesystem::exists(file_name, ec)) {
       return file_name;
    }

    auto file = (boost::filesystem::path(g_var_dir) / file_name).make_preferred();
    return file.string();
}

static std::string g_resources_dir;

void set_resources_dir(const std::string &dir)
{
    g_resources_dir = dir;
}

const std::string& resources_dir()
{
    return g_resources_dir;
}

static std::string g_local_dir;

void set_local_dir(const std::string &dir)
{
    g_local_dir = dir;
}

const std::string& localization_dir()
{
	return g_local_dir;
}

static std::string g_sys_shapes_dir;

void set_sys_shapes_dir(const std::string &dir)
{
    g_sys_shapes_dir = dir;
}

const std::string& sys_shapes_dir()
{
	return g_sys_shapes_dir;
}

static std::string g_custom_gcodes_dir;

void set_custom_gcodes_dir(const std::string &dir)
{
    g_custom_gcodes_dir = dir;
}

const std::string& custom_gcodes_dir()
{
    return g_custom_gcodes_dir;
}

std::string custom_shapes_dir()
{
    return (boost::filesystem::path(user_data_dir()) / "shapes").string();
}

}
#include "VersionInfo.hpp"

#include "libslic3r/libslic3r.h"

namespace Slic3r {
namespace GUI {

std::string VersionInfo::convert_full_version(std::string short_version)
{
    std::string result = "";
    std::vector<std::string> items;
    boost::split(items, short_version, boost::is_any_of("."));
    if (items.size() == VERSION_LEN) {
        for (int i = 0; i < VERSION_LEN; i++) {
            std::stringstream ss;
            ss << std::setw(2) << std::setfill('0') << items[i];
            result += ss.str();
            if (i != VERSION_LEN - 1)
                result += ".";
        }
        return result;
    }
    return result;
}

std::string VersionInfo::convert_short_version(std::string full_version)
{
    full_version.erase(std::remove(full_version.begin(), full_version.end(), '0'), full_version.end());
    return full_version;
}

std::string VersionInfo::get_full_version() 
{
    return convert_full_version(SLIC3R_VERSION);
}
}
}
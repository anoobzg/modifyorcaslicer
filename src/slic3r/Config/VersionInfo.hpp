#pragma once
#include <boost/algorithm/string.hpp>
namespace Slic3r {
namespace GUI {

#define  VERSION_LEN    4
class VersionInfo
{
public:
    std::string version_str;
    std::string version_name;
    std::string description;
    std::string url;
    bool        force_upgrade{ false };
    int      ver_items[VERSION_LEN];  // AA.BB.CC.DD
    VersionInfo() {
        for (int i = 0; i < VERSION_LEN; i++) {
            ver_items[i] = 0;
        }
        force_upgrade = false;
        version_str = "";
    }

    void parse_version_str(std::string str) {
        version_str = str;
        std::vector<std::string> items;
        boost::split(items, str, boost::is_any_of("."));
        if (items.size() == VERSION_LEN) {
            try {
                for (int i = 0; i < VERSION_LEN; i++) {
                    ver_items[i] = stoi(items[i]);
                }
            }
            catch (...) {
                ;
            }
        }
    }
    static std::string convert_full_version(std::string short_version);
    static std::string convert_short_version(std::string full_version);
    static std::string get_full_version();

    /* return > 0, need update */
    int compare(std::string ver_str) {
        if (version_str.empty()) return -1;

        int      ver_target[VERSION_LEN];
        std::vector<std::string> items;
        boost::split(items, ver_str, boost::is_any_of("."));
        if (items.size() == VERSION_LEN) {
            try {
                for (int i = 0; i < VERSION_LEN; i++) {
                    ver_target[i] = stoi(items[i]);
                    if (ver_target[i] < ver_items[i]) {
                        return 1;
                    }
                    else if (ver_target[i] == ver_items[i]) {
                        continue;
                    }
                    else {
                        return -1;
                    }
                }
            }
            catch (...) {
                return -1;
            }
        }
        return -1;
    }
};

}
}
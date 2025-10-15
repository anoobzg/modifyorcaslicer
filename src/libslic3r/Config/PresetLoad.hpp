#pragma once 
#include <nlohmann/json.hpp>

namespace Slic3r {
    int get_filament_info(const std::string& VendorDirectory, const nlohmann::json& pFilaList, const std::string& filepath, std::string &sVendor, std::string &sType, const std::string& default_path = "");
    int load_profile_family(const std::string& vendor, const std::string& profile_file, nlohmann::json& json);
}
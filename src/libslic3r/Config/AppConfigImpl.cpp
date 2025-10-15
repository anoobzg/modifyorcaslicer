#include "libslic3r/AppConfig.hpp"

#define ENV_DEV_HOST		"dev_host"
#define ENV_QAT_HOST		"qa_host"
#define ENV_PRE_HOST		"pre_host"
#define ENV_PRODUCT_HOST	"product_host"

namespace Slic3r {

bool AppConfig::is_engineering_region(){
    std::string sel = get("iot_environment");
    std::string region;
    if (sel == ENV_DEV_HOST
        || sel == ENV_QAT_HOST
        ||sel == ENV_PRE_HOST)
        return true;
    return false;
}

void AppConfig::set_iot_environment(const std::string& value)
{
    std::string sel = value;
    if (sel != ENV_DEV_HOST && sel != ENV_QAT_HOST && sel != ENV_PRE_HOST && sel != ENV_PRODUCT_HOST) {
        sel = ENV_DEV_HOST;
    }

    this->set("iot_environment", sel);
}

std::string AppConfig::get_iot_environment() const
{
    std::string sel = get("iot_environment");
    if (sel != ENV_DEV_HOST && sel != ENV_QAT_HOST && sel != ENV_PRE_HOST && sel != ENV_PRODUCT_HOST) {
        sel = ENV_DEV_HOST;
    }
    return sel;
}

bool AppConfig::is_pre_host() const
{
    std::string sel = get("iot_environment");
    return sel == ENV_PRE_HOST;
}

}
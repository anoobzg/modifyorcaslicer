#include "LogUtil.hpp"
#include <boost/log/trivial.hpp>

#include "libslic3r/Base/Platform.hpp"

namespace Slic3r {
void log_platform_flavor() {
    BOOST_LOG_TRIVIAL(info) << "Application will starting up.";

    BOOST_LOG_TRIVIAL(info) << "Platform: " << platform_to_string(platform())
                            << ", Flavor: " << platform_flavor_to_string(platform_flavor());
}
}

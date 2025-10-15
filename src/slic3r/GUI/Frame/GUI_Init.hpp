#ifndef slic3r_GUI_Init_hpp_
#define slic3r_GUI_Init_hpp_

#include "framework_interface.h"
#include "libslic3r/PrintConfig.hpp"
#include <boost/filesystem.hpp>

namespace Slic3r {

namespace GUI {

int GUI_Init();

class CLI {
public:
    int run(const boost::filesystem::path& resource_path = "");

private:
    DynamicPrintAndCLIConfig    m_config;
    bool setup(const boost::filesystem::path& resource_path = "") ;
};


} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GUI_Init_hpp_

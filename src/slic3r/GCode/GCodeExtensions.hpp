#ifndef _slic3r_GCodeExtensions_hpp_
#define _slic3r_GCodeExtensions_hpp_

#include "GCodeDefine.hpp"

namespace Slic3r {
namespace GUI {
namespace GCode {

class GCodeViewerData;

void export_toolpaths_to_obj(const GCodeViewerData* data, const char* filename);
void log_memory_used(const GCodeViewerData* data, const std::string& label, int64_t additional);

};
};
};

#endif
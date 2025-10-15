#pragma once 
#include "libslic3r/TriangleMesh.hpp"

namespace Slic3r {
    class ModelObject;
    TriangleMesh combine_mesh_fff(const ModelObject& mo, int instance_id, std::function<void(const std::string&)> notify_func);

    TriangleMesh mesh_to_export_fff_no_boolean(const ModelObject &mo, int instance_id);
}
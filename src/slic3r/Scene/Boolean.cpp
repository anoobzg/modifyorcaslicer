#include "Boolean.hpp"
#include "libslic3r/Model.hpp"

// For stl export
#include "libslic3r/CSGMesh/ModelToCSGMesh.hpp"
#include "libslic3r/CSGMesh/PerformCSGMeshBooleans.hpp"

#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"

#include "slic3r/Scene/NotificationManager.hpp"

namespace Slic3r {

// Following lambda generates a combined mesh for export with normals pointing outwards.
TriangleMesh combine_mesh_fff(const ModelObject& mo, int instance_id, std::function<void(const std::string&)> notify_func)
{
    TriangleMesh mesh;

    std::vector<csg::CSGPart> csgmesh;
    csgmesh.reserve(2 * mo.volumes.size());
    bool has_splitable_volume = csg::model_to_csgmesh(mo, Transform3d::Identity(), std::back_inserter(csgmesh),
        csg::mpartsPositive | csg::mpartsNegative);
        
    std::string fail_msg = _u8L("Unable to perform boolean operation on model meshes. "
        "Only positive parts will be kept. You may fix the meshes and try again.");
    if (auto fail_reason_name = csg::check_csgmesh_booleans(Range{ std::begin(csgmesh), std::end(csgmesh) }); std::get<0>(fail_reason_name) != csg::BooleanFailReason::OK) {
        std::string name = std::get<1>(fail_reason_name);
        std::map<csg::BooleanFailReason, std::string> fail_reasons = {
            {csg::BooleanFailReason::OK, "OK"},
            {csg::BooleanFailReason::MeshEmpty, Slic3r::format( _u8L("Reason: part \"%1%\" is empty."), name)},
            {csg::BooleanFailReason::NotBoundAVolume, Slic3r::format(_u8L("Reason: part \"%1%\" does not bound a volume."), name)},
            {csg::BooleanFailReason::SelfIntersect, Slic3r::format(_u8L("Reason: part \"%1%\" has self intersection."), name)},
            {csg::BooleanFailReason::NoIntersection, Slic3r::format(_u8L("Reason: \"%1%\" and another part have no intersection."), name)} };
        fail_msg += " " + fail_reasons[std::get<0>(fail_reason_name)];
    }
    else {
        try {
            MeshBoolean::mcut::McutMeshPtr meshPtr = csg::perform_csgmesh_booleans_mcut(Range{ std::begin(csgmesh), std::end(csgmesh) });
            mesh = MeshBoolean::mcut::mcut_to_triangle_mesh(*meshPtr);
        }
        catch (...) {}
#if 0
        // if mcut fails, try again with CGAL
        if (mesh.empty()) {
            try {
                auto meshPtr = csg::perform_csgmesh_booleans(Range{ std::begin(csgmesh), std::end(csgmesh) });
                mesh = MeshBoolean::cgal::cgal_to_triangle_mesh(*meshPtr);
                }
            catch (...) {}
        }
#endif
    }

    if (mesh.empty()) {
        if (notify_func)
            notify_func(fail_msg);

        for (const ModelVolume* v : mo.volumes)
            if (v->is_model_part()) {
                TriangleMesh vol_mesh(v->mesh());
                vol_mesh.transform(v->get_matrix(), true);
                mesh.merge(vol_mesh);
            }
    }

    if (instance_id == -1) {
        TriangleMesh vols_mesh(mesh);
        mesh = TriangleMesh();
        for (const ModelInstance* i : mo.instances) {
            TriangleMesh m = vols_mesh;
            m.transform(i->get_matrix(), true);
            mesh.merge(m);
        }
    }
    else if (0 <= instance_id && instance_id < int(mo.instances.size()))
        mesh.transform(mo.instances[instance_id]->get_matrix(), true);
    return mesh;
}

// Following lambda generates a combined mesh for export with normals pointing outwards.
TriangleMesh mesh_to_export_fff_no_boolean(const ModelObject &mo, int instance_id) 
{
    TriangleMesh mesh;

    //Prusa export negative parts
    std::vector<csg::CSGPart> csgmesh;
    csgmesh.reserve(2 * mo.volumes.size());
    csg::model_to_csgmesh(mo, Transform3d::Identity(), std::back_inserter(csgmesh),
                            csg::mpartsPositive | csg::mpartsNegative | csg::mpartsDoSplits);

    auto csgrange = range(csgmesh);
    if (csg::is_all_positive(csgrange)) {
        mesh = TriangleMesh{csg::csgmesh_merge_positive_parts(csgrange)};
    } else if (std::get<2>(csg::check_csgmesh_booleans(csgrange)) == csgrange.end()) {
        try {
            auto cgalm = csg::perform_csgmesh_booleans(csgrange);
            mesh = MeshBoolean::cgal::cgal_to_triangle_mesh(*cgalm);
        } catch (...) {}
    }

    if (mesh.empty()) {
        GUI::get_notification_manager()->push_plater_error_notification(
            _u8L("Unable to perform boolean operation on model meshes. "
                    "Only positive parts will be exported."));

        for (const ModelVolume* v : mo.volumes)
            if (v->is_model_part()) {
                TriangleMesh vol_mesh(v->mesh());
                vol_mesh.transform(v->get_matrix(), true);
                mesh.merge(vol_mesh);
            }
    }
    if (instance_id == -1) {
        TriangleMesh vols_mesh(mesh);
        mesh = TriangleMesh();
        for (const ModelInstance *i : mo.instances) {
            TriangleMesh m = vols_mesh;
            m.transform(i->get_matrix(), true);
            mesh.merge(m);
        }
    } else if (0 <= instance_id && instance_id < int(mo.instances.size()))
        mesh.transform(mo.instances[instance_id]->get_matrix(), true);
    return mesh;
}

}
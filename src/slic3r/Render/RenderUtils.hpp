#pragma once
#include "libslic3r/Model.hpp"
#include "slic3r/Scene/Camera.hpp"
#include "slic3r/Render/3DScene.hpp"

namespace Slic3r {
namespace GUI {
class Selection;
class GLCanvas3DFacade;
class PartPlateList;

const ModelVolume *get_model_volume(const GLVolume &v, const Model &model);
ModelVolume *get_model_volume(const ObjectID &volume_id, const ModelObjectPtrs &objects);
ModelVolume *get_model_volume(const GLVolume &v, const ModelObjectPtrs &objects);
ModelVolume *get_model_volume(const GLVolume &v, const ModelObject &object);

GLVolume *get_first_hovered_gl_volume(const GLCanvas3DFacade* canvas);
GLVolume *get_selected_gl_volume(GLCanvas3DFacade* canvas);

ModelObject *get_model_object(const GLVolume &gl_volume, const Model &model);
ModelObject *get_model_object(const GLVolume &gl_volume, const ModelObjectPtrs &objects);

ModelInstance *get_model_instance(const GLVolume &gl_volume, const Model &model);
ModelInstance *get_model_instance(const GLVolume &gl_volume, const ModelObjectPtrs &objects);
ModelInstance *get_model_instance(const GLVolume &gl_volume, const ModelObject &object);

ModelVolume    *get_selected_volume   (const Selection *selection);
const GLVolume *get_selected_gl_volume(const Selection *selection);

ModelVolume    *get_selected_volume   (const ObjectID &volume_id, const Selection *selection);
ModelVolume    *get_volume            (const ObjectID &volume_id, const Selection *selection);

double calc_zoom_to_volumes_factor(Camera& camera, const GLVolumePtrs& volumes, Vec3d& center, double margin_factor);
void zoom_to_volumes(Camera& camera, const GLVolumePtrs& volumes, double margin_factor = Camera::DefaultZoomToVolumesMarginFactor);

void apply_viewport(const Camera& camera);

#if ENABLE_CAMERA_STATISTICS
void debug_render_camera(const Camera& camera);
#endif // ENABLE_CAMERA_STATISTICS

/// <summary>
/// Create hull around GLVolume in 2d space of camera
/// </summary>
/// <param name="camera">Projection params</param>
/// <param name="volume">Outline by 3d object</param>
/// <returns>Polygon around object</returns>
Polygon create_hull2d(const Camera &camera, const GLVolume &volume);

void render_thumbnail_internal(ThumbnailData& thumbnail_data, const ThumbnailsParams& thumbnail_params, PartPlateList& partplate_list, ModelObjectPtrs& model_objects,
    const GLVolumeCollection& volumes, std::vector<ColorRGBA>& extruder_colors,
    GLShaderProgram* shader, Camera::EType camera_type, bool use_top_view = false, bool for_picking = false, bool ban_light = false);

// render thumbnail using an off-screen framebuffer
void render_thumbnail_framebuffer(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params,
    PartPlateList& partplate_list, ModelObjectPtrs& model_objects, const GLVolumeCollection& volumes, std::vector<ColorRGBA>& extruder_colors,
    GLShaderProgram* shader, Camera::EType camera_type, bool use_top_view = false, bool for_picking = false, bool ban_light = false);
// render thumbnail using an off-screen framebuffer when GLEW_EXT_framebuffer_object is supported
void render_thumbnail_framebuffer_ext(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params,
    PartPlateList& partplate_list, ModelObjectPtrs& model_objects, const GLVolumeCollection& volumes, std::vector<ColorRGBA>& extruder_colors,
    GLShaderProgram* shader, Camera::EType camera_type, bool use_top_view = false, bool for_picking = false, bool ban_light = false);

namespace GCode {
class GCodeViewerData;
}

void render_calibration_thumbnail_framebuffer(GCode::GCodeViewerData& data, ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const BoundingBoxf3& box);
void render_calibration_thumbnail_internal(GCode::GCodeViewerData& data, ThumbnailData& thumbnail_data, const BoundingBoxf3& box);
}
}
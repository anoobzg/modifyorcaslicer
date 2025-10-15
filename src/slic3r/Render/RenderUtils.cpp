#include "RenderUtils.hpp"

#include "libslic3r/Geometry/ConvexHull.hpp"
#include "slic3r/Render/AppRender.hpp"
#include "slic3r/Scene/CameraUtils.hpp"
#include "slic3r/GCode/GCodeViewerData.hpp"

#if ENABLE_CAMERA_STATISTICS
#include "Mouse3DController.hpp"
#endif // ENABLE_CAMERA_STATISTICS
#include "slic3r/GUI/Frame/Plater.hpp"
#include "slic3r/Scene/PartPlate.hpp"
#include "slic3r/Scene/PartPlateList.hpp"
#include "slic3r/Render/GLCanvas3DFacade.hpp"

namespace Slic3r {
namespace GUI {

    const ModelVolume *get_model_volume(const GLVolume &v, const Model &model)
{
    const ModelVolume * ret = nullptr;

    if (v.object_idx() < (int)model.objects.size()) {
        const ModelObject *obj = model.objects[v.object_idx()];
        if (v.volume_idx() < (int)obj->volumes.size())
            ret = obj->volumes[v.volume_idx()];
    }

    return ret;
}

ModelVolume *get_model_volume(const ObjectID &volume_id, const ModelObjectPtrs &objects)
{
    for (const ModelObject *obj : objects)
        for (ModelVolume *vol : obj->volumes)
            if (vol->id() == volume_id)
                return vol;
    return nullptr;
}

ModelVolume *get_model_volume(const GLVolume &v, const ModelObject& object) {
    if (v.volume_idx() < 0)
        return nullptr;

    size_t volume_idx = static_cast<size_t>(v.volume_idx());
    if (volume_idx >= object.volumes.size())
        return nullptr;

    return object.volumes[volume_idx];
}

ModelVolume *get_model_volume(const GLVolume &v, const ModelObjectPtrs &objects)
{
    if (v.object_idx() < 0)
        return nullptr;
    size_t objext_idx = static_cast<size_t>(v.object_idx());
    if (objext_idx >= objects.size())
        return nullptr;
    if (objects[objext_idx] == nullptr)
        return nullptr;
    return get_model_volume(v, *objects[objext_idx]);
}

GLVolume *get_first_hovered_gl_volume(const GLCanvas3DFacade* canvas) {
    int hovered_id_signed = canvas->get_first_hover_volume_idx();
    if (hovered_id_signed < 0)
        return nullptr;

    size_t hovered_id = static_cast<size_t>(hovered_id_signed);
    const GLVolumePtrs &volumes = canvas->get_volumes().volumes;
    if (hovered_id >= volumes.size())
        return nullptr;

    return volumes[hovered_id];
}

GLVolume *get_selected_gl_volume(GLCanvas3DFacade* canvas) {
    const Selection* selection = canvas->get_selection();
    const GLVolume *gl_volume = get_selected_gl_volume(selection);
    if (gl_volume == nullptr)
        return nullptr;

    const GLVolumePtrs &gl_volumes = canvas->get_volumes().volumes;
    for (GLVolume *v : gl_volumes)
        if (v->composite_id == gl_volume->composite_id)
            return v;
    return nullptr;
}

ModelObject *get_model_object(const GLVolume &gl_volume, const Model &model) {
    return get_model_object(gl_volume, model.objects);
}

ModelObject *get_model_object(const GLVolume &gl_volume, const ModelObjectPtrs &objects) {
    if (gl_volume.object_idx() < 0)
        return nullptr;
    size_t objext_idx = static_cast<size_t>(gl_volume.object_idx());
    if (objext_idx >= objects.size())
        return nullptr;
    return objects[objext_idx];
}

ModelInstance *get_model_instance(const GLVolume &gl_volume, const Model& model) {
    return get_model_instance(gl_volume, model.objects);
}

ModelInstance *get_model_instance(const GLVolume &gl_volume, const ModelObjectPtrs &objects) {
    if (gl_volume.instance_idx() < 0)
        return nullptr;
    ModelObject *object = get_model_object(gl_volume, objects);
    return get_model_instance(gl_volume, *object);
}

ModelInstance *get_model_instance(const GLVolume &gl_volume, const ModelObject &object) {
    if (gl_volume.instance_idx() < 0)
        return nullptr;
    size_t instance_idx = static_cast<size_t>(gl_volume.instance_idx());
    if (instance_idx >= object.instances.size())
        return nullptr;
    return object.instances[instance_idx];
}

ModelVolume *get_selected_volume(const Selection *selection)
{
    const GLVolume *gl_volume = get_selected_gl_volume(selection);
    if (gl_volume == nullptr)
        return nullptr;
    const ModelObjectPtrs &objects = selection->get_model()->objects;
    return get_model_volume(*gl_volume, objects);
}

const GLVolume *get_selected_gl_volume(const Selection *selection)
{
    int object_idx = selection->get_object_idx();
    // is more object selected?
    if (object_idx == -1)
        return nullptr;

    const auto &list = selection->get_volume_idxs();
    // is more volumes selected?
    if (list.size() != 1)
        return nullptr;

    unsigned int volume_idx = *list.begin();
    return selection->get_volume(volume_idx);
}

ModelVolume *get_selected_volume(const ObjectID &volume_id, const Selection *selection) {
    const Selection::IndicesList &volume_ids = selection->get_volume_idxs();
    const ModelObjectPtrs &model_objects     = selection->get_model()->objects;
    for (auto id : volume_ids) {
        const GLVolume *selected_volume = selection->get_volume(id);
        const GLVolume::CompositeID &cid = selected_volume->composite_id;
        ModelObject *obj    = model_objects[cid.object_id];
        ModelVolume *volume = obj->volumes[cid.volume_id];
        if (volume_id == volume->id())
            return volume;
    }
    return nullptr;
}

ModelVolume *get_volume(const ObjectID &volume_id, const Selection *selection) {
    const ModelObjectPtrs &objects = selection->get_model()->objects;
    for (const ModelObject *object : objects) {
        for (ModelVolume *volume : object->volumes) {
            if (volume->id() == volume_id)
                return volume;
        }        
    }
    return nullptr;
}

double calc_zoom_to_volumes_factor(Camera& camera, const GLVolumePtrs& volumes, Vec3d& center, double margin_factor)
{
    if (volumes.empty())
        return -1.0;

    // project the volumes vertices on a plane perpendicular to the camera forward axis
    // then calculates the vertices coordinate on this plane along the camera xy axes

    const Vec3d right = camera.get_dir_right();
    const Vec3d up = camera.get_dir_up();
    const Vec3d forward = camera.get_dir_forward();

    BoundingBoxf3 box;
    for (const GLVolume* volume : volumes) {
        box.merge(volume->transformed_bounding_box());
    }
    center = box.center();

    double min_x = DBL_MAX;
    double min_y = DBL_MAX;
    double max_x = -DBL_MAX;
    double max_y = -DBL_MAX;

    for (const GLVolume* volume : volumes) {
        const Transform3d& transform = volume->world_matrix();
        const TriangleMesh* hull = volume->convex_hull();
        if (hull == nullptr)
            continue;

        for (const Vec3f& vertex : hull->its.vertices) {
            const Vec3d v = transform * vertex.cast<double>();

            // project vertex on the plane perpendicular to camera forward axis
            const Vec3d pos = v - center;
            const Vec3d proj_on_plane = pos - pos.dot(forward) * forward;

            // calculates vertex coordinate along camera xy axes
            const double x_on_plane = proj_on_plane.dot(right);
            const double y_on_plane = proj_on_plane.dot(up);

            min_x = std::min(min_x, x_on_plane);
            min_y = std::min(min_y, y_on_plane);
            max_x = std::max(max_x, x_on_plane);
            max_y = std::max(max_y, y_on_plane);
        }
    }

    center += 0.5 * (max_x + min_x) * right + 0.5 * (max_y + min_y) * up;

    const double dx = margin_factor * (max_x - min_x);
    const double dy = margin_factor * (max_y - min_y);

    if (dx <= 0.0 || dy <= 0.0)
        return -1.0f;

    const std::array<int, 4>& viewports = camera.get_viewport();
    return std::min((double)viewports[2] / dx, (double)viewports[3] / dy);
}

void zoom_to_volumes(Camera& camera, const GLVolumePtrs& volumes, double margin_factor)
{
    Vec3d center;
    const double zoom = calc_zoom_to_volumes_factor(camera, volumes, center, margin_factor);
    if (zoom > 0.0) {
        camera.set_zoom(zoom);
        // center view around the calculated center
        camera.set_target(center);
    }
}

void apply_viewport(const Camera& camera)
{
    const std::array<int, 4>& viewports = camera.get_viewport();
    glsafe(::glViewport(viewports[0], viewports[1], viewports[2], viewports[3]));
}

#if ENABLE_CAMERA_STATISTICS
void debug_render_camera(const Camera& camera)
{
    ImGuiWrapper& imgui = global_im_gui();
    imgui.begin(std::string("Camera statistics"), ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    std::string type = get_type_as_string();
    if (AppAdapter::plater()->get_mouse3d_controller().connected()
        || (app_get_bool("use_free_camera"))
        )
        type += "/free";
    else
        type += "/constrained";

    Vec3f position = get_position().cast<float>();
    Vec3f target = m_target.cast<float>();
    float distance = (float)get_distance();
    float zenit = (float)m_zenit;
    Vec3f forward = get_dir_forward().cast<float>();
    Vec3f right = get_dir_right().cast<float>();
    Vec3f up = get_dir_up().cast<float>();
    float nearZ = (float)m_frustrum_zs.first;
    float farZ = (float)m_frustrum_zs.second;
    float deltaZ = farZ - nearZ;
    float zoom = (float)m_zoom;
    float fov = (float)get_fov();
    std::array<int, 4>viewport = get_viewport();
    float gui_scale = (float)get_gui_scale();

    ImGui::InputText("Type", type.data(), type.length(), ImGuiInputTextFlags_ReadOnly);
    ImGui::Separator();
    ImGui::InputFloat3("Position", position.data(), "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::InputFloat3("Target", target.data(), "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::InputFloat("Distance", &distance, 0.0f, 0.0f, "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::Separator();
    ImGui::InputFloat("Zenit", &zenit, 0.0f, 0.0f, "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::Separator();
    ImGui::InputFloat3("Forward", forward.data(), "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::InputFloat3("Right", right.data(), "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::InputFloat3("Up", up.data(), "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::Separator();
    ImGui::InputFloat("Near Z", &nearZ, 0.0f, 0.0f, "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::InputFloat("Far Z", &farZ, 0.0f, 0.0f, "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::InputFloat("Delta Z", &deltaZ, 0.0f, 0.0f, "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::Separator();
    ImGui::InputFloat("Zoom", &zoom, 0.0f, 0.0f, "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::InputFloat("Fov", &fov, 0.0f, 0.0f, "%.6f", ImGuiInputTextFlags_ReadOnly);
    ImGui::Separator();
    ImGui::InputInt4("Viewport", viewport.data(), ImGuiInputTextFlags_ReadOnly);
    ImGui::Separator();
    ImGui::InputFloat("GUI scale", &gui_scale, 0.0f, 0.0f, "%.6f", ImGuiInputTextFlags_ReadOnly);
    imgui.end();
}
#endif // ENABLE_CAMERA_STATISTICS

Slic3r::Polygon create_hull2d(const Camera &  camera,
                                   const GLVolume &volume)
{
    std::vector<Vec3d>  vertices;
    const TriangleMesh *hull = volume.convex_hull();
    if (hull != nullptr) {
        const indexed_triangle_set &its = hull->its;        
        vertices.reserve(its.vertices.size());
        // cast vector
        for (const Vec3f &vertex : its.vertices)
            vertices.emplace_back(vertex.cast<double>());
    } else {
        // Negative volume doesn't have convex hull so use bounding box
        auto bb = volume.bounding_box();
        Vec3d &min = bb.min;
        Vec3d &max = bb.max;
        vertices   = {min,
                    Vec3d(min.x(), min.y(), max.z()),
                    Vec3d(min.x(), max.y(), min.z()),
                    Vec3d(min.x(), max.y(), max.z()),
                    Vec3d(max.x(), min.y(), min.z()),
                    Vec3d(max.x(), min.y(), max.z()),
                    Vec3d(max.x(), max.y(), min.z()),
                    max};
    }

    const Transform3d &trafoMat =
        volume.get_instance_transformation().get_matrix() *
        volume.get_volume_transformation().get_matrix();
    for (Vec3d &vertex : vertices)
        vertex = trafoMat * vertex.cast<double>();

    Points vertices_2d = CameraUtils::project(camera, vertices);
    return Geometry::convex_hull(vertices_2d);
}

void render_thumbnail_internal(ThumbnailData& thumbnail_data, const ThumbnailsParams& thumbnail_params,
    PartPlateList& partplate_list, ModelObjectPtrs& model_objects, const GLVolumeCollection& volumes, std::vector<ColorRGBA>& extruder_colors,
    GLShaderProgram* shader, Camera::EType camera_type, bool use_top_view, bool for_picking, bool ban_light)
{
    //BBS modify visible calc function
    int plate_idx = thumbnail_params.plate_id;
    PartPlate* plate = partplate_list.get_plate(plate_idx);
    BoundingBoxf3 plate_build_volume = plate->get_build_volume();
    plate_build_volume.min(0) -= Slic3r::BuildVolume::SceneEpsilon;
    plate_build_volume.min(1) -= Slic3r::BuildVolume::SceneEpsilon;
    plate_build_volume.min(2) -= Slic3r::BuildVolume::SceneEpsilon;
    plate_build_volume.max(0) += Slic3r::BuildVolume::SceneEpsilon;
    plate_build_volume.max(1) += Slic3r::BuildVolume::SceneEpsilon;
    plate_build_volume.max(2) += Slic3r::BuildVolume::SceneEpsilon;

    auto is_visible = [plate_idx, plate_build_volume](const GLVolume& v) {
        bool ret = v.printable;
        if (plate_idx >= 0) {
            bool contained = false;
            BoundingBoxf3 plate_bbox = plate_build_volume;
            plate_bbox.min(2) = -1e10;
            const BoundingBoxf3& volume_bbox = v.transformed_convex_hull_bounding_box();
            if (plate_bbox.contains(volume_bbox) && (volume_bbox.max(2) > 0)) {
                contained = true;
            }
            ret &= contained;
        }
        else {
            ret &= (!v.shader_outside_printer_detection_enabled || !v.is_outside);
        }
        return ret;
    };

    static ColorRGBA curr_color;

    GLVolumePtrs visible_volumes;

    for (GLVolume* vol : volumes.volumes) {
        if (!vol->is_modifier && !vol->is_wipe_tower && (!thumbnail_params.parts_only || vol->composite_id.volume_id >= 0)) {
            if (is_visible(*vol)) {
                visible_volumes.emplace_back(vol);
            }
        }
    }

    BOOST_LOG_TRIVIAL(info) << boost::format("render_thumbnail: plate_idx %1% volumes size %2%, shader %3%, use_top_view=%4%, for_picking=%5%") % plate_idx % visible_volumes.size() %shader %use_top_view %for_picking;
    //BoundingBoxf3 volumes_box = plate_build_volume;
    BoundingBoxf3 volumes_box;
    volumes_box.min.z() = 0;
    volumes_box.max.z() = 0;
    if (!visible_volumes.empty()) {
        for (const GLVolume* vol : visible_volumes) {
            volumes_box.merge(vol->transformed_bounding_box());
        }
    }
    volumes_box.min.z() = -Slic3r::BuildVolume::SceneEpsilon;
    double width = volumes_box.max.x() - volumes_box.min.x();
    double depth = volumes_box.max.y() - volumes_box.min.y();
    double height = volumes_box.max.z() - volumes_box.min.z();
    volumes_box.max.x() = volumes_box.max.x() + width * 0.01f;
    volumes_box.min.x() = volumes_box.min.x() - width * 0.01f;
    volumes_box.max.y() = volumes_box.max.y() + depth * 0.01f;
    volumes_box.min.y() = volumes_box.min.y() - depth * 0.01f;
    volumes_box.max.z() = volumes_box.max.z() + height * 0.01f;
    volumes_box.min.z() = volumes_box.min.z() - height * 0.01f;

    Camera camera;
    camera.set_type(camera_type);
    camera.set_scene_box(plate_build_volume);
    camera.set_viewport(0, 0, thumbnail_data.width, thumbnail_data.height);
    apply_viewport(camera);

    if (use_top_view) {
        float center_x = (plate_build_volume.max(0) + plate_build_volume.min(0))/2;
        float center_y = (plate_build_volume.max(1) + plate_build_volume.min(1))/2;
        float distance_z = plate_build_volume.max(2) - plate_build_volume.min(2);
        Vec3d center(center_x, center_y, 0.f);
        double zoom_ratio, scale_x, scale_y;

        scale_x = ((double)thumbnail_data.width)/(plate_build_volume.max(0) - plate_build_volume.min(0));
        scale_y = ((double)thumbnail_data.height)/(plate_build_volume.max(1) - plate_build_volume.min(1));
        zoom_ratio = (scale_x <= scale_y)?scale_x:scale_y;
        camera.look_at(center + distance_z * Vec3d::UnitZ(), center, Vec3d::UnitY());
        camera.set_zoom(zoom_ratio);
        //camera.select_view("top");
    }
    else {
        camera.select_view("iso");
        camera.zoom_to_box(volumes_box);
    }

    const Transform3d &view_matrix = camera.get_view_matrix();

    camera.apply_projection(plate_build_volume);

    if (!for_picking && (shader == nullptr)) {
        BOOST_LOG_TRIVIAL(info) <<  boost::format("render_thumbnail with no picking: shader is null, return directly");
        return;
    }

    glsafe(::glClearColor(0.f, 0.f, 0.f, 0.f));


    glsafe(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    glsafe(::glEnable(GL_DEPTH_TEST));
    if (ban_light) {
        glsafe(::glDisable(GL_BLEND));
    }
    const Transform3d &projection_matrix = camera.get_projection_matrix();

    if (for_picking) {
        shader->start_using();

        glsafe(::glDisable(GL_BLEND));

        static const GLfloat INV_255 = 1.0f / 255.0f;

        // do not cull backfaces to show broken geometry, if any
        glsafe(::glDisable(GL_CULL_FACE));

        for (GLVolume* vol : visible_volumes) {
            unsigned int id = vol->model_object_ID;
            //unsigned int id = 1 + volume.second.first;
            unsigned int r = (id & (0x000000FF << 0)) >> 0;
            unsigned int g = (id & (0x000000FF << 8)) >> 8;
            unsigned int b = (id & (0x000000FF << 16)) >> 16;
            unsigned int a = 0xFF;
            vol->model.set_color({(GLfloat)r * INV_255, (GLfloat)g * INV_255, (GLfloat)b * INV_255, (GLfloat)a * INV_255});

            const bool is_active = vol->is_active;
            vol->is_active = true;
            const Transform3d model_matrix = vol->world_matrix();
            shader->set_uniform("view_model_matrix", view_matrix * model_matrix);
            shader->set_uniform("projection_matrix", projection_matrix);
            vol->simple_render(shader, model_objects, extruder_colors);
            vol->is_active = is_active;
        }

        glsafe(::glEnable(GL_CULL_FACE));
    }
    else {
        shader->start_using();
        shader->set_uniform("emission_factor", 0.1f);
        shader->set_uniform("ban_light", ban_light);
        for (GLVolume* vol : visible_volumes) {
            //BBS set render color for thumbnails
            curr_color = vol->color;

            ColorRGBA new_color = adjust_color_for_rendering(curr_color);
            if (ban_light) {
                new_color[3] =(255 - (vol->extruder_id -1))/255.0f;
            }
            vol->model.set_color(new_color);
            shader->set_uniform("volume_world_matrix", vol->world_matrix());

            // the volume may have been deactivated by an active gizmo
            const bool is_active = vol->is_active;
            vol->is_active = true;
            const Transform3d model_matrix = vol->world_matrix();
            shader->set_uniform("view_model_matrix", view_matrix * model_matrix);
            shader->set_uniform("projection_matrix", projection_matrix);
            const Matrix3d view_normal_matrix = view_matrix.matrix().block(0, 0, 3, 3) * model_matrix.matrix().block(0, 0, 3, 3).inverse().transpose();
            shader->set_uniform("view_normal_matrix", view_normal_matrix); 
            vol->simple_render(shader,  model_objects, extruder_colors, ban_light);
            vol->is_active = is_active;
        }
        shader->stop_using();
    }

    glsafe(::glDisable(GL_DEPTH_TEST));

    BOOST_LOG_TRIVIAL(info) << boost::format("render_thumbnail: finished");
}

void render_thumbnail_framebuffer(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params,
    PartPlateList& partplate_list, ModelObjectPtrs& model_objects, const GLVolumeCollection& volumes, std::vector<ColorRGBA>& extruder_colors,
    GLShaderProgram* shader, Camera::EType camera_type, bool use_top_view, bool for_picking, bool ban_light)
{
    thumbnail_data.set(w, h);
    if (!thumbnail_data.is_valid())
        return;

    bool multisample = can_multisample();
    if (for_picking)
        multisample = false;

    GLint max_samples;
    glsafe(::glGetIntegerv(GL_MAX_SAMPLES, &max_samples));
    GLsizei num_samples = max_samples / 2;

    GLuint render_fbo;
    glsafe(::glGenFramebuffers(1, &render_fbo));
    glsafe(::glBindFramebuffer(GL_FRAMEBUFFER, render_fbo));

    BOOST_LOG_TRIVIAL(info) << boost::format("render_thumbnail prepare: w %1%, h %2%, max_samples  %3%, render_fbo %4%") %w %h %max_samples % render_fbo;
    GLuint render_tex = 0;
    GLuint render_tex_buffer = 0;
    if (multisample) {
        // use renderbuffer instead of texture to avoid the need to use glTexImage2DMultisample which is available only since OpenGL 3.2
        glsafe(::glGenRenderbuffers(1, &render_tex_buffer));
        glsafe(::glBindRenderbuffer(GL_RENDERBUFFER, render_tex_buffer));
        glsafe(::glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples, GL_RGBA8, w, h));
        glsafe(::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_tex_buffer));
    }
    else {
        glsafe(::glGenTextures(1, &render_tex));
        glsafe(::glBindTexture(GL_TEXTURE_2D, render_tex));
        glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
        glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        glsafe(::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_tex, 0));
    }

    GLuint render_depth;
    glsafe(::glGenRenderbuffers(1, &render_depth));
    glsafe(::glBindRenderbuffer(GL_RENDERBUFFER, render_depth));
    if (multisample)
        glsafe(::glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples, GL_DEPTH_COMPONENT24, w, h));
    else
        glsafe(::glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h));

    glsafe(::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_depth));

    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glsafe(::glDrawBuffers(1, drawBufs));

    if (::glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        render_thumbnail_internal(thumbnail_data, thumbnail_params, partplate_list, model_objects, volumes, extruder_colors, shader,
                                  camera_type, use_top_view, for_picking,ban_light);

        if (multisample) {
            GLuint resolve_fbo;
            glsafe(::glGenFramebuffers(1, &resolve_fbo));
            glsafe(::glBindFramebuffer(GL_FRAMEBUFFER, resolve_fbo));

            GLuint resolve_tex;
            glsafe(::glGenTextures(1, &resolve_tex));
            glsafe(::glBindTexture(GL_TEXTURE_2D, resolve_tex));
            glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
            glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            glsafe(::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolve_tex, 0));

            glsafe(::glDrawBuffers(1, drawBufs));

            if (::glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                glsafe(::glBindFramebuffer(GL_READ_FRAMEBUFFER, render_fbo));
                glsafe(::glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_fbo));
                glsafe(::glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR));

                glsafe(::glBindFramebuffer(GL_READ_FRAMEBUFFER, resolve_fbo));
                glsafe(::glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)thumbnail_data.pixels.data()));
            }

            glsafe(::glDeleteTextures(1, &resolve_tex));
            glsafe(::glDeleteFramebuffers(1, &resolve_fbo));
        }
        else
            glsafe(::glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)thumbnail_data.pixels.data()));

#if ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT
        debug_output_thumbnail(thumbnail_data);
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT
    }
    else {
        BOOST_LOG_TRIVIAL(info) << boost::format("render_thumbnail prepare: GL_FRAMEBUFFER not complete");
    }

    glsafe(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
    glsafe(::glDeleteRenderbuffers(1, &render_depth));
    if (render_tex_buffer != 0)
        glsafe(::glDeleteRenderbuffers(1, &render_tex_buffer));
    if (render_tex != 0)
        glsafe(::glDeleteTextures(1, &render_tex));
    glsafe(::glDeleteFramebuffers(1, &render_fbo));

    BOOST_LOG_TRIVIAL(info) << boost::format("render_thumbnail prepare: finished");
}

void render_thumbnail_framebuffer_ext(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params,
    PartPlateList& partplate_list, ModelObjectPtrs& model_objects, const GLVolumeCollection& volumes, std::vector<ColorRGBA>& extruder_colors,
    GLShaderProgram* shader, Camera::EType camera_type, bool use_top_view, bool for_picking, bool ban_light)
{
    thumbnail_data.set(w, h);
    if (!thumbnail_data.is_valid())
        return;

    bool multisample = can_multisample();
    if (for_picking)
        multisample = false;

    GLint max_samples;
    glsafe(::glGetIntegerv(GL_MAX_SAMPLES_EXT, &max_samples));
    GLsizei num_samples = max_samples / 2;

    GLuint render_fbo;
    glsafe(::glGenFramebuffersEXT(1, &render_fbo));
    glsafe(::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, render_fbo));

    GLuint render_tex = 0;
    GLuint render_tex_buffer = 0;
    if (multisample) {
        // use renderbuffer instead of texture to avoid the need to use glTexImage2DMultisample which is available only since OpenGL 3.2
        glsafe(::glGenRenderbuffersEXT(1, &render_tex_buffer));
        glsafe(::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_tex_buffer));
        glsafe(::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, num_samples, GL_RGBA8, w, h));
        glsafe(::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, render_tex_buffer));
    }
    else {
        glsafe(::glGenTextures(1, &render_tex));
        glsafe(::glBindTexture(GL_TEXTURE_2D, render_tex));
        glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
        glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        glsafe(::glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, render_tex, 0));
    }

    GLuint render_depth;
    glsafe(::glGenRenderbuffersEXT(1, &render_depth));
    glsafe(::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_depth));
    if (multisample)
        glsafe(::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, num_samples, GL_DEPTH_COMPONENT24, w, h));
    else
        glsafe(::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, w, h));

    glsafe(::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, render_depth));

    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glsafe(::glDrawBuffers(1, drawBufs));

    if (::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT) {
        render_thumbnail_internal(thumbnail_data, thumbnail_params, partplate_list, model_objects, volumes, extruder_colors, shader, camera_type, use_top_view, for_picking,
                                  ban_light);

        if (multisample) {
            GLuint resolve_fbo;
            glsafe(::glGenFramebuffersEXT(1, &resolve_fbo));
            glsafe(::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, resolve_fbo));

            GLuint resolve_tex;
            glsafe(::glGenTextures(1, &resolve_tex));
            glsafe(::glBindTexture(GL_TEXTURE_2D, resolve_tex));
            glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
            glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            glsafe(::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, resolve_tex, 0));

            glsafe(::glDrawBuffers(1, drawBufs));

            if (::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT) {
                glsafe(::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, render_fbo));
                glsafe(::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, resolve_fbo));
                glsafe(::glBlitFramebufferEXT(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR));

                glsafe(::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, resolve_fbo));
                glsafe(::glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)thumbnail_data.pixels.data()));
            }

            glsafe(::glDeleteTextures(1, &resolve_tex));
            glsafe(::glDeleteFramebuffersEXT(1, &resolve_fbo));
        }
        else
            glsafe(::glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)thumbnail_data.pixels.data()));

#if ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT
        debug_output_thumbnail(thumbnail_data);
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG_OUTPUT
    }

    glsafe(::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0));
    glsafe(::glDeleteRenderbuffersEXT(1, &render_depth));
    if (render_tex_buffer != 0)
        glsafe(::glDeleteRenderbuffersEXT(1, &render_tex_buffer));
    if (render_tex != 0)
        glsafe(::glDeleteTextures(1, &render_tex));
    glsafe(::glDeleteFramebuffersEXT(1, &render_fbo));
}

void render_calibration_thumbnail_framebuffer(GCode::GCodeViewerData& data, ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const BoundingBoxf3& box)
{
    BOOST_LOG_TRIVIAL(info) << boost::format("render_calibration_thumbnail prepare: width %1%, height %2%")%w %h;
    thumbnail_data.set(w, h);
    if (!thumbnail_data.is_valid())
        return;

    //TODO bool multisample = m_multisample_allowed;
    bool multisample = can_multisample();
    //if (!multisample)
    //    glsafe(::glEnable(GL_MULTISAMPLE));

    GLint max_samples;
    glsafe(::glGetIntegerv(GL_MAX_SAMPLES, &max_samples));
    GLsizei num_samples = max_samples / 2;

    GLuint render_fbo;
    glsafe(::glGenFramebuffers(1, &render_fbo));
    glsafe(::glBindFramebuffer(GL_FRAMEBUFFER, render_fbo));
    BOOST_LOG_TRIVIAL(info) << boost::format("render_calibration_thumbnail prepare: max_samples %1%, multisample %2%, render_fbo %3%")%max_samples %multisample %render_fbo;

    GLuint render_tex = 0;
    GLuint render_tex_buffer = 0;
    if (multisample) {
        // use renderbuffer instead of texture to avoid the need to use glTexImage2DMultisample which is available only since OpenGL 3.2
        glsafe(::glGenRenderbuffers(1, &render_tex_buffer));
        glsafe(::glBindRenderbuffer(GL_RENDERBUFFER, render_tex_buffer));
        glsafe(::glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples, GL_RGBA8, w, h));
        glsafe(::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_tex_buffer));
    }
    else {
        glsafe(::glGenTextures(1, &render_tex));
        glsafe(::glBindTexture(GL_TEXTURE_2D, render_tex));
        glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
        glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        glsafe(::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_tex, 0));
    }

    GLuint render_depth;
    glsafe(::glGenRenderbuffers(1, &render_depth));
    glsafe(::glBindRenderbuffer(GL_RENDERBUFFER, render_depth));
    if (multisample)
        glsafe(::glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples, GL_DEPTH_COMPONENT24, w, h));
    else
        glsafe(::glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h));

    glsafe(::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_depth));

    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glsafe(::glDrawBuffers(1, drawBufs));


    if (::glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        render_calibration_thumbnail_internal(data, thumbnail_data, box);

        if (multisample) {
            GLuint resolve_fbo;
            glsafe(::glGenFramebuffers(1, &resolve_fbo));
            glsafe(::glBindFramebuffer(GL_FRAMEBUFFER, resolve_fbo));

            GLuint resolve_tex;
            glsafe(::glGenTextures(1, &resolve_tex));
            glsafe(::glBindTexture(GL_TEXTURE_2D, resolve_tex));
            glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
            glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            glsafe(::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolve_tex, 0));

            glsafe(::glDrawBuffers(1, drawBufs));

            if (::glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                glsafe(::glBindFramebuffer(GL_READ_FRAMEBUFFER, render_fbo));
                glsafe(::glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_fbo));
                glsafe(::glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR));

                glsafe(::glBindFramebuffer(GL_READ_FRAMEBUFFER, resolve_fbo));
                glsafe(::glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)thumbnail_data.pixels.data()));
            }

            glsafe(::glDeleteTextures(1, &resolve_tex));
            glsafe(::glDeleteFramebuffers(1, &resolve_fbo));
        }
        else
            glsafe(::glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)thumbnail_data.pixels.data()));
    }
#if ENABLE_CALIBRATION_THUMBNAIL_OUTPUT
     debug_calibration_output_thumbnail(thumbnail_data);
#endif

     glsafe(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
     glsafe(::glDeleteRenderbuffers(1, &render_depth));
     if (render_tex_buffer != 0)
         glsafe(::glDeleteRenderbuffers(1, &render_tex_buffer));
     if (render_tex != 0)
         glsafe(::glDeleteTextures(1, &render_tex));
     glsafe(::glDeleteFramebuffers(1, &render_fbo));

    //if (!multisample)
    //    glsafe(::glDisable(GL_MULTISAMPLE));
    BOOST_LOG_TRIVIAL(info) << boost::format("render_calibration_thumbnail prepare: exit");
}

void render_calibration_thumbnail_internal(GCode::GCodeViewerData& data, ThumbnailData& thumbnail_data, const BoundingBoxf3& box)
{
    //int plate_idx = thumbnail_params.plate_id;
    //PartPlate* plate = partplate_list.get_plate(plate_idx);
    //BoundingBoxf3 plate_box = plate->get_bounding_box(false);
    //plate_box.min.z() = 0.0;
    //plate_box.max.z() = 0.0;
    Vec3d center = box.center();
    center[2] = 0.0;

#if 1
    Camera camera;
    camera.set_viewport(0, 0, thumbnail_data.width, thumbnail_data.height);
    apply_viewport(camera);
    camera.set_scene_box(box);
    camera.set_type(Camera::EType::Ortho);
    camera.set_target(center);
    camera.select_view("top");
    camera.zoom_to_box(box, 1.0f);
    camera.apply_projection(box);

    using namespace GCode;

    auto render_as_triangles = [](TBuffer &buffer, std::vector<RenderPath>::iterator it_path, std::vector<RenderPath>::iterator it_end, GLShaderProgram& shader, int uniform_color) {
        for (auto it = it_path; it != it_end && it_path->ibuffer_id == it->ibuffer_id; ++it) {
            const RenderPath& path = *it;
            // Some OpenGL drivers crash on empty glMultiDrawElements, see GH #7415.
            assert(!path.sizes.empty());
            assert(!path.offsets.empty());
            shader.set_uniform(uniform_color, path.color);
            glsafe(::glMultiDrawElements(GL_TRIANGLES, (const GLsizei*)path.sizes.data(), GL_UNSIGNED_SHORT, (const void* const*)path.offsets.data(), (GLsizei)path.sizes.size()));
        }
    };

    auto render_as_instanced_model = [](TBuffer& buffer, GLShaderProgram& shader) {
        for (auto& range : buffer.model.instances.render_ranges.ranges) {
            if (range.vbo == 0 && range.count > 0) {
                glsafe(::glGenBuffers(1, &range.vbo));
                glsafe(::glBindBuffer(GL_ARRAY_BUFFER, range.vbo));
                glsafe(::glBufferData(GL_ARRAY_BUFFER, range.count * buffer.model.instances.instance_size_bytes(), (const void*)&buffer.model.instances.buffer[range.offset * buffer.model.instances.instance_size_floats()], GL_STATIC_DRAW));
                glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));
            }

            if (range.vbo > 0) {
                buffer.model.model.set_color(range.color);
                buffer.model.model.render_instanced(range.vbo, range.count);
            }
        }
    };

    auto render_as_batched_model = [](TBuffer& buffer, GLShaderProgram& shader, int position_id, int normal_id) {
        struct Range
        {
            unsigned int first;
            unsigned int last;
            bool intersects(const Range& other) const { return (other.last < first || other.first > last) ? false : true; }
        };
        Range buffer_range = { 0, 0 };
        size_t indices_per_instance = buffer.model.data.indices_count();

        for (size_t j = 0; j < buffer.indices.size(); ++j) {
            const IBuffer& i_buffer = buffer.indices[j];
            buffer_range.last = buffer_range.first + i_buffer.count / indices_per_instance;
            glsafe(::glBindBuffer(GL_ARRAY_BUFFER, i_buffer.vbo));
            if (position_id != -1) {
                glsafe(::glVertexAttribPointer(position_id, buffer.vertices.position_size_floats(), GL_FLOAT, GL_FALSE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.position_offset_bytes()));
                glsafe(::glEnableVertexAttribArray(position_id));
            }
            bool has_normals = buffer.vertices.normal_size_floats() > 0;
            if (has_normals) {
                if (normal_id != -1) {
                    glsafe(::glVertexAttribPointer(normal_id, buffer.vertices.normal_size_floats(), GL_FLOAT, GL_FALSE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.normal_offset_bytes()));
                    glsafe(::glEnableVertexAttribArray(normal_id));
                }
            }

            glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i_buffer.ibo));

            for (auto& range : buffer.model.instances.render_ranges.ranges) {
                Range range_range = { range.offset, range.offset + range.count };
                if (range_range.intersects(buffer_range)) {
                    shader.set_uniform("uniform_color", range.color);
                    unsigned int offset = (range_range.first > buffer_range.first) ? range_range.first - buffer_range.first : 0;
                    size_t offset_bytes = static_cast<size_t>(offset) * indices_per_instance * sizeof(IBufferType);
                    Range render_range = { std::max(range_range.first, buffer_range.first), std::min(range_range.last, buffer_range.last) };
                    size_t count = static_cast<size_t>(render_range.last - render_range.first) * indices_per_instance;
                    if (count > 0) {
                        glsafe(::glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_SHORT, (const void*)offset_bytes));
                    }
                }
            }

            glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
            
            if (normal_id != -1)
                glsafe(::glDisableVertexAttribArray(normal_id));
            if (position_id != -1)
                glsafe(::glDisableVertexAttribArray(position_id));
            glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));

            buffer_range.first = buffer_range.last;
        }
    };

    unsigned char begin_id = buffer_id(EMoveType::Retract);
    unsigned char end_id = buffer_id(EMoveType::Count);

    BOOST_LOG_TRIVIAL(info) << boost::format("render_calibration_thumbnail: begin_id %1%, end_id %2%")%begin_id %end_id;
    for (unsigned char i = begin_id; i < end_id; ++i) {
        TBuffer& buffer = data.m_buffers[i];
        if (!buffer.visible || !buffer.has_data())
            continue;

        GLShaderProgram* shader = get_shader("flat");
        if (shader != nullptr) {
            shader->start_using();

            shader->set_uniform("view_model_matrix", camera.get_view_matrix());
            shader->set_uniform("projection_matrix", camera.get_projection_matrix());
            int position_id = shader->get_attrib_location("v_position");
            int normal_id   = shader->get_attrib_location("v_normal");

            if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::InstancedModel) {
                //shader->set_uniform("emission_factor", 0.25f);
                render_as_instanced_model(buffer, *shader);
                //shader->set_uniform("emission_factor", 0.0f);
            }
            else if (buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::BatchedModel) {
                //shader->set_uniform("emission_factor", 0.25f);
                render_as_batched_model(buffer, *shader, position_id, normal_id);
                //shader->set_uniform("emission_factor", 0.0f);
            }
            else {
                int uniform_color = shader->get_uniform_location("uniform_color");
                auto it_path = buffer.render_paths.begin();
                for (unsigned int ibuffer_id = 0; ibuffer_id < static_cast<unsigned int>(buffer.indices.size()); ++ibuffer_id) {
                    const IBuffer& i_buffer = buffer.indices[ibuffer_id];
                    // Skip all paths with ibuffer_id < ibuffer_id.
                    for (; it_path != buffer.render_paths.end() && it_path->ibuffer_id < ibuffer_id; ++it_path);
                    if (it_path == buffer.render_paths.end() || it_path->ibuffer_id > ibuffer_id)
                        // Not found. This shall not happen.
                        continue;

                    glsafe(::glBindBuffer(GL_ARRAY_BUFFER, i_buffer.vbo));
                    if (position_id != -1) {
                        glsafe(::glVertexAttribPointer(position_id, buffer.vertices.position_size_floats(), GL_FLOAT, GL_FALSE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.position_offset_bytes()));
                        glsafe(::glEnableVertexAttribArray(position_id));
                    }
                    bool has_normals = false;// buffer.vertices.normal_size_floats() > 0;
                    if (has_normals) {
                        if (normal_id != -1) {
                            glsafe(::glVertexAttribPointer(normal_id, buffer.vertices.normal_size_floats(), GL_FLOAT, GL_FALSE, buffer.vertices.vertex_size_bytes(), (const void*)buffer.vertices.normal_offset_bytes()));
                            glsafe(::glEnableVertexAttribArray(normal_id));
                        }
                    }

                    glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i_buffer.ibo));

                    // Render all elements with it_path->ibuffer_id == ibuffer_id, possible with varying colors.
                    switch (buffer.render_primitive_type)
                    {
                    case TBuffer::ERenderPrimitiveType::Triangle: {
                        render_as_triangles(buffer, it_path, buffer.render_paths.end(), *shader, uniform_color);
                        break;
                    }
                    default: { break; }
                    }

                    glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
                    
                    if (normal_id != -1)
                        glsafe(::glDisableVertexAttribArray(normal_id));
                    if (position_id != -1)
                        glsafe(::glDisableVertexAttribArray(position_id));

                    glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));
                }
            }

            shader->stop_using();
        }
        else {
            BOOST_LOG_TRIVIAL(info) << boost::format("render_calibration_thumbnail: can not find shader");
        }
    }
#endif
    BOOST_LOG_TRIVIAL(info) << boost::format("render_calibration_thumbnail: exit");

}

}
}
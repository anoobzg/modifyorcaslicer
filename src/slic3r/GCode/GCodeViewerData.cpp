#include "GCodeViewerData.hpp"
#include "libslic3r/Geometry/ConvexHull.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/BuildVolume.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "slic3r/Render/AppRender.hpp"

#include <GL/glew.h>

namespace Slic3r {
namespace GUI{
namespace GCode {

// format data into the buffers to be rendered as lines
static void add_vertices_as_line(const GCodeProcessorResult::MoveVertex& prev, const GCodeProcessorResult::MoveVertex& curr, VertexBuffer& vertices) {
    auto add_vertex = [&vertices](const Vec3f& position) {
        // add position
        vertices.push_back(position.x());
        vertices.push_back(position.y());
        vertices.push_back(position.z());
    };
    // x component of the normal to the current segment (the normal is parallel to the XY plane)
    //BBS: Has modified a lot for this function to support arc move
    size_t loop_num = curr.is_arc_move_with_interpolation_points() ? curr.interpolation_points.size() : 0;
    for (size_t i = 0; i < loop_num + 1; i++) {
        const Vec3f &previous = (i == 0? prev.position : curr.interpolation_points[i-1]);
        const Vec3f &current = (i == loop_num? curr.position : curr.interpolation_points[i]);
        // add previous vertex
        add_vertex(previous);
        // add current vertex
        add_vertex(current);
    }
}

//BBS: modify a lot to support arc travel
static void add_indices_as_line(const GCodeProcessorResult::MoveVertex& prev, const GCodeProcessorResult::MoveVertex& curr, TBuffer& buffer,
    size_t& vbuffer_size, unsigned int ibuffer_id, IndexBuffer& indices, size_t move_id) {

        if (buffer.paths.empty() || prev.type != curr.type || !buffer.paths.back().matches(curr)) {
            buffer.add_path(curr, ibuffer_id, indices.size(), move_id - 1);
            buffer.paths.back().sub_paths.front().first.position = prev.position;
        }

        Path& last_path = buffer.paths.back();
        size_t loop_num = curr.is_arc_move_with_interpolation_points() ? curr.interpolation_points.size() : 0;
        for (size_t i = 0; i < loop_num + 1; i++) {
            //BBS: add previous index
            indices.push_back(static_cast<IBufferType>(indices.size()));
            //BBS: add current index
            indices.push_back(static_cast<IBufferType>(indices.size()));
            vbuffer_size += buffer.max_vertices_per_segment();
        }
        last_path.sub_paths.back().last = { ibuffer_id, indices.size() - 1, move_id, curr.position };
}

// format data into the buffers to be rendered as solid.
static void add_vertices_as_solid(const GCodeProcessorResult::MoveVertex& prev, const GCodeProcessorResult::MoveVertex& curr, TBuffer& buffer, unsigned int vbuffer_id, VertexBuffer& vertices, size_t move_id) {
    auto store_vertex = [](VertexBuffer& vertices, const Vec3f& position, const Vec3f& normal) {
        // append position
        vertices.push_back(position.x());
        vertices.push_back(position.y());
        vertices.push_back(position.z());
        // append normal as 3 bytes + 1 byte padding (4 bytes total)
        // Convert normal from [-1,1] to [0,255] range
        unsigned char nx = static_cast<unsigned char>((normal.x() + 1.0f) * 127.5f);
        unsigned char ny = static_cast<unsigned char>((normal.y() + 1.0f) * 127.5f);
        unsigned char nz = static_cast<unsigned char>((normal.z() + 1.0f) * 127.5f);
        unsigned char padding = 0;
        
        // Pack 4 bytes into 1 float
        unsigned int packed = (static_cast<unsigned int>(nx) << 24) | 
                             (static_cast<unsigned int>(ny) << 16) | 
                             (static_cast<unsigned int>(nz) << 8) | 
                             static_cast<unsigned int>(padding);
        vertices.push_back(*reinterpret_cast<float*>(&packed));
    };

    if (buffer.paths.empty() || prev.type != curr.type || !buffer.paths.back().matches(curr)) {
        buffer.add_path(curr, vbuffer_id, vertices.size(), move_id - 1);
        buffer.paths.back().sub_paths.back().first.position = prev.position;
    }

    Path& last_path = buffer.paths.back();
    //BBS: Has modified a lot for this function to support arc move
    size_t loop_num = curr.is_arc_move_with_interpolation_points() ? curr.interpolation_points.size() : 0;
    for (size_t i = 0; i < loop_num + 1; i++) {
        const Vec3f &prev_position = (i == 0? prev.position : curr.interpolation_points[i-1]);
        const Vec3f &curr_position = (i == loop_num? curr.position : curr.interpolation_points[i]);

        const Vec3f dir = (curr_position - prev_position).normalized();
        const Vec3f right = Vec3f(dir.y(), -dir.x(), 0.0f).normalized();
        const Vec3f left = -right;
        const Vec3f up = right.cross(dir);
        const Vec3f down = -up;
        const float half_width = 0.5f * last_path.width;
        const float half_height = 0.5f * last_path.height;
        const Vec3f prev_pos = prev_position - half_height * up;
        const Vec3f curr_pos = curr_position - half_height * up;
        const Vec3f d_up = half_height * up;
        const Vec3f d_down = -half_height * up;
        const Vec3f d_right = half_width * right;
        const Vec3f d_left = -half_width * right;

        if ((last_path.vertices_count() == 1 || vertices.empty()) && i == 0) {
            store_vertex(vertices, prev_pos + d_up, up);
            store_vertex(vertices, prev_pos + d_right, right);
            store_vertex(vertices, prev_pos + d_down, down);
            store_vertex(vertices, prev_pos + d_left, left);
        } else {
            store_vertex(vertices, prev_pos + d_right, right);
            store_vertex(vertices, prev_pos + d_left, left);
        }

        store_vertex(vertices, curr_pos + d_up, up);
        store_vertex(vertices, curr_pos + d_right, right);
        store_vertex(vertices, curr_pos + d_down, down);
        store_vertex(vertices, curr_pos + d_left, left);
    }

    last_path.sub_paths.back().last = { vbuffer_id, vertices.size(), move_id, curr.position };
}

static void add_indices_as_solid (const GCodeProcessorResult::MoveVertex& prev, const GCodeProcessorResult::MoveVertex& curr, const GCodeProcessorResult::MoveVertex* next,
    TBuffer& buffer, size_t& vbuffer_size, unsigned int ibuffer_id, IndexBuffer& indices, size_t move_id) {
        static Vec3f prev_dir;
        static Vec3f prev_up;
        static float sq_prev_length;
        auto store_triangle = [](IndexBuffer& indices, IBufferType i1, IBufferType i2, IBufferType i3) {
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        };
        auto append_dummy_cap = [store_triangle](IndexBuffer& indices, IBufferType id) {
            store_triangle(indices, id, id, id);
            store_triangle(indices, id, id, id);
        };
        auto convert_vertices_offset = [](size_t vbuffer_size, const std::array<int, 8>& v_offsets) {
            std::array<IBufferType, 8> ret = {
                static_cast<IBufferType>(static_cast<int>(vbuffer_size) + v_offsets[0]),
                static_cast<IBufferType>(static_cast<int>(vbuffer_size) + v_offsets[1]),
                static_cast<IBufferType>(static_cast<int>(vbuffer_size) + v_offsets[2]),
                static_cast<IBufferType>(static_cast<int>(vbuffer_size) + v_offsets[3]),
                static_cast<IBufferType>(static_cast<int>(vbuffer_size) + v_offsets[4]),
                static_cast<IBufferType>(static_cast<int>(vbuffer_size) + v_offsets[5]),
                static_cast<IBufferType>(static_cast<int>(vbuffer_size) + v_offsets[6]),
                static_cast<IBufferType>(static_cast<int>(vbuffer_size) + v_offsets[7])
            };
            return ret;
        };
        auto append_starting_cap_triangles = [&](IndexBuffer& indices, const std::array<IBufferType, 8>& v_offsets) {
            store_triangle(indices, v_offsets[0], v_offsets[2], v_offsets[1]);
            store_triangle(indices, v_offsets[0], v_offsets[3], v_offsets[2]);
        };
        auto append_stem_triangles = [&](IndexBuffer& indices, const std::array<IBufferType, 8>& v_offsets) {
            store_triangle(indices, v_offsets[0], v_offsets[1], v_offsets[4]);
            store_triangle(indices, v_offsets[1], v_offsets[5], v_offsets[4]);
            store_triangle(indices, v_offsets[1], v_offsets[2], v_offsets[5]);
            store_triangle(indices, v_offsets[2], v_offsets[6], v_offsets[5]);
            store_triangle(indices, v_offsets[2], v_offsets[3], v_offsets[6]);
            store_triangle(indices, v_offsets[3], v_offsets[7], v_offsets[6]);
            store_triangle(indices, v_offsets[3], v_offsets[0], v_offsets[7]);
            store_triangle(indices, v_offsets[0], v_offsets[4], v_offsets[7]);
        };
        auto append_ending_cap_triangles = [&](IndexBuffer& indices, const std::array<IBufferType, 8>& v_offsets) {
            store_triangle(indices, v_offsets[4], v_offsets[6], v_offsets[7]);
            store_triangle(indices, v_offsets[4], v_offsets[5], v_offsets[6]);
        };

        if (buffer.paths.empty() || prev.type != curr.type || !buffer.paths.back().matches(curr)) {
            buffer.add_path(curr, ibuffer_id, indices.size(), move_id - 1);
            buffer.paths.back().sub_paths.back().first.position = prev.position;
        }

        Path& last_path = buffer.paths.back();
        bool is_first_segment = (last_path.vertices_count() == 1);
        //BBS: has modified a lot for this function to support arc move
        std::array<IBufferType, 8> first_seg_v_offsets = convert_vertices_offset(vbuffer_size, { 0, 1, 2, 3, 4, 5, 6, 7 });
        std::array<IBufferType, 8> non_first_seg_v_offsets = convert_vertices_offset(vbuffer_size, { -4, 0, -2, 1, 2, 3, 4, 5 });

        size_t loop_num = curr.is_arc_move_with_interpolation_points() ? curr.interpolation_points.size() : 0;
        for (size_t i = 0; i < loop_num + 1; i++) {
            const Vec3f &prev_position = (i == 0? prev.position : curr.interpolation_points[i-1]);
            const Vec3f &curr_position = (i == loop_num? curr.position : curr.interpolation_points[i]);

            const Vec3f dir = (curr_position - prev_position).normalized();
            const Vec3f right = Vec3f(dir.y(), -dir.x(), 0.0f).normalized();
            const Vec3f up = right.cross(dir);
            const float sq_length = (curr_position - prev_position).squaredNorm();

            if ((is_first_segment || vbuffer_size == 0) && i == 0) {
                if (is_first_segment && i == 0)
                    // starting cap triangles
                    append_starting_cap_triangles(indices, first_seg_v_offsets);
                // dummy triangles outer corner cap
                append_dummy_cap(indices, vbuffer_size);
                // stem triangles
                append_stem_triangles(indices, first_seg_v_offsets);

                vbuffer_size += 8;
            } else {
                float displacement = 0.0f;
                float cos_dir = prev_dir.dot(dir);
                if (cos_dir > -0.9998477f) {
                    // if the angle between adjacent segments is smaller than 179 degrees
                    const Vec3f med_dir = (prev_dir + dir).normalized();
                    const float half_width = 0.5f * last_path.width;
                    displacement = half_width * ::tan(::acos(std::clamp(dir.dot(med_dir), -1.0f, 1.0f)));
                }

                float sq_displacement = sqr(displacement);
                bool can_displace = displacement > 0.0f && sq_displacement < sq_prev_length&& sq_displacement < sq_length;

                bool is_right_turn = prev_up.dot(prev_dir.cross(dir)) <= 0.0f;
                // whether the angle between adjacent segments is greater than 45 degrees
                bool is_sharp = cos_dir < 0.7071068f;

                bool right_displaced = false;
                bool left_displaced = false;

                if (!is_sharp && can_displace) {
                    if (is_right_turn)
                        left_displaced = true;
                    else
                        right_displaced = true;
                }

                // triangles outer corner cap
                if (is_right_turn) {
                    if (left_displaced)
                        // dummy triangles
                        append_dummy_cap(indices, vbuffer_size);
                    else {
                        store_triangle(indices, vbuffer_size - 4, vbuffer_size + 1, vbuffer_size - 1);
                        store_triangle(indices, vbuffer_size + 1, vbuffer_size - 2, vbuffer_size - 1);
                    }
                }
                else {
                    if (right_displaced)
                        // dummy triangles
                        append_dummy_cap(indices, vbuffer_size);
                    else {
                        store_triangle(indices, vbuffer_size - 4, vbuffer_size - 3, vbuffer_size + 0);
                        store_triangle(indices, vbuffer_size - 3, vbuffer_size - 2, vbuffer_size + 0);
                    }
                }
                // stem triangles
                non_first_seg_v_offsets = convert_vertices_offset(vbuffer_size, { -4, 0, -2, 1, 2, 3, 4, 5 });
                append_stem_triangles(indices, non_first_seg_v_offsets);
                vbuffer_size += 6;
            }
            prev_dir = dir;
            prev_up = up;
            sq_prev_length = sq_length;
        }

        if (next != nullptr && (curr.type != next->type || !last_path.matches(*next)))
            // ending cap triangles
            append_ending_cap_triangles(indices, (is_first_segment && !curr.is_arc_move_with_interpolation_points()) ? first_seg_v_offsets : non_first_seg_v_offsets);

        last_path.sub_paths.back().last = { ibuffer_id, indices.size() - 1, move_id, curr.position };
};

// format data into the buffers to be rendered as instanced model
static void add_model_instance(const GCodeProcessorResult::MoveVertex& curr, InstanceBuffer& instances, InstanceIdBuffer& instances_ids, size_t move_id) {
    // append position
    instances.push_back(curr.position.x());
    instances.push_back(curr.position.y());
    instances.push_back(curr.position.z());
    // append width
    instances.push_back(curr.width);
    // append height
    instances.push_back(curr.height);

    // append id
    instances_ids.push_back(move_id);
}

// format data into the buffers to be rendered as batched model
static void add_vertices_as_model_batch(const GCodeProcessorResult::MoveVertex& curr, const GLModel::Geometry& data, VertexBuffer& vertices, InstanceBuffer& instances, InstanceIdBuffer& instances_ids, size_t move_id) {
    const double width = static_cast<double>(1.5f * curr.width);
    const double height = static_cast<double>(1.5f * curr.height);

    const Transform3d trafo = Geometry::assemble_transform((curr.position - 0.5f * curr.height * Vec3f::UnitZ()).cast<double>(), Vec3d::Zero(), { width, width, height });
    const Eigen::Matrix<double, 3, 3, Eigen::DontAlign> normal_matrix = trafo.matrix().template block<3, 3>(0, 0).inverse().transpose();

    // append vertices
    const size_t vertices_count = data.vertices_count();
    for (size_t i = 0; i < vertices_count; ++i) {
        // append position
        const Vec3d position = trafo * data.extract_position_3(i).cast<double>();
        vertices.push_back(float(position.x()));
        vertices.push_back(float(position.y()));
        vertices.push_back(float(position.z()));

        // append normal as 3 bytes + 1 byte padding (4 bytes total)
        const Vec3d normal = normal_matrix * data.extract_normal_3(i).cast<double>();
        // Convert normal from [-1,1] to [0,255] range
        unsigned char nx = static_cast<unsigned char>((normal.x() + 1.0) * 127.5);
        unsigned char ny = static_cast<unsigned char>((normal.y() + 1.0) * 127.5);
        unsigned char nz = static_cast<unsigned char>((normal.z() + 1.0) * 127.5);
        unsigned char padding = 0;
        
        // Pack 4 bytes into 1 float
        unsigned int packed = (static_cast<unsigned int>(nx) << 24) | 
                             (static_cast<unsigned int>(ny) << 16) | 
                             (static_cast<unsigned int>(nz) << 8) | 
                             static_cast<unsigned int>(padding);
        vertices.push_back(*reinterpret_cast<float*>(&packed));
    }

    // append instance position
    instances.push_back(curr.position.x());
    instances.push_back(curr.position.y());
    instances.push_back(curr.position.z());
    // append instance id
    instances_ids.push_back(move_id);
}

static void add_indices_as_model_batch(const GLModel::Geometry& data, IndexBuffer& indices, IBufferType base_index) {
    const size_t indices_count = data.indices_count();
    for (size_t i = 0; i < indices_count; ++i) {
        indices.push_back(static_cast<IBufferType>(data.extract_index(i) + base_index));
    }
}

//BBS: smooth toolpaths corners for the given TBuffer using triangles
static void smooth_triangle_toolpaths_corners(const GCodeProcessorResult& gcode_result, const TBuffer& t_buffer, MultiVertexBuffer& v_multibuffer, std::vector<size_t>& m_ssid_to_moveid_map) {
    auto extract_position_at = [](const VertexBuffer& vertices, size_t offset) {
        return Vec3f(vertices[offset + 0], vertices[offset + 1], vertices[offset + 2]);
    };
    auto update_position_at = [](VertexBuffer& vertices, size_t offset, const Vec3f& position) {
        vertices[offset + 0] = position.x();
        vertices[offset + 1] = position.y();
        vertices[offset + 2] = position.z();
    };
    auto match_right_vertices_with_internal_point = [&](const Path::Sub_Path& prev_sub_path, const Path::Sub_Path& next_sub_path,
        size_t curr_s_id, bool is_internal_point, size_t interpolation_point_id, size_t vertex_size_floats, const Vec3f& displacement_vec) {
        if (&prev_sub_path == &next_sub_path || is_internal_point) { // previous and next segment are both contained into to the same vertex buffer
            VertexBuffer& vbuffer = v_multibuffer[prev_sub_path.first.b_id];
            // offset into the vertex buffer of the next segment 1st vertex
            size_t temp_offset = prev_sub_path.last.s_id - curr_s_id;
            for (size_t i = prev_sub_path.last.s_id; i > curr_s_id; i--) {
                size_t move_id = m_ssid_to_moveid_map[i];
                temp_offset += (gcode_result.moves[move_id].is_arc_move() ? gcode_result.moves[move_id].interpolation_points.size() : 0);
            }
            if (is_internal_point) {
                size_t move_id = m_ssid_to_moveid_map[curr_s_id];
                temp_offset += (gcode_result.moves[move_id].interpolation_points.size() - interpolation_point_id);
            }
            const size_t next_1st_offset = temp_offset * 6 * vertex_size_floats;
            // offset into the vertex buffer of the right vertex of the previous segment
            const size_t prev_right_offset = prev_sub_path.last.i_id - next_1st_offset - 3 * vertex_size_floats;
            // new position of the right vertices
            const Vec3f shared_vertex = extract_position_at(vbuffer, prev_right_offset) + displacement_vec;
            // update previous segment
            update_position_at(vbuffer, prev_right_offset, shared_vertex);
            // offset into the vertex buffer of the right vertex of the next segment
            const size_t next_right_offset = prev_sub_path.last.i_id - next_1st_offset;
            // update next segment
            update_position_at(vbuffer, next_right_offset, shared_vertex);
        }
        else { // previous and next segment are contained into different vertex buffers
            VertexBuffer& prev_vbuffer = v_multibuffer[prev_sub_path.first.b_id];
            VertexBuffer& next_vbuffer = v_multibuffer[next_sub_path.first.b_id];
            // offset into the previous vertex buffer of the right vertex of the previous segment
            const size_t prev_right_offset = prev_sub_path.last.i_id - 3 * vertex_size_floats;
            // new position of the right vertices
            const Vec3f shared_vertex = extract_position_at(prev_vbuffer, prev_right_offset) + displacement_vec;
            // update previous segment
            update_position_at(prev_vbuffer, prev_right_offset, shared_vertex);
            // offset into the next vertex buffer of the right vertex of the next segment
            const size_t next_right_offset = next_sub_path.first.i_id + 1 * vertex_size_floats;
            // update next segment
            update_position_at(next_vbuffer, next_right_offset, shared_vertex);
        }
    };
    //BBS: modify a lot of this function to support arc move
    auto match_left_vertices_with_internal_point = [&](const Path::Sub_Path& prev_sub_path, const Path::Sub_Path& next_sub_path,
        size_t curr_s_id, bool is_internal_point, size_t interpolation_point_id, size_t vertex_size_floats, const Vec3f& displacement_vec) {
        if (&prev_sub_path == &next_sub_path || is_internal_point) { // previous and next segment are both contained into to the same vertex buffer
            VertexBuffer& vbuffer = v_multibuffer[prev_sub_path.first.b_id];
            // offset into the vertex buffer of the next segment 1st vertex
            size_t temp_offset = prev_sub_path.last.s_id - curr_s_id;
            for (size_t i = prev_sub_path.last.s_id; i > curr_s_id; i--) {
                size_t move_id = m_ssid_to_moveid_map[i];
                temp_offset += (gcode_result.moves[move_id].is_arc_move() ? gcode_result.moves[move_id].interpolation_points.size() : 0);
            }
            if (is_internal_point) {
                size_t move_id = m_ssid_to_moveid_map[curr_s_id];
                temp_offset += (gcode_result.moves[move_id].interpolation_points.size() - interpolation_point_id);
            }
            const size_t next_1st_offset = temp_offset * 6 * vertex_size_floats;
            // offset into the vertex buffer of the left vertex of the previous segment
            const size_t prev_left_offset = prev_sub_path.last.i_id - next_1st_offset - 1 * vertex_size_floats;
            // new position of the left vertices
            const Vec3f shared_vertex = extract_position_at(vbuffer, prev_left_offset) + displacement_vec;
            // update previous segment
            update_position_at(vbuffer, prev_left_offset, shared_vertex);
            // offset into the vertex buffer of the left vertex of the next segment
            const size_t next_left_offset = prev_sub_path.last.i_id - next_1st_offset + 1 * vertex_size_floats;
            // update next segment
            update_position_at(vbuffer, next_left_offset, shared_vertex);
        }
        else { // previous and next segment are contained into different vertex buffers
            VertexBuffer& prev_vbuffer = v_multibuffer[prev_sub_path.first.b_id];
            VertexBuffer& next_vbuffer = v_multibuffer[next_sub_path.first.b_id];
            // offset into the previous vertex buffer of the left vertex of the previous segment
            const size_t prev_left_offset = prev_sub_path.last.i_id - 1 * vertex_size_floats;
            // new position of the left vertices
            const Vec3f shared_vertex = extract_position_at(prev_vbuffer, prev_left_offset) + displacement_vec;
            // update previous segment
            update_position_at(prev_vbuffer, prev_left_offset, shared_vertex);
            // offset into the next vertex buffer of the left vertex of the next segment
            const size_t next_left_offset = next_sub_path.first.i_id + 3 * vertex_size_floats;
            // update next segment
            update_position_at(next_vbuffer, next_left_offset, shared_vertex);
        }
    };

    size_t vertex_size_floats = t_buffer.vertices.vertex_size_floats();
    for (const Path& path : t_buffer.paths) {
        //BBS: the two segments of the path sharing the current vertex may belong
        //to two different vertex buffers
        size_t prev_sub_path_id = 0;
        size_t next_sub_path_id = 0;
        const size_t path_vertices_count = path.vertices_count();
        const float half_width = 0.5f * path.width;
        // BBS: modify a lot to support arc move which has internal points
        for (size_t j = 1; j < path_vertices_count; ++j) {
            size_t curr_s_id = path.sub_paths.front().first.s_id + j;
            size_t move_id = m_ssid_to_moveid_map[curr_s_id];
            int interpolation_points_num = gcode_result.moves[move_id].is_arc_move_with_interpolation_points()?
                                                gcode_result.moves[move_id].interpolation_points.size() : 0;
            int loop_num = interpolation_points_num;
            //BBS: select the subpaths which contains the previous/next segments
            if (!path.sub_paths[prev_sub_path_id].contains(curr_s_id))
                ++prev_sub_path_id;
            if (j == path_vertices_count - 1) {
                if (!gcode_result.moves[move_id].is_arc_move_with_interpolation_points())
                    break;   // BBS: the last move has no internal point.
                loop_num--;  //BBS: don't need to handle the endpoint of the last arc move of path
                next_sub_path_id = prev_sub_path_id;
            } else {
                if (!path.sub_paths[next_sub_path_id].contains(curr_s_id + 1))
                    ++next_sub_path_id;
            }
            const Path::Sub_Path& prev_sub_path = path.sub_paths[prev_sub_path_id];
            const Path::Sub_Path& next_sub_path = path.sub_paths[next_sub_path_id];

            // BBS: smooth triangle toolpaths corners including arc move which has internal interpolation point
            for (int k = 0; k <= loop_num; k++) {
                const Vec3f& prev = k==0?
                                    gcode_result.moves[move_id - 1].position :
                                    gcode_result.moves[move_id].interpolation_points[k-1];
                const Vec3f& curr = k==interpolation_points_num?
                                    gcode_result.moves[move_id].position :
                                    gcode_result.moves[move_id].interpolation_points[k];
                const Vec3f& next = k < interpolation_points_num - 1?
                                    gcode_result.moves[move_id].interpolation_points[k+1]:
                                    (k == interpolation_points_num - 1? gcode_result.moves[move_id].position :
                                    (gcode_result.moves[move_id + 1].is_arc_move_with_interpolation_points()?
                                    gcode_result.moves[move_id + 1].interpolation_points[0] :
                                    gcode_result.moves[move_id + 1].position));

                const Vec3f prev_dir = (curr - prev).normalized();
                const Vec3f prev_right = Vec3f(prev_dir.y(), -prev_dir.x(), 0.0f).normalized();
                const Vec3f prev_up = prev_right.cross(prev_dir);

                const Vec3f next_dir = (next - curr).normalized();

                const bool is_right_turn = prev_up.dot(prev_dir.cross(next_dir)) <= 0.0f;
                const float cos_dir = prev_dir.dot(next_dir);
                // whether the angle between adjacent segments is greater than 45 degrees
                const bool is_sharp = cos_dir < 0.7071068f;

                float displacement = 0.0f;
                if (cos_dir > -0.9998477f) {
                    // if the angle between adjacent segments is smaller than 179 degrees
                    Vec3f med_dir = (prev_dir + next_dir).normalized();
                    displacement = half_width * ::tan(::acos(std::clamp(next_dir.dot(med_dir), -1.0f, 1.0f)));
                }

                const float sq_prev_length = (curr - prev).squaredNorm();
                const float sq_next_length = (next - curr).squaredNorm();
                const float sq_displacement = sqr(displacement);
                const bool can_displace = displacement > 0.0f && sq_displacement < sq_prev_length&& sq_displacement < sq_next_length;
                bool is_internal_point = interpolation_points_num > k;

                if (can_displace) {
                    // displacement to apply to the vertices to match
                    Vec3f displacement_vec = displacement * prev_dir;
                    // matches inner corner vertices
                    if (is_right_turn)
                        match_right_vertices_with_internal_point(prev_sub_path, next_sub_path, curr_s_id, is_internal_point, k, vertex_size_floats, -displacement_vec);
                    else
                        match_left_vertices_with_internal_point(prev_sub_path, next_sub_path, curr_s_id, is_internal_point, k, vertex_size_floats, -displacement_vec);

                    if (!is_sharp) {
                        //BBS: matches outer corner vertices
                        if (is_right_turn)
                            match_left_vertices_with_internal_point(prev_sub_path, next_sub_path, curr_s_id, is_internal_point, k, vertex_size_floats, displacement_vec);
                        else
                            match_right_vertices_with_internal_point(prev_sub_path, next_sub_path, curr_s_id, is_internal_point, k, vertex_size_floats, displacement_vec);
                    }
                }
            }
        }
    }
};


GCodeViewerData::GCodeViewerData()
{

}

void GCodeViewerData::init(ConfigOptionMode mode, PresetBundle* preset_bundle)
{
    if (m_gl_data_initialized)
        return;

    // initializes opengl data of TBuffers
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        TBuffer& buffer = m_buffers[i];
        EMoveType type = buffer_type(i);
        switch (type)
        {
        default: { break; }
        case EMoveType::Tool_change:
        case EMoveType::Color_change:
        case EMoveType::Pause_Print:
        case EMoveType::Custom_GCode:
        case EMoveType::Retract:
        case EMoveType::Unretract:
        case EMoveType::Seam: {

            if(type == EMoveType::Seam)
                buffer.visible = true;

                buffer.render_primitive_type = TBuffer::ERenderPrimitiveType::BatchedModel;
                buffer.vertices.format = VBuffer::EFormat::PositionNormal3Byte;
                buffer.shader = "gouraud_light";

                buffer.model.data = diamond(16);
                buffer.model.color = option_color(type);
                buffer.model.instances.format = InstanceVBuffer::EFormat::BatchedModel;
//            }
            break;
        }
        case EMoveType::Wipe:
        case EMoveType::Extrude: {
            buffer.render_primitive_type = TBuffer::ERenderPrimitiveType::Triangle;
            buffer.vertices.format = VBuffer::EFormat::PositionNormal3Byte;
            buffer.shader = "gouraud_light";
            break;
        }
        case EMoveType::Travel: {
            buffer.render_primitive_type = TBuffer::ERenderPrimitiveType::Line;
            buffer.vertices.format = VBuffer::EFormat::Position;
            buffer.shader = "flat";
            break;
        }
        }

        set_toolpath_move_type_visible(EMoveType::Extrude, true);
    }

    // initializes tool marker
    std::string filename;
    if (preset_bundle != nullptr) {
        const Preset* curr = &preset_bundle->printers.get_selected_preset();
        if (curr->is_system)
            filename = PresetUtils::system_printer_hotend_model(*curr);
        else {
            auto *printer_model = curr->config.opt<ConfigOptionString>("printer_model");
            if (printer_model != nullptr && ! printer_model->value.empty()) {
                filename = preset_bundle->get_hotend_model_for_printer_model(printer_model->value);
            }

            if (filename.empty()) {
                filename = preset_bundle->get_hotend_model_for_printer_model(PresetBundle::ORCA_DEFAULT_PRINTER_MODEL);
            }
        }
    }


    m_sequential_view.marker.init(filename);

    // initializes point sizes
    std::array<int, 2> point_sizes;
    ::glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, point_sizes.data());

    // BBS initialzed view_type items
    m_user_mode = mode;
    update_by_mode(m_user_mode);

    //m_layers_slider->init_texture();

    m_gl_data_initialized = true;
}

void GCodeViewerData::load_gcode(const GCodeProcessorResult& gcode_result, const Print& print, const BuildVolume& build_volume,
    const std::vector<BuildVolume>& sub_build_volumes,
    const std::vector<BoundingBoxf3>& exclude_bounding_box, ConfigOptionMode mode, bool only_gcode)
{
    if (m_last_result_id == gcode_result.id)
        return;

    if (gcode_result.moves.empty())
        return;

    gcode_result.lock();

    load_parameters1(gcode_result, only_gcode);
    load_toolpaths(gcode_result, build_volume, sub_build_volumes,exclude_bounding_box);
    load_parameters2(gcode_result);

    gcode_result.unlock();
}


void GCodeViewerData::refresh(const GCodeProcessorResult& gcode_result, const std::vector<std::string>& str_tool_colors)
{
    //BBS: add mutex for protection of gcode result
    gcode_result.lock();

    //BBS: add safe check
    if (gcode_result.moves.size() == 0) {
        //result cleaned before slicing ,should return here
        gcode_result.unlock();
        return;
    }

    //BBS: add mutex for protection of gcode result
    if (m_moves_count == 0) {
        gcode_result.unlock();
        return;
    }

    wxBusyCursor busy;

    if (m_view_type == EViewType::Tool && !gcode_result.extruder_colors.empty()) {
        // update tool colors from config stored in the gcode
        decode_colors(gcode_result.extruder_colors, m_tools.m_tool_colors);
        m_tools.m_tool_visibles = std::vector<bool>(m_tools.m_tool_colors.size());
        for (auto item: m_tools.m_tool_visibles) item = true;
    }
    else {
        // update tool colors
        decode_colors(str_tool_colors, m_tools.m_tool_colors);
        m_tools.m_tool_visibles = std::vector<bool>(m_tools.m_tool_colors.size());
        for (auto item : m_tools.m_tool_visibles) item = true;
    }

    for (int i = 0; i < m_tools.m_tool_colors.size(); i++) {
        m_tools.m_tool_colors[i] = adjust_color_for_rendering(m_tools.m_tool_colors[i]);
    }
    ColorRGBA default_color;
    decode_color("#FF8000", default_color);
	// ensure there are enough colors defined
    while (m_tools.m_tool_colors.size() < std::max(size_t(1), gcode_result.extruders_count)) {
        m_tools.m_tool_colors.push_back(default_color);
        m_tools.m_tool_visibles.push_back(true);
    }

    // update ranges for coloring / legend
    m_extrusions.reset_ranges();
    for (size_t i = 0; i < m_moves_count; ++i) {
        // skip first vertex
        if (i == 0)
            continue;

        const GCodeProcessorResult::MoveVertex& curr = gcode_result.moves[i];

        switch (curr.type)
        {
        case EMoveType::Extrude:
        {
            m_extrusions.ranges.height.update_from(round_to_bin(curr.height));
            m_extrusions.ranges.width.update_from(round_to_bin(curr.width));
            m_extrusions.ranges.fan_speed.update_from(curr.fan_speed);
            m_extrusions.ranges.temperature.update_from(curr.temperature);
            if (curr.extrusion_role != erCustom || is_visible(erCustom))
                m_extrusions.ranges.volumetric_rate.update_from(round_to_bin(curr.volumetric_rate()));

            if (curr.layer_duration > 0.f) {
                m_extrusions.ranges.layer_duration.update_from(curr.layer_duration);
                m_extrusions.ranges.layer_duration_log.update_from(curr.layer_duration);
            }
            [[fallthrough]];
        }
        case EMoveType::Travel:
        {
            if (m_buffers[buffer_id(curr.type)].visible)
                m_extrusions.ranges.feedrate.update_from(curr.feedrate);

            break;
        }
        default: { break; }
        }
    }

    //BBS: add mutex for protection of gcode result
    gcode_result.unlock();

}


void GCodeViewerData::update_by_mode(ConfigOptionMode mode)
{
    view_type_items.clear();
    view_type_items_str.clear();
    options_items.clear();

    // BBS initialzed view_type items
    view_type_items.push_back(EViewType::FeatureType);
    view_type_items.push_back(EViewType::ColorPrint);
    view_type_items.push_back(EViewType::Feedrate);
    view_type_items.push_back(EViewType::Height);
    view_type_items.push_back(EViewType::Width);
    view_type_items.push_back(EViewType::VolumetricRate);
    view_type_items.push_back(EViewType::LayerTime);
    view_type_items.push_back(EViewType::LayerTimeLog);
    view_type_items.push_back(EViewType::FanSpeed);
    view_type_items.push_back(EViewType::Temperature);
    //if (mode == ConfigOptionMode::comDevelop) {
    //    view_type_items.push_back(EViewType::Tool);
    //}

    for (int i = 0; i < view_type_items.size(); i++) {
        view_type_items_str.push_back(get_view_type_string(view_type_items[i]));
    }

    // BBS for first layer inspection
    view_type_items.push_back(EViewType::FilamentId);

    options_items.push_back(EMoveType::Travel);
    options_items.push_back(EMoveType::Retract);
    options_items.push_back(EMoveType::Unretract);
    options_items.push_back(EMoveType::Wipe);
    //if (mode == ConfigOptionMode::comDevelop) {
    //    options_items.push_back(EMoveType::Tool_change);
    //}
    //BBS: seam is not real move and extrusion, put at last line
    options_items.push_back(EMoveType::Seam);
}

void GCodeViewerData::reset()
{
    m_gcode_result = NULL;
    m_last_result_id = -1;
    //BBS: add only gcode mode
    m_only_gcode_in_preview = false;

    m_moves_count = 0;
    m_ssid_to_moveid_map.clear();
    m_ssid_to_moveid_map.shrink_to_fit();
    for (TBuffer& buffer : m_buffers) {
        buffer.reset();
    }
    m_paths_bounding_box = BoundingBoxf3();
    m_max_bounding_box = BoundingBoxf3();
    m_max_print_height = 0.0f;
    m_tools.m_tool_colors = std::vector<ColorRGBA>();
    m_tools.m_tool_visibles = std::vector<bool>();
    m_extruders_count = 0;
    m_extruder_ids = std::vector<unsigned char>();
    m_filament_diameters = std::vector<float>();
    m_filament_densities = std::vector<float>();
    m_extrusions.reset_ranges();
    //BBS: always load shell at preview
    //m_shells.volumes.clear();
    m_layers.reset();
    m_layers_z_range = { 0, 0 };
    m_roles = std::vector<ExtrusionRole>();
    m_print_statistics.reset();
    m_custom_gcode_per_print_z = std::vector<CustomGCode::Item>();
    m_sequential_view.file.reset();
    m_contained_in_bed = true;
}

void GCodeViewerData::load_parameters1(const GCodeProcessorResult& gcode_result,bool only_gcode)
{
    //BBS: move the id to the end of reset
    m_last_result_id = gcode_result.id;
    m_gcode_result = &gcode_result;
    m_only_gcode_in_preview = only_gcode;

    m_sequential_view.file.load_gcode(gcode_result.filename, gcode_result.lines_ends);

    //BBS: add only gcode mode
    //if (AppAdapter::gui_app()->is_gcode_viewer())
    if (m_only_gcode_in_preview)
        m_custom_gcode_per_print_z = gcode_result.custom_gcode_per_print_z;

    m_max_print_height = gcode_result.printable_height;
    m_idex_mode = gcode_result.hot_bed_divide == Four_Areas ? gcode_result.idex_mode : IdexMode_Pack;

}

void GCodeViewerData::load_toolpaths(const GCodeProcessorResult& gcode_result, 
    const BuildVolume& build_volume, 
    const std::vector<BuildVolume>& sub_build_volumes,
    const std::vector<BoundingBoxf3>& exclude_bounding_box)
{
    // max index buffer size, in bytes
    static const size_t IBUFFER_THRESHOLD_BYTES = 64 * 1024 * 1024;

    m_moves_count = gcode_result.moves.size();
    if (m_moves_count == 0)
        return;

    m_extruders_count = gcode_result.extruders_count;

    unsigned int progress_count = 0;
    static const unsigned int progress_threshold = 1000;
    ////BBS: add only gcode mode
    //ProgressDialog *          progress_dialog    = m_only_gcode_in_preview ?
    //    new ProgressDialog(_L("Loading G-codes"), "...",
    //        100, AppAdapter::main_panel(), wxPD_AUTO_HIDE | wxPD_APP_MODAL) : nullptr;

    wxBusyCursor busy;

    //BBS: use convex_hull for toolpath outside check
    Points pts;

    // extract approximate paths bounding box from result
    //BBS: add only gcode mode
    for (const GCodeProcessorResult::MoveVertex& move : gcode_result.moves) {
        if (move.type == EMoveType::Extrude && move.extrusion_role != erCustom && move.width != 0.0f && move.height != 0.0f) {
            m_paths_bounding_box.merge(move.position.cast<double>());
            //BBS: use convex_hull for toolpath outside check
            pts.emplace_back(Point(scale_(move.position.x()), scale_(move.position.y())));
        }
    }

    // BBS: also merge the point on arc to bounding box
    for (const GCodeProcessorResult::MoveVertex& move : gcode_result.moves) {
        // continue if not arc path
        if (!move.is_arc_move_with_interpolation_points())
            continue;

        if (move.type == EMoveType::Extrude && move.width != 0.0f && move.height != 0.0f)
            for (int i = 0; i < move.interpolation_points.size(); i++) {
                m_paths_bounding_box.merge(move.interpolation_points[i].cast<double>());
                //BBS: use convex_hull for toolpath outside check
                pts.emplace_back(Point(scale_(move.interpolation_points[i].x()), scale_(move.interpolation_points[i].y())));
            }
    }

    // set approximate max bounding box (take in account also the tool marker)
    m_max_bounding_box = m_paths_bounding_box;
    m_max_bounding_box.merge(m_paths_bounding_box.max + m_sequential_view.marker.get_bounding_box().size().z() * Vec3d::UnitZ());

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< boost::format(",m_paths_bounding_box {%1%, %2%}-{%3%, %4%}\n")
        %m_paths_bounding_box.min.x() %m_paths_bounding_box.min.y() %m_paths_bounding_box.max.x() %m_paths_bounding_box.max.y();

    {
        //BBS: use convex_hull for toolpath outside check
        m_contained_in_bed = build_volume.all_paths_inside(gcode_result, m_paths_bounding_box);
        if (m_contained_in_bed) {
            if (exclude_bounding_box.size() > 0)
            {
                int index;
                Slic3r::Polygon convex_hull_2d = Slic3r::Geometry::convex_hull(std::move(pts));
                for (index = 0; index < exclude_bounding_box.size(); index ++)
                {
                    Slic3r::Polygon p = exclude_bounding_box[index].polygon(true);  // instance convex hull is scaled, so we need to scale here
                    if (intersection({ p }, { convex_hull_2d }).empty() == false)
                    {
                        m_contained_in_bed = false;
                        break;
                    }
                }
            }
        }
        (const_cast<GCodeProcessorResult&>(gcode_result)).toolpath_outside = !m_contained_in_bed;
    }

    m_sequential_view.gcode_ids.clear();
    for (size_t i = 0; i < gcode_result.moves.size(); ++i) {
        const GCodeProcessorResult::MoveVertex& move = gcode_result.moves[i];
        if (move.type != EMoveType::Seam)
            m_sequential_view.gcode_ids.push_back(move.gcode_id);
    }
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< boost::format(",m_contained_in_bed %1%\n")%m_contained_in_bed;

    std::vector<MultiVertexBuffer> vertices(m_buffers.size());
    std::vector<MultiIndexBuffer> indices(m_buffers.size());
    std::vector<InstanceBuffer> instances(m_buffers.size());
    std::vector<InstanceIdBuffer> instances_ids(m_buffers.size());
    std::vector<InstancesOffsets> instances_offsets(m_buffers.size());
    std::vector<float> options_zs;

    size_t seams_count = 0;
    std::vector<size_t> biased_seams_ids;

    // toolpaths data -> extract vertices from result
    for (size_t i = 0; i < m_moves_count; ++i) {
        const GCodeProcessorResult::MoveVertex& curr = gcode_result.moves[i];
        if (curr.type == EMoveType::Seam) {
            ++seams_count;
            biased_seams_ids.push_back(i - biased_seams_ids.size() - 1);
        }

        size_t move_id = i - seams_count;

        // skip first vertex
        if (i == 0)
            continue;

        const GCodeProcessorResult::MoveVertex& prev = gcode_result.moves[i - 1];

        //// update progress dialog
        //++progress_count;
        //if (progress_dialog != nullptr && progress_count % progress_threshold == 0) {
        //    progress_dialog->Update(int(100.0f * float(i) / (2.0f * float(m_moves_count))),
        //        _L("Generating geometry vertex data") + ": " + wxNumberFormatter::ToString(100.0 * double(i) / double(m_moves_count), 0, wxNumberFormatter::Style_None) + "%");
        //    progress_dialog->Fit();
        //    progress_count = 0;
        //}

        const unsigned char id = buffer_id(curr.type);
        TBuffer& t_buffer = m_buffers[id];
        MultiVertexBuffer& v_multibuffer = vertices[id];
        InstanceBuffer& inst_buffer = instances[id];
        InstanceIdBuffer& inst_id_buffer = instances_ids[id];
        InstancesOffsets& inst_offsets = instances_offsets[id];

        /*if (i%1000 == 1) {
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":i=%1%, buffer_id %2% render_type %3%, gcode_id %4%\n")
                %i %(int)id %(int)t_buffer.render_primitive_type %curr.gcode_id;
        }*/

        // ensure there is at least one vertex buffer
        if (v_multibuffer.empty())
            v_multibuffer.push_back(VertexBuffer());

        // if adding the vertices for the current segment exceeds the threshold size of the current vertex buffer
        // add another vertex buffer
        // BBS: get the point number and then judge whether the remaining buffer is enough
        size_t points_num = curr.is_arc_move_with_interpolation_points() ? curr.interpolation_points.size() + 1 : 1;
        size_t vertices_size_to_add = (t_buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::BatchedModel) ? t_buffer.model.data.vertices_size_bytes() : points_num * t_buffer.max_vertices_per_segment_size_bytes();
        if (v_multibuffer.back().size() * sizeof(float) > t_buffer.vertices.max_size_bytes() - vertices_size_to_add) {
            v_multibuffer.push_back(VertexBuffer());
            if (t_buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::Triangle) {
                Path& last_path = t_buffer.paths.back();
                if (prev.type == curr.type && last_path.matches(curr))
                    last_path.add_sub_path(prev, static_cast<unsigned int>(v_multibuffer.size()) - 1, 0, move_id - 1);
            }
        }

        VertexBuffer& v_buffer = v_multibuffer.back();

        switch (t_buffer.render_primitive_type)
        {
        case TBuffer::ERenderPrimitiveType::Line:     { add_vertices_as_line(prev, curr, v_buffer); break; }
        case TBuffer::ERenderPrimitiveType::Triangle: { add_vertices_as_solid(prev, curr, t_buffer, static_cast<unsigned int>(v_multibuffer.size()) - 1, v_buffer, move_id); break; }
        case TBuffer::ERenderPrimitiveType::InstancedModel:
        {
            add_model_instance(curr, inst_buffer, inst_id_buffer, move_id);
            inst_offsets.push_back(prev.position - curr.position);
            break;
        }
        case TBuffer::ERenderPrimitiveType::BatchedModel:
        {
            add_vertices_as_model_batch(curr, t_buffer.model.data, v_buffer, inst_buffer, inst_id_buffer, move_id);
            inst_offsets.push_back(prev.position - curr.position);
            break;
        }
        }

        // collect options zs for later use
        if (curr.type == EMoveType::Pause_Print || curr.type == EMoveType::Custom_GCode) {
            const float* const last_z = options_zs.empty() ? nullptr : &options_zs.back();
            if (last_z == nullptr || curr.position[2] < *last_z - EPSILON || *last_z + EPSILON < curr.position[2])
                options_zs.emplace_back(curr.position[2]);
        }
    }

    /*for (size_t b = 0; b < vertices.size(); ++b) {
        MultiVertexBuffer& v_multibuffer = vertices[b];
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(":b=%1%, vertex buffer count %2%\n")
            %b %v_multibuffer.size();
    }*/
    auto extract_move_id = [&biased_seams_ids](size_t id) {
        size_t new_id = size_t(-1);
        auto it = std::lower_bound(biased_seams_ids.begin(), biased_seams_ids.end(), id);
        if (it == biased_seams_ids.end())
            new_id = id + biased_seams_ids.size();
        else {
            if (it == biased_seams_ids.begin() && *it < id)
                new_id = id;
            else if (it != biased_seams_ids.begin())
                new_id = id + std::distance(biased_seams_ids.begin(), it);
        }
        return (new_id == size_t(-1)) ? id : new_id;
    };
    //BBS: generate map from ssid to move id in advance to reduce computation
    m_ssid_to_moveid_map.clear();
    m_ssid_to_moveid_map.reserve( m_moves_count - biased_seams_ids.size());
    for (size_t i = 0; i < m_moves_count - biased_seams_ids.size(); i++)
        m_ssid_to_moveid_map.push_back(extract_move_id(i));

    // smooth toolpaths corners for TBuffers using triangles
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        const TBuffer& t_buffer = m_buffers[i];
        if (t_buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::Triangle) {
            smooth_triangle_toolpaths_corners(gcode_result, t_buffer, vertices[i], m_ssid_to_moveid_map);
        }
    }

    // dismiss, no more needed
    std::vector<size_t>().swap(biased_seams_ids);

    for (MultiVertexBuffer& v_multibuffer : vertices) {
        for (VertexBuffer& v_buffer : v_multibuffer) {
            v_buffer.shrink_to_fit();
        }
    }

    // move the wipe toolpaths half height up to render them on proper position
    MultiVertexBuffer& wipe_vertices = vertices[buffer_id(EMoveType::Wipe)];
    for (VertexBuffer& v_buffer : wipe_vertices) {
        for (size_t i = 2; i < v_buffer.size(); i += 3) {
            v_buffer[i] += 0.5f * GCodeProcessor::Wipe_Height;
        }
    }

    // send vertices data to gpu, where needed
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        TBuffer& t_buffer = m_buffers[i];
        if (t_buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::InstancedModel) {
            const InstanceBuffer& inst_buffer = instances[i];
            if (!inst_buffer.empty()) {
                t_buffer.model.instances.buffer = inst_buffer;
                t_buffer.model.instances.s_ids = instances_ids[i];
                t_buffer.model.instances.offsets = instances_offsets[i];
            }
        }
        else {
            if (t_buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::BatchedModel) {
                const InstanceBuffer& inst_buffer = instances[i];
                if (!inst_buffer.empty()) {
                    t_buffer.model.instances.buffer = inst_buffer;
                    t_buffer.model.instances.s_ids = instances_ids[i];
                    t_buffer.model.instances.offsets = instances_offsets[i];
                }
            }
            const MultiVertexBuffer& v_multibuffer = vertices[i];
            for (const VertexBuffer& v_buffer : v_multibuffer) {
                const size_t size_elements = v_buffer.size();
                const size_t size_bytes = size_elements * sizeof(float);
                const size_t vertices_count = size_elements / t_buffer.vertices.vertex_size_floats();
                t_buffer.vertices.count += vertices_count;

                GLuint id = 0;
                glsafe(::glGenBuffers(1, &id));
                glsafe(::glBindBuffer(GL_ARRAY_BUFFER, id));
                glsafe(::glBufferData(GL_ARRAY_BUFFER, size_bytes, v_buffer.data(), GL_STATIC_DRAW));
                glsafe(::glBindBuffer(GL_ARRAY_BUFFER, 0));

                t_buffer.vertices.vbos.push_back(static_cast<unsigned int>(id));
                t_buffer.vertices.sizes.push_back(size_bytes);
            }
        }
    }

    // dismiss vertices data, no more needed
    std::vector<MultiVertexBuffer>().swap(vertices);
    std::vector<InstanceBuffer>().swap(instances);
    std::vector<InstanceIdBuffer>().swap(instances_ids);

    // toolpaths data -> extract indices from result
    // paths may have been filled while extracting vertices,
    // so reset them, they will be filled again while extracting indices
    for (TBuffer& buffer : m_buffers) {
        buffer.paths.clear();
    }

    // variable used to keep track of the current vertex buffers index and size
    using CurrVertexBuffer = std::pair<unsigned int, size_t>;
    std::vector<CurrVertexBuffer> curr_vertex_buffers(m_buffers.size(), { 0, 0 });

    // variable used to keep track of the vertex buffers ids
    using VboIndexList = std::vector<unsigned int>;
    std::vector<VboIndexList> vbo_indices(m_buffers.size());

    seams_count = 0;

    for (size_t i = 0; i < m_moves_count; ++i) {
        const GCodeProcessorResult::MoveVertex& curr = gcode_result.moves[i];
        if (curr.type == EMoveType::Seam)
            ++seams_count;

        size_t move_id = i - seams_count;

        // skip first vertex
        if (i == 0)
            continue;

        const GCodeProcessorResult::MoveVertex& prev = gcode_result.moves[i - 1];
        const GCodeProcessorResult::MoveVertex* next = nullptr;
        if (i < m_moves_count - 1)
            next = &gcode_result.moves[i + 1];

        //++progress_count;
        //if (progress_dialog != nullptr && progress_count % progress_threshold == 0) {
        //    progress_dialog->Update(int(100.0f * float(m_moves_count + i) / (2.0f * float(m_moves_count))),
        //        _L("Generating geometry index data") + ": " + wxNumberFormatter::ToString(100.0 * double(i) / double(m_moves_count), 0, wxNumberFormatter::Style_None) + "%");
        //    progress_dialog->Fit();
        //    progress_count = 0;
        //}

        const unsigned char id = buffer_id(curr.type);
        TBuffer& t_buffer = m_buffers[id];
        MultiIndexBuffer& i_multibuffer = indices[id];
        CurrVertexBuffer& curr_vertex_buffer = curr_vertex_buffers[id];
        VboIndexList& vbo_index_list = vbo_indices[id];

        // ensure there is at least one index buffer
        if (i_multibuffer.empty()) {
            i_multibuffer.push_back(IndexBuffer());
            if (!t_buffer.vertices.vbos.empty())
                vbo_index_list.push_back(t_buffer.vertices.vbos[curr_vertex_buffer.first]);
        }

        // if adding the indices for the current segment exceeds the threshold size of the current index buffer
        // create another index buffer
        // BBS: get the point number and then judge whether the remaining buffer is enough
        size_t points_num = curr.is_arc_move_with_interpolation_points() ? curr.interpolation_points.size() + 1 : 1;
        size_t indiced_size_to_add = (t_buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::BatchedModel) ? t_buffer.model.data.indices_size_bytes() : points_num * t_buffer.max_indices_per_segment_size_bytes();
        if (i_multibuffer.back().size() * sizeof(IBufferType) >= IBUFFER_THRESHOLD_BYTES - indiced_size_to_add) {
            i_multibuffer.push_back(IndexBuffer());
            vbo_index_list.push_back(t_buffer.vertices.vbos[curr_vertex_buffer.first]);
            if (t_buffer.render_primitive_type != TBuffer::ERenderPrimitiveType::BatchedModel) {
                Path& last_path = t_buffer.paths.back();
                last_path.add_sub_path(prev, static_cast<unsigned int>(i_multibuffer.size()) - 1, 0, move_id - 1);
            }
        }

        // if adding the vertices for the current segment exceeds the threshold size of the current vertex buffer
        // create another index buffer
        // BBS: support multi points in one MoveVertice, should multiply point number
        size_t vertices_size_to_add = (t_buffer.render_primitive_type == TBuffer::ERenderPrimitiveType::BatchedModel) ? t_buffer.model.data.vertices_size_bytes() : points_num * t_buffer.max_vertices_per_segment_size_bytes();
        if (curr_vertex_buffer.second * t_buffer.vertices.vertex_size_bytes() > t_buffer.vertices.max_size_bytes() - vertices_size_to_add) {
            i_multibuffer.push_back(IndexBuffer());

            ++curr_vertex_buffer.first;
            curr_vertex_buffer.second = 0;
            vbo_index_list.push_back(t_buffer.vertices.vbos[curr_vertex_buffer.first]);

            if (t_buffer.render_primitive_type != TBuffer::ERenderPrimitiveType::BatchedModel) {
                Path& last_path = t_buffer.paths.back();
                last_path.add_sub_path(prev, static_cast<unsigned int>(i_multibuffer.size()) - 1, 0, move_id - 1);
            }
        }

        IndexBuffer& i_buffer = i_multibuffer.back();

        switch (t_buffer.render_primitive_type)
        {
        case TBuffer::ERenderPrimitiveType::Line: {
            add_indices_as_line(prev, curr, t_buffer, curr_vertex_buffer.second, static_cast<unsigned int>(i_multibuffer.size()) - 1, i_buffer, move_id);
            break;
        }
        case TBuffer::ERenderPrimitiveType::Triangle: {
            add_indices_as_solid(prev, curr, next, t_buffer, curr_vertex_buffer.second, static_cast<unsigned int>(i_multibuffer.size()) - 1, i_buffer, move_id);
            break;
        }
        case TBuffer::ERenderPrimitiveType::BatchedModel: {
            add_indices_as_model_batch(t_buffer.model.data, i_buffer, curr_vertex_buffer.second);
            curr_vertex_buffer.second += t_buffer.model.data.vertices_count();
            break;
        }
        default: { break; }
        }
    }

    for (MultiIndexBuffer& i_multibuffer : indices) {
        for (IndexBuffer& i_buffer : i_multibuffer) {
            i_buffer.shrink_to_fit();
        }
    }

    // toolpaths data -> send indices data to gpu
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        TBuffer& t_buffer = m_buffers[i];
        if (t_buffer.render_primitive_type != TBuffer::ERenderPrimitiveType::InstancedModel) {
            const MultiIndexBuffer& i_multibuffer = indices[i];
            for (const IndexBuffer& i_buffer : i_multibuffer) {
                const size_t size_elements = i_buffer.size();
                const size_t size_bytes = size_elements * sizeof(IBufferType);

                // stores index buffer informations into TBuffer
                t_buffer.indices.push_back(IBuffer());
                IBuffer& ibuf = t_buffer.indices.back();
                ibuf.count = size_elements;
                ibuf.vbo = vbo_indices[i][t_buffer.indices.size() - 1];

                glsafe(::glGenBuffers(1, &ibuf.ibo));
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuf.ibo));
                glsafe(::glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_bytes, i_buffer.data(), GL_STATIC_DRAW));
                glsafe(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
            }
        }
    }

    //if (progress_dialog != nullptr) {
    //    progress_dialog->Update(100, "");
    //    progress_dialog->Fit();
    //}

    //log_memory_usage("Loaded G-code generated indices buffers ", vertices, indices);

    // dismiss indices data, no more needed
    std::vector<MultiIndexBuffer>().swap(indices);

    // layers zs / roles / extruder ids -> extract from result
    size_t last_travel_s_id = 0;
    seams_count = 0;
    for (size_t i = 0; i < m_moves_count; ++i) {
        const GCodeProcessorResult::MoveVertex& move = gcode_result.moves[i];
        if (move.type == EMoveType::Seam)
            ++seams_count;

        size_t move_id = i - seams_count;

        if (move.type == EMoveType::Extrude) {
            // layers zs
            const double* const last_z = m_layers.empty() ? nullptr : &m_layers.get_zs().back();
            const double z = static_cast<double>(move.position.z());
            if (last_z == nullptr || z < *last_z - EPSILON || *last_z + EPSILON < z)
                m_layers.append(z, { last_travel_s_id, move_id });
            else
                m_layers.get_endpoints().back().last = move_id;
            // extruder ids
            m_extruder_ids.emplace_back(move.extruder_id);
            // roles
            if (i > 0)
                m_roles.emplace_back(move.extrusion_role);
        }
        else if (move.type == EMoveType::Travel) {
            if (move_id - last_travel_s_id > 1 && !m_layers.empty())
                m_layers.get_endpoints().back().last = move_id;

            last_travel_s_id = move_id;
        }
    }

    // roles -> remove duplicates
    sort_remove_duplicates(m_roles);
    m_roles.shrink_to_fit();

    // extruder ids -> remove duplicates
    sort_remove_duplicates(m_extruder_ids);
    m_extruder_ids.shrink_to_fit();

    std::vector<int> plater_extruder;
	for (auto mid : m_extruder_ids){
        int eid = mid;
        plater_extruder.push_back(++eid);
	}
    m_plater_extruder = plater_extruder;

    // replace layers for spiral vase mode
    if (!gcode_result.spiral_vase_layers.empty()) {
        m_layers.reset();
        for (const auto& layer : gcode_result.spiral_vase_layers) {
            m_layers.append(layer.first, { layer.second.first, layer.second.second });
        }
    }

    // set layers z range
    if (!m_layers.empty())
        m_layers_z_range = { 0, static_cast<unsigned int>(m_layers.size() - 1) };

    // change color of paths whose layer contains option points
    if (!options_zs.empty()) {
        TBuffer& extrude_buffer = m_buffers[buffer_id(EMoveType::Extrude)];
        for (Path& path : extrude_buffer.paths) {
            const float z = path.sub_paths.front().first.position.z();
            if (std::find_if(options_zs.begin(), options_zs.end(), [z](float f) { return f - EPSILON <= z && z <= f + EPSILON; }) != options_zs.end())
                path.cp_color_id = 255 - path.cp_color_id;
        }
    }

    mirror_center = build_volume.bed_center();

    if (sub_build_volumes.size() >= 4)
    {
        Vec2d offset;
        Vec2d center1 = sub_build_volumes[0].bed_center();
        Vec2d center4 = sub_build_volumes[3].bed_center();
        ms_offset = center1 - center4;
    }
}

void GCodeViewerData::load_parameters2(const GCodeProcessorResult& gcode_result)
{
    //BBS: add mutex for protection of gcode result
    if (m_layers.empty()) {
        return;
    }

    m_settings_ids = gcode_result.settings_ids;
    m_filament_diameters = gcode_result.filament_diameters;
    m_filament_densities = gcode_result.filament_densities;
    m_sequential_view.m_show_marker = false;


    m_print_statistics = gcode_result.print_statistics;

    if (m_time_estimate_mode != PrintEstimatedStatistics::ETimeMode::Normal) {
        const float time = m_print_statistics.modes[static_cast<size_t>(m_time_estimate_mode)].time;
        if (time == 0.0f ||
            short_time(get_time_dhms(time)) == short_time(get_time_dhms(m_print_statistics.modes[static_cast<size_t>(PrintEstimatedStatistics::ETimeMode::Normal)].time)))
            m_time_estimate_mode = PrintEstimatedStatistics::ETimeMode::Normal;
    }

    // set to color print by default if use multi extruders
    if (m_extruder_ids.size() > 1) {
        for (int i = 0; i < view_type_items.size(); i++) {
            if (view_type_items[i] == EViewType::ColorPrint) {
                m_view_type_sel = i;
                break;
            }
        }

        //set_view_type(EViewType::ColorPrint);
    }

    m_conflict_result = gcode_result.conflict_result;
    if (m_conflict_result) 
    { 
        m_conflict_result.value().layer = m_layers.get_l_at(m_conflict_result.value()._height); 
    }

}


bool GCodeViewerData::is_visible(ExtrusionRole role)
{
    return role < erCount && (m_extrusions.role_visibility_flags & (1 << role)) != 0;
}

bool GCodeViewerData::is_visible(const Path& path)
{
    return is_visible(path.role);
}

void GCodeViewerData::set_view_type(EViewType type, bool reset_feature_type_visible) 
{
    if (type == EViewType::Count)
        type = EViewType::FeatureType;

    m_view_type = (EViewType)type;
    if (reset_feature_type_visible && type == EViewType::ColorPrint) {
        reset_visible(EViewType::FeatureType);
    }
}

void GCodeViewerData::reset_visible(EViewType type) 
{
    if (type == EViewType::FeatureType) {
        for (size_t i = 0; i < m_roles.size(); ++i) {
            ExtrusionRole role = m_roles[i];
            m_extrusions.role_visibility_flags = m_extrusions.role_visibility_flags | (1 << role);
        }
    } else if (type == EViewType::ColorPrint){
        for(auto item: m_tools.m_tool_visibles) item = true;
    }
}

bool GCodeViewerData::is_toolpath_move_type_visible(EMoveType type) const
{
    size_t id = static_cast<size_t>(buffer_id(type));
    return (id < m_buffers.size()) ? m_buffers[id].visible : false;
}

void GCodeViewerData::set_toolpath_move_type_visible(EMoveType type, bool visible)
{
    size_t id = static_cast<size_t>(buffer_id(type));
    if (id < m_buffers.size())
        m_buffers[id].visible = visible;
}

bool GCodeViewerData::has_data() const 
{ 
    return !m_roles.empty(); 
}

void GCodeViewerData::set_scale(float scale)
{
    if(m_scale != scale)m_scale = scale;
    if (m_sequential_view.m_scale != scale) {
        m_sequential_view.m_scale = scale;
        m_sequential_view.marker.m_scale = scale;
    }
}

bool GCodeViewerData::update_viewer_state_current(unsigned int first, unsigned int last)
{
    auto is_visible = [this](unsigned int id) {
        for (const TBuffer &buffer : m_buffers) {
            if (buffer.visible) {
                for (const Path &path : buffer.paths) {
                    if (path.sub_paths.front().first.s_id <= id && id <= path.sub_paths.back().last.s_id) return true;
                }
            }
        }
        return false;
    };

    const int first_diff = static_cast<int>(first) - static_cast<int>(m_sequential_view.last_current.first);
    const int last_diff  = static_cast<int>(last) - static_cast<int>(m_sequential_view.last_current.last);

    unsigned int new_first = first;
    unsigned int new_last  = last;

    if (m_sequential_view.skip_invisible_moves) {
        while (!is_visible(new_first)) {
            if (first_diff > 0)
                ++new_first;
            else
                --new_first;
        }

        while (!is_visible(new_last)) {
            if (last_diff > 0)
                ++new_last;
            else
                --new_last;
        }
    }

    m_sequential_view.current.first = new_first;
    m_sequential_view.current.last  = new_last;
    m_sequential_view.last_current  = m_sequential_view.current;

    return new_first != first || new_last != last; // return change state
}

void GCodeViewerData::on_change_color_mode(bool is_dark)
{
    m_sequential_view.marker.on_change_color_mode(is_dark);
    m_sequential_view.file.on_change_color_mode(is_dark);
}

};
};
};
#version 140


const vec3 ZERO = vec3(0.0, 0.0, 0.0);

struct SlopeDetection
{
    bool actived;
	float normal_z;
    mat3 volume_world_normal_matrix;
};

uniform mat4 view_model_matrix;
uniform mat4 projection_matrix;
uniform mat3 view_normal_matrix;
uniform mat4 volume_world_matrix;
uniform SlopeDetection slope;

// Clipping plane, x = min z, y = max z. Used by the FFF and SLA previews to clip with a top / bottom plane.
uniform vec2 z_range;
// Clipping plane - general orientation. Used by the SLA gizmo.
uniform vec4 clipping_plane;
// Color clip plane - general orientation. Used by the cut gizmo.
uniform vec4 color_clip_plane;

in vec3 v_position;
in vec3 v_normal;

// x = diffuse, y = specular;
//out vec2 intensity;

//out vec3 clipping_planes_dots;
//out float color_clip_plane_dot;
//out vec3 eye_normal;

out vec4  world_pos;
out vec4  view_pos;
out float world_normal_z;
out vec3  normal;


void main()
{
	// First transform the normal into camera space and normalize the result.
    //eye_normal = normalize(view_normal_matrix * v_normal);

	// Compute the cos of the angle between the normal and lights direction. The light is directional so the direction is constant for every vertex.
	// Since these two are normalized the cosine is the dot product. We also need to clamp the result to the [0,1] range.
	//float NdotL = max(dot(eye_normal, LIGHT_TOP_DIR), 0.0);

	//intensity.x = INTENSITY_AMBIENT + NdotL * LIGHT_TOP_DIFFUSE;
    view_pos = view_model_matrix * vec4(v_position, 1.0);
    //intensity.y = LIGHT_TOP_SPECULAR * pow(max(dot(-normalize(view_pos.xyz), reflect(-LIGHT_TOP_DIR, eye_normal)), 0.0), LIGHT_TOP_SHININESS);

	// Perform the same lighting calculation for the 2nd light source (no specular applied).
	//NdotL = max(dot(eye_normal, LIGHT_FRONT_DIR), 0.0);
	//intensity.x += NdotL * LIGHT_FRONT_DIFFUSE;

    // Point in homogenous coordinates.
    world_pos = volume_world_matrix * vec4(v_position, 1.0);

    // z component of normal vector in world coordinate used for slope shading
    world_normal_z = slope.actived ? (normalize(slope.volume_world_normal_matrix * v_normal)).z : 0.0;
    gl_Position = projection_matrix * view_pos;

    // Fill in the scalars for fragment shader clipping. Fragments with any of these components lower than zero are discarded.
    //clipping_planes_dots = vec3(dot(world_pos, clipping_plane), world_pos.z - z_range.x, z_range.y - world_pos.z);
    //color_clip_plane_dot = dot(world_pos, color_clip_plane);
    normal = v_normal;
}

#version 460
#pragma import_defines(ROCKY_ATMOSPHERE)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// inter-stage interface block
layout(location = 0) in Varyings {
    vec2 uv;
    vec3 normal_vs;
    vec3 vertex_vs;
    vec3 look_ws;
    vec3 camera_ws;
    vec3 sundir_ecef;
    float discardVert;
} vary;

// uniforms (TerrainState.h)
layout(set = 0, binding = 9) uniform TerrainSettings {
    vec4 backgroundColor;
    float atmosphere;
    float lighting;
    float debugTriangles;
    float debugNormals;
} terrain;

layout(set = 0, binding = 11) uniform sampler2D color_tex;

#pragma include "rocky.viewdependentstate.glsl"
#pragma include "rocky.lighting.glsl"
#pragma include "rocky.atmo.glsl"
#pragma include "rocky.debug.frag.glsl"

// outputs
layout(location = 0) out vec4 out_color;

void main()
{
    if (vary.discardVert > 0.0)
        discard;

    // sample the imagery color
    vec4 texel = texture(color_tex, vary.uv);

    // mix in the background color
    out_color = mix(terrain.backgroundColor, clamp(texel, 0, 1), texel.a);

    vec3 normal_vs = normalize(vary.normal_vs);

    // debug normals
    out_color.rgb = mix(out_color.rgb, (normal_vs + 1.0) * 0.5, terrain.debugNormals);

#if defined(ROCKY_ATMOSPHERE)
    vec3 ground_color = apply_atmo_color_to_ground(
        out_color.rgb, vary.vertex_vs, vary.look_ws,
        vary.camera_ws, normalize(vary.sundir_ecef), vds.ellipsoidAxes);

    out_color.rgb = mix(out_color.rgb, ground_color, terrain.lighting * terrain.atmosphere);
#endif

    // PBR lighting
    vec4 lit_color = apply_lighting(out_color, vary.vertex_vs, normal_vs);
    out_color = mix(out_color, lit_color, terrain.lighting);

    // show triangle outlines
    apply_debug_triangles(out_color, terrain.debugTriangles);
}

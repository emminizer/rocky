#version 450
#pragma import_defines(ROCKY_ATMOSPHERE)

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

// rocky::detail::MeshStyleRecord
struct MeshStyle {
    vec4 color;
    float depthOffset;
    uint stipplePattern;
    uint featureMask; // see defines below
    uint _padding[1];
};

#define MASK_HAS_TEXTURE 1
#define MASK_HAS_LIGHTING 2
#define MASK_HAS_PER_VERTEX_COLORS 4

layout(set = 0, binding = 1) uniform MeshUniform {
    MeshStyle style;
} mesh;

layout(location = 1) out Varyings {
    vec4 color;
    vec2 uv;
    vec3 normal;
    vec3 vertex_vs;
    float applyTexture;
    float applyLighting;
    flat uint stipplePattern;
} vary;

// GLSL built-ins
out gl_PerVertex {
    vec4 gl_Position;
    float gl_ClipDistance[1];
};

#pragma include "rocky.viewdependentstate.glsl"
#pragma include "rocky.projection.glsl"
#pragma include "rocky.depthoffset.glsl"

void main()
{    
    bool hasPerVertexColors = (MASK_HAS_PER_VERTEX_COLORS & mesh.style.featureMask) != 0;
    bool hasTexture = (MASK_HAS_TEXTURE & mesh.style.featureMask) != 0;
    bool hasLighting = (MASK_HAS_LIGHTING & mesh.style.featureMask) != 0;

    vary.color = hasPerVertexColors ? in_color : mesh.style.color;
    vary.applyTexture = hasTexture ? 1.0 : 0.0;
    vary.applyLighting = hasLighting ? 1.0 : 0.0;
    vary.stipplePattern = mesh.style.stipplePattern;

    vec4 vertex_vs = pc.modelview * vec4(in_vertex, 1.0);
    
    vertex_vs = apply_projection(vertex_vs);

    mat3 normalMatrix = mat3(transpose(inverse(pc.modelview)));
    vary.normal = normalMatrix * in_normal;
    
    vertex_vs = apply_depth_offset(vertex_vs, mesh.style.depthOffset);

    vary.vertex_vs = vertex_vs.xyz / vertex_vs.w;
    vary.uv = in_uv;

    gl_Position = pc.projection * vertex_vs;
}

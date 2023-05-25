#version 450

#extension GL_GOOGLE_include_directive: require

#include <mesh.glsl>

#define VertexPulling true // Not FixedVertexFunction

layout(set=0, binding=0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

#if (VertexPulling)

layout(set=0, binding=2) readonly buffer Vertices {
    Vertex vertices[];
};

#else

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inCoord; // VK_FORMAT_R16G16_SFLOAT
layout(location = 2) in vec3 inNormal;

#endif

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragCoord;

void main() {

#if (VertexPulling)

    Vertex v_pulling = vertices[gl_VertexIndex];

    vec3 inPosition = v_pulling.position;
    vec3 inNormal = v_pulling.normal;
    vec2 inCoord = v_pulling.coord;

#endif

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragCoord = inCoord;
}
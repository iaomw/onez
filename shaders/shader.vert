#version 450

layout(set=0, binding=0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

#define VertexPulling 1

#if (VertexPulling)

    struct Vertex
    {
        vec3 pos;
        vec3 color;
        vec2 coord;
    };

    layout(set=0, binding=2) readonly buffer Vertices {
        Vertex vertices[];
    };

#else

    layout(location = 0) in vec3 inPosition;
    layout(location = 1) in vec3 inColor;
    layout(location = 2) in vec2 inCoord;

#endif

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {

#if (VertexPulling)

    Vertex v_pulling = vertices[gl_VertexIndex];

    vec3 inPosition = v_pulling.pos;
    vec3 inColor = v_pulling.color;
    vec2 inCoord = v_pulling.coord;

#endif

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragTexCoord = inCoord;
    fragColor = inColor;
}
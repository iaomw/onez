#version 450

#extension GL_EXT_shader_explicit_arithmetic_types_int32: require
#extension GL_EXT_shader_explicit_arithmetic_types_int16: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_NV_mesh_shader: require

#define threadgroup_size 32

// TODO: bad for perf! local_size_x should be 32
layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices = 64, max_primitives = 126) out;

layout(set=0, binding=0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

struct Vertex
{
	vec3 position;
	f16vec2 coord;
	f16vec3 normal;
};

layout(binding = 2) readonly buffer Vertices
{
	Vertex vertices[];
};

struct Meshlet
{
	uint32_t vertices[64];
	uint8_t indices[126 * 3]; // up to 126 triangles

	uint8_t vertexCount;
	//uint8_t indexCount;
	uint8_t triangleCount;
};

layout(binding = 3) readonly buffer Meshlets
{
	Meshlet meshlets[];
};

layout(location = 0) out vec4 color[];

void main()
{
	uint mi = gl_WorkGroupID.x; // threadgroup ID
	uint ti = gl_LocalInvocationID.x; // ID inside threadgroup

	uint vertexCount = uint(meshlets[mi].vertexCount); 
	uint triangleCount = uint(meshlets[mi].triangleCount); 
	uint indexCount = triangleCount * 3; 

	for (uint i = ti; i < vertexCount; i+=32)
	{
		uint vi = meshlets[mi].vertices[i];

		vec3 position = vertices[vi].position;
		vec3 normal = vertices[vi].normal;
		vec2 coord = vertices[vi].coord;  

		gl_MeshVerticesNV[i].gl_Position = ubo.proj * ubo.view * ubo.model * vec4(position, 1.0); 
		//vec4(position * vec3(1, 1, 0.5) + vec3(0, 0, 0.5), 1.0);
		color[i] = vec4(normal, 1.0); //vec4(normal * 0.5 + vec3(0.5), 1.0);
	}

	for (uint i = ti; i < indexCount; i+=32)
	{
		// TODO: possibly bad for perf, consider writePackedPrimitiveIndices4x8NV
		gl_PrimitiveIndicesNV[i] = uint(meshlets[mi].indices[i]);
	}

	if (ti == 0) {
		gl_PrimitiveCountNV = uint(meshlets[mi].triangleCount);
	}
}
#version 450

#extension GL_EXT_shader_explicit_arithmetic_types_int32: require
#extension GL_EXT_shader_explicit_arithmetic_types_int16: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_NV_mesh_shader: require

// TODO: bad for perf! local_size_x should be 32
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices = 64, max_primitives = 42) out;

layout(set=0, binding=0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec2 coord;
};

layout(binding = 2) readonly buffer Vertices
{
	Vertex vertices[];
};

struct Meshlet
{
	uint32_t vertices[64];
	uint8_t indices[126]; // up to 42 triangles

	uint8_t vertexCount;
	uint8_t indexCount;
};

layout(binding = 3) readonly buffer Meshlets
{
	Meshlet meshlets[];
};

layout(location = 0) out vec4 color[];

void main()
{
	uint mi = gl_WorkGroupID.x;

	// TODO: really bad for perf; our workgroup has 1 thread!
	for (uint i = 0; i < uint(meshlets[mi].vertexCount); ++i)
	{
		uint vi = meshlets[mi].vertices[i];

		vec3 position = vertices[vi].position;
		vec3 normal = vertices[vi].normal;
		vec2 coord = vertices[vi].coord;  

		gl_MeshVerticesNV[i].gl_Position = ubo.proj * ubo.view * ubo.model * vec4(position, 1.0); 
		//vec4(position * vec3(1, 1, 0.5) + vec3(0, 0, 0.5), 1.0);
		color[i] = vec4(normal * 0.5 + vec3(0.5), 1.0);
	}

    gl_PrimitiveCountNV = uint(meshlets[mi].indexCount) / 3;

	// TODO: really bad for perf; our workgroup has 1 thread!
	for (uint i = 0; i < uint(meshlets[mi].indexCount); ++i)
	{
		// TODO: possibly bad for perf, consider writePackedPrimitiveIndices4x8NV
		gl_PrimitiveIndicesNV[i] = uint(meshlets[mi].indices[i]);
	}
}
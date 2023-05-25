#version 450
#extension GL_NV_mesh_shader: require
#extension GL_GOOGLE_include_directive: require

#include <mesh.glsl>

#define DEBUG 1

#define threadgroup_size 32

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices = 64, max_primitives = 126) out;

layout(set=0, binding=0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 2) readonly buffer Vertices
{
	Vertex vertices[];
};

struct Meshlet
{
	uint32_t vertices[64];
	uint8_t indices[126*3]; // up to 126 triangles

	uint8_t vertexCount;
	//uint8_t indexCount;
	uint8_t triangleCount;
};

layout(binding = 3) readonly buffer Meshlets
{
	Meshlet meshlets[];
};

layout(location = 0) out vec4 color[];

uint hash(uint a) 
{ 
   a = (a+0x7ed55d16) + (a<<12); 
   a = (a^0xc761c23c) ^ (a>>19); 
   a = (a+0x165667b1) + (a<<5); 
   a = (a+0xd3a2646c) ^ (a<<9); 
   a = (a+0xfd7046c5) + (a<<3); 
   a = (a^0xb55a4f09) ^ (a>>16); 
   return a; 
} 

void main()
{
	uint mi = gl_WorkGroupID.x; // threadgroup ID
	uint ti = gl_LocalInvocationID.x; // ID inside threadgroup

	uint vertexCount = uint(meshlets[mi].vertexCount); 
	uint triangleCount = uint(meshlets[mi].triangleCount); 
	uint indexCount = triangleCount * 3; 

#if DEBUG

	uint mhash = hash(mi); 
	vec3 mcolor = vec3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0; 

#endif

	for (uint i = ti; i < vertexCount; i+=32)
	{
		uint vi = meshlets[mi].vertices[i];

		vec3 position = vertices[vi].position;
		vec3 normal = vertices[vi].normal;
		vec2 coord = vertices[vi].coord;  

		gl_MeshVerticesNV[i].gl_Position = ubo.proj * ubo.view * ubo.model * vec4(position, 1.0); 
		//vec4(position * vec3(1, 1, 0.5) + vec3(0, 0, 0.5), 1.0);
		color[i] = vec4(normal, 1.0); //vec4(normal * 0.5 + vec3(0.5), 1.0);
	#if DEBUG
		color[i].rgb = mcolor;
	#endif
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
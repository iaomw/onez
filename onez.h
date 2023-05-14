#pragma once 

// #ifndef ONEZH
// #define ONEZH

// #include <vulkan/vulkan.h>
// #include <vulkan/vulkan_core.h>

#include <iostream>
#include "../shaders/mesh.glsl"

#define VK_CHECK(x)                                                    \
	do                                                                 \
	{                                                                  \
		VkResult err = x;                                              \
		if (err != VK_SUCCESS && err != VK_NOT_READY)                                         \
		{                                                              \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
			abort();                                                   \
		}                                                              \
	} while (0)


#define ARRAYSIZE(array) ( sizeof(array)/sizeof(array[0]) )

#define VertexPulling 1
#define MeshShading 0

// #endif
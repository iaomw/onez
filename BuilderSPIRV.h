#pragma once 

#include "onez.h"
#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <filesystem>

// #include <glslang/Include/ResourceLimits.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/glslang_c_shader_types.h>

#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>

// #include "spirv_reflect.h"

#ifdef __linux__
#include <spirv/unified1/spirv.h>
#elif VK_HEADER_VERSION >= 135
#include <spirv-headers/spirv.h>
#else
#include <vulkan/spirv.h>
#endif

struct _Shader final
{
	VkShaderModule vkModule {};
    VkShaderStageFlagBits stage {};

    VkDescriptorType resourceTypes[32] {};
	uint32_t resourceMask {};

	uint32_t localSizeX {};
	uint32_t localSizeY {};
	uint32_t localSizeZ {};

	bool usesPushConstants = false;

	std::vector<uint32_t> SPIRV {};
};

struct _Program
{
	VkPipelineBindPoint bindPoint;
	VkPipelineLayout layout;
	VkDescriptorSetLayout setLayout;
	VkDescriptorUpdateTemplate updateTemplate;
	VkShaderStageFlags pushConstantStages;
};

struct DescriptorInfo
{
	union
	{
		VkDescriptorImageInfo image;
		VkDescriptorBufferInfo buffer;
	};

	DescriptorInfo()
	{
	}

	DescriptorInfo(VkImageView imageView, VkImageLayout imageLayout)
	{
		image.sampler = VK_NULL_HANDLE;
		image.imageView = imageView;
		image.imageLayout = imageLayout;
	}

	DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
	{
		image.sampler = sampler;
		image.imageView = imageView;
		image.imageLayout = imageLayout;
	}

	DescriptorInfo(VkBuffer buffer_, VkDeviceSize offset, VkDeviceSize range)
	{
		buffer.buffer = buffer_;
		buffer.offset = offset;
		buffer.range = range;
	}

	DescriptorInfo(VkBuffer buffer_)
	{
		buffer.buffer = buffer_;
		buffer.offset = 0;
		buffer.range = VK_WHOLE_SIZE;
	}
};


int SpirvReflectExample(const void* spirv_code, size_t spirv_nbytes);

bool loadinShader(_Shader& shader, VkDevice device, const char* path);
bool loadinShader(_Shader& shader, VkDevice device, const std::filesystem::path root_path, const char* path);

void parseShader(_Shader& shader, const uint32_t* code, uint32_t codeSize);

using _Shaders = std::initializer_list<const _Shader*>;
using _Constants = std::initializer_list<int>;

VkPipeline createGraphicsPipelineVK13(VkDevice device, VkPipelineCache pipelineCache, const VkPipelineRenderingCreateInfo& renderingInfo, _Shaders shaders, VkPipelineLayout layout, _Constants constants = {});
VkPipeline createComputePipeline(VkDevice device, VkPipelineCache pipelineCache, const _Shader& shader, VkPipelineLayout layout, _Constants constants = {});


std::string readFileGLSL(const char* fileName);
bool saveFileSPIRV(const char* filename, unsigned int* code, size_t size);

static std::vector<char> readFileSPIRV(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + filename);
	}

	size_t fileSize = (size_t) file.tellg();

	auto remain = fileSize%4;

	size_t arraySize = remain ? (fileSize-remain+4) : fileSize;

	std::vector<char> buffer(arraySize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

// glslang_stage_t glslangShaderStageFromFileName(const char* fileName);

size_t compileShaderData(glslang_stage_t stage, const char* shaderSource, _Shader& _shader);
size_t compileShaderFile(const char* file, _Shader& _shader);

VkShaderModule createVkShaderModule(const std::vector<uint32_t>& code, VkDevice device);

void testShaderCompilation(const char* sourceFilename, const char* destFilename);
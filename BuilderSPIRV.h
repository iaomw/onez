#pragma once 

#include "onez.h"

#include <cstdint>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

// #include <glslang/Include/ResourceLimits.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/glslang_c_shader_types.h>

#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/ResourceLimits.h>

#include "spirv_reflect.h"

struct FatShaderModule final
{
	std::vector<uint32_t> SPIRV;

	VkShaderModule shaderModule = nullptr;
    VkShaderStageFlagBits stage;

    VkDescriptorType resourceTypes[32];
	uint32_t resourceMask;

	uint32_t localSizeX;
	uint32_t localSizeY;
	uint32_t localSizeZ;

	bool usesPushConstants;
};

struct Program
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

#pragma mark - SPIRV reflect

int SpirvReflectExample(const void* spirv_code, size_t spirv_nbytes);

bool loadinShader(FatShaderModule& shader, VkDevice device, const char* path);
void parseShader(FatShaderModule& shader, const uint32_t* code, uint32_t codeSize);

using Shaders = std::initializer_list<const FatShaderModule*>;
using Constants = std::initializer_list<int>;

VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, const VkPipelineRenderingCreateInfo& renderingInfo, Shaders shaders, VkPipelineLayout layout, Constants constants = {});
VkPipeline createComputePipeline(VkDevice device, VkPipelineCache pipelineCache, const FatShaderModule& shader, VkPipelineLayout layout, Constants constants = {});

Program createProgram(VkDevice device, VkPipelineBindPoint bindPoint, Shaders shaders, size_t pushConstantSize, bool pushDescriptorsSupported);
void destroyProgram(VkDevice device, const Program& program);

#pragma mark - SPIRV compile

std::string readShaderFile(const char* fileName);
bool saveFileSPIRV(const char* filename, unsigned int* code, size_t size);

// glslang_stage_t glslangShaderStageFromFileName(const char* fileName);

size_t compileShader(glslang_stage_t stage, const char* shaderSource, FatShaderModule& shaderModule);
size_t compileShaderFile(const char* file, FatShaderModule& shaderModule);

void testShaderCompilation(const char* sourceFilename, const char* destFilename);
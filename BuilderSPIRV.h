#pragma once 

#include <cstdint>
#include <vulkan/vulkan.h>

// #include <glslang/Include/ResourceLimits.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/glslang_c_shader_types.h>

#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/ResourceLimits.h>

// #include "spirv_reflect.h"

struct ShaderModule final
{
	std::vector<uint32_t> SPIRV;
	VkShaderModule shaderModule = nullptr;
};

std::string readShaderFile(const char* fileName);

bool saveFileBinarySPIRV(const char* filename, unsigned int* code, size_t size);

glslang_stage_t glslangShaderStageFromFileName(const char* fileName);

static size_t compileShader(glslang_stage_t stage, const char* shaderSource, ShaderModule& shaderModule);

size_t compileShaderFile(const char* file, ShaderModule& shaderModule);

void testShaderCompilation(const char* sourceFilename, const char* destFilename);
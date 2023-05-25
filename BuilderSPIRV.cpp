#include "BuilderSPIRV.h"

#include <cstring>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <filesystem>

#include <stdio.h>
#include <vector>
#include <algorithm>

// #include "spirv_reflect.c"

std::string readFileGLSL(const char* filePath)
{
	FILE* file = fopen(filePath, "r");

	if (!file)
	{
		printf("I/O error. Cannot open shader file '%s'\n", filePath);
		return std::string();
	}

	auto _filePath = std::filesystem::path(filePath);
	_filePath = _filePath.remove_filename();

	fseek(file, 0L, SEEK_END);
	const auto bytesinfile = ftell(file);
	fseek(file, 0L, SEEK_SET);

	char* buffer = (char*)alloca(bytesinfile + 1);
	const size_t bytesread = fread(buffer, 1, bytesinfile, file);
	fclose(file);

	buffer[bytesread] = 0;

	static constexpr unsigned char BOM[] = { 0xEF, 0xBB, 0xBF };

	if (bytesread > 3)
	{
		if (!memcmp(buffer, BOM, 3))
			memset(buffer, ' ', 3);
	}

	std::string code(buffer);
	
	size_t offset = 0;
	std::string result;

	while (code.find("#include", offset) != code.npos)
	{		
		const auto pos = code.find("#include", offset);
		const auto end = code.find("\n", pos);

		result += code.substr(offset, pos-offset);
		offset = end + 1;

		const auto macro_line = code.substr(pos, end-pos+1);

		auto p1 = macro_line.find('<', 0);
		auto p2 = macro_line.find('>', 0);

		if (p1 == macro_line.npos || p2 == macro_line.npos || p2 <= p1)
		{
			p1 = macro_line.find('\"', 0);
			p2 = macro_line.find('\"', p1+1);
		}

		if (p1 == macro_line.npos || p2 == macro_line.npos || p2 <= p1)
		{
			printf("Error while loading shader program:\n%s\n", code.c_str());
			return std::string();
		}

		const std::string name = macro_line.substr(p1 + 1, p2 - p1 - 1);

		auto work_path = _filePath;
		work_path.append(name);

		result += readFileGLSL(work_path.string().c_str());
	}

	result += code.substr(offset);
	return result;
}

int endsWith(const char* s, const char* part)
{
	return (strstr( s, part ) - s) == (strlen( s ) - strlen( part ));
}

static const std::map<std::string, glslang_stage_t> STAGE_MAP {
	{".vert", GLSLANG_STAGE_VERTEX  },
	{".frag", GLSLANG_STAGE_FRAGMENT},
	{".geom", GLSLANG_STAGE_GEOMETRY},

	{".comp", GLSLANG_STAGE_COMPUTE },

	{".tesc", GLSLANG_STAGE_TESSCONTROL   },
	{".tese", GLSLANG_STAGE_TESSEVALUATION},

	{".task", GLSLANG_STAGE_TASK},
	{".mesh", GLSLANG_STAGE_MESH}
};

glslang_stage_t glslangShaderStageFromFileName(const char* filePath)
{
	auto fileName = std::filesystem::path(filePath).filename();
	auto extension = fileName.extension().string();

	static const auto extensionGLSL = std::string(".glsl");

	auto isGLSL = (extensionGLSL == extension);

	if (isGLSL) {
		extension = fileName.stem().extension().string();
	}

	if (STAGE_MAP.count(extension)) {
		auto x = STAGE_MAP.at(extension);
		return x;
	} 

	return GLSLANG_STAGE_FRAGMENT;
}

void printShaderSource(const char* text)
{
	int line = 1;

	printf("\n(%3i) ", line);

	while (text && *text++)
	{
		if (*text == '\n')
		{
			printf("\n(%3i) ", ++line);
		}
		else if (*text == '\r')
		{
		}
		else
		{
			printf("%c", *text);
		}
	}

	printf("\n");
}

size_t compileShaderData(glslang_stage_t stage, const char* shaderSource, _Shader& _shader)
{
#if defined(_MSC_VER)
	const glslang_input_t input =
	{
		GLSLANG_SOURCE_GLSL,
		stage,
		GLSLANG_CLIENT_VULKAN,
		GLSLANG_TARGET_VULKAN_1_2,
		GLSLANG_TARGET_SPV,
		GLSLANG_TARGET_SPV_1_3,
		shaderSource,
		100,
		GLSLANG_NO_PROFILE,
		false,
		false,
		GLSLANG_MSG_DEFAULT_BIT,
		(const glslang_resource_t*)GetDefaultResources()
	};
#else 
	const glslang_input_t input =
	{
		.language = GLSLANG_SOURCE_GLSL,
		.stage = stage,
		.client = GLSLANG_CLIENT_VULKAN,
		.client_version = GLSLANG_TARGET_VULKAN_1_2,
		.target_language = GLSLANG_TARGET_SPV,
		.target_language_version = GLSLANG_TARGET_SPV_1_3,
		.code = shaderSource,
		.default_version = 100,
		.default_profile = GLSLANG_NO_PROFILE,
		.force_default_version_and_profile = false,
		.forward_compatible = false,
		.messages = GLSLANG_MSG_DEFAULT_BIT,
		.resource = (const glslang_resource_t*)GetDefaultResources()
	};
#endif

	glslang_shader_t* shader = glslang_shader_create(&input);

	if (!glslang_shader_preprocess(shader, &input))
	{
		fprintf(stderr, "GLSL preprocessing failed\n");
		fprintf(stderr, "\n%s", glslang_shader_get_info_log(shader));
		fprintf(stderr, "\n%s", glslang_shader_get_info_debug_log(shader));
		printShaderSource(input.code);
		return 0;
	}

	if (!glslang_shader_parse(shader, &input))
	{
		fprintf(stderr, "GLSL parsing failed\n");
		fprintf(stderr, "\n%s", glslang_shader_get_info_log(shader));
		fprintf(stderr, "\n%s", glslang_shader_get_info_debug_log(shader));
		printShaderSource(glslang_shader_get_preprocessed_code(shader));
		return 0;
	}

	glslang_program_t* program = glslang_program_create();
	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
	{
		fprintf(stderr, "GLSL linking failed\n");
		fprintf(stderr, "\n%s", glslang_program_get_info_log(program));
		fprintf(stderr, "\n%s", glslang_program_get_info_debug_log(program));
		return 0;
	}

	glslang_program_SPIRV_generate(program, stage);

	_shader.SPIRV.resize(glslang_program_SPIRV_get_size(program));
	glslang_program_SPIRV_get(program, _shader.SPIRV.data());

	{
		const char* spirv_messages =
			glslang_program_SPIRV_get_messages(program);

		if (spirv_messages)
			fprintf(stderr, "%s", spirv_messages);
	}

	glslang_program_delete(program);
	glslang_shader_delete(shader);

	return _shader.SPIRV.size();
}

size_t compileShaderFile(const char* file, _Shader& _shader)
{
	if (auto shaderSource = readFileGLSL(file); !shaderSource.empty()) {

		const auto stage = glslangShaderStageFromFileName(file);

		if (stage == GLSLANG_STAGE_VERTEX) {

			auto lineBegin = shaderSource.find("#define VertexPulling", 0);
			auto lineEnd = shaderSource.find_first_of("\n", lineBegin);;

			if (lineBegin != 0 && lineEnd > lineBegin) {
				#if VertexPulling
					auto s = "#define VertexPulling 1";
				#else 
					auto s = "#define VertexPulling 0";
				#endif

				shaderSource.replace(lineBegin, lineEnd-lineBegin, s);
			}
		}

		return compileShaderData(stage, shaderSource.c_str(), _shader);
	}

	return 0;
}

VkShaderModule createVkShaderModule(const std::vector<uint32_t>& code, VkDevice device) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size() * sizeof(uint32_t);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

bool saveFileSPIRV(const char* filename, unsigned int* code, size_t size)
{
	FILE* f = fopen(filename, "w");

	if (!f)
		return false;

	fwrite(code, sizeof(uint32_t), size, f);
	fclose(f);

	return true;
}

void testShaderCompilation(const char* sourceFilename, const char* destFilename)
{

	if (endsWith(sourceFilename, "spv")) {
		auto data = readFileSPIRV(sourceFilename);

		_Shader ss {};

		parseShader(ss, reinterpret_cast<const uint32_t*>(data.data()), (uint32_t)data.size()/4);

		return;
	}

	_Shader _shader;

	if (compileShaderFile(sourceFilename, _shader) < 1)
		return;

	parseShader(_shader, _shader.SPIRV.data(), _shader.SPIRV.size());

	saveFileSPIRV(destFilename, _shader.SPIRV.data(), _shader.SPIRV.size());
}

int SpirvReflectExample(const void* spirv_code, size_t spirv_nbytes)
{
    // // Generate reflection data for a shader
    // SpvReflectShaderModule module;
    // SpvReflectResult result = spvReflectCreateShaderModule(spirv_nbytes, spirv_code, &module);
    // assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // // Enumerate and extract shader's input variables
    // uint32_t var_count = 0;
    // result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
    // assert(result == SPV_REFLECT_RESULT_SUCCESS);
    // SpvReflectInterfaceVariable** input_vars =
    // (SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
    // result = spvReflectEnumerateInputVariables(&module, &var_count, input_vars);
    // assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // // Output variables, descriptor bindings, descriptor sets, and push constants
    // // can be enumerated and extracted using a similar mechanism.

    // // Destroy the reflection data when no longer required.
    // spvReflectDestroyShaderModule(&module);

    return 0;
}

// https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.pdf
struct Id
{
	uint32_t opcode {};
	uint32_t typeId {};
	uint32_t storageClass {};
	uint32_t binding {};
	uint32_t set {};
	uint32_t constant {};
};

static VkShaderStageFlagBits getShaderStage(SpvExecutionModel executionModel)
{
	switch (executionModel)
	{
	case SpvExecutionModelVertex:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case SpvExecutionModelFragment:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case SpvExecutionModelGLCompute:
		return VK_SHADER_STAGE_COMPUTE_BIT;

	case SpvExecutionModelTaskEXT:
		return VK_SHADER_STAGE_TASK_BIT_EXT;
	case SpvExecutionModelMeshEXT:
		return VK_SHADER_STAGE_MESH_BIT_EXT;

	case SpvExecutionModelTaskNV:
		return VK_SHADER_STAGE_TASK_BIT_NV;
	case SpvExecutionModelMeshNV:
		return VK_SHADER_STAGE_MESH_BIT_NV;

	default:
		assert(!"Unsupported execution model");
		return VkShaderStageFlagBits(0);
	}
}

static VkDescriptorType getDescriptorType(SpvOp op)
{
	switch (op)
	{
	case SpvOpTypeStruct:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case SpvOpTypeImage:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case SpvOpTypeSampler:
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	case SpvOpTypeSampledImage:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	default:
		assert(!"Unknown resource type");
		return VkDescriptorType(0);
	}
}

void parseShader(_Shader& shader, const uint32_t* code, uint32_t codeSize)
{
	assert(code[0] == SpvMagicNumber);

	uint32_t idBound = code[3];

	std::vector<Id> ids(idBound);

	int localSizeIdX = -1;
	int localSizeIdY = -1;
	int localSizeIdZ = -1;

	const uint32_t* insn = code + 5;

	while (insn != code + codeSize)
	{
		uint16_t opcode = uint16_t(insn[0]);
		uint16_t wordCount = uint16_t(insn[0] >> 16);

		switch (opcode)
		{
		case SpvOpEntryPoint:
		{
			assert(wordCount >= 2);
			shader.stage = getShaderStage(SpvExecutionModel(insn[1]));
		} break;
		case SpvOpExecutionMode:
		{
			assert(wordCount >= 3);
			uint32_t mode = insn[2];

			switch (mode)
			{
			case SpvExecutionModeLocalSize:
				assert(wordCount == 6);
				shader.localSizeX = insn[3];
				shader.localSizeY = insn[4];
				shader.localSizeZ = insn[5];
				break;
			}
		} break;
		case SpvOpExecutionModeId:
		{
			assert(wordCount >= 3);
			uint32_t mode = insn[2];

			switch (mode)
			{
			case SpvExecutionModeLocalSizeId:
				assert(wordCount == 6);
				localSizeIdX = int(insn[3]);
				localSizeIdY = int(insn[4]);
				localSizeIdZ = int(insn[5]);
				break;
			}
		} break;
		case SpvOpDecorate:
		{
			assert(wordCount >= 3);

			uint32_t id = insn[1];
			assert(id < idBound);

			switch (insn[2])
			{
			case SpvDecorationDescriptorSet:
				assert(wordCount == 4);
				ids[id].set = insn[3];
				break;
			case SpvDecorationBinding:
				assert(wordCount == 4);
				ids[id].binding = insn[3];
				break;
			}
		} break;
		case SpvOpTypeStruct:
		case SpvOpTypeImage:
		case SpvOpTypeSampler:
		case SpvOpTypeSampledImage:
		{
			assert(wordCount >= 2);

			uint32_t id = insn[1];
			assert(id < idBound);

			assert(ids[id].opcode == 0);
			ids[id].opcode = opcode;
		} break;
		case SpvOpTypePointer:
		{
			assert(wordCount == 4);

			uint32_t id = insn[1];
			assert(id < idBound);

			assert(ids[id].opcode == 0);
			ids[id].opcode = opcode;
			ids[id].typeId = insn[3];
			ids[id].storageClass = insn[2];
		} break;
		case SpvOpConstant:
		{
			assert(wordCount >= 4); // we currently only correctly handle 32-bit integer constants

			uint32_t id = insn[2];
			assert(id < idBound);

			assert(ids[id].opcode == 0);
			ids[id].opcode = opcode;
			ids[id].typeId = insn[1];
			ids[id].constant = insn[3]; // note: this is the value, not the id of the constant
		} break;
		case SpvOpVariable:
		{
			assert(wordCount >= 4);

			uint32_t id = insn[2];
			assert(id < idBound);

			assert(ids[id].opcode == 0);
			ids[id].opcode = opcode;
			ids[id].typeId = insn[1];
			ids[id].storageClass = insn[3];
		} break;
		}

		assert(insn + wordCount <= code + codeSize);
		insn += wordCount;
	}

	for (auto& id : ids)
	{
		if (id.opcode == SpvOpVariable && (id.storageClass == SpvStorageClassUniform || id.storageClass == SpvStorageClassUniformConstant || id.storageClass == SpvStorageClassStorageBuffer))
		{
			assert(id.set == 0);
			assert(id.binding < 32);
			assert(ids[id.typeId].opcode == SpvOpTypePointer);

			uint32_t typeKind = ids[ids[id.typeId].typeId].opcode;
			VkDescriptorType resourceType = getDescriptorType(SpvOp(typeKind));

			assert((shader.resourceMask & (1 << id.binding)) == 0 || shader.resourceTypes[id.binding] == resourceType);

			shader.resourceTypes[id.binding] = resourceType;
			shader.resourceMask |= 1 << id.binding;
		}

		if (id.opcode == SpvOpVariable && id.storageClass == SpvStorageClassPushConstant)
		{
			shader.usesPushConstants = true;
		}
	}

	if (shader.stage == VK_SHADER_STAGE_COMPUTE_BIT)
	{
		if (localSizeIdX >= 0)
		{
			assert(ids[localSizeIdX].opcode == SpvOpConstant);
			shader.localSizeX = ids[localSizeIdX].constant;
		}

		if (localSizeIdY >= 0)
		{
			assert(ids[localSizeIdY].opcode == SpvOpConstant);
			shader.localSizeY = ids[localSizeIdY].constant;
		}

		if (localSizeIdZ >= 0)
		{
			assert(ids[localSizeIdZ].opcode == SpvOpConstant);
			shader.localSizeZ = ids[localSizeIdZ].constant;
		}

		assert(shader.localSizeX && shader.localSizeY && shader.localSizeZ);
	}
}

bool loadinShader(_Shader& shader, VkDevice device, const char* path) {

	auto size = compileShaderFile(path, shader);
	if (size < 1 ) return false;

	shader.vkModule = createVkShaderModule(shader.SPIRV, device);
	parseShader(shader, shader.SPIRV.data(), shader.SPIRV.size());

	return true;
}

bool loadinShader(_Shader& shader, VkDevice device, const std::filesystem::path root_path, const char* path) {
	auto file_path = root_path / path;
	return loadinShader(shader, device, file_path.string().c_str());
}

static VkSpecializationInfo fillSpecializationInfo(std::vector<VkSpecializationMapEntry>& entries, const _Constants& constants)
{
	for (size_t i = 0; i < constants.size(); ++i)
		entries.push_back({ uint32_t(i), uint32_t(i * 4), 4 });

	VkSpecializationInfo result = {};
	result.mapEntryCount = uint32_t(entries.size());
	result.pMapEntries = entries.data();
	result.dataSize = constants.size() * sizeof(int);
	result.pData = constants.begin();

	return result;
}

VkPipeline createComputePipeline(VkDevice device, VkPipelineCache pipelineCache, const _Shader &shader, VkPipelineLayout layout, _Constants constants) 
{
	assert(shader.stage == VK_SHADER_STAGE_COMPUTE_BIT);

	VkComputePipelineCreateInfo createInfo {};

	std::vector<VkSpecializationMapEntry> specializationEntries;
	VkSpecializationInfo specializationInfo = fillSpecializationInfo(specializationEntries, constants);

	VkPipelineShaderStageCreateInfo stage {};

	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = shader.stage;
	stage.module = shader.vkModule;

	stage.pName = "main";
	stage.pSpecializationInfo = &specializationInfo;

	createInfo.stage = stage;
	createInfo.layout = layout;

	VkPipeline pipeline = 0;
	VK_CHECK(vkCreateComputePipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));

	return pipeline;
}

static uint32_t gatherResources(_Shaders shaders, VkDescriptorType(&resourceTypes)[32])
{
	uint32_t resourceMask = 0;

	for (const auto* shader : shaders)
	{
		for (uint32_t i = 0; i < 32; ++i)
		{
			if (shader->resourceMask & (1 << i))
			{
				if (resourceMask & (1 << i))
				{
					assert(resourceTypes[i] == shader->resourceTypes[i]);
				}
				else
				{
					resourceTypes[i] = shader->resourceTypes[i];
					resourceMask |= 1 << i;
				}
			}
		}
	}

	return resourceMask;
}

VkDescriptorSetLayout createSetLayout(VkDevice device, _Shaders shaders, bool pushDescriptorsSupported) 
{
	std::vector<VkDescriptorSetLayoutBinding> setBindings;

	VkDescriptorType resourceTypes[32] {};
	uint32_t resourceMask = gatherResources(shaders, resourceTypes);

	for (uint32_t i=0; i<32; ++i) {
		if (resourceMask & (1<<i)) {

			VkDescriptorSetLayoutBinding binding = {};
			binding.binding = i;
			binding.descriptorType = resourceTypes[i];
			binding.descriptorCount = 1;

			binding.stageFlags = 0;
			for (const auto* shader : shaders)
				if (shader->resourceMask & (1 << i))
					binding.stageFlags |= shader->stage;

			setBindings.push_back(binding);
		}
	} // for

	VkDescriptorSetLayoutCreateInfo setCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	setCreateInfo.flags = pushDescriptorsSupported ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
	setCreateInfo.bindingCount = uint32_t(setBindings.size());
	setCreateInfo.pBindings = setBindings.data();

	VkDescriptorSetLayout setLayout = 0;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &setCreateInfo, 0, &setLayout));

	return setLayout;
}

VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout, VkShaderStageFlags pushConstantStages, size_t pushConstantSize)
{
	VkPipelineLayoutCreateInfo createInfo {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	createInfo.setLayoutCount = 1;
	createInfo.pSetLayouts = &setLayout;

	if (pushConstantSize) 
	{
		VkPushConstantRange pushConstantRange {};

		pushConstantRange.stageFlags = pushConstantStages;
		pushConstantRange.size = uint32_t(pushConstantSize);

		createInfo.pushConstantRangeCount = 1;
		createInfo.pPushConstantRanges = &pushConstantRange;
	}

	VkPipelineLayout layout = 0;
	VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));

	return layout;
}

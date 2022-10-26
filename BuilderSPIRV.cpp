#include "BuilderSPIRV.h"
#include "glslang_c_shader_types.h"

#include <StandAlone/ResourceLimits.cpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "onez.h"
#include "spirv_reflect.c"

std::string readShaderFile(const char* fileName)
{
	FILE* file = fopen(fileName, "r");

	if (!file)
	{
		printf("I/O error. Cannot open shader file '%s'\n", fileName);
		return std::string();
	}

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

	while (code.find("#include ") != code.npos)
	{
		const auto pos = code.find("#include ");
		const auto p1 = code.find('<', pos);
		const auto p2 = code.find('>', pos);
		if (p1 == code.npos || p2 == code.npos || p2 <= p1)
		{
			printf("Error while loading shader program: %s\n", code.c_str());
			return std::string();
		}
		const std::string name = code.substr(p1 + 1, p2 - p1 - 1);
		const std::string include = readShaderFile(name.c_str());
		code.replace(pos, p2-pos+1, include.c_str());
	}

	return code;
}

int endsWith(const char* s, const char* part)
{
	return (strstr( s, part ) - s) == (strlen( s ) - strlen( part ));
}

glslang_stage_t glslangShaderStageFromFileName(const char* fileName)
{
	if (endsWith(fileName, ".vert"))
		return GLSLANG_STAGE_VERTEX;

	if (endsWith(fileName, ".frag"))
		return GLSLANG_STAGE_FRAGMENT;

	if (endsWith(fileName, ".geom"))
		return GLSLANG_STAGE_GEOMETRY;

	if (endsWith(fileName, ".comp"))
		return GLSLANG_STAGE_COMPUTE;

	if (endsWith(fileName, ".tesc"))
		return GLSLANG_STAGE_TESSCONTROL;

	if (endsWith(fileName, ".tese"))
		return GLSLANG_STAGE_TESSEVALUATION;

	return GLSLANG_STAGE_VERTEX;
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

size_t compileShader(glslang_stage_t stage, const char* shaderSource, FatShaderModule& shaderModule)
{
	const glslang_input_t input =
	{
		.language = GLSLANG_SOURCE_GLSL,
		.stage = stage,
		.client = GLSLANG_CLIENT_VULKAN,
		.client_version = GLSLANG_TARGET_VULKAN_1_1,
		.target_language = GLSLANG_TARGET_SPV,
		.target_language_version = GLSLANG_TARGET_SPV_1_3,
		.code = shaderSource,
		.default_version = 100,
		.default_profile = GLSLANG_NO_PROFILE,
		.force_default_version_and_profile = false,
		.forward_compatible = false,
		.messages = GLSLANG_MSG_DEFAULT_BIT,
		.resource = (const glslang_resource_t*)&glslang::DefaultTBuiltInResource,
	};

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

	shaderModule.SPIRV.resize(glslang_program_SPIRV_get_size(program));
	glslang_program_SPIRV_get(program, shaderModule.SPIRV.data());

	{
		const char* spirv_messages =
			glslang_program_SPIRV_get_messages(program);

		if (spirv_messages)
			fprintf(stderr, "%s", spirv_messages);
	}

	glslang_program_delete(program);
	glslang_shader_delete(shader);

	return shaderModule.SPIRV.size();
}

size_t compileShaderFile(const char* file, FatShaderModule& shaderModule)
{
	if (auto shaderSource = readShaderFile(file); !shaderSource.empty())
		return compileShader(glslangShaderStageFromFileName(file), shaderSource.c_str(), shaderModule);

	return 0;
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
	FatShaderModule shaderModule;

	if (compileShaderFile(sourceFilename, shaderModule) < 1)
		return;

	parseShader(shaderModule, shaderModule.SPIRV.data(), shaderModule.SPIRV.size());

	saveFileSPIRV(destFilename, shaderModule.SPIRV.data(), shaderModule.SPIRV.size());
}

int SpirvReflectExample(const void* spirv_code, size_t spirv_nbytes)
{
    // Generate reflection data for a shader
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirv_nbytes, spirv_code, &module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // Enumerate and extract shader's input variables
    uint32_t var_count = 0;
    result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    SpvReflectInterfaceVariable** input_vars =
    (SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
    result = spvReflectEnumerateInputVariables(&module, &var_count, input_vars);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // Output variables, descriptor bindings, descriptor sets, and push constants
    // can be enumerated and extracted using a similar mechanism.

    // Destroy the reflection data when no longer required.
    spvReflectDestroyShaderModule(&module);

    return 0;
}

// https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.pdf
struct Id
{
	uint32_t opcode;
	uint32_t typeId;
	uint32_t storageClass;
	uint32_t binding;
	uint32_t set;
	uint32_t constant;
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
	case SpvExecutionModelTaskNV:
		return VK_SHADER_STAGE_TASK_BIT_NV;
	case SpvExecutionModelMeshNV:
		return VK_SHADER_STAGE_MESH_BIT_NV;

	default:
		assert(!"Unsupported execution model");
		return VkShaderStageFlagBits(0);
	}
}

void parseShader(FatShaderModule& shader, const uint32_t* code, uint32_t codeSize)
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

			assert((shader.resourceMask & (1 << id.binding)) == 0);

			uint32_t typeKind = ids[ids[id.typeId].typeId].opcode;

			switch (typeKind)
			{
			case SpvOpTypeStruct:
				shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				shader.resourceMask |= 1 << id.binding;
				break;
			case SpvOpTypeImage:
				shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				shader.resourceMask |= 1 << id.binding;
				break;
			case SpvOpTypeSampler:
				shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_SAMPLER;
				shader.resourceMask |= 1 << id.binding;
				break;
			case SpvOpTypeSampledImage:
				shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				shader.resourceMask |= 1 << id.binding;
				break;
			default:
				assert(!"Unknown resource type");
			}
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

static VkSpecializationInfo fillSpecializationInfo(std::vector<VkSpecializationMapEntry>& entries, const Constants& constants)
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

VkPipeline createComputePipeline(VkDevice device, VkPipelineCache pipelineCache, const FatShaderModule &shader, VkPipelineLayout layout, Constants constants) 
{
	assert(shader.stage == VK_SHADER_STAGE_COMPUTE_BIT);

	VkComputePipelineCreateInfo createInfo {};

	std::vector<VkSpecializationMapEntry> specializationEntries;
	VkSpecializationInfo specializationInfo = fillSpecializationInfo(specializationEntries, constants);

	VkPipelineShaderStageCreateInfo stage {};

	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = shader.stage;
	stage.module = shader.shaderModule;

	stage.pName = "main";
	stage.pSpecializationInfo = &specializationInfo;

	createInfo.stage = stage;
	createInfo.layout = layout;

	VkPipeline pipeline = 0;
	VK_CHECK(vkCreateComputePipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));

	return pipeline;
}

VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, const VkPipelineRenderingCreateInfo&renderingInfo, Shaders shaders, VkPipelineLayout layout, Constants constants)
{
	std::vector<VkSpecializationMapEntry> specializationEntries;
	VkSpecializationInfo specializationInfo = fillSpecializationInfo(specializationEntries, constants);

	VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

	VkPipeline pipeline = 0;

	std::vector<VkPipelineShaderStageCreateInfo> stages;

	for (const auto shader : shaders)
	{
		VkPipelineShaderStageCreateInfo stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage = shader->stage;
		stage.module = shader->shaderModule;
		stage.pName = "main";
		stage.pSpecializationInfo = &specializationInfo;

		stages.push_back(stage);
	}

	createInfo.stageCount = uint32_t(stages.size());
	createInfo.pStages = stages.data();

		VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	createInfo.pVertexInputState = &vertexInput;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	createInfo.pInputAssemblyState = &inputAssembly;

		VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
	createInfo.pViewportState = &viewportState;

		VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizationState.lineWidth = 1.f;
		rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	createInfo.pRasterizationState = &rasterizationState;

		VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.pMultisampleState = &multisampleState;

		VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencilState.depthTestEnable = true;
		depthStencilState.depthWriteEnable = true;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER;
	createInfo.pDepthStencilState = &depthStencilState;

	VkPipelineColorBlendAttachmentState colorAttachmentState = {};
	colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &colorAttachmentState;
	createInfo.pColorBlendState = &colorBlendState;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;
	createInfo.pDynamicState = &dynamicState;

	createInfo.layout = layout;
	createInfo.pNext = &renderingInfo;

	VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));

	return pipeline;
}

static uint32_t gatherResources(Shaders shaders, VkDescriptorType(&resourceTypes)[32])
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

VkDescriptorSetLayout createSetLayout(VkDevice device, Shaders shaders, bool pushDescriptorsSupported) 
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

VkDescriptorUpdateTemplate createUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, VkDescriptorSetLayout setLayout, Shaders shaders, bool pushDescriptorsSupported)
{
	std::vector<VkDescriptorUpdateTemplateEntry> entries;

	VkDescriptorType resourceTypes[32] = {};
	uint32_t resourceMask = gatherResources(shaders, resourceTypes);

	for (uint32_t i=0; i<32; ++i) {
		if (resourceMask & (1 << i)) {
			VkDescriptorUpdateTemplateEntry entry {};
			entry.dstBinding = i;
			entry.dstArrayElement = 0;
			entry.descriptorCount = 1;
			entry.descriptorType = resourceTypes[i];
			entry.offset = sizeof(DescriptorInfo) * i;
			entry.stride = sizeof(DescriptorInfo);

			entries.push_back(entry);
		}
	} //for

	VkDescriptorUpdateTemplateCreateInfo createInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };

	createInfo.descriptorUpdateEntryCount = uint32_t(entries.size());
	createInfo.pDescriptorUpdateEntries = entries.data();

	createInfo.templateType = pushDescriptorsSupported ? VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR : VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;

	createInfo.descriptorSetLayout = pushDescriptorsSupported ? 0 : setLayout;
	createInfo.pipelineBindPoint = bindPoint;
	createInfo.pipelineLayout = layout;

	VkDescriptorUpdateTemplate updateTemplate = 0;
	VK_CHECK(vkCreateDescriptorUpdateTemplate(device, &createInfo, 0, &updateTemplate));

	return updateTemplate;
}

Program createProgram(VkDevice device, VkPipelineBindPoint bindPoint, Shaders shaders, size_t pushConstantSize, bool pushDescriptorsSupported)
{
	VkShaderStageFlags pushConstantStages = 0;
	for (const auto* shader : shaders) {
		if (shader->usesPushConstants) {
			pushConstantStages |= shader->stage;
		}
	}

	Program program {};

	program.bindPoint = bindPoint;

	program.setLayout = createSetLayout(device, shaders, pushDescriptorsSupported);
	assert(program.setLayout);
	program.layout = createPipelineLayout(device, program.setLayout, pushConstantStages, pushConstantSize);
	assert(program.layout);

	program.updateTemplate = createUpdateTemplate(device, bindPoint, program.layout, program.setLayout, shaders, pushDescriptorsSupported);
	assert(program.updateTemplate);

	program.pushConstantStages = pushConstantStages;

	return program;
}

void destroyProgram(VkDevice device, const Program& program)
{
	vkDestroyDescriptorUpdateTemplate(device, program.updateTemplate, 0);
	vkDestroyPipelineLayout(device, program.layout, 0);
	vkDestroyDescriptorSetLayout(device, program.setLayout, 0);
}
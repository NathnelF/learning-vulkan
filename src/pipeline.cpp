#include "headers.h"

VkShaderModule LoadShaders(State *state, const char *path)
{
  FILE *file = fopen(path, "rb");
  if (file == NULL)
  {
    err("failed to load file %s", path);
  }

  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(file_size);
  if (buffer == NULL)
  {
    err("failed to allocate for shader file");
  }

  size_t bytes_read = fread(buffer, 1, file_size, file);
  fclose(file);
  if (file_size != bytes_read)
  {
    err("expected %zu bytes, got %zu instead", file_size, bytes_read);
  }

  VkShaderModuleCreateInfo module_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = file_size,
    .pCode = (u32 *)buffer,
  };

  VkShaderModule module;
  vkCreateShaderModule(state->context->device, &module_info, NULL, &module);

  free(buffer);
  debug("loaded shader at %s", path);
  return module;
}

void CreatePipeline(State *state)
{
  VkShaderModule vertex_shader = LoadShaders(state, "src/vert.spv");
  VkShaderModule fragment_shader = LoadShaders(state, "src/frag.spv");

  // to create a pipeline
  // pipeline layout
  // this describes how the pipeline interacts with descriptors and push
  // constants right now we don't have either so it just is blank.
  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 0,
    .pSetLayouts = NULL,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = NULL,
  };

  VkPipelineLayout pipeline_layout;
  validate(
    vkCreatePipelineLayout(
      state->context->device, &pipeline_layout_info, NULL, &pipeline_layout),
    "could not create pipeline layout");

  debug("created pipeline layout");
  // shader stages
  VkPipelineShaderStageCreateInfo shader_stages[] = {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertex_shader,
      .pName = "main",
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragment_shader,
      .pName = "main",
    },
  };

  // vertex input
  VkVertexInputBindingDescription input_binding = {
    .binding = 0,
    .stride = sizeof(Vertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  VkVertexInputAttributeDescription input_attributes[] = {
    {
      .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = 0,
    },
  };

  VkPipelineVertexInputStateCreateInfo vertex_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &input_binding,
    .vertexAttributeDescriptionCount = 1,
    .pVertexAttributeDescriptions = input_attributes,
  };
  debug("pipeline vertex input assembly");
  // input assembly
  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  // view port and scissor
  VkPipelineViewportStateCreateInfo viewport_scissor_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = NULL,
    .scissorCount = 1,
    .pScissors = NULL,
  };
  //  rasterization
  VkPipelineRasterizationStateCreateInfo rasterization_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .lineWidth = 1.0f,
  };

  // multi sample
  VkPipelineMultisampleStateCreateInfo multisample_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
  };

  // color blend
  VkPipelineColorBlendAttachmentState color_blend_attachment = {
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendStateCreateInfo color_blend_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments = &color_blend_attachment,
  };
  //  dynamic state (viewport / scissor
  VkDynamicState dynamic_states[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = 2,
    .pDynamicStates = dynamic_states,
  };

  // rendering info
  VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
  VkPipelineRenderingCreateInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &format,
  };

  //  pipeline create info
  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &rendering_info,
    .stageCount = 2,
    .pStages = shader_stages,
    .pVertexInputState = &vertex_state_info,
    .pInputAssemblyState = &input_assembly_info,
    .pViewportState = &viewport_scissor_info,
    .pRasterizationState = &rasterization_info,
    .pMultisampleState = &multisample_info,
    .pColorBlendState = &color_blend_info,
    .pDynamicState = &dynamic_state_info,
    .layout = pipeline_layout,
    .renderPass = VK_NULL_HANDLE,
    .subpass = 0,

  };
  debug("pipeline configured");
  //  create pipeline
  validate(vkCreateGraphicsPipelines(state->context->device,
                                     VK_NULL_HANDLE,
                                     1,
                                     &pipeline_info,
                                     NULL,
                                     &state->context->pipeline),
           "could not create graphics pipelines");
  debug("created pipeline successfully!");
}

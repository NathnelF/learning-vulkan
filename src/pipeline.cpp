#include "headers.h"

VkShaderModule load_shader(State *state, const char *path)
{

  FILE *file = fopen(path, "rb");
  if (file == NULL)
  {
    err("failed to read file %s", path);
  }

  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(file_size);
  if (buffer == NULL)
  {
    err("malloc failed in file read");
  }

  size_t bytes_read = fread(buffer, 1, file_size, file);
  if (bytes_read != file_size)
  {
    err("expected %zu read, got %zu read", file_size, bytes_read);
  }
  fclose(file);

  VkShaderModuleCreateInfo shader_create_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = file_size,
    .pCode = (u32 *)buffer,
  };

  VkShaderModule shader_module;
  validate(vkCreateShaderModule(
             state->context->device, &shader_create_info, NULL, &shader_module),
           "could not create shader module");

  free(buffer);
  debug("loaded shader from %s", path);
  return shader_module;
}

void CreatePipeline(State *state)
{

  VkShaderModule vert_shader = load_shader(state, "src/vert.spv");
  VkShaderModule frag_shader = load_shader(state, "src/frag.spv");

  VkPipelineLayoutCreateInfo layout_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 0,
    .pSetLayouts = NULL,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = NULL,
  };

  VkPipelineLayout layout;
  validate(vkCreatePipelineLayout(
             state->context->device, &layout_create_info, NULL, &layout),
           "could not create pipeline layout");

  VkPipelineShaderStageCreateInfo shader_stages[2] = {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vert_shader,
      .pName = "main",
    },
    {

      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = frag_shader,
      .pName = "main",
    },
  };

  VkVertexInputBindingDescription binding = {
    .binding = 0,
    .stride = sizeof(Vertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  VkVertexInputAttributeDescription attribute = {
    .location = 0,
    .binding = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = 0,
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &binding,
    .vertexAttributeDescriptionCount = 1,
    .pVertexAttributeDescriptions = &attribute,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineViewportStateCreateInfo viewport_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = NULL,
    .scissorCount = 1,
    .pScissors = NULL,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = false,
    .rasterizerDiscardEnable = false,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasClamp = VK_FALSE,
    .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisample = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = false,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachmnet = {
    .blendEnable = false,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendStateCreateInfo color_blend = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = false,
    .attachmentCount = 1,
    .pAttachments = &color_blend_attachmnet,
  };

  VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
  VkPipelineRenderingCreateInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &format,
  };

  VkDynamicState dynamic_states[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = 2,
    .pDynamicStates = dynamic_states,
  };

  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &rendering_info,
    .stageCount = 2,
    .pStages = shader_stages,
    .pVertexInputState = &vertex_input_info,
    .pInputAssemblyState = &input_assembly_info,
    .pViewportState = &viewport_state_info,
    .pRasterizationState = &rasterizer,
    .pMultisampleState = &multisample,
    .pDepthStencilState = NULL,
    .pColorBlendState = &color_blend,
    .pDynamicState = &dynamic_state_info,
    .layout = layout,
    .renderPass = VK_NULL_HANDLE,
    .subpass = 0,
  };
  // assembly final pipeline
  VkResult pipeline_result =
    vkCreateGraphicsPipelines(state->context->device,
                              VK_NULL_HANDLE,
                              1,
                              &pipeline_info,
                              NULL,
                              &state->context->pipeline);

  vkDestroyShaderModule(state->context->device, vert_shader, NULL);
  vkDestroyShaderModule(state->context->device, frag_shader, NULL);
}

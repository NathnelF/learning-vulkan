#include "headers.h"

void RenderLoop(State *state, int frame_index)
{
  // first we get our frame context
  FrameContext *frame = &state->context->frame_context[frame_index];
  // wait on the fence
  validate(
    vkWaitForFences(state->context->device, 1, &frame->fence, true, UINT64_MAX),
    "could not wait for fence");

  // reset the fence
  validate(vkResetFences(state->context->device, 1, &frame->fence),
           "could not reset fence");
  // reset command pool
  validate(vkResetCommandPool(state->context->device,
                              frame->command_pool,
                              VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT),
           "could not reset command pool");
  // acquire next swapchain image
  u32 image_index = 0;
  VkResult swapchain_result =
    vkAcquireNextImageKHR(state->context->device,
                          state->swapchain->handle,
                          UINT64_MAX,
                          frame->begin_rendering_semaphore,
                          VK_NULL_HANDLE,
                          &image_index);
  if (swapchain_result == VK_ERROR_OUT_OF_DATE_KHR ||
      swapchain_result == VK_SUBOPTIMAL_KHR)
  {
    printf("recreating swapchain\n");
    RecreateVulkanSwapchain(state);
    return;
  }

  validate(swapchain_result, "could not acquire next swapchain image");
  //
  // begin command buffer
  VkCommandBufferBeginInfo buffer_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VkCommandBuffer buffer = frame->command_buffer;
  validate(vkBeginCommandBuffer(buffer, &buffer_info),
           "could not begin command buffer");

  // transition to color attachment layout
  VkImageMemoryBarrier2 color_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .image = state->swapchain->images[image_index],
    .subresourceRange = {
       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
       .levelCount = 1,
       .layerCount = 1,
    },
  };

  VkDependencyInfo color_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &color_barrier,
  };

  vkCmdPipelineBarrier2(buffer, &color_info);
  // begin rendering
  // dynamic rendering lets us specify attachments at runtime
  // our attachments are color and depth (null for 2D)
  VkRenderingAttachmentInfo color_attachment_info = {
     .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
     .imageView = state->swapchain->views[image_index],
     .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
     .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
     .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
     .clearValue =
     {
        .color = {0.15f, 0.35f, 0.0f, 1.0f},
     },
  };

  VkRenderingAttachmentInfo depth_stencil_attachment_info = {
     .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
     .imageView = state->swapchain->depth_view,
     .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
     .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
     .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
     .clearValue = {
        .depthStencil = { 1.0f, 0 },
     },
  };
  VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = {
       .extent = {
          .width = state->swapchain->width,
          .height = state->swapchain->height,
       },
    },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment_info,
    .pDepthAttachment = &depth_stencil_attachment_info,
  };

  vkCmdBeginRendering(buffer, &rendering_info);

  // bind pipeline
  vkCmdBindPipeline(
    buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->context->pipeline);

  VkViewport viewport = {
    .x = 0.0f,
    .y = 0.0f,
    .width = (float)state->swapchain->width,
    .height = (float)state->swapchain->height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };
  vkCmdSetViewport(buffer, 0, 1, &viewport);

  VkRect2D scissor = {
     .offset = {0,0},
     .extent = {
         .width = state->swapchain->width,
         .height = state->swapchain->height,
     },
  };
  vkCmdSetScissor(buffer, 0, 1, &scissor);
  // bind buffers
  // TODO(Nate): need to actually load 3D data to render to now.
  MeshRegion *cube_mesh = &state->mega_buffer.regions[0];

  VkDeviceSize vertex_offset = state->mega_buffer.vertex_region_offset +
                               cube_mesh->vertex_offset * sizeof(Vertex);

  vkCmdBindVertexBuffers(
    buffer, 0, 1, &state->mega_buffer.buffer, &vertex_offset);

  VkDeviceSize index_offset = state->mega_buffer.index_region_offset +
                              cube_mesh->index_offset * sizeof(u32);

  vkCmdBindIndexBuffer(
    buffer, state->mega_buffer.buffer, index_offset, VK_INDEX_TYPE_UINT32);

  // push constants for camera
  HMM_Mat4 view =
    HMM_LookAt_RH(HMM_V3(5, 5, -8), HMM_V3(0, 0, 0), HMM_V3(0, 1, 0));
  HMM_Mat4 projection = HMM_Perspective_RH_ZO(HMM_AngleDeg(60.0f),
                                              (float)state->swapchain->width /
                                                (float)state->swapchain->height,
                                              0.1f,
                                              100.0f);
  float time = SDL_GetTicks() / 1000.0f;
  float angle = time * 1.6f;
  HMM_Mat4 rotate = HMM_Rotate_RH(HMM_AngleRad(angle), HMM_V3(0, 1, 0));
  HMM_Mat4 model = HMM_MulM4(HMM_Translate(HMM_V3(0, 0, 0)), rotate);
  HMM_Mat4 mvp = HMM_MulM4(HMM_MulM4(projection, view), model);
  vkCmdPushConstants(buffer,
                     state->context->pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT,
                     0,
                     sizeof(HMM_Mat4),
                     &mvp);
  vkCmdDrawIndexed(buffer, cube_mesh->index_count, 1, 0, 0, 0);
  vkCmdEndRendering(buffer);
  // end rendering
  //
  VkImageMemoryBarrier2 present_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .image = state->swapchain->images[image_index],
    .subresourceRange = {
       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
       .levelCount = 1,
       .layerCount = 1,
    },
  };
  VkDependencyInfo present_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &present_barrier,
  };
  vkCmdPipelineBarrier2(buffer, &present_info);

  // end command buffer
  vkEndCommandBuffer(buffer);
  // submit to queue
  VkCommandBufferSubmitInfo cmd_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = buffer,
  };

  // wait info
  VkSemaphoreSubmitInfo wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = frame->begin_rendering_semaphore,
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
  };

  // signal info
  VkSemaphoreSubmitInfo signal_info{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = state->swapchain->begin_presenting_semaphore[image_index],
    .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
  };

  VkSubmitInfo2 submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .waitSemaphoreInfoCount = 1,
    .pWaitSemaphoreInfos = &wait_info,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &signal_info,
  };

  validate(vkQueueSubmit2(state->context->queue, 1, &submit_info, frame->fence),
           "could not submit to queue");

  // present queue
  VkPresentInfoKHR queue_present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores =
      &state->swapchain->begin_presenting_semaphore[image_index],
    .swapchainCount = 1,
    .pSwapchains = &state->swapchain->handle,
    .pImageIndices = &image_index,
  };
  VkResult present_result =
    vkQueuePresentKHR(state->context->queue, &queue_present_info);

  if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
      present_result == VK_SUBOPTIMAL_KHR)
  {
    printf("recreating swapchain\n");
    RecreateVulkanSwapchain(state);
    return;
  }

  validate(present_result, "could not present image to the swapchain");
}

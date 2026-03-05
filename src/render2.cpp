#include "headers.h"

void RenderLoop2(State *state, int frame_index)
{
  // We get the context for the current frame
  FrameContext *frame = &state->context->frame_context[frame_index];
  // make sure we aren't using the current frame
  validate(
    vkWaitForFences(state->context->device, 1, &frame->fence, true, UINT64_MAX),
    "could not wait on the fence");

  validate(vkResetFences(state->context->device, 1, &frame->fence),
           "could not reset fences");

  // now we know that this frame is not being rendered by the gpu

  // reset the command pool
  validate(vkResetCommandPool(state->context->device,
                              frame->command_pool,
                              VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT),
           "could not reset command pool");

  // acquire the next swapchain image
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
    // return early
    return;
  }

  // handle other more typical errors
  validate(swapchain_result, "could not get next swapchain image");

  // begin command buffer
  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VkCommandBuffer buffer = frame->command_buffer;

  validate(vkBeginCommandBuffer(buffer, &begin_info),
           "could not begin command buffer");
  // transition image layout to color attachment bit
  VkImageMemoryBarrier2 color_barrier = {
     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
     .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     .srcAccessMask = 0,
     .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
     .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
     .image = state->swapchain->images[image_index],
     .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
     },
  };

  VkDependencyInfo color_barrier_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &color_barrier,
  };

  vkCmdPipelineBarrier2(buffer, &color_barrier_info);

  // dynamic rendering info
  // need to provide our rendering attachments
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
     .pDepthAttachment = NULL,
  };

  // begin rendering
  vkCmdBeginRendering(buffer, &rendering_info);

  // bind pipeline
  vkCmdBindPipeline(
    buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->context->pipeline);
  // set viewport and scissor

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
     .offset = {0, 0},
     .extent = {
        .width = state->swapchain->width,
        .height = state->swapchain->height,
     },
  };
  vkCmdSetScissor(buffer, 0, 1, &scissor);

  // draw command
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(
    buffer, 0, 1, &state->context->vertex_buffer.buffer, &offset);
  vkCmdDraw(buffer, 3, 1, 0, 0);
  //  end rendering
  vkCmdEndRendering(buffer);
  // transition image layout to presnet optimal
  VkImageMemoryBarrier2 present_barrier = {
     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
     .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR, //?
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

  VkDependencyInfo present_barrier_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &present_barrier,
  };

  vkCmdPipelineBarrier2(buffer, &present_barrier_info);

  // end command buffer
  vkEndCommandBuffer(buffer);

  // submit to queue
  VkCommandBufferSubmitInfo cmd_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = buffer,
  };

  VkSemaphoreSubmitInfo wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = frame->begin_rendering_semaphore,
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
  };

  VkSemaphoreSubmitInfo signal_info = {
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
           "could not submit queue");
  // present queue
  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores =
      &state->swapchain->begin_presenting_semaphore[image_index],
    .swapchainCount = 1,
    .pSwapchains = &state->swapchain->handle,
    .pImageIndices = &image_index,
  };

  VkResult present_result =
    vkQueuePresentKHR(state->context->queue, &present_info);

  if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
      present_result == VK_SUBOPTIMAL_KHR)
  {
    RecreateVulkanSwapchain(state);
    return;
  }
  validate(present_result, "could not present image to swapchain");
}

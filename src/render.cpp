#include "headers.h"
#include <vulkan/vulkan_core.h>

void RenderLoop(State *state, int frame_index)
{
    FrameContext *frame = &state->context->frame_context[frame_index];
    validate(vkWaitForFences(state->context->device, 1, &frame->fence, true,
                             UINT64_MAX),
             "could not wait for frame fance");

    validate(vkResetFences(state->context->device, 1, &frame->fence),
             "could not reset fence");

    // reset command pool
    vkResetCommandPool(state->context->device, frame->command_pool, 0);

    // acquire next image
    u32 image_index = 0;
    VkResult swapchain_result = vkAcquireNextImageKHR(
        state->context->device, state->swapchain->handle, UINT64_MAX,
        frame->begin_rendering_semaphore, VK_NULL_HANDLE, &image_index);

    if (swapchain_result == VK_ERROR_OUT_OF_DATE_KHR ||
        swapchain_result == VK_SUBOPTIMAL_KHR)
    {
        printf("recreating\n\n");
        RecreateVulkanSwapchain(state);
        return;
    }

    validate(swapchain_result, "could not get swapchain image");

    // begin command buffer
    VkCommandBuffer buffer = frame->command_buffer;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    validate(vkBeginCommandBuffer(buffer, &begin_info),
             "could not begin command buffer");

    // image layout transition to color attachment
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
        .subresourceRange =
            {
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

    // begin dynamic rendering
    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = state->swapchain->views[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =
            {
                .color = {0.0f, 0.25f, 0.35f, 1.0f},
            },
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            {
                .extent =
                    {
                        .width = state->swapchain->width,
                        .height = state->swapchain->height,
                    },
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .pDepthAttachment = NULL,
    };

    vkCmdBeginRendering(buffer, &rendering_info);

    // TODO(Nate): render more stuff here

    vkCmdEndRendering(buffer);

    // image layout transition to present
    VkImageMemoryBarrier2 present_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = state->swapchain->images[image_index],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },

    };

    VkDependencyInfo present_dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &present_barrier,
    };

    vkCmdPipelineBarrier2(buffer, &present_dep_info);

    vkEndCommandBuffer(buffer);

    // end command buffer

    // submit commands
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

    validate(
        vkQueueSubmit2(state->context->queue, 1, &submit_info, frame->fence),
        "could not submit queue");

    // present
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
    validate(present_result, "could not present");
}

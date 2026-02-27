#include "headers.h"

void RenderLoop2(State* state, int frame_index)
{
  // We get the context for the current frame
  FrameContext* frame = &state->context->frame_context[frame_index];
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
  // begin command buffer
  // transition image layout to color attachment bit
  // dynamic rendering info
  //
  // begin rendering
  // end rendering
  // transition image layout to presnet optimal
  // end command buffer
  // submit to queue
  // present queue
}

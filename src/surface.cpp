#include "headers.h"

void CreateSwapchain(State *state, VkSwapchainKHR handle)
{
    // query the capabilities
    VkSurfaceCapabilitiesKHR surface_caps;
    validate(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->context->gpu, state->context->surface.handle, &surface_caps),
        "could not get surface capabilities");

    int width, height;
    SDL_GetWindowSize(state->context->surface.window, &width, &height);
    state->swapchain->width = (u32)width;
    state->swapchain->height = (u32)height;
    // swapchain create info
    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = state->context->surface.handle,
        .minImageCount = surface_caps.minImageCount + 1,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent =
            {
                .width = state->swapchain->width,
                .height = state->swapchain->height,
            },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .oldSwapchain = handle,
    };

    // create swapchain
    validate(vkCreateSwapchainKHR(state->context->device, &swapchain_info, NULL, &state->swapchain->handle),
             "could not craete swapchain");

    // get images
    validate(
        vkGetSwapchainImagesKHR(state->context->device, state->swapchain->handle, &state->swapchain->image_count, NULL),
        "could not get swapchain image count");

    state->swapchain->images =
        (VkImage *)ArenaPush(&state->swapchain_arena, sizeof(VkImage) * state->swapchain->image_count);

    validate(vkGetSwapchainImagesKHR(state->context->device, state->swapchain->handle, &state->swapchain->image_count,
                                     state->swapchain->images),
             "could not get swapchain images");

    // create views
    state->swapchain->views =
        (VkImageView *)ArenaPush(&state->swapchain_arena, sizeof(VkImageView) * state->swapchain->image_count);

    for (u32 i = 0; i < state->swapchain->image_count; i++)
    {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = state->swapchain->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
        };

        validate(vkCreateImageView(state->context->device, &view_info, NULL, &state->swapchain->views[i]),
                 "could not create image view");
    }

    debug("Created swapchain");

    // create semaphore images
    state->swapchain->begin_presenting_semaphore =
        (VkSemaphore *)ArenaPush(&state->swapchain_arena, sizeof(VkSemaphore) * state->swapchain->image_count);

    for (u32 i = 0; i < state->swapchain->image_count; i++)
    {
        VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        validate(vkCreateSemaphore(state->context->device, &semaphore_info, NULL,
                                   &state->swapchain->begin_presenting_semaphore[i]),
                 "could not create vk semapohre");
    }

    debug("Created render semaphores");
}

void CreateDepthImages(State *state)
{
    // already have the supported format
    // vma alloc
    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    // image create info
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = state->context->surface.depth_format,
        .extent =
            {
                .width = state->swapchain->width,
                .height = state->swapchain->height,
                .depth = 1,

            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,

    };

    // vma create image
    validate(vmaCreateImage(state->context->allocator, &image_info, &alloc_info, &state->swapchain->depth_image,
                            &state->swapchain->depth_alloc, NULL),
             "could not create depth image");

    // create image view
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = state->swapchain->depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = state->context->surface.depth_format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };

    validate(vkCreateImageView(state->context->device, &view_info, NULL, &state->swapchain->depth_view),
             "could not create depth image view");

    debug("Created depth image and views");
}

void CreateVulkanSwapchain(State *state, VkSwapchainKHR handle)
{
    // create swapchain
    CreateSwapchain(state, handle);

    // create depth images too
    CreateDepthImages(state);
}

void RecreateVulkanSwapchain(State *state)
{
    // TODO(Nate): handle minimization at some point
    // vkDeviceWaitIdle(state->context->device);
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        vkWaitForFences(state->context->device, 1, &state->context->frame_context[i].fence, VK_TRUE, UINT64_MAX);
    }
    Swapchain old_swapchain = *state->swapchain;
    VkImageView old_views[10];
    VkSemaphore old_sems[10];
    for (u32 i = 0; i < old_swapchain.image_count; i++)
    {
        old_views[i] = old_swapchain.views[i];
        old_sems[i] = old_swapchain.begin_presenting_semaphore[i];
    }
    //  reset arena
    ArenaReset(&state->swapchain_arena);
    state->swapchain = (Swapchain *)ArenaPush(&state->swapchain_arena, sizeof(Swapchain));
    // create new swapchain
    CreateVulkanSwapchain(state, old_swapchain.handle);
    //  delete shit
    vkDestroyImageView(state->context->device, old_swapchain.depth_view, NULL);
    vmaDestroyImage(state->context->allocator, old_swapchain.depth_image, old_swapchain.depth_alloc);

    for (u32 i = 0; i < old_swapchain.image_count; i++)
    {
        vkDestroySemaphore(state->context->device, old_sems[i], NULL);
        vkDestroyImageView(state->context->device, old_views[i], NULL);
    }
    vkDestroySwapchainKHR(state->context->device, old_swapchain.handle, NULL);
    //  create chain
    debug("recreated vulkan swapchain");
}

#pragma once

#define VK_NO_PROTOTYPES
#include "arena.h"
#include "types.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#include <volk.h>
//
#include "vk_mem_alloc.h"
//

#define FRAMES_IN_FLIGHT 3

struct Surface
{
    SDL_Window *window;
    VkSurfaceKHR handle;
    VkFormat depth_format;
};

struct FrameContext
{
    VkSemaphore begin_rendering_semaphore;
    VkFence fence;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
};

struct Context
{
    VkInstance instance;
    VkPhysicalDevice gpu;
    VkQueue queue;
    u32 queue_index;
    VkDevice device;
    VmaAllocator allocator;
    FrameContext frame_context[FRAMES_IN_FLIGHT];
    Surface surface;
};

struct Swapchain
{
    VkSwapchainKHR handle;
    VkImage *images;
    VkImageView *views;
    VkSemaphore *begin_presenting_semaphore; // tied to image count
    VkImage depth_image;
    VkImageView depth_view;
    VmaAllocation depth_alloc;
    u32 image_count;
    u32 width;
    u32 height;
};

struct State
{
    Context *context;
    Swapchain *swapchain;

    int resize_ticker;

    Arena permanent_arena;
    Arena swapchain_arena;
    Arena scratch_arena;
};

// macros
inline const char *vk_debug_string(VkResult code)
{
    switch (code)
    {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
        return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    default:
        return "VK_UNKNOWN";
    }
}

// DEFINED RECREATE SWAPCHAIN HERE
void RecreateVulkanSwapchain(State *state);

#define validate(error, format, ...)                                           \
    {                                                                          \
        if (error != VK_SUCCESS)                                               \
        {                                                                      \
            fprintf(stderr, "%s -> %s -> %i [ERROR]: %s (%i)\n\t" format "\n", \
                    __FILE_NAME__, __FUNCTION__, __LINE__,                     \
                    vk_debug_string(error), error, ##__VA_ARGS__);             \
            exit(-1);                                                          \
        }                                                                      \
    }

#define err(format, ...)                                                       \
    {                                                                          \
        fprintf(stderr, "%s -> %s -> %i [ERROR]: " format "\n", __FILE_NAME__, \
                __FUNCTION__, __LINE__, ##__VA_ARGS__);                        \
        exit(-1);                                                              \
    }

extern int g_debug_enabled;

#define debug(format, ...)                                                     \
    {                                                                          \
        if (g_debug_enabled == 1)                                              \
        {                                                                      \
            fprintf(stdout, "[DEBUG]: " format "\n", ##__VA_ARGS__);           \
        }                                                                      \
    }

#define megabytes(n) ((u64)(n) * 1024 * 1024)

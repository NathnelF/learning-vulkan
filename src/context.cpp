#include "headers.h"

void CreateInstance(State *state)
{
    // volk initialize
    validate(volkInitialize(), "could not initialize volk");
    // SDL init
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        err("could not initialize sdl");
    }

    // instance level extensions (glfw)
    u32 count;
    const char *const *sdl_extensions =
        SDL_Vulkan_GetInstanceExtensions(&count);

    if (sdl_extensions == NULL)
    {
        err("could not get glfw extensions");
    }

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_3,
    };

    // instance create info
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = 0,
        .enabledExtensionCount = count,
        .ppEnabledExtensionNames = sdl_extensions,
    };

    // create instance
    validate(vkCreateInstance(&instance_info, NULL, &state->context->instance),
             "could not create instance");

    // volk load instance
    volkLoadInstance(state->context->instance);

    debug("Created and loaded vulkan instance");
}

void GetPhysicalDevice(State *state)
{
    // enumerate physical deviecs
    u32 count;
    validate(vkEnumeratePhysicalDevices(state->context->instance, &count, NULL),
             "could not enumerate physical device count");

    // allocate to scratch space
    VkPhysicalDevice *devices = (VkPhysicalDevice *)ArenaPush(
        &state->scratch_arena, count * sizeof(VkPhysicalDevice));

    validate(
        vkEnumeratePhysicalDevices(state->context->instance, &count, devices),
        "could not enumerate physical devices");

    // check for discrete physical device
    for (u32 i = 0; i < count; i++)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            state->context->gpu = devices[i];
            debug("Chose device: %s", properties.deviceName);
            return;
        }
    }
    // check for integrated as second choice
    for (u32 i = 0; i < count; i++)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            state->context->gpu = devices[i];
            debug("Chose device: %s", properties.deviceName);
            return;
        }
    }
}

void GetDepthFormat(State *state)
{
    VkFormat valid_formats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    VkFormat current_format = VK_FORMAT_UNDEFINED;

    for (u32 i = 0; i < 1; i++)
    {
        VkFormatProperties2 properties = {
            .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        };

        vkGetPhysicalDeviceFormatProperties2(state->context->gpu,
                                             valid_formats[i], &properties);

        if (properties.formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            current_format = valid_formats[i];
            state->context->surface.depth_format = valid_formats[i];
            break;
        }
    }

    if (current_format == VK_FORMAT_UNDEFINED)
    {
        err("failed to find appropriate depth format");
    }

    debug("Retrieved valid depth format");
}

void GetQueueIndex(State *state)
{
    // enumerate queues
    u32 count;
    vkGetPhysicalDeviceQueueFamilyProperties(state->context->gpu, &count, NULL);

    VkQueueFamilyProperties *families = (VkQueueFamilyProperties *)ArenaPush(
        &state->scratch_arena, sizeof(VkQueueFamilyProperties) * count);

    vkGetPhysicalDeviceQueueFamilyProperties(state->context->gpu, &count,
                                             families);

    // grapb the graphics queue
    for (u32 i = 0; i < count; i++)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            state->context->queue_index = i;
            debug("Retrieved valid graphics queue index");
            return;
        }
    }
    err("Failed to find graphics queue index");
}

void CreateLogicalDevice(State *state)
{
    // create the actual queue
    float priorities = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = state->context->queue_index,
        .queueCount = 1,
        .pQueuePriorities = &priorities,
    };

    // get device level extensions and features
    // core features
    VkPhysicalDeviceFeatures core_features = {
        .samplerAnisotropy = true,
    };

    VkPhysicalDeviceVulkan12Features vk_12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .bufferDeviceAddress = true,
    };

    VkPhysicalDeviceVulkan13Features vk_13_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vk_12_features,
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vk_13_features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = &core_features,
    };

    // create device
    validate(vkCreateDevice(state->context->gpu, &device_info, NULL,
                            &state->context->device),
             "Could not create logical device");

    // volk load device
    volkLoadDevice(state->context->device);

    vkGetDeviceQueue(state->context->device, state->context->queue_index, 0,
                     &state->context->queue);
    debug("Created logical device")
}

void InitVma(State *state)
{
    VmaVulkanFunctions vulkan_functions = {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    VmaAllocatorCreateInfo allocator_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = state->context->gpu,
        .device = state->context->device,
        .pVulkanFunctions = &vulkan_functions,
        .instance = state->context->instance,
    };

    validate(vmaCreateAllocator(&allocator_info, &state->context->allocator),
             "could not create vma allocator");

    debug("Created vma allocator");
}

void CreateWindow(State *state)
{
    state->context->surface.window = SDL_CreateWindow(
        "Barbarian", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (state->context->surface.window == NULL)
    {
        err("Could not create sdl window");
    }

    // create vulkan surface

    if (!SDL_Vulkan_CreateSurface(state->context->surface.window,
                                  state->context->instance, NULL,
                                  &state->context->surface.handle))
    {
        err("failed to create sdl vulkan surface");
    }
}

void InitFrameContext(State *state)
{
    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = state->context->queue_index,
    };

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        validate(
            vkCreateSemaphore(
                state->context->device, &sem_info, NULL,
                &state->context->frame_context[i].begin_rendering_semaphore),
            "could not create presentation semapohre");

        validate(vkCreateFence(state->context->device, &fence_info, NULL,
                               &state->context->frame_context[i].fence),
                 "could not create frame fence");

        validate(
            vkCreateCommandPool(state->context->device, &pool_info, NULL,
                                &state->context->frame_context[i].command_pool),
            "could not create command pool");

        VkCommandBufferAllocateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = state->context->frame_context[i].command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        validate(vkAllocateCommandBuffers(
                     state->context->device, &buffer_info,
                     &state->context->frame_context[i].command_buffer),
                 "could not allocate command buffer");
    }

    debug("created frame context");
}

void CreateVulkanContext(State *state)
{
    // create instance
    CreateInstance(state);
    // get physical device
    GetPhysicalDevice(state);
    // get queue index
    GetQueueIndex(state);
    // get depth format
    GetDepthFormat(state);
    // create logical device
    CreateLogicalDevice(state);
    // init vma
    InitVma(state);
    // create glfw window
    CreateWindow(state);
    // init frame synchronization
    InitFrameContext(state);
    // reset scratch
    ArenaReset(&state->scratch_arena);
    // notify
    debug("Created vulkan context successfully");
}

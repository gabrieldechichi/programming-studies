#include <vulkan/vulkan.h>
#include "gpu_backend.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lib/profiler.h"

#define MAX_FRAMES_IN_FLIGHT 2

// YUV constants (matching video_renderer.c)
#define FRAME_WIDTH 1080
#define FRAME_HEIGHT 1920
#define YUV_Y_SIZE_BYTES (FRAME_WIDTH * FRAME_HEIGHT)
#define YUV_UV_SIZE_BYTES (FRAME_WIDTH * FRAME_HEIGHT / 4)
#define YUV_TOTAL_SIZE_BYTES (YUV_Y_SIZE_BYTES + 2 * YUV_UV_SIZE_BYTES)

// Helper macro for checking Vulkan results
#define VK_CHECK(call) \
    do { \
        VkResult result = call; \
        if (result != VK_SUCCESS) { \
            fprintf(stderr, "Vulkan error in %s at line %d: %d\n", __FILE__, __LINE__, result); \
            exit(1); \
        } \
    } while(0)

// Vulkan memory allocation callbacks
static void* VKAPI_PTR vulkan_alloc_func(void* pUserData, size_t size, size_t alignment,
                                         VkSystemAllocationScope allocationScope) {
    Allocator* allocator = (Allocator*)pUserData;
    return allocator->alloc_alloc(allocator->ctx, size, alignment);
}

static void* VKAPI_PTR vulkan_realloc_func(void* pUserData, void* pOriginal, size_t size,
                                           size_t alignment, VkSystemAllocationScope allocationScope) {
    Allocator* allocator = (Allocator*)pUserData;
    // Note: realloc doesn't support alignment in the current interface,
    // but that's OK as the arena will maintain alignment from the original allocation
    return allocator->alloc_realloc(allocator->ctx, pOriginal, size);
}

static void VKAPI_PTR vulkan_free_func(void* pUserData, void* pMemory) {
    if (!pMemory) return;
    Allocator* allocator = (Allocator*)pUserData;
    allocator->alloc_free(allocator->ctx, pMemory);
}

// Internal structures
struct gpu_device {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue transfer_queue;
    VkCommandPool command_pool;
    VkCommandPool transfer_command_pool;  // Separate pool for transfer operations
    uint32_t graphics_queue_family;
    uint32_t transfer_queue_family;

    // For shader compilation
    VkShaderModule vertex_shader;
    VkShaderModule fragment_shader;

    // Memory allocators
    Allocator* permanent_allocator;
    Allocator* temporary_allocator;

    // Vulkan allocation callbacks
    VkAllocationCallbacks vk_alloc_callbacks;

    // Fence tracking for cleanup
    VkFence* tracked_fences;
    uint32_t fence_count;
    uint32_t fence_capacity;
};

struct gpu_texture {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory memory;
    VkFormat format;
    int width;
    int height;
    gpu_device_t* device;  // For proper cleanup
    // Depth buffer for render targets
    VkImage depth_image;
    VkImageView depth_image_view;
    VkDeviceMemory depth_memory;
    VkFormat depth_format;
};

struct gpu_readback_buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    size_t size;
    void* mapped_data;
    gpu_device_t* device;  // Need device reference for invalidation
    bool is_coherent;      // Track if memory is coherent (no invalidation needed)
};

struct gpu_command_buffer {
    VkCommandBuffer cmd_buffer;
    VkFence fence;
    gpu_device_t* device;
    bool completed;
};

struct gpu_descriptor_set {
    VkDescriptorSet descriptor_set;
    gpu_pipeline_t* pipeline;  // Back reference to pipeline

    // Per-descriptor-set uniform and storage buffers
    VkBuffer* uniform_buffers;
    VkDeviceMemory* uniform_memories;
    void** uniform_mapped;

    VkBuffer* storage_buffers;
    VkDeviceMemory* storage_memories;
    void** storage_mapped;
};

struct gpu_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkRenderPass render_pass;
    VkFramebuffer* framebuffers;  // One per texture target
    gpu_device_t* device;  // For proper cleanup

    // Uniform buffer support
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;  // Default descriptor set for backward compat
    uint32_t max_descriptor_sets;    // Maximum number of descriptor sets in pool
    VkBuffer uniform_buffer;  // Backward compatibility - points to uniform_buffers[0]
    VkDeviceMemory uniform_buffer_memory;  // Points to uniform_memories[0]
    void* uniform_buffer_mapped;  // Points to uniform_mapped[0]
    bool has_uniforms;

    // Multiple uniform buffers for toon shader
    VkBuffer* uniform_buffers;      // Array of uniform buffers (one per binding)
    VkDeviceMemory* uniform_memories;  // Memory for each uniform buffer
    void** uniform_mapped;           // Mapped pointers for each uniform buffer

    // Storage buffer for blendshapes
    VkBuffer storage_buffer;
    VkDeviceMemory storage_buffer_memory;
    void* storage_buffer_mapped;

    // New flexible pipeline fields
    uint32_t num_uniform_buffers;
    uint32_t num_storage_buffers;
    size_t* uniform_sizes;           // Size of each uniform buffer
    size_t* storage_sizes;           // Size of each storage buffer
    gpu_uniform_buffer_desc_t* uniform_buffer_descs; // Descriptors for lookup
    gpu_storage_buffer_desc_t* storage_buffer_descs; // Descriptors for lookup

    // Multiple storage buffers
    VkBuffer* storage_buffers;
    VkDeviceMemory* storage_memories;
    void** storage_mapped;

    // Texture/sampler support
    uint32_t num_texture_bindings;
    gpu_texture_desc_t* texture_descs;  // Store texture descriptors for lookup
    VkSampler default_sampler;          // Default sampler for all textures

    // Pre-allocated buffer pools for descriptor sets
    VkBuffer* uniform_buffer_pool;      // Pool of uniform buffers [max_sets * num_uniform_buffers]
    VkDeviceMemory* uniform_memory_pool;  // Memory for uniform buffers
    void** uniform_mapped_pool;          // Mapped pointers for uniform buffers

    VkBuffer* storage_buffer_pool;      // Pool of storage buffers [max_sets * num_storage_buffers]
    VkDeviceMemory* storage_memory_pool;  // Memory for storage buffers
    void** storage_mapped_pool;          // Mapped pointers for storage buffers

    uint32_t next_buffer_index;         // Next available buffer index in pools
};

struct gpu_buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    size_t size;
    gpu_device_t* device;  // For proper cleanup
};

struct gpu_render_encoder {
    VkCommandBuffer cmd_buffer;
    gpu_texture_t* target;
    VkFramebuffer framebuffer;
    VkRenderPass render_pass;
    gpu_device_t* device;      // Need device reference
};

struct gpu_compute_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkShaderModule compute_shader;
    gpu_device_t* device;  // For proper cleanup
};

// Helper function to find memory type (returns UINT32_MAX if not found)
static uint32_t find_memory_type_optional(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}

// Helper function to find memory type (exits on failure)
static uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    uint32_t memory_type = find_memory_type_optional(physical_device, type_filter, properties);
    if (memory_type == UINT32_MAX) {
        fprintf(stderr, "Failed to find suitable memory type\n");
        exit(1);
    }
    return memory_type;
}

// Load SPIR-V shader from file
static VkShaderModule load_shader_module(VkDevice device, const char* filename, Allocator* temp_allocator, VkAllocationCallbacks* alloc_callbacks) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open shader file: %s\n", filename);
        return VK_NULL_HANDLE;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint32_t* code = ALLOC_ARRAY(temp_allocator, uint32_t, file_size / sizeof(uint32_t));
    if (!code) {
        fprintf(stderr, "Failed to allocate memory for shader file: %s\n", filename);
        fclose(file);
        return VK_NULL_HANDLE;
    }

    fread(code, 1, file_size, file);
    fclose(file);

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = file_size,
        .pCode = code
    };

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(device, &create_info, alloc_callbacks, &shader_module));

    // No need to free - temporary allocator will handle cleanup
    return shader_module;
}

// Initialize Vulkan instance
static VkInstance create_instance(Allocator* temp_allocator, VkAllocationCallbacks* alloc_callbacks) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Video Renderer",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2
    };

    const char* validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties* available_layers = ALLOC_ARRAY(temp_allocator, VkLayerProperties, layer_count);
    if (!available_layers) {
        fprintf(stderr, "Failed to allocate memory for layer properties\n");
        exit(1);
    }
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    bool validation_found = false;
    for (uint32_t i = 0; i < layer_count; i++) {
        if (strcmp(available_layers[i].layerName, validation_layers[0]) == 0) {
            validation_found = true;
            break;
        }
    }
    // No need to free - temporary allocator will handle cleanup

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = validation_found ? 1 : 0,
        .ppEnabledLayerNames = validation_found ? validation_layers : NULL,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL
    };

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&create_info, alloc_callbacks, &instance));

    if (validation_found) {
        printf("[Vulkan] Validation layers enabled\n");
    }

    return instance;
}

// Select physical device
static VkPhysicalDevice select_physical_device(VkInstance instance, Allocator* temp_allocator) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);

    if (device_count == 0) {
        fprintf(stderr, "Failed to find GPUs with Vulkan support\n");
        exit(1);
    }

    VkPhysicalDevice* devices = ALLOC_ARRAY(temp_allocator, VkPhysicalDevice, device_count);
    if (!devices) {
        fprintf(stderr, "Failed to allocate memory for device enumeration\n");
        exit(1);
    }
    vkEnumeratePhysicalDevices(instance, &device_count, devices);

    printf("[Vulkan] Found %u GPU(s):\n", device_count);

    VkPhysicalDevice selected_device = VK_NULL_HANDLE;
    int best_score = -1;

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        vkGetPhysicalDeviceMemoryProperties(devices[i], &mem_props);

        const char* device_type_str;
        int score = 0;

        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                device_type_str = "Discrete GPU";
                score = 1000;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                device_type_str = "Integrated GPU";
                score = 100;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                device_type_str = "Virtual GPU";
                score = 50;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                device_type_str = "CPU";
                score = 10;
                break;
            default:
                device_type_str = "Other";
                score = 1;
                break;
        }

        // Calculate total VRAM
        VkDeviceSize total_memory = 0;
        for (uint32_t j = 0; j < mem_props.memoryHeapCount; j++) {
            if (mem_props.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                total_memory += mem_props.memoryHeaps[j].size;
            }
        }

        printf("[Vulkan]   %u: %s (%s) - VRAM: %.1f GB\n",
               i, props.deviceName, device_type_str,
               (double)total_memory / (1024.0 * 1024.0 * 1024.0));

        if (score > best_score) {
            best_score = score;
            selected_device = devices[i];
        }
    }

    VkPhysicalDeviceProperties selected_props;
    vkGetPhysicalDeviceProperties(selected_device, &selected_props);
    printf("[Vulkan] Selected: %s\n", selected_props.deviceName);

    // No need to free - temporary allocator will handle cleanup
    return selected_device;
}

// Find queue families
static void find_queue_families(VkPhysicalDevice physical_device, uint32_t* graphics_family, uint32_t* transfer_family, Allocator* temp_allocator) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

    VkQueueFamilyProperties* queue_families = ALLOC_ARRAY(temp_allocator, VkQueueFamilyProperties, queue_family_count);
    if (!queue_families) {
        fprintf(stderr, "Failed to allocate memory for queue family properties\n");
        exit(1);
    }
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

    *graphics_family = UINT32_MAX;
    *transfer_family = UINT32_MAX;

    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *graphics_family = i;
        }
        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            *transfer_family = i;
        }
    }

    // No need to free - temporary allocator will handle cleanup

    if (*graphics_family == UINT32_MAX) {
        fprintf(stderr, "Failed to find graphics queue family\n");
        exit(1);
    }
}

gpu_device_t* gpu_init(Allocator* permanent_allocator, Allocator* temporary_allocator) {
    gpu_device_t* device = ALLOC(permanent_allocator, gpu_device_t);
    device->permanent_allocator = permanent_allocator;
    //todo: use temporary_allocator correctly
    device->temporary_allocator = temporary_allocator;
    device->temporary_allocator = permanent_allocator;

    // Initialize Vulkan allocation callbacks
    device->vk_alloc_callbacks = (VkAllocationCallbacks){
        .pUserData = permanent_allocator,
        .pfnAllocation = vulkan_alloc_func,
        .pfnReallocation = vulkan_realloc_func,
        .pfnFree = vulkan_free_func,
        .pfnInternalAllocation = NULL,  // Optional, not needed
        .pfnInternalFree = NULL         // Optional, not needed
    };

    // Initialize fence tracking (allocate space for up to 1000 fences)
    device->fence_capacity = 1000;
    device->tracked_fences = ALLOC_ARRAY(permanent_allocator, VkFence, device->fence_capacity);
    device->fence_count = 0;

    // Create Vulkan instance
    device->instance = create_instance(device->temporary_allocator, &device->vk_alloc_callbacks);

    // Select physical device
    device->physical_device = select_physical_device(device->instance, device->temporary_allocator);

    // Find queue families
    find_queue_families(device->physical_device, &device->graphics_queue_family, &device->transfer_queue_family, device->temporary_allocator);

    // Create logical device
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = device->graphics_queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    VkPhysicalDeviceFeatures device_features = {0};

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL
    };

    VK_CHECK(vkCreateDevice(device->physical_device, &device_create_info, &device->vk_alloc_callbacks, &device->device));

    // Get device queues
    vkGetDeviceQueue(device->device, device->graphics_queue_family, 0, &device->graphics_queue);
    device->transfer_queue = device->graphics_queue;  // Use same queue for simplicity

    // Create command pool
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device->graphics_queue_family
    };

    VK_CHECK(vkCreateCommandPool(device->device, &pool_info, &device->vk_alloc_callbacks, &device->command_pool));

    // Create separate command pool for transfer operations (allows concurrent command buffer allocation)
    VkCommandPoolCreateInfo transfer_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = device->graphics_queue_family
    };

    VK_CHECK(vkCreateCommandPool(device->device, &transfer_pool_info, &device->vk_alloc_callbacks, &device->transfer_command_pool));

    // Removed loading of unused triangle shaders and test loading
    // Shaders are now loaded on demand when pipelines are created
    device->vertex_shader = VK_NULL_HANDLE;
    device->fragment_shader = VK_NULL_HANDLE;

    printf("[Vulkan] Device initialized\n");
    return device;
}

gpu_texture_t* gpu_create_texture(gpu_device_t* device, int width, int height) {
    gpu_texture_t* texture = ALLOC(device->permanent_allocator, gpu_texture_t);
    memset(texture, 0, sizeof(gpu_texture_t));  // Clear all fields including depth fields
    texture->width = width;
    texture->height = height;
    texture->format = VK_FORMAT_B8G8R8A8_UNORM;
    texture->device = device;

    // Create image
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = texture->format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VK_CHECK(vkCreateImage(device->device, &image_info, &device->vk_alloc_callbacks, &texture->image));

    // Allocate memory for image
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device->device, texture->image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &texture->memory));
    vkBindImageMemory(device->device, texture->image, texture->memory, 0);

    // Create image view
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = texture->format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_CHECK(vkCreateImageView(device->device, &view_info, &device->vk_alloc_callbacks, &texture->image_view));

    // Only create depth buffer for render targets (textures with COLOR_ATTACHMENT usage)
    // Check if this is a render target by checking the usage flags
    if (image_info.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        texture->depth_format = VK_FORMAT_D32_SFLOAT;

        // Create depth image
        VkImageCreateInfo depth_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = texture->depth_format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VK_CHECK(vkCreateImage(device->device, &depth_image_info, &device->vk_alloc_callbacks, &texture->depth_image));

    // Allocate memory for depth image
    VkMemoryRequirements depth_mem_requirements;
    vkGetImageMemoryRequirements(device->device, texture->depth_image, &depth_mem_requirements);

    VkMemoryAllocateInfo depth_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = depth_mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, depth_mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &depth_alloc_info, &device->vk_alloc_callbacks, &texture->depth_memory));
    vkBindImageMemory(device->device, texture->depth_image, texture->depth_memory, 0);

    // Create depth image view
    VkImageViewCreateInfo depth_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture->depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = texture->depth_format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_CHECK(vkCreateImageView(device->device, &depth_view_info, &device->vk_alloc_callbacks, &texture->depth_image_view));
    } // End of depth buffer creation (only for render targets)

    return texture;
}

// Helper function to execute commands immediately
static VkCommandBuffer gpu_begin_immediate_commands(gpu_device_t* device) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = device->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmd_buffer;
    VK_CHECK(vkAllocateCommandBuffers(device->device, &alloc_info, &cmd_buffer));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd_buffer, &begin_info));

    return cmd_buffer;
}

static void gpu_end_immediate_commands(gpu_device_t* device, VkCommandBuffer cmd_buffer) {
    VK_CHECK(vkEndCommandBuffer(cmd_buffer));

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer
    };

    VK_CHECK(vkQueueSubmit(device->graphics_queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(device->graphics_queue));

    vkFreeCommandBuffers(device->device, device->command_pool, 1, &cmd_buffer);
}

gpu_texture_t* gpu_create_texture_with_data(gpu_device_t* device, int width, int height,
                                           const void* data, size_t data_size) {
    gpu_texture_t* texture = ALLOC(device->permanent_allocator, gpu_texture_t);
    texture->width = width;
    texture->height = height;
    texture->format = VK_FORMAT_R8G8B8A8_UNORM; // RGBA8 format for typical textures
    texture->device = device;

    // Create image with transfer dst usage for uploading data
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = texture->format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VK_CHECK(vkCreateImage(device->device, &image_info, &device->vk_alloc_callbacks, &texture->image));

    // Allocate memory for image
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device->device, texture->image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &texture->memory));
    vkBindImageMemory(device->device, texture->image, texture->memory, 0);

    // Create staging buffer for uploading data
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = data_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(device->device, &buffer_info, &device->vk_alloc_callbacks, &staging_buffer));

    VkMemoryRequirements buffer_mem_requirements;
    vkGetBufferMemoryRequirements(device->device, staging_buffer, &buffer_mem_requirements);

    VkMemoryAllocateInfo buffer_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = buffer_mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, buffer_mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &buffer_alloc_info, &device->vk_alloc_callbacks, &staging_memory));
    vkBindBufferMemory(device->device, staging_buffer, staging_memory, 0);

    // Copy data to staging buffer
    void* mapped_data;
    vkMapMemory(device->device, staging_memory, 0, data_size, 0, &mapped_data);
    memcpy(mapped_data, data, data_size);
    vkUnmapMemory(device->device, staging_memory);

    // Copy staging buffer to image
    VkCommandBuffer cmd_buffer = gpu_begin_immediate_commands(device);

    // Transition image to transfer destination
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
    };

    vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, NULL, 0, NULL, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(cmd_buffer, staging_buffer, texture->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition image to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, NULL, 0, NULL, 1, &barrier);

    gpu_end_immediate_commands(device, cmd_buffer);

    // Clean up staging resources
    vkDestroyBuffer(device->device, staging_buffer, &device->vk_alloc_callbacks);
    vkFreeMemory(device->device, staging_memory, &device->vk_alloc_callbacks);

    // Create image view
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = texture->format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_CHECK(vkCreateImageView(device->device, &view_info, &device->vk_alloc_callbacks, &texture->image_view));

    return texture;
}

gpu_readback_buffer_t* gpu_create_readback_buffer(gpu_device_t* device, size_t size) {
    gpu_readback_buffer_t* buffer = ALLOC(device->permanent_allocator, gpu_readback_buffer_t);
    buffer->size = size;
    buffer->device = device;

    // Create staging buffer with host-visible memory
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(device->device, &buffer_info, &device->vk_alloc_callbacks, &buffer->buffer));

    // Allocate memory
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device->device, buffer->buffer, &mem_requirements);

    // Try cached memory first (better for CPU reads), fall back to coherent
    uint32_t memory_type = find_memory_type_optional(device->physical_device, mem_requirements.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if (memory_type == UINT32_MAX) {
        memory_type = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        buffer->is_coherent = true;
        printf("[Vulkan] Using coherent memory for readback buffers\n");
    } else {
        buffer->is_coherent = false;
        printf("[Vulkan] Using cached memory for readback buffers\n");
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &buffer->memory));
    vkBindBufferMemory(device->device, buffer->buffer, buffer->memory, 0);

    // Map memory for later access
    VK_CHECK(vkMapMemory(device->device, buffer->memory, 0, size, 0, &buffer->mapped_data));

    return buffer;
}

gpu_command_buffer_t* gpu_readback_yuv_textures_async(
    gpu_device_t* device,
    gpu_texture_t* y_texture,
    gpu_texture_t* u_texture,
    gpu_texture_t* v_texture,
    gpu_readback_buffer_t* buffer,
    int width,
    int height
) {
    gpu_command_buffer_t* cmd = gpu_begin_commands(device);

    // Create buffer copy regions for Y, U, V planes with proper offsets
    VkBufferImageCopy copy_regions[3];

    // Y plane copy (full resolution at offset 0)
    copy_regions[0] = (VkBufferImageCopy){
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    // U plane copy (quarter resolution at Y offset)
    copy_regions[1] = (VkBufferImageCopy){
        .bufferOffset = YUV_Y_SIZE_BYTES,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width/2, height/2, 1}
    };

    // V plane copy (quarter resolution at Y+U offset)
    copy_regions[2] = (VkBufferImageCopy){
        .bufferOffset = YUV_Y_SIZE_BYTES + YUV_UV_SIZE_BYTES,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width/2, height/2, 1}
    };

    // Copy Y plane
    vkCmdCopyImageToBuffer(cmd->cmd_buffer, y_texture->image, VK_IMAGE_LAYOUT_GENERAL,
                          buffer->buffer, 1, &copy_regions[0]);

    // Copy U plane
    vkCmdCopyImageToBuffer(cmd->cmd_buffer, u_texture->image, VK_IMAGE_LAYOUT_GENERAL,
                          buffer->buffer, 1, &copy_regions[1]);

    // Copy V plane
    vkCmdCopyImageToBuffer(cmd->cmd_buffer, v_texture->image, VK_IMAGE_LAYOUT_GENERAL,
                          buffer->buffer, 1, &copy_regions[2]);

    VK_CHECK(vkEndCommandBuffer(cmd->cmd_buffer));
    return cmd;
}

void gpu_submit_commands(gpu_command_buffer_t* cmd_buffer, bool wait) {
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer->cmd_buffer
    };

    VK_CHECK(vkQueueSubmit(cmd_buffer->device->graphics_queue, 1, &submit_info, cmd_buffer->fence));

    if (wait) {
        VK_CHECK(vkWaitForFences(cmd_buffer->device->device, 1, &cmd_buffer->fence, VK_TRUE, UINT64_MAX));
        cmd_buffer->completed = true;
    }
}

bool gpu_is_readback_complete(gpu_command_buffer_t* cmd_buffer) {
    if (cmd_buffer->completed) return true;

    VkResult result = vkGetFenceStatus(cmd_buffer->device->device, cmd_buffer->fence);
    if (result == VK_SUCCESS) {
        cmd_buffer->completed = true;
        return true;
    }
    return false;
}

void gpu_copy_readback_data(gpu_readback_buffer_t* buffer, void* dst, size_t size) {
    size_t copy_size = size < buffer->size ? size : buffer->size;

    // Invalidate cache if using cached memory (non-coherent)
    if (!buffer->is_coherent) {
        VkMappedMemoryRange range = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = buffer->memory,
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };
        vkInvalidateMappedMemoryRanges(buffer->device->device, 1, &range);
    }

    memcpy(dst, buffer->mapped_data, copy_size);
}

void gpu_destroy_command_buffer(gpu_command_buffer_t* cmd_buffer) {
    if (cmd_buffer) {
        if (cmd_buffer->fence) {
            vkDestroyFence(cmd_buffer->device->device, cmd_buffer->fence, &cmd_buffer->device->vk_alloc_callbacks);
        }
        if (cmd_buffer->cmd_buffer) {
            vkFreeCommandBuffers(cmd_buffer->device->device, cmd_buffer->device->command_pool, 1, &cmd_buffer->cmd_buffer);
        }
        // No need to free - allocated from arena allocator
    }
}

void gpu_destroy_texture(gpu_texture_t* texture) {
    if (texture) {
        // Destroy color resources
        if (texture->image_view) vkDestroyImageView(texture->device->device, texture->image_view, &texture->device->vk_alloc_callbacks);
        if (texture->image) vkDestroyImage(texture->device->device, texture->image, &texture->device->vk_alloc_callbacks);
        if (texture->memory) vkFreeMemory(texture->device->device, texture->memory, &texture->device->vk_alloc_callbacks);

        // Destroy depth resources
        if (texture->depth_image_view) vkDestroyImageView(texture->device->device, texture->depth_image_view, &texture->device->vk_alloc_callbacks);
        if (texture->depth_image) vkDestroyImage(texture->device->device, texture->depth_image, &texture->device->vk_alloc_callbacks);
        if (texture->depth_memory) vkFreeMemory(texture->device->device, texture->depth_memory, &texture->device->vk_alloc_callbacks);
        // No need to free - allocated from arena allocator
    }
}

void gpu_destroy_readback_buffer(gpu_readback_buffer_t* buffer) {
    if (buffer) {
        if (buffer->mapped_data) {
            vkUnmapMemory(buffer->device->device, buffer->memory);
        }
        vkDestroyBuffer(buffer->device->device, buffer->buffer, &buffer->device->vk_alloc_callbacks);
        vkFreeMemory(buffer->device->device, buffer->memory, &buffer->device->vk_alloc_callbacks);
        // No need to free - allocated from arena allocator
    }
}

// === Rendering Implementation ===

// New flexible pipeline creation with descriptor
gpu_pipeline_t* gpu_create_pipeline_desc(gpu_device_t* device, const gpu_pipeline_desc_t* desc) {
    gpu_pipeline_t* pipeline = ALLOC_ARRAY(device->permanent_allocator, gpu_pipeline_t, 1);
    pipeline->device = device;
    pipeline->has_uniforms = (desc->num_uniform_buffers > 0);
    pipeline->num_texture_bindings = desc->num_texture_bindings;

    // Load shaders
    PROFILE_BEGIN("gpu_pipeline: load shaders");
    VkShaderModule vert_shader = load_shader_module(device->device, desc->vertex_shader_path, device->temporary_allocator, &device->vk_alloc_callbacks);
    VkShaderModule frag_shader = load_shader_module(device->device, desc->fragment_shader_path, device->temporary_allocator, &device->vk_alloc_callbacks);
    PROFILE_END();

    if (!vert_shader || !frag_shader) {
        printf("Failed to load shaders: %s, %s\n", desc->vertex_shader_path, desc->fragment_shader_path);
        return NULL;
    }

    // Create default sampler for textures
    if (desc->num_texture_bindings > 0) {
        PROFILE_BEGIN("gpu_pipeline: create sampler");
        VkSamplerCreateInfo sampler_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .mipLodBias = 0.0f,
            .minLod = 0.0f,
            .maxLod = 0.0f
        };
        VK_CHECK(vkCreateSampler(device->device, &sampler_info, &device->vk_alloc_callbacks, &pipeline->default_sampler));

        // Store texture descriptors
        pipeline->texture_descs = ALLOC_ARRAY(device->permanent_allocator, gpu_texture_desc_t, desc->num_texture_bindings);
        memcpy(pipeline->texture_descs, desc->texture_bindings, sizeof(gpu_texture_desc_t) * desc->num_texture_bindings);
        PROFILE_END();
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_shader,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_shader,
            .pName = "main"
        }
    };

    // Vertex input
    PROFILE_BEGIN("gpu_pipeline: setup vertex input");
    VkVertexInputBindingDescription binding_description = {
        .binding = 0,
        .stride = desc->vertex_layout->stride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription* attribute_descriptions =
        ALLOC_ARRAY(device->temporary_allocator, VkVertexInputAttributeDescription, desc->vertex_layout->num_attributes);

    for (int i = 0; i < desc->vertex_layout->num_attributes; i++) {
        gpu_vertex_attr_t* attr = &desc->vertex_layout->attributes[i];
        attribute_descriptions[i].binding = 0;
        attribute_descriptions[i].location = attr->index;
        attribute_descriptions[i].offset = attr->offset;

        // Map format
        switch (attr->format) {
            case 0: attribute_descriptions[i].format = VK_FORMAT_R32G32_SFLOAT; break;
            case 1: attribute_descriptions[i].format = VK_FORMAT_R32G32B32_SFLOAT; break;
            case 2: attribute_descriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT; break;
            case 3: attribute_descriptions[i].format = VK_FORMAT_R8G8B8A8_UINT; break; // ubyte4 as unsigned integers
            default: attribute_descriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT; break;
        }
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = desc->vertex_layout->num_attributes,
        .pVertexAttributeDescriptions = attribute_descriptions
    };
    PROFILE_END();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = desc->cull_mode == 1 ? VK_CULL_MODE_BACK_BIT :
                   desc->cull_mode == 2 ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    // Depth stencil state
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = desc->depth_test ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = desc->depth_write ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // Color blending
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    // Dynamic state
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states
    };

    // Create uniform buffers if needed
    if (desc->num_uniform_buffers > 0) {
        PROFILE_BEGIN("gpu_pipeline: create uniform buffers");
        pipeline->num_uniform_buffers = desc->num_uniform_buffers;
        pipeline->uniform_buffers = ALLOC_ARRAY(device->permanent_allocator, VkBuffer, desc->num_uniform_buffers);
        pipeline->uniform_memories = ALLOC_ARRAY(device->permanent_allocator, VkDeviceMemory, desc->num_uniform_buffers);
        pipeline->uniform_mapped = ALLOC_ARRAY(device->permanent_allocator, void*, desc->num_uniform_buffers);
        pipeline->uniform_sizes = ALLOC_ARRAY(device->permanent_allocator, size_t, desc->num_uniform_buffers);

        for (uint32_t i = 0; i < desc->num_uniform_buffers; i++) {
            pipeline->uniform_sizes[i] = desc->uniform_buffers[i].size;

            VkBufferCreateInfo buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = desc->uniform_buffers[i].size,
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            VK_CHECK(vkCreateBuffer(device->device, &buffer_info, &device->vk_alloc_callbacks, &pipeline->uniform_buffers[i]));

            VkMemoryRequirements mem_requirements;
            vkGetBufferMemoryRequirements(device->device, pipeline->uniform_buffers[i], &mem_requirements);

            VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = mem_requirements.size,
                .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            };

            VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &pipeline->uniform_memories[i]));
            VK_CHECK(vkBindBufferMemory(device->device, pipeline->uniform_buffers[i], pipeline->uniform_memories[i], 0));
            VK_CHECK(vkMapMemory(device->device, pipeline->uniform_memories[i], 0, desc->uniform_buffers[i].size, 0, &pipeline->uniform_mapped[i]));
        }

        // Keep backward compatibility pointers
        pipeline->uniform_buffer = pipeline->uniform_buffers[0];
        pipeline->uniform_buffer_memory = pipeline->uniform_memories[0];
        pipeline->uniform_buffer_mapped = pipeline->uniform_mapped[0];
        PROFILE_END();
    }

    // Create storage buffers if needed
    if (desc->num_storage_buffers > 0) {
        PROFILE_BEGIN("gpu_pipeline: create storage buffers");
        pipeline->num_storage_buffers = desc->num_storage_buffers;
        pipeline->storage_buffers = ALLOC_ARRAY(device->permanent_allocator, VkBuffer, desc->num_storage_buffers);
        pipeline->storage_memories = ALLOC_ARRAY(device->permanent_allocator, VkDeviceMemory, desc->num_storage_buffers);
        pipeline->storage_mapped = ALLOC_ARRAY(device->permanent_allocator, void*, desc->num_storage_buffers);
        pipeline->storage_sizes = ALLOC_ARRAY(device->permanent_allocator, size_t, desc->num_storage_buffers);

        for (uint32_t i = 0; i < desc->num_storage_buffers; i++) {
            pipeline->storage_sizes[i] = desc->storage_buffers[i].size;

            VkBufferCreateInfo buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = desc->storage_buffers[i].size,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            VK_CHECK(vkCreateBuffer(device->device, &buffer_info, &device->vk_alloc_callbacks, &pipeline->storage_buffers[i]));

            VkMemoryRequirements mem_requirements;
            vkGetBufferMemoryRequirements(device->device, pipeline->storage_buffers[i], &mem_requirements);

            VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = mem_requirements.size,
                .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            };

            VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &pipeline->storage_memories[i]));
            VK_CHECK(vkBindBufferMemory(device->device, pipeline->storage_buffers[i], pipeline->storage_memories[i], 0));
            VK_CHECK(vkMapMemory(device->device, pipeline->storage_memories[i], 0, desc->storage_buffers[i].size, 0, &pipeline->storage_mapped[i]));
        }

        // Keep backward compatibility
        if (desc->num_storage_buffers > 0) {
            pipeline->storage_buffer = pipeline->storage_buffers[0];
            pipeline->storage_buffer_memory = pipeline->storage_memories[0];
            pipeline->storage_buffer_mapped = pipeline->storage_mapped[0];
        }
        PROFILE_END();
    }

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding* layout_bindings = NULL;
    uint32_t total_bindings = desc->num_uniform_buffers + desc->num_storage_buffers + desc->num_texture_bindings;

    if (total_bindings > 0) {
        PROFILE_BEGIN("gpu_pipeline: create descriptor layout");
        layout_bindings = ALLOC_ARRAY(device->temporary_allocator, VkDescriptorSetLayoutBinding, total_bindings);
        uint32_t binding_index = 0;

        // Add uniform buffer bindings
        for (uint32_t i = 0; i < desc->num_uniform_buffers; i++) {
            VkShaderStageFlags stage_flags = 0;
            if (desc->uniform_buffers[i].stage_flags & GPU_STAGE_VERTEX) {
                stage_flags |= VK_SHADER_STAGE_VERTEX_BIT;
            }
            if (desc->uniform_buffers[i].stage_flags & GPU_STAGE_FRAGMENT) {
                stage_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            layout_bindings[binding_index++] = (VkDescriptorSetLayoutBinding){
                .binding = desc->uniform_buffers[i].binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = stage_flags
            };
        }

        // Add storage buffer bindings
        for (uint32_t i = 0; i < desc->num_storage_buffers; i++) {
            VkShaderStageFlags stage_flags = 0;
            if (desc->storage_buffers[i].stage_flags & GPU_STAGE_VERTEX) {
                stage_flags |= VK_SHADER_STAGE_VERTEX_BIT;
            }
            if (desc->storage_buffers[i].stage_flags & GPU_STAGE_FRAGMENT) {
                stage_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            layout_bindings[binding_index++] = (VkDescriptorSetLayoutBinding){
                .binding = desc->storage_buffers[i].binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = stage_flags
            };
        }

        // Add texture/sampler bindings
        for (uint32_t i = 0; i < desc->num_texture_bindings; i++) {
            VkShaderStageFlags stage_flags = 0;
            if (desc->texture_bindings[i].stage_flags & GPU_STAGE_VERTEX) {
                stage_flags |= VK_SHADER_STAGE_VERTEX_BIT;
            }
            if (desc->texture_bindings[i].stage_flags & GPU_STAGE_FRAGMENT) {
                stage_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            layout_bindings[binding_index++] = (VkDescriptorSetLayoutBinding){
                .binding = desc->texture_bindings[i].binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = stage_flags
            };
        }

        VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = total_bindings,
            .pBindings = layout_bindings
        };

        VK_CHECK(vkCreateDescriptorSetLayout(device->device, &layout_info, &device->vk_alloc_callbacks, &pipeline->descriptor_set_layout));
        PROFILE_END();

        // Create descriptor pool
        PROFILE_BEGIN("gpu_pipeline: create descriptor pool");
        VkDescriptorPoolSize* pool_sizes = ALLOC_ARRAY(device->temporary_allocator, VkDescriptorPoolSize, 4);  // Increased to 4 for samplers
        uint32_t pool_size_count = 0;

        if (desc->num_uniform_buffers > 0) {
            pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = desc->num_uniform_buffers
            };
        }

        if (desc->num_storage_buffers > 0) {
            pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = desc->num_storage_buffers
            };
        }

        if (desc->num_texture_bindings > 0) {
            pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = desc->num_texture_bindings
            };
            pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = desc->num_texture_bindings
            };
        }

        // Create pool for multiple descriptor sets (up to 1000 draws per frame)
        const uint32_t MAX_DESCRIPTOR_SETS = 16;
        pipeline->max_descriptor_sets = MAX_DESCRIPTOR_SETS;

        // Scale pool sizes for multiple sets
        for (uint32_t i = 0; i < pool_size_count; i++) {
            pool_sizes[i].descriptorCount *= MAX_DESCRIPTOR_SETS;
        }

        VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,  // Allow pool reset
            .maxSets = MAX_DESCRIPTOR_SETS,
            .poolSizeCount = pool_size_count,
            .pPoolSizes = pool_sizes
        };

        VK_CHECK(vkCreateDescriptorPool(device->device, &pool_info, &device->vk_alloc_callbacks, &pipeline->descriptor_pool));
        printf("[Vulkan] Created descriptor pool with %u max sets\n", MAX_DESCRIPTOR_SETS);
        PROFILE_END();

        // Pre-allocate buffer pools for all descriptor sets
        if (desc->num_uniform_buffers > 0) {
            PROFILE_BEGIN("gpu_pipeline: preallocate uniform pool");
            uint32_t total_uniform_buffers = MAX_DESCRIPTOR_SETS * desc->num_uniform_buffers;
            pipeline->uniform_buffer_pool = ALLOC_ARRAY(device->permanent_allocator, VkBuffer, total_uniform_buffers);
            pipeline->uniform_memory_pool = ALLOC_ARRAY(device->permanent_allocator, VkDeviceMemory, total_uniform_buffers);
            pipeline->uniform_mapped_pool = ALLOC_ARRAY(device->permanent_allocator, void*, total_uniform_buffers);

            for (uint32_t set_idx = 0; set_idx < MAX_DESCRIPTOR_SETS; set_idx++) {
                for (uint32_t buf_idx = 0; buf_idx < desc->num_uniform_buffers; buf_idx++) {
                    uint32_t pool_idx = set_idx * desc->num_uniform_buffers + buf_idx;

                    VkBufferCreateInfo buffer_info = {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                        .size = desc->uniform_buffers[buf_idx].size,
                        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
                    };

                    VK_CHECK(vkCreateBuffer(device->device, &buffer_info, &device->vk_alloc_callbacks, &pipeline->uniform_buffer_pool[pool_idx]));

                    VkMemoryRequirements mem_requirements;
                    vkGetBufferMemoryRequirements(device->device, pipeline->uniform_buffer_pool[pool_idx], &mem_requirements);

                    VkMemoryAllocateInfo alloc_info = {
                        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                        .allocationSize = mem_requirements.size,
                        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                    };

                    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &pipeline->uniform_memory_pool[pool_idx]));
                    VK_CHECK(vkBindBufferMemory(device->device, pipeline->uniform_buffer_pool[pool_idx],
                                               pipeline->uniform_memory_pool[pool_idx], 0));
                    VK_CHECK(vkMapMemory(device->device, pipeline->uniform_memory_pool[pool_idx], 0,
                                        desc->uniform_buffers[buf_idx].size, 0, &pipeline->uniform_mapped_pool[pool_idx]));
                }
            }
            printf("[Vulkan] Pre-allocated %u uniform buffers\n", total_uniform_buffers);
            PROFILE_END();
        }

        if (desc->num_storage_buffers > 0) {
            PROFILE_BEGIN("gpu_pipeline: preallocate storage pool");
            uint32_t total_storage_buffers = MAX_DESCRIPTOR_SETS * desc->num_storage_buffers;
            pipeline->storage_buffer_pool = ALLOC_ARRAY(device->permanent_allocator, VkBuffer, total_storage_buffers);
            pipeline->storage_memory_pool = ALLOC_ARRAY(device->permanent_allocator, VkDeviceMemory, total_storage_buffers);
            pipeline->storage_mapped_pool = ALLOC_ARRAY(device->permanent_allocator, void*, total_storage_buffers);

            for (uint32_t set_idx = 0; set_idx < MAX_DESCRIPTOR_SETS; set_idx++) {
                for (uint32_t buf_idx = 0; buf_idx < desc->num_storage_buffers; buf_idx++) {
                    uint32_t pool_idx = set_idx * desc->num_storage_buffers + buf_idx;

                    VkBufferCreateInfo buffer_info = {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                        .size = desc->storage_buffers[buf_idx].size,
                        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
                    };

                    VK_CHECK(vkCreateBuffer(device->device, &buffer_info, &device->vk_alloc_callbacks, &pipeline->storage_buffer_pool[pool_idx]));

                    VkMemoryRequirements mem_requirements;
                    vkGetBufferMemoryRequirements(device->device, pipeline->storage_buffer_pool[pool_idx], &mem_requirements);

                    VkMemoryAllocateInfo alloc_info = {
                        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                        .allocationSize = mem_requirements.size,
                        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                    };

                    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &pipeline->storage_memory_pool[pool_idx]));
                    VK_CHECK(vkBindBufferMemory(device->device, pipeline->storage_buffer_pool[pool_idx],
                                               pipeline->storage_memory_pool[pool_idx], 0));
                    VK_CHECK(vkMapMemory(device->device, pipeline->storage_memory_pool[pool_idx], 0,
                                        desc->storage_buffers[buf_idx].size, 0, &pipeline->storage_mapped_pool[pool_idx]));
                }
            }
            printf("[Vulkan] Pre-allocated %u storage buffers\n", total_storage_buffers);
            PROFILE_END();
        }

        pipeline->next_buffer_index = 0;

        // Allocate descriptor set
        PROFILE_BEGIN("gpu_pipeline: allocate descriptor sets");
        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pipeline->descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &pipeline->descriptor_set_layout
        };

        VK_CHECK(vkAllocateDescriptorSets(device->device, &alloc_info, &pipeline->descriptor_set));
        PROFILE_END();

        // Write descriptor sets
        PROFILE_BEGIN("gpu_pipeline: write descriptor sets");
        VkWriteDescriptorSet* writes = ALLOC_ARRAY(device->temporary_allocator, VkWriteDescriptorSet, total_bindings);
        VkDescriptorBufferInfo* buffer_infos = ALLOC_ARRAY(device->temporary_allocator, VkDescriptorBufferInfo, total_bindings);
        uint32_t write_count = 0;

        // Write uniform buffer descriptors
        for (uint32_t i = 0; i < desc->num_uniform_buffers; i++) {
            buffer_infos[write_count] = (VkDescriptorBufferInfo){
                .buffer = pipeline->uniform_buffers[i],
                .offset = 0,
                .range = desc->uniform_buffers[i].size
            };

            writes[write_count] = (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = pipeline->descriptor_set,
                .dstBinding = desc->uniform_buffers[i].binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_infos[write_count]
            };
            write_count++;
        }

        // Write storage buffer descriptors
        for (uint32_t i = 0; i < desc->num_storage_buffers; i++) {
            buffer_infos[write_count] = (VkDescriptorBufferInfo){
                .buffer = pipeline->storage_buffers[i],
                .offset = 0,
                .range = desc->storage_buffers[i].size
            };

            writes[write_count] = (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = pipeline->descriptor_set,
                .dstBinding = desc->storage_buffers[i].binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &buffer_infos[write_count]
            };
            write_count++;
        }

        // Create default white texture for initial binding
        gpu_texture_t* default_texture = NULL;
        VkDescriptorImageInfo* image_infos = NULL;
        if (desc->num_texture_bindings > 0) {
            // Create default white texture
            uint32_t white_pixel = 0xFFFFFFFF;  // RGBA format - white with full alpha
            default_texture = gpu_create_texture_with_data(device, 1, 1, &white_pixel, sizeof(white_pixel));

            image_infos = ALLOC_ARRAY(device->temporary_allocator, VkDescriptorImageInfo, desc->num_texture_bindings);

            // Write texture descriptors
            for (uint32_t i = 0; i < desc->num_texture_bindings; i++) {
                image_infos[i] = (VkDescriptorImageInfo){
                    .sampler = pipeline->default_sampler,
                    .imageView = default_texture->image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };

                writes[write_count] = (VkWriteDescriptorSet){
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = pipeline->descriptor_set,
                    .dstBinding = desc->texture_bindings[i].binding,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &image_infos[i]
                };
                write_count++;
            }
        }

        vkUpdateDescriptorSets(device->device, write_count, writes, 0, NULL);
        PROFILE_END();
    }

    // Create render pass with color and depth attachments
    PROFILE_BEGIN("gpu_pipeline: create render pass");
    VkAttachmentDescription attachments[2] = {
        // Color attachment
        {
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        // Depth attachment
        {
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depth_attachment_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    VK_CHECK(vkCreateRenderPass(device->device, &render_pass_info, &device->vk_alloc_callbacks, &pipeline->render_pass));
    PROFILE_END();

    // Pipeline layout
    PROFILE_BEGIN("gpu_pipeline: create pipeline layout");
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = total_bindings > 0 ? 1 : 0,
        .pSetLayouts = total_bindings > 0 ? &pipeline->descriptor_set_layout : NULL,
        .pushConstantRangeCount = 0
    };

    VK_CHECK(vkCreatePipelineLayout(device->device, &pipeline_layout_info, &device->vk_alloc_callbacks, &pipeline->pipeline_layout));
    PROFILE_END();

    // Create graphics pipeline
    PROFILE_BEGIN("gpu_pipeline: create graphics pipeline");
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = pipeline->pipeline_layout,
        .renderPass = pipeline->render_pass,
        .subpass = 0
    };

    VK_CHECK(vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, &device->vk_alloc_callbacks, &pipeline->pipeline));
    PROFILE_END();

    PROFILE_BEGIN("gpu_pipeline: cleanup");
    vkDestroyShaderModule(device->device, vert_shader, &device->vk_alloc_callbacks);
    vkDestroyShaderModule(device->device, frag_shader, &device->vk_alloc_callbacks);
    PROFILE_END();

    // Store uniform buffer info for flexible updates
    pipeline->uniform_buffer_descs = ALLOC_ARRAY(device->permanent_allocator, gpu_uniform_buffer_desc_t, desc->num_uniform_buffers);
    memcpy(pipeline->uniform_buffer_descs, desc->uniform_buffers, sizeof(gpu_uniform_buffer_desc_t) * desc->num_uniform_buffers);

    if (desc->num_storage_buffers > 0) {
        pipeline->storage_buffer_descs = ALLOC_ARRAY(device->permanent_allocator, gpu_storage_buffer_desc_t, desc->num_storage_buffers);
        memcpy(pipeline->storage_buffer_descs, desc->storage_buffers, sizeof(gpu_storage_buffer_desc_t) * desc->num_storage_buffers);
    }

    return pipeline;
}

// New slot-based uniform update function (backward compatibility)
void gpu_update_uniforms(gpu_pipeline_t* pipeline, uint32_t binding, const void* data, size_t size) {
    if (!pipeline || !pipeline->has_uniforms) {
        return;
    }

    // Find the uniform buffer with this binding
    for (uint32_t i = 0; i < pipeline->num_uniform_buffers; i++) {
        if (pipeline->uniform_buffer_descs[i].binding == binding) {
            if (pipeline->uniform_mapped[i] && size <= pipeline->uniform_sizes[i]) {
                memcpy(pipeline->uniform_mapped[i], data, size);
            }
            return;
        }
    }
}

// Allocate a new descriptor set from the pipeline's pool
gpu_descriptor_set_t* gpu_allocate_descriptor_set(gpu_pipeline_t* pipeline) {
    if (!pipeline || !pipeline->descriptor_pool) {
        return NULL;
    }

    // Check if we have available buffers in the pool
    if (pipeline->next_buffer_index >= pipeline->max_descriptor_sets) {
        printf("[Vulkan] WARNING: Descriptor set pool exhausted! Consider increasing pool size.\n");
        return NULL;
    }

    gpu_descriptor_set_t* desc_set = ALLOC(pipeline->device->temporary_allocator, gpu_descriptor_set_t);
    desc_set->pipeline = pipeline;

    // Allocate descriptor set from pool
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pipeline->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &pipeline->descriptor_set_layout
    };

    VK_CHECK(vkAllocateDescriptorSets(pipeline->device->device, &alloc_info, &desc_set->descriptor_set));

    // Use pre-allocated uniform buffers from the pool
    if (pipeline->num_uniform_buffers > 0) {
        desc_set->uniform_buffers = ALLOC_ARRAY(pipeline->device->temporary_allocator, VkBuffer, pipeline->num_uniform_buffers);
        desc_set->uniform_memories = ALLOC_ARRAY(pipeline->device->temporary_allocator, VkDeviceMemory, pipeline->num_uniform_buffers);
        desc_set->uniform_mapped = ALLOC_ARRAY(pipeline->device->temporary_allocator, void*, pipeline->num_uniform_buffers);

        uint32_t buffer_base_idx = pipeline->next_buffer_index * pipeline->num_uniform_buffers;
        for (uint32_t i = 0; i < pipeline->num_uniform_buffers; i++) {
            uint32_t pool_idx = buffer_base_idx + i;

            // Simply reference the pre-allocated buffers from the pool
            desc_set->uniform_buffers[i] = pipeline->uniform_buffer_pool[pool_idx];
            desc_set->uniform_memories[i] = pipeline->uniform_memory_pool[pool_idx];
            desc_set->uniform_mapped[i] = pipeline->uniform_mapped_pool[pool_idx];
        }
    }

    // Use pre-allocated storage buffers from the pool
    if (pipeline->num_storage_buffers > 0) {
        desc_set->storage_buffers = ALLOC_ARRAY(pipeline->device->temporary_allocator, VkBuffer, pipeline->num_storage_buffers);
        desc_set->storage_memories = ALLOC_ARRAY(pipeline->device->temporary_allocator, VkDeviceMemory, pipeline->num_storage_buffers);
        desc_set->storage_mapped = ALLOC_ARRAY(pipeline->device->temporary_allocator, void*, pipeline->num_storage_buffers);

        uint32_t buffer_base_idx = pipeline->next_buffer_index * pipeline->num_storage_buffers;
        for (uint32_t i = 0; i < pipeline->num_storage_buffers; i++) {
            uint32_t pool_idx = buffer_base_idx + i;

            // Simply reference the pre-allocated buffers from the pool
            desc_set->storage_buffers[i] = pipeline->storage_buffer_pool[pool_idx];
            desc_set->storage_memories[i] = pipeline->storage_memory_pool[pool_idx];
            desc_set->storage_mapped[i] = pipeline->storage_mapped_pool[pool_idx];
        }
    }

    // Increment the buffer index for next allocation
    pipeline->next_buffer_index++;

    // Write descriptor set with buffers and textures
    uint32_t total_bindings = pipeline->num_uniform_buffers + pipeline->num_storage_buffers + pipeline->num_texture_bindings;
    if (total_bindings > 0) {
        VkWriteDescriptorSet* writes = ALLOC_ARRAY(pipeline->device->temporary_allocator, VkWriteDescriptorSet, total_bindings);
        VkDescriptorBufferInfo* buffer_infos = ALLOC_ARRAY(pipeline->device->temporary_allocator, VkDescriptorBufferInfo, total_bindings);
        VkDescriptorImageInfo* image_infos = NULL;
        uint32_t write_count = 0;

        // Write uniform buffer descriptors
        for (uint32_t i = 0; i < pipeline->num_uniform_buffers; i++) {
            buffer_infos[write_count] = (VkDescriptorBufferInfo){
                .buffer = desc_set->uniform_buffers[i],
                .offset = 0,
                .range = pipeline->uniform_sizes[i]
            };

            writes[write_count] = (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = desc_set->descriptor_set,
                .dstBinding = pipeline->uniform_buffer_descs[i].binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_infos[write_count]
            };
            write_count++;
        }

        // Write storage buffer descriptors
        for (uint32_t i = 0; i < pipeline->num_storage_buffers; i++) {
            buffer_infos[write_count] = (VkDescriptorBufferInfo){
                .buffer = desc_set->storage_buffers[i],
                .offset = 0,
                .range = pipeline->storage_sizes[i]
            };

            writes[write_count] = (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = desc_set->descriptor_set,
                .dstBinding = pipeline->storage_buffer_descs[i].binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &buffer_infos[write_count]
            };
            write_count++;
        }

        // Copy texture bindings from default descriptor set if any
        if (pipeline->num_texture_bindings > 0) {
            // Create default white texture if needed
            static gpu_texture_t* default_white_texture = NULL;
            if (!default_white_texture) {
                uint32_t white_pixel = 0xFFFFFFFF;
                default_white_texture = gpu_create_texture_with_data(pipeline->device, 1, 1, &white_pixel, sizeof(white_pixel));
            }

            image_infos = ALLOC_ARRAY(pipeline->device->temporary_allocator, VkDescriptorImageInfo, pipeline->num_texture_bindings);

            for (uint32_t i = 0; i < pipeline->num_texture_bindings; i++) {
                image_infos[i] = (VkDescriptorImageInfo){
                    .sampler = pipeline->default_sampler,
                    .imageView = default_white_texture->image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };

                writes[write_count] = (VkWriteDescriptorSet){
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = desc_set->descriptor_set,
                    .dstBinding = pipeline->texture_descs[i].binding,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &image_infos[i]
                };
                write_count++;
            }
        }

        vkUpdateDescriptorSets(pipeline->device->device, write_count, writes, 0, NULL);
    }

    return desc_set;
}

// Update uniforms in a specific descriptor set
void gpu_update_descriptor_uniforms(gpu_descriptor_set_t* descriptor_set, uint32_t binding, const void* data, size_t size) {
    if (!descriptor_set || !descriptor_set->pipeline) {
        return;
    }

    gpu_pipeline_t* pipeline = descriptor_set->pipeline;

    // Find the uniform buffer with this binding
    for (uint32_t i = 0; i < pipeline->num_uniform_buffers; i++) {
        if (pipeline->uniform_buffer_descs[i].binding == binding) {
            if (descriptor_set->uniform_mapped[i] && size <= pipeline->uniform_sizes[i]) {
                memcpy(descriptor_set->uniform_mapped[i], data, size);
            }
            return;
        }
    }

    // Check storage buffers too
    for (uint32_t i = 0; i < pipeline->num_storage_buffers; i++) {
        if (pipeline->storage_buffer_descs[i].binding == binding) {
            if (descriptor_set->storage_mapped[i] && size <= pipeline->storage_sizes[i]) {
                memcpy(descriptor_set->storage_mapped[i], data, size);
            }
            return;
        }
    }
}

// Update texture in a specific descriptor set
void gpu_update_descriptor_texture(gpu_descriptor_set_t* descriptor_set, gpu_texture_t* texture, uint32_t binding) {
    if (!descriptor_set || !texture || !descriptor_set->pipeline) {
        return;
    }

    gpu_pipeline_t* pipeline = descriptor_set->pipeline;

    // Find the texture binding
    for (uint32_t i = 0; i < pipeline->num_texture_bindings; i++) {
        if (pipeline->texture_descs[i].binding == binding) {
            VkDescriptorImageInfo image_info = {
                .sampler = pipeline->default_sampler,
                .imageView = texture->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptor_set->descriptor_set,
                .dstBinding = binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_info
            };

            vkUpdateDescriptorSets(pipeline->device->device, 1, &write, 0, NULL);
            return;
        }
    }
}

// Update storage buffer in a specific descriptor set
void gpu_update_descriptor_storage_buffer(gpu_descriptor_set_t* descriptor_set, gpu_buffer_t* buffer, uint32_t binding) {
    if (!descriptor_set || !buffer || !descriptor_set->pipeline) {
        return;
    }

    gpu_pipeline_t* pipeline = descriptor_set->pipeline;

    // Create descriptor buffer info
    VkDescriptorBufferInfo buffer_info = {
        .buffer = buffer->buffer,
        .offset = 0,
        .range = buffer->size
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set->descriptor_set,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_info
    };

    vkUpdateDescriptorSets(pipeline->device->device, 1, &write, 0, NULL);
}

// Bind a specific descriptor set for rendering
void gpu_bind_descriptor_set(gpu_render_encoder_t* encoder, gpu_pipeline_t* pipeline, gpu_descriptor_set_t* descriptor_set) {
    if (!encoder || !pipeline || !descriptor_set) {
        return;
    }

    vkCmdBindDescriptorSets(encoder->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pipeline->pipeline_layout, 0, 1, &descriptor_set->descriptor_set, 0, NULL);
}

// Reset descriptor pool (call at frame start)
void gpu_reset_pipeline_descriptor_pool(gpu_pipeline_t* pipeline) {
    if (!pipeline || !pipeline->descriptor_pool) {
        return;
    }

    // Reset descriptor pool to free all allocated descriptor sets
    VK_CHECK(vkResetDescriptorPool(pipeline->device->device, pipeline->descriptor_pool, 0));

    // Reset the buffer pool index so buffers can be reused
    pipeline->next_buffer_index = 0;
}

gpu_buffer_t* gpu_create_buffer(gpu_device_t* device, const void* data, size_t size) {
    gpu_buffer_t* buffer = ALLOC(device->permanent_allocator, gpu_buffer_t);
    buffer->size = size;
    buffer->device = device;

    // Create buffer
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(device->device, &buffer_info, &device->vk_alloc_callbacks, &buffer->buffer));

    // Allocate memory
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device->device, buffer->buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &buffer->memory));
    vkBindBufferMemory(device->device, buffer->buffer, buffer->memory, 0);

    // Copy data
    if (data) {
        void* mapped_data;
        vkMapMemory(device->device, buffer->memory, 0, size, 0, &mapped_data);
        memcpy(mapped_data, data, size);
        vkUnmapMemory(device->device, buffer->memory);
    }

    return buffer;
}

gpu_buffer_t* gpu_create_storage_buffer(gpu_device_t* device, const void* data, size_t size) {
    gpu_buffer_t* buffer = ALLOC(device->permanent_allocator, gpu_buffer_t);
    buffer->size = size;
    buffer->device = device;

    // Create buffer with storage buffer usage
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(device->device, &buffer_info, &device->vk_alloc_callbacks, &buffer->buffer));

    // Allocate memory
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device->device, buffer->buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &buffer->memory));
    vkBindBufferMemory(device->device, buffer->buffer, buffer->memory, 0);

    // Copy data
    if (data) {
        void* mapped_data;
        vkMapMemory(device->device, buffer->memory, 0, size, 0, &mapped_data);
        memcpy(mapped_data, data, size);
        vkUnmapMemory(device->device, buffer->memory);
    }

    return buffer;
}

gpu_command_buffer_t* gpu_begin_commands(gpu_device_t* device) {
    gpu_command_buffer_t* cmd = ALLOC(device->temporary_allocator, gpu_command_buffer_t);
    cmd->device = device;
    cmd->completed = false;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = device->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VK_CHECK(vkAllocateCommandBuffers(device->device, &alloc_info, &cmd->cmd_buffer));

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0
    };

    VK_CHECK(vkCreateFence(device->device, &fence_info, &device->vk_alloc_callbacks, &cmd->fence));

    // Track the fence for later cleanup
    if (device->fence_count < device->fence_capacity) {
        device->tracked_fences[device->fence_count++] = cmd->fence;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd->cmd_buffer, &begin_info));

    return cmd;
}

gpu_render_encoder_t* gpu_begin_render_pass(
    gpu_command_buffer_t* cmd_buffer,
    gpu_texture_t* target
) {
    gpu_render_encoder_t* encoder = ALLOC(cmd_buffer->device->temporary_allocator, gpu_render_encoder_t);
    encoder->cmd_buffer = cmd_buffer->cmd_buffer;
    encoder->target = target;
    encoder->device = cmd_buffer->device;
    encoder->render_pass = NULL;  // Will be set from first pipeline
    encoder->framebuffer = VK_NULL_HANDLE;

    return encoder;
}

void gpu_set_pipeline(gpu_render_encoder_t* encoder, gpu_pipeline_t* pipeline, float clear_color[4]) {
    // First pipeline sets up the render pass
    if (!encoder->render_pass) {
        encoder->render_pass = pipeline->render_pass;

        // Create framebuffer with both color and depth attachments
        // Check if depth buffer exists (only for render targets)
        if (!encoder->target->depth_image_view) {
            printf("[Vulkan] ERROR: Render target missing depth buffer!\n");
            return;
        }

        VkImageView attachments[2] = {
            encoder->target->image_view,       // Color attachment
            encoder->target->depth_image_view  // Depth attachment
        };

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = pipeline->render_pass,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = (uint32_t)encoder->target->width,
            .height = (uint32_t)encoder->target->height,
            .layers = 1
        };

        VK_CHECK(vkCreateFramebuffer(encoder->device->device, &framebuffer_info, &encoder->device->vk_alloc_callbacks, &encoder->framebuffer));

        // Create clear values for both color and depth
        VkClearValue clear_values[2];
        clear_values[0].color.float32[0] = clear_color[0];
        clear_values[0].color.float32[1] = clear_color[1];
        clear_values[0].color.float32[2] = clear_color[2];
        clear_values[0].color.float32[3] = clear_color[3];
        clear_values[1].depthStencil.depth = 1.0f;  // Clear depth to 1.0 (far)
        clear_values[1].depthStencil.stencil = 0;

        // Begin render pass ONLY for the first pipeline
        VkRenderPassBeginInfo render_pass_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = pipeline->render_pass,
            .framebuffer = encoder->framebuffer,
            .renderArea = {
                .offset = {0, 0},
                .extent = {(uint32_t)encoder->target->width, (uint32_t)encoder->target->height}
            },
            .clearValueCount = 2,
            .pClearValues = clear_values
        };

        vkCmdBeginRenderPass(encoder->cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        // Set viewport with Y-flip to match OpenGL convention (only once)
        VkViewport viewport = {
            .x = 0.0f,
            .y = (float)encoder->target->height,  // Start from bottom
            .width = (float)encoder->target->width,
            .height = -(float)encoder->target->height,  // Negative height flips Y
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(encoder->cmd_buffer, 0, 1, &viewport);

        // Set scissor (only once)
        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {(uint32_t)encoder->target->width, (uint32_t)encoder->target->height}
        };
        vkCmdSetScissor(encoder->cmd_buffer, 0, 1, &scissor);
    }

    // Always bind the pipeline and descriptor sets
    vkCmdBindPipeline(encoder->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

    // Bind descriptor sets if pipeline has uniforms
    if (pipeline->has_uniforms) {
        vkCmdBindDescriptorSets(encoder->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               pipeline->pipeline_layout, 0, 1, &pipeline->descriptor_set, 0, NULL);
    }
}

void gpu_set_vertex_buffer(gpu_render_encoder_t* encoder, gpu_buffer_t* buffer, int index) {
    (void)index;  // Vulkan uses binding point 0
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(encoder->cmd_buffer, 0, 1, &buffer->buffer, offsets);
}

void gpu_set_index_buffer(gpu_render_encoder_t* encoder, gpu_buffer_t* buffer) {
    if (encoder && buffer && buffer->buffer) {
        vkCmdBindIndexBuffer(encoder->cmd_buffer, buffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

void gpu_draw(gpu_render_encoder_t* encoder, int index_count) {
    // Always use indexed drawing with 32-bit indices
    vkCmdDrawIndexed(encoder->cmd_buffer, index_count, 1, 0, 0, 0);
}

void gpu_end_render_pass(gpu_render_encoder_t* encoder) {
    vkCmdEndRenderPass(encoder->cmd_buffer);

    // Clean up framebuffer (should be cached in production)
    if (encoder->framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(encoder->device->device, encoder->framebuffer, &encoder->device->vk_alloc_callbacks);
    }

    // No need to free - allocated from arena allocator
}

void gpu_commit_commands(gpu_command_buffer_t* cmd_buffer, bool wait) {
    VK_CHECK(vkEndCommandBuffer(cmd_buffer->cmd_buffer));

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer->cmd_buffer
    };

    VK_CHECK(vkQueueSubmit(cmd_buffer->device->graphics_queue, 1, &submit_info, cmd_buffer->fence));

    if (wait) {
        VK_CHECK(vkWaitForFences(cmd_buffer->device->device, 1, &cmd_buffer->fence, VK_TRUE, UINT64_MAX));
        cmd_buffer->completed = true;
    }
}

void gpu_destroy_pipeline(gpu_pipeline_t* pipeline) {
    if (pipeline) {
        vkDestroyPipeline(pipeline->device->device, pipeline->pipeline, &pipeline->device->vk_alloc_callbacks);
        vkDestroyPipelineLayout(pipeline->device->device, pipeline->pipeline_layout, &pipeline->device->vk_alloc_callbacks);
        vkDestroyRenderPass(pipeline->device->device, pipeline->render_pass, &pipeline->device->vk_alloc_callbacks);

        // Clean up uniform buffer resources if present
        if (pipeline->has_uniforms) {
            vkDestroyDescriptorPool(pipeline->device->device, pipeline->descriptor_pool, &pipeline->device->vk_alloc_callbacks);
            vkDestroyDescriptorSetLayout(pipeline->device->device, pipeline->descriptor_set_layout, &pipeline->device->vk_alloc_callbacks);
            if (pipeline->uniform_buffer_mapped) {
                vkUnmapMemory(pipeline->device->device, pipeline->uniform_buffer_memory);
            }
            vkDestroyBuffer(pipeline->device->device, pipeline->uniform_buffer, &pipeline->device->vk_alloc_callbacks);
            vkFreeMemory(pipeline->device->device, pipeline->uniform_buffer_memory, &pipeline->device->vk_alloc_callbacks);
        }
        // No need to free - allocated from arena allocator
    }
}

void gpu_destroy_buffer(gpu_buffer_t* buffer) {
    if (buffer) {
        vkDestroyBuffer(buffer->device->device, buffer->buffer, &buffer->device->vk_alloc_callbacks);
        vkFreeMemory(buffer->device->device, buffer->memory, &buffer->device->vk_alloc_callbacks);
        // No need to free - allocated from arena allocator
    }
}

// === Compute Pipeline Implementation ===

gpu_compute_pipeline_t* gpu_create_compute_pipeline(gpu_device_t* device, const char* compute_shader_path, int max_frames) {
    gpu_compute_pipeline_t* compute_pipeline = ALLOC(device->permanent_allocator, gpu_compute_pipeline_t);
    compute_pipeline->device = device;

    // Load compute shader
    compute_pipeline->compute_shader = load_shader_module(device->device, compute_shader_path, device->temporary_allocator, &device->vk_alloc_callbacks);
    if (!compute_pipeline->compute_shader) {
        fprintf(stderr, "Failed to load compute shader: %s\n", compute_shader_path);
        return NULL;
    }

    // Create descriptor set layout for storage images
    VkDescriptorSetLayoutBinding bindings[4] = {
        // Input BGRA texture
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        // Output Y texture
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        // Output U texture
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        // Output V texture
        {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        }
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 4,
        .pBindings = bindings
    };

    VK_CHECK(vkCreateDescriptorSetLayout(device->device, &layout_info, &device->vk_alloc_callbacks, &compute_pipeline->descriptor_set_layout));

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &compute_pipeline->descriptor_set_layout
    };

    VK_CHECK(vkCreatePipelineLayout(device->device, &pipeline_layout_info, &device->vk_alloc_callbacks, &compute_pipeline->pipeline_layout));

    // Create compute pipeline
    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compute_pipeline->compute_shader,
            .pName = "main"
        },
        .layout = compute_pipeline->pipeline_layout
    };

    VK_CHECK(vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, &device->vk_alloc_callbacks, &compute_pipeline->pipeline));

    // Create descriptor pool (big enough for max_frames * 4 textures each)
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = max_frames * 4  // max_frames * 4 textures per frame
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,  // Allow freeing individual sets
        .maxSets = max_frames,  // max_frames descriptor sets (one per frame)
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };

    VK_CHECK(vkCreateDescriptorPool(device->device, &pool_info, &device->vk_alloc_callbacks, &compute_pipeline->descriptor_pool));

    printf("[Vulkan] Compute pipeline created (descriptor pool: %d sets, %d images)\n", max_frames, max_frames * 4);
    return compute_pipeline;
}

gpu_texture_t* gpu_create_storage_texture(gpu_device_t* device, int width, int height, int format) {
    gpu_texture_t* texture = ALLOC(device->permanent_allocator, gpu_texture_t);
    texture->width = width;
    texture->height = height;
    texture->device = device;

    // Map format: 0=RGBA8, 1=R8
    switch (format) {
        case 0: texture->format = VK_FORMAT_R8G8B8A8_UNORM; break;
        case 1: texture->format = VK_FORMAT_R8_UNORM; break;
        default: texture->format = VK_FORMAT_R8G8B8A8_UNORM; break;
    }

    // Create image with storage usage
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = texture->format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VK_CHECK(vkCreateImage(device->device, &image_info, &device->vk_alloc_callbacks, &texture->image));

    // Allocate memory for image
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device->device, texture->image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, &device->vk_alloc_callbacks, &texture->memory));
    vkBindImageMemory(device->device, texture->image, texture->memory, 0);

    // Create image view
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = texture->format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_CHECK(vkCreateImageView(device->device, &view_info, &device->vk_alloc_callbacks, &texture->image_view));

    return texture;
}

void gpu_dispatch_compute(
    gpu_command_buffer_t* cmd_buffer,
    gpu_compute_pipeline_t* pipeline,
    gpu_texture_t** textures,
    int num_textures,
    int groups_x, int groups_y, int groups_z
) {
    // Transition images to GENERAL layout for compute shader access
    VkImageMemoryBarrier barriers[4];
    for (int i = 0; i < num_textures && i < 4; i++) {
        barriers[i] = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = (i == 0) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = textures[i]->image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .srcAccessMask = (i == 0) ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        };
    }

    vkCmdPipelineBarrier(cmd_buffer->cmd_buffer,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 0, NULL, 0, NULL, num_textures, barriers);

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pipeline->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &pipeline->descriptor_set_layout
    };

    VkDescriptorSet descriptor_set;
    VK_CHECK(vkAllocateDescriptorSets(cmd_buffer->device->device, &alloc_info, &descriptor_set));

    // Update descriptor set with texture bindings
    VkDescriptorImageInfo image_infos[4];
    VkWriteDescriptorSet writes[4];

    for (int i = 0; i < num_textures && i < 4; i++) {
        image_infos[i] = (VkDescriptorImageInfo){
            .imageView = textures[i]->image_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = i,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &image_infos[i]
        };
    }

    vkUpdateDescriptorSets(cmd_buffer->device->device, num_textures, writes, 0, NULL);

    // Bind compute pipeline and dispatch
    vkCmdBindPipeline(cmd_buffer->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
    vkCmdBindDescriptorSets(cmd_buffer->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                           pipeline->pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
    vkCmdDispatch(cmd_buffer->cmd_buffer, groups_x, groups_y, groups_z);

    // Add memory barrier after compute to ensure writes are visible to subsequent operations
    VkMemoryBarrier memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT
    };

    vkCmdPipelineBarrier(cmd_buffer->cmd_buffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 1, &memory_barrier, 0, NULL, 0, NULL);
}

void gpu_destroy_compute_pipeline(gpu_compute_pipeline_t* pipeline) {
    if (pipeline) {
        vkDestroyPipeline(pipeline->device->device, pipeline->pipeline, &pipeline->device->vk_alloc_callbacks);
        vkDestroyPipelineLayout(pipeline->device->device, pipeline->pipeline_layout, &pipeline->device->vk_alloc_callbacks);
        vkDestroyDescriptorSetLayout(pipeline->device->device, pipeline->descriptor_set_layout, &pipeline->device->vk_alloc_callbacks);
        vkDestroyDescriptorPool(pipeline->device->device, pipeline->descriptor_pool, &pipeline->device->vk_alloc_callbacks);
        vkDestroyShaderModule(pipeline->device->device, pipeline->compute_shader, &pipeline->device->vk_alloc_callbacks);
        // No need to free - allocated from arena allocator
    }
}

void gpu_reset_command_pools(gpu_device_t* device) {
    if (device) {
        // Destroy all tracked fences first
        uint32_t fences_destroyed = device->fence_count;
        for (uint32_t i = 0; i < device->fence_count; i++) {
            if (device->tracked_fences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(device->device, device->tracked_fences[i], &device->vk_alloc_callbacks);
            }
        }
        device->fence_count = 0;
        if (fences_destroyed > 0) {
            printf("[Vulkan] Destroyed %u tracked fences\n", fences_destroyed);
        }

        // Reset command pools to free all allocated command buffers
        // This is much more efficient than freeing individual command buffers
        if (device->command_pool) {
            VK_CHECK(vkResetCommandPool(device->device, device->command_pool, 0));
        }
        if (device->transfer_command_pool) {
            VK_CHECK(vkResetCommandPool(device->device, device->transfer_command_pool, 0));
        }
        printf("[Vulkan] Command pools reset - all command buffers freed\n");
    }
}

void gpu_reset_compute_descriptor_pool(gpu_compute_pipeline_t* pipeline) {
    if (pipeline && pipeline->descriptor_pool) {
        // Reset descriptor pool to free all allocated descriptor sets
        VK_CHECK(vkResetDescriptorPool(pipeline->device->device, pipeline->descriptor_pool, 0));
        printf("[Vulkan] Descriptor pool reset - all descriptor sets freed\n");
    }
}

// Update texture in pipeline's descriptor set
void gpu_update_pipeline_texture(gpu_pipeline_t* pipeline,
                                 gpu_texture_t* texture,
                                 uint32_t binding) {
    if (!pipeline || !texture || pipeline->num_texture_bindings == 0) {
        printf("[Vulkan] WARNING: Cannot update texture - pipeline=%p, texture=%p, num_bindings=%d\n",
               (void*)pipeline, (void*)texture, pipeline ? pipeline->num_texture_bindings : -1);
        return;
    }

    // Find the texture binding
    for (uint32_t i = 0; i < pipeline->num_texture_bindings; i++) {
        if (pipeline->texture_descs[i].binding == binding) {
            // Update the descriptor set with the new texture
            VkDescriptorImageInfo image_info = {
                .sampler = pipeline->default_sampler,
                .imageView = texture->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = pipeline->descriptor_set,
                .dstBinding = binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_info
            };

            vkUpdateDescriptorSets(pipeline->device->device, 1, &write, 0, NULL);
            // Removed forced flush for performance - descriptor update is immediate
            return;
        }
    }
}

void gpu_destroy(gpu_device_t* device) {
    if (device) {
        // Shaders now destroyed with their pipelines
        if (device->command_pool) {
            vkDestroyCommandPool(device->device, device->command_pool, &device->vk_alloc_callbacks);
        }
        if (device->transfer_command_pool) {
            vkDestroyCommandPool(device->device, device->transfer_command_pool, &device->vk_alloc_callbacks);
        }
        if (device->device) {
            vkDestroyDevice(device->device, &device->vk_alloc_callbacks);
        }
        if (device->instance) {
            vkDestroyInstance(device->instance, &device->vk_alloc_callbacks);
        }
        free(device);
    }
}

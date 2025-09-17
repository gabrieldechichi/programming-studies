#include <vulkan/vulkan.h>
#include "gpu_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_FRAMES_IN_FLIGHT 2

// Helper macro for checking Vulkan results
#define VK_CHECK(call) \
    do { \
        VkResult result = call; \
        if (result != VK_SUCCESS) { \
            fprintf(stderr, "Vulkan error in %s at line %d: %d\n", __FILE__, __LINE__, result); \
            exit(1); \
        } \
    } while(0)

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
};

struct gpu_texture {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory memory;
    VkFormat format;
    int width;
    int height;
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

struct gpu_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkRenderPass render_pass;
    VkFramebuffer* framebuffers;  // One per texture target
};

struct gpu_buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    size_t size;
};

struct gpu_render_encoder {
    VkCommandBuffer cmd_buffer;
    gpu_texture_t* target;
    VkFramebuffer framebuffer;
    VkRenderPass render_pass;
    gpu_pipeline_t* pipeline;  // Store pipeline for push constants
    gpu_device_t* device;      // Need device reference
};

struct gpu_compute_pipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkShaderModule compute_shader;
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
static VkShaderModule load_shader_module(VkDevice device, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open shader file: %s\n", filename);
        return VK_NULL_HANDLE;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint32_t* code = (uint32_t*)malloc(file_size);
    fread(code, 1, file_size, file);
    fclose(file);

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = file_size,
        .pCode = code
    };

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(device, &create_info, NULL, &shader_module));

    free(code);
    return shader_module;
}

// Initialize Vulkan instance
static VkInstance create_instance(void) {
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
    VkLayerProperties* available_layers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    bool validation_found = false;
    for (uint32_t i = 0; i < layer_count; i++) {
        if (strcmp(available_layers[i].layerName, validation_layers[0]) == 0) {
            validation_found = true;
            break;
        }
    }
    free(available_layers);

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = validation_found ? 1 : 0,
        .ppEnabledLayerNames = validation_found ? validation_layers : NULL,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL
    };

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&create_info, NULL, &instance));

    if (validation_found) {
        printf("[Vulkan] Validation layers enabled\n");
    }

    return instance;
}

// Select physical device
static VkPhysicalDevice select_physical_device(VkInstance instance) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);

    if (device_count == 0) {
        fprintf(stderr, "Failed to find GPUs with Vulkan support\n");
        exit(1);
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
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

    free(devices);
    return selected_device;
}

// Find queue families
static void find_queue_families(VkPhysicalDevice physical_device, uint32_t* graphics_family, uint32_t* transfer_family) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
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

    free(queue_families);

    if (*graphics_family == UINT32_MAX) {
        fprintf(stderr, "Failed to find graphics queue family\n");
        exit(1);
    }
}

gpu_device_t* gpu_init(void) {
    gpu_device_t* device = (gpu_device_t*)calloc(1, sizeof(gpu_device_t));

    // Create Vulkan instance
    device->instance = create_instance();

    // Select physical device
    device->physical_device = select_physical_device(device->instance);

    // Find queue families
    find_queue_families(device->physical_device, &device->graphics_queue_family, &device->transfer_queue_family);

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

    VK_CHECK(vkCreateDevice(device->physical_device, &device_create_info, NULL, &device->device));

    // Get device queues
    vkGetDeviceQueue(device->device, device->graphics_queue_family, 0, &device->graphics_queue);
    device->transfer_queue = device->graphics_queue;  // Use same queue for simplicity

    // Create command pool
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device->graphics_queue_family
    };

    VK_CHECK(vkCreateCommandPool(device->device, &pool_info, NULL, &device->command_pool));

    // Create separate command pool for transfer operations (allows concurrent command buffer allocation)
    VkCommandPoolCreateInfo transfer_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = device->graphics_queue_family
    };

    VK_CHECK(vkCreateCommandPool(device->device, &transfer_pool_info, NULL, &device->transfer_command_pool));

    // Load shader modules (will be compiled to SPIR-V by build system)
    device->vertex_shader = load_shader_module(device->device, "triangle.vert.spv");
    device->fragment_shader = load_shader_module(device->device, "triangle.frag.spv");

    if (!device->vertex_shader || !device->fragment_shader) {
        // Try different paths
        device->vertex_shader = load_shader_module(device->device, "out/linux/triangle.vert.spv");
        device->fragment_shader = load_shader_module(device->device, "out/linux/triangle.frag.spv");
    }

    printf("[Vulkan] Device initialized\n");
    return device;
}

void* gpu_get_native_device(gpu_device_t* device) {
    return device->device;
}

gpu_texture_t* gpu_create_texture(gpu_device_t* device, int width, int height) {
    gpu_texture_t* texture = (gpu_texture_t*)calloc(1, sizeof(gpu_texture_t));
    texture->width = width;
    texture->height = height;
    texture->format = VK_FORMAT_B8G8R8A8_UNORM;

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

    VK_CHECK(vkCreateImage(device->device, &image_info, NULL, &texture->image));

    // Allocate memory for image
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device->device, texture->image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &texture->memory));
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

    VK_CHECK(vkCreateImageView(device->device, &view_info, NULL, &texture->image_view));

    return texture;
}

void* gpu_get_native_texture(gpu_texture_t* texture) {
    return texture->image;
}

gpu_readback_buffer_t* gpu_create_readback_buffer(gpu_device_t* device, size_t size) {
    gpu_readback_buffer_t* buffer = (gpu_readback_buffer_t*)calloc(1, sizeof(gpu_readback_buffer_t));
    buffer->size = size;
    buffer->device = device;

    // Create staging buffer with host-visible memory
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(device->device, &buffer_info, NULL, &buffer->buffer));

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

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &buffer->memory));
    vkBindBufferMemory(device->device, buffer->buffer, buffer->memory, 0);

    // Map memory for later access
    VK_CHECK(vkMapMemory(device->device, buffer->memory, 0, size, 0, &buffer->mapped_data));

    return buffer;
}

gpu_command_buffer_t* gpu_readback_texture_async(
    gpu_device_t* device,
    gpu_texture_t* texture,
    gpu_readback_buffer_t* buffer,
    int width,
    int height
) {
    gpu_command_buffer_t* cmd = (gpu_command_buffer_t*)calloc(1, sizeof(gpu_command_buffer_t));
    cmd->device = device;
    cmd->completed = false;

    // Allocate command buffer from transfer pool for better concurrency
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = device->transfer_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VK_CHECK(vkAllocateCommandBuffers(device->device, &alloc_info, &cmd->cmd_buffer));

    // Create fence for synchronization
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0
    };

    VK_CHECK(vkCreateFence(device->device, &fence_info, NULL, &cmd->fence));

    // Begin command buffer
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd->cmd_buffer, &begin_info));

    // Transition image layout from color attachment to transfer source
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT
    };

    vkCmdPipelineBarrier(cmd->cmd_buffer,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, NULL, 0, NULL, 1, &barrier);

    // Copy image to buffer
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

    vkCmdCopyImageToBuffer(cmd->cmd_buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer->buffer, 1, &region);

    // Transition image back to color attachment
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd->cmd_buffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        0, 0, NULL, 0, NULL, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(cmd->cmd_buffer));

    return cmd;
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

void* gpu_get_readback_data(gpu_readback_buffer_t* buffer) {
    return buffer->mapped_data;
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
            vkDestroyFence(cmd_buffer->device->device, cmd_buffer->fence, NULL);
        }
        if (cmd_buffer->cmd_buffer) {
            vkFreeCommandBuffers(cmd_buffer->device->device, cmd_buffer->device->command_pool, 1, &cmd_buffer->cmd_buffer);
        }
        free(cmd_buffer);
    }
}

void gpu_destroy_texture(gpu_texture_t* texture) {
    if (texture) {
        gpu_device_t* device = NULL;  // Need to store device reference in texture struct
        // Note: In production code, we'd need to store the device reference
        // For now, we'll leak these resources (they'll be cleaned up on program exit)
        free(texture);
    }
}

void gpu_destroy_readback_buffer(gpu_readback_buffer_t* buffer) {
    if (buffer) {
        // Note: In production code, we'd need to store the device reference
        // For now, we'll leak these resources
        free(buffer);
    }
}

// === Rendering Implementation ===

gpu_pipeline_t* gpu_create_pipeline(
    gpu_device_t* device,
    const char* shader_source,
    const char* vertex_function,
    const char* fragment_function,
    gpu_vertex_layout_t* vertex_layout
) {
    (void)shader_source;  // Not used in Vulkan version
    (void)vertex_function;
    (void)fragment_function;

    gpu_pipeline_t* pipeline = (gpu_pipeline_t*)calloc(1, sizeof(gpu_pipeline_t));

    // Create render pass
    VkAttachmentDescription color_attachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    VK_CHECK(vkCreateRenderPass(device->device, &render_pass_info, NULL, &pipeline->render_pass));

    // Shader stages
    VkPipelineShaderStageCreateInfo vert_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = device->vertex_shader,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo frag_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = device->fragment_shader,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info, frag_stage_info};

    // Vertex input
    VkVertexInputBindingDescription binding_description = {
        .binding = 0,
        .stride = vertex_layout->stride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription* attribute_descriptions =
        (VkVertexInputAttributeDescription*)malloc(sizeof(VkVertexInputAttributeDescription) * vertex_layout->num_attributes);

    for (int i = 0; i < vertex_layout->num_attributes; i++) {
        gpu_vertex_attr_t* attr = &vertex_layout->attributes[i];
        attribute_descriptions[i].binding = 0;
        attribute_descriptions[i].location = attr->index;
        attribute_descriptions[i].offset = attr->offset;

        // Map format
        switch (attr->format) {
            case 0: attribute_descriptions[i].format = VK_FORMAT_R32G32_SFLOAT; break;
            case 1: attribute_descriptions[i].format = VK_FORMAT_R32G32B32_SFLOAT; break;
            case 2: attribute_descriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT; break;
            default: attribute_descriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT; break;
        }
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = vertex_layout->num_attributes,
        .pVertexAttributeDescriptions = attribute_descriptions
    };

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
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
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

    // Push constants for uniforms
    VkPushConstantRange push_constant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(float) * 16  // mat4
    };

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant
    };

    VK_CHECK(vkCreatePipelineLayout(device->device, &pipeline_layout_info, NULL, &pipeline->pipeline_layout));

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = pipeline->pipeline_layout,
        .renderPass = pipeline->render_pass,
        .subpass = 0
    };

    VK_CHECK(vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline->pipeline));

    free(attribute_descriptions);

    printf("[Vulkan] Pipeline created\n");
    return pipeline;
}

gpu_buffer_t* gpu_create_buffer(gpu_device_t* device, const void* data, size_t size) {
    gpu_buffer_t* buffer = (gpu_buffer_t*)calloc(1, sizeof(gpu_buffer_t));
    buffer->size = size;

    // Create buffer
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(device->device, &buffer_info, NULL, &buffer->buffer));

    // Allocate memory
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device->device, buffer->buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &buffer->memory));
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
    gpu_command_buffer_t* cmd = (gpu_command_buffer_t*)calloc(1, sizeof(gpu_command_buffer_t));
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

    VK_CHECK(vkCreateFence(device->device, &fence_info, NULL, &cmd->fence));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd->cmd_buffer, &begin_info));

    return cmd;
}

gpu_render_encoder_t* gpu_begin_render_pass(
    gpu_command_buffer_t* cmd_buffer,
    gpu_texture_t* target,
    float clear_r, float clear_g, float clear_b, float clear_a
) {
    (void)clear_r; (void)clear_g; (void)clear_b; (void)clear_a;  // Suppress warnings for now

    gpu_render_encoder_t* encoder = (gpu_render_encoder_t*)calloc(1, sizeof(gpu_render_encoder_t));
    encoder->cmd_buffer = cmd_buffer->cmd_buffer;
    encoder->target = target;
    encoder->device = cmd_buffer->device;
    encoder->render_pass = NULL;  // Will be set when pipeline is bound
    encoder->framebuffer = VK_NULL_HANDLE;  // Will be created when pipeline is set

    return encoder;
}

void gpu_set_pipeline(gpu_render_encoder_t* encoder, gpu_pipeline_t* pipeline) {
    encoder->render_pass = pipeline->render_pass;
    encoder->pipeline = pipeline;

    // Create framebuffer for the target texture
    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = pipeline->render_pass,
        .attachmentCount = 1,
        .pAttachments = &encoder->target->image_view,
        .width = (uint32_t)encoder->target->width,
        .height = (uint32_t)encoder->target->height,
        .layers = 1
    };

    VK_CHECK(vkCreateFramebuffer(encoder->device->device, &framebuffer_info, NULL, &encoder->framebuffer));

    // Create clear value
    VkClearValue clear_value;
    clear_value.color.float32[0] = 0.0f;
    clear_value.color.float32[1] = 0.0f;
    clear_value.color.float32[2] = 0.0f;
    clear_value.color.float32[3] = 1.0f;

    // Begin render pass
    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = pipeline->render_pass,
        .framebuffer = encoder->framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = {(uint32_t)encoder->target->width, (uint32_t)encoder->target->height}
        },
        .clearValueCount = 1,
        .pClearValues = &clear_value
    };

    vkCmdBeginRenderPass(encoder->cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the pipeline
    vkCmdBindPipeline(encoder->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

    // Set viewport
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)encoder->target->width,
        .height = (float)encoder->target->height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(encoder->cmd_buffer, 0, 1, &viewport);

    // Set scissor
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {(uint32_t)encoder->target->width, (uint32_t)encoder->target->height}
    };
    vkCmdSetScissor(encoder->cmd_buffer, 0, 1, &scissor);
}

void gpu_set_vertex_buffer(gpu_render_encoder_t* encoder, gpu_buffer_t* buffer, int index) {
    (void)index;  // Vulkan uses binding point 0
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(encoder->cmd_buffer, 0, 1, &buffer->buffer, offsets);
}

void gpu_set_uniforms(gpu_render_encoder_t* encoder, int index, const void* data, size_t size) {
    (void)index;  // Not used in push constants

    if (encoder->pipeline && data && size > 0) {
        vkCmdPushConstants(encoder->cmd_buffer, encoder->pipeline->pipeline_layout,
                          VK_SHADER_STAGE_VERTEX_BIT, 0, (uint32_t)size, data);
    }
}

void gpu_draw(gpu_render_encoder_t* encoder, int vertex_count) {
    vkCmdDraw(encoder->cmd_buffer, vertex_count, 1, 0, 0);
}

void gpu_end_render_pass(gpu_render_encoder_t* encoder) {
    vkCmdEndRenderPass(encoder->cmd_buffer);

    // Clean up framebuffer (should be cached in production)
    if (encoder->framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(encoder->device->device, encoder->framebuffer, NULL);
    }

    free(encoder);
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

void gpu_wait_for_command_buffer(gpu_command_buffer_t* cmd_buffer) {
    if (!cmd_buffer || cmd_buffer->completed) {
        return;
    }

    VK_CHECK(vkWaitForFences(cmd_buffer->device->device, 1, &cmd_buffer->fence, VK_TRUE, UINT64_MAX));
    cmd_buffer->completed = true;
}

void gpu_destroy_pipeline(gpu_pipeline_t* pipeline) {
    if (pipeline) {
        // Note: Need device reference to properly clean up
        free(pipeline);
    }
}

void gpu_destroy_buffer(gpu_buffer_t* buffer) {
    if (buffer) {
        // Note: Need device reference to properly clean up
        free(buffer);
    }
}

// === Compute Pipeline Implementation ===

gpu_compute_pipeline_t* gpu_create_compute_pipeline(gpu_device_t* device, const char* compute_shader_path) {
    gpu_compute_pipeline_t* compute_pipeline = (gpu_compute_pipeline_t*)calloc(1, sizeof(gpu_compute_pipeline_t));

    // Load compute shader
    compute_pipeline->compute_shader = load_shader_module(device->device, compute_shader_path);
    if (!compute_pipeline->compute_shader) {
        fprintf(stderr, "Failed to load compute shader: %s\n", compute_shader_path);
        free(compute_pipeline);
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

    VK_CHECK(vkCreateDescriptorSetLayout(device->device, &layout_info, NULL, &compute_pipeline->descriptor_set_layout));

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &compute_pipeline->descriptor_set_layout
    };

    VK_CHECK(vkCreatePipelineLayout(device->device, &pipeline_layout_info, NULL, &compute_pipeline->pipeline_layout));

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

    VK_CHECK(vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &compute_pipeline->pipeline));

    // Create descriptor pool (big enough for 200 frames * 4 textures each)
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 800  // 200 frames * 4 textures per frame
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,  // Allow freeing individual sets
        .maxSets = 200,  // 200 descriptor sets (one per frame)
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };

    VK_CHECK(vkCreateDescriptorPool(device->device, &pool_info, NULL, &compute_pipeline->descriptor_pool));

    printf("[Vulkan] Compute pipeline created\n");
    return compute_pipeline;
}

gpu_texture_t* gpu_create_storage_texture(gpu_device_t* device, int width, int height, int format) {
    gpu_texture_t* texture = (gpu_texture_t*)calloc(1, sizeof(gpu_texture_t));
    texture->width = width;
    texture->height = height;

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

    VK_CHECK(vkCreateImage(device->device, &image_info, NULL, &texture->image));

    // Allocate memory for image
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device->device, texture->image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device, mem_requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &texture->memory));
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

    VK_CHECK(vkCreateImageView(device->device, &view_info, NULL, &texture->image_view));

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
        // Note: Need device reference to properly clean up
        // For now, we'll leak these resources (they'll be cleaned up on program exit)
        free(pipeline);
    }
}

void gpu_destroy(gpu_device_t* device) {
    if (device) {
        if (device->vertex_shader) {
            vkDestroyShaderModule(device->device, device->vertex_shader, NULL);
        }
        if (device->fragment_shader) {
            vkDestroyShaderModule(device->device, device->fragment_shader, NULL);
        }
        if (device->command_pool) {
            vkDestroyCommandPool(device->device, device->command_pool, NULL);
        }
        if (device->transfer_command_pool) {
            vkDestroyCommandPool(device->device, device->transfer_command_pool, NULL);
        }
        if (device->device) {
            vkDestroyDevice(device->device, NULL);
        }
        if (device->instance) {
            vkDestroyInstance(device->instance, NULL);
        }
        free(device);
    }
}

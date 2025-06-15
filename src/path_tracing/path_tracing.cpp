#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <glm/vec2.hpp>
#include <stb/stb_image_write.h>
#include <cornell_box.hpp>

#include <minilog.hpp>

#include <cstdint>
#include <vector>
#include <cstring> // for strcmp
#include <string_view>
#include <cmath>

using namespace std::literals::string_literals;

#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYER { false };
#else
    constexpr bool ENABLE_VALIDATION_LAYER { true };
#endif


const std::vector<const char*> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> INSTANCE_EXTENSIONS = { vk::EXTDebugUtilsExtensionName };
// const std::vector<const char*> logical_device_extensions = { VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME };


VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageType,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    switch (messageSeverity) {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose: {
            minilog::log_trace("Vulkan Validation Layer [verbose]: {}", pCallbackData->pMessage);
            return vk::False;
        }
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo: {
            minilog::log_trace("Vulkan Validation Layer [info]: {}", pCallbackData->pMessage);
            return vk::False;
        }
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning: {
            minilog::log_trace("Vulkan Validation Layer [warning]: {}", pCallbackData->pMessage);
            return vk::False;
        }
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError: {
            minilog::log_trace("Vulkan Validation Layer [error]: {}", pCallbackData->pMessage);
            return vk::False;
        }

        default: { return vk::False; }
    }

    return vk::False;
}



struct PushConstantData {
    glm::uvec2 screen_size { 0u, 0u };
    std::uint32_t hittableCount { 0u };
    std::uint32_t sample_start { 0u };
    std::uint32_t samples { 0u };
    std::uint32_t total_samples { 0u };
    std::uint32_t max_depth { 0u };
};


struct CornellBoxSceneData {
    std::vector<float> output_image;
    std::vector<float> seed_image;
    std::vector<float> vertices;
};


class PathTracing {
    PushConstantData constant_data;
    CornellBoxSceneData scene_data;

    vk::Instance instance { nullptr };
    bool validation_layers_supported { false };
    vk::DebugUtilsMessengerEXT debug_utils_messenger { nullptr };

    vk::Queue compute_queue;
    std::optional<std::uint32_t> compute_queue_family_index;
    vk::PhysicalDevice physical_device { nullptr };
    std::uint32_t compute_shader_process_unit { 0u };
    vk::Device logical_device { nullptr };

    std::array<vk::Buffer, 2uz> uniform_buffers;
    std::array<vk::DeviceMemory, 2uz> uniform_buffer_memorys;
    std::array<vk::Buffer, 5uz> storage_buffers;
    std::array <vk::DeviceMemory, 5uz> storage_buffer_memorys;

    vk::DescriptorPool descriptor_pool;
    std::array<vk::DescriptorSetLayout, 2uz> descriptor_set_layouts;
    std::array<vk::DescriptorSet, 2uz> descriptor_sets;

    vk::PipelineLayout pipeline_layout;
    vk::Pipeline compute_pipeline;

    vk::CommandPool command_pool;
    std::array<vk::CommandBuffer, 1uz> command_buffers;

public:
    PathTracing() {
        prepare_scene_data();
    }


    ~PathTracing() {
        // vkDestroyCommandPool(logical_device, command_pool, nullptr);
        // vkDestroyDescriptorPool(logical_device, descriptor_pool, nullptr);
        // vkDestroyPipeline(logical_device, compute_pipeline, nullptr);
        // vkDestroyPipelineLayout(logical_device, pipeline_layout, nullptr);
        // for (std::size_t i = 0uz; i < descriptor_set_layouts.size(); ++i) {
        //     vkDestroyDescriptorSetLayout(logical_device, descriptor_set_layouts[i], nullptr);
        // }

        // for (std::size_t i = 0uz; i < storageBuffers.size(); ++i) {
        //     vkDestroyBuffer(logical_device, storage_buffers[i], nullptr);
        //     vkFreeMemory(logical_device, storage_buffer_memorys[i], nullptr);
        // }

        // vkDestroyBuffer(logical_device, uniform_buffer, nullptr);
        // vkFreeMemory(logical_device, uniform_buffer_memory, nullptr);

        // MuVk::Proxy::destoryDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        // vkDestroyDevice(logical_device, nullptr);
        // vkDestroyInstance(instance, nullptr);
    }

    void prepare_scene_data() {
        tinyobj::ObjReaderConfig obj_reader_config;
        obj_reader_config.triangulate = true;
        obj_reader_config.triangulation_method = "simple";
        obj_reader_config.vertex_color = false;
        obj_reader_config.mtl_search_path = ""s;
        tinyobj::ObjReader obj_reader;
        if (!obj_reader.ParseFromString(cornell_box_string, "", obj_reader_config)) {
            std::string_view error_message = "unknown error.";
            if (auto &&e = obj_reader.Error(); !e.empty()) { error_message = e; }
            std::cout << "Failed to load OBJ file: " << error_message << std::endl;
        }
        if (auto &&e = obj_reader.Warning(); !e.empty()) { std::cout << e << std::endl; }

        auto &&p = obj_reader.GetAttrib().vertices;
        scene_data.vertices.reserve(p.size());
        for (std::uint32_t i = 0u; i < p.size(); ++i) { scene_data.vertices.emplace_back(p[i]); }
        std::cout << "Loaded mesh with " << obj_reader.GetShapes().size() << " shapes"
                << " and " << scene_data.vertices.size() << " vertices."
                << std::endl;
    }

    void run() {
        check_validation_layer_support();
        create_instance();

        pick_physical_device();
        create_logical_device();

        create_buffers();
        create_descriptor_pool();
        create_descriptor_set_layout();
        create_descriptor_set();
        // create_compute_pipeline();




        create_command_pool();
        create_command_buffer();


        // write_memory(uniform_buffer_memorys[0], output_image.data(), output_image.size());
        // write_memory(uniform_buffer_memorys[1], seed_image.data(), seed_image.size());


    }

    void check_validation_layer_support() {
        std::vector<vk::LayerProperties> available_layers = vk::enumerateInstanceLayerProperties();
        for (const char* layer_name : VALIDATION_LAYERS) {
            for (const vk::LayerProperties& layer_properties : available_layers) {
                if (strcmp(layer_name, layer_properties.layerName) == 0) {
                    validation_layers_supported = true;
                    minilog::log_debug("the {} is supported!", layer_name);
                    break;
                }
            }
            if (validation_layers_supported) { break; }
        }

        if (ENABLE_VALIDATION_LAYER && (!validation_layers_supported)) {
            minilog::log_fatal("validation layers requested, but not available!");
        }
    }

    void create_instance() {
        std::uint32_t support_vulkan_version = vk::enumerateInstanceVersion();
        auto version_major = vk::apiVersionMajor(support_vulkan_version);
        auto version_minor = vk::apiVersionMinor(support_vulkan_version);
        auto version_patch = vk::apiVersionPatch(support_vulkan_version);
        minilog::log_debug("vulkan version(vk::enumerateInstanceVersion): {}.{}.{}", version_major, version_minor, version_patch);

        vk::ApplicationInfo application_info {
            .pApplicationName = "hello compute shader",
            .applicationVersion = support_vulkan_version,
            .pEngineName = "No Engine",
            .engineVersion = support_vulkan_version,
            .apiVersion = support_vulkan_version
        };

        vk::InstanceCreateInfo instance_ci { .pApplicationInfo = &application_info };
        if (ENABLE_VALIDATION_LAYER) {
            instance_ci.enabledLayerCount = static_cast<std::uint32_t>(VALIDATION_LAYERS.size());
            instance_ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        } else {
            instance_ci.enabledLayerCount = 0u;
            instance_ci.ppEnabledLayerNames = nullptr;
        }
        instance_ci.enabledExtensionCount = static_cast<std::uint32_t>(INSTANCE_EXTENSIONS.size());
        instance_ci.ppEnabledExtensionNames = INSTANCE_EXTENSIONS.data();

        if (vk::createInstance(&instance_ci, nullptr, &instance) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::Instance!");
        }
    }

    void pick_physical_device() {
        std::vector<vk::PhysicalDevice> physical_devices = instance.enumeratePhysicalDevices();
        for (const vk::PhysicalDevice& device : physical_devices) {
            std::vector<vk::QueueFamilyProperties> queue_family_properties = device.getQueueFamilyProperties();
            for (std::size_t i { 0uz }; i < queue_family_properties.size(); ++i) {
                if (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eCompute) {
                    compute_queue_family_index = static_cast<std::uint32_t>(i);
                    physical_device = device;
                    break;
                }
            }
            if (compute_queue_family_index.has_value()) { break; }
        }

        if (physical_device == nullptr) { minilog::log_fatal("failed to find a suitable GPU!"); }
        minilog::log_debug("Select Queue Index: {}", compute_queue_family_index.value());

        vk::PhysicalDeviceProperties properties = physical_device.getProperties();
        minilog::log_debug(
            "maxComputeWorkGroupInvocations: {}",
            properties.limits.maxComputeWorkGroupInvocations
        );
        compute_shader_process_unit = std::sqrt(properties.limits.maxComputeWorkGroupInvocations);
    }

    void create_logical_device() {
        float queue_priority { 1.0f };
        vk::DeviceQueueCreateInfo device_queue_ci {
            .queueFamilyIndex = compute_queue_family_index.value(),
            .queueCount = 1u,
            .pQueuePriorities = &queue_priority
        };

        vk::DeviceCreateInfo device_ci {
            .queueCreateInfoCount = 1u,
            .pQueueCreateInfos = &device_queue_ci
        };
        if (ENABLE_VALIDATION_LAYER) {
            device_ci.enabledLayerCount = static_cast<std::uint32_t>(VALIDATION_LAYERS.size());
            device_ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        } else {
            device_ci.enabledLayerCount = 0u;
            device_ci.ppEnabledLayerNames = nullptr;
        }
        device_ci.enabledExtensionCount = 0u;
        device_ci.ppEnabledExtensionNames = nullptr;
        device_ci.pEnabledFeatures = nullptr;

        if (
            vk::Result result = physical_device.createDevice(&device_ci, nullptr, &logical_device);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create logical device!");
        }

        logical_device.getQueue(compute_queue_family_index.value(), 0, &compute_queue);
    }

    void create_buffers() {
        for (std::size_t i = 0uz; i < uniform_buffers.size(); ++i) {
            _create_buffer(
                1920u * 1080u * 4u,
                vk::BufferUsageFlagBits::eUniformBuffer,
                uniform_buffers[i],
                uniform_buffer_memorys[i]
            );
        }
        for (std::size_t i = 0uz; i < storage_buffers.size(); ++i) {
            _create_buffer(
                1920u * 1080u * 4u,
                vk::BufferUsageFlagBits::eStorageBuffer,
                storage_buffers[i],
                storage_buffer_memorys[i]
            );
        }
    }

    void create_descriptor_pool() {
        std::array<vk::DescriptorPoolSize, 2uz> descriptor_pool_sizes;
        {
            vk::DescriptorPoolSize descriptor_pool_size {
                .type = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1u
            };
            descriptor_pool_sizes[0] = descriptor_pool_size;
        }
        {
            vk::DescriptorPoolSize descriptor_pool_size {
                .type = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1u + 2u + 2u
            };
            descriptor_pool_sizes[1] = descriptor_pool_size;
        }

        vk::DescriptorPoolCreateInfo descriptor_pool_ci {
            .flags = vk::DescriptorPoolCreateFlags{},
            .maxSets = 2u,
            .poolSizeCount = static_cast<std::uint32_t>(descriptor_pool_sizes.size()),
            .pPoolSizes = descriptor_pool_sizes.data()
        };

        if (
            vk::Result result = logical_device.createDescriptorPool(&descriptor_pool_ci, nullptr, &descriptor_pool);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create descriptor pool!");
        }
    }

    void create_descriptor_set_layout() {
        {
            std::array<vk::DescriptorSetLayoutBinding, 2uz> descriptor_set_layout_bindings;
            for (std::size_t i { 0uz }; i < descriptor_set_layout_bindings.size(); ++i) {
                vk::DescriptorSetLayoutBinding descriptor_set_layout_binding {
                    .binding = static_cast<std::uint32_t>(i),
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = 1u,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                    .pImmutableSamplers = nullptr
                };
                descriptor_set_layout_bindings[i] = descriptor_set_layout_binding;
            }

            vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci {
                .flags = vk::DescriptorSetLayoutCreateFlags{},
                .bindingCount = static_cast<std::uint32_t>(descriptor_set_layout_bindings.size()),
                .pBindings = descriptor_set_layout_bindings.data()
            };

            if (
                vk::Result result = logical_device.createDescriptorSetLayout(
                    &descriptor_set_layout_ci, nullptr, &descriptor_set_layouts[0uz]
                );
                result != vk::Result::eSuccess
            ) {
                minilog::log_fatal("failed to create descriptor_set_layout!");
            }
        }
        {
            std::array<vk::DescriptorSetLayoutBinding, 5uz> descriptor_set_layout_bindings;
            for (std::size_t i { 0uz }; i < descriptor_set_layout_bindings.size(); ++i) {
                vk::DescriptorSetLayoutBinding descriptor_set_layout_binding {
                    .binding = static_cast<std::uint32_t>(i),
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1u,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                    .pImmutableSamplers = nullptr
                };
                descriptor_set_layout_bindings[i] = descriptor_set_layout_binding;
            }

            vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci {
                .flags = vk::DescriptorSetLayoutCreateFlags{},
                .bindingCount = static_cast<std::uint32_t>(descriptor_set_layout_bindings.size()),
                .pBindings = descriptor_set_layout_bindings.data()
            };

            if (
                vk::Result result = logical_device.createDescriptorSetLayout(
                    &descriptor_set_layout_ci, nullptr, &descriptor_set_layouts[1uz]
                );
                result != vk::Result::eSuccess
            ) {
                minilog::log_fatal("failed to create descriptor_set_layout!");
            }
        }
    }

    void create_descriptor_set() {
        vk::DescriptorSetAllocateInfo descriptor_set_ai {
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
            .pSetLayouts = descriptor_set_layouts.data()
        };
        if (
            vk::Result result = logical_device.allocateDescriptorSets(&descriptor_set_ai, descriptor_sets.data());
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::DescriptorSet");
        }

        // TODO
    }


    std::uint32_t _find_memory_type(
        const vk::MemoryRequirements& memory_requirements,
        vk::MemoryPropertyFlags memory_properties
    ) {
        vk::PhysicalDeviceMemoryProperties physical_device_memory_properties = physical_device.getMemoryProperties();
        for (std::size_t i { 0uz }; i < physical_device_memory_properties.memoryTypeCount; ++i) {
            if (
                memory_requirements.memoryTypeBits & (1 << i)
                && (physical_device_memory_properties.memoryTypes[i].propertyFlags & memory_properties) == memory_properties
            ) {
                minilog::log_debug("pick memory type [{}]", i);
                return i;
            }
        }

        return 0u;
    }

    void _create_buffer(
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::Buffer& buffer,
        vk::DeviceMemory& memory
    ) {
        vk::BufferCreateInfo buffer_ci {
            .flags = vk::BufferCreateFlags{},
            .size = size, // 1920u * 1080u * 4u
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 0u,
            .pQueueFamilyIndices = nullptr
        };
        if (
            vk::Result result = logical_device.createBuffer(&buffer_ci, nullptr, &buffer);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::buffer!");
        }

        vk::MemoryRequirements memory_requirements = logical_device.getBufferMemoryRequirements(buffer);
        vk::MemoryAllocateInfo memory_ai {
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = _find_memory_type(
                memory_requirements,
                vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent
            )
        };

        if (
            vk::Result result = logical_device.allocateMemory(&memory_ai, nullptr, &memory);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to allocate buffer memory!");
        }

        logical_device.bindBufferMemory(buffer, memory, 0u);
    }



    #if 0
    void read_memory(vk::DeviceMemory memory, void* dataBlock, vk::DeviceSize size) {
        void* data { nullptr };
        if (vkMapMemory(logical_device, memory, 0, size, 0, &data) != VK_SUCCESS) {
            throw std::runtime_error("failed to map memory");
        }
        memcpy(dataBlock, data, size);
        vkUnmapMemory(device, memory);
    }

    void write_memory(vk::DeviceMemory memory, void* dataBlock, vk::DeviceSize size) {
        void* data { nullptr };
        if (vkMapMemory(logical_device, memory, 0u, size, 0u, &data) != VK_SUCCESS) {
            throw std::runtime_error("failed to map memory");
        }
        memcpy(data, dataBlock, size);
        vkUnmapMemory(device, memory);
    }


    vk::ShaderModule create_shader_module(const std::vector<char>& code) {
        vk::ShaderModule shader_module;
        vk::ShaderModuleCreateInfo shader_module_ci {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const std::uint32_t*>(code.data())
        };
        if (
            vk::Result result = vkCreateShaderModule(logical_device, &shader_module_ci, nullptr, &shader_module);
            result != VK_SUCCESS
        ) {
            throw std::runtime_error("fail to create shader module");
        }

        return shader_module;
    }

    void create_compute_pipeline() {
        auto compute_shader_code = read_shader_file(MU_SHADER_PATH "./src/path_tracing/path_tracing_kernel.spv");
        auto compute_shader_module = create_shader_module(compute_shader_code);

        vk::SpecializationMapEntry specialization_map_entry {
            .constantID = 0,
            .offset = 0,
            .size = sizeof(uint32_t)
        };

        vk::SpecializationInfo specialization_ci {
            .mapEntryCount = 1u,
            .pMapEntries = &specialization_map_entry,
            .dataSize = sizeof(compute_shader_process_unit),
            .pData = &compute_shader_process_unit
        };

        vk::PipelineShaderStageCreateInfo pipeline_shader_stage_ci {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compute_shader_module,
            .pName = "main",
            .pSpecializationInfo = &specialization_ci
        };

        vk::PushConstantRange push_constant_range {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0u,
            .size = sizeof(PushConstantData)
        };

        vk::PipelineLayoutCreateInfo pipeline_layout_ci {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .setLayoutCount = descriptor_set_layouts.size(),
            .pSetLayouts = descriptor_set_layouts.data(),
            .pushConstantRangeCount = 1u;
            .pPushConstantRanges = &push_constant_range;
        };
        if (
            vk::Result result = vkCreatePipelineLayout(device, &pipeline_layout_ci, nullptr, &pipeline_layout)
            result != VK_SUCCESS
        ) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vk::ComputePipelineCreateInfo compute_pipeline_ci {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .stage = pipeline_shader_stage_ci,
            .layout = pipeline_layout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        if (
            vk::Result result = vkCreateComputePipelines(device, nullptr, 1u, &compute_pipeline_ci, nullptr, &compute_pipeline);
            result != VK_SUCCESS
        ) {
            throw std::runtime_error("failed to create compute pipeline");
        }

        vkDestroyShaderModule(logical_device, computeShaderModule, nullptr);
    }



#endif

    void create_command_pool() {
        vk::CommandPoolCreateInfo command_pool_ci {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = compute_queue_family_index.value()
        };

        if (
            vk::Result result = logical_device.createCommandPool(&command_pool_ci, nullptr, &command_pool);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create command pool!");
        }
    }

    void create_command_buffer() {
        vk::CommandBufferAllocateInfo command_buffer_ai {
            .commandPool = command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1u
        };
        if (
            vk::Result result = logical_device.allocateCommandBuffers(&command_buffer_ai, command_buffers.data());
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create command buffer!");
        }
    }
};





#if 0
vk::DebugUtilsMessengerCreateInfoEXT populate_debug_messenger_ci() {
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_ci {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0u,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = nullptr
    };

    return debug_utils_messenger_ci;
}

vk::Result createDebugUtilsMessengerEXT(
    vk::Instance instance,
    const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const vk::AllocationCallbacks* pAllocator,
    vk::DebugUtilsMessengerEXT* pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        throw std::runtime_error("can't find proc: vkCreateDebugUtilsMessengerEXT");
    }
}

void destoryDebugUtilsMessengerEXT(
    vk::Instance instance,
    vk::DebugUtilsMessengerEXT messenger,
    const vk::AllocationCallbacks* pAllocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func) {
        func(instance, messenger, pAllocator);
    } else {
        throw std::runtime_error("can't find proc");
    }
}

std::vector<vk::ExtensionProperties> query_instance_extension_properties() {
    std::uint32_t extension_count { 0u };
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    std::vector<vk::ExtensionProperties> extension_properties(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_properties.data());
    return extension_properties;
}

std::vector<vk::PhysicalDevice> query_physical_devices(vk::Instance instance) {
    std::uint32_t device_count { 0u };
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<vk::PhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());
    return physical_devices;
}

std::vector<vk::QueueFamilyProperties> query_physical_device_queue_family_properties(vk::PhysicalDevice physical_device) {
    std::uint32_t property_count { 0u };
    vkGetPhysicalDeviceQueueFamilyProperties(device, &property_count, nullptr);
    std::vector<vk::QueueFamilyProperties> queue_family_properties(property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &property_count, queue_family_properties.data());
    return queue_family_properties;
}

vk::MemoryRequirements query_memory_requirements(vk::Device device, vk::Buffer buffer) {
    vk::MemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);
    return memory_requirements;
}

vk::PhysicalDeviceMemoryProperties query_physical_device_memory_properties(vk::PhysicalDevice physical_device) {
    vk::PhysicalDeviceMemoryProperties physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);
    return physical_device_memory_properties;
}

std::vector<char> read_shader_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) { throw std::runtime_error("failed to open file!"); }

    std::size_t file_size = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}
#endif

int main() {
    minilog::set_log_level(minilog::log_level::trace); // default log level is 'info'
    // minilog::set_log_file("./mini.log"); // dump log to a specific file

    PathTracing path_tracing {};

    try {
        path_tracing.run();
    } catch (std::runtime_error e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}

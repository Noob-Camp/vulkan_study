#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <stb/stb_image_write.h>

#include <minilog.hpp>
#include <cornell_box.hpp>

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


struct Camera {
    float fov;
    glm::uvec2 resolution;
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
};


struct Triangle {
    std::uint32_t index0;
    std::uint32_t index1;
    std::uint32_t index2;
};


struct CornellBoxSceneData {
    Camera camera;
    std::vector<glm::vec4> output_image;
    std::vector<glm::vec4> seed_image;
    std::vector<glm::vec3> vertices;
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
    std::array<vk::DeviceMemory, 2uz> uniform_device_memorys;
    std::array<vk::Buffer, 5uz> storage_buffers;
    std::array <vk::DeviceMemory, 5uz> storage_device_memorys;

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
        logical_device.destroyCommandPool(command_pool);

        logical_device.destroyPipeline(compute_pipeline);
        logical_device.destroyPipelineLayout(pipeline_layout);

        for (vk::DescriptorSetLayout& descriptor_set_layout : descriptor_set_layouts) {
            logical_device.destroyDescriptorSetLayout(descriptor_set_layouts);
        }
        logical_device.destroyDescriptorPool(descriptor_pool, nullptr);

        for (vk::DeviceMemory& uniform_device_memory : uniform_device_memorys) {
            logical_device.freeMemory(uniform_device_memory);
        }
        for (vk::DeviceMemory& storage_device_memory : storage_device_memorys) {
            logical_device.freeMemory(storage_device_memory);
        }
        for (vk::Buffer& uniform_buffer : uniform_buffers) {
            logical_device.destroyBuffer(uniform_buffer);
        }
        for (vk::Buffer& storage_buffer : storage_buffers) {
            logical_device.destroyBuffer(storage_buffer);
        }

        logical_device.waitIdle();
        logical_device.destroy();

        vk::detail::DynamicLoader dynamic_loader;
        PFN_vkGetInstanceProcAddr
        getInstanceProcAddr = dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::detail::DispatchLoaderDynamic dispatch_loader_dynamic(instance, getInstanceProcAddr);
        instance.destroyDebugUtilsMessengerEXT(debug_utils_messenger, nullptr, dispatch_loader_dynamic);

        instance.destroy();

        minilog::log_debug("the compute shader programme is destruction.");
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
            minilog::log_fatal("Failed to load OBJ file: {}", error_message);
        }
        if (auto &&e = obj_reader.Warning(); !e.empty()) { minilog::log_fatal("{}", e); }

        // vertices
        auto &&p = obj_reader.GetAttrib().vertices;
        scene_data.vertices.reserve(p.size() / 3uz);
        for (std::size_t i { 0uz }; i < p.size(); ++i) {
            scene_data.vertices.emplace_back({ p[i], p[i + 1uz], p[i + 2uz] });
        }
        minilog::log_debug(
            "Loaded mesh with {} shape(s) and {} vertices.",
            obj_reader.GetShapes().size(), scene_data.vertices.size()
        );

        // mesh
        // for (auto &&shape : obj_reader.GetShapes()) {
        //     std::uint32_t index = static_cast<std::uint32_t>(meshes.size());
        //     std::vector<tinyobj::index_t> const &t = shape.mesh.indices;
        //     std::uint32_t triangle_count = t.size() / 3uz;
        //     minilog::log_debug(
        //         "Processing shape '{}' at index {} with {} triangle(s).",
        //         shape.name, index, triangle_count
        //     );

        //     std::vector<std::uint32_t> indices;
        //     indices.reserve(t.size());
        //     for (tinyobj::index_t i : t) { indices.emplace_back(i.vertex_index); }
        // }
    }

    void run() {
        check_validation_layer_support();
        create_instance();
        setup_debug_messenger();

        pick_physical_device();
        create_logical_device();

        create_buffers();
        create_descriptor_pool();
        create_descriptor_set_layout();
        create_descriptor_set();
        create_compute_pipeline();

        create_command_pool();
        create_command_buffer();

        execute();
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
        minilog::log_debug(
            "vulkan version(vk::enumerateInstanceVersion): {}.{}.{}",
            version_major, version_minor, version_patch
        );

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

    void setup_debug_messenger() {
        if (!ENABLE_VALIDATION_LAYER) { return ; }

        vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_ci {
            .flags = vk::DebugUtilsMessengerCreateFlagsEXT{},
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debug_callback),
            .pUserData = nullptr
        };

        vk::detail::DynamicLoader dynamic_loader;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr =
            dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::detail::DispatchLoaderDynamic dispatch_loader_dynamic(instance, getInstanceProcAddr);

        if (
            vk::Result result = instance.createDebugUtilsMessengerEXT(
                &debug_utils_messenger_ci, nullptr, &debug_utils_messenger, dispatch_loader_dynamic
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to set up debug messenger!");
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
                uniform_device_memorys[i]
            );
        }
        for (std::size_t i = 0uz; i < storage_buffers.size(); ++i) {
            _create_buffer(
                1920u * 1080u * 4u,
                vk::BufferUsageFlagBits::eStorageBuffer,
                storage_buffers[i],
                storage_device_memorys[i]
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

    void create_compute_pipeline() {
        std::vector<char> compute_shader_code = read_shader_file("./src/path_tracing/path_tracing_kernel.spv");
        vk::ShaderModule compute_shader_module = _create_shader_module(compute_shader_code);

        vk::SpecializationMapEntry specialization_map_entry {
            .constantID = 0u,
            .offset = 0u,
            .size = sizeof(uint32_t)
        };

        vk::SpecializationInfo specialization_info {
            .mapEntryCount = 1u,
            .pMapEntries = &specialization_map_entry,
            .dataSize = sizeof(compute_shader_process_unit),
            .pData = &compute_shader_process_unit
        };

        vk::PipelineShaderStageCreateInfo pipeline_shader_stage_ci {
            .flags = {},
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = compute_shader_module,
            .pName = "main",
            .pSpecializationInfo = &specialization_info
        };

        vk::PushConstantRange push_constant_range {
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0u,
            .size = sizeof(PushConstantData)
        };

        vk::PipelineLayoutCreateInfo pipeline_layout_ci {
            .flags = {},
            .setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
            .pSetLayouts = descriptor_set_layouts.data(),
            .pushConstantRangeCount = 1u;
            .pPushConstantRanges = &push_constant_range;
        };
        if (
            vk::Result result = logical_device.createPipelineLayout(&pipeline_layout_ci, nullptr, &pipeline_layout);
            result != vk::Result::eSuccess
        ) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vk::ComputePipelineCreateInfo compute_pipeline_ci {
            .flags = {},
            .stage = pipeline_shader_stage_ci,
            .layout = pipeline_layout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        if (
            vk::Result result = logical_device.createComputePipelines(
                nullptr, 1u, &compute_pipeline_ci, nullptr, &compute_pipeline
            );
            result != vk::Result::eSuccess
        ) {
            throw std::runtime_error("failed to create compute pipeline");
        }

        logical_device.destroyShaderModule(compute_shader_module, nullptr);
    }

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

    void execute() {
        vk::CommandBufferBeginInfo command_buffer_begin_info {
            .flags = {},
            .pInheritanceInfo = nullptr
        };
        if (
            vk::Result result = command_buffers[0].begin(&command_buffer_begin_info);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("command buffer failed to begin!");
        }

        command_buffers[0].bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline);
        command_buffers[0].bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            pipeline_layout,
            0u,
            1u,
            descriptor_sets.data(),
            0u,
            nullptr
        );
        command_buffers[0].dispatch(16u, 16u, 1u);
        command_buffers[0].end();

        vk::SubmitInfo submit_info {
            .waitSemaphoreCount = 0u,
            .commandBufferCount = 1u,
            .pCommandBuffers = &command_buffers[0uz],
            .signalSemaphoreCount = 0u
        };

        if (compute_queue.submit(1u, &submit_info, VK_NULL_HANDLE) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to submit command buffer!");
        }
        if (
            vk::Result result = compute_queue.waitIdle(); // wait the calculation to finish
            result == vk::Result::eSuccess
        ) {
            minilog::log_fatal("compute queue synchronization!");
        }

        // void* data = logical_device.mapMemory(storage_buffer_memory, 0u, sizeof(input_data), {});
        // memcpy(output_data.data(), data, sizeof(input_data));
        // logical_device.unmapMemory(storage_buffer_memory);
    }

private:
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

    vk::ShaderModule _create_shader_module(const std::vector<char>& code) {
        vk::ShaderModule shader_module;
        vk::ShaderModuleCreateInfo shader_module_ci {
            .flags = {},
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const std::uint32_t*>(code.data())
        };
        if (
            vk::Result result = vkCreateShaderModule(logical_device, &shader_module_ci, nullptr, &shader_module);
            result != vk::Result::eSuccess
        ) {
            throw std::runtime_error("fail to create shader module");
        }

        return shader_module;
    }

#if 0
    void read_memory(vk::DeviceMemory memory, void* dataBlock, vk::DeviceSize size) {
        void* data { nullptr };
        if (vkMapMemory(logical_device, memory, 0, size, 0, &data) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to map memory");
        }
        memcpy(dataBlock, data, size);
        vkUnmapMemory(device, memory);
    }

    void write_memory(vk::DeviceMemory memory, void* dataBlock, vk::DeviceSize size) {
        void* data { nullptr };
        if (vkMapMemory(logical_device, memory, 0u, size, 0u, &data) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to map memory");
        }
        memcpy(data, dataBlock, size);
        vkUnmapMemory(device, memory);
    }
#endif
};

#if 0
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
#endif

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

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include <minilog.hpp>

#include <array>
#include <vector>
#include <string>
#include <optional>
#include <iostream>


#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYER { false };
#else
    constexpr bool ENABLE_VALIDATION_LAYER { true };
#endif

const std::vector<const char*> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> INSTANCE_EXTENSIONS = { vk::EXTDebugUtilsExtensionName };


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


struct HelloComputeShader {
    vk::Instance instance { nullptr };
    bool validation_layers_supported { false };
    vk::DebugUtilsMessengerEXT debug_utils_messenger { nullptr };

    vk::PhysicalDevice physical_device { nullptr };
    vk::Device logical_device { nullptr };
    vk::Queue compute_queue;
    std::optional<std::uint32_t> compute_queue_family_index;

    vk::Buffer storage_buffer;
    vk::DeviceMemory storage_buffer_memory;

    vk::DescriptorPool descriptor_pool;
    vk::DescriptorSetLayout descriptor_set_layout;
    std::vector<vk::DescriptorSet> descriptor_sets;

    vk::PipelineLayout pipeline_layout;
    vk::Pipeline compute_pipeline;

    vk::CommandPool command_pool;
    std::array<vk::CommandBuffer, 1uz> command_buffers;

    std::array<float, 1024uz> input_data;
    std::array<float, 1024uz> output_data;


    HelloComputeShader() {
        input_data.fill(1.0f);
        output_data.fill(0.0f);
    }

    ~HelloComputeShader() {
        logical_device.destroyCommandPool(command_pool);
        logical_device.destroyPipeline(compute_pipeline);
        logical_device.destroyPipelineLayout(pipeline_layout);
        logical_device.destroyDescriptorSetLayout(descriptor_set_layout);
        logical_device.destroyDescriptorPool(descriptor_pool, nullptr);
        logical_device.freeMemory(storage_buffer_memory);
        logical_device.destroyBuffer(storage_buffer);

        logical_device.waitIdle();
        logical_device.destroy();

        vk::detail::DynamicLoader dynamic_loader;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr =
            dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::detail::DispatchLoaderDynamic dispatch_loader_dynamic(instance, getInstanceProcAddr);
        instance.destroyDebugUtilsMessengerEXT(debug_utils_messenger, nullptr, dispatch_loader_dynamic);

        instance.destroy();

        minilog::log_debug("the compute shader programme is destruction.");
    }

    void run() {
        check_validation_layer_support();
        create_instance();
        setup_debug_messenger();

        pick_physical_device();
        create_logical_device();

        create_storage_buffer();
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

    void create_storage_buffer() {
        vk::BufferCreateInfo buffer_ci {
            .size = sizeof(input_data),
            .usage = vk::BufferUsageFlagBits::eStorageBuffer,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 0u,
            .pQueueFamilyIndices = nullptr
        };

        if (
            vk::Result result = logical_device.createBuffer(&buffer_ci, nullptr, &storage_buffer);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::buffer!");
        }

        vk::MemoryRequirements memory_requirements = logical_device.getBufferMemoryRequirements(storage_buffer);
        vk::MemoryAllocateInfo memory_ai {
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = _find_memory_type(
                memory_requirements,
                vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent
            )
        };

        if (
            vk::Result result = logical_device.allocateMemory(&memory_ai, nullptr, &storage_buffer_memory);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to allocate buffer memory!");
        }

        logical_device.bindBufferMemory(storage_buffer, storage_buffer_memory, 0u);

        void* data { nullptr };
        if (
            vk::Result result = logical_device.mapMemory(
                storage_buffer_memory, 0u, sizeof(input_data), vk::MemoryMapFlags{}, &data
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to map memory!");
        }
        memcpy(data, input_data.data(), sizeof(input_data));
        logical_device.unmapMemory(storage_buffer_memory);
    }

    void create_descriptor_pool() {
        vk::DescriptorPoolSize descriptor_pool_size {
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1u
        };

        vk::DescriptorPoolCreateInfo descriptor_pool_ci {
            .flags = 0u,
            .maxSets = 1u,
            .poolSizeCount = 1u,
            .pPoolSizes = &descriptor_pool_size
        };

        if (
            vk::Result result = logical_device.createDescriptorPool(&descriptor_pool_ci, nullptr, &descriptor_pool);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create descriptor pool!");
        }
    }

    void create_descriptor_set_layout() {
        vk::DescriptorSetLayoutBinding descriptor_set_layout_binding {
            .binding = 0u,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1u,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .pImmutableSamplers = nullptr
        };

        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci {
            .bindingCount = 1u,
            .pBindings = &descriptor_set_layout_binding
        };

        if (
            vk::Result result = logical_device.createDescriptorSetLayout(
                &descriptor_set_layout_ci, nullptr, &descriptor_set_layout
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create descriptor_set_layout!");
        }
    }

    void create_descriptor_set() {
        vk::DescriptorSetAllocateInfo descriptor_set_ai {
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = 1u,
            .pSetLayouts = &descriptor_set_layout
        };
        descriptor_sets = logical_device.allocateDescriptorSets(descriptor_set_ai);

        vk::DescriptorBufferInfo descriptor_buffer_info {
            .buffer = storage_buffer,
            .offset = 0u,
            .range = sizeof(input_data)
        };

        vk::WriteDescriptorSet write_descriptor_set {
            .dstSet = descriptor_sets[0uz],
            .dstBinding = 0u,
            .dstArrayElement = 0u,
            .descriptorCount = 1u,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptor_buffer_info,
            .pTexelBufferView = nullptr
        };

        logical_device.updateDescriptorSets(1u, &write_descriptor_set, 0u, nullptr);
    }

    vk::ShaderModule create_shader_module(const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo shader_module_ci {
            .flags = 0u,
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const std::uint32_t*>(code.data())
        };

        vk::ShaderModule shader_module;
        if (
            vk::Result result = logical_device.createShaderModule(&shader_module_ci, nullptr, &shader_module);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::ShaderModule");
        }

        return shader_module;
    }

    static std::vector<char> read_shader_file(const std::string& fileName) {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);
        if (!file.is_open()) { minilog::log_fatal("failed to open file: {}", fileName); }

        std::size_t file_size = static_cast<std::size_t>(file.tellg());
        std::vector<char> buffer(file_size);
        file.seekg(0);
        file.read(buffer.data(), file_size);
        file.close();

        return buffer;
    }

    void create_compute_pipeline() {
        std::vector<char> compute_shader_code = read_shader_file("./src/hello_compute_shader/compute_shader.spv");
        vk::ShaderModule compute_shader_module = create_shader_module(compute_shader_code);

        vk::PipelineShaderStageCreateInfo pipeline_shader_stage_ci {
            .flags = 0u,
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = compute_shader_module,
            .pName = "main",
            .pSpecializationInfo = nullptr
        };

        vk::PipelineLayoutCreateInfo pipeline_layout_ci {
            .flags = 0u,
            .setLayoutCount = 1u,
            .pSetLayouts = &descriptor_set_layout,
            .pushConstantRangeCount = 0u,
            .pPushConstantRanges = nullptr
        };
        pipeline_layout = logical_device.createPipelineLayout(pipeline_layout_ci);

        vk::ComputePipelineCreateInfo compute_pipeline_ci {
            .flags = 0u,
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
            minilog::log_fatal("failed to create compute vk::Pipeline!");
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
        minilog::log_debug("input data:");
        for (std::size_t i { 0uz }; i < input_data.size(); ++i) {
            if (i % 64uz == 0uz && i != 0uz) { std::cout << '\n'; }
            std::cout << input_data[i];
        }
        std::cout << '\n';

        vk::CommandBufferBeginInfo command_buffer_begin_info {};
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
        command_buffers[0].dispatch(4u, 1u, 1u);
        command_buffers[0].end();

        vk::SubmitInfo submit_info {
            .waitSemaphoreCount = 0u,
            .commandBufferCount = 1u,
            .pCommandBuffers = &command_buffers[0],
            .signalSemaphoreCount = 0u
        };

        if (compute_queue.submit(1u, &submit_info, VK_NULL_HANDLE) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to submit command buffer!");
        }
        compute_queue.waitIdle(); // wait the calculation to finish

        void* data = logical_device.mapMemory(storage_buffer_memory, 0u, sizeof(input_data), {});
        memcpy(output_data.data(), data, sizeof(input_data));
        logical_device.unmapMemory(storage_buffer_memory);

        minilog::log_debug("output data:");
        for (std::size_t i { 0uz }; i < output_data.size(); ++i) {
            if (i % 64uz == 0uz && i != 0uz) { std::cout << '\n'; }
            std::cout << output_data[i];
        }
        std::cout << '\n';
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
};


int main() {
    minilog::set_log_level(minilog::log_level::trace); // default log level is 'info'
    // minilog::set_log_file("./mini.log"); // dump log to a specific file

    HelloComputeShader app {};

    try {
        app.run();
    } catch (std::runtime_error e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}

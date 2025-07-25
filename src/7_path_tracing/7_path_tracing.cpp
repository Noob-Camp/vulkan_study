#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <minilog.hpp>

#include <cstdint>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <optional>
#include <set>
#include <cstddef> // offsetof
#include <random>

#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYER { false };
#else
    constexpr bool ENABLE_VALIDATION_LAYER { true };
#endif

using namespace std::literals::string_literals;


const std::vector<const char*> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" };
// const std::vector<const char*> INSTANCE_EXTENSIONS = { vk::EXTDebugUtilsExtensionName }; // TODO
const std::vector<const char*> DEVICE_EXTENSIONS = { vk::KHRSwapchainExtensionName };

constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT { 2u };
constexpr std::uint32_t PARTICLE_COUNT { 1 };


VKAPI_ATTR vk::Bool32 VKAPI_CALL
debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageType,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    switch (messageSeverity) {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose: {
            // minilog::log_trace("Vulkan Validation Layer [verbose]: {}", pCallbackData->pMessage);
            return vk::False;
        }
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo: {
            // minilog::log_trace("Vulkan Validation Layer [info]: {}", pCallbackData->pMessage);
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


std::vector<char> read_shader_file(const std::string& fileName) {
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);
    if (!file.is_open()) { minilog::log_fatal("Failed to open file: {}", fileName); }

    std::size_t file_size = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}


struct UniformBufferObject {
    std::uint32_t sample_index { 0u };
};


struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 uv;

    bool operator==(const Vertex& other) const {
        return position == other.position
            && color == other.color
            && uv == other.uv;
    }
};
namespace std {
template<> struct hash<Vertex> {
    std::size_t operator()(const Vertex& vertex) const {
        auto x = hash<glm::vec3>()(vertex.position);
        auto y = hash<glm::vec3>()(vertex.color) << 1;
        auto z = hash<glm::vec2>()(vertex.uv) << 1;
        return ((x ^ y) >> 1) ^ z;
    }
};
} // namespace std end


struct Triangle {
    std::uint32_t t0;
    std::uint32_t t1;
    std::uint32_t t2;
};


struct QueueFamilyIndex {
    std::optional<std::uint32_t> graphic_and_compute;
    std::optional<std::uint32_t> present;

    bool has_value() {
        return graphic_and_compute.has_value()
            && present.has_value();
    }
};


struct SwapChainSupportDetail {
    vk::SurfaceCapabilitiesKHR surface_capabilities;
    std::vector<vk::SurfaceFormatKHR> surface_formats;
    std::vector<vk::PresentModeKHR> present_modes;
};


class PathTracing {
private:
    std::uint32_t width;
    std::uint32_t height;
    std::string window_name;
    GLFWwindow* glfw_window { nullptr };

    vk::Instance instance;
    bool validation_layers_supported { false };
    vk::DebugUtilsMessengerEXT debug_utils_messenger;

    vk::PhysicalDevice physical_device;
    vk::Device logical_device;
    vk::Queue graphic_queue;
    vk::Queue compute_queue;
    vk::Queue present_queue;

    vk::CommandPool command_pool;
    std::vector<vk::CommandBuffer> render_command_buffers;
    std::vector<vk::CommandBuffer> compute_command_buffers;

    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;
    vk::Format swapchain_image_format;
    vk::Extent2D swapchain_extent;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_imageviews;
    std::vector<vk::Framebuffer> frame_buffers;

    UniformBufferObject ubo;
    std::vector<vk::Buffer> uniform_buffers;
    std::vector<vk::DeviceMemory> uniform_device_memorys;
    std::vector<void*> uniform_buffers_mapped;
    std::vector<Vertex> vertices;
    std::vector<Triangle> indices;
    std::vector<vk::Buffer> storage_buffers;
    std::vector<vk::DeviceMemory> storage_device_memorys;

    vk::DescriptorSetLayout compute_descriptor_set_layout;
    vk::PipelineLayout compute_pipeline_layout;
    vk::Pipeline compute_pipeline;

    vk::RenderPass render_pass;
    vk::DescriptorSetLayout render_descriptor_set_layout;
    vk::PipelineLayout render_pipeline_layout;
    vk::Pipeline render_pipeline;

    vk::DescriptorPool descriptor_pool;
    std::vector<vk::DescriptorSet> compute_descriptor_sets;
    std::vector<vk::DescriptorSet> render_descriptor_sets;

    std::vector<vk::Semaphore> image_available_semaphores;
    std::vector<vk::Semaphore> render_finished_semaphores;
    std::vector<vk::Semaphore> compute_finished_semaphores;
    std::vector<vk::Fence> render_in_flight_fences;
    std::vector<vk::Fence> compute_in_flight_fences;
    std::uint32_t current_frame { 0u };

    bool framebuffer_resized { false };
    float last_frame_time { 0.0f };
    double last_time { 0.0 };

public:
    PathTracing()
        : width { 1920u }
        , height { 1080u }
        , window_name { "7_path_tracing"s }
    {
        ubo.sample_index = 0u;
    };

    PathTracing(
        std::uint32_t _width,
        std::uint32_t _height,
        const std::string& _window_name
    )
        : width { _width }
        , height { _height }
        , window_name { _window_name }
    {
        ubo.sample_index = 0u;
    }

    ~PathTracing() {
        cleanup_swapchain();
        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            logical_device.destroy(render_finished_semaphores[i]);
            logical_device.destroy(image_available_semaphores[i]);
            logical_device.destroy(compute_finished_semaphores[i]);
            logical_device.destroy(render_in_flight_fences[i]);
            logical_device.destroy(compute_in_flight_fences[i]);
        }
        logical_device.destroy(descriptor_pool);
        logical_device.destroy(render_pipeline);;
        logical_device.destroy(render_pipeline_layout);
        logical_device.destroy(render_pass);
        logical_device.destroy(compute_pipeline);
        logical_device.destroy(compute_pipeline_layout);
        logical_device.destroy(compute_descriptor_set_layout);
        logical_device.destroy(render_descriptor_set_layout);

        for (std::size_t i { 0uz }; i < uniform_buffers.size(); ++i) {
            logical_device.destroy(uniform_buffers[i]);
            logical_device.freeMemory(uniform_device_memorys[i]);
        }
        for (std::size_t i { 0uz }; i < storage_buffers.size(); ++i) {
            logical_device.destroy(storage_buffers[i]);
            logical_device.freeMemory(storage_device_memorys[i]);
        }
        logical_device.destroy(command_pool);

        // logical_device.waitIdle();
        logical_device.destroy();

        if (ENABLE_VALIDATION_LAYER) {
            vk::detail::DynamicLoader dynamic_loader;
            PFN_vkGetInstanceProcAddr getInstanceProcAddr =
                dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
            vk::detail::DispatchLoaderDynamic dispatch_loader_dynamic(instance, getInstanceProcAddr);
            instance.destroyDebugUtilsMessengerEXT(debug_utils_messenger, nullptr, dispatch_loader_dynamic);
        }
        instance.destroy(surface);
        instance.destroy();

        glfwDestroyWindow(glfw_window);
        glfwTerminate();
    }

    void run() {
        init_window();
        init_vulkan();
        render_loop();
    }

private:
    void init_window() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        glfw_window = glfwCreateWindow(width, height, window_name.c_str(), nullptr, nullptr);
        if (!glfw_window) { minilog::log_fatal("Failed to create GLFWwindow!"); }

        glfwSetWindowUserPointer(glfw_window, this);
        glfwSetFramebufferSizeCallback(
            glfw_window,
            [](GLFWwindow* window, int width, int height) {
                auto app = reinterpret_cast<PathTracing*>(glfwGetWindowUserPointer(window));
                app->framebuffer_resized = true;
                minilog::log_info("the window's size is ({0}, {1})", width, height);
            }
        );

        last_time = glfwGetTime();
    }

    void init_vulkan() {
        check_validation_layer_support();
        create_instance();
        setup_debug_messenger();

        create_surface();
        pick_physical_device();
        create_logical_device();

        create_command_pool();
        allocate_render_command_buffers();
        allocate_compute_command_buffers();

        create_swapchain();
        create_swapchain_imageviews();

        create_uniform_buffers();
        load_obj_model();
        create_storage_buffers();

        create_compute_descriptor_set_layout();
        create_compute_pipeline();

        create_render_pass();
        create_render_descriptor_set_layout();
        create_graphic_pipeline();

        create_frame_buffers();

        create_descriptor_pool();
        create_compute_descriptor_sets();
        create_render_descriptor_sets();

        create_sync_objects();
    }

    void render_loop() {
        while (!glfwWindowShouldClose(glfw_window)) {
            glfwPollEvents();
            draw_frame();

            // F11: reset sample_index
            if (glfwGetKey(glfw_window, GLFW_KEY_F11) != GLFW_RELEASE) {
                ubo.sample_index = 0u;
                minilog::log_debug("the ubo.sample_index is reset to 0u");
            }

            // F12: screenshot
            if (glfwGetKey(glfw_window, GLFW_KEY_F12) != GLFW_RELEASE) {
                std::vector<float> output_data(width * height * 4uz, 0.0f);

                vk::DeviceSize device_size = width * height * 4u * 4u;
                vk::Buffer staging_buffer;
                vk::DeviceMemory staging_device_memory;
                create_buffer(
                    device_size,
                    vk::BufferUsageFlagBits::eTransferDst,
                    vk::MemoryPropertyFlagBits::eHostVisible
                    | vk::MemoryPropertyFlagBits::eHostCoherent,
                    staging_buffer,
                    staging_device_memory
                );

                copy_buffer(storage_buffers[2uz], staging_buffer, device_size);

                void* data = logical_device.mapMemory(staging_device_memory, 0u, device_size, {});
                memcpy(output_data.data(), data, static_cast<std::size_t>(device_size));
                logical_device.unmapMemory(staging_device_memory);

                logical_device.destroy(staging_buffer);
                logical_device.freeMemory(staging_device_memory);

                std::vector<unsigned char> image_data_uchar(width * height * 4u);
                for (size_t i = 0; i < output_data.size(); ++i) {
                    float value = output_data[i];
                    value = std::fmax(0.0f, std::fmin(1.0f, value)); // clamp [0.0, 1.0]
                    image_data_uchar[i] = static_cast<unsigned char>(std::round(value * 255.0f)); // [0, 255]
                }

                const char* file_name = "output_image.png";
                int stride_in_bytes = width * 4; // bytes/row
                if (
                    int result = stbi_write_png(file_name, width, height, 4, image_data_uchar.data(), stride_in_bytes);
                    result == 1
                ) {
                    minilog::log_debug("write {} successful!", file_name);
                }
            }

            // We want to animate the particle system using the last frames time to get smooth,
            //   frame-rate independent animation
            double current_time = glfwGetTime();
            last_frame_time = (current_time - last_time) * 1000.0;
            last_time = current_time;
        }
        logical_device.waitIdle();
    }

    void check_validation_layer_support() {
        std::vector<vk::LayerProperties> available_layers = vk::enumerateInstanceLayerProperties();
        for (const char* layer_name : VALIDATION_LAYERS) {
            for (const auto& layer_properties : available_layers) {
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
            .pNext = nullptr,
            .pApplicationName = "ReCreate the Swap Chain",
            .applicationVersion = support_vulkan_version,
            .pEngineName = "No Engine",
            .engineVersion = support_vulkan_version,
            .apiVersion = support_vulkan_version
        };

        auto instance_extensions = get_required_extensions();
        vk::InstanceCreateInfo instance_ci {
            .pNext = nullptr,
            .flags = {},
            .pApplicationInfo = &application_info,
            .enabledLayerCount = 0u,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<std::uint32_t>(instance_extensions.size()),
            .ppEnabledExtensionNames = instance_extensions.data()
        };
        if (ENABLE_VALIDATION_LAYER) {
            vk::DebugUtilsMessengerCreateInfoEXT
            debug_utils_messenger_ci = create_debug_messenger_ci();
            instance_ci.pNext = (vk::DebugUtilsMessengerCreateInfoEXT*)&debug_utils_messenger_ci;
            instance_ci.enabledLayerCount = static_cast<std::uint32_t>(VALIDATION_LAYERS.size());
            instance_ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }

        if (
            vk::Result result = vk::createInstance(&instance_ci, nullptr, &instance);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::Instance!");
        }
    }

    void setup_debug_messenger() {
        if (!ENABLE_VALIDATION_LAYER) { return ; }

        vk::DebugUtilsMessengerCreateInfoEXT
        debug_utils_messenger_ci = create_debug_messenger_ci();

        vk::detail::DynamicLoader dynamic_loader;
        PFN_vkGetInstanceProcAddr
        getInstanceProcAddr = dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::detail::DispatchLoaderDynamic dispatch_loader_dynamic(instance, getInstanceProcAddr);

        if (
            vk::Result result = instance.createDebugUtilsMessengerEXT(
                &debug_utils_messenger_ci, nullptr, &debug_utils_messenger, dispatch_loader_dynamic
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to set up debug messenger!");
        }
    }

    void create_surface() {
        VkSurfaceKHR _surface;
        if (
            VkResult result = glfwCreateWindowSurface(instance, glfw_window, nullptr, &_surface);
            result == VK_SUCCESS
        ) {
            surface = vk::SurfaceKHR(_surface);
        } else {
            minilog::log_fatal("Failed to create vk::SurfaceKHR: {}!", static_cast<std::int32_t>(result));
        }
    }

    void pick_physical_device() {
        std::vector<vk::PhysicalDevice> physical_devices = instance.enumeratePhysicalDevices();
        for (const auto& device : physical_devices) {
            if (is_physical_device_suitable(device)) {
                physical_device = device;
                break;
            }
        }

        if (!physical_device) { minilog::log_fatal("Failed to find a suitable physical GPU!"); }
    }

    void create_logical_device() {
        QueueFamilyIndex queue_family_index = find_queue_families(physical_device);
        std::set<std::uint32_t> unique_queue_families = {
            queue_family_index.graphic_and_compute.value(),
            queue_family_index.present.value()
        };

        float queue_priority { 1.0f }; // default
        std::vector<vk::DeviceQueueCreateInfo> device_queue_cis;
        for (std::uint32_t queue_family : unique_queue_families) {
            vk::DeviceQueueCreateInfo device_queue_ci {
                .queueFamilyIndex = queue_family,
                .queueCount = 1u,
                .pQueuePriorities = &queue_priority
            };
            device_queue_cis.push_back(device_queue_ci);
        }

        vk::PhysicalDeviceFeatures physical_device_features {
            .samplerAnisotropy = vk::True
        };
        vk::DeviceCreateInfo device_ci {
            .pNext = nullptr,
            .flags = {}, // flags is reserved for future use
            .queueCreateInfoCount = static_cast<std::uint32_t>(device_queue_cis.size()),
            .pQueueCreateInfos = device_queue_cis.data(),
            .enabledLayerCount = 0u, // deprecated
            .ppEnabledLayerNames = nullptr, // deprecated
            .enabledExtensionCount = static_cast<std::uint32_t>(DEVICE_EXTENSIONS.size()),
            .ppEnabledExtensionNames = DEVICE_EXTENSIONS.data(),
            .pEnabledFeatures = &physical_device_features
        };
        if (ENABLE_VALIDATION_LAYER) {
            device_ci.enabledLayerCount = static_cast<std::uint32_t>(VALIDATION_LAYERS.size());
            device_ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }

        if (
            vk::Result result = physical_device.createDevice(&device_ci, nullptr, &logical_device);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create logical device!");
        }

        logical_device.getQueue(queue_family_index.graphic_and_compute.value(), 0u, &graphic_queue);
        logical_device.getQueue(queue_family_index.graphic_and_compute.value(), 0u, &compute_queue);
        logical_device.getQueue(queue_family_index.present.value(), 0u, &present_queue);
    }

    void create_command_pool() {
        QueueFamilyIndex queue_family_index = find_queue_families(physical_device);
        vk::CommandPoolCreateInfo command_pool_ci {
            .pNext = nullptr,
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queue_family_index.graphic_and_compute.value()
        };
        if (
            vk::Result result = logical_device.createCommandPool(&command_pool_ci, nullptr, &command_pool);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::CommandPool!");
        }
    }

    void allocate_render_command_buffers() {
        render_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        vk::CommandBufferAllocateInfo command_buffer_ai {
            .pNext = nullptr,
            .commandPool = command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<std::uint32_t>(render_command_buffers.size())
        };
        if (
            vk::Result result = logical_device.allocateCommandBuffers(&command_buffer_ai, render_command_buffers.data());
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to allocate command buffers!");
        }
    }

    void allocate_compute_command_buffers() {
        compute_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        vk::CommandBufferAllocateInfo allocInfo {
            .pNext = nullptr,
            .commandPool = command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<std::uint32_t>(compute_command_buffers.size())
        };
        if (
            vk::Result result = logical_device.allocateCommandBuffers(&allocInfo, compute_command_buffers.data());
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to allocate compute command buffers!");
        }
    }

    void create_swapchain() {
        SwapChainSupportDetail swapchain_support_detail = query_swapchain_support_detail(physical_device);
        vk::SurfaceFormatKHR surface_format = choose_swapchain_surface_format(swapchain_support_detail.surface_formats);
        vk::PresentModeKHR present_mode = choose_swapchain_present_mode(swapchain_support_detail.present_modes);
        vk::Extent2D extent = choose_swapchain_extent(swapchain_support_detail.surface_capabilities);

        // realization of triple buffer
        std::uint32_t image_count = std::min(
            swapchain_support_detail.surface_capabilities.minImageCount + 1u,
            swapchain_support_detail.surface_capabilities.maxImageCount
        );

        vk::SwapchainCreateInfoKHR swapchain_ci {
            .pNext = nullptr,
            .flags = {},
            .surface = surface,
            .minImageCount = image_count,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1u,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 0u,
            .pQueueFamilyIndices = nullptr,
            .preTransform = swapchain_support_detail.surface_capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = present_mode,
            .clipped = vk::True,
            .oldSwapchain = nullptr
        };
        QueueFamilyIndex queue_family_index = find_queue_families(physical_device);
        std::array<std::uint32_t, 2uz> queue_family_indices {
            queue_family_index.graphic_and_compute.value(),
            queue_family_index.present.value()
        };
        if (queue_family_index.graphic_and_compute != queue_family_index.present) {
            swapchain_ci.imageSharingMode = vk::SharingMode::eConcurrent;
            swapchain_ci.queueFamilyIndexCount = 2u;
            swapchain_ci.pQueueFamilyIndices = queue_family_indices.data();
        }

        if (
            vk::Result result = logical_device.createSwapchainKHR(&swapchain_ci, nullptr, &swapchain);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::SwapchainCreateInfoKHR!");
        }

        swapchain_images = logical_device.getSwapchainImagesKHR(swapchain);
        swapchain_image_format = swapchain_ci.imageFormat;
        swapchain_extent = swapchain_ci.imageExtent;
    }

    void create_swapchain_imageviews() {
        swapchain_imageviews.resize(swapchain_images.size());
        for (std::size_t i { 0uz }; i < swapchain_images.size(); ++i) {
            swapchain_imageviews[i] = create_imageview(
                swapchain_images[i],
                swapchain_image_format,
                vk::ImageAspectFlagBits::eColor,
                1u
            );
        }
    }

    void create_uniform_buffers() {
        uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_device_memorys.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);
        vk::DeviceSize device_size = sizeof(UniformBufferObject);
        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            create_buffer(
                device_size,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent,
                uniform_buffers[i],
                uniform_device_memorys[i]
            );
            uniform_buffers_mapped[i] = logical_device.mapMemory(uniform_device_memorys[i], 0u, device_size, {});
        }
    }

    void load_obj_model() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn { ""s };
        std::string err { ""s };
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "./resource/cornell_box.obj", "./resource/cornell_box.mtl")) {
            minilog::log_fatal("{}", warn + err);
        }

        std::vector<std::uint32_t> flat_indices;
        std::unordered_map<Vertex, std::uint32_t> unique_vertices {};
        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex {
                    .position = {
                        attrib.vertices[static_cast<std::size_t>(3 * index.vertex_index + 0)],
                        attrib.vertices[static_cast<std::size_t>(3 * index.vertex_index + 1)],
                        attrib.vertices[static_cast<std::size_t>(3 * index.vertex_index + 2)]
                    },
                    .color = { 1.0f, 1.0f, 1.0f },
                    .uv = {
                        // attrib.texcoords[static_cast<std::size_t>(2 * index.texcoord_index)],
                        // 1.0f - attrib.texcoords[static_cast<std::size_t>(2 * index.texcoord_index + 1)]
                        0.0f, 0.0f // TODO: use texture
                    }
                };
                if (unique_vertices.count(vertex) == 0) {
                    unique_vertices[vertex] = static_cast<std::uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                flat_indices.push_back(unique_vertices[vertex]);
            }
        }

        indices.resize(flat_indices.size() / 3uz);
        for (std::size_t i { 0uz }; i < indices.size(); ++i) {
            indices[i] = Triangle {
                flat_indices[3uz * i + 0uz],
                flat_indices[3uz * i + 1uz],
                flat_indices[3uz * i + 2uz]
            };
        }
    }

    void create_storage_buffers() {
        // storage_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        // storage_device_memorys.resize(MAX_FRAMES_IN_FLIGHT);
        storage_buffers.resize(4uz);
        storage_device_memorys.resize(4uz);

        create_vertex_buffer();
        create_index_buffer();
        create_output_buffer();
        create_seed_buffer();
    }

    void create_vertex_buffer() {
        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_device_memory;
        vk::DeviceSize vertex_device_size = sizeof(vertices[0uz]) * vertices.size();
        create_buffer(
            vertex_device_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent,
            staging_buffer,
            staging_device_memory
        );
        void* data = logical_device.mapMemory(staging_device_memory, 0u, vertex_device_size, {});
        memcpy(data, vertices.data(), static_cast<std::size_t>(vertex_device_size));
        logical_device.unmapMemory(staging_device_memory);

        create_buffer(
            vertex_device_size,
            vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            storage_buffers[0uz],
            storage_device_memorys[0uz]
        );
        copy_buffer(staging_buffer, storage_buffers[0uz], vertex_device_size);

        logical_device.destroy(staging_buffer);
        logical_device.freeMemory(staging_device_memory);
    }

    void create_index_buffer() {
        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_device_memory;
        vk::DeviceSize index_device_size = sizeof(indices[0uz]) * indices.size();
        create_buffer(
            index_device_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent,
            staging_buffer,
            staging_device_memory
        );
        void* data = logical_device.mapMemory(staging_device_memory, 0u, index_device_size, {});
        memcpy(data, indices.data(), static_cast<std::size_t>(index_device_size));
        logical_device.unmapMemory(staging_device_memory);

        create_buffer(
            index_device_size,
            vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            storage_buffers[1uz],
            storage_device_memorys[1uz]
        );
        copy_buffer(staging_buffer, storage_buffers[1uz], index_device_size);

        logical_device.destroy(staging_buffer);
        logical_device.freeMemory(staging_device_memory);
    }

    void create_output_buffer() {
        vk::DeviceSize device_size = width * height * 4u * 4u;
        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_device_memory;
        create_buffer(
            device_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent,
            staging_buffer,
            staging_device_memory
        );

        std::vector<float> test(width * height * 4u, 0.0f);
        void* data = logical_device.mapMemory(staging_device_memory, 0u, device_size);
        memcpy(data, test.data(), static_cast<std::size_t>(device_size));
        logical_device.unmapMemory(staging_device_memory);

        create_buffer(
            device_size,
            vk::BufferUsageFlagBits::eTransferSrc
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            storage_buffers[2uz],
            storage_device_memorys[2uz]
        );
        copy_buffer(staging_buffer, storage_buffers[2uz], device_size);

        logical_device.destroy(staging_buffer);
        logical_device.freeMemory(staging_device_memory);
    }

    void create_seed_buffer() {
        vk::DeviceSize device_size = width * height * 4u;
        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_device_memory;
        create_buffer(
            device_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent,
            staging_buffer,
            staging_device_memory
        );

        std::vector<std::uint32_t> test(width * height, 0u);
        void* data = logical_device.mapMemory(staging_device_memory, 0u, device_size);
        memcpy(data, test.data(), static_cast<std::size_t>(device_size));
        logical_device.unmapMemory(staging_device_memory);

        create_buffer(
            device_size,
            vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal
            | vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent,
            storage_buffers[3uz],
            storage_device_memorys[3uz]
        );
        copy_buffer(staging_buffer, storage_buffers[3uz], device_size);

        logical_device.destroy(staging_buffer);
        logical_device.freeMemory(staging_device_memory);
    }

    void create_compute_descriptor_set_layout() {
        std::vector<vk::DescriptorSetLayoutBinding> descriptor_set_layout_bindings = {
            vk::DescriptorSetLayoutBinding { // ubo
                .binding = 0u,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1u,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { // vertex buffer
                .binding = 1u,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1u,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { // index buffer
                .binding = 2u,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1u,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { // pixel colors
                .binding = 3u,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1u,
                .stageFlags = vk::ShaderStageFlagBits::eCompute
                    | vk::ShaderStageFlagBits::eFragment,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { // seed buffer
                .binding = 4u,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1u,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            }
        };
        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci {
            .pNext = nullptr,
            .flags = {},
            .bindingCount = static_cast<std::uint32_t>(descriptor_set_layout_bindings.size()),
            .pBindings = descriptor_set_layout_bindings.data()
        };
        if (
            vk::Result result = logical_device.createDescriptorSetLayout(
                &descriptor_set_layout_ci, nullptr, &compute_descriptor_set_layout
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::DescriptorSetLayout!");
        }
    }

    void create_compute_pipeline() {
        std::vector<char> comp_code = read_shader_file("./src/7_path_tracing/shaders/7_path_tracing_comp.spv");
        vk::ShaderModule comp_shader_module = create_shader_module(comp_code);
        vk::PipelineShaderStageCreateInfo comp_pipeline_shader_stage_ci {
            .pNext = nullptr,
            .flags = {},
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = comp_shader_module,
            .pName = "main",
            .pSpecializationInfo = nullptr
        };

        std::vector<vk::DescriptorSetLayout>
        descriptor_set_layouts = { compute_descriptor_set_layout };
        vk::PipelineLayoutCreateInfo pipeline_layout_ci {
            .pNext = nullptr,
            .flags = {},
            .setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
            .pSetLayouts = descriptor_set_layouts.data(),
            .pushConstantRangeCount = 0u,
            .pPushConstantRanges = nullptr
        };
        if (
            vk::Result result = logical_device.createPipelineLayout(
                &pipeline_layout_ci, nullptr, &compute_pipeline_layout
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::PipelineLayout!");
        }

        vk::ComputePipelineCreateInfo compute_pipeline_ci {
            .pNext = nullptr,
            .flags = {},
            .stage = comp_pipeline_shader_stage_ci,
            .layout = compute_pipeline_layout,
            .basePipelineHandle = nullptr,
            .basePipelineIndex = 0
        };
        if (
            vk::Result result = logical_device.createComputePipelines(
                nullptr, 1u, &compute_pipeline_ci, nullptr, &compute_pipeline
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create compute pipeline!");
        }

        logical_device.destroyShaderModule(comp_shader_module, nullptr);
    }

    void create_render_pass() {
        vk::AttachmentDescription attachment_desc_color {
            .flags = {},
            .format = swapchain_image_format,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR
        };

        vk::AttachmentReference attachment_ref_color {
            .attachment = 0u,
            .layout = vk::ImageLayout::eColorAttachmentOptimal
        };

        vk::SubpassDescription subpass_desc {
            .flags = {},
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0u,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1u,
            .pColorAttachments = &attachment_ref_color,
            .pResolveAttachments = nullptr,
            .pDepthStencilAttachment = nullptr,
            .preserveAttachmentCount = 0u,
            .pPreserveAttachments = nullptr
        };

        vk::SubpassDependency subpass_dependency {
            .srcSubpass = vk::SubpassExternal,
            .dstSubpass = 0u,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .srcAccessMask = {},
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dependencyFlags = {}
        };

        vk::RenderPassCreateInfo render_pass_ci {
            .pNext = nullptr,
            .flags = {},
            .attachmentCount = 1u,
            .pAttachments = &attachment_desc_color,
            .subpassCount = 1u,
            .pSubpasses = &subpass_desc,
            .dependencyCount = 1u,
            .pDependencies = &subpass_dependency
        };
        if (
            vk::Result result = logical_device.createRenderPass(&render_pass_ci, nullptr, &render_pass);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::RenderPass!");
        }
    }

    void create_render_descriptor_set_layout() {
        std::vector<vk::DescriptorSetLayoutBinding> descriptor_set_layout_bindings = {
            vk::DescriptorSetLayoutBinding { // pixel colors
                .binding = 0u,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1u,
                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                .pImmutableSamplers = nullptr
            }
        };
        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci {
            .pNext = nullptr,
            .flags = {},
            .bindingCount = static_cast<std::uint32_t>(descriptor_set_layout_bindings.size()),
            .pBindings = descriptor_set_layout_bindings.data()
        };
        if (
            vk::Result result = logical_device.createDescriptorSetLayout(
                &descriptor_set_layout_ci, nullptr, &render_descriptor_set_layout
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::DescriptorSetLayout!");
        }
    }

    void create_graphic_pipeline() {
        // Create graphic pipeline layout
        std::vector<char> vert_code = read_shader_file("./src/7_path_tracing/shaders/7_path_tracing_vert.spv");
        std::vector<char> frag_code = read_shader_file("./src/7_path_tracing/shaders/7_path_tracing_frag.spv");
        vk::ShaderModule vert_shader_module = create_shader_module(vert_code);
        vk::ShaderModule frag_shader_module = create_shader_module(frag_code);
        vk::PipelineShaderStageCreateInfo vert_pipeline_shader_stage_ci {
            .pNext = nullptr,
            .flags = {},
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vert_shader_module,
            .pName = "main",
            .pSpecializationInfo = nullptr
        };
        vk::PipelineShaderStageCreateInfo frag_pipeline_shader_stage_ci {
            .pNext = nullptr,
            .flags = {},
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = frag_shader_module,
            .pName = "main",
            .pSpecializationInfo = nullptr
        };
        std::array<vk::PipelineShaderStageCreateInfo, 2uz> pipeline_shader_stage_cis = {
            vert_pipeline_shader_stage_ci,
            frag_pipeline_shader_stage_ci
        };

        vk::PipelineVertexInputStateCreateInfo vertex_input_state_ci {
            .pNext = nullptr,
            .flags = {},
            .vertexBindingDescriptionCount = 0u,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0u,
            .pVertexAttributeDescriptions = nullptr
        };

        vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_ci {
            .pNext = nullptr,
            .flags = {},
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = vk::False
        };

        vk::PipelineTessellationStateCreateInfo tessellation_state_ci {
            .pNext = nullptr,
            .flags = {},
            .patchControlPoints = 0u
        };

        vk::Viewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapchain_extent.width),
            .height = static_cast<float>(swapchain_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vk::Rect2D scissor {
            .offset { .x = 0u, .y = 0u },
            .extent = swapchain_extent
        };
        vk::PipelineViewportStateCreateInfo viewport_state_ci {
            .pNext = nullptr,
            .flags = {},
            .viewportCount = 1u,
            .pViewports = &viewport,
            .scissorCount = 1u,
            .pScissors = &scissor
        };

        vk::PipelineRasterizationStateCreateInfo rasterization_state_ci {
            .pNext = nullptr,
            .flags = {},
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eFront,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = vk::False,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisample_state_ci {
            .pNext = nullptr,
            .flags = {},
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = vk::False,
            .minSampleShading = 0.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = vk::False,
            .alphaToOneEnable = vk::False
        };

        vk::PipelineColorBlendAttachmentState color_blend_attachment_state {
            .blendEnable = vk::True,
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR
                | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA
        };
        std::array<float, 4uz> blend_constants = { 0.0f, 0.0f, 0.0f, 0.0f }; // RGBA
        vk::PipelineColorBlendStateCreateInfo color_blend_state_ci {
            .pNext = nullptr,
            .flags = {},
            .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1u,
            .pAttachments = &color_blend_attachment_state,
            .blendConstants = blend_constants
        };

        std::array<vk::DynamicState, 2uz> dynamic_states = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamic_state_ci {
            .pNext = nullptr,
            .flags = {},
            .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data()
        };

        std::vector<vk::DescriptorSetLayout>
        descriptor_set_layouts = { render_descriptor_set_layout };
        vk::PipelineLayoutCreateInfo pipeline_layout_ci {
            .pNext = nullptr,
            .flags = {},
            .setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
            .pSetLayouts = descriptor_set_layouts.data(),
            .pushConstantRangeCount = 0u,
            .pPushConstantRanges = nullptr
        };
        if (
            vk::Result result = logical_device.createPipelineLayout(&pipeline_layout_ci, nullptr, &render_pipeline_layout);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::PipelineLayout!");
        }

        // Create graphic pipeline
        vk::GraphicsPipelineCreateInfo graphics_pipeline_ci {
            .pNext = nullptr,
            .flags = {},
            .stageCount = static_cast<std::uint32_t>(pipeline_shader_stage_cis.size()),
            .pStages = pipeline_shader_stage_cis.data(),
            .pVertexInputState = &vertex_input_state_ci,
            .pInputAssemblyState = &input_assembly_state_ci,
            .pTessellationState = &tessellation_state_ci,
            .pViewportState = &viewport_state_ci,
            .pRasterizationState = &rasterization_state_ci,
            .pMultisampleState = &multisample_state_ci,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &color_blend_state_ci,
            .pDynamicState = &dynamic_state_ci,
            .layout = render_pipeline_layout,
            .renderPass = render_pass,
            .subpass = 0u,
            .basePipelineHandle = nullptr,
            .basePipelineIndex = 0,
        };
        if (
            vk::Result result = logical_device.createGraphicsPipelines(
                nullptr, 1u, &graphics_pipeline_ci, nullptr, &render_pipeline
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::Pipeline!");
        }

        logical_device.destroyShaderModule(vert_shader_module, nullptr);
        logical_device.destroyShaderModule(frag_shader_module, nullptr);
    }

    void create_frame_buffers() {
        frame_buffers.resize(swapchain_imageviews.size());
        for (std::size_t i { 0uz }; i < swapchain_imageviews.size(); ++i) {
            std::array<vk::ImageView, 1uz> imageviews = { swapchain_imageviews[i] };
            vk::FramebufferCreateInfo frame_buffer_ci {
                .pNext = nullptr,
                .flags = {},
                .renderPass = render_pass,
                .attachmentCount = static_cast<std::uint32_t>(imageviews.size()),
                .pAttachments = imageviews.data(),
                .width = swapchain_extent.width,
                .height = swapchain_extent.height,
                .layers = 1u
            };
            if (
                vk::Result result = logical_device.createFramebuffer(&frame_buffer_ci, nullptr, &frame_buffers[i]);
                result != vk::Result::eSuccess
            ) {
                minilog::log_fatal("Failed to create vk::Framebuffer!");
            }
        }
    }

    void create_descriptor_pool() {
        std::array<vk::DescriptorPoolSize, 2uz> descriptor_pool_size = {
            vk::DescriptorPoolSize {
                .type = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = static_cast<std::uint32_t>(MAX_FRAMES_IN_FLIGHT)
            },
            vk::DescriptorPoolSize {
                .type = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = static_cast<std::uint32_t>(MAX_FRAMES_IN_FLIGHT) * 4u
            }
        };
        vk::DescriptorPoolCreateInfo descriptor_pool_ci {
            .pNext = nullptr,
            .flags = {},
            .maxSets = static_cast<std::uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2u,
            .poolSizeCount = static_cast<std::uint32_t>(descriptor_pool_size.size()),
            .pPoolSizes = descriptor_pool_size.data()
        };
        if (
            vk::Result result = logical_device.createDescriptorPool(&descriptor_pool_ci, nullptr, &descriptor_pool);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::DescriptorPool!");
        }
    }

    void create_compute_descriptor_sets() {
        compute_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        std::vector<vk::DescriptorSetLayout> descriptor_set_layouts(
            MAX_FRAMES_IN_FLIGHT,
            compute_descriptor_set_layout
        );
        vk::DescriptorSetAllocateInfo descriptor_set_ai {
            .pNext = nullptr,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
            .pSetLayouts = descriptor_set_layouts.data()
        };
        if (
            vk::Result result = logical_device.allocateDescriptorSets(&descriptor_set_ai, compute_descriptor_sets.data());
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::DescriptorSet!");
        }

        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vk::DescriptorBufferInfo descriptor_buffer_info { // ubo
                .buffer = uniform_buffers[i],
                .offset = vk::DeviceSize { 0u },
                .range = vk::DeviceSize { sizeof(UniformBufferObject) }
            };
            vk::DescriptorBufferInfo descriptor_buffer_info2 { // vertex buffer
                .buffer = storage_buffers[0uz],
                .offset = vk::DeviceSize { 0u },
                .range = vk::DeviceSize { sizeof(vertices[0uz]) * vertices.size() }
            };
            vk::DescriptorBufferInfo descriptor_buffer_info3 { // index buffer
                .buffer = storage_buffers[1uz],
                .offset = vk::DeviceSize { 0u },
                .range = vk::DeviceSize { sizeof(indices[0uz]) * indices.size() }
            };
            vk::DescriptorBufferInfo descriptor_buffer_info4 { // pixel colors
                .buffer = storage_buffers[2uz],
                .offset = vk::DeviceSize { 0u },
                .range = vk::DeviceSize { width * height * 4u * 4u }
            };
            vk::DescriptorBufferInfo descriptor_buffer_info5 { // seed buffer
                .buffer = storage_buffers[3uz],
                .offset = vk::DeviceSize { 0u },
                .range = vk::DeviceSize { width * height * 4u }
            };
            std::vector<vk::WriteDescriptorSet> write_descriptor_sets = {
                vk::WriteDescriptorSet {
                    .pNext = nullptr,
                    .dstSet = compute_descriptor_sets[i],
                    .dstBinding = 0u,
                    .dstArrayElement = 0u,
                    .descriptorCount = 1u,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &descriptor_buffer_info,
                    .pTexelBufferView = nullptr
                },
                vk::WriteDescriptorSet {
                    .pNext = nullptr,
                    .dstSet = compute_descriptor_sets[i],
                    .dstBinding = 1u,
                    .dstArrayElement = 0u,
                    .descriptorCount = 1u,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &descriptor_buffer_info2,
                    .pTexelBufferView = nullptr
                },
                vk::WriteDescriptorSet {
                    .pNext = nullptr,
                    .dstSet = compute_descriptor_sets[i],
                    .dstBinding = 2u,
                    .dstArrayElement = 0u,
                    .descriptorCount = 1u,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &descriptor_buffer_info3,
                    .pTexelBufferView = nullptr
                },
                vk::WriteDescriptorSet {
                    .pNext = nullptr,
                    .dstSet = compute_descriptor_sets[i],
                    .dstBinding = 3u,
                    .dstArrayElement = 0u,
                    .descriptorCount = 1u,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &descriptor_buffer_info4,
                    .pTexelBufferView = nullptr
                },
                vk::WriteDescriptorSet {
                    .pNext = nullptr,
                    .dstSet = compute_descriptor_sets[i],
                    .dstBinding = 4u,
                    .dstArrayElement = 0u,
                    .descriptorCount = 1u,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &descriptor_buffer_info5,
                    .pTexelBufferView = nullptr
                }
            };
            logical_device.updateDescriptorSets(
                static_cast<std::uint32_t>(write_descriptor_sets.size()),
                write_descriptor_sets.data(),
                0u,
                nullptr
            );
        }
    }

    void create_render_descriptor_sets() {
        render_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        std::vector<vk::DescriptorSetLayout> descriptor_set_layouts(
            MAX_FRAMES_IN_FLIGHT,
            render_descriptor_set_layout
        );
        vk::DescriptorSetAllocateInfo descriptor_set_ai {
            .pNext = nullptr,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
            .pSetLayouts = descriptor_set_layouts.data()
        };
        if (
            vk::Result result = logical_device.allocateDescriptorSets(&descriptor_set_ai, render_descriptor_sets.data());
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::DescriptorSet!");
        }

        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vk::DescriptorBufferInfo descriptor_buffer_info { // pixel colors
                .buffer = storage_buffers[2uz],
                .offset = vk::DeviceSize { 0u },
                .range = vk::DeviceSize { width * height * 4u * 4u }
            };
            std::vector<vk::WriteDescriptorSet> write_descriptor_sets = {
                vk::WriteDescriptorSet {
                    .pNext = nullptr,
                    .dstSet = render_descriptor_sets[i],
                    .dstBinding = 0u,
                    .dstArrayElement = 0u,
                    .descriptorCount = 1u,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &descriptor_buffer_info,
                    .pTexelBufferView = nullptr
                }
            };
            logical_device.updateDescriptorSets(
                static_cast<std::uint32_t>(write_descriptor_sets.size()),
                write_descriptor_sets.data(),
                0u,
                nullptr
            );
        }
    }

    void create_sync_objects() {
        image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        compute_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        render_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
        compute_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        vk::SemaphoreCreateInfo semaphore_ci {
            .pNext = nullptr,
            .flags = {}
        };
        vk::FenceCreateInfo fence_ci {
            .pNext = nullptr,
            .flags = vk::FenceCreateFlagBits::eSignaled
        };
        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (
                logical_device.createSemaphore(&semaphore_ci, nullptr, &image_available_semaphores[i]) != vk::Result::eSuccess
                || logical_device.createSemaphore(&semaphore_ci, nullptr, &render_finished_semaphores[i]) != vk::Result::eSuccess
                || logical_device.createFence(&fence_ci, nullptr, &render_in_flight_fences[i]) != vk::Result::eSuccess
            ) {
                minilog::log_fatal("Failed to create render synchronization objects for a frame!");
            }

            if (
                logical_device.createSemaphore(&semaphore_ci, nullptr, &compute_finished_semaphores[i]) != vk::Result::eSuccess
                || logical_device.createFence(&fence_ci, nullptr, &compute_in_flight_fences[i]) != vk::Result::eSuccess
            ) {
                minilog::log_fatal("Failed to create compute synchronization objects for a frame!");
            }
        }
    }

    void draw_frame() {
        // Compute submission
        if (
            vk::Result result = logical_device.waitForFences(
                1u, &compute_in_flight_fences[current_frame], vk::True, std::numeric_limits<std::uint64_t>::max()
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_debug("compute: wait for vk::Fence failed!");
        }

        update_uniform_buffer(current_frame);

        if (
            vk::Result result = logical_device.resetFences(1u, &compute_in_flight_fences[current_frame]);
            result != vk::Result::eSuccess
        ) {
            minilog::log_debug("compute: reset vk::Fence failed!");
        }

        compute_command_buffers[current_frame].reset({});
        record_compute_command_buffer(compute_command_buffers[current_frame]);

        vk::SubmitInfo submit_info {
            .pNext = nullptr,
            .waitSemaphoreCount = 0u,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1u,
            .pCommandBuffers = &compute_command_buffers[current_frame],
            .signalSemaphoreCount = 1u,
            .pSignalSemaphores = &compute_finished_semaphores[current_frame]
        };
        if (
            vk::Result result = compute_queue.submit(1u, &submit_info, compute_in_flight_fences[current_frame]);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("compute: failed to submit compute command buffer!");
        }

        // Render submission
        if (
            vk::Result result = logical_device.waitForFences(
                1u, &render_in_flight_fences[current_frame], vk::True, std::numeric_limits<std::uint64_t>::max()
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_debug("render: wait for vk::Fence failed!");
        }

        std::uint32_t image_index { 0u };
        if (
            vk::Result result = logical_device.acquireNextImageKHR(
                swapchain,
                std::numeric_limits<std::uint64_t>::max(),
                image_available_semaphores[current_frame],
                nullptr,
                &image_index
            );
            result == vk::Result::eErrorOutOfDateKHR
        ) {
            recreate_swapchain();
            return ;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            minilog::log_fatal("Failed to acquire swap chain image!");
        }

        if (
            vk::Result result = logical_device.resetFences(1u, &render_in_flight_fences[current_frame]);
            result != vk::Result::eSuccess
        ) {
            minilog::log_debug("render: reset vk::Fence failed!");
        }

        render_command_buffers[current_frame].reset({});
        record_render_command_buffer(render_command_buffers[current_frame], image_index);

        std::array<vk::Semaphore, 2uz> wait_semaphores = {
            compute_finished_semaphores[current_frame],
            image_available_semaphores[current_frame]
        };
        std::array<vk::PipelineStageFlags, 2uz> wait_stages = {
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };
        submit_info = vk::SubmitInfo {};
        submit_info.pNext = nullptr;
        submit_info.waitSemaphoreCount = static_cast<std::uint32_t>(wait_semaphores.size());
        submit_info.pWaitSemaphores = wait_semaphores.data();
        submit_info.pWaitDstStageMask = wait_stages.data();
        submit_info.commandBufferCount = 1u;
        submit_info.pCommandBuffers = &render_command_buffers[current_frame];
        submit_info.signalSemaphoreCount = 1u;
        submit_info.pSignalSemaphores = &render_finished_semaphores[current_frame];
        if (
            vk::Result result = graphic_queue.submit(1u, &submit_info, render_in_flight_fences[current_frame]);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("render: failed to submit render command buffer!");
        }

        vk::SwapchainKHR swap_chains[] = { swapchain };
        vk::PresentInfoKHR present_info {
            .pNext = nullptr,
            .waitSemaphoreCount = 1u,
            .pWaitSemaphores = &render_finished_semaphores[current_frame],
            .swapchainCount = 1u,
            .pSwapchains = swap_chains,
            .pImageIndices = &image_index,
            .pResults = nullptr
        };
        if (
            vk::Result result = present_queue.presentKHR(&present_info);
            result == vk::Result::eErrorOutOfDateKHR
                || result == vk::Result::eSuboptimalKHR
                || framebuffer_resized
        ) {
            framebuffer_resized = false;
            recreate_swapchain();
        } else if (result != vk::Result::eSuccess) {
            minilog::log_fatal("Failed to present swap chain image!");
        }

        current_frame = (current_frame + 1u) % MAX_FRAMES_IN_FLIGHT;
    }

    void cleanup_swapchain() {
        for (auto& framebuffer : frame_buffers) { logical_device.destroy(framebuffer); }
        for (auto& imageview : swapchain_imageviews) { logical_device.destroy(imageview); }
        logical_device.destroy(swapchain);
    }

    void recreate_swapchain() {
        int width { 0u };
        int height { 0u };
        glfwGetFramebufferSize(glfw_window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(glfw_window, &width, &height);
            glfwWaitEvents();
        }

        logical_device.waitIdle();

        cleanup_swapchain();
        create_swapchain();
        create_swapchain_imageviews();
        create_frame_buffers();
    }

    void record_render_command_buffer(vk::CommandBuffer commandBuffer, std::uint32_t imageIndex) {
        vk::CommandBufferBeginInfo command_buffer_bi {
            .pNext = nullptr,
            .flags = {},
            .pInheritanceInfo = nullptr
        };
        if (
            vk::Result result = commandBuffer.begin(&command_buffer_bi); // command buffer begin
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to begin recording command buffer!");
        }

        vk::Rect2D render_area { .offset { .x = 0, .y = 0 }, .extent = swapchain_extent };
        vk::ClearValue clear_value { .color { std::array<float, 4uz>{ 0.2f, 0.3f, 0.3f, 1.0f } } };
        vk::RenderPassBeginInfo render_pass_bi {
            .pNext = nullptr,
            .renderPass = render_pass,
            .framebuffer = frame_buffers[imageIndex],
            .renderArea = render_area,
            .clearValueCount = 1u,
            .pClearValues = &clear_value
        };
        commandBuffer.beginRenderPass(&render_pass_bi, vk::SubpassContents::eInline); // render pass begin
        vk::Viewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapchain_extent.width),
            .height = static_cast<float>(swapchain_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vk::Rect2D scissor { .offset { .x = 0, .y = 0 }, .extent = swapchain_extent };
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, render_pipeline);
        commandBuffer.setViewport(0u, 1u, &viewport);
        commandBuffer.setScissor(0u, 1u, &scissor);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            render_pipeline_layout,
            0u,
            1u, &render_descriptor_sets[current_frame],
            0u, nullptr
        );
        commandBuffer.draw(3u, 1u, 0u, 0u);
        commandBuffer.endRenderPass(); // render pass end
        commandBuffer.end(); // command buffer end
    }

    QueueFamilyIndex find_queue_families(vk::PhysicalDevice physicalDevice) {
        std::vector<vk::QueueFamilyProperties>
        queue_family_properties = physicalDevice.getQueueFamilyProperties();

        std::uint32_t i { 0u };
        QueueFamilyIndex queue_family_index {};
        for (const auto& properties : queue_family_properties) {
            if (
                (properties.queueFlags & vk::QueueFlagBits::eGraphics)
                &&(properties.queueFlags & vk::QueueFlagBits::eCompute)
            ) {
                queue_family_index.graphic_and_compute = i;
            }

            if (physicalDevice.getSurfaceSupportKHR(i, surface)) {
                queue_family_index.present = i;
            }

            if (queue_family_index.has_value()) { break; }
            ++i;
        }

        return queue_family_index;
    }

    SwapChainSupportDetail query_swapchain_support_detail(vk::PhysicalDevice physicalDevice) {
        SwapChainSupportDetail swapchain_support_detail {
            .surface_capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface),
            .surface_formats = physicalDevice.getSurfaceFormatsKHR(surface),
            .present_modes = physicalDevice.getSurfacePresentModesKHR(surface)
        };

        return swapchain_support_detail;
    }

    vk::SurfaceFormatKHR choose_swapchain_surface_format(
        const std::vector<vk::SurfaceFormatKHR>& avaiableFormats
    ) {
        for (const auto& surface_format : avaiableFormats) {
            if (
                (surface_format.format == vk::Format::eB8G8R8A8Unorm)
                && (surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            ) {
                return surface_format;
            }
        }

        return avaiableFormats[0uz];
    }

    vk::PresentModeKHR choose_swapchain_present_mode(
        const std::vector<vk::PresentModeKHR>& avaiablePresentModes
    ) {
        for (const auto& present_mode : avaiablePresentModes) {
            if (present_mode == vk::PresentModeKHR::eMailbox) {
                return present_mode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D choose_swapchain_extent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        int width { 0 };
        int height { 0 };
        glfwGetFramebufferSize(glfw_window, &width, &height);

        vk::Extent2D actual_extent {
            .width = std::clamp(
                static_cast<std::uint32_t>(width),
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width
            ),
            .height = std::clamp(
                static_cast<std::uint32_t>(height),
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height
            )
        };

        return actual_extent;
    }

    std::vector<const char*> get_required_extensions() {
        std::uint32_t glfw_required_instance_count { 0u };
        const char** glfw_required_instance_ext = glfwGetRequiredInstanceExtensions(&glfw_required_instance_count);
        std::vector<const char*> instance_extensions(
            glfw_required_instance_ext,
            glfw_required_instance_ext + glfw_required_instance_count
        );
        if (ENABLE_VALIDATION_LAYER) {
            minilog::log_debug("ENABLE_VALIDATION_LAYER: true");
            instance_extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        // For debug
        for (auto& ext : instance_extensions) { minilog::log_debug("instance extensions: {}", ext); }
        return instance_extensions;
    }

    bool check_physical_device_extension_support(vk::PhysicalDevice physicalDevice) {
        std::vector<vk::ExtensionProperties>
        available_extensions = physicalDevice.enumerateDeviceExtensionProperties();
        std::set<std::string> required_extensions(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
        for (const auto& extension : available_extensions) { required_extensions.erase(extension.extensionName); }

        return required_extensions.empty();
    }

    bool is_physical_device_suitable(vk::PhysicalDevice physicalDevice) {
        bool swapchain_adequate { false };
        bool extensions_supported = check_physical_device_extension_support(physicalDevice);
        if (extensions_supported) {
            SwapChainSupportDetail detail = query_swapchain_support_detail(physicalDevice);
            swapchain_adequate = (!detail.surface_formats.empty()) && (!detail.present_modes.empty());
        }

        QueueFamilyIndex queue_family_index = find_queue_families(physicalDevice);

        return queue_family_index.has_value()
            && extensions_supported
            && swapchain_adequate;
    }

    vk::ImageView create_imageview(
        vk::Image image,
        vk::Format format,
        vk::ImageAspectFlags imageAspectFlags,
        std::uint32_t mipLevels
    ) {
        vk::ImageViewCreateInfo imageview_info {
            .pNext = nullptr,
            .flags = {},
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange {
                .aspectMask = imageAspectFlags,
                .baseMipLevel = 0u,
                .levelCount = mipLevels,
                .baseArrayLayer = 0u,
                .layerCount = 1u,
            }
        };

        vk::ImageView imageview;
        if (
            vk::Result result = logical_device.createImageView(&imageview_info, nullptr, &imageview);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create image view!");
        }

        return imageview;
    }

    vk::ShaderModule create_shader_module(const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo shader_module_ci {
            .flags = {},
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const std::uint32_t*>(code.data())
        };

        vk::ShaderModule shader_module;
        if (
            vk::Result result = logical_device.createShaderModule(&shader_module_ci, nullptr, &shader_module);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::ShaderModule");
        }

        return shader_module;
    }

    std::uint32_t find_memory_type(
        std::uint32_t typeFilter,
        vk::MemoryPropertyFlags properties
    ) {
        vk::PhysicalDeviceMemoryProperties memory_properties = physical_device.getMemoryProperties();
        for (std::uint32_t i { 0u }; i < memory_properties.memoryTypeCount; ++i) {
            if (
                (typeFilter & (1u << i))
                && ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
            ) {
                return i;
            }
        }

        minilog::log_fatal("Failed to find suitable memory type!");

        return 0u;
    }

    void create_buffer(
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::Buffer& buffer,
        vk::DeviceMemory& bufferMemory
    ) {
        vk::BufferCreateInfo buffer_ci {
            .pNext = nullptr,
            .flags = {},
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 0u,
            .pQueueFamilyIndices = nullptr
        };
        if (
            vk::Result result = logical_device.createBuffer(&buffer_ci, nullptr, &buffer);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::Buffer!");
        }

        vk::MemoryRequirements memory_requirements = logical_device.getBufferMemoryRequirements(buffer);
        vk::MemoryAllocateInfo memory_ai {
            .pNext = nullptr,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties)
        };
        if (
            vk::Result result = logical_device.allocateMemory(&memory_ai, nullptr, &bufferMemory);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to allocate vk::BufferMemory!");
        }

        logical_device.bindBufferMemory(buffer, bufferMemory, 0u);
    }

    vk::CommandBuffer begin_single_time_commands() {
        vk::CommandBuffer command_buffer;
        vk::CommandBufferAllocateInfo command_buffer_ai {
            .pNext = nullptr,
            .commandPool = command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1u,
        };
        if (
            vk::Result result = logical_device.allocateCommandBuffers(&command_buffer_ai, &command_buffer);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("begin_single_time_commands: failed to allocate vk::CommandBuffer!");
        }

        vk::CommandBufferBeginInfo command_buffer_bi {
            .pNext = nullptr,
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr
        };
        if (
            vk::Result result = command_buffer.begin(&command_buffer_bi);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("begin_single_time_commands: command buffer failed to begin!");
        }

        return command_buffer;
    }

    void end_single_time_commands(vk::CommandBuffer commandBuffer) {
        commandBuffer.end();

        vk::SubmitInfo submit_info {
            .pNext = nullptr,
            .waitSemaphoreCount = 0u,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = {},
            .commandBufferCount = 1u,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0u,
            .pSignalSemaphores = nullptr
        };
        if (
            vk::Result result = graphic_queue.submit(1u, &submit_info, nullptr);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("end_single_time_commands: failed to submit command buffer!");
        }
        graphic_queue.waitIdle();

        logical_device.freeCommandBuffers(command_pool, 1u, &commandBuffer);
    }

    void copy_buffer(
        vk::Buffer srcBuffer,
        vk::Buffer dstBuffer,
        vk::DeviceSize size
    ) {
        vk::CommandBuffer command_buffer = begin_single_time_commands();

        vk::BufferCopy buffer_copy {
            .srcOffset = 0u,
            .dstOffset = 0u,
            .size = size
        };
        command_buffer.copyBuffer(srcBuffer, dstBuffer, 1u, &buffer_copy);

        end_single_time_commands(command_buffer);
    }

    vk::DebugUtilsMessengerCreateInfoEXT create_debug_messenger_ci() {
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

        return debug_utils_messenger_ci;
    }

    void update_uniform_buffer(std::uint32_t currentImage) {
        minilog::log_debug("the sample index: {}", ubo.sample_index);
        memcpy(uniform_buffers_mapped[currentImage], &ubo, sizeof(ubo));
        ubo.sample_index++;
    }

    void record_compute_command_buffer(vk::CommandBuffer commandBuffer) {
        vk::CommandBufferBeginInfo command_buffer_bi {
            .pNext = nullptr,
            .flags = {},
            .pInheritanceInfo = {}
        };
        if (
            vk::Result result = commandBuffer.begin(&command_buffer_bi); // command buffer begin
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to begin recording command buffer!");
        }

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            compute_pipeline_layout,
            0u,
            1u, &compute_descriptor_sets[current_frame],
            0u, nullptr
        );
        commandBuffer.dispatch(width / 8u, height / 8u, 1u);
        commandBuffer.end(); // command buffer end
    }

    void create_image(
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t mipLevels,
        vk::SampleCountFlagBits numSamples,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::Image& image,
        vk::DeviceMemory& imageMemory
    ) {
        vk::ImageCreateInfo image_ci {
            .pNext = nullptr,
            .flags = {},
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent {
                .width = width,
                .height = height,
                .depth = 1u
            },
            .mipLevels = mipLevels,
            .arrayLayers = 1u,
            .samples = numSamples,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 0u,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = vk::ImageLayout::eUndefined
        };
        if (
            vk::Result result = logical_device.createImage(&image_ci, nullptr, &image);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::Image!");
        }

        vk::MemoryRequirements memory_requirements = logical_device.getImageMemoryRequirements(image);
        vk::MemoryAllocateInfo memory_allocate_info {
            .pNext = nullptr,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties)
        };
        if (
            vk::Result result = logical_device.allocateMemory(&memory_allocate_info, nullptr, &imageMemory);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to allocate vk::ImageMemory!");
        }

        logical_device.bindImageMemory(image, imageMemory, 0u);
    }
};




int main() {
    minilog::set_log_level(minilog::log_level::trace); // default log level is 'info'
    // minilog::set_log_file("./mini.log"); // dump log to a specific file

    PathTracing particle_system {};

    try {
        particle_system.run();
    } catch (const std::exception& e) {
        minilog::log_fatal("{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

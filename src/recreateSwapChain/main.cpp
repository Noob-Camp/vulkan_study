#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYER { false };
#else
    constexpr bool ENABLE_VALIDATION_LAYER { true };
#endif

using namespace std::literals::string_literals;


const std::vector<const char*> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" };
// const std::vector<const char*> INSTANCE_EXTENSIONS = { vk::EXTDebugUtilsExtensionName }; // TODO
const std::vector<const char*> DEVICE_EXTENSIONS = { vk::KHRSwapchainExtensionName };

const std::string MODEL_PATH = "./resource/viking_room.obj";
const std::string TEXTURE_PATH = "./resource/viking_room.png";
constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT { 2u };


VKAPI_ATTR vk::Bool32 VKAPI_CALL
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


struct ProjectionTransformation {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};


struct QueueFamilyIndex {
    std::optional<std::uint32_t> graphic;
    std::optional<std::uint32_t> present;

    bool has_value() { return graphic.has_value() && present.has_value(); }
};


struct SwapChainSupportDetail {
    vk::SurfaceCapabilitiesKHR surface_capabilities;
    std::vector<vk::SurfaceFormatKHR> surface_formats;
    std::vector<vk::PresentModeKHR> present_modes;
};


class Application {
private:
    std::uint32_t width { 800u };
    std::uint32_t height { 600u };
    std::string window_name { "Recreate the Swap Chain"s };
    GLFWwindow* glfw_window { nullptr };

    vk::Instance instance;
    bool validation_layers_supported { false };
    vk::DebugUtilsMessengerEXT debug_utils_messenger;

    vk::PhysicalDevice physical_device;
    vk::SampleCountFlagBits msaa_samples { vk::SampleCountFlagBits::e1 };

    vk::Device logical_device;
    vk::Queue graphic_queue;
    vk::Queue present_queue;

    vk::CommandPool command_pool;
    std::vector<vk::CommandBuffer> command_buffers;

    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;
    vk::Format swapchain_image_format;
    vk::Extent2D swapchain_extent;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_imageviews;
    std::vector<vk::Framebuffer> frame_buffers;

    vk::Image color_image;
    vk::DeviceMemory color_device_memory;
    vk::ImageView color_imageview;
    vk::Image depth_image;
    vk::DeviceMemory depth_device_memory;
    vk::ImageView depth_imageview;
    std::uint32_t mip_levels { 0u };
    vk::Image texture_image;
    vk::DeviceMemory texture_device_memory;
    vk::ImageView texture_imageview;
    vk::Sampler texture_sampler;

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vk::Buffer vertex_buffer;
    vk::DeviceMemory vertex_device_memory;
    vk::Buffer index_buffer;
    vk::DeviceMemory index_device_memory;
    std::vector<vk::Buffer> uniform_buffers;
    std::vector<vk::DeviceMemory> uniform_device_memorys;
    std::vector<void*> uniform_buffers_mapped;

    vk::RenderPass render_pass;
    vk::DescriptorSetLayout descriptor_set_layout;
    vk::PipelineLayout render_pipeline_layout;
    vk::Pipeline render_pipeline;

    vk::DescriptorPool descriptor_pool;
    std::vector<vk::DescriptorSet> descriptor_sets;

    std::vector<vk::Semaphore> image_available_semaphores;
    std::vector<vk::Semaphore> render_finished_semaphores;
    std::vector<vk::Fence> in_flight_fences;
    std::uint32_t current_frame { 0u };
    bool framebuffer_resized { false };

public:
    Application() = default;

    Application(
        std::uint32_t _width,
        std::uint32_t _height,
        const std::string& window_name
    )
        : width { _width }
        , height { _height }
        , window_name { window_name }
    {}

    ~Application() {
        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            logical_device.destroy(render_finished_semaphores[i]);
            logical_device.destroy(image_available_semaphores[i]);
            logical_device.destroy(in_flight_fences[i]);
        }
        logical_device.destroy(descriptor_pool);
        logical_device.destroy(render_pipeline);;
        logical_device.destroy(render_pipeline_layout);
        logical_device.destroy(descriptor_set_layout);
        logical_device.destroy(render_pass);
        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            logical_device.destroy(uniform_buffers[i]);
            logical_device.destroy(uniform_device_memorys[i]);
        }
        logical_device.destroy(index_buffer);
        logical_device.destroy(index_device_memory);
        logical_device.destroy(vertex_buffer);
        logical_device.destroy(vertex_device_memory);
        logical_device.destroy(texture_sampler);
        logical_device.destroy(texture_imageview);
        logical_device.destroy(texture_device_memory);
        logical_device.destroy(texture_image);
        cleanup_swapchain();
        logical_device.destroy(command_pool);
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
                auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
                app->framebuffer_resized = true;
                minilog::log_info("the window's size is ({0}, {1})", width, height);
            }
        );
    }

    void init_vulkan() {
        check_validation_layer_support();
        create_instance();
        setup_debug_messenger();

        create_surface();
        pick_physical_device();
        create_logical_device();

        create_command_pool();
        allocate_command_buffers();

        create_swapchain();
        create_swapchain_imageviews();
        create_color_resource();
        create_depth_resource();
        create_frame_buffers();

        load_obj_model();
        create_vertex_buffer();
        create_index_buffer();
        create_uniform_buffers();
        create_texture_image();
        create_texture_imageview();
        create_texutre_sampler();

        create_render_pass();
        create_descriptor_set_layout();
        create_graphic_pipeline();

        create_descriptor_pool();
        create_descriptor_sets();

        create_sync_objects();
    }

    void render_loop() {
        while (!glfwWindowShouldClose(glfw_window)) {
            glfwPollEvents();
            draw_frame();
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
            minilog::log_fatal("Failed to create vk::SurfaceKHR!");
        }
    }

    void pick_physical_device() {
        std::vector<vk::PhysicalDevice> physical_devices = instance.enumeratePhysicalDevices();
        for (const auto& device : physical_devices) {
            if (is_physical_device_suitable(device)) {
                physical_device = device;
                msaa_samples = get_max_usable_sample_count();
                break;
            }
        }

        if (!physical_device) { minilog::log_fatal("Failed to find a suitable physical GPU!"); }
    }

    void create_logical_device() {
        QueueFamilyIndex queue_family_index = find_queue_families(physical_device);
        std::set<std::uint32_t> unique_queue_families = {
            queue_family_index.graphic.value(),
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

        logical_device.getQueue(queue_family_index.graphic.value(), 0u, &graphic_queue);
        logical_device.getQueue(queue_family_index.present.value(), 0u, &present_queue);
    }

    void create_command_pool() {
        QueueFamilyIndex queue_family_index = find_queue_families(physical_device);

        vk::CommandPoolCreateInfo command_pool_ci {
            .pNext = nullptr,
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queue_family_index.graphic.value()
        };
        if (
            vk::Result result = logical_device.createCommandPool(&command_pool_ci, nullptr, &command_pool);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::CommandPool!");
        }
    }

    void allocate_command_buffers() {
        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        vk::CommandBufferAllocateInfo allocInfo {
            .pNext = nullptr,
            .commandPool = command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<std::uint32_t>(command_buffers.size())
        };
        if (
            vk::Result result = logical_device.allocateCommandBuffers(&allocInfo, command_buffers.data());
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to allocate command buffers!");
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
            queue_family_index.graphic.value(),
            queue_family_index.present.value()
        };
        if (queue_family_index.graphic != queue_family_index.present) {
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

    void create_color_resource() {
        vk::Format color_format = swapchain_image_format;
        create_image(
            swapchain_extent.width,
            swapchain_extent.height,
            1u,
            msaa_samples,
            color_format,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment
            | vk::ImageUsageFlagBits::eTransientAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            color_image,
            color_device_memory
        );
        color_imageview = create_imageview(
            color_image,
            color_format,
            vk::ImageAspectFlagBits::eColor,
            1u
        );
    }

    void create_depth_resource() {
        vk::Format depth_format = find_depth_format();
        create_image(
            swapchain_extent.width,
            swapchain_extent.height,
            1u,
            msaa_samples,
            depth_format,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            depth_image,
            depth_device_memory
        );
        depth_imageview = create_imageview(
            depth_image,
            depth_format,
            vk::ImageAspectFlagBits::eDepth,
            1u
        );
    }

    void create_frame_buffers() {
        frame_buffers.resize(swapchain_imageviews.size());
        for (std::size_t i { 0uz }; i < swapchain_imageviews.size(); ++i) {
            std::array<vk::ImageView, 3uz> imageviews = {
                color_imageview,
                depth_imageview,
                swapchain_imageviews[i]
            };
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

    void load_obj_model() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn { ""s };
        std::string err { ""s };
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
            minilog::log_fatal(warn + err);
        }

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
                        attrib.texcoords[static_cast<std::size_t>(2 * index.texcoord_index)],
                        1.0f - attrib.texcoords[static_cast<std::size_t>(2 * index.texcoord_index + 1)]
                    }
                };
                if (unique_vertices.count(vertex) == 0) {
                    unique_vertices[vertex] = static_cast<std::uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(unique_vertices[vertex]);
            }
        }
    }

    void create_vertex_buffer() {
        vk::DeviceSize vertex_device_size = sizeof(vertices[0uz]) * vertices.size();

        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_device_memory;
        create_buffer(
            vertex_device_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent,
            staging_buffer,
            staging_device_memory
        );
        void* data = logical_device.mapMemory(staging_device_memory, 0u, vertex_device_size);
        memcpy(data, vertices.data(), static_cast<std::size_t>(vertex_device_size));
        logical_device.unmapMemory(staging_device_memory);

        create_buffer(
            vertex_device_size,
            vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vertex_buffer,
            vertex_device_memory
        );
        copy_buffer(staging_buffer, vertex_buffer, vertex_device_size);

        logical_device.destroy(staging_buffer);
        logical_device.freeMemory(staging_device_memory);
    }

    void create_index_buffer() {
        vk::DeviceSize index_device_size = sizeof(indices[0uz]) * indices.size();

        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_device_memory;
        create_buffer(
            index_device_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent,
            staging_buffer,
            staging_device_memory
        );
        void* data = logical_device.mapMemory(staging_device_memory, 0u, index_device_size, {});
        memcpy(data, vertices.data(), static_cast<std::size_t>(index_device_size));
        logical_device.unmapMemory(staging_device_memory);

        create_buffer(
            index_device_size,
            vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            index_buffer,
            index_device_memory
        );
        copy_buffer(staging_buffer, index_buffer, index_device_size);

        logical_device.destroy(staging_buffer);
        logical_device.freeMemory(staging_device_memory);
    }

    void create_uniform_buffers() {
        uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_device_memorys.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);
        vk::DeviceSize device_size = sizeof(ProjectionTransformation);
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

    void create_texture_image() {
        int tex_width { 0 };
        int tex_height { 0 };
        int tex_channels { 0 };
        stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
        if (!pixels) { minilog::log_fatal("Failed to load texture image!"); }

        vk::DeviceSize image_device_size = tex_width * tex_height * 4u;
        mip_levels = static_cast<std::uint32_t>(std::floor(std::log2(std::max(tex_width, tex_height)))) + 1u;

        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_device_memory;
        create_buffer(
            image_device_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent,
            staging_buffer,
            staging_device_memory
        );
        void* data = logical_device.mapMemory(staging_device_memory, 0u, image_device_size, {});
        memcpy(data, pixels, static_cast<std::size_t>(image_device_size));
        logical_device.unmapMemory(staging_device_memory);
        stbi_image_free(pixels);

        create_image(
            tex_width,
            tex_height,
            mip_levels,
            vk::SampleCountFlagBits::e1,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferSrc
            | vk::ImageUsageFlagBits::eTransferDst
            | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            texture_image,
            texture_device_memory
        );

        transition_image_layout(
            texture_image,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            mip_levels
        );

        copy_buffer_to_image(
            staging_buffer,
            texture_image,
            static_cast<std::uint32_t>(tex_width),
            static_cast<std::uint32_t>(tex_height)
        );

        logical_device.destroy(staging_buffer);
        logical_device.freeMemory(staging_device_memory);

        generate_mipmaps(
            texture_image,
            vk::Format::eR8G8B8A8Srgb,
            tex_width,
            tex_height,
            mip_levels
        );
    }

    void create_texture_imageview() {
        texture_imageview = create_imageview(
            texture_image,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageAspectFlagBits::eColor,
            mip_levels
        );
    }

    void create_texutre_sampler() {
        vk::PhysicalDeviceProperties physical_device_properties = physical_device.getProperties();
        vk::SamplerCreateInfo sample_ci {
            .pNext = nullptr,
            .flags = {},
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .mipLodBias = 0.0f,
            .anisotropyEnable = vk::True,
            .maxAnisotropy = physical_device_properties.limits.maxSamplerAnisotropy,
            .compareEnable = vk::False,
            .compareOp = vk::CompareOp::eAlways,
            .minLod = 0.0f,
            .maxLod = vk::LodClampNone,
            .borderColor = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = vk::False
        };
        if (
            vk::Result result = logical_device.createSampler(&sample_ci, nullptr, &texture_sampler);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::Sampler!");
        }
    }

    void create_render_pass() {
        vk::AttachmentDescription attachment_desc_color {
            .flags = {},
            .format = swapchain_image_format,
            .samples = msaa_samples,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eColorAttachmentOptimal
        };
        vk::AttachmentReference attachment_ref_color {
            .attachment = 0u,
            .layout = vk::ImageLayout::eColorAttachmentOptimal
        };

        vk::AttachmentDescription attachment_desc_depth {
            .flags = {},
            .format = find_depth_format(),
            .samples = msaa_samples,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        };
        vk::AttachmentReference attachment_ref_depth {
            .attachment = 1u,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        };

        vk::AttachmentDescription attachment_desc_color_resolve {
            .flags = {},
            .format = swapchain_image_format,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eDontCare,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR
        };
        vk::AttachmentReference attachment_ref_color_resolve {
            .attachment = 2u,
            .layout = vk::ImageLayout::eColorAttachmentOptimal
        };

        std::array<vk::AttachmentDescription, 3uz> attachment_descs = {
            attachment_desc_color,
            attachment_desc_depth,
            attachment_desc_color_resolve
        };

        vk::SubpassDescription subpass_desc {
            .flags = {},
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0u,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1u,
            .pColorAttachments = &attachment_ref_color,
            .pResolveAttachments = &attachment_ref_color_resolve,
            .pDepthStencilAttachment = &attachment_ref_depth,
            .preserveAttachmentCount = 0u,
            .pPreserveAttachments = nullptr
        };

        vk::SubpassDependency subpass_dependency {
            .srcSubpass = vk::SubpassExternal,
            .dstSubpass = 0u,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput
                | vk::PipelineStageFlagBits::eLateFragmentTests,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput
                | vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
                | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
                | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            .dependencyFlags = {}
        };

        vk::RenderPassCreateInfo render_pass_ci {
            .pNext = nullptr,
            .flags = {},
            .attachmentCount = static_cast<std::uint32_t>(attachment_descs.size()),
            .pAttachments = attachment_descs.data(),
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

    void create_descriptor_set_layout() {
        vk::DescriptorSetLayoutBinding descriptor_set_layout_binding_ubo {
            .binding = 0u,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1u,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .pImmutableSamplers = nullptr
        };

        vk::DescriptorSetLayoutBinding descriptor_set_layout_binding_sampler {
            .binding = 1u,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1u,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = nullptr
        };

        std::array<vk::DescriptorSetLayoutBinding, 2uz> descriptor_set_layout_bindings = {
            descriptor_set_layout_binding_ubo,
            descriptor_set_layout_binding_sampler
        };

        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci {
            .pNext = nullptr,
            .flags = {},
            .bindingCount = static_cast<std::uint32_t>(descriptor_set_layout_bindings.size()),
            .pBindings = descriptor_set_layout_bindings.data()
        };
        if (
            vk::Result result = logical_device.createDescriptorSetLayout(
                &descriptor_set_layout_ci, nullptr, &descriptor_set_layout
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::DescriptorSetLayout!");
        }
    }

    void create_graphic_pipeline() {
        // Create graphic pipeline layout
        std::vector<char> vert_code = read_shader_file("./src/recreateSwapChain/shaders/vert.spv");
        std::vector<char> frag_code = read_shader_file("./src/recreateSwapChain/shaders/frag.spv");
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

        vk::VertexInputBindingDescription vertex_input_binding_desc {
            .binding = 0u,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };
        std::array<vk::VertexInputAttributeDescription, 3uz> vertex_input_attribute_descs = {
            vk::VertexInputAttributeDescription {
                .location = 0u,
                .binding = 0u,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(Vertex, position)
            },
            vk::VertexInputAttributeDescription {
                .location = 1u,
                .binding = 0u,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(Vertex, color)
            },
            vk::VertexInputAttributeDescription {
                .location = 2u,
                .binding = 0u,
                .format = vk::Format::eR32G32Sfloat,
                .offset = offsetof(Vertex, uv)
            }
        };
        vk::PipelineVertexInputStateCreateInfo vertex_input_state_ci {
            .pNext = nullptr,
            .flags = {},
            .vertexBindingDescriptionCount = 1u,
            .pVertexBindingDescriptions = &vertex_input_binding_desc,
            .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertex_input_attribute_descs.size()),
            .pVertexAttributeDescriptions = vertex_input_attribute_descs.data()
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
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = vk::False,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisample_state_ci {
            .pNext = nullptr,
            .flags = {},
            .rasterizationSamples = msaa_samples,
            .sampleShadingEnable = vk::False,
            .minSampleShading = 0.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = vk::False,
            .alphaToOneEnable = vk::False
        };

        vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_ci {
            .pNext = nullptr,
            .flags = {},
            .depthTestEnable = vk::True,
            .depthWriteEnable = vk::True,
            .depthCompareOp = vk::CompareOp::eLess,
            .depthBoundsTestEnable = vk::False,
            .stencilTestEnable = vk::False,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f
        };

        vk::PipelineColorBlendAttachmentState color_blend_attachment_state {
            .blendEnable = vk::False,
            .srcColorBlendFactor = vk::BlendFactor::eZero,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eZero,
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

        vk::PipelineLayoutCreateInfo pipeline_layout_ci {
            .pNext = nullptr,
            .flags = {},
            .setLayoutCount = 1u,
            .pSetLayouts = &descriptor_set_layout,
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
            .pDepthStencilState = &depth_stencil_state_ci,
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

    void create_descriptor_pool() {
        std::array<vk::DescriptorPoolSize, 2uz> descriptor_pool_size = {
            vk::DescriptorPoolSize {
                .type = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = static_cast<std::uint32_t>(MAX_FRAMES_IN_FLIGHT)
            },
            vk::DescriptorPoolSize {
                .type = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = static_cast<std::uint32_t>(MAX_FRAMES_IN_FLIGHT)
            }
        };
        vk::DescriptorPoolCreateInfo descriptor_pool_ci {
            .pNext = nullptr,
            .flags = {},
            .maxSets = static_cast<std::uint32_t>(MAX_FRAMES_IN_FLIGHT),
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

    void create_descriptor_sets() {
        descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        std::vector<vk::DescriptorSetLayout> descriptor_set_layouts(
            MAX_FRAMES_IN_FLIGHT,
            descriptor_set_layout
        );
        vk::DescriptorSetAllocateInfo descriptor_set_ai {
            .pNext = nullptr,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
            .pSetLayouts = descriptor_set_layouts.data()
        };
        if (
            vk::Result result = logical_device.allocateDescriptorSets(&descriptor_set_ai, descriptor_sets.data());
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to create vk::DescriptorSet!");
        }

        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vk::DescriptorBufferInfo descriptor_buffer_info {
                .buffer = uniform_buffers[i],
                .offset = 0u,
                .range = sizeof(ProjectionTransformation)
            };
            vk::DescriptorImageInfo descriptor_image_info {
                .sampler = texture_sampler,
                .imageView = texture_imageview,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            };
            std::array<vk::WriteDescriptorSet, 2uz> write_descriptor_sets = {
                vk::WriteDescriptorSet {
                    .pNext = nullptr,
                    .dstSet = descriptor_sets[i],
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
                    .dstSet = descriptor_sets[i],
                    .dstBinding = 1u,
                    .dstArrayElement = 0u,
                    .descriptorCount = 1u,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo = &descriptor_image_info,
                    .pBufferInfo = nullptr,
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
        in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

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
                || logical_device.createFence(&fence_ci, nullptr, &in_flight_fences[i]) != vk::Result::eSuccess
            ) {
                minilog::log_fatal("Failed to create synchronization objects for a frame!");
            }
        }
    }

    void draw_frame() {
        if (
            vk::Result result = logical_device.waitForFences(
                1u, &in_flight_fences[current_frame], vk::True, std::numeric_limits<std::uint64_t>::max();
            );
            result != vk::Result::eSuccess
        ) {
            minilog::log_debug("waitForFences failed!");
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

        update_uniform_buffer(current_frame);

        if (
            vk::Result result = logical_device.resetFences(1u, &in_flight_fences[current_frame]);
            result != vk::Result::eSuccess
        ) {
            minilog::log_debug("resetFences failed!");
        }

        command_buffers[current_frame].reset({});
        record_command_buffer(command_buffers[current_frame], image_index);

        vk::Semaphore wait_semaphores[] = { image_available_semaphores[current_frame] };
        vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::Semaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
        vk::SubmitInfo submitInfo {
            .pNext = nullptr,
            .waitSemaphoreCount = 1u,
            .pWaitSemaphores = wait_semaphores,
            .pWaitDstStageMask = wait_stages,
            .commandBufferCount = 1u,
            .pCommandBuffers = &command_buffers[current_frame],
            .signalSemaphoreCount = 1u,
            .pSignalSemaphores = signal_semaphores
        };
        if (
            vk::Result result = graphic_queue.submit(1u, &submitInfo, in_flight_fences[current_frame]);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to submit draw command buffer!");
        }

        vk::SwapchainKHR swap_chains[] = { swapchain };
        vk::PresentInfoKHR present_info {
            .pNext = nullptr,
            .waitSemaphoreCount = 1u,
            .pWaitSemaphores = signal_semaphores,
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
        logical_device.destroy(depth_imageview);
        logical_device.destroy(depth_image);
        logical_device.destroy(depth_device_memory);
        logical_device.destroy(color_imageview);
        logical_device.destroy(color_image);
        logical_device.destroy(color_device_memory);
        for (auto& framebuffer : frame_buffers) { logical_device.destroy(framebuffer); }
        for (auto& imageview : swapchain_imageviews) { logical_device.destroy(imageview); }
        for (auto& image : swapchain_images) { logical_device.destroy(image); }
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
        create_color_resource();
        create_depth_resource();
        create_frame_buffers();
    }

    void record_command_buffer(vk::CommandBuffer commandBuffer, std::uint32_t imageIndex) {
        vk::CommandBufferBeginInfo command_buffer_begin_info {
            .pNext = nullptr,
            .flags = {},
            .pInheritanceInfo = nullptr
        };
        if (
            vk::Result result = commandBuffer.begin(&command_buffer_begin_info); // command buffer begin
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to begin recording command buffer!");
        }

        vk::Rect2D render_area {
            .offset { .x = 0, .y = 0 },
            .extent = swapchain_extent
        };
        std::array<vk::ClearValue, 2uz> clear_values = {
            vk::ClearValue { .color { std::array<float, 4uz>{ 0.2f, 0.3f, 0.3f, 1.0f } } },
            vk::ClearValue { .depthStencil { .depth = 1.0f, .stencil = 0u } }
        };
        vk::RenderPassBeginInfo render_pass_begin_info {
            .pNext = nullptr,
            .renderPass = render_pass,
            .framebuffer = frame_buffers[imageIndex],
            .renderArea = render_area,
            .clearValueCount = static_cast<std::uint32_t>(clear_values.size()),
            .pClearValues = clear_values.data()
        };
        commandBuffer.beginRenderPass(&render_pass_begin_info, vk::SubpassContents::eInline); // render pass begin
        vk::Viewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapchain_extent.width),
            .height = static_cast<float>(swapchain_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vk::Rect2D scissor {
            .offset { .x = 0, .y = 0 },
            .extent = swapchain_extent
        };
        vk::Buffer vertex_buffers[] = { vertex_buffer };
        vk::DeviceSize offsets[] = { 0u };
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, render_pipeline);
        commandBuffer.setViewport(0u, 1u, &viewport);
        commandBuffer.setScissor(0u, 1u, &scissor);
        commandBuffer.bindVertexBuffers(0u, 1u, vertex_buffers, offsets);
        commandBuffer.bindIndexBuffer(index_buffer, 0u, vk::IndexType::eUint32);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            render_pipeline_layout,
            0u,
            1u,
            &descriptor_sets[current_frame],
            0u,
            nullptr
        );
        commandBuffer.drawIndexed(static_cast<std::uint32_t>(indices.size()), 1u, 0u, 0u, 0u);
        commandBuffer.endRenderPass(); // render pass end

        if (
            vk::Result result = commandBuffer.end(); // command buffer end
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("Failed to record command buffer!");
        }
    }

    QueueFamilyIndex find_queue_families(vk::PhysicalDevice physicalDevice) {
        std::vector<vk::QueueFamilyProperties>
        queue_family_properties = physicalDevice.getQueueFamilyProperties();

        std::uint32_t i { 0u };
        QueueFamilyIndex queue_family_index {};
        for (const auto& properties : queue_family_properties) {
            if (properties.queueFlags & vk::QueueFlagBits::eGraphics) {
                queue_family_index.graphic = i;
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
            .surface_capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
            .surface_formats = physicalDevice.getSurfaceFormatsKHR(surface);
            .present_modes = physicalDevice.getSurfacePresentModesKHR(surface);
        };

        return swapchain_support_detail;
    }

    vk::SurfaceFormatKHR choose_swapchain_surface_format(
        const std::vector<vk::SurfaceFormatKHR>& avaiableFormats
    ) {
        for (const auto& surface_format : avaiableFormats) {
            if (
                (surface_format.format == vk::Format::eB8G8R8A8Srgb)
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
        if (ENABLE_VALIDATION_LAYER) { instance_extensions.push_back(vk::EXTDebugUtilsExtensionName); }

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
        vk::PhysicalDeviceFeatures features = physicalDevice.getFeatures();

        return queue_family_index.has_value()
            && extensions_supported
            && swapchain_adequate
            && features.samplerAnisotropy;
    }

    vk::SampleCountFlagBits get_max_usable_sample_count() {
        vk::PhysicalDeviceProperties properties = physical_device.getProperties();
        vk::SampleCountFlags sample_counts = properties.limits.framebufferColorSampleCounts
            & properties.limits.framebufferDepthSampleCounts;
        if (sample_counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
        if (sample_counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
        if (sample_counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
        if (sample_counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
        if (sample_counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
        if (sample_counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

        return vk::SampleCountFlagBits::e1;
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

    vk::Format find_supported_format(
        const std::vector<vk::Format>& candidates,
        vk::ImageTiling tiling,
        vk::FormatFeatureFlags features
    ) {
        for (auto format : candidates) {
            vk::FormatProperties properties = physical_device.getFormatProperties(format);
            if (
                (tiling == vk::ImageTiling::eLinear)
                && ((properties.linearTilingFeatures & features) == features)
            ) {
                return format;
            } else if (
                (tiling == vk::ImageTiling::eOptimal)
                && ((properties.optimalTilingFeatures & features) == features)
            ) {
                return format;
            }
        }

        minilog::log_fatal("Failed to find supported vk::Format!");
    }

    vk::Format find_depth_format() {
        return find_supported_format(
            {
                vk::Format::eD32Sfloat,
                vk::Format::eD32SfloatS8Uint,
                vk::Format::eD24UnormS8Uint
            },
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
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
        vk::MemoryAllocateInfo memory_allocate_info {
            .pNext = nullptr,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties)
        };
        if (
            vk::Result result = logical_device.allocateMemory(&memory_allocate_info, nullptr, &bufferMemory);
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

        vk::CommandBufferBeginInfo command_buffer_begin_info {
            .pNext = nullptr,
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr
        };
        if (
            vk::Result result = command_buffer.begin(&command_buffer_begin_info);
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

    void transition_image_layout(
        vk::Image image,
        vk::Format format,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout,
        std::uint32_t mipLevels
    ) {
        vk::CommandBuffer command_buffer = begin_single_time_commands();

        vk::ImageMemoryBarrier image_memory_barrier {
            .pNext = nullptr,
            .srcAccessMask = {},
            .dstAccessMask = {},
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image,
            .subresourceRange {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0u,
                .levelCount = mipLevels,
                .baseArrayLayer = 0u,
                .layerCount = 1u
            }
        };

        vk::PipelineStageFlags src_pipeline_stage_flags;
        vk::PipelineStageFlags dst_pipeline_stage_flags;
        if (
            (oldLayout == vk::ImageLayout::eUndefined)
            && (newLayout == vk::ImageLayout::eTransferDstOptimal)
        ) {
            image_memory_barrier.srcAccessMask = {};
            image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            src_pipeline_stage_flags = vk::PipelineStageFlagBits::eTopOfPipe;
            dst_pipeline_stage_flags = vk::PipelineStageFlagBits::eTransfer;
        } else if (
            (oldLayout == vk::ImageLayout::eTransferDstOptimal)
            && (newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        ) {
            image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            src_pipeline_stage_flags = vk::PipelineStageFlagBits::eTransfer;
            dst_pipeline_stage_flags = vk::PipelineStageFlagBits::eFragmentShader;
        } else {
            throw std::invalid_argument("transition_image_layout: unsupported layout transition!");
        }

        command_buffer.pipelineBarrier(
            src_pipeline_stage_flags, dst_pipeline_stage_flags,
            {},
            0u, nullptr,
            0u, nullptr,
            1u, &image_memory_barrier
        );

        end_single_time_commands(command_buffer);
    }

    void copy_buffer_to_image(
        vk::Buffer buffer,
        vk::Image image,
        std::uint32_t width,
        std::uint32_t height
    ) {
        vk::CommandBuffer command_buffer = begin_single_time_commands();

        vk::BufferImageCopy buffer_image_copy {
            .bufferOffset = 0u,
            .bufferRowLength = 0u,
            .bufferImageHeight = 0u,
            .imageSubresource {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0u,
                .baseArrayLayer = 0u,
                .layerCount = 1u
            },
            .imageOffset { .x = 0u, .y = 0u, .z = 0u },
            .imageExtent { .width = width, .height = height, .depth = 1u }
        };
        command_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1u, &buffer_image_copy);

        end_single_time_commands(command_buffer);
    }

    void generate_mipmaps(
        vk::Image image,
        vk::Format imageFormat,
        std::int32_t texWidth,
        std::int32_t texHeight,
        std::uint32_t mipLevels
    ) {
        vk::FormatProperties
        format_properties = physical_device.getFormatProperties(imageFormat);
        if (!(format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
            minilog::log_fatal("texture image format does not support linear blitting!");
        }

        vk::CommandBuffer command_buffer = begin_single_time_commands();
        vk::ImageMemoryBarrier image_memory_barrier {
            .pNext = nullptr,
            .srcAccessMask = {},
            .dstAccessMask = {},
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eUndefined,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image,
            .subresourceRange {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0u,
                .levelCount = 1u,
                .baseArrayLayer = 0u,
                .layerCount = 1u
            }
        };

        std::int32_t mipWidth = texWidth;
        std::int32_t mipHeight = texHeight;
        for (std::uint32_t i { 1u }; i < mipLevels; ++i) {
            image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            image_memory_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            image_memory_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            image_memory_barrier.subresourceRange.baseMipLevel = i - 1u;
            command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                {},
                0u, nullptr,
                0u, nullptr,
                1u, &image_memory_barrier
            );

            vk::ImageBlit image_blit {
                .srcSubresource {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = i - 1u,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u
                },
                .srcOffsets = std::array<vk::Offset3D, 2uz> {
                    vk::Offset3D { .x = 0, .y = 0, .z = 0 },
                    vk::Offset3D { .x = mipWidth, .y = mipHeight, .z = 1 }
                },
                .dstSubresource {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = i,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u
                },
                .dstOffsets = std::array<vk::Offset3D, 2uz> {
                    vk::Offset3D { .x = 0, .y = 0, .z = 0 },
                    vk::Offset3D {
                        .x = (mipWidth > 1) ? mipWidth / 2 : 1,
                        .y = (mipHeight > 1) ? mipHeight / 2 : 1,
                        .z = 1
                    }
                }
            };
            command_buffer.blitImage(
                image, vk::ImageLayout::eTransferSrcOptimal,
                image, vk::ImageLayout::eTransferDstOptimal,
                1u, &image_blit,
                vk::Filter::eLinear
            );

            image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
            image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            image_memory_barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
            image_memory_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                {},
                0u, nullptr,
                0u, nullptr,
                1u, &image_memory_barrier
            );

            if (mipWidth > 1) { mipWidth /= 2; }
            if (mipHeight > 1) { mipHeight /= 2; }
        }

        image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        image_memory_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        image_memory_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        image_memory_barrier.subresourceRange.baseMipLevel = mipLevels - 1u;
        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
            {},
            0u, nullptr,
            0u, nullptr,
            1u, &image_memory_barrier
        );

        end_single_time_commands(command_buffer);
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
        static auto start_time = std::chrono::high_resolution_clock::now();

        auto current_time = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

        ProjectionTransformation ubo {
            .model = glm::rotate(
                glm::mat4(1.0f),
                time * glm::radians(90.0f),
                glm::vec3(0.0f, 0.0f, 1.0f)
            ),
            .view = glm::lookAt(
                glm::vec3(2.0f, 2.0f, 2.0f),
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, 1.0f)
            ),
            .proj = glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(swapChainExtent.width)
                    / static_cast<float>(swapChainExtent.height),
                0.1f,
                10.0f
            )
        };
        ubo.proj[1][1] *= -1.0f;

        memcpy(uniform_buffers_mapped[currentImage], &ubo, sizeof(ubo));
    }
};




int main() {
    Application application {};

    try {
        application.run();
    } catch (const std::exception& e) {
        minilog::log_fatal("{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

// #define STB_IMAGE_IMPLEMENTATION
// #include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <minilog.hpp>

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>

#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYER { false };
#else
    constexpr bool ENABLE_VALIDATION_LAYER { true };
#endif

using namespace std::literals::string_literals;


const std::vector<const char*> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" };
// const std::vector<const char*> INSTANCE_EXTENSIONS = { vk::EXTDebugUtilsExtensionName }; // TODO
const std::vector<const char*> DEVICE_EXTENSIONS = { vk::KHRSwapchainExtensionName };


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
    std::size_t operator()(Vertex const& vertex) const {
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
    std::string window_name { "reCreate the Swap Chain"s };
    GLFWwindow* glfw_window { nullptr };

    vk::Instance instance { nullptr };
    bool validation_layers_supported { false };
    vk::DebugUtilsMessengerEXT debug_utils_messenger { nullptr };

    vk::PhysicalDevice physical_device { nullptr };
    vk::SampleCountFlagBits msaa_samples { vk::SampleCountFlagBits::e1 };

    vk::Device logical_device { nullptr };
    vk::Queue graphics_queue { nullptr };
    vk::Queue present_queue { nullptr };

    vk::SurfaceKHR surface { nullptr };
    vk::SwapchainKHR swapchain { nullptr };
    vk::Format swapchain_image_format;
    vk::Extent2D swapchain_extent {};
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_imageviews;
    std::vector<vk::Framebuffer> swapchain_framebuffers;

    vk::RenderPass render_pass { nullptr };
    vk::PipelineLayout render_pipeline_layout { nullptr };
    vk::Pipeline render_pipeline { nullptr };

    vk::CommandPool command_pool { nullptr };
    std::vector<vk::CommandBuffer> command_buffers;

    std::vector<vk::Semaphore> image_available_semaphores;
    std::vector<vk::Semaphore> render_finished_semaphores;
    std::vector<vk::Fence> in_flight_fences;
    std::uint32_t current_frame { 0u };
    static constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT { 2u };
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
        cleanup_swapchain();

        logical_device.destroy(render_pipeline);;
        logical_device.destroy(render_pipeline_layout);
        logical_device.destroy(render_pass);

        for (std::size_t i { 0uz }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            logical_device.destroy(render_finished_semaphores[i]);
            logical_device.destroy(image_available_semaphores[i]);
            logical_device.destroy(in_flight_fences[i]);
        }

        logical_device.destroy(command_pool);
        logical_device.destroy();

        // if (ENABLE_VALIDATION_LAYER) {
        //     instance.destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        // }

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
        if (!glfw_window) { minilog::log_fatal("GLFW Failed to create GLFWwindow!"); }

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

        create_swapchain();
        create_imageviews();

        create_render_pass();
        createGraphicsPipeline();

        createFrameBuffers();
        createCommandPool();
        createCommandBuffer();

        createSyncObjects();
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
            .pApplicationName = "ReCreate the Swap Chain",
            .applicationVersion = support_vulkan_version,
            .pEngineName = "No Engine",
            .engineVersion = support_vulkan_version,
            .apiVersion = support_vulkan_version
        };

        auto instance_extensions = get_required_extensions();
        vk::InstanceCreateInfo instance_ci {
            .flags = {},
            .pApplicationInfo = &application_info,
            .enabledLayerCount = 0u,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<std::uint32_t>(instance_extensions.size()),
            .ppEnabledExtensionNames = instance_extensions.data()
        };
        if (ENABLE_VALIDATION_LAYER) {
            instance_ci.enabledLayerCount = static_cast<std::uint32_t>(VALIDATION_LAYERS.size());
            instance_ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }

        if (
            vk::Result result = vk::createInstance(&instance_ci, nullptr, &instance);
            result != vk::Result::eSuccess
        ) {
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
        PFN_vkGetInstanceProcAddr
        getInstanceProcAddr = dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
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

    void create_surface() {
        if (
            VkResult result = glfwCreateWindowSurface(
                static_cast<VkInstance>(instance),
                glfw_window,
                nullptr,
                reinterpret_cast<VkSurfaceKHR*>(&surface)
            );
            result != VK_SUCCESS
        ) {
            minilog::log_fatal("failed to create vk::SurfaceKHR!");
        }
    }

    void pick_physical_device() {
        std::vector<vk::PhysicalDevice> physical_devices = instance.enumeratePhysicalDevices();
        for (const vk::PhysicalDevice& device : physical_devices) {
            if (is_physical_device_suitable(device)) {
                physical_device = device;
                msaa_samples = get_max_usable_sample_count();
                break;
            }
        }

        if (!physical_device) { minilog::log_fatal("failed to find a suitable GPU!"); }
    }

    void create_logical_device() {
        QueueFamilyIndex queue_family_index = find_queue_families(physical_device);
        std::set<std::uint32_t> unique_queue_families = {
            queue_family_index.graphic.value(),
            queue_family_index.present.value()
        };

        float queuePriority { 1.0f }; // default
        std::vector<vk::DeviceQueueCreateInfo> device_queue_cis;
        for (std::uint32_t queue_family : unique_queue_families) {
            vk::DeviceQueueCreateInfo device_queue_ci {
                .queueFamilyIndex = queue_family,
                .queueCount = 1u,
                .pQueuePriorities = &queuePriority
            };
            device_queue_cis.push_back(device_queue_ci);
        }

        vk::PhysicalDeviceFeatures physical_device_features {};
        physical_device_features.samplerAnisotropy = vk::True;

        vk::DeviceCreateInfo device_ci {
            .flags = {}, // flags is reserved for future use
            .queueCreateInfoCount = static_cast<std::uint32_t>(device_queue_cis.size()),
            .pQueueCreateInfos = device_queue_cis.data(),
            .enabledLayerCount = 0u,
            .ppEnabledLayerNames = nullptr,
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
            minilog::log_fatal("failed to create logical device!");
        }

        logical_device.getQueue(queue_family_index.graphic.value(), 0u, &graphics_queue);
        logical_device.getQueue(queue_family_index.present.value(), 0u, &present_queue);
    }

    void create_swapchain() {
        SwapChainSupportDetail swapchain_support_detail = query_swapchain_support(physical_device);
        vk::SurfaceFormatKHR surface_format = choose_swapchain_surface_format(swapchain_support_detail.surface_formats);
        vk::PresentModeKHR present_mode = choose_swapchain_present_mode(swapchain_support_detail.present_modes);
        vk::Extent2D extent = choose_swapchain_extent(swapchain_support_detail.surface_capabilities);

        std::uint32_t image_count = swapchain_support_detail.surface_capabilities.minImageCount + 1u; // realization of triple buffer
        if (
            (swapchain_support_detail.surface_capabilities.maxImageCount > 0u)
            && (image_count > swapchain_support_detail.surface_capabilities.maxImageCount)
        ) {
            image_count = swapchain_support_detail.surface_capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR swapchain_ci {
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
        QueueFamilyIndex indices = find_queue_families(physical_device);
        std::array<std::uint32_t, 2uz> queue_family_indices {
            indices.graphic.value(),
            indices.present.value()
        };
        if (indices.graphic != indices.present) {
            swapchain_ci.imageSharingMode = vk::SharingMode::eConcurrent;
            swapchain_ci.queueFamilyIndexCount = 2;
            swapchain_ci.pQueueFamilyIndices = queue_family_indices.data();
        }

        if (
            vk::Result result = logical_device.createSwapchainKHR(&swapchain_ci, nullptr, &swapchain);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::SwapchainCreateInfoKHR!");
        }

        swapchain_images = logical_device.getSwapchainImagesKHR(swapchain);
        swapchain_image_format = swapchain_ci.imageFormat;
        swapchain_extent = swapchain_ci.imageExtent;
    }

    void create_imageviews() {
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
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dependencyFlags = ,
        };

        vk::RenderPassCreateInfo render_pass_ci {
            .flags = {},
            .attachmentCount = static_cast<std::uint32_t>(attachment_descs.size()),
            .pAttachments = &attachment_descs.data(),
            .subpassCount = 1u,
            .pSubpasses = &subpass_desc,
            .dependencyCount = 1u,
            .pDependencies = &subpass_dependency,
        };

        if (
            vk::Result result = logical_device.createRenderPass(&render_pass_ci, nullptr, &render_pass);
            result != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::RenderPass!");
        }
    }

    void cleanup_swapchain() {
        for (auto framebuffer : swapchain_framebuffers) { logical_device.destroy(framebuffer); }
        for (auto imageview : swapchain_imageviews) { logical_device.destroy(imageview); }
        logical_device.destroy(swapchain);
    }

    void recreateSwapChain() {
        int width { 0u }, height { 0u };
        glfwGetFramebufferSize(glfw_window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(glfw_window, &width, &height);
            glfwWaitEvents();
        }

        logical_device.waitIdle();

        cleanup_swapchain();
        create_swapchain();
        create_imageviews();
        createFrameBuffers();
    }

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, std::uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo {
            .pInheritanceInfo = nullptr
        };

        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to begin recording command buffer!");
        }

        vk::RenderPassBeginInfo renderPassBeginInfo {};
        renderPassBeginInfo.renderPass = render_pass;
        renderPassBeginInfo.framebuffer = swapchain_framebuffers[imageIndex];
        vk::Rect2D renderArea {};
        renderArea.offset.setX(0);
        renderArea.offset.setY(0);
        renderArea.extent = swapchain_extent;
        renderPassBeginInfo.renderArea = renderArea;
        vk::ClearValue clearColor {
            .color {
                std::array<float, 4>{ 0.2f, 0.3f, 0.3f, 1.0f }
            }
        };
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearColor;
        commandBuffer.beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, render_pipeline);
            vk::Viewport viewport {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapchain_extent.width);
            viewport.height = static_cast<float>(swapchain_extent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            commandBuffer.setViewport(0, 1, &viewport);

            vk::Rect2D scissor{
                .offset { .x = 0, .y = 0 },
                .extent = swapchain_extent
            };
            commandBuffer.setScissor(0, 1, &scissor);
            commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();///

        commandBuffer.end();
        // if (.result != vk::Result::eSuccess) {
        //     minilog::log_fatal("failed to record command buffer!");
        // } else {
        //     minilog::log_info("record command buffer successfully!");
        // }
    }

    void draw_frame() {
        if (
            vk::Result result = logical_device.waitForFences(1, &in_flight_fences[current_frame], vk::True, UINT64_MAX);
            result != vk::Result::eSuccess
        ) {
            minilog::log_debug("waitForFences failed!");
        }

        std::uint32_t imageIndex { 0u };
        vk::Result result = logical_device.acquireNextImageKHR(swapchain, UINT64_MAX, image_available_semaphores[current_frame], nullptr, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            minilog::log_fatal("failed to acquire swap chain image!");
        }
        if (
            vk::Result result = logical_device.resetFences(1, &in_flight_fences[current_frame]);
            result != vk::Result::eSuccess
        ) {
            minilog::log_debug("resetFences failed!");
        }

        command_buffers[current_frame].reset();
        recordCommandBuffer(command_buffers[current_frame], imageIndex);

        vk::Semaphore waitSemaphores[] = { image_available_semaphores[current_frame] };
        vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::SubmitInfo submitInfo {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffers[current_frame]
        };


        vk::Semaphore signalSemaphores[] = { render_finished_semaphores[current_frame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (graphics_queue.submit(1, &submitInfo, in_flight_fences[current_frame]) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to submit draw command buffer!");
        }

        vk::PresentInfoKHR presentInfo {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores
        };


        vk::SwapchainKHR swapChains[] = { swapchain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;
        result = present_queue.presentKHR(&presentInfo);

        if (result == vk::Result::eErrorOutOfDateKHR
                        || result == vk::Result::eSuboptimalKHR
                        || framebuffer_resized
        ) {
            framebuffer_resized = false;
            recreateSwapChain();
        } else if (result != vk::Result::eSuccess) {
            minilog::log_fatal("failed to present swap chain image!");
        }

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }



    // vk::Result CreateDebugUtilsMessengerEXT(
    //     vk::Instance instance_,
    //     const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    //     const vk::AllocationCallbacks* pAllocator,
    //     vk::DebugUtilsMessengerEXT* pDebugMessenger
    // ) {
    //     auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
    //                     vk::getInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
    //     if (func != nullptr) {
    //         return func(instance_, pCreateInfo, pAllocator, pDebugMessenger);
    //     } else {
    //         return VK_ERROR_EXTENSION_NOT_PRESENT;
    //     }
    // }

    // void DestroyDebugUtilsMessengerEXT(
    //     vk::Instance instance_,
    //     vk::DebugUtilsMessengerEXT debugMessenger,
    //     const vk::AllocationCallbacks* pAllocator
    // ) {
    //     auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
    //                 vk::getInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
    //     if (func != nullptr) {
    //         func(instance_, debugMessenger, pAllocator);
    //     }
    // }






    static std::vector<char> readFile(const std::string& fileName) {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            minilog::log_fatal("failed to open file: {}", fileName);
        }

        std::size_t fileSize = static_cast<std::size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    vk::ShaderModule createShaderModule(const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo createInfo {};
        //createInfo.flags = ;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

        vk::ShaderModule shaderModule;
        if (logical_device.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::ShaderModule");
        }

        return shaderModule;
    }

    void createGraphicsPipeline() {
        std::vector<char> vertCode = readFile("./src/recreateSwapChain/shaders/vert.spv");
        std::vector<char> fragCode = readFile("./src/recreateSwapChain/shaders/frag.spv");
        vk::ShaderModule vertShaderModule = createShaderModule(vertCode);
        vk::ShaderModule fragShaderModule = createShaderModule(fragCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo {};
        // vertShaderStageInfo.flags = 0;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        vertShaderStageInfo.pSpecializationInfo = nullptr;

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo {};
        // fragShaderStageInfo.flags = 0;
        fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        fragShaderStageInfo.pSpecializationInfo = nullptr;

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
            vertShaderStageInfo,
            fragShaderStageInfo
        };

        vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo {};// begin
        //graphicsPipelineCreateInfo.flags = ;
        graphicsPipelineCreateInfo.stageCount = 2;
        graphicsPipelineCreateInfo.pStages = shaderStages.data();

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo {};
        //vertexInputInfo.flags = ;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        graphicsPipelineCreateInfo.pVertexInputState = &vertexInputInfo;///

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {};
        inputAssemblyStateInfo.topology = vk::PrimitiveTopology::eTriangleList;
        inputAssemblyStateInfo.primitiveRestartEnable = vk::Bool32(VK_FALSE);
        graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateInfo;///

        vk::PipelineTessellationStateCreateInfo tessellationStateInfo {};
        graphicsPipelineCreateInfo.pTessellationState = &tessellationStateInfo;///

        vk::Viewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain_extent.width);
        viewport.height = static_cast<float>(swapchain_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor {};
        scissor.offset.setX(0);
        scissor.offset.setY(0);
        scissor.extent = swapchain_extent;

        vk::PipelineViewportStateCreateInfo viewportInfo {};
        //viewportInfo.flags = ;
        viewportInfo.viewportCount = 1;
        viewportInfo.pViewports = &viewport;
        viewportInfo.scissorCount = 1;
        viewportInfo.pScissors = &scissor;
        graphicsPipelineCreateInfo.pViewportState = &viewportInfo;///

        vk::PipelineRasterizationStateCreateInfo rasterizationStateInfo {};
        rasterizationStateInfo.depthClampEnable = vk::Bool32(VK_FALSE);
        rasterizationStateInfo.rasterizerDiscardEnable = vk::Bool32(VK_FALSE);
        rasterizationStateInfo.polygonMode = vk::PolygonMode::eFill;
        rasterizationStateInfo.lineWidth = 1.0f;
        rasterizationStateInfo.cullMode = vk::CullModeFlagBits::eFront;
        rasterizationStateInfo.depthBiasEnable = vk::Bool32(VK_FALSE);
        graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateInfo;///

        vk::PipelineMultisampleStateCreateInfo multisampleStateInfo {};
        multisampleStateInfo.sampleShadingEnable = vk::Bool32(VK_FALSE);
        multisampleStateInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
        graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateInfo;///

        vk::PipelineDepthStencilStateCreateInfo depthStencilStateInfo {};
        //depthStencilStateInfo.flags =
        depthStencilStateInfo.depthTestEnable = vk::Bool32(VK_TRUE);
        depthStencilStateInfo.depthWriteEnable = vk::Bool32(VK_TRUE);
        depthStencilStateInfo.depthCompareOp = vk::CompareOp::eLess;
        depthStencilStateInfo.depthBoundsTestEnable = vk::Bool32(VK_FALSE);
        depthStencilStateInfo.stencilTestEnable = vk::Bool32(VK_FALSE);
        // depthStencilStateInfo.front = {};// Optional
        // depthStencilStateInfo.back = {};// Optional
        // depthStencilStateInfo.minDepthBounds = 0.0f;// Optional
        // depthStencilStateInfo.maxDepthBounds = 1.0f;// Optional
        graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateInfo;///

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.blendEnable = vk::Bool32(VK_FALSE);
        // colorBlendAttachment.srcColorBlendFactor =
        // colorBlendAttachment.dstColorBlendFactor =
        // colorBlendAttachment.colorBlendOp =
        // colorBlendAttachment.srcAlphaBlendFactor =
        // colorBlendAttachment.dstAlphaBlendFactor =
        // colorBlendAttachment.alphaBlendOp =
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR
                                                | vk::ColorComponentFlagBits::eG
                                                | vk::ColorComponentFlagBits::eB
                                                | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendStateCreateInfo colorBlendStateInfo {};
        //colorBlendStateInfo.flags =
        colorBlendStateInfo.logicOpEnable = vk::Bool32(VK_FALSE);
        colorBlendStateInfo.logicOp = vk::LogicOp::eCopy;
        colorBlendStateInfo.attachmentCount = 1;
        colorBlendStateInfo.pAttachments = &colorBlendAttachment;
        colorBlendStateInfo.blendConstants[0] = 0.0f;// R
        colorBlendStateInfo.blendConstants[1] = 0.0f;// G
        colorBlendStateInfo.blendConstants[2] = 0.0f;// B
        colorBlendStateInfo.blendConstants[3] = 0.0f;// A
        graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateInfo;///

        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicStateInfo {};
        //dynamicStateInfo.flags =;
        dynamicStateInfo.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamicStateInfo.pDynamicStates = dynamicStates.data();
        graphicsPipelineCreateInfo.pDynamicState = &dynamicStateInfo;///

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        if (logical_device.createPipelineLayout(
            &pipelineLayoutInfo,
            nullptr,
            &render_pipeline_layout) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::PipelineLayout!");
        }

        graphicsPipelineCreateInfo.layout = render_pipeline_layout;
        graphicsPipelineCreateInfo.renderPass = render_pass;
        graphicsPipelineCreateInfo.subpass = 0;
        graphicsPipelineCreateInfo.basePipelineHandle = nullptr;
        graphicsPipelineCreateInfo.basePipelineIndex = -1;

        if (logical_device.createGraphicsPipelines(
            nullptr,
            1,
            &graphicsPipelineCreateInfo,
            nullptr,
            &render_pipeline) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::Pipeline!");
        }

        logical_device.destroyShaderModule(vertShaderModule, nullptr);
        logical_device.destroyShaderModule(fragShaderModule, nullptr);
    }

    void createFrameBuffers() {
        swapchain_framebuffers.resize(swapchain_imageviews.size());
        for (std::size_t i { 0u }; i < swapchain_imageviews.size(); ++i) {
            vk::ImageView attachments[] = { swapchain_imageviews[i] };
            vk::FramebufferCreateInfo framebufferInfo {};
            //framebufferInfo.flages = ;
            framebufferInfo.renderPass = render_pass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapchain_extent.width;
            framebufferInfo.height = swapchain_extent.height;
            framebufferInfo.layers = 1;

            if (logical_device.createFramebuffer(&framebufferInfo, nullptr, &swapchain_framebuffers[i]) != vk::Result::eSuccess) {
                minilog::log_fatal("failed to create vk::Framebuffer!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndex queueFamilyIndices = find_queue_families(physical_device);

        vk::CommandPoolCreateInfo poolInfo {};
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphic.value();

        if (logical_device.createCommandPool(&poolInfo, nullptr, &command_pool) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::CommandPool!");
        }
    }

    void createCommandBuffer() {
        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        vk::CommandBufferAllocateInfo allocInfo {};
        allocInfo.commandPool = command_pool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = static_cast<std::uint32_t>(command_buffers.size());

        if (logical_device.allocateCommandBuffers(&allocInfo, command_buffers.data()) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::CommandBuffer!");
        }
    }


    void createSyncObjects() {
        image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        vk::SemaphoreCreateInfo semaphoreInfo {};
        //semaphoreInfo.flags = ;

        vk::FenceCreateInfo fenceInfo {};
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (std::size_t i { 0 }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (logical_device.createSemaphore(&semaphoreInfo, nullptr, &image_available_semaphores[i]) != vk::Result::eSuccess
                || logical_device.createSemaphore(&semaphoreInfo, nullptr, &render_finished_semaphores[i]) != vk::Result::eSuccess
                || logical_device.createFence(&fenceInfo, nullptr, &in_flight_fences[i]) != vk::Result::eSuccess
            ) {
                minilog::log_fatal("failed to create synchronization objects for a frame!");
            }
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

            vk::Bool32 is_present_support = physicalDevice.getSurfaceSupportKHR(i, surface);
            if (is_present_support) { queue_family_index.present = i; }

            if (queue_family_index.has_value()) { break; }
            ++i;
        }

        return queue_family_index;
    }

    SwapChainSupportDetail query_swapchain_support(vk::PhysicalDevice physicalDevice) {
        SwapChainSupportDetail swapchain_support_detail {};
        swapchain_support_detail.surface_capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);

        swapchain_support_detail.surface_formats = physicalDevice.getSurfaceFormatsKHR(surface);
        swapchain_support_detail.present_modes = physicalDevice.getSurfacePresentModesKHR(surface);

        return swapchain_support_detail;
    }

    vk::SurfaceFormatKHR choose_swapchain_surface_format(
        const std::vector<vk::SurfaceFormatKHR>& avaiableFormats
    ) {
        for (const auto& availableFormat : avaiableFormats) {
            if (
                (availableFormat.format == vk::Format::eB8G8R8A8Srgb)
                && (availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            ) {
                return availableFormat;
            }
        }

        return avaiableFormats[0uz];
    }

    vk::PresentModeKHR choose_swapchain_present_mode(
        const std::vector<vk::PresentModeKHR>& avaiablePresentModes
    ) {
        for (const auto& avaiablePresentMode : avaiablePresentModes) {
            if (avaiablePresentMode == vk::PresentModeKHR::eMailbox) {
                return avaiablePresentMode;
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
        std::uint32_t glfw_extension_count { 0u };
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char*> instance_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
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
        QueueFamilyIndex queue_family_index = find_queue_families(physicalDevice);
        bool extensions_supported = check_physical_device_extension_support(physicalDevice);

        bool swapchain_adequate { false };
        if (extensions_supported) {
            SwapChainSupportDetail detail = query_swapchain_support(physicalDevice);
            swapchain_adequate = (!detail.surface_formats.empty()) && (!detail.present_modes.empty());
        }

        vk::PhysicalDeviceFeatures supportedFeatures = physicalDevice.getFeatures();

        return queue_family_index.has_value()
            && extensions_supported
            && swapchain_adequate
            && supportedFeatures.samplerAnisotropy; // specifies whether anisotropic filtering is supported
    }

    vk::SampleCountFlagBits get_max_usable_sample_count() {
        vk::PhysicalDeviceProperties
        physical_device_properties = physical_device.getProperties();

        vk::SampleCountFlags
        sample_counts = physical_device_properties.limits.framebufferColorSampleCounts
            & physical_device_properties.limits.framebufferDepthSampleCounts;
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
            throw std::runtime_error("failed to create image view!");
        }

        return imageview;
    }

    vk::Format find_supported_format(
        const std::vector<vk::Format>& candidates,
        vk::ImageTiling tiling,
        vk::FormatFeaturesFlags features
    ) {
        for (auto format : candidates) {
            vk::FormatProperties format_properties = physical_device.getFormatProperties(format);
            if (
                (tiling == vk::ImageTiling::eLinear)
                && ((format_properties.linearTilingFeatures & features) == features)
            ) {
                return format;
            } else if (
                (tiling == vk::ImageTiling::eOptimal)
                && ((format_properties.optimalTilingFeatures & features) == features)
            ) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported vk::Format!");
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
};




int main() {
    Application application {};

    try {
        application.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

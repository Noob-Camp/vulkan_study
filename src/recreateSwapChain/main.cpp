#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

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
    bool validationLayersSupported { false };
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    vk::DebugUtilsMessengerEXT debugMessenger { nullptr };

    vk::PhysicalDevice physicalDevice { nullptr };
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
        if (glfw_window) { minilog::log_fatal("GLFW Failed to create GLFWwindow!"); }

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
        populateDebugUtilsMessengerCreateInfoEXT();
        create_instance();
        setupDebugMessenger();

        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();

        createSwapChain();
        createImageViews();

        createRenderPass();
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
            .pApplicationInfo = &application_info
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
        createSwapChain();
        createImageViews();
        createFrameBuffers();
    }

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, std::uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo {
            .pInheritanceInfo = nullptr
        };

        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to begin recording command buffer!");
        } else {
            minilog::log_info("begin recording command buffer!");
        }

        vk::RenderPassBeginInfo renderPassBeginInfo {};
        renderPassBeginInfo.render_pass = render_pass;
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
        logical_device.waitForFences(1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

        std::uint32_t imageIndex { 0u };
        vk::Result result = logical_device.acquireNextImageKHR(swapchain, UINT64_MAX, image_available_semaphores[current_frame], nullptr, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            minilog::log_fatal("failed to acquire swap chain image!");
        }
        logical_device.resetFences(1, &in_flight_fences[current_frame]);

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
        } else {
            minilog::log_info("submit draw command buffer successfully!");
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

    void setupDebugMessenger() {
        if (!ENABLE_VALIDATION_LAYER) { return; }
        vk::detail::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::detail::DispatchLoaderDynamic dispatch(instance, getInstanceProcAddr);
        if (instance.createDebugUtilsMessengerEXT(&debugCreateInfo, nullptr, &debugMessenger, dispatch) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to set up debug messenger!");
        }
    }

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




    void populateDebugUtilsMessengerCreateInfoEXT() {
        debugCreateInfo.flags = vk::DebugUtilsMessengerCreateFlagsEXT();
        debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugCreateInfo.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debug_callback);
        debugCreateInfo.pUserData = nullptr;// optional
    }

    void createSurface() {
        if (glfwCreateWindowSurface(
                static_cast<VkInstance>(instance),
                glfw_window,
                nullptr,
                reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS
        ) {
            minilog::log_fatal("failed to create vk::SurfaceKHR!");
        } else {
            minilog::log_info("create vk::SurfaceKHR successfully!");
        }
    }

    void pickPhysicalDevice() {
        std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
        for (const vk::PhysicalDevice& device : devices) {
            // if (isPhysicalDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            // }
        }

        if (physicalDevice == nullptr) {
            minilog::log_fatal("failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice() {
        QueueFamilyIndex indices = findQueueFamilies(physicalDevice);
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<std::uint32_t> uniqueQueueFamilies = {
            indices.graphic.value(),
            indices.present.value()
        };

        float queuePriority { 1.0f };
        for (std::uint32_t queueFamily : uniqueQueueFamilies) {
            vk::DeviceQueueCreateInfo queueCreateInfo {};
            // queueCreateInfo.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        vk::DeviceCreateInfo deviceCreateInfo {};
        // createInfo.flags = ;// flags is reserved for future use
        deviceCreateInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

        if (ENABLE_VALIDATION_LAYER) {
            deviceCreateInfo.enabledLayerCount = static_cast<std::uint32_t>(VALIDATION_LAYERS.size());
            deviceCreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        } else {
            deviceCreateInfo.enabledLayerCount = 0;
            deviceCreateInfo.ppEnabledLayerNames = nullptr;
        }

        deviceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        vk::PhysicalDeviceFeatures physicalDeviceFeatures {};
        physicalDeviceFeatures.samplerAnisotropy = vk::Bool32(VK_TRUE);
        deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

        if (physicalDevice.createDevice(&deviceCreateInfo, nullptr, &logical_device) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create logical device!");
        } else {
            minilog::log_info("create logical device successfully!");
        }

        logical_device.getQueue(indices.graphic.value(), 0, &graphics_queue);
        logical_device.getQueue(indices.present.value(), 0, &present_queue);
    }

    void createSwapChain() {
        SwapChainSupportDetail swapChainSupport = querySwapChainSupport(physicalDevice);
        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.surface_formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.present_modes);
        vk::Extent2D extent = chooseSwapExtent(swapChainSupport.surface_capabilities);

        std::uint32_t imageCount = swapChainSupport.surface_capabilities.minImageCount + 1;// realization of triple buffer
        if ((swapChainSupport.surface_capabilities.maxImageCount > 0)
            && (imageCount > swapChainSupport.surface_capabilities.maxImageCount)
        ) {
            imageCount = swapChainSupport.surface_capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR swapChainCreateInfo {};
        //createInfo.flags =;
        swapChainCreateInfo.surface = surface;
        swapChainCreateInfo.minImageCount = imageCount;
        swapChainCreateInfo.imageFormat = surfaceFormat.format;
        swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapChainCreateInfo.imageExtent = extent;
        swapChainCreateInfo.imageArrayLayers = 1;
        swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

        QueueFamilyIndex indices = findQueueFamilies(physicalDevice);
        std::uint32_t queueFamilyIndices[] = {
            indices.graphic.value(),
            indices.present.value()
        };
        if (indices.graphic != indices.present) {
            swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            swapChainCreateInfo.queueFamilyIndexCount = 2;
            swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
            swapChainCreateInfo.queueFamilyIndexCount = 0;
            swapChainCreateInfo.pQueueFamilyIndices = nullptr;
        }

        swapChainCreateInfo.preTransform = swapChainSupport.surface_capabilities.currentTransform;
        swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapChainCreateInfo.presentMode = presentMode;
        swapChainCreateInfo.clipped = vk::Bool32(VK_TRUE);
        swapChainCreateInfo.oldSwapchain = nullptr;

        if (logical_device.createSwapchainKHR(&swapChainCreateInfo, nullptr, &swapchain) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::SwapchainCreateInfoKHR!");
        } else {
            minilog::log_info("create vk::SwapchainCreateInfoKHR successfully!");
        }

        swapchain_images = logical_device.getSwapchainImagesKHR(swapchain);
        swapchain_image_format = swapChainCreateInfo.imageFormat;
        swapchain_extent = swapChainCreateInfo.imageExtent;
    }

    void createImageViews() {
        swapchain_imageviews.resize(swapchain_images.size());
        for (std::size_t i = 0; i < swapchain_images.size(); ++i) {
            vk::ImageViewCreateInfo viewCreateInfo {};
            //viewCreateInfo.flags = ;
            viewCreateInfo.image = swapchain_images[i];
            viewCreateInfo.viewType = vk::ImageViewType::e2D;
            viewCreateInfo.format = swapchain_image_format;

            vk::ComponentMapping componentMappingInfo {};
            componentMappingInfo.r = vk::ComponentSwizzle::eIdentity;
            componentMappingInfo.g = vk::ComponentSwizzle::eIdentity;
            componentMappingInfo.b = vk::ComponentSwizzle::eIdentity;
            componentMappingInfo.a = vk::ComponentSwizzle::eIdentity;
            viewCreateInfo.components = componentMappingInfo;

            vk::ImageSubresourceRange imageSubresourceRange {};
            imageSubresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            imageSubresourceRange.baseMipLevel = 0;
            imageSubresourceRange.levelCount = 1;
            imageSubresourceRange.baseArrayLayer = 0;
            imageSubresourceRange.layerCount = 1;
            viewCreateInfo.subresourceRange = imageSubresourceRange;

            if (logical_device.createImageView(
                    &viewCreateInfo,
                    nullptr,
                    &swapchain_imageviews[i]) != vk::Result::eSuccess
            ) {
                minilog::log_fatal("failed to create vk::ImageView!");
            } else {
                minilog::log_info("create vk::ImageView successfully!");
            }
        }
    }

    void createRenderPass() {
        vk::AttachmentDescription colorAttachment {};
        //colorAttachment.flags = ;
        colorAttachment.format = swapchain_image_format;
        colorAttachment.samples = vk::SampleCountFlagBits::e1;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference colorAttachmentRef {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpass {};
        //subpass.flags = ;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        vk::SubpassDependency dependency {};
        dependency.srcSubpass = vk::SubpassExternal;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        // dependency.srcAccessMask = 0;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        //dependency.dependencyFlags = ;

        vk::RenderPassCreateInfo renderPassInfo {};
        //renderPassInfo.flags = ;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (logical_device.createRenderPass(&renderPassInfo, nullptr, &render_pass) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::RenderPass!");
        } else {
            minilog::log_info("create vk::RenderPass successfully!");
        }
    }

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
        } else {
            minilog::log_info("create vk::ShaderModule successfully!");
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
        graphicsPipelineCreateInfo.render_pass = render_pass;
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
        } else {
            minilog::log_info("create vk::Pipeline successfully!");
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
            framebufferInfo.render_pass = render_pass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapchain_extent.width;
            framebufferInfo.height = swapchain_extent.height;
            framebufferInfo.layers = 1;

            if (logical_device.createFramebuffer(&framebufferInfo, nullptr, &swapchain_framebuffers[i]) != vk::Result::eSuccess) {
                minilog::log_fatal("failed to create vk::Framebuffer!");
            }
        }
        minilog::log_info("create vk::Framebuffer successfully!");
    }

    void createCommandPool() {
        QueueFamilyIndex queueFamilyIndices = findQueueFamilies(physicalDevice);

        vk::CommandPoolCreateInfo poolInfo {};
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphic.value();

        if (logical_device.createCommandPool(&poolInfo, nullptr, &command_pool) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::CommandPool!");
        } else {
            minilog::log_info("create vk::CommandPool successfully!");
        }
    }

    void createCommandBuffer() {
        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        vk::CommandBufferAllocateInfo allocInfo {};
        allocInfo.command_pool = command_pool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = static_cast<std::uint32_t>(command_buffers.size());

        if (logical_device.allocateCommandBuffers(&allocInfo, command_buffers.data()) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::CommandBuffer!");
        } else {
            minilog::log_info("create vk::CommandBuffer successfully!");
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

    QueueFamilyIndex findQueueFamilies(vk::PhysicalDevice physicalDevice_) {
        std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice_.getQueueFamilyProperties();

        std::uint32_t i { 0u };
        QueueFamilyIndex indices {};
        for (const vk::QueueFamilyProperties& queueFamily : queueFamilies) {
            if (static_cast<std::uint32_t>(queueFamily.queueFlags)
                & static_cast<std::uint32_t>(vk::QueueFlagBits::eGraphics)
            ) {
                indices.graphic = i;
            }

            vk::Bool32 isPresentSupport = false;
            physicalDevice_.getSurfaceSupportKHR(i, surface, &isPresentSupport);
            if (isPresentSupport) {
                indices.present = i;
            }

            if (indices.has_value()) { break; }
            ++i;
        }

        return indices;
    }

    SwapChainSupportDetail querySwapChainSupport(vk::PhysicalDevice physicalDevice_) {
        SwapChainSupportDetail details {};
        physicalDevice_.getSurfaceCapabilitiesKHR(surface, &details.surface_capabilities);

        details.surface_formats = physicalDevice_.getSurfaceFormatsKHR(surface);
        details.present_modes = physicalDevice_.getSurfacePresentModesKHR(surface);

        return details;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& avaiableFormats
    ) {
        for (const vk::SurfaceFormatKHR& availableFormat : avaiableFormats) {
            if (availableFormat.format == vk::Format::eB8G8R8A8Unorm
                && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear
            ) {
                return availableFormat;
            }
        }

        return avaiableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(
        const std::vector<vk::PresentModeKHR>& avaiablePresentModes
    ) {
        for (const auto& avaiablePresentMode : avaiablePresentModes) {
            if (avaiablePresentMode == vk::PresentModeKHR::eMailbox) {
                return avaiablePresentMode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            vk::Extent2D actualExtent {};
            actualExtent.width = std::max(
                capabilities.minImageExtent.width,
                std::min(capabilities.maxImageExtent.width, actualExtent.width)
            );
            actualExtent.height = std::max(
                capabilities.minImageExtent.height,
                std::min(capabilities.maxImageExtent.height, actualExtent.height)
            );

            return actualExtent;
        }
    }

    std::vector<const char*> get_required_extensions() {
        std::uint32_t glfw_extension_count { 0u };
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char*> instance_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
        if (ENABLE_VALIDATION_LAYER) { instance_extensions.push_back(vk::EXTDebugUtilsExtensionName); }

        return instance_extensions;
    }

    bool checkPhysicalDeviceExtensionSupport(vk::PhysicalDevice physicalDevice_) {
        std::vector<vk::ExtensionProperties> availableExtensions =
            physicalDevice_.enumerateDeviceExtensionProperties(nullptr);
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        return requiredExtensions.empty();
    }

    bool isPhysicalDeviceSuitable(vk::PhysicalDevice physicalDevice_) {
        QueueFamilyIndex indices = findQueueFamilies(physicalDevice_);
        bool extensionsSupported = checkPhysicalDeviceExtensionSupport(physicalDevice_);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetail swapChainSupport = querySwapChainSupport(physicalDevice_);
            swapChainAdequate = (!swapChainSupport.surface_formats.empty())
                                && (!swapChainSupport.present_modes.empty());
        }

        vk::PhysicalDeviceFeatures supportedFeatures {};// why somting are vkCreate others are vkGet
        physicalDevice_.getFeatures(&supportedFeatures);

        return indices.has_value()
                && extensionsSupported
                && swapChainAdequate
                && supportedFeatures.samplerAnisotropy;// specifies whether anisotropic filtering is supported
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

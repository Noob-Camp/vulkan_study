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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include <minilog.hpp>

using namespace std::literals::string_literals;


#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };


struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value()
                && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};


struct Application {
    uint32_t width { 800u };
    uint32_t height { 600u };
    const std::string windowName { "reCreate the Swap Chain"s };
    GLFWwindow* glfwWindow { nullptr };

    vk::Instance instance { nullptr };
    bool validationLayersSupported = false;
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    vk::DebugUtilsMessengerEXT debugMessenger { nullptr };

    vk::PhysicalDevice physicalDevice { nullptr };
    vk::Device logicalDevice { nullptr };
    vk::Queue graphicsQueue { nullptr };
    vk::Queue presentQueue { nullptr };

    vk::SurfaceKHR surface { nullptr };
    vk::SwapchainKHR swapChain { nullptr };
    vk::Format swapChainImageFormat;
    vk::Extent2D swapChainExtent {};

    std::vector<vk::Image> swapChainImages;
    std::vector<vk::ImageView> swapChainImageViews;
    std::vector<vk::Framebuffer> swapChainFramebuffers;

    vk::RenderPass renderPass { nullptr };
    vk::PipelineLayout renderPipelineLayout { nullptr };
    vk::Pipeline graphicsPipeline { nullptr };

    vk::CommandPool commandPool { nullptr };
    std::vector<vk::CommandBuffer> commandBuffers;

    std::vector<vk::Semaphore> imageAvailableSemaphores;
    std::vector<vk::Semaphore> renderFinishedSemaphores;
    std::vector<vk::Fence> inFlightFences;
    uint32_t currentFrame { 0u };
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT { 2u };
    bool framebufferResized = false;

    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanUp();
    }

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        glfwWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
        if (glfwWindow == nullptr) {
            minilog::log_fatal("GLFW Failed to create GLFWwindow!");
        } else {
            minilog::log_info("GLFW Create GLFWwindow Successfully!");
        }
        glfwSetWindowUserPointer(glfwWindow, this);
        glfwSetFramebufferSizeCallback(glfwWindow, framebufferResizeCallback);// 可不可以写成 lambda 表达式
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
        app->width =  width;
        app->height = height;
        minilog::log_info("the window's size is ({0}, {1})", width, height);
    }

    void initVulkan() {
        checkValidationLayerSupport();
        populateDebugUtilsMessengerCreateInfoEXT();
        createInstance();
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

    void mainLoop() {
        while (!glfwWindowShouldClose(glfwWindow)) {
            glfwPollEvents();
            drawFrame();
        }
        logicalDevice.waitIdle();
    }

    void cleanupSwapChain() {
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
        }

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(logicalDevice, imageView, nullptr);
        }

        vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
    }

    void cleanUp() {
        cleanupSwapChain();

        vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(logicalDevice, renderPipelineLayout, nullptr);

        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

        for (size_t i { 0u }; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

        vkDestroyDevice(logicalDevice, nullptr);

        // if (enableValidationLayers) {
        //     instance.destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        // }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(glfwWindow);
        glfwTerminate();
    }

    void recreateSwapChain() {
        int width { 0u }, height { 0u };
        glfwGetFramebufferSize(glfwWindow, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(glfwWindow, &width, &height);
            glfwWaitEvents();
        }

        logicalDevice.waitIdle();

        cleanupSwapChain();
        createSwapChain();
        createImageViews();
        createFrameBuffers();
    }

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo {
            .pInheritanceInfo = nullptr
        };

        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to begin recording command buffer!");
        } else {
            minilog::log_info("begin recording command buffer!");
        }

        vk::RenderPassBeginInfo renderPassBeginInfo {};
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = swapChainFramebuffers[imageIndex];
        vk::Rect2D renderArea {};
        renderArea.offset.setX(0);
        renderArea.offset.setY(0);
        renderArea.extent = swapChainExtent;
        renderPassBeginInfo.renderArea = renderArea;
        vk::ClearValue clearColor {
            .color {
                std::array<float, 4>{ 0.2f, 0.3f, 0.3f, 1.0f }
            }
        };
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearColor;
        commandBuffer.beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
            vk::Viewport viewport {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapChainExtent.width);
            viewport.height = static_cast<float>(swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            commandBuffer.setViewport(0, 1, &viewport);

            vk::Rect2D scissor{
                .offset { .x = 0, .y = 0 },
                .extent = swapChainExtent
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

    void drawFrame() {
        logicalDevice.waitForFences(1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex { 0u };
        vk::Result result = logicalDevice.acquireNextImageKHR(swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], nullptr, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            minilog::log_fatal("failed to acquire swap chain image!");
        }
        logicalDevice.resetFences(1, &inFlightFences[currentFrame]);

        commandBuffers[currentFrame].reset();
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        vk::Semaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::SubmitInfo submitInfo {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[currentFrame]
        };


        vk::Semaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (graphicsQueue.submit(1, &submitInfo, inFlightFences[currentFrame]) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to submit draw command buffer!");
        } else {
            minilog::log_info("submit draw command buffer successfully!");
        }

        vk::PresentInfoKHR presentInfo {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores
        };


        vk::SwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;
        result = presentQueue.presentKHR(&presentInfo);

        if (result == vk::Result::eErrorOutOfDateKHR
                        || result == vk::Result::eSuboptimalKHR
                        || framebufferResized
        ) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != vk::Result::eSuccess) {
            minilog::log_fatal("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    VKAPI_ATTR VKAPI_CALL
    static vk::Bool32 debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
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
        if (!enableValidationLayers) { return; }
        vk::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::DispatchLoaderDynamic dispatch(instance, getInstanceProcAddr);
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


    void checkValidationLayerSupport() {
        std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
        for (const char* layerName : validationLayers) {
            for (const vk::LayerProperties& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    validationLayersSupported = true;
                    minilog::log_info("the {} is supported!", layerName);
                    break;
                }
            }
            if (validationLayersSupported) { break; }
        }

        if (enableValidationLayers && (!validationLayersSupported)) {
            minilog::log_fatal("validation layers requested, but not available!");
        }
    }

    void populateDebugUtilsMessengerCreateInfoEXT() {
        debugCreateInfo.flags = vk::DebugUtilsMessengerCreateFlagsEXT();
        debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugCreateInfo.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debugCallback);
        debugCreateInfo.pUserData = nullptr;// optional
    }

    void createInstance() {
        vk::ApplicationInfo appInfo {
            .pApplicationName = "reCreate the Swap Chain",
            .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 3, 0),
            .apiVersion = VK_API_VERSION_1_3
        };

        vk::InstanceCreateInfo instanceCreateInfo {
            .pApplicationInfo = &appInfo
        };

        if (enableValidationLayers) {
            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            instanceCreateInfo.enabledLayerCount = 0u;
            instanceCreateInfo.ppEnabledLayerNames = nullptr;
        }

        std::vector<const char*> extensions = getRequiredExtensions();
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

        if (vk::createInstance(&instanceCreateInfo, nullptr, &instance) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::Instance!");
        } else {
            minilog::log_info("create vk::Instance successfully!");
        }
    }


    void createSurface() {
        if (glfwCreateWindowSurface(
                static_cast<VkInstance>(instance),
                glfwWindow,
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
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        float queuePriority { 1.0f };
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            vk::DeviceQueueCreateInfo queueCreateInfo {};
            // queueCreateInfo.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        vk::DeviceCreateInfo deviceCreateInfo {};
        // createInfo.flags = ;// flags is reserved for future use
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

        if (enableValidationLayers) {
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            deviceCreateInfo.enabledLayerCount = 0;
            deviceCreateInfo.ppEnabledLayerNames = nullptr;
        }

        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        vk::PhysicalDeviceFeatures physicalDeviceFeatures {};
        physicalDeviceFeatures.samplerAnisotropy = vk::Bool32(VK_TRUE);
        deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

        if (physicalDevice.createDevice(&deviceCreateInfo, nullptr, &logicalDevice) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create logical device!");
        } else {
            minilog::log_info("create logical device successfully!");
        }

        logicalDevice.getQueue(indices.graphicsFamily.value(), 0, &graphicsQueue);
        logicalDevice.getQueue(indices.presentFamily.value(), 0, &presentQueue);
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;// realization of triple buffer
        if ((swapChainSupport.capabilities.maxImageCount > 0)
            && (imageCount > swapChainSupport.capabilities.maxImageCount)
        ) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
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

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };
        if (indices.graphicsFamily != indices.presentFamily) {
            swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            swapChainCreateInfo.queueFamilyIndexCount = 2;
            swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
            swapChainCreateInfo.queueFamilyIndexCount = 0;
            swapChainCreateInfo.pQueueFamilyIndices = nullptr;
        }

        swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapChainCreateInfo.presentMode = presentMode;
        swapChainCreateInfo.clipped = vk::Bool32(VK_TRUE);
        swapChainCreateInfo.oldSwapchain = nullptr;

        if (logicalDevice.createSwapchainKHR(&swapChainCreateInfo, nullptr, &swapChain) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::SwapchainCreateInfoKHR!");
        } else {
            minilog::log_info("create vk::SwapchainCreateInfoKHR successfully!");
        }

        swapChainImages = logicalDevice.getSwapchainImagesKHR(swapChain);
        swapChainImageFormat = swapChainCreateInfo.imageFormat;
        swapChainExtent = swapChainCreateInfo.imageExtent;
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); ++i) {
            vk::ImageViewCreateInfo viewCreateInfo {};
            //viewCreateInfo.flags = ;
            viewCreateInfo.image = swapChainImages[i];
            viewCreateInfo.viewType = vk::ImageViewType::e2D;
            viewCreateInfo.format = swapChainImageFormat;

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

            if (logicalDevice.createImageView(
                    &viewCreateInfo,
                    nullptr,
                    &swapChainImageViews[i]) != vk::Result::eSuccess
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
        colorAttachment.format = swapChainImageFormat;
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

        if (logicalDevice.createRenderPass(&renderPassInfo, nullptr, &renderPass) != vk::Result::eSuccess) {
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

        size_t fileSize = static_cast<size_t>(file.tellg());
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
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        vk::ShaderModule shaderModule;
        if (logicalDevice.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::ShaderModule");
        } else {
            minilog::log_info("create vk::ShaderModule successfully!");
        }

        return shaderModule;
    }

    void createGraphicsPipeline() {
        std::vector<char> vertCode = readFile("../../src/Triangle/shaders/vert.spv");
        std::vector<char> fragCode = readFile("../../src/Triangle/shaders/frag.spv");
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
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor {};
        scissor.offset.setX(0);
        scissor.offset.setY(0);
        scissor.extent = swapChainExtent;

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
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateInfo.pDynamicStates = dynamicStates.data();
        graphicsPipelineCreateInfo.pDynamicState = &dynamicStateInfo;///

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        if (logicalDevice.createPipelineLayout(
            &pipelineLayoutInfo,
            nullptr,
            &renderPipelineLayout) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::PipelineLayout!");
        }

        graphicsPipelineCreateInfo.layout = renderPipelineLayout;
        graphicsPipelineCreateInfo.renderPass = renderPass;
        graphicsPipelineCreateInfo.subpass = 0;
        graphicsPipelineCreateInfo.basePipelineHandle = nullptr;
        graphicsPipelineCreateInfo.basePipelineIndex = -1;

        if (logicalDevice.createGraphicsPipelines(
            nullptr,
            1,
            &graphicsPipelineCreateInfo,
            nullptr,
            &graphicsPipeline) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::Pipeline!");
        } else {
            minilog::log_info("create vk::Pipeline successfully!");
        }

        logicalDevice.destroyShaderModule(vertShaderModule, nullptr);
        logicalDevice.destroyShaderModule(fragShaderModule, nullptr);
    }

    void createFrameBuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i { 0u }; i < swapChainImageViews.size(); ++i) {
            vk::ImageView attachments[] = { swapChainImageViews[i] };
            vk::FramebufferCreateInfo framebufferInfo {};
            //framebufferInfo.flages = ;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (logicalDevice.createFramebuffer(&framebufferInfo, nullptr, &swapChainFramebuffers[i]) != vk::Result::eSuccess) {
                minilog::log_fatal("failed to create vk::Framebuffer!");
            }
        }
        minilog::log_info("create vk::Framebuffer successfully!");
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        vk::CommandPoolCreateInfo poolInfo {};
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (logicalDevice.createCommandPool(&poolInfo, nullptr, &commandPool) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::CommandPool!");
        } else {
            minilog::log_info("create vk::CommandPool successfully!");
        }
    }

    void createCommandBuffer() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        vk::CommandBufferAllocateInfo allocInfo {};
        allocInfo.commandPool = commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (logicalDevice.allocateCommandBuffers(&allocInfo, commandBuffers.data()) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::CommandBuffer!");
        } else {
            minilog::log_info("create vk::CommandBuffer successfully!");
        }
    }


    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        vk::SemaphoreCreateInfo semaphoreInfo {};
        //semaphoreInfo.flags = ;

        vk::FenceCreateInfo fenceInfo {};
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (size_t i { 0 }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (logicalDevice.createSemaphore(&semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != vk::Result::eSuccess
                || logicalDevice.createSemaphore(&semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != vk::Result::eSuccess
                || logicalDevice.createFence(&fenceInfo, nullptr, &inFlightFences[i]) != vk::Result::eSuccess
            ) {
                minilog::log_fatal("failed to create synchronization objects for a frame!");
            }
        }
    }

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice physicalDevice_) {
        std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice_.getQueueFamilyProperties();

        uint32_t i { 0u };
        QueueFamilyIndices indices {};
        for (const vk::QueueFamilyProperties& queueFamily : queueFamilies) {
            if (static_cast<uint32_t>(queueFamily.queueFlags)
                & static_cast<uint32_t>(vk::QueueFlagBits::eGraphics)
            ) {
                indices.graphicsFamily = i;
            }

            vk::Bool32 isPresentSupport = false;
            physicalDevice_.getSurfaceSupportKHR(i, surface, &isPresentSupport);
            if (isPresentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) { break; }
            ++i;
        }

        return indices;
    }

    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice physicalDevice_) {
        SwapChainSupportDetails details {};
        physicalDevice_.getSurfaceCapabilitiesKHR(surface, &details.capabilities);

        details.formats = physicalDevice_.getSurfaceFormatsKHR(surface);
        details.presentModes = physicalDevice_.getSurfacePresentModesKHR(surface);

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
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
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

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount { 0u };
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
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
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
        bool extensionsSupported = checkPhysicalDeviceExtensionSupport(physicalDevice_);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice_);
            swapChainAdequate = (!swapChainSupport.formats.empty())
                                && (!swapChainSupport.presentModes.empty());
        }

        vk::PhysicalDeviceFeatures supportedFeatures {};// why somting are vkCreate others are vkGet
        physicalDevice_.getFeatures(&supportedFeatures);

        return indices.isComplete()
                && extensionsSupported
                && swapChainAdequate
                && supportedFeatures.samplerAnisotropy;// specifies whether anisotropic filtering is supported
    }
};




int main() {
    Application app {};

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
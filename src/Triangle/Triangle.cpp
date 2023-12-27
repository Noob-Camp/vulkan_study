#include <iostream>
#include <vector>
#include <string>
#include <cstring>// for strcmp
#include <optional>
#include <set>
#include <limits>
#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <minilog.hpp>


using namespace std::literals::string_literals;


#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif


struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value()
                && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;

    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};




class Triangle {
    GLFWwindow* glfwWindow;
    size_t width { 800 };
    size_t height { 600 };
    std::string windowName { "Hello Triangle"s };
    bool framebufferResized { false };

    VkInstance instance { VK_NULL_HANDLE };
    bool validationLayersSupported = false;
    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    VkDebugUtilsMessengerEXT debugMessenger { VK_NULL_HANDLE };

    VkPhysicalDevice physicalDevice { VK_NULL_HANDLE };
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDevice logicalDevice { VK_NULL_HANDLE };
    VkQueue graphicsQueue { VK_NULL_HANDLE };
    VkQueue presentQueue { VK_NULL_HANDLE }; 

    VkSurfaceKHR surface { VK_NULL_HANDLE };
    VkSwapchainKHR swapChain { VK_NULL_HANDLE };
    VkFormat swapChainImageFormat;
    VkExtent2D windowExtent {};
    VkExtent2D swapChainExtent {};

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass { VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout { VK_NULL_HANDLE };
    VkPipeline graphicsPipeline { VK_NULL_HANDLE };

    VkCommandPool commandPool { VK_NULL_HANDLE };
    std::vector<VkCommandBuffer> commandBuffers;

    // to avoid synchronization interference, create semaphores for each frame
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame { 0 };
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT { 2 };// number of frames for parallel processing

public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
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
        glfwSetFramebufferSizeCallback(glfwWindow, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Triangle*>(glfwGetWindowUserPointer(window));
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
        vkDeviceWaitIdle(logicalDevice);
    }

    void cleanup() {
        for (size_t i { 0 }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
        }

        vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(logicalDevice, imageView, nullptr);
        }

        vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
        vkDestroyDevice(logicalDevice, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(glfwWindow);

        glfwTerminate();
    }




    // core functions
    void createInstance() {
        VkApplicationInfo appInfo {};
        
        //* pNext is a const void* type, so it can point to an arbitrary structure,
        //  thus enabling extensions to the members of that structure
        //* However, since the compiler cannot determine the exact type that the void pointer is pointing to,
        // you need to explicitly type convert when assigning a value to pNetxt,
        // and you need to be careful when using the converted pointer to ensure type safety and correctness

        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = "Triangle App";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instanceCreateInfo {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        instanceCreateInfo.pApplicationInfo = &appInfo;

        if (enableValidationLayers) {
            instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo; 
            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            instanceCreateInfo.pNext = nullptr;
            instanceCreateInfo.enabledLayerCount = 0u;
            instanceCreateInfo.ppEnabledLayerNames = nullptr;
        }

        std::vector<const char*> extensions = getRequiredExtensions();
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkInstance!");
        } else {
            minilog::log_info("create VkInstance successfully!");
        }
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) { return; }
        if (CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            minilog::log_fatal("failed to set up debug messenger!");
        }
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkSurfaceKHR!");
        } else {
            minilog::log_info("create VkSurfaceKHR successfully!");
        }
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount { 0u };
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            minilog::log_fatal("failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const VkPhysicalDevice& device : devices) {
            if (isPhysicalDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            minilog::log_fatal("failed to find a suitable GPU!");
        } else {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);
            minilog::log_info("physical device: {}", properties.deviceName);
        }
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        float queuePriority { 1.0f };
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.pNext = nullptr;
            // queueCreateInfo.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkDeviceCreateInfo deviceCreateInfo {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = nullptr;
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

        VkPhysicalDeviceFeatures physicalDeviceFeatures {};
        physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
            minilog::log_fatal("failed to create logical device!");
        } else {
            minilog::log_info("create logical device successfully!");
        }

        // the third argument is queueIndex
        vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;// realization of triple buffer
        if ((swapChainSupport.capabilities.maxImageCount > 0)
            && (imageCount > swapChainSupport.capabilities.maxImageCount)
        ) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR swapChainCreateInfo {};
        swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapChainCreateInfo.pNext = nullptr;
        //createInfo.flags =;
        swapChainCreateInfo.surface = surface;
        swapChainCreateInfo.minImageCount = imageCount;
        swapChainCreateInfo.imageFormat = surfaceFormat.format;
        swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapChainCreateInfo.imageExtent = extent;
        //* each swap chain image can consist of multiple layers,
        //  which can be useful in certain situations,
        //  such as when performing multi-view rendering or stereo rendering
        //* each layer can be manipulated individually during the rendering process,
        //  e.g. by setting up different viewports, clipping areas, etc.
        swapChainCreateInfo.imageArrayLayers = 1;// specify the levels contained in each image
        // specifies what operations will be performed on the image
        swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;// draw operator

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };
        if (indices.graphicsFamily != indices.presentFamily) {
            swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapChainCreateInfo.queueFamilyIndexCount = 2;
            swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapChainCreateInfo.queueFamilyIndexCount = 0;
            swapChainCreateInfo.pQueueFamilyIndices = nullptr;
        }

        swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapChainCreateInfo.presentMode = presentMode;
        swapChainCreateInfo.clipped = VK_TRUE;
        swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(logicalDevice, &swapChainCreateInfo, nullptr, &swapChain) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkSwapchainCreateInfoKHR!");
        } else {
            minilog::log_info("create VkSwapchainCreateInfoKHR successfully!");
        }

        //* to obtain the array of presentable images associated with a swapchain
        //* these images are actually created automatically by the Vulkan implementation (usually the graphics card driver)
        //* the specific creation and management of swap chain images is done by the Vulkan implementation without direct intervention by the developer
        vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = swapChainCreateInfo.imageFormat;
        swapChainExtent = swapChainCreateInfo.imageExtent;
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); ++i) {
            VkImageViewCreateInfo viewCreateInfo {};
            viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCreateInfo.pNext = nullptr;
            //viewCreateInfo.flags = ;
            viewCreateInfo.image = swapChainImages[i];
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = swapChainImageFormat;

            //* controls how each color component is mapped in the image view
            //* color components of an image view can be rearranged, replaced or masked to meet specific needs
            VkComponentMapping componentMappingInfo {};
            // indicates that the color component does not undergo any mapping or rearrangement and remains as is
            componentMappingInfo.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            componentMappingInfo.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            componentMappingInfo.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            componentMappingInfo.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.components = componentMappingInfo;

            // set of mipmap levels and array layers for selecting image view accessibility
            VkImageSubresourceRange imageSubresourceRange {};
            imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageSubresourceRange.baseMipLevel = 0;
            imageSubresourceRange.levelCount = 1;
            imageSubresourceRange.baseArrayLayer = 0;
            imageSubresourceRange.layerCount = 1;
            viewCreateInfo.subresourceRange = imageSubresourceRange;

            if (vkCreateImageView(
                    logicalDevice,
                    &viewCreateInfo,
                    nullptr,
                    &swapChainImageViews[i]) != VK_SUCCESS
            ) {
                minilog::log_fatal("failed to create VkImageView!");
            } else {
                minilog::log_info("create VkImageView successfully!");
            }
        }
    }

    // specifies the color and depth buffers used,
    // as well as the number of samples,
    // and how the rendering operation handles the contents of the buffer
    void createRenderPass() {
        VkAttachmentDescription colorAttachment {};
        //colorAttachment.flags = ;
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // defining the color attachments for this subpass and their layouts
        VkAttachmentReference colorAttachmentRef {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // VkAttachmentDescription depthAttachment {};
        // depthAttachment.format = findDepthFormat();
        // depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        // depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // VkAttachmentReference depthAttachmentRef{};
        // depthAttachmentRef.attachment = 1;
        // depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        //subpass.flags = ;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        //* 输入附件是在渲染通道中作为输入使用的图像或缓冲附件
        //  它们可以是在之前的子通道中进行渲染的输出结果，或者是外部传入的图像数据
        //* 子通道可以使用输入附件来进行采样、读取或其他操作，以实现渲染操作的输入需求
        //* 在使用 VkSubpassDescription 时，通常需要为每个输入附件创建一个 VkAttachmentReference 结构体，并将这些结构体放入 inputAttachments 数组中
        //* 每个 VkAttachmentReference 结构体描述了一个输入附件的索引和图像布局
        //* 通过设置 inputAttachmentCount 的值，并正确配置 inputAttachments 数组，可以定义和管理子通道中的输入附件，以满足渲染操作对输入数据的需求
        //* 需要注意的是，inputAttachmentCount 的值必须小于或等于 VkRenderPassCreateInfo 结构体中 attachmentCount 的值，以确保索引不会超出附件数组的范围
        //subpass.inputAttachmentCount = ;
        //subpass.pinputAttachments = ;
        //* the index of the color attachment set here in the array will be used by the fragment shader,
        //  the corresponding statement is layout(location = 0) out vec4 outColor
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        // 解析附件（Resolve Attachment）是指在渲染通道中使用多重采样的附件后，将其解析（Resolve）为普通的非多重采样附件的过程
        // 多重采样附件是指在渲染通道中进行多重采样操作的附件，它们具有更高的采样数，以提高图像质量和平滑度
        // 然而，多重采样附件的数据在进行后续处理或显示时可能会受到限制，因此需要将其解析为普通的非多重采样附件
        // subpass.pResolveAttachments = ;
        // subpass.pDepthStencilAttachment = ;
        // subpass.preserveAttachmentCount = ;// 保留附件
        // subpass.pPreserveAttachments = ;

        // RenderPass 的 Subpass 会自动进行图像布局变换
        // 用于描述子传递之间的依赖关系。它定义了一个子传递与其他子传递或外部操作之间的同步关系
        VkSubpassDependency dependency {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;// 为了避免出现循环依赖，我们给 dstSubpass 设置的值必须始终大于 srcSubpass
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        //dependency.dependencyFlags = ;

        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.pNext = nullptr;
        // flags 成员的作用是允许开发者在创建对象时进行一些自定义设置或配置
        // 通过设置不同的标志位，可以改变对象的行为、性能或其他方面的属性
        //renderPassInfo.flags = ;
        renderPassInfo.attachmentCount = 1;// the number of attachments used by this render pass
        // a pointer to an array of attachmentCount VkAttachmentDescription structures describing the attachments used by the render pass
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;// the number of memory dependencies between pairs of subpasses.
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkRenderPass!");
        } else {
            minilog::log_info("create VkRenderPass successfully!");
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

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = nullptr;
        //createInfo.flags = ;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkShaderModule");
        } else {
            minilog::log_info("create VkShaderModule successfully!");
        }

        return shaderModule;
    }

    void createGraphicsPipeline() {
        std::vector<char> vertCode = readFile("../../src/Triangle/shaders/vert.spv");
        std::vector<char> fragCode = readFile("../../src/Triangle/shaders/frag.spv");
        VkShaderModule vertShaderModule = createShaderModule(vertCode);
        VkShaderModule fragShaderModule = createShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.pNext = nullptr;
        // vertShaderStageInfo.flags = 0;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;// specifying a single pipeline stage
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        vertShaderStageInfo.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.pNext = nullptr;
        // fragShaderStageInfo.flags = 0;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        fragShaderStageInfo.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertShaderStageInfo,
            fragShaderStageInfo
        };

        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo {};// begin
        graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineCreateInfo.pNext = nullptr;
        //graphicsPipelineCreateInfo.flags = ;
        graphicsPipelineCreateInfo.stageCount = 2;
        graphicsPipelineCreateInfo.pStages = shaderStages;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.pNext = nullptr;
        //vertexInputInfo.flags = ;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        graphicsPipelineCreateInfo.pVertexInputState = &vertexInputInfo;///

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {};
        inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;
        graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateInfo;///

        VkPipelineTessellationStateCreateInfo tessellationStateInfo {};
        graphicsPipelineCreateInfo.pTessellationState = &tessellationStateInfo;///

        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor {};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;

        VkPipelineViewportStateCreateInfo viewportInfo {};
        viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportInfo.pNext = nullptr;
        //viewportInfo.flags = ;
        viewportInfo.viewportCount = 1;
        viewportInfo.pViewports = &viewport;
        viewportInfo.scissorCount = 1;
        viewportInfo.pScissors = &scissor;
        graphicsPipelineCreateInfo.pViewportState = &viewportInfo;///

        VkPipelineRasterizationStateCreateInfo rasterizationStateInfo {};
        rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateInfo.depthClampEnable = VK_FALSE;
        rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateInfo.lineWidth = 1.0f;
        rasterizationStateInfo.cullMode = VK_FRONT_FACE_CLOCKWISE;
        rasterizationStateInfo.depthBiasEnable = VK_FALSE;
        graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateInfo;///

        VkPipelineMultisampleStateCreateInfo multisampleStateInfo {};
        multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateInfo.sampleShadingEnable = VK_FALSE;
        multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateInfo;///

        VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo {};
        depthStencilStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateInfo.pNext = nullptr;
        //depthStencilStateInfo.flags = 
        depthStencilStateInfo.depthTestEnable = VK_TRUE;
        depthStencilStateInfo.depthWriteEnable = VK_TRUE;
        depthStencilStateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilStateInfo.stencilTestEnable = VK_FALSE;
        depthStencilStateInfo.front = {};// Optional
        depthStencilStateInfo.back = {};// Optional
        depthStencilStateInfo.minDepthBounds = 0.0f;// Optional
        depthStencilStateInfo.maxDepthBounds = 1.0f;// Optional
        graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateInfo;///

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.blendEnable = VK_FALSE;
        // colorBlendAttachment.srcColorBlendFactor = 
        // colorBlendAttachment.dstColorBlendFactor = 
        // colorBlendAttachment.colorBlendOp =
        // colorBlendAttachment.srcAlphaBlendFactor =
        // colorBlendAttachment.dstAlphaBlendFactor =
        // colorBlendAttachment.alphaBlendOp =
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                                | VK_COLOR_COMPONENT_G_BIT
                                                | VK_COLOR_COMPONENT_B_BIT
                                                | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendStateInfo {};
        colorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateInfo.pNext = nullptr;
        //colorBlendStateInfo.flags = 
        colorBlendStateInfo.logicOpEnable = VK_FALSE;
        colorBlendStateInfo.logicOp = VK_LOGIC_OP_COPY;
        colorBlendStateInfo.attachmentCount = 1;
        colorBlendStateInfo.pAttachments = &colorBlendAttachment;
        colorBlendStateInfo.blendConstants[0] = 0.0f;// R
        colorBlendStateInfo.blendConstants[1] = 0.0f;// G
        colorBlendStateInfo.blendConstants[2] = 0.0f;// B
        colorBlendStateInfo.blendConstants[3] = 0.0f;// A
        graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateInfo;///

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicStateInfo {};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.pNext = nullptr;
        //dynamicStateInfo.flags =;
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateInfo.pDynamicStates = dynamicStates.data();
        graphicsPipelineCreateInfo.pDynamicState = &dynamicStateInfo;///

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkPipelineLayout!");
        }

        graphicsPipelineCreateInfo.layout = pipelineLayout;
        graphicsPipelineCreateInfo.renderPass = renderPass;
        graphicsPipelineCreateInfo.subpass = 0;
        graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
        graphicsPipelineCreateInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkPipeline!");
        } else {
            minilog::log_info("create VkPipeline successfully!");
        }

        vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
        vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
    }

    // 在创建渲染流程对象时指定使用的附着需要绑定在帧缓冲对象上使用
    void createFrameBuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i { 0u }; i < swapChainImageViews.size(); ++i) {
            VkImageView attachments[] = { swapChainImageViews[i] };
            VkFramebufferCreateInfo framebufferInfo {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.pNext = nullptr;
            //framebufferInfo.flages = ;
            // 定义帧缓冲区将与哪些渲染过程兼容
            // 一般来说，使用相同数量，相同类型附着的渲染流程对象是相兼容的
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;// 每个句柄都将用作渲染过程实例中的相应附件
            // width, height and layers define the dimensions of the framebuffer
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                minilog::log_fatal("failed to create VkFramebuffer!");
            }
        }
        minilog::log_info("create VkFramebuffer successfully!");
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.pNext = nullptr;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        // 从特定命令池分配的所有命令缓冲区都必须在同一队列族的队列上提交
        // 在这里，我们使用的是绘制指令，它可以被提交给支持图形操作的队列
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkCommandPool!");
        } else {
            minilog::log_info("create VkCommandPool successfully!");
        }
    }

    void createCommandBuffer() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;// 可以被提交到队列进行执行，但不能被其它指令缓冲对象调用
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            minilog::log_fatal("failed to create VkCommandBuffer!");
        } else {
            minilog::log_info("create VkCommandBuffer successfully!");
        }
    }

    //* 栅栏（fence）来对应用程序本身和渲染操作进行同步
    //* 使用信号量（semaphore）来对一个指令队列内的操作或多个不同指令队列的操作进行同步
    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.pNext = nullptr;
        //semaphoreInfo.flags = ;

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.pNext = nullptr;
        // specifies that the fence object is created in the signaled state
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i { 0 }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
                || vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS
                || vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS
            ) {
                minilog::log_fatal("failed to create synchronization objects for a frame!");
            }
        }
    }




    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo {};// 指定一些有关指令缓冲的使用细节
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;// 指定我们将要怎样使用指令缓冲
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;// 在指令缓冲等待执行时，仍然可以提交这一指令缓冲(使得我们可以在上一帧还未结束渲染时，提交下一帧的渲染指令)
        beginInfo.pInheritanceInfo = nullptr;// 只用于辅助指令缓冲，可以用它来指定从调用它的主要指令缓冲继承的状态
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {// 该函数会重置指令缓冲对象
            minilog::log_fatal("failed to begin recording command buffer!");
        } else {
            minilog::log_info("begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassBeginInfo {};// 指定使用的渲染流程对象
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = renderPass;// the render pass to begin an instance of
        renderPassBeginInfo.framebuffer = swapChainFramebuffers[imageIndex];

        // 用于指定用于渲染的区域。位于这一区域外的像素数据会处于未定义状态
        VkRect2D renderArea {};// renderArea 是受渲染过程实例影响的渲染区域
        renderArea.offset = { 0, 0 };
        renderArea.extent = swapChainExtent;
        renderPassBeginInfo.renderArea = renderArea;

        VkClearValue clearColor {
            .color {
                {0.2f, 0.3f, 0.3f, 1.0f}
            }
        };
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearColor;
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);///

        minilog::log_info("the vkCmdEndRenderPass is end");
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {// 结束记录指令到指令缓冲
            minilog::log_fatal("failed to record command buffer!");
        } else {
            minilog::log_info("record command buffer successfully!");
        }
    }

    // 1、从交换链中获取一张图像
    // 2、对帧缓冲附件执行指令缓冲中的渲染命令
    // 3、返回渲染后的图像到交换链进行呈现操作
    // 这些操作每一个都是通过一个函数调用设置的，但每个操作的实际执行却是异步进行的
    //* 函数调用会在操作实际结束前返回，并且操作的实际执行顺序也是不确定的
    //  而我们需要操作的执行能按照一定的顺序，所以就需要进行同步操作
    void drawFrame() {
        vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

        // 从交换链中获取一张图像
        uint32_t imageIndex { 0u };
        vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(commandBuffers[currentFrame],  0);// the second is VkCommandBufferResetFlagBits
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;// 队列开始执行前需要等待的信号量
        submitInfo.pWaitDstStageMask = waitStages;// 需要等待的管线阶段
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];// 用于指定实际被提交执行的指令缓冲对象

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;// 用于指定在指令缓冲执行结束后发出信号的信号量对象

        // 最后一个参数是一个可选的栅栏对象，可以用它同步提交的指令缓冲执行结束后要进行的操作
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {// 提交指令缓冲给图形指令队列
            minilog::log_fatal("failed to submit draw command buffer!");
        } else {
            minilog::log_info("submit draw command buffer successfully!");
        }

        VkPresentInfoKHR presentInfo {};// 配置呈现信息
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;
        vkQueuePresentKHR(presentQueue, &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }




    // help functions
    void checkValidationLayerSupport() {
        uint32_t layerCount { 0u };
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            for (const VkLayerProperties& layerProperties : availableLayers) {
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
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.pNext = nullptr;
        debugCreateInfo.flags = 0;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        debugCreateInfo.pUserData = nullptr;// optional
    }

    VKAPI_ATTR VKAPI_CALL
    static VkBool32 debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
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

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance_,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    ) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
                        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance_, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance_,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator
    ) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
                    vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance_, debugMessenger, pAllocator);
        }
    }

    bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice_) {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
        bool extensionsSupported = checkPhysicalDeviceExtensionSupport(physicalDevice_);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice_);
            swapChainAdequate = (!swapChainSupport.formats.empty())
                                && (!swapChainSupport.presentModes.empty());
        }

        VkPhysicalDeviceFeatures supportedFeatures {};// why somting are vkCreate others are vkGet
        vkGetPhysicalDeviceFeatures(physicalDevice_, &supportedFeatures);

        return indices.isComplete()
                && extensionsSupported
                && swapChainAdequate
                && supportedFeatures.samplerAnisotropy;// specifies whether anisotropic filtering is supported
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice_) {
        uint32_t queueFamilyCount { 0u };// unuse
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, queueFamilies.data());

        uint32_t i { 0u };
        QueueFamilyIndices indices {};
        for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            VkBool32 isPresentSupport = false;
            // Not all physical devices will include WSI(window system integration) support
            // Within a physical device, not all queue families will support presentation
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface, &isPresentSupport);
            if (isPresentSupport) {
                indices.presentFamily = i;
            }
            if (indices.isComplete()) { break; }
            ++i;
        }

        return indices;
    }

    // check if the SwapChain is supported
    bool checkPhysicalDeviceExtensionSupport(VkPhysicalDevice physicalDevice_) {
        uint32_t extensionCount { 0u };
        vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        return requiredExtensions.empty();
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice_) {
        SwapChainSupportDetails details {};
        // to query the basic capabilities of a surface, needed in order to create a swapchain
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface, &details.capabilities);

        uint32_t formatCount { 0u };
        // to query the supported swapchain format-color space pairs for a surface
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, nullptr);
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, details.formats.data());

        uint32_t presentModeCount { 0u };
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface, &presentModeCount, nullptr);
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface, &presentModeCount, details.presentModes.data());

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& avaiableFormats
    ) {
        for (const VkSurfaceFormatKHR& availableFormat : avaiableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM
                && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            ) {
                return availableFormat;
            }
        }

        return avaiableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& avaiablePresentModes
    ) {
        for (const auto& avaiablePresentMode : avaiablePresentModes) {
            if (avaiablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                std::cout << "Present mode: " << avaiablePresentMode << std::endl;
                return avaiablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D actualExtent = windowExtent;
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

};

int main() {
    class Triangle app {};

    try {
        std::cout << "Hello Vulkan, This is a triangle!" << std::endl;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

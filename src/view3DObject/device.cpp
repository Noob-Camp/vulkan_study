#include <cstring>// strcmp function
#include <iostream>
#include <set>
#include <unordered_set>

#include <device.hpp>


namespace RealTimeBox {

// local callback functions
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData
) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance_,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                    instance_, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance_, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance_,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                    instance_, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance_, debugMessenger, pAllocator);
    }
}




// class member functions
Device::Device(MainWindow &window_) : mainWindow{ window_ } {
    createInstance();
    setupDebugMessenger();
    createSurface();

    pickPhysicalDevice();
    createLogicalDevice();
    
    createCommandPool();
}

Device::~Device() {
    vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);
    vkDestroyDevice(logicalDevice_, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface_, nullptr);
    vkDestroyInstance(instance, nullptr);
}

uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i { 0 }; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i))
            && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        ) {
            return i;
        }
    }

    std::cout << "failed to find suitable memory type!" << std::endl;
}

VkFormat Device::findSupportedImageFormat(
    const std::vector<VkFormat> &candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR
            && (props.linearTilingFeatures & features) == features
        ) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL
                    && (props.optimalTilingFeatures & features) == features
        ) {
            return format;
        }
    }

    std::cout << "failed to find supported format!" << std::endl;
}

void Device::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer &buffer,
    VkDeviceMemory &bufferMemory
) {
    VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateBuffer(logicalDevice_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        std::cout << "failed to create vertex buffer!" << std::endl;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties),
    };

    if (vkAllocateMemory(logicalDevice_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        std::cout << "failed to allocate vertex buffer memory!" << std::endl;
    }

    vkBindBufferMemory(logicalDevice_, buffer, bufferMemory, 0);
}

void Device::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion {};
    copyRegion.srcOffset = 0;  // Optional
    copyRegion.dstOffset = 0;  // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void Device::copyBufferToImage(
    VkBuffer buffer, VkImage image,
    uint32_t width, uint32_t height, uint32_t layerCount
) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = layerCount;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer Device::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(logicalDevice_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void Device::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(logicalDevice_, commandPool, 1, &commandBuffer);
}

void Device::createImageWithInfo(
    const VkImageCreateInfo &imageInfo,
    VkMemoryPropertyFlags properties,
    VkImage &image,
    VkDeviceMemory &imageMemory
) {
    if (vkCreateImage(logicalDevice_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        std::cout << "failed to create image!" << std::endl;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice_, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(logicalDevice_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        std::cout << "failed to allocate image memory!" << std::endl;
    }

    if (vkBindImageMemory(logicalDevice_, image, imageMemory, 0) != VK_SUCCESS) {
        std::cout << "failed to bind image memory!" << std::endl;
    }
}




// private functions
void Device::createInstance() {
    if (enableValidationLayers && !isValidationLayerSupport()) {
        std::cout << "validation layers requested, but not available!" << std::endl;
    }

    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "VulkanEngine Application",
        .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 3, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo
    };

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.pNext = nullptr;
    }

    std::vector<const char*> extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS) {
        hasGlfwRequiredInstanceExtensions();
    } else {
        std::cout << "failed to create instance!" << std::endl;
    }
}

void Device::setupDebugMessenger() {
    if (!enableValidationLayers) { return; };
    VkDebugUtilsMessengerCreateInfoEXT createInfo {};
    populateDebugMessengerCreateInfo(createInfo);
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        std::cout << "failed to set up debug messenger!" << std::endl;
    }
}

void Device::createSurface() { mainWindow.createWindowSurface(instance, &surface_); }

void Device::pickPhysicalDevice() {
    uint32_t deviceCount { 0 };
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cout << "failed to find GPUs with Vulkan support!" << std::endl;
    }
    std::cout << "Device count: " << deviceCount << std::endl;
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto &device : devices) {
        if (isPhysicalDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice != VK_NULL_HANDLE) {
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        std::cout << "physical device apiVersion: " << properties.apiVersion << std::endl;
        std::cout << "physical device devicetype: " << properties.deviceType << std::endl;
        std::cout << "physical device name: " << properties.deviceName << std::endl;
    } else {
        std::cout << "failed to find a suitable GPU!" << std::endl;
    }
}

void Device::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    float queuePriority { 1.0f };
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0u;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    // might not really be necessary anymore because device specific validation layers
    // have been deprecated
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0u;
        createInfo.ppEnabledLayerNames = nullptr;
    }
    createInfo.enabledExtensionCount = static_cast<uint32_t>(physicalDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = physicalDeviceExtensions.data();
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    createInfo.pEnabledFeatures = &deviceFeatures;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice_) != VK_SUCCESS) {
        std::cout << "failed to create logical device!" << std::endl;
    }

    // 第三个参数是 VkQueue 的索引
    vkGetDeviceQueue(logicalDevice_, indices.graphicsFamily.value(), 0u, &graphicsQueue_);
    vkGetDeviceQueue(logicalDevice_, indices.presentFamily.value(), 0u, &presentQueue_);
}

void Device::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

    VkCommandPoolCreateInfo poolInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
    };

    if (vkCreateCommandPool(logicalDevice_, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cout << "failed to create command pool!" << std::endl;
    }
}




bool Device::isValidationLayerSupport() {
    uint32_t layerCount{ 0 };
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    // creates a layerCount-element vector holding { 0, 0, 0, ... }
    std::vector<VkLayerProperties> availableLayers(layerCount);
    // Prefer container.data() over &container[0]
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* validationLayerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(validationLayerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void Device::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.pNext = nullptr;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;  // Optional
}

std::vector<const char*> Device::getRequiredExtensions() {
    uint32_t glfwExtensionCount { 0 };
    const char** glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    // glfwExtensions + glfwExtensionCount
    // ��?�� glfwExtensions ?����?�?� glfwExtensionCount ��?�㝝�?�������??����?��?��
    // ��������?��?�� std::vector ���?��?���������?���??�?������?����������?���� null ��?���?���
    // ?�����?�?��extensions ������?���� glfwExtensions �����?���??�?��������?���� glfwExtensionCount ��?
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        // VK_EXT_DEBUG_UTILS_EXTENSION_NAME 等价于字符串 "VK_EXT_debug_utils"
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Device::hasGlfwRequiredInstanceExtensions() {
    uint32_t extensionCount{ 0 };
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    std::cout << "available extensions: " << std::endl;
    std::unordered_set<std::string> available;
    for (const auto& extension : extensions) {
        std::cout << "\t" << extension.extensionName << std::endl;
        available.insert(extension.extensionName);
    }

    std::cout << "required extensions:" << std::endl;
    std::vector<const char*> requiredExtensions = getRequiredExtensions();
    for (const auto& required : requiredExtensions) {
        std::cout << "\t" << required << std::endl;
        if (available.find(required) == available.end()) {
            std::cout << "Missing required glfw extension" << std::endl;
        }
    }
}

bool Device::isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice_) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    bool extensionsSupported = checkPhysicalDeviceExtensionSupport(physicalDevice_);
    bool swapChainAdequate = false;

    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice_);
        swapChainAdequate = (!swapChainSupport.formats.empty())
            && (!swapChainSupport.presentModes.empty());
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice_, &supportedFeatures);

    return indices.isComplete()
        && extensionsSupported
        && swapChainAdequate
        && supportedFeatures.samplerAnisotropy;// �?�?�?������?���
}

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice physicalDevice_) {
    QueueFamilyIndices indices {};
    uint32_t queueFamilyCount { 0 };
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, queueFamilies.data());

    int i{ 0 };
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueCount > 0) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            VkBool32 isPresentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &isPresentSupport);
            if (isPresentSupport) {
                indices.presentFamily = i;
            }
            if (indices.isComplete()) { break; }
            ++i;
        }
    }

    return indices;
}

bool Device::checkPhysicalDeviceExtensionSupport(VkPhysicalDevice physicalDevice_) {
    uint32_t extensionCount { 0 };
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(
        physicalDevice_,
        nullptr,
        &extensionCount,
        availableExtensions.data()
    );

    std::set<std::string> requiredExtensions(physicalDeviceExtensions.begin(), physicalDeviceExtensions.end());

    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice physicalDevice_) {
    SwapChainSupportDetails details {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &details.capabilities);

    uint32_t formatCount{ 0 };
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount{ 0 };
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice_,
            surface_,
            &presentModeCount,
            details.presentModes.data()
        );
    }

    return details;
}

}// namespace RealTimeBox

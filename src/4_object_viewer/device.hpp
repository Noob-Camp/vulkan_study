#ifndef DEVICE_H_
#define DEVICE_H_

#include <string>
#include <vector>
#include <optional>

#include <mainWindow.hpp>


namespace RealTimeBox {

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily { 0u };
    std::optional<uint32_t> presentFamily { 0u };

    bool isComplete() {
        return graphicsFamily.has_value()
                && presentFamily.has_value();
    }
};


struct Device {
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    Device(MainWindow& window_);
    Device(const Device &) = delete;
    void operator=(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(Device &&) = delete;
    ~Device();

    VkDevice device() { return logicalDevice_; }
    VkQueue graphicsQueue() { return graphicsQueue_; }
    VkQueue presentQueue() { return presentQueue_; }
    VkSurfaceKHR surface() { return surface_; }
    VkCommandPool getCommandPool() { return commandPool; }

    SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }
    QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findSupportedImageFormat(
        const std::vector<VkFormat> &candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    );

    // Buffer Helper Functions
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer &buffer,
        VkDeviceMemory &bufferMemory
    );
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void copyBufferToImage(
        VkBuffer buffer, VkImage image,
        uint32_t width, uint32_t height, uint32_t layerCount
    );


    void createImageWithInfo(
        const VkImageCreateInfo &imageInfo,
        VkMemoryPropertyFlags properties,
        VkImage &image,
        VkDeviceMemory &imageMemory
    );

    VkPhysicalDeviceProperties properties {};

private:
    VkInstance instance { VK_NULL_HANDLE };
    MainWindow& mainWindow;
    VkDebugUtilsMessengerEXT debugMessenger { VK_NULL_HANDLE };

    VkPhysicalDevice physicalDevice { VK_NULL_HANDLE };
    VkDevice logicalDevice_ { VK_NULL_HANDLE };
    VkQueue graphicsQueue_ { VK_NULL_HANDLE };
    VkQueue presentQueue_ { VK_NULL_HANDLE };

    VkSurfaceKHR surface_ { VK_NULL_HANDLE };
    VkCommandPool commandPool { VK_NULL_HANDLE };

    const std::vector<const char *> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    const std::vector<const char *> physicalDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };


    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    // helper functions
    bool isValidationLayerSupport();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    std::vector<const char*> getRequiredExtensions();
    void hasGlfwRequiredInstanceExtensions();

    bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice_);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice_);
    bool checkPhysicalDeviceExtensionSupport(VkPhysicalDevice physicalDevice_);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice_);
};

}  // namespace RealTimeBox
#endif// DEVICE_H_

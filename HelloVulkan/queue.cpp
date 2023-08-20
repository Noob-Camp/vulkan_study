#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

void printVkExtent3D(const VkExtent3D& extent) {
    std::cout << "the VkExtent3D is: "
              << "width = " << extent.width
              << ", height = " << extent.height
              << ", depth = " << extent.depth
              << std::endl;
}

void findQueueFamilies(VkPhysicalDevice physicalDevice_) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, queueFamilies.data());

    std::cout << "The pQueueFamilyPropertyCount is: " << queueFamilyCount << std::endl;

    std::cout << "List all pQueueFamilyProperties: " << std::endl;
    std::size_t i {0};
    for (const auto& queueFamily : queueFamilies) {
        std::cout << "i = " << i++ << std::endl;
        std::cout << "queueFlags: " << queueFamily.queueFlags << std::endl;
        std::cout << "queueCount: " << queueFamily.queueCount << std::endl;
        std::cout << "timestampValidBits: " << queueFamily.timestampValidBits << std::endl;
        std::cout << "minImageTransferGranularity: ";
            printVkExtent3D(queueFamily.minImageTransferGranularity);
    }
}

/////////////////////
void queryInstance(VkInstance instance) {
}

void queryPhysicalDevice(VkPhysicalDevice physicalDevice_) {
    findQueueFamilies(physicalDevice_);
}

void queryLogicalDevice(VkDevice logicalDevice_) {
}


int main() {
    VkInstance instance;

    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties properties;

    VkDevice logicalDevice;

    void createInstance() {
    return 0;
}

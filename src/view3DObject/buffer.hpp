#ifndef BUFFER_H_
#define BUFFER_H_

#include <device.hpp>

namespace RealTimeBox {

struct Buffer {
    Device& device;
    void* mapped = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkDeviceSize bufferSize;
    uint32_t instanceCount;
    VkDeviceSize instanceSize;
    VkDeviceSize alignmentSize;
    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryPropertyFlags;


    Buffer(
        Device& device_,
        VkDeviceSize instanceSize_,
        uint32_t instanceCount_,
        VkBufferUsageFlags usageFlags_,
        VkMemoryPropertyFlags memoryPropertyFlags_,
        VkDeviceSize minOffsetAlignment_ = 1
    );
    Buffer(const Buffer& ) = delete;
    Buffer& operator=(const Buffer& ) = delete;
    ~Buffer();

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap();

    void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    void writeToIndex(void* data, int index);
    VkResult flushIndex(int index);
    VkDescriptorBufferInfo descriptorInfoForIndex(int index);
    VkResult invalidateIndex(int index);


    static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);
};


} // namespace RealTimeBox

#endif// BUFFER_H_

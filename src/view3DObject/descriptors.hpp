#ifndef DESCRIPTORS_H_
#define DESCRIPTORS_H_


#include <device.hpp>

#include <memory>
#include <unordered_map>
#include <vector>
#include <cassert>


namespace RealTimeBox {

struct DescriptorSetLayout {
    struct Builder {
        Builder(Device& device_) : device { device_ } {}
        Builder& addBinding(
            uint32_t binding,
            VkDescriptorType descriptorType,
            VkShaderStageFlags stageFlags,
            uint32_t count = 1
        ) {
            assert(bindings.count(binding) == 0 && "Binding already in use");
            VkDescriptorSetLayoutBinding layoutBinding {
                .binding = binding,
                .descriptorType = descriptorType,
                .descriptorCount = count,
                .stageFlags = stageFlags,
                .pImmutableSamplers = nullptr
            };
            bindings[binding] = layoutBinding;
            return *this;
        }

        std::unique_ptr<DescriptorSetLayout> build() const {
            return std::make_unique<DescriptorSetLayout>(device, bindings);
        }
    
    private:
        Device& device;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings {};
    };

    DescriptorSetLayout(
        Device& device_,
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings_
    );
    DescriptorSetLayout(const DescriptorSetLayout &) = delete;
    DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;
    ~DescriptorSetLayout();

    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

private:
    Device& device;
    VkDescriptorSetLayout descriptorSetLayout;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

    friend struct DescriptorWriter;
};




struct DescriptorPool {
    struct Builder {
        Builder(Device& device_) : device { device_ } {}
        Builder& addPoolSize(VkDescriptorType descriptorType_, uint32_t count_)  {
            poolSizes.push_back({ descriptorType_, count_ });
            return *this;
        };
        Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags_) {
            poolFlags = flags_;
            return *this;
        };
        Builder& setMaxSets(uint32_t count_) {
            maxSets = count_;
            return *this;
        };
        std::unique_ptr<DescriptorPool> build() const {
            return std::make_unique<DescriptorPool>(device, maxSets, poolFlags, poolSizes);
        };

    private:
        Device& device;
        std::vector<VkDescriptorPoolSize> poolSizes {};
        uint32_t maxSets { 1000 };
        VkDescriptorPoolCreateFlags poolFlags = 0;
    };

    DescriptorPool(
        Device& device_,
        uint32_t maxSets_,
        VkDescriptorPoolCreateFlags poolFlags_,
        const std::vector<VkDescriptorPoolSize> &poolSizes_
    );
    DescriptorPool(const DescriptorPool &) = delete;
    DescriptorPool &operator=(const DescriptorPool &) = delete;
    ~DescriptorPool();

    bool allocateDescriptor(
        const VkDescriptorSetLayout descriptorSetLayout,
        VkDescriptorSet &descriptor
    ) const;
    void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;
    void resetPool();

private:
    Device& device;
    VkDescriptorPool descriptorPool;

    friend struct DescriptorWriter;
};




struct DescriptorWriter {
    DescriptorWriter(DescriptorSetLayout &setLayout_, DescriptorPool &pool_);

    DescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
    DescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);

    bool build(VkDescriptorSet &set);
    void overwrite(VkDescriptorSet &set);

private:
    DescriptorSetLayout& setLayout;
    DescriptorPool& pool;
    std::vector<VkWriteDescriptorSet> writes;
};

}// namespace RealTimeBox

#endif// DESCRIPTORS_H_

#ifndef SIMPLE_H_
#define SIMPLE_H_

#include <memory>
#include <vector>

#include <camera.hpp>
#include <device.hpp>
#include <frameInfo.hpp>
#include <gameObject.hpp>
#include <pipeline.hpp>


namespace RealTimeBox {

struct SimpleRenderSystem {
    SimpleRenderSystem(
        Device& device_,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout
    );
    SimpleRenderSystem(const SimpleRenderSystem &) = delete;
    SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;
    ~SimpleRenderSystem();

    void renderGameObjects(FrameInfo& frameInfo);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    Device& device;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout pipelineLayout;
};

}  // namespace RealTimeBox

#endif// SIMPLE_H_
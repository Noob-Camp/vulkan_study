#ifndef POINT_H_
#define POINT_H_

#include <memory>
#include <vector>

#include <camera.hpp>
#include <device.hpp>
#include <frameInfo.hpp>
#include <gameObject.hpp>
#include <pipeline.hpp>


namespace RealTimeBox {

struct PointLightSystem {
    PointLightSystem(
        Device& device_,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout
    );
    PointLightSystem(const PointLightSystem &) = delete;
    PointLightSystem &operator=(const PointLightSystem &) = delete;
    ~PointLightSystem();

    void update(FrameInfo &frameInfo, GlobalUbo &ubo);
    void render(FrameInfo &frameInfo);

    private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    Device& device;

    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout pipelineLayout;
};

}// namespace RealTimeBox

#endif// POINT_H_
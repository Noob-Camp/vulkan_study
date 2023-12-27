#ifndef PIPELINE_H_
#define PIPELINE_H_

#include <string>
#include <vector>

#include <device.hpp>


namespace RealTimeBox {

struct PipelineConfigInfo {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineViewportStateCreateInfo viewportInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t subpass = 0;

    PipelineConfigInfo() = default;
    PipelineConfigInfo(const PipelineConfigInfo&) = delete;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;
};


struct Pipeline {
    Pipeline(
        Device& device,
        const std::string& vertFilepath,
        const std::string& fragFilepath,
        const PipelineConfigInfo& configInfo
    );
    Pipeline(const Pipeline&) = delete;
    void operator=(const Pipeline&) = delete;
    ~Pipeline();

    void bind(VkCommandBuffer commandBuffer_);
    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
    static void enableAlphaBlending(PipelineConfigInfo& configInfo);

private:
    Device& device;
    VkPipeline graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

    static std::vector<char> readFile(const std::string& filename);
    void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

    void createGraphicsPipeline(
        const std::string& vertFilepath,
        const std::string& fragFilepath,
        const PipelineConfigInfo& configInfo
    );
};

}  // namespace RealTimeBox

#endif// PIPELINE_H_

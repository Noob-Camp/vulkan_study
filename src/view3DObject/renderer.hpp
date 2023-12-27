#ifndef RENDERER_H_
#define RENDERER_H_

#include <cassert>
#include <memory>
#include <vector>

#include <device.hpp>
#include <swapChain.hpp>
#include <mainWindow.hpp>


namespace RealTimeBox {

struct Renderer {
    Renderer(MainWindow& mainWindow_, Device& device_);
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    ~Renderer();

    VkRenderPass getSwapChainRenderPass() const { return swapChain_ptr->getRenderPass(); }
    float getAspectRatio() const { return swapChain_ptr->extentAspectRatio(); }
    bool isFrameInProgress() const { return isFrameStarted; }

    VkCommandBuffer getCurrentCommandBuffer() const {
        assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
        return commandBuffers[currentFrameIndex];
    }

    int getFrameIndex() const {
        assert(isFrameStarted && "Cannot get frame index when frame not in progress");
        return currentFrameIndex;
    }

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

    MainWindow& mainWindow;
    Device& device;
    std::unique_ptr<SwapChain> swapChain_ptr;
    std::vector<VkCommandBuffer> commandBuffers;

    uint32_t currentImageIndex;
    int currentFrameIndex { 0 };
    bool isFrameStarted { false };
};

}// namespace RealTimeBox
#endif// RENDERER_H_
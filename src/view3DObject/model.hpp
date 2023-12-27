#ifndef MODEL_H_
#define MODEL_H_

#include <buffer.hpp>
#include <device.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>


namespace RealTimeBox {

struct Model {
    struct Vertex {
        glm::vec3 position { 0.0f, 0.0f, 0.0f };
        glm::vec3 color { 0.0f, 0.0f, 0.0f };
        glm::vec3 normal { 0.0f, 0.0f, 0.0f };
        glm::vec2 uv { 0.0f, 0.0f };

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

        bool operator==(const Vertex &other) const {
            return (position == other.position)
                    && (color == other.color)
                    && (normal == other.normal)
                    && (uv == other.uv);
        }
    };

    struct Builder {
        std::vector<Vertex> vertices {};
        std::vector<uint32_t> indices {};

        void loadModel(const std::string& filepath);
    };

    Model(Device& device_, const Model::Builder& builder);
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;
    ~Model();

    static std::unique_ptr<Model> createModelFromFile(
        Device& device_,
        const std::string& filepath
    );

    void bind(VkCommandBuffer commandBuffer_);
    void draw(VkCommandBuffer commandBuffer_);

private:
    void createVertexBuffers(const std::vector<Vertex> &vertices_);
    void createIndexBuffers(const std::vector<uint32_t> &indices_);

    Device &device;
    std::unique_ptr<Buffer> vertexBuffer;
    uint32_t vertexCount { 0 };

    bool hasIndexBuffer = false;
    std::unique_ptr<Buffer> indexBuffer;
    uint32_t indexCount { 0 };
};


}// namespace RealTimeBox {

#endif// MODEL_H_

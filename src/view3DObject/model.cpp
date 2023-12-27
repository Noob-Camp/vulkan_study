#include <cassert>
#include <cstring>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <model.hpp>
#include <utils.hpp>


namespace std {
// Vertex 转换为一个 size_t 类型的 hash 值
template <>
struct hash<RealTimeBox::Model::Vertex> {
    size_t operator()(RealTimeBox::Model::Vertex const &vertex) const {
        size_t seed = 0;// 存储 hash 值
        RealTimeBox::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
        return seed;
    }
};
}// namespace std

namespace RealTimeBox {

Model::Model(Device& device_, const Model::Builder& builder)
    : device { device_ }
{
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
}

Model::~Model() {}

std::unique_ptr<Model> Model::createModelFromFile(
    Device& device_,
    const std::string& filepath
) {
    Builder builder {};
    builder.loadModel(filepath);

    return std::make_unique<Model>(device_, builder);
}

void Model::createVertexBuffers(const std::vector<Vertex> &vertices_) {
    vertexCount = static_cast<uint32_t>(vertices_.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    VkDeviceSize bufferSize = sizeof(vertices_[0]) * vertexCount;
    uint32_t vertexSize = sizeof(vertices_[0]);

    Buffer stagingBuffer {
        device,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)vertices_.data());

    vertexBuffer = std::make_unique<Buffer>(
        device,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    device.copyBuffer(stagingBuffer.buffer, vertexBuffer->buffer, bufferSize);
}

void Model::createIndexBuffers(const std::vector<uint32_t> &indices) {
    indexCount = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;

    if (!hasIndexBuffer) { return; }

    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
    uint32_t indexSize = sizeof(indices[0]);

    Buffer stagingBuffer{
        device,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)indices.data());

    indexBuffer = std::make_unique<Buffer>(
        device,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    device.copyBuffer(stagingBuffer.buffer, indexBuffer->buffer, bufferSize);
}

void Model::draw(VkCommandBuffer commandBuffer_) {
    if (hasIndexBuffer) {
        vkCmdDrawIndexed(commandBuffer_, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer_, vertexCount, 1, 0, 0);
    }
}

void Model::bind(VkCommandBuffer commandBuffer_) {
    VkBuffer buffers[] = { vertexBuffer->buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer_, 0, 1, buffers, offsets);

    if (hasIndexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer_, indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions() {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions {};
    attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
    attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) });
    attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
    attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });

    return attributeDescriptions;
}

void Model::Builder::loadModel(const std::string &filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        throw std::runtime_error(warn + err);
    }

    vertices.clear();
    indices.clear();

    // 顶点去重：uint32_t 表示在 vertex buffer 中，该顶点的索引值
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto &shape : shapes) {
        for (const auto &index : shape.mesh.indices) {
            Vertex vertex{};
            if (index.vertex_index >= 0) {
                vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2],
                };

                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0],
                    attrib.colors[3 * index.vertex_index + 1],
                    attrib.colors[3 * index.vertex_index + 2],
                };
            }

            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2],
                };
            }

            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1],
                };
            }

            if (uniqueVertices.count(vertex) == 0) {
                // 如果顶点是新的，则添加到 map 中
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }
}

}// namespace RealTimeBox

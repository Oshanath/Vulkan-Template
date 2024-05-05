#ifndef VERTEX_BUFFER_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <stdexcept>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

class VertexBuffer
{
public:
    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };

    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkVertexInputBindingDescription bindingDescription;
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    VertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice);
    void cleanup();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

#endif // !VERTEX_BUFFER_H

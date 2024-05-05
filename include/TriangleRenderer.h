#ifndef TRIANGLE_RENDERER_H

#include "Application.h"
#include <glm/glm.hpp>
#include <array>

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;
};

class TriangleRenderer : public Application
{
private:
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	const std::vector<Vertex> vertices = {
		{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
	};

	VkVertexInputBindingDescription bindingDescription;
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

public:
	TriangleRenderer(std::string app_name);

	void main_loop_extended(uint32_t currentFrame, uint32_t imageIndex) override;
	void cleanup_extended() override;
	void createGraphicsPipeline();
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex) override;
	void beginRenderPass(uint32_t currentFrame, uint32_t imageIndex);
	void setDynamicState();
	void createVertexBuffer();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

#endif // !TRIANGLE_RENDERER_H
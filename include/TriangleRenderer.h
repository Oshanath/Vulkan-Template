#ifndef TRIANGLE_RENDERER_H
#define TRIANGLE_RENDERER_H

#include "Application.h"
#include <array>
#include "Model.h"

struct MainPushConstants
{
	glm::mat4 submeshTransform;
	glm::mat4 modelTransform;
	uint32_t materialIndex;
	uint32_t colorIndex;
	uint32_t textureType;
};

class TriangleRenderer : public vpp::Application
{
private:

	vpp::Camera camera;
	std::vector<std::shared_ptr<vpp::Model>> models;
	std::shared_ptr<vpp::Model> sky;

	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	std::vector<std::shared_ptr<vpp::Buffer>> viewProjectionUniformBuffers;
	std::vector<std::shared_ptr<vpp::Buffer>> modelUniformBuffers;

	std::shared_ptr<vpp::SuperDescriptorSetLayout> modelViewProjectionDescriptorSetLayout;
	std::vector< std::shared_ptr<vpp::SuperDescriptorSet>> modelViewProjectionDescriptorSets;

public:
	TriangleRenderer(std::string app_name, uint32_t apiVersion, std::vector<VkValidationFeatureEnableEXT> validation_features);

	void main_loop_extended(uint32_t currentFrame, uint32_t imageIndex) override;
	void cleanup_extended() override;
	void createGraphicsPipeline();
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex) override;
	void beginRenderPass(uint32_t currentFrame, uint32_t imageIndex);
	void setDynamicState();
	void createUniformBuffers();
	void initialize();
	void updateUniformBuffers(uint32_t currentFrame);
	void createDescriptorSets();
	void key_callback_extended(GLFWwindow* window, int key, int scancode, int action, int mods, double deltaTime) override;
	void mouse_callback_extended(GLFWwindow* window, int button, int action, int mods, double deltaTime) override;
	void cursor_position_callback_extended(GLFWwindow* window, double xpos, double ypos) override;
};

#endif // !TRIANGLE_RENDERER_H
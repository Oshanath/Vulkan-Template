#ifndef TRIANGLE_RENDERER_H

#include "Application.h"

class TriangleRenderer : public Application
{
private:
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

public:
	TriangleRenderer(std::string app_name);

	void main_loop_extended(uint32_t currentFrame, uint32_t imageIndex) override;
	void cleanup_extended() override;
	void createGraphicsPipeline();
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex) override;
	void beginRenderPass(uint32_t currentFrame, uint32_t imageIndex);
	void setDynamicState();
};

#endif // !TRIANGLE_RENDERER_H
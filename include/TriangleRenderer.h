#ifndef TRIANGLE_RENDERER_H

#include "Application.h"

class TriangleRenderer : public Application
{
private:
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;

public:
	TriangleRenderer(std::string app_name);

	void main_loop_extended(uint32_t imageIndex) override;
	void cleanup_extended() override;
	void createRenderPass();
	void createGraphicsPipeline();
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void createFramebuffers();
	void recordCommandBuffer(uint32_t imageIndex) override;
	void beginRenderPass(uint32_t imageIndex);
	void setDynamicState();
};

#endif // !TRIANGLE_RENDERER_H
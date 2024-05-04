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

public:
	TriangleRenderer(std::string app_name);

	void main_loop_extended() override {};
	void cleanup_extended() override;
	void createRenderPass();
	void createGraphicsPipeline();
	VkShaderModule createShaderModule(const std::vector<char>& code);
};

#endif // !TRIANGLE_RENDERER_H
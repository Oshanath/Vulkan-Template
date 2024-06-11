#include "TriangleRenderer.h"

#include "CubeMap.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include "util.h"

TriangleRenderer::TriangleRenderer(std::string app_name, uint32_t apiVersion, std::vector<VkValidationFeatureEnableEXT> validation_features) : 
    vpp::Application(app_name, apiVersion, validation_features), camera(glm::vec3(-2907.25, 2827.39, 755.888), glm::vec3(0.0f, 0.0f, 0.0f))
{

    sky = std::make_shared<vpp::Model>("models/skyBox/sky.glb", backend, vpp::TextureType::EMBEDDED);
    models.push_back(sky);
    sky->scale = glm::vec3(190.0f);
        
    std::shared_ptr<vpp::Model> sponza = std::make_shared<vpp::Model>("models/sponza/Sponza.gltf", backend, vpp::TEXTURE);
    models.push_back(sponza);

    /*std::shared_ptr<vpp::Model> sponza = std::make_unique<vpp::Model>("models/sponza3/NewSponza_Main_glTF_002.gltf", backend, vpp::TEXTURE);
    models.push_back(sponza);
    sponza->scale = glm::vec3(100.0f);

    std::shared_ptr<vpp::Model> sponzaCurtains = std::make_unique<vpp::Model>("models/sponza3curtains/NewSponza_Curtains_glTF.gltf", backend, vpp::TEXTURE);
    models.push_back(sponzaCurtains);
    sponzaCurtains->scale = glm::vec3(100.0f);*/

    std::shared_ptr<vpp::Model> trashGod = std::make_shared<vpp::Model>("models/trashGod/scene.fbx", backend, vpp::FLAT_COLOR);
    models.push_back(trashGod);

    vpp::Model::finishLoadingModels(backend);

    CubeMap cubeMap(backend);

    createUniformBuffers();
    initialize();

    sampler = std::make_shared<vpp::Sampler>(backend, 1, "Depth sampler");

    createGeometryPassImages();
    createDescriptorSets();
    createGraphicsPipeline();
    createGeometryPassRenderPass();
    createGeometryPassPipeline();
    createGeometryPassFrameBuffer();
    createLightingPassPipeline();
    createToneMappingPassPipeline();

    controls.ambientFactor = 0.1f;
    controls.sunlightIntensity = 3.0f;
}

void TriangleRenderer::cleanup_extended()
{
    vkDestroyRenderPass(backend->device, backend->swapChainRenderPass, nullptr);

    graphicsPipeline.reset();

    perFrameDescriptorSetLayout.reset();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		viewProjectionUniformBuffers[i].reset();
        modelUniformBuffers[i].reset();
        cameraLightInfoBuffers[i].reset();
        controlUniformBuffers[i].reset();
        viewportUniformBuffers[i].reset();
	}

    vpp::Model::destroyModels(backend);

    perFrameDescriptorSetLayout.reset();
    perFrameDescriptorSets.clear();

    vkDestroyFramebuffer(backend->device, geometryPassFrameBuffer, nullptr);

	positionImageView.reset();
	positionImage.reset();

	normalImageView.reset();
	normalImage.reset();

	albedoImageView.reset();
	albedoImage.reset();

	metallicImageView.reset();
	metallicImage.reset();

	roughnessImageView.reset();
	roughnessImage.reset();

	vkDestroyRenderPass(backend->device, geometryPassRenderPass, nullptr);

    geometryPassGraphicsPipeline.reset();

	lightingImageDescriptorSetLayout.reset();
	lightingImageDescriptorSet.reset();
	lightingImageView.reset();
	lightingImage.reset();

	lightingPassComputePipeline.reset();

    gBufferDescriptorSetLayout.reset();
    gBufferDescriptorSet.reset();

    toneMappingPassGraphicsPipeline.reset();

    sampler.reset();
    depthImageDescriptorSet.reset();
    depthImageDescriptorSetLayout.reset();


}

void TriangleRenderer::createGraphicsPipeline()
{
    graphicsPipeline = std::make_shared<vpp::GraphicsPipeline>(backend, "TriangleRenderer::Graphics Pipeline", backend->swapChainRenderPass, VK_TRUE, VK_TRUE, 1);
    graphicsPipeline->addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "shaders/test.vert.spv");
    graphicsPipeline->addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/test.frag.spv");
    graphicsPipeline->addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vpp::MainPushConstants));
    graphicsPipeline->addDescriptorSetLayout(perFrameDescriptorSetLayout);
    graphicsPipeline->addDescriptorSetLayout(vpp::Model::getTextureDescriptorSetLayout());
    graphicsPipeline->addDescriptorSetLayout(vpp::Model::getColorDescriptorSetLayout());
    graphicsPipeline->createPipeline();
}

void TriangleRenderer::createToneMappingPassPipeline()
{
    toneMappingPassGraphicsPipeline = std::make_shared<vpp::GraphicsPipeline>(backend, "TriangleRenderer::Tone Mapping Pipeline", backend->swapChainRenderPass, VK_FALSE, VK_FALSE, 1);
    toneMappingPassGraphicsPipeline->addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "shaders/toneMappingPass.vert.spv");
    toneMappingPassGraphicsPipeline->addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/toneMappingPass.frag.spv");
    toneMappingPassGraphicsPipeline->addDescriptorSetLayout(lightingImageDescriptorSetLayout);

    toneMappingPassGraphicsPipeline->vertexInputInfo.vertexAttributeDescriptionCount = 0;
    toneMappingPassGraphicsPipeline->vertexInputInfo.vertexBindingDescriptionCount = 0;
    toneMappingPassGraphicsPipeline->vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    toneMappingPassGraphicsPipeline->vertexInputInfo.pVertexBindingDescriptions = nullptr;
    toneMappingPassGraphicsPipeline->rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
    toneMappingPassGraphicsPipeline->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    toneMappingPassGraphicsPipeline->createPipeline();
}

void TriangleRenderer::createLightingPassPipeline()
{
    lightingPassComputePipeline = std::make_shared<vpp::ComputePipeline>(backend, "TriangleRenderer::Lighting pass Pipeline", "shaders/lightingPass.comp.spv");
    lightingPassComputePipeline->addDescriptorSetLayout(perFrameDescriptorSetLayout);
    lightingPassComputePipeline->addDescriptorSetLayout(gBufferDescriptorSetLayout);
    lightingPassComputePipeline->addDescriptorSetLayout(lightingImageDescriptorSetLayout);
    lightingPassComputePipeline->addDescriptorSetLayout(depthImageDescriptorSetLayout);
    lightingPassComputePipeline->addPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vpp::MainPushConstants));
    lightingPassComputePipeline->createPipeline();
}

void TriangleRenderer::createGeometryPassPipeline()
{
    geometryPassGraphicsPipeline = std::make_shared<vpp::GraphicsPipeline>(backend, "TriangleRenderer::Geometry pass Pipeline", geometryPassRenderPass, VK_TRUE, VK_TRUE, 4);
    geometryPassGraphicsPipeline->addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "shaders/geometryPass.vert.spv");
    geometryPassGraphicsPipeline->addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/geometryPass.frag.spv");
    geometryPassGraphicsPipeline->addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vpp::MainPushConstants));
    geometryPassGraphicsPipeline->addDescriptorSetLayout(perFrameDescriptorSetLayout);
    geometryPassGraphicsPipeline->addDescriptorSetLayout(vpp::Model::getTextureDescriptorSetLayout());
    geometryPassGraphicsPipeline->addDescriptorSetLayout(vpp::Model::getColorDescriptorSetLayout());
    geometryPassGraphicsPipeline->createPipeline();
}

void TriangleRenderer::createGeometryPassRenderPass()
{
    // Attachment descriptions
    VkAttachmentDescription normalAttachment{};
    normalAttachment.format = normalImage->format;
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentDescription albedoAttachment = normalAttachment;
    albedoAttachment.format = albedoImage->format;

    VkAttachmentDescription metallicAttachment = normalAttachment;
    metallicAttachment.format = metallicImage->format;

    VkAttachmentDescription roughnessAttachment = normalAttachment;
    roughnessAttachment.format = roughnessImage->format;

    // Attachment references
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference normalAttachmentRef{};
	normalAttachmentRef.attachment = 1;
    normalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference albedoAttachmentRef = normalAttachmentRef;
	albedoAttachmentRef.attachment = 2;

	VkAttachmentReference metallicAttachmentRef = normalAttachmentRef;
	metallicAttachmentRef.attachment = 3;

	VkAttachmentReference roughnessAttachmentRef = normalAttachmentRef;
	roughnessAttachmentRef.attachment = 4;

    std::vector<VkAttachmentReference> colorAttachmentRefs = { normalAttachmentRef, albedoAttachmentRef, metallicAttachmentRef, roughnessAttachmentRef };

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = colorAttachmentRefs.size();
    subpass.pColorAttachments = colorAttachmentRefs.data();
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependencies
    std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dependencyFlags = 0;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dependencies[1].dependencyFlags = 0;

    std::vector<VkAttachmentDescription> attachments = { depthAttachment, normalAttachment, albedoAttachment, metallicAttachment, roughnessAttachment };

    VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = dependencies.size();
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(backend->device, &renderPassInfo, nullptr, &geometryPassRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void TriangleRenderer::createGeometryPassFrameBuffer()
{
    std::vector<VkImageView> attachments = {
        backend->depthImageView->imageView,
        normalImageView->imageView,
        albedoImageView->imageView,
        metallicImageView->imageView,
        roughnessImageView->imageView
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = geometryPassRenderPass;
    framebufferInfo.attachmentCount = attachments.size();
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = backend->swapChainExtent.width;
    framebufferInfo.height = backend->swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(backend->device, &framebufferInfo, nullptr, &geometryPassFrameBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
    }
}

void TriangleRenderer::createGeometryPassImages()
{
    positionImage = std::make_shared<vpp::Image>(backend, backend->swapChainExtent.width, backend->swapChainExtent.height, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Geometry pass::Position Image");
    positionImageView = std::make_shared<vpp::ImageView>(backend, positionImage, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "Position Image View");

    normalImage = std::make_shared<vpp::Image>(backend, backend->swapChainExtent.width, backend->swapChainExtent.height, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Geometry pass::Normal Image");
	normalImageView = std::make_shared<vpp::ImageView>(backend, normalImage, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "Normal Image View");

	albedoImage = std::make_shared<vpp::Image>(backend, backend->swapChainExtent.width, backend->swapChainExtent.height, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Geometry pass::Albedo Image");
	albedoImageView = std::make_shared<vpp::ImageView>(backend, albedoImage, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "Albedo Image View");

	metallicImage = std::make_shared<vpp::Image>(backend, backend->swapChainExtent.width, backend->swapChainExtent.height, 1, 1, VK_FORMAT_R8G8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Geometry pass::Metallic Image");
	metallicImageView = std::make_shared<vpp::ImageView>(backend, metallicImage, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "Metallic Image View");

	roughnessImage = std::make_shared<vpp::Image>(backend, backend->swapChainExtent.width, backend->swapChainExtent.height, 1, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Geometry pass::Roughness Image");
	roughnessImageView = std::make_shared<vpp::ImageView>(backend, roughnessImage, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "Roughness Image View");

    lightingImage = std::make_shared<vpp::Image>(backend, backend->swapChainExtent.width, backend->swapChainExtent.height, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Geometry pass::Temp Out Image");
    lightingImageView = std::make_shared<vpp::ImageView>(backend, lightingImage, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "Lighting Image View");
    lightingImage->transitionLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

void TriangleRenderer::renderObjects()
{
    vpp::MainPushConstants pushConstants;

    for (auto& model : models)
    {
        pushConstants.textureType = uint32_t(model->textureType);
        pushConstants.modelTransform = model->getModelMatrix();

        if (model->hasTree)
        {
            for (auto& node : model->nodes)
            {
                pushConstants.submeshTransform = node.transform;
                pushConstants.materialIndex = model->meshes[node.meshIndex].materialIndex;
                pushConstants.colorIndex = model->meshes[node.meshIndex].colorIndex;
                vkCmdPushConstants(backend->commandBuffers[currentFrame], graphicsPipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vpp::MainPushConstants), &pushConstants);
                vkCmdDrawIndexed(backend->commandBuffers[currentFrame], model->meshes[node.meshIndex].indexCount, 1, model->meshes[node.meshIndex].startIndex, 0, 0);
            }
        }
        else
        {
            pushConstants.submeshTransform = glm::mat4(1.0f);

            for (auto& mesh : model->meshes)
            {
                pushConstants.materialIndex = mesh.materialIndex;
                pushConstants.colorIndex = mesh.colorIndex;
                vkCmdPushConstants(backend->commandBuffers[currentFrame], graphicsPipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vpp::MainPushConstants), &pushConstants);
                vkCmdDrawIndexed(backend->commandBuffers[currentFrame], mesh.indexCount, 1, mesh.startIndex, 0, 0);
            }
        }
    }
}

void TriangleRenderer::recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex)
{
    beginCommandBuffer();

    {
        // Geometry pass
        beginGeometryPass(currentFrame, imageIndex);
        renderObjects();
        vkCmdEndRenderPass(backend->commandBuffers[currentFrame]);

        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = lightingImage->image;
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(backend->commandBuffers[currentFrame], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        // Lighting pass
        vkCmdBindPipeline(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, lightingPassComputePipeline->pipeline);

		vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, lightingPassComputePipeline->pipelineLayout, 0, 1, &perFrameDescriptorSets[currentFrame]->descriptorSet, 0, nullptr);
		vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, lightingPassComputePipeline->pipelineLayout, 1, 1, &gBufferDescriptorSet->descriptorSet, 0, nullptr);
		vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, lightingPassComputePipeline->pipelineLayout, 2, 1, &lightingImageDescriptorSet->descriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, lightingPassComputePipeline->pipelineLayout, 3, 1, &depthImageDescriptorSet->descriptorSet, 0, nullptr);

		vkCmdDispatch(backend->commandBuffers[currentFrame], backend->swapChainExtent.width / 16, backend->swapChainExtent.height / 16, 1);
    }

    beginRenderPass(currentFrame, imageIndex);

    vkCmdBindPipeline(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, toneMappingPassGraphicsPipeline->pipeline);
    setDynamicState();
    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, toneMappingPassGraphicsPipeline->pipelineLayout, 0, 1, &lightingImageDescriptorSet->descriptorSet, 0, nullptr);
    vkCmdDraw(backend->commandBuffers[currentFrame], 3, 1, 0, 0);

    // Imgui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), backend->commandBuffers[currentFrame]);

    vkCmdEndRenderPass(backend->commandBuffers[currentFrame]);

    if (vkEndCommandBuffer(backend->commandBuffers[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void TriangleRenderer::beginRenderPass(uint32_t currentFrame, uint32_t imageIndex)
{
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = backend->swapChainRenderPass;
    renderPassInfo.framebuffer = backend->swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = backend->swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(backend->commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void TriangleRenderer::beginGeometryPass(uint32_t currentFrame, uint32_t imageIndex)
{
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = geometryPassRenderPass;
    renderPassInfo.framebuffer = geometryPassFrameBuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = backend->swapChainExtent;

    std::vector<VkClearValue> clearValues(6);
    clearValues[0].depthStencil = { 1.0f, 0 };
    clearValues[1].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    clearValues[2].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    clearValues[3].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    clearValues[4].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    clearValues[5].color = { {0.0f, 0.0f, 0.0f, 0.0f} };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(backend->commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, geometryPassGraphicsPipeline->pipeline);

    setDynamicState();

    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(backend->commandBuffers[currentFrame], 0, 1, &(vpp::Model::getVertexBuffer()->buffer), offsets);
    vkCmdBindIndexBuffer(backend->commandBuffers[currentFrame], vpp::Model::getIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->pipelineLayout, 0, 1, &perFrameDescriptorSets[currentFrame]->descriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->pipelineLayout, 1, 1, &vpp::Model::getTextureDescriptorSet()->descriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->pipelineLayout, 2, 1, &vpp::Model::getColorDescriptorSet()->descriptorSet, 0, nullptr);
}

void TriangleRenderer::setDynamicState()
{
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(backend->swapChainExtent.width);
    viewport.height = static_cast<float>(backend->swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(backend->commandBuffers[currentFrame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = backend->swapChainExtent;
    vkCmdSetScissor(backend->commandBuffers[currentFrame], 0, 1, &scissor);
}

void TriangleRenderer::main_loop_extended(uint32_t currentFrame, uint32_t imageIndex)
{

    camera.deltaTime = deltaTime;
    camera.move();

    if (sky.get() != nullptr)
    {
        sky->position = camera.position;
	}

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Imgui window here
    ImGui::Text("Controls\n");

    ImGui::SliderFloat("Sunlight Intensity", &controls.sunlightIntensity, 0.0f, 10.0f);

    ImGui::SliderFloat("Ambient Factor", &controls.ambientFactor, 0.0f, 1.0f);

    updateUniformBuffers(currentFrame);
    recordCommandBuffer(currentFrame, imageIndex);
}

void TriangleRenderer::createUniformBuffers()
{
    VkDeviceSize viewProjectionUBOSize = sizeof(vpp::ViewProjectionMatrices);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        viewProjectionUniformBuffers.push_back(std::make_shared<vpp::Buffer>(backend, viewProjectionUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vpp::CONTINOUS_TRANSFER, nullptr, "View Projection Uniform Buffer"));
        modelUniformBuffers.push_back(std::make_shared<vpp::Buffer>(backend, sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vpp::CONTINOUS_TRANSFER, nullptr, "Model Uniform buffer"));
        cameraLightInfoBuffers.push_back(std::make_shared<vpp::Buffer>(backend, sizeof(vpp::CameraLightInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vpp::CONTINOUS_TRANSFER, nullptr, "Camera Light Info buffer"));
        controlUniformBuffers.push_back(std::make_shared<vpp::Buffer>(backend, sizeof(vpp::Controls), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vpp::CONTINOUS_TRANSFER, nullptr, "Control Uniform buffer"));
        viewportUniformBuffers.push_back(std::make_shared<vpp::Buffer>(backend, sizeof(ViewportDims), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vpp::CONTINOUS_TRANSFER, nullptr, "Viewport Uniform buffer"));
    }
}

void TriangleRenderer::initialize()
{
    
}

void TriangleRenderer::updateUniformBuffers(uint32_t currentImage)
{
    vpp::ViewProjectionMatrices ubo = camera.getMVPMatrices(backend->swapChainExtent.width, backend->swapChainExtent.height);
    memcpy(viewProjectionUniformBuffers[currentImage]->mappedPtr, &ubo, sizeof(ubo));

    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::mat4(1.0f);
    memcpy(modelUniformBuffers[currentImage]->mappedPtr, &modelMatrix, sizeof(modelMatrix));

    vpp::CameraLightInfo cameraLightInfo;
    cameraLightInfo.cameraPos = glm::vec4(camera.position, 1.0f);
    cameraLightInfo.lightDir = glm::vec4(-1.0f, 1.0f, -1.0f, 0.0f);
    memcpy(cameraLightInfoBuffers[currentImage]->mappedPtr, &cameraLightInfo, sizeof(cameraLightInfo));

    memcpy(controlUniformBuffers[currentImage]->mappedPtr, &controls, sizeof(controls));

    ViewportDims viewportDims;
    viewportDims.width = backend->swapChainExtent.width;
    viewportDims.height = backend->swapChainExtent.height;
    memcpy(viewportUniformBuffers[currentImage]->mappedPtr, &viewportDims, sizeof(ViewportDims));
}

void TriangleRenderer::createDescriptorSets()
{
    // Per frame descriptor set
    perFrameDescriptorSetLayout = std::make_shared<vpp::SuperDescriptorSetLayout>(backend, "Model view projection descriptor set layout");
    perFrameDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 1);
    perFrameDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    perFrameDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 1);
    perFrameDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 1);
    perFrameDescriptorSetLayout->createLayout();

    perFrameDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
        perFrameDescriptorSets[i] = std::make_shared<vpp::SuperDescriptorSet>(backend, perFrameDescriptorSetLayout, "Model View Projection descriptor set " + std::to_string(i));
        perFrameDescriptorSets[i]->addBuffersToBinding({ viewProjectionUniformBuffers[i] });
        perFrameDescriptorSets[i]->addBuffersToBinding({ modelUniformBuffers[i] });
        perFrameDescriptorSets[i]->addBuffersToBinding({ cameraLightInfoBuffers[i] });
        perFrameDescriptorSets[i]->addBuffersToBinding({ controlUniformBuffers[i]});
        perFrameDescriptorSets[i]->createDescriptorSet();
	}

    // G buffer descriptor set
    gBufferDescriptorSetLayout = std::make_shared<vpp::SuperDescriptorSetLayout>(backend, "G Buffer descriptor set layout");
    gBufferDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1); // normal
    gBufferDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1); // albedo
    gBufferDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1); // metallic
    gBufferDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1); // roughness
    gBufferDescriptorSetLayout->createLayout();

    gBufferDescriptorSet = std::make_shared<vpp::SuperDescriptorSet>(backend, gBufferDescriptorSetLayout, "G Buffer descriptor set");
	gBufferDescriptorSet->addImagesToBinding({ normalImageView }, { sampler }, { VK_IMAGE_LAYOUT_GENERAL });
	gBufferDescriptorSet->addImagesToBinding({ albedoImageView }, { sampler }, { VK_IMAGE_LAYOUT_GENERAL });
	gBufferDescriptorSet->addImagesToBinding({ metallicImageView }, { sampler }, { VK_IMAGE_LAYOUT_GENERAL });
	gBufferDescriptorSet->addImagesToBinding({ roughnessImageView }, { sampler }, { VK_IMAGE_LAYOUT_GENERAL });
	gBufferDescriptorSet->createDescriptorSet();

    // Lighting image descriptor set

    lightingImageDescriptorSetLayout = std::make_shared<vpp::SuperDescriptorSetLayout>(backend, "Lighting image descriptor set layout");
    lightingImageDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);   
    lightingImageDescriptorSetLayout->createLayout();

	lightingImageDescriptorSet = std::make_shared<vpp::SuperDescriptorSet>(backend, lightingImageDescriptorSetLayout, "Lighting image descriptor set");
    lightingImageDescriptorSet->addImagesToBinding({ lightingImageView }, { sampler }, { VK_IMAGE_LAYOUT_GENERAL });
	lightingImageDescriptorSet->createDescriptorSet();

    // Depth image descriptor set
    depthImageDescriptorSetLayout = std::make_shared<vpp::SuperDescriptorSetLayout>(backend, "Depth image descriptor set layout");
    depthImageDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1); // depth
    depthImageDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1); // viewPort dims
    depthImageDescriptorSetLayout->createLayout();

    depthImageDescriptorSet = std::make_shared<vpp::SuperDescriptorSet>(backend, depthImageDescriptorSetLayout, "Depth image descriptor set");
	depthImageDescriptorSet->addImagesToBinding({ backend->depthImageView }, { sampler }, { VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL });
    depthImageDescriptorSet->addBuffersToBinding({ viewportUniformBuffers[currentFrame] });
    depthImageDescriptorSet->createDescriptorSet();

}

void TriangleRenderer::key_callback_extended(GLFWwindow* window, int key, int scancode, int action, int mods, double deltaTime)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    if (key == GLFW_KEY_W && action == GLFW_PRESS)
    {
        camera.movingForward = true;
	}
    else if (key == GLFW_KEY_W && action == GLFW_RELEASE)
    {
        camera.movingForward = false;
    }

    if (key == GLFW_KEY_S && action == GLFW_PRESS)
    {
		camera.movingBackward = true;
	}
    else if (key == GLFW_KEY_S && action == GLFW_RELEASE)
    {
		camera.movingBackward = false;
	}

    if (key == GLFW_KEY_A && action == GLFW_PRESS)
    {
		camera.movingLeft = true;
	}
    else if (key == GLFW_KEY_A && action == GLFW_RELEASE)
    {
		camera.movingLeft = false;
	}

    if (key == GLFW_KEY_D && action == GLFW_PRESS)
    {
		camera.movingRight = true;
	}
    else if (key == GLFW_KEY_D && action == GLFW_RELEASE)
    {
		camera.movingRight = false;
	}

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
		camera.movingUp = true;
	}
    else if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
    {
		camera.movingUp = false;
	}

    if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_PRESS)
    {
		camera.movingDown = true;
	}
    else if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_RELEASE)
    {
		camera.movingDown = false;
	}

    if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        std::cout << "Camera position: " << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << std::endl;
	}
}

void TriangleRenderer::mouse_callback_extended(GLFWwindow* window, int button, int action, int mods, double deltaTime)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
		camera.freeLook = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        camera.freeLook = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}

void TriangleRenderer::cursor_position_callback_extended(GLFWwindow* window, double xpos, double ypos)
{
    camera.mouse_callback(xpos, ypos);
}
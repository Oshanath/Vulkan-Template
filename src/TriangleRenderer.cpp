#include "TriangleRenderer.h"

#include "CubeMap.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

TriangleRenderer::TriangleRenderer(std::string app_name, uint32_t apiVersion, std::vector<VkValidationFeatureEnableEXT> validation_features) : 
    vpp::Application(app_name, apiVersion, validation_features), camera(glm::vec3(-2907.25, 2827.39, 755.888), glm::vec3(0.0f, 0.0f, 0.0f))
{

    sky = std::make_shared<vpp::Model>("models/skyBox/sky.glb", backend, vpp::TextureType::EMBEDDED);
    models.push_back(sky);
    sky->scale = glm::vec3(19.0f);
    
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
    createDescriptorSets();
    createGraphicsPipeline();
}

void TriangleRenderer::cleanup_extended()
{
    vkDestroyPipeline(backend->device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(backend->device, pipelineLayout, nullptr);
    vkDestroyRenderPass(backend->device, backend->swapChainRenderPass, nullptr);

    modelViewProjectionDescriptorSetLayout.reset();

    vkDestroyShaderModule(backend->device, fragShaderModule, nullptr);
    vkDestroyShaderModule(backend->device, vertShaderModule, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		viewProjectionUniformBuffers[i].reset();
        modelUniformBuffers[i].reset();
        cameraLightInfoBuffers[i].reset();
	}

    vpp::Model::destroyModels(backend);

    modelViewProjectionDescriptorSetLayout.reset();
    modelViewProjectionDescriptorSets.clear();
}

void TriangleRenderer::createGraphicsPipeline()
{
    auto vertShaderCode = readFile("shaders/test.vert.spv");
    auto fragShaderCode = readFile("shaders/test.frag.spv");
    vertShaderModule = createShaderModule(vertShaderCode);
    fragShaderModule = createShaderModule(fragShaderCode);
    backend->setNameOfObject(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vertShaderModule, "TriangleRenderer::Vertex Shader Module");
    backend->setNameOfObject(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)fragShaderModule, "TriangleRenderer::Fragment Shader Module");

    // Vertex shader stage
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    // Fragment shader stage
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Depth stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vpp::Vertex::getAttributeDescriptions().size());
    VkVertexInputBindingDescription bindingDescription = vpp::Vertex::getBindingDescription();
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = vpp::Vertex::getAttributeDescriptions().data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)backend->swapChainExtent.width;
    viewport.height = (float)backend->swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = backend->swapChainExtent;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Viewport state
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    // Pipeline layout
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(MainPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    std::vector<VkDescriptorSetLayout> layouts = {
        modelViewProjectionDescriptorSetLayout->descriptorSetLayout,
        vpp::Model::getTextureDescriptorSetLayout()->descriptorSetLayout,
        vpp::Model::getColorDescriptorSetLayout()->descriptorSetLayout
    };
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = layouts.size();
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(backend->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = backend->swapChainRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(backend->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
}

VkShaderModule TriangleRenderer::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(backend->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void TriangleRenderer::recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex)
{
    beginCommandBuffer();

    beginRenderPass(currentFrame, imageIndex);

    vkCmdBindPipeline(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    setDynamicState();

    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(backend->commandBuffers[currentFrame], 0, 1, &(vpp::Model::getVertexBuffer()->buffer), offsets);
    vkCmdBindIndexBuffer(backend->commandBuffers[currentFrame], vpp::Model::getIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &modelViewProjectionDescriptorSets[currentFrame]->descriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &vpp::Model::getTextureDescriptorSet()->descriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &vpp::Model::getColorDescriptorSet()->descriptorSet, 0, nullptr);

    MainPushConstants pushConstants;
    
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
                vkCmdPushConstants(backend->commandBuffers[currentFrame], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MainPushConstants), &pushConstants);
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
                vkCmdPushConstants(backend->commandBuffers[currentFrame], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MainPushConstants), &pushConstants);
                vkCmdDrawIndexed(backend->commandBuffers[currentFrame], mesh.indexCount, 1, mesh.startIndex, 0, 0);
            }
        }
    }

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

    updateUniformBuffers(currentFrame);
    recordCommandBuffer(currentFrame, imageIndex);
}

void TriangleRenderer::createUniformBuffers()
{
    VkDeviceSize viewProjectionUBOSize = sizeof(vpp::ViewProjectionMatrices);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        viewProjectionUniformBuffers.push_back(std::make_shared<vpp::Buffer>(backend, viewProjectionUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vpp::CONTINOUS_TRANSFER, nullptr, "View Projection Uniform Buffer"));
        modelUniformBuffers.push_back(std::make_shared<vpp::Buffer>(backend, sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vpp::CONTINOUS_TRANSFER, nullptr, "Model Uniform buffer"));
        cameraLightInfoBuffers.push_back(std::make_shared<vpp::Buffer>(backend, sizeof(CameraLightInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vpp::CONTINOUS_TRANSFER, nullptr, "Camera Light Info buffer"));
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

    CameraLightInfo cameraLightInfo;
    cameraLightInfo.cameraPos = glm::vec4(camera.position, 1.0f);
    cameraLightInfo.lightDir = glm::vec4(-1.0f, 1.0f, -1.0f, 0.0f);
    memcpy(cameraLightInfoBuffers[currentImage]->mappedPtr, &cameraLightInfo, sizeof(cameraLightInfo));
}

void TriangleRenderer::createDescriptorSets()
{
    modelViewProjectionDescriptorSetLayout = std::make_shared<vpp::SuperDescriptorSetLayout>(backend, "Model view projection descriptor set layout");
    modelViewProjectionDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
    modelViewProjectionDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    modelViewProjectionDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    modelViewProjectionDescriptorSetLayout->createLayout();

    modelViewProjectionDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
        modelViewProjectionDescriptorSets[i] = std::make_shared<vpp::SuperDescriptorSet>(backend, modelViewProjectionDescriptorSetLayout, "Model View Projection descriptor set " + std::to_string(i));
        modelViewProjectionDescriptorSets[i]->addBuffersToBinding({ viewProjectionUniformBuffers[i] });
        modelViewProjectionDescriptorSets[i]->addBuffersToBinding({ modelUniformBuffers[i] });
        modelViewProjectionDescriptorSets[i]->addBuffersToBinding({ cameraLightInfoBuffers[i] });
        modelViewProjectionDescriptorSets[i]->createDescriptorSet();
	}

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
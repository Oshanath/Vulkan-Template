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
    sky->scale = glm::vec3(1900.0f);
        
    /*std::shared_ptr<vpp::Model> sponza = std::make_shared<vpp::Model>("models/sponza/Sponza.gltf", backend, vpp::TEXTURE);
    models.push_back(sponza);*/

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
	}

    vpp::Model::destroyModels(backend);

    perFrameDescriptorSetLayout.reset();
    perFrameDescriptorSets.clear();
}

void TriangleRenderer::createGraphicsPipeline()
{
    graphicsPipeline = std::make_shared<vpp::GraphicsPipeline>(backend, "TriangleRenderer::Graphics Pipeline", backend->swapChainRenderPass, VK_TRUE, VK_TRUE);
    graphicsPipeline->addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "shaders/test.vert.spv");
    graphicsPipeline->addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/test.frag.spv");
    graphicsPipeline->addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vpp::MainPushConstants));
    graphicsPipeline->addDescriptorSetLayout(perFrameDescriptorSetLayout);
    graphicsPipeline->addDescriptorSetLayout(vpp::Model::getTextureDescriptorSetLayout());
    graphicsPipeline->addDescriptorSetLayout(vpp::Model::getColorDescriptorSetLayout());
    graphicsPipeline->createPipeline();
}

void TriangleRenderer::recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex)
{
    beginCommandBuffer();

    beginRenderPass(currentFrame, imageIndex);

    vkCmdBindPipeline(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->pipeline);

    setDynamicState();

    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(backend->commandBuffers[currentFrame], 0, 1, &(vpp::Model::getVertexBuffer()->buffer), offsets);
    vkCmdBindIndexBuffer(backend->commandBuffers[currentFrame], vpp::Model::getIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->pipelineLayout, 0, 1, &perFrameDescriptorSets[currentFrame]->descriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->pipelineLayout, 1, 1, &vpp::Model::getTextureDescriptorSet()->descriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(backend->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->pipelineLayout, 2, 1, &vpp::Model::getColorDescriptorSet()->descriptorSet, 0, nullptr);

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
}

void TriangleRenderer::createDescriptorSets()
{
    perFrameDescriptorSetLayout = std::make_shared<vpp::SuperDescriptorSetLayout>(backend, "Model view projection descriptor set layout");
    perFrameDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
    perFrameDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    perFrameDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    perFrameDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
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
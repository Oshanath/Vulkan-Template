#ifndef MODEL_H
#define MODEL_H

#include "Backend.h"

#include <iostream>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <array>
#include <memory>

namespace vpp
{
	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 texCoord;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, normal);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

			return attributeDescriptions;
		}
	};

	struct Mesh
	{
		uint32_t materialIndex;
		uint32_t colorIndex;
		uint32_t vertexCount;
		uint32_t indexCount;
		uint32_t startIndex;
		uint32_t startVertex;
	};

	struct Node
	{
		uint32_t meshIndex;
		glm::mat4 transform;
	};

	enum TextureType
	{
		TEXTURE,
		FLAT_COLOR,
		EMBEDDED
	};

	class Model
	{
	public:
		

		std::shared_ptr<vpp::Backend> backend;
		std::string path;
		std::string directory;

		std::vector<Mesh> meshes;
		std::vector<Node> nodes;
		TextureType textureType;
		bool hasTree;

		glm::vec3 position;
		glm::vec3 scale;
		std::pair<float, glm::vec3> rotationAngleAxis;

		Model(std::string path, std::shared_ptr<vpp::Backend> backend, TextureType textureType);

		glm::mat4 getModelMatrix();

		inline static std::shared_ptr<SuperDescriptorSetLayout> getTextureDescriptorSetLayout()
		{
			if (!finished)
			{
				throw std::runtime_error("Models not loaded");
			}

			return textureDescriptorSetLayout;
		}

		inline static std::shared_ptr<SuperDescriptorSetLayout> getColorDescriptorSetLayout()
		{
			if (!finished)
			{
				throw std::runtime_error("Models not loaded");
			}

			return colorDescriptorSetLayout;
		}

		inline static std::shared_ptr<SuperDescriptorSet> getTextureDescriptorSet()
		{
			if (!initialized)
				throw std::runtime_error("Models not loaded");

			return textureDescriptorSet;
		}

		inline static std::shared_ptr<SuperDescriptorSet> getColorDescriptorSet()
		{
			if (!initialized)
				throw std::runtime_error("Models not loaded");

			return colorDescriptorSet;
		}

		inline static void finishLoadingModels(std::shared_ptr<Backend> backend)
		{
			if(vertices.size() == 0) throw std::runtime_error("No models loaded");

			if(!initialized) throw std::runtime_error("Models not loaded");

			if(finished) throw std::runtime_error("Models already Loaded");

			vertexBuffer = std::make_shared<Buffer>(backend, vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vpp::ONE_TIME_TRANSFER, vertices.data(), "Vertex Buffer");
			indexBuffer = std::make_shared<Buffer>(backend, indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, vpp::ONE_TIME_TRANSFER, indices.data(), "Index Buffer");

			// Sampler
			textureSampler = std::make_shared<Sampler>(backend, 10, "Texture Sampler");

			// create layouts and descriptor sets
			textureDescriptorSetLayout = std::make_shared<SuperDescriptorSetLayout>(backend, "Texture descriptor set layout");
			textureDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (textureImages.size() == 0) ? 1 : textureImages.size());
			textureDescriptorSetLayout->createLayout();

			colorDescriptorSetLayout = std::make_shared<SuperDescriptorSetLayout>(backend, "Color descriptor set layout");
			colorDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
			colorDescriptorSetLayout->createLayout();

			if (textureImages.size() > 0)
			{
				textureDescriptorSet = std::make_shared<SuperDescriptorSet>(backend, textureDescriptorSetLayout, "Texture descriptor set");
				textureDescriptorSet->addImagesToBinding(textureImageViews, std::vector<std::shared_ptr<Sampler>>(textureImages.size(), textureSampler), std::vector<VkImageLayout>(textureImages.size(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
				textureDescriptorSet->createDescriptorSet();
			}
			else
			{
				defaultImage = std::make_shared<Image>(backend, 1, 1,1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Default image");
				defaultImageView = std::make_shared<ImageView>(backend, defaultImage, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "Default Image View");

				textureDescriptorSet = std::make_shared<SuperDescriptorSet>(backend, textureDescriptorSetLayout, "Texture descriptor set");
				textureDescriptorSet->addImagesToBinding({ defaultImageView }, {textureSampler}, {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
				textureDescriptorSet->createDescriptorSet();
			}

			if(colors.size() > 0)
			{
				colorBuffer = std::make_shared<Buffer>(backend, colors.size() * sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vpp::ONE_TIME_TRANSFER, colors.data(), "Color Buffer");

				colorDescriptorSet = std::make_shared<SuperDescriptorSet>(backend, colorDescriptorSetLayout, "Color descriptor set");
				colorDescriptorSet->addBuffersToBinding({ colorBuffer });
				colorDescriptorSet->createDescriptorSet();
			}
			else
			{
				defaultBuffer = std::make_shared<Buffer>(backend, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vpp::GPU_ONLY, nullptr, "Default SSBO");

				colorDescriptorSet = std::make_shared<SuperDescriptorSet>(backend, colorDescriptorSetLayout, "Color descriptor set");
				colorDescriptorSet->addBuffersToBinding({ defaultBuffer });
				colorDescriptorSet->createDescriptorSet();
			}

			finished = true;
		}

		inline static std::shared_ptr<Buffer> getVertexBuffer()
		{
			if (!initialized)
				throw std::runtime_error("Models not loaded");

			if (!finished)
				throw std::runtime_error("Models not finished loading");

			return vertexBuffer;
		}

		inline static std::shared_ptr<Buffer> getIndexBuffer()
		{
			if (!initialized)
				throw std::runtime_error("Models not loaded");

			if (!finished)
				throw std::runtime_error("Models not finished loading");

			return indexBuffer;
		}

		inline static void destroyModels(std::shared_ptr<Backend> backend)
		{
			textureDescriptorSet.reset();
			textureDescriptorSetLayout.reset();
			colorDescriptorSet.reset();
			colorDescriptorSetLayout.reset();

			textureImages.clear();
			textureImageViews.clear();
			textureSampler.reset();
			vertexBuffer.reset();
			indexBuffer.reset();
			colorBuffer.reset();
			defaultImage.reset();
			defaultImageView.reset();
			defaultBuffer.reset();

			initialized = false;
			finished = false;
		}

		inline static std::vector<std::shared_ptr<Image>> textureImages;
		inline static std::vector<glm::vec4> colors;

	private:
		inline static std::shared_ptr<SuperDescriptorSetLayout> textureDescriptorSetLayout;
		inline static std::shared_ptr<SuperDescriptorSetLayout> colorDescriptorSetLayout;
		inline static bool superDescriptorSetLayoutCreated = false;

		inline static uint32_t globalMaterialCount = 0;

		inline static std::vector<Vertex> vertices;
		inline static std::vector<uint32_t> indices;

		inline static std::vector<std::shared_ptr<ImageView>> textureImageViews;
		inline static std::shared_ptr<SuperDescriptorSet> textureDescriptorSet;
		inline static std::vector<uint32_t> mipLevels;
		inline static std::shared_ptr<vpp::Sampler> textureSampler;

		inline static std::shared_ptr<Buffer> colorBuffer;
		inline static std::shared_ptr<SuperDescriptorSet> colorDescriptorSet;

		inline static std::shared_ptr<Buffer> vertexBuffer;
		inline static std::shared_ptr<Buffer> indexBuffer;

		inline static bool initialized = false;
		inline static bool finished = false;

		inline static std::shared_ptr<Image> defaultImage;
		inline static std::shared_ptr<ImageView> defaultImageView;
		inline static std::shared_ptr<Buffer> defaultBuffer;

		void processNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform);

	};
}

#endif // !MODEL_H

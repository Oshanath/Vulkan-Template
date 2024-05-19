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
#include "util.h"

namespace vpp
{
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
			textureDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (albedoImages.size() == 0) ? 1 : albedoImages.size());
			textureDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (metallicImages.size() == 0) ? 1 : metallicImages.size());
			textureDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (roughnessImages.size() == 0) ? 1 : roughnessImages.size());
			textureDescriptorSetLayout->createLayout();

			colorDescriptorSetLayout = std::make_shared<SuperDescriptorSetLayout>(backend, "Color descriptor set layout");
			colorDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
			colorDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
			colorDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
			colorDescriptorSetLayout->createLayout();

			if (albedoImages.size() > 0)
			{
				textureDescriptorSet = std::make_shared<SuperDescriptorSet>(backend, textureDescriptorSetLayout, "Texture descriptor set");
				textureDescriptorSet->addImagesToBinding(albedoImageViews, std::vector<std::shared_ptr<Sampler>>(albedoImages.size(), textureSampler), std::vector<VkImageLayout>(albedoImages.size(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
				textureDescriptorSet->addImagesToBinding(metallicImageViews, std::vector<std::shared_ptr<Sampler>>(metallicImages.size(), textureSampler), std::vector<VkImageLayout>(metallicImages.size(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
				textureDescriptorSet->addImagesToBinding(roughnessImageViews, std::vector<std::shared_ptr<Sampler>>(roughnessImages.size(), textureSampler), std::vector<VkImageLayout>(roughnessImages.size(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
				textureDescriptorSet->createDescriptorSet();
			}
			else
			{
				textureDescriptorSet = std::make_shared<SuperDescriptorSet>(backend, textureDescriptorSetLayout, "Texture descriptor set");
				textureDescriptorSet->addImagesToBinding({ defaultImageView }, { textureSampler }, { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
				textureDescriptorSet->addImagesToBinding({ defaultImageView }, { textureSampler }, { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
				textureDescriptorSet->addImagesToBinding({ defaultImageView }, {textureSampler}, {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
				textureDescriptorSet->createDescriptorSet();
			}

			if(flatAlbedos.size() > 0)
			{
				flatAlbedoBuffer = std::make_shared<Buffer>(backend, flatAlbedos.size() * sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vpp::ONE_TIME_TRANSFER, flatAlbedos.data(), "Flat Albedo Buffer");
				flatMetallicBuffer = std::make_shared<Buffer>(backend, flatMetallics.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vpp::ONE_TIME_TRANSFER, flatMetallics.data(), "Flat metallic Buffer");
				flatRoughnessBuffer = std::make_shared<Buffer>(backend, flatRoughnesses.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vpp::ONE_TIME_TRANSFER, flatRoughnesses.data(), "Flat roughness Buffer");

				colorDescriptorSet = std::make_shared<SuperDescriptorSet>(backend, colorDescriptorSetLayout, "Color descriptor set");
				colorDescriptorSet->addBuffersToBinding({ flatAlbedoBuffer });
				colorDescriptorSet->addBuffersToBinding({ flatMetallicBuffer });
				colorDescriptorSet->addBuffersToBinding({ flatRoughnessBuffer });
				colorDescriptorSet->createDescriptorSet();
			}
			else
			{
				colorDescriptorSet = std::make_shared<SuperDescriptorSet>(backend, colorDescriptorSetLayout, "Color descriptor set");
				colorDescriptorSet->addBuffersToBinding({ defaultBuffer });
				colorDescriptorSet->addBuffersToBinding({ defaultBuffer });
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

			albedoImages.clear();
			albedoImageViews.clear();
			metallicImages.clear();
			metallicImageViews.clear();
			roughnessImages.clear();
			roughnessImageViews.clear();
			textureSampler.reset();
			vertexBuffer.reset();
			indexBuffer.reset();
			flatAlbedoBuffer.reset();
			flatMetallicBuffer.reset();
			flatRoughnessBuffer.reset();
			defaultImage.reset();
			defaultImageView.reset();
			defaultBuffer.reset();

			initialized = false;
			finished = false;
		}

		inline static std::vector<std::shared_ptr<Image>> albedoImages;
		inline static std::vector<std::shared_ptr<Image>> metallicImages;
		inline static std::vector<std::shared_ptr<Image>> roughnessImages;
		inline static std::vector<glm::vec4> flatAlbedos;
		inline static std::vector<float> flatMetallics;
		inline static std::vector<float> flatRoughnesses;

	private:
		inline static std::shared_ptr<SuperDescriptorSetLayout> textureDescriptorSetLayout;
		inline static std::shared_ptr<SuperDescriptorSetLayout> colorDescriptorSetLayout;
		inline static bool superDescriptorSetLayoutCreated = false;

		inline static uint32_t globalMaterialCount = 0;

		inline static std::vector<Vertex> vertices;
		inline static std::vector<uint32_t> indices;

		inline static std::vector<std::shared_ptr<ImageView>> albedoImageViews;
		inline static std::vector<std::shared_ptr<ImageView>> metallicImageViews;
		inline static std::vector<std::shared_ptr<ImageView>> roughnessImageViews;

		inline static std::shared_ptr<SuperDescriptorSet> textureDescriptorSet;
		inline static std::vector<uint32_t> mipLevels;
		inline static std::shared_ptr<vpp::Sampler> textureSampler;

		inline static std::shared_ptr<Buffer> flatAlbedoBuffer;
		inline static std::shared_ptr<Buffer> flatMetallicBuffer;
		inline static std::shared_ptr<Buffer> flatRoughnessBuffer;
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

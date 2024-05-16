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

	class Mesh
	{
	public:
		std::shared_ptr<vpp::Backend> backend;

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		uint32_t materialIndex;

		std::shared_ptr<vpp::Buffer> vertexBuffer;
		std::shared_ptr<vpp::Buffer> indexBuffer;
		VkDeviceMemory indexBufferMemry;

		Mesh(std::shared_ptr<vpp::Backend> backend, std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices, uint32_t materialIndex);
		~Mesh();

	private:
		void createVertexBuffer();
		void createIndexBuffer();
	};

	class Model
	{
	public:
		

		std::shared_ptr<vpp::Backend> backend;
		std::string path;
		std::string directory;

		std::vector<std::shared_ptr<SuperDescriptorSet>> superDescriptorSets;
		std::vector<std::unique_ptr<Mesh>> meshes;
		std::vector<std::shared_ptr<Image>> textureImages;
		std::vector<std::shared_ptr<ImageView>> textureImageViews;
		std::shared_ptr<vpp::Sampler> textureSampler;
		std::vector<uint32_t> mipLevels;

		Model(std::string path, std::shared_ptr<vpp::Backend> backend);
		~Model();

		inline static void createDescriptorSetLayout(std::shared_ptr<Backend> backend)
		{
			if (superDescriptorSetLayoutCreated)
				return;

			superDescriptorSetLayoutCreated = true;
			superDescriptorSetLayout = std::make_shared<SuperDescriptorSetLayout>(backend);
			superDescriptorSetLayout->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
			superDescriptorSetLayout->createLayout();
		}

		inline static std::shared_ptr<SuperDescriptorSetLayout> getSuperDescriptorSetLayout(std::shared_ptr<Backend> backend)
		{
			if(!superDescriptorSetLayoutCreated)
				createDescriptorSetLayout(backend);

			return superDescriptorSetLayout;
		}

		inline static void destroyDescriptorSetLayout(std::shared_ptr<Backend> backend)
		{
			if (superDescriptorSetLayoutCreated)
			{
				superDescriptorSetLayout.reset();
				superDescriptorSetLayoutCreated = false;
			}
		}

	private:
		inline static std::shared_ptr<SuperDescriptorSetLayout> superDescriptorSetLayout;
		inline static bool superDescriptorSetLayoutCreated = false;
	};
}

#endif // !MODEL_H

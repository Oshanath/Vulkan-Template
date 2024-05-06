#ifndef MESH_H
#define MESH_H

#include "Helper.h"

#include <iostream>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <array>
#include <memory>

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
	std::shared_ptr<Helper> helper;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	uint32_t materialIndex;

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	Mesh(std::shared_ptr<Helper> helper, std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices, uint32_t materialIndex);
	~Mesh();

private:
	void createVertexBuffer();
	void createIndexBuffer();
};

class Model
{
public:
	std::shared_ptr<Helper> helper;

	std::vector<std::unique_ptr<Mesh>> meshes;

	Model(std::string path, std::shared_ptr<Helper> helper);
	~Model();
};

#endif // !MESH_H

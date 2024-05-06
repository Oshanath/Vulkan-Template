#ifndef MESH_H

#include <iostream>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <array>

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

inline std::pair<std::vector<Vertex>, std::vector<uint32_t>> testMesh()
{
	Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile("models/sponza/Sponza.gltf",
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);

    if (nullptr == scene) {
        std::cout << "Error loading file\n";
    }

    std::cout << "File loaded\n";

    std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

    for (unsigned int i = 0; i < scene->mMeshes[0]->mNumVertices; i++) {
		aiVector3D pos = scene->mMeshes[0]->mVertices[i];
		Vertex vertex{};
		vertex.pos = { pos.x, pos.y, pos.z };
		vertices.push_back(vertex);
	}

	// Get indices of mesh 0
	for (unsigned int i = 0; i < scene->mMeshes[0]->mNumFaces; i++) 
	{
		aiFace face = scene->mMeshes[0]->mFaces[i];

		for (unsigned int j = 0; j < face.mNumIndices; j++) {
			indices.push_back(face.mIndices[j]);
		}
	}

    return std::pair<std::vector<Vertex>, std::vector<uint32_t>>(vertices, indices);
}

#endif // !MESH_H

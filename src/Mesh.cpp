#include "Mesh.h"

#include <stdexcept>


Model::Model(std::string path, std::shared_ptr<Helper> helper) : helper(helper)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);

    if (nullptr == scene) {
        throw std::runtime_error("Failed to load model");
    }

    // Populate vertices and indices
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) 
    {
		const aiMesh* mesh = scene->mMeshes[i];
        uint32_t materialIndex = mesh->mMaterialIndex;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (unsigned int j = 0; j < mesh->mNumVertices; j++) 
        {
			Vertex vertex = {};
			vertex.pos = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
			vertex.normal = glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
            if (mesh->mTextureCoords[0]) {
				vertex.texCoord = mesh->HasTextureCoords(0) ? glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y) : glm::vec2(0.0f, 0.0f);
			}
			vertices.push_back(vertex);
		}

        for (unsigned int j = 0; j < mesh->mNumFaces; j++) 
        {
			const aiFace& face = mesh->mFaces[j];

            if (face.mNumIndices != 3)
            {
				throw std::runtime_error("Model not triangulated");
			}

            for (unsigned int k = 0; k < face.mNumIndices; k++) 
            {
				indices.push_back(face.mIndices[k]);
			}
		}

        meshes.emplace_back(std::make_unique<Mesh>(helper, std::move(vertices), std::move(indices), materialIndex));
	}

}

Model::~Model()
{

}

Mesh::Mesh(std::shared_ptr<Helper> helper, std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices, uint32_t materialIndex)
    : helper(helper), vertices(vertices), indices(indices), materialIndex(materialIndex)
{
    //std::cout << "Creaing mesh buffers\n";
    createVertexBuffer();
	createIndexBuffer();
}

Mesh::~Mesh()
{
    //std::cout << "Destroying mesh buffers\n";
    vkDestroyBuffer(helper->device, vertexBuffer, nullptr);
    vkFreeMemory(helper->device, vertexBufferMemory, nullptr);

    vkDestroyBuffer(helper->device, indexBuffer, nullptr);
    vkFreeMemory(helper->device, indexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(helper->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(helper->device, stagingBufferMemory);

    helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    helper->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(helper->device, stagingBuffer, nullptr);
    vkFreeMemory(helper->device, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(helper->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(helper->device, stagingBufferMemory);

    helper->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    helper->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(helper->device, stagingBuffer, nullptr);
    vkFreeMemory(helper->device, stagingBufferMemory, nullptr);
}
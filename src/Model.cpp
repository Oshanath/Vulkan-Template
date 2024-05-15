#include "Model.h"

#include <stdexcept>

vpp::Model::Model(std::string path, std::shared_ptr<vpp::Backend> backend) : 
    backend(backend), path(path), directory(path.substr(0, path.find_last_of('/')))
{
    createDescriptorSetLayout(*backend);

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
				vertex.texCoord = mesh->HasTextureCoords(0) ? glm::vec2(mesh->mTextureCoords[0][j].x, 1-mesh->mTextureCoords[0][j].y) : glm::vec2(0.0f, 0.0f);
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

        meshes.emplace_back(std::make_unique<Mesh>(backend, std::move(vertices), std::move(indices), materialIndex));
	}

    // Populate materials
    textureImages.resize(scene->mNumMaterials);
    textureImagesMemory.resize(scene->mNumMaterials);
    textureImageViews.resize(scene->mNumMaterials);
    descriptorSets.resize(scene->mNumMaterials);
    mipLevels.resize(scene->mNumMaterials);

    backend->createSampler(textureSampler, 10);

    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        const aiMaterial* material = scene->mMaterials[i];

        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) 
        {
            aiString Path;

            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) 
            {
                std::string FullPath = directory + "/" + Path.data;

                backend->createTextureImage(FullPath, textureImages[i], textureImagesMemory[i], textureImageViews[i], &mipLevels[i]);

                // Create descriptor set
                VkDescriptorSetLayout layouts[] = { getDescriptorSetLayout() };
                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = backend->descriptorPool;
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = layouts;

                if (vkAllocateDescriptorSets(backend->device, &allocInfo, &descriptorSets[i]) != VK_SUCCESS) {
					throw std::runtime_error("Failed to allocate descriptor sets");
				}

                VkDescriptorImageInfo imageInfo = {};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = textureImageViews[i];
				imageInfo.sampler = textureSampler;

				VkWriteDescriptorSet descriptorWrite = {};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = descriptorSets[i];
				descriptorWrite.dstBinding = 0;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(backend->device, 1, &descriptorWrite, 0, nullptr);
            }
            else
            {
                throw std::runtime_error("Texture path retrieval failed");
            }
        }
        else
        {
            std::cout << "Material has no diffuse texture\n";
            break;
        }
    }

}

vpp::Model::~Model()
{
    destroyDescriptorSetLayout(*backend);

    for (unsigned int i = 0; i < textureImages.size(); i++)
    {
        vkDestroyImageView(backend->device, textureImageViews[i], nullptr);
        vkFreeMemory(backend->device, textureImagesMemory[i], nullptr);
        vkDestroyImage(backend->device, textureImages[i], nullptr);
    }

    vkDestroySampler(backend->device, textureSampler, nullptr);
}

vpp::Mesh::Mesh(std::shared_ptr<vpp::Backend> backend, std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices, uint32_t materialIndex)
    : backend(backend), vertices(vertices), indices(indices), materialIndex(materialIndex)
{
    //std::cout << "Creaing mesh buffers\n";
    createVertexBuffer();
	createIndexBuffer();
}

vpp::Mesh::~Mesh()
{
    //std::cout << "Destroying mesh buffers\n";
    vkDestroyBuffer(backend->device, vertexBuffer, nullptr);
    vkFreeMemory(backend->device, vertexBufferMemory, nullptr);

    vkDestroyBuffer(backend->device, indexBuffer, nullptr);
    vkFreeMemory(backend->device, indexBufferMemory, nullptr);
}

void vpp::Mesh::createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    backend->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(backend->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(backend->device, stagingBufferMemory);

    backend->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    backend->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(backend->device, stagingBuffer, nullptr);
    vkFreeMemory(backend->device, stagingBufferMemory, nullptr);
}

void vpp::Mesh::createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    backend->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(backend->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(backend->device, stagingBufferMemory);

    backend->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    backend->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(backend->device, stagingBuffer, nullptr);
    vkFreeMemory(backend->device, stagingBufferMemory, nullptr);
}
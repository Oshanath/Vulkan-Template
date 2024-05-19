#include "Model.h"

#include <stdexcept>

glm::mat4 convertMatrix(const aiMatrix4x4& aiMat)
{
    return {
        aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
        aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
        aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
        aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
    };
}

void vpp::Model::processNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform)
{
    glm::mat4 transform = parentTransform * convertMatrix(node->mTransformation);

    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        nodes.push_back({ node->mMeshes[i], transform });
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene, transform);
    }
}

glm::mat4 vpp::Model::getModelMatrix()
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotationAngleAxis.first), rotationAngleAxis.second);
    model = glm::scale(model, scale);
	return model;
}

vpp::Model::Model(std::string path, std::shared_ptr<vpp::Backend> backend, TextureType textureType) :
    backend(backend), path(path), directory(path.substr(0, path.find_last_of('/'))), textureType(textureType)
{
    if (!initialized)
    {
        defaultImage = std::make_shared<Image>(backend, 1, 1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Default image");
        defaultImageView = std::make_shared<ImageView>(backend, defaultImage, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, "Default Image View");
        defaultBuffer = std::make_shared<Buffer>(backend, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vpp::GPU_ONLY, nullptr, "Default SSBO");

        // transition default image
        backend->transitionImageLayout(defaultImage->image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    }

    position = glm::vec3(0.0f);
    scale = glm::vec3(1.0f);
    rotationAngleAxis = { 0.0f, glm::vec3(0.0f, 1.0f, 0.0f) };

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);

    std::cout << scene->HasTextures() << std::endl;

    aiNode* root = scene->mRootNode;
    hasTree = root->mNumChildren > 0;

    if(hasTree)
        processNode(root, scene, glm::mat4(1.0f));

    if (nullptr == scene) {
        throw std::runtime_error("Failed to load model");
    }

    // Populate vertices and indices
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) 
    {
		const aiMesh* aiMesh = scene->mMeshes[i];

        Mesh mesh;
        mesh.materialIndex = aiMesh->mMaterialIndex + albedoImages.size();
        mesh.colorIndex = aiMesh->mMaterialIndex + flatAlbedos.size();
        mesh.vertexCount = aiMesh->mNumVertices;
        mesh.indexCount = aiMesh->mNumFaces * 3;
        mesh.startVertex = vertices.size();
        mesh.startIndex = indices.size();
        meshes.push_back(mesh);

        uint32_t indexBias = vertices.size();

        for (unsigned int j = 0; j < aiMesh->mNumVertices; j++) 
        {
			Vertex vertex = {};
			vertex.pos = glm::vec3(aiMesh->mVertices[j].x, aiMesh->mVertices[j].y, aiMesh->mVertices[j].z);
			vertex.normal = glm::vec3(aiMesh->mNormals[j].x, aiMesh->mNormals[j].y, aiMesh->mNormals[j].z);
            if (aiMesh->mTextureCoords[0]) {
				vertex.texCoord = aiMesh->HasTextureCoords(0) ? glm::vec2(aiMesh->mTextureCoords[0][j].x, 1-aiMesh->mTextureCoords[0][j].y) : glm::vec2(0.0f, 0.0f);
			}
			vertices.push_back(vertex);
		}

        for (unsigned int j = 0; j < aiMesh->mNumFaces; j++) 
        {
			const aiFace& face = aiMesh->mFaces[j];

            if (face.mNumIndices != 3)
            {
				throw std::runtime_error("Model not triangulated");
			}

            for (unsigned int k = 0; k < face.mNumIndices; k++) 
            {
				indices.push_back(face.mIndices[k] + indexBias);
			}
		}
	}

    // embedded textures
    if (textureType == EMBEDDED)
    {
        mipLevels.resize(mipLevels.size() + scene->mNumTextures);

        for (unsigned int i = 0; i < scene->mNumTextures; i++)
        {
			aiTexture* texture = scene->mTextures[i];
			
            // if compressed
            if (texture->mHeight == 0)
            {
                vpp::TextureImageCreationResults result = backend->createTextureImage((stbi_uc*)texture->pcData, texture->mWidth, &mipLevels[i]);
                albedoImages.push_back(result.image);
                albedoImageViews.push_back(result.imageView);
            }
		}
	}

    if (textureType == vpp::TEXTURE)
    {
        mipLevels.resize(mipLevels.size() + scene->mNumMaterials);
    }

    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        const aiMaterial* material = scene->mMaterials[i];
        
        if (textureType == FLAT_COLOR)
        {
            aiColor3D diffuse;
            material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
            flatAlbedos.push_back(glm::vec4(diffuse.r, diffuse.g, diffuse.b, 1.0f));

            aiColor3D emissive;
            material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);

            aiColor3D transparent;
            material->Get(AI_MATKEY_COLOR_TRANSPARENT, transparent);

            aiColor3D metallic;
            material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
            flatMetallics.push_back(metallic.r);

            aiColor3D roughness;
            material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
            flatRoughnesses.push_back(roughness.r);
        }

        else if (textureType == TEXTURE || textureType == EMBEDDED)
        {
            aiString Path;

            //albedo
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                if (material->GetTexture(aiTextureType_DIFFUSE, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
                {
                    if (textureType == TEXTURE)
                    {
                        std::string FullPath = directory + "/" + Path.data;

                        vpp::TextureImageCreationResults results = backend->createTextureImage(FullPath, &mipLevels[i]);
                        albedoImages.push_back(results.image);
                        albedoImageViews.push_back(results.imageView);
                    }
                }
                else
                {
                    throw std::runtime_error("Texture path retrieval failed");
                }
			}
            else
            {
                std::cout << "No albedo texture found for material " << i << std::endl;
                albedoImages.push_back(defaultImage);
                albedoImageViews.push_back(defaultImageView);
            }

            //metallic
            if (material->GetTextureCount(aiTextureType_METALNESS) > 0)
            {
                if (material->GetTexture(aiTextureType_METALNESS, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
                {
					std::string FullPath = directory + "/" + Path.data;

					vpp::TextureImageCreationResults results = backend->createTextureImage(FullPath, &mipLevels[i]);
					metallicImages.push_back(results.image);
					metallicImageViews.push_back(results.imageView);
				}
                else
                {
					throw std::runtime_error("Texture path retrieval failed");
				}
			}
            else
            {
                std::cout << "No metallic texture found for material " << i << std::endl;
                metallicImages.push_back(defaultImage);
                metallicImageViews.push_back(defaultImageView);
            }

            //roughness
            if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0)
            {
                if (material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
                {
                    std::string FullPath = directory + "/" + Path.data;

                    vpp::TextureImageCreationResults results = backend->createTextureImage(FullPath, &mipLevels[i]);
                    roughnessImages.push_back(results.image);
                    roughnessImageViews.push_back(results.imageView);
                }
                else
                {
                    throw std::runtime_error("Texture path retrieval failed");
                }
            }
            else
            {
                std::cout << "No roughness texture found for material " << i << std::endl;
                roughnessImages.push_back(defaultImage);
                roughnessImageViews.push_back(defaultImageView);
            }
        }
    }

    initialized = true;
}
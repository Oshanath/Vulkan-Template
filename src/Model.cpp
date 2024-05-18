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

vpp::Model::Model(std::string path, std::shared_ptr<vpp::Backend> backend, TextureType textureType) :
    backend(backend), path(path), directory(path.substr(0, path.find_last_of('/'))), textureType(textureType)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);

    aiNode* root = scene->mRootNode;
    hasTree = root->mNumChildren > 0;

    if(hasTree)
        processNode(root, scene, glm::mat4(1.0f));

    std::cout << "Model has " << nodes.size() << " nodes\n";

    if (nullptr == scene) {
        throw std::runtime_error("Failed to load model");
    }

    // Populate vertices and indices
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) 
    {
		const aiMesh* aiMesh = scene->mMeshes[i];

        Mesh mesh;
        mesh.materialIndex = aiMesh->mMaterialIndex + textureImages.size();
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

    // Populate materials
    mipLevels.resize(mipLevels.size() + scene->mNumMaterials);

    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        const aiMaterial* material = scene->mMaterials[i];
        
        if (textureType == FLAT_COLOR)
        {
            aiColor3D color;
            material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
            colors.push_back(glm::vec4(color.r, color.g, color.b, 1.0f));
        }

        else if (textureType == TEXTURE)
        {
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                aiString Path;

                if (material->GetTexture(aiTextureType_DIFFUSE, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
                {
                    std::string FullPath = directory + "/" + Path.data;

                    vpp::TextureImageCreationResults results = backend->createTextureImage(FullPath, &mipLevels[i]);
                    textureImages.push_back(results.image);
                    textureImageViews.push_back(results.imageView);
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

    initialized = true;
}
#ifndef UTIL_H
#define UTIL_H

#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>
#include <optional>
#include <vector>
#include <memory>

namespace vpp
{
	class Image;
	class ImageView;

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    enum BufferType
    {
        CONTINOUS_TRANSFER,
        ONE_TIME_TRANSFER,
        GPU_ONLY
    };

    struct TextureImageCreationResults
    {
        std::shared_ptr<Image> image;
        std::shared_ptr<ImageView> imageView;
    };

    struct ViewProjectionMatrices {
        glm::mat4 view;
        glm::mat4 proj;
    };

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

		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
		{
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);

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

	struct MainPushConstants
	{
		glm::mat4 submeshTransform;
		glm::mat4 modelTransform;
		uint32_t materialIndex;
		uint32_t colorIndex;
		uint32_t textureType;
	};

	struct CameraLightInfo
	{
		glm::vec4 cameraPos;
		glm::vec4 lightDir;
	};

	struct Controls
	{
		float sunlightIntensity;
		float ambientFactor;
	};
}

#endif // !UTIL_H

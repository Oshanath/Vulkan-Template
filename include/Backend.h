#ifndef HELPER_H
#define HELPER_H

#include <vulkan/vulkan.h>
#include <string>
#include <stb_image.h>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace vpp
{
	enum BufferType
	{
		CONTINOUS_TRANSFER,
		ONE_TIME_TRANSFER,
		GPU_ONLY
	};

	class Backend
	{
	public:
		GLFWwindow* window;
		VkInstance instance;
		VkDebugUtilsMessengerEXT debugMessenger;
		VkSurfaceKHR surface;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device;

		VkCommandPool commandPool;
		VkDescriptorPool descriptorPool;

		uint32_t imageCount;
		VkSwapchainKHR swapChain;
		std::vector<VkImage> swapChainImages;
		VkFormat swapChainImageFormat;
		std::vector<VkImageView> swapChainImageViews;
		VkExtent2D swapChainExtent;
		VkRenderPass swapChainRenderPass;
		std::vector<VkFramebuffer> swapChainFramebuffers;

		std::vector<VkCommandBuffer> commandBuffers;
		VkQueue graphicsQueue;
		VkQueue presentQueue;

		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;

		VkImage depthImage;
		VkDeviceMemory depthImageMemory;
		VkImageView depthImageView;

		VkCommandBuffer beginSingleTimeCommands();
		void endSingleTimeCommands(VkCommandBuffer commandBuffer);
		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void createTextureImage(std::string path, VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& textureImageView, uint32_t* mipLevels = nullptr);
		void createImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
		VkImageView createImageView(VkImage image, uint32_t mipLevels, VkFormat format, VkImageAspectFlagBits aspectFlags);
		void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		void createSampler(VkSampler& textureSampler, uint32_t mipLevels = 0);
		void generateMipmaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
	};

	class Buffer
	{
	private:
		VkBuffer buffer;
		VkDeviceMemory bufferMemory;
		VkDeviceSize size;
		void* mappedPtr;
		Backend* backend;

		BufferType type;

	public:
		Buffer(vpp::Backend* backend, VkDeviceSize size, VkBufferUsageFlags usage, vpp::BufferType type, void* data);
		~Buffer();

		static void copyBuffer(vpp::Backend* backend, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	};
}

#endif // !HELPER_H

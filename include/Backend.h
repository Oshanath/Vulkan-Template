#ifndef HELPER_H
#define HELPER_H

#include <vulkan/vulkan.h>
#include <string>
#include <stb_image.h>
#include <vector>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace vpp
{
	class Backend;
	class Buffer;
	class Image;
	class ImageView;
	class Sampler;

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
		std::vector<std::shared_ptr<ImageView>> swapChainImageViews;
		VkExtent2D swapChainExtent;
		VkRenderPass swapChainRenderPass;
		std::vector<VkFramebuffer> swapChainFramebuffers;

		std::vector<VkCommandBuffer> commandBuffers;
		VkQueue graphicsQueue;
		VkQueue presentQueue;

		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;

		std::shared_ptr<Image> depthImage;
		std::shared_ptr<ImageView> depthImageView;

		VkCommandBuffer beginSingleTimeCommands();
		void endSingleTimeCommands(VkCommandBuffer commandBuffer);
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		std::shared_ptr<vpp::Image> createTextureImage(std::string path, uint32_t* mipLevels);
		void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	};

	class Buffer
	{
	public:
		VkBuffer buffer;
		VkDeviceMemory bufferMemory;
		VkDeviceSize size;
		void* mappedPtr;
		Backend* backend;

		BufferType type;

		Buffer(vpp::Backend* backend, VkDeviceSize size, VkBufferUsageFlags usage, vpp::BufferType type, void* data);
		~Buffer();

		static void copyBuffer(vpp::Backend* backend, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	};

	class Image
	{
	public:
		Backend* backend;
		VkImage image;
		VkDeviceMemory imageMemory;
		uint32_t mipLevels;
		VkFormat format;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		VkImageType imageType;

		Image(Backend* backend, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
		~Image();

		void generateMipMaps();
	};

	class ImageView
	{
	public:
		Backend* backend;
		VkImageView imageView;

		ImageView(Backend* backend, std::shared_ptr<Image> image, uint32_t baseMipLevel, uint32_t mipLevels, VkImageAspectFlagBits aspectFlags);
		ImageView(Backend* backend, VkImage image, uint32_t baseMipLevel, uint32_t mipLevels, VkImageAspectFlagBits aspectFlags, VkFormat format, VkImageViewType viewType);
		~ImageView();
	};

	class Sampler
	{
	public:
		Backend* backend;
		VkSampler sampler;

		Sampler(Backend* backend, uint32_t mipLevels = 0);
		~Sampler();
	};
}

#endif // !HELPER_H

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
#include <unordered_map>


namespace vpp
{
	class Backend;
	class Buffer;
	class Image;
	class ImageView;
	class Sampler;
	class SuperDescriptorSet;
	class SuperDescriptorSetLayout;

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

	class Backend : public std::enable_shared_from_this<Backend>
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
		TextureImageCreationResults createTextureImage(std::string path, uint32_t* mipLevels);
		void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	};

	class Buffer
	{
	public:
		VkBuffer buffer;
		VkDeviceMemory bufferMemory;
		VkDeviceSize size;
		void* mappedPtr;
		std::shared_ptr<Backend> backend;

		BufferType type;

		Buffer(std::shared_ptr<vpp::Backend> backend, VkDeviceSize size, VkBufferUsageFlags usage, vpp::BufferType type, void* data);
		~Buffer();

		static void copyBuffer(std::shared_ptr<vpp::Backend> backend, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	};

	class Image
	{
	public:
		std::shared_ptr<Backend> backend;
		VkImage image;
		VkDeviceMemory imageMemory;
		uint32_t mipLevels;
		VkFormat format;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		VkImageType imageType;

		Image(std::shared_ptr<Backend> backend, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
		~Image();

		void generateMipMaps();
	};

	class ImageView
	{
	public:
		std::shared_ptr<Backend> backend;
		VkImageView imageView;

		ImageView(std::shared_ptr<Backend> backend, std::shared_ptr<Image> image, uint32_t baseMipLevel, uint32_t mipLevels, VkImageAspectFlagBits aspectFlags);
		ImageView(std::shared_ptr<Backend> backend, VkImage image, uint32_t baseMipLevel, uint32_t mipLevels, VkImageAspectFlagBits aspectFlags, VkFormat format, VkImageViewType viewType);
		~ImageView();
	};

	class Sampler
	{
	public:
		std::shared_ptr<Backend> backend;
		VkSampler sampler;

		Sampler(std::shared_ptr<Backend> backend, uint32_t mipLevels = 0);
		~Sampler();
	};

	class SuperDescriptorSetLayout
	{
	public:
		std::shared_ptr<Backend> backend;
		VkDescriptorSetLayout descriptorSetLayout;
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		SuperDescriptorSetLayout(std::shared_ptr<Backend> backend);
		~SuperDescriptorSetLayout();

		void addBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount);
		void createLayout();

		inline VkDescriptorSetLayout getDescriptorSetLayout() { return descriptorSetLayout; }
		inline bool isLayoutCreated() { return layoutCreated; }

	private:
		bool layoutCreated;
	};

	class SuperDescriptorSet
	{
	public:
		std::shared_ptr<Backend> backend;
		std::shared_ptr<SuperDescriptorSetLayout> superDescriptorSetLayout;
		VkDescriptorSet descriptorSet;

		SuperDescriptorSet(std::shared_ptr<Backend> backend, std::shared_ptr<SuperDescriptorSetLayout> superDescriptorSetLayout);
		~SuperDescriptorSet();

		void addImageToBinding(std::vector<std::shared_ptr<ImageView>> imageViews, std::vector<std::shared_ptr<Sampler>> samplers, std::vector<VkImageLayout> imageLayouts);
		void addBufferToBinding(std::vector<std::shared_ptr<Buffer>> buffers);
		void createDescriptorSet();

	private:
		uint32_t currentBinding = 0;
		std::unique_ptr<std::unordered_map<uint32_t, std::vector<VkDescriptorImageInfo>>> imageInfos;
		std::unique_ptr<std::unordered_map<uint32_t, std::vector<VkDescriptorBufferInfo>>> bufferInfos;


	};
}

#endif // !HELPER_H

#ifndef HELPER_H
#define HELPER_H

#include <vulkan/vulkan.h>
#include <string>
#include <stb_image.h>

class Helper
{
public:
	VkCommandPool commandPool;
	VkDevice device;
	VkQueue graphicsQueue;
	VkPhysicalDevice physicalDevice;
	VkDescriptorPool descriptorPool;

	Helper(VkCommandPool commandPool, VkDevice device, VkQueue graphicsQueue, VkPhysicalDevice physicalDevice, VkDescriptorPool descriptorPool);
	Helper();

	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void createTextureImage(std::string path, VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& textureImageView);
	void createImage(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlagBits aspectFlags);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createSampler(VkSampler& textureSampler);
};


#endif // !HELPER_H

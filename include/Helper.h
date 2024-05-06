#ifndef HELPER_H
#define HELPER_H

#include <vulkan/vulkan.h>

class Helper
{
public:
	VkCommandPool commandPool;
	VkDevice device;
	VkQueue graphicsQueue;
	VkPhysicalDevice physicalDevice;

	Helper(VkCommandPool commandPool, VkDevice device, VkQueue graphicsQueue, VkPhysicalDevice physicalDevice);
	Helper();

	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};

#endif // !HELPER_H

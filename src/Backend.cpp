#include "Backend.h"
#include <stdexcept>
#include <cmath>
#include <iostream>

VkCommandBuffer vpp::Backend::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void vpp::Backend::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t vpp::Backend::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

VkShaderModule vpp::Backend::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

vpp::TextureImageCreationResults vpp::Backend::createTextureImage(std::string path, uint32_t* mipLevels)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    return createTextureImage(pixels, texWidth, texHeight, texChannels, mipLevels);
}

vpp::TextureImageCreationResults vpp::Backend::createTextureImage(stbi_uc* unloadedPixels, uint32_t size, uint32_t* mipLevels)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load_from_memory((stbi_uc*)unloadedPixels, size, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    return createTextureImage(pixels, texWidth, texHeight, texChannels, mipLevels);
}

vpp::TextureImageCreationResults vpp::Backend::createTextureImage(stbi_uc* pixels, int texWidth, int texHeight, int texChannels, uint32_t* mipLevels)
{
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    uint32_t levels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    if (mipLevels) *mipLevels = levels;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    Buffer stagingBuffer(shared_from_this(), imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, CONTINOUS_TRANSFER, nullptr, "Staging buffer for texture");
    memcpy(stagingBuffer.mappedPtr, pixels, static_cast<size_t>(imageSize));
    stbi_image_free(pixels);

    std::shared_ptr<Image> image = std::make_shared<Image>(shared_from_this(), texWidth, texHeight, 1, (mipLevels ? levels : 1), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Texture image");

    // Transition image layout
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = image->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = (mipLevels ? levels : 1);
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(commandBuffer);

    copyBufferToImage(stagingBuffer.buffer, image->image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    image->generateMipMaps();

    std::shared_ptr<ImageView> imageView = std::make_shared<ImageView>(shared_from_this(), image, 0, (mipLevels ? levels : 1), VK_IMAGE_ASPECT_COLOR_BIT, "Texture image view");

    return { image, imageView };
}

void vpp::Backend::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

/* ------------------------------------------------------------
    Buffer ----------------------------------------------------
 ------------------------------------------------------------ */

void vpp::Buffer::copyBuffer(std::shared_ptr<vpp::Backend> backend, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = backend->beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    backend->endSingleTimeCommands(commandBuffer);
}

vpp::Buffer::Buffer(std::shared_ptr<vpp::Backend> backend, VkDeviceSize size, VkBufferUsageFlags usage, vpp::BufferType type, void* data, std::string name):
    backend(backend), size(size), type(type)
{

    if (type == GPU_ONLY)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(backend->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(backend->device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        allocInfo.memoryTypeIndex = backend->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(backend->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(backend->device, buffer, bufferMemory, 0);
    }

    else if (type == CONTINOUS_TRANSFER)
    {
        
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(backend->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(backend->device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        allocInfo.memoryTypeIndex = backend->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(backend->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(backend->device, buffer, bufferMemory, 0);

        vkMapMemory(backend->device, bufferMemory, 0, size, 0, &mappedPtr);
    }

    else if (type == ONE_TIME_TRANSFER)
    {
        Buffer stagingBuffer(backend, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, CONTINOUS_TRANSFER, nullptr, "stagin buffer for texture");
        memcpy(stagingBuffer.mappedPtr, data, (size_t)size);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(backend->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(backend->device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        allocInfo.memoryTypeIndex = backend->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(backend->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(backend->device, buffer, bufferMemory, 0);

        copyBuffer(backend, stagingBuffer.buffer, buffer, size);
    }

    backend->setNameOfObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, name);
}

void vpp::Backend::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	endSingleTimeCommands(commandBuffer);
}

vpp::Buffer::~Buffer()
{
    if (type == CONTINOUS_TRANSFER)
    {
		vkUnmapMemory(backend->device, bufferMemory);
	}

	vkDestroyBuffer(backend->device, buffer, nullptr);
	vkFreeMemory(backend->device, bufferMemory, nullptr);
}

vpp::Image::Image(std::shared_ptr<Backend> backend, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::string name):
    backend(backend), width(width), height(height), depth(depth), mipLevels(mipLevels), format(format), imageType(depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = imageType;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = depth;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(backend->device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(backend->device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = backend->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(backend->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(backend->device, image, imageMemory, 0);

    backend->setNameOfObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)image, name);
}

vpp::Image::~Image()
{
	vkDestroyImage(backend->device, image, nullptr);
	vkFreeMemory(backend->device, imageMemory, nullptr);
}

void vpp::Image::generateMipMaps()
{
    VkCommandBuffer commandBuffer = backend->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = width;
    int32_t mipHeight = height;

    for (uint32_t i = 1; i < mipLevels; i++)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    backend->endSingleTimeCommands(commandBuffer);
}

vpp::ImageView::ImageView(std::shared_ptr<Backend> backend, std::shared_ptr<Image> image, uint32_t baseMipLevel, uint32_t mipLevels, VkImageAspectFlagBits aspectFlags, std::string name)
    : backend(backend)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image->image;
    viewInfo.viewType = image->imageType == VK_IMAGE_TYPE_3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = image->format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(backend->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    backend->setNameOfObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)imageView, name);
}

vpp::ImageView::ImageView(std::shared_ptr<Backend> backend, VkImage image, uint32_t baseMipLevel, uint32_t mipLevels, VkImageAspectFlagBits aspectFlags, VkFormat format, VkImageViewType viewType, std::string name)
    : backend(backend)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = viewType;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(backend->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    backend->setNameOfObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)imageView, name);
}

vpp::ImageView::~ImageView()
{
	vkDestroyImageView(backend->device, imageView, nullptr);
}

vpp::Sampler::Sampler(std::shared_ptr<Backend> backend, uint32_t mipLevels, std::string name):
    backend(backend)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(backend->physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f; // Optional
    samplerInfo.maxLod = static_cast<float>(mipLevels);
    samplerInfo.mipLodBias = 0.0f; // Optional

    if (vkCreateSampler(backend->device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    backend->setNameOfObject(VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler, name);
}

vpp::Sampler::~Sampler()
{
	vkDestroySampler(backend->device, sampler, nullptr);
}

vpp::SuperDescriptorSetLayout::SuperDescriptorSetLayout(std::shared_ptr<Backend> backend, std::string name):
	backend(backend), layoutCreated(false), name(name)
{
}

vpp::SuperDescriptorSetLayout::~SuperDescriptorSetLayout()
{
    vkDestroyDescriptorSetLayout(backend->device, descriptorSetLayout, nullptr);
}

void vpp::SuperDescriptorSetLayout::addBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount)
{
    if (layoutCreated) throw std::runtime_error("Cannot add binding after layout creation.");

    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = bindings.size();
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = descriptorCount;
    layoutBinding.stageFlags = stageFlags;
    layoutBinding.pImmutableSamplers = nullptr;

    bindings.push_back(layoutBinding);
}

void vpp::SuperDescriptorSetLayout::createLayout()
{
    if (layoutCreated) throw std::runtime_error("Layout already created.");

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(backend->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    layoutCreated = true;

    backend->setNameOfObject(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)descriptorSetLayout, name);
}

vpp::SuperDescriptorSet::SuperDescriptorSet(std::shared_ptr<Backend> backend, std::shared_ptr<SuperDescriptorSetLayout> textureDescriptorSetLayout, std::string name):
    backend(backend), textureDescriptorSetLayout(textureDescriptorSetLayout), name(name)
{
    currentBinding = 0;
    imageInfos = std::make_unique<std::unordered_map<uint32_t, std::vector<VkDescriptorImageInfo>>>();
    bufferInfos = std::make_unique<std::unordered_map<uint32_t, std::vector<VkDescriptorBufferInfo>>>();
}

vpp::SuperDescriptorSet::~SuperDescriptorSet()
{
}

void vpp::SuperDescriptorSet::addImagesToBinding(std::vector<std::shared_ptr<ImageView>> imageViews, std::vector<std::shared_ptr<Sampler>> samplers, std::vector<VkImageLayout> imageLayouts)
{
    if(!textureDescriptorSetLayout->isLayoutCreated()) throw std::runtime_error("Descriptor set layout not created.");

    if (imageViews.size() != textureDescriptorSetLayout->bindings[currentBinding].descriptorCount || 
        samplers.size() != textureDescriptorSetLayout->bindings[currentBinding].descriptorCount ||
        imageLayouts.size() != textureDescriptorSetLayout->bindings[currentBinding].descriptorCount)
    {
		throw std::runtime_error("Binding's image view count, sampler count and image layout count must match layout's descriptor count.");
    }

    if ((textureDescriptorSetLayout->bindings[currentBinding].descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && textureDescriptorSetLayout->bindings[currentBinding].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE))
    {
		throw std::runtime_error("Binding's descriptor type must be VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER or VK_DESCRIPTOR_TYPE_STORAGE_IMAGE.");
    }

    std::vector<VkDescriptorImageInfo> imageInfoVector(textureDescriptorSetLayout->bindings[currentBinding].descriptorCount);
    for (uint32_t i = 0; i < textureDescriptorSetLayout->bindings[currentBinding].descriptorCount; i++)
	{
        imageInfoVector[i].imageLayout = imageLayouts[i];
        imageInfoVector[i].imageView = imageViews[i]->imageView;
        imageInfoVector[i].sampler = samplers[i]->sampler;
	}
    (*imageInfos)[currentBinding] = std::move(imageInfoVector);
    currentBinding++;
}

void vpp::SuperDescriptorSet::addBuffersToBinding(std::vector<std::shared_ptr<Buffer>> buffers)
{
    if (!textureDescriptorSetLayout->isLayoutCreated()) throw std::runtime_error("Descriptor set layout not created.");

    if (buffers.size() != textureDescriptorSetLayout->bindings[currentBinding].descriptorCount)
    {
        throw std::runtime_error("Binding's buffer count(" + std::to_string(buffers.size()) + ") does not match descriptor count(" + std::to_string(textureDescriptorSetLayout->bindings[currentBinding].descriptorCount) + ").");
    }

    if ((textureDescriptorSetLayout->bindings[currentBinding].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && textureDescriptorSetLayout->bindings[currentBinding].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER))
    {
        throw std::runtime_error("Binding's descriptor type must be VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER.");
    }

    std::vector<VkDescriptorBufferInfo> bufferInfoVector(textureDescriptorSetLayout->bindings[currentBinding].descriptorCount);
    for (uint32_t i = 0; i < textureDescriptorSetLayout->bindings[currentBinding].descriptorCount; i++)
	{
		bufferInfoVector[i].buffer = buffers[i]->buffer;
		bufferInfoVector[i].offset = 0;
		bufferInfoVector[i].range = buffers[i]->size;
	}
	(*bufferInfos)[currentBinding] = std::move(bufferInfoVector);
    currentBinding++;
}

void vpp::SuperDescriptorSet::createDescriptorSet()
{
    if(!textureDescriptorSetLayout->isLayoutCreated()) throw std::runtime_error("Descriptor set layout not created.");

	if (currentBinding != textureDescriptorSetLayout->bindings.size()) throw std::runtime_error("Not all bindings have been filled.");
	
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = backend->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureDescriptorSetLayout->descriptorSetLayout;

    if (vkAllocateDescriptorSets(backend->device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // Write descriptor sets
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    for (uint32_t j = 0; j < textureDescriptorSetLayout->bindings.size(); j++)
    {
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = j;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = textureDescriptorSetLayout->bindings[j].descriptorType;
        descriptorWrite.descriptorCount = textureDescriptorSetLayout->bindings[j].descriptorCount;
        descriptorWrite.pBufferInfo = nullptr;
        descriptorWrite.pImageInfo = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;

        if (textureDescriptorSetLayout->bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || textureDescriptorSetLayout->bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            descriptorWrite.pBufferInfo = bufferInfos->at(j).data();
        }
        else if (textureDescriptorSetLayout->bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || textureDescriptorSetLayout->bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        {
            descriptorWrite.pImageInfo = imageInfos->at(j).data();
        }

        descriptorWrites.push_back(descriptorWrite);
    }

    vkUpdateDescriptorSets(backend->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    imageInfos.reset();
    bufferInfos.reset();

    backend->setNameOfObject(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSet, name);
}
#pragma once

#include <fstream>

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 2;

const std::vector<const char* > deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex data representation
struct Vertex {
    glm::vec3 pos;  // Vertex position (x, y, z)
    glm::vec3 col;  // Vertex colour (r, g, b)
};

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentationFamily = -1;

    bool isValid() {
        return graphicsFamily >= 0 && presentationFamily >= 0;
    }

    bool sameFamily() {
        return graphicsFamily == presentationFamily;
    }
};

struct SwapChainDetails {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;               // Surface properties, e.g. image size/extent
    std::vector<VkSurfaceFormatKHR> surfaceFormats;             // Surface image formats e.g. RGBA and size of each color
    std::vector<VkPresentModeKHR> presentationModes;            // How images should be presented to screen
};

struct SwapChainImage {
    VkImage image;
    VkImageView imageView;
};

static std::vector<char> readFile(const std::string& filename)
{
    //  std::ios::binary: open binary file
    //  std::ios::ate   : read from end of the file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open the file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> fileBuffer(fileSize);

    // Move position to beginning of the file
    file.seekg(0);

    file.read(fileBuffer.data(), fileSize);

    file.close();

    return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        // 000000000100100011011    - shifting over all possible devices

        if ((allowedTypes & (1 << i))                                                           // Index of memory type must match corresponding bit in allowedTypes
            && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties     // Desired property bit flags are part of memory type's property flags
            )
        {
            return i;       // This memory type is valid
        }
    }
}

static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage
    , VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
    VkBufferCreateInfo bufferInfo = {};                         // Information to create a buffer (doesn't include assign memory)
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;                               // Size of buffer
    bufferInfo.usage = bufferUsage;                             // Multiple types of buffers possible
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;         // Similar to swapchain images, can share vertex buffers

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Vertex Buffer!");
    }

    // GET BUFFER MEMORY REQUIREMENTS
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

    // ALLOCATE MEMEORY TO BUFFER
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, bufferProperties);    // Index of memory type on Physical Device that has required bit flags

    result = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, bufferMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allcate Vertex Buffer Memory!");
    }

    vkBindBufferMemory(device, *buffer, *bufferMemory, 0);

}

static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
    // Command Buffer to hold transfer commands
    VkCommandBuffer commandBuffer;

    // Command Buffer Details
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;

    // Allocate command buffer from pool
    vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // Using Command Buffer Once - one time submit (create, transfer commands, destroy)

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    return commandBuffer;
}

static void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    // Queue Submission Information
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Submit transfer command to transfer queue and wait until it finishes
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // Free temporary command buffer
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
    VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
    // Create Buffer
    VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

    // Origin of data to copy from and to
    VkBufferCopy bufferCopyRegion = {};
    bufferCopyRegion.srcOffset = 0;
    bufferCopyRegion.dstOffset = 0;
    bufferCopyRegion.size = bufferSize;

    // Command to copy src buffer to dst buffer
    vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

    endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

    VkBufferImageCopy imageRegion = {};
    imageRegion.bufferOffset = 0;                                           // Offset into data
    imageRegion.bufferRowLength = 0;                                        // Row length of data to calculate data spacing
    imageRegion.bufferImageHeight = 0;                                      // Image height to calculate data spacing
    imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;    // What aspect of image to copy
    imageRegion.imageSubresource.mipLevel = 0;                              // Mipmap level to copy
    imageRegion.imageSubresource.baseArrayLayer = 0;                        // Starting array layer
    imageRegion.imageSubresource.layerCount = 1;                            // Number of layers to copy starting at baseArrayLayer
    imageRegion.imageOffset = { 0, 0, 0 };                                  // Offset into image (as opposed to raw data in bufferOffset)
    imageRegion.imageExtent = { width, height, 1 };                         // Size of region to copy as (x, y, z) values

    // Copy Buffer to given image
    vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

    endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = oldLayout;                                    // Layout to transition from
    imageMemoryBarrier.newLayout = newLayout;                                    // Layout to transition to
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;            // Queue family to transition from
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;            // Queue family to transition to
    imageMemoryBarrier.image = image;                                            // Image being accessed and modified as part of barrier
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // Aspect of image being altered
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;                        // First mip level to start alterations on
    imageMemoryBarrier.subresourceRange.levelCount = 1;                          // Number of mip levels to alter starting from baseMipLevel
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;                      // First layer to start alterations on
    imageMemoryBarrier.subresourceRange.layerCount = 1;                          // Number of layers to alter starting from baseArrayLayer

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    // If transitioning from new image to image to receive data
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = 0;                                    // Memory access stage transition must after...
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;         // Memory access stage transition must before...

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    // If transitioning from transfer destination to shader readable
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStage, dstStage,                    // Pipeline stages (match to src and dst AccessMask)
        0,                                     // Dependency Flags
        0, nullptr,                            // Memory Barrier Count + data
        0, nullptr,                            // Buffer Memory Barrier + data
        1, &imageMemoryBarrier                 // Image Memory Barrier + data
    );

    endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}
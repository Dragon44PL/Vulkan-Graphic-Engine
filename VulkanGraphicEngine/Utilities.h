#pragma once

#include <fstream>

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;

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

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
    VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
    // Command Buffer to hold transfer commands
    VkCommandBuffer transferCommandBuffer;

    // Command Buffer Details
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool = transferCommandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;

    // Allocate command buffer from pool
    vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &transferCommandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // Using Command Buffer Once - one time submit (create, transfer commands, destroy)

    vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

    // Origin of data to copy from and to
    VkBufferCopy bufferCopyRegion = {};
    bufferCopyRegion.srcOffset = 0;
    bufferCopyRegion.dstOffset = 0;
    bufferCopyRegion.size = bufferSize;

    // Command to copy src buffer to dst buffer
    vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

    vkEndCommandBuffer(transferCommandBuffer);

   // Queue Submission Information
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transferCommandBuffer;

    // Submit transfer command to transfer queue and wait until it finishes
    vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(transferQueue);

    // Free temporary command buffer
    vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}

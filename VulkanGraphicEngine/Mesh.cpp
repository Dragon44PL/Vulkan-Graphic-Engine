#include "Mesh.h"

Mesh::Mesh()
{

}

Mesh::~Mesh()
{

}

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<Vertex>* vertices)
{
    vertexCount = vertices->size();
    this->physicalDevice = physicalDevice;
    this->device = device;
    createVertexBuffer(vertices);
}

int Mesh::getVertexCount()
{
    return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
    return vertexBuffer;
}

void Mesh::destroyVertexBuffer()
{
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer(std::vector<Vertex> * vertices)
{
    VkBufferCreateInfo bufferInfo = {};                         // Information to create a buffer (doesn't include assign memory)
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(Vertex) * vertices->size();         // Size of buffer
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;       // Multiple types of buffers possible
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;         // Similar to swapchain images, can share vertex buffers

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Vertex Buffer!");
    }

    // GET BUFFER MEMORY REQUIREMENTS
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memoryRequirements);

    // ALLOCATE MEMEORY TO BUFFER
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);    // Index of memory type on Physical Device that has required bit flags

    result = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &vertexBufferMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allcate Vertex Buffer Memory!");
    }

    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

    // MAP MEMORY TO VERTEX BUFFER
    void * data;                                                                // 1. Create pointer to a point in normal memory
    vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);      // 2. Map the Vertex Buffer Memory to the pointer - create a relationship between these two
    memcpy(data, vertices->data(), (size_t) bufferInfo.size);                    // 3. Copy memory from vertices to the pointer - data is in vertexBufferMemory
    vkUnmapMemory(device, vertexBufferMemory);                                  // 4. Unmap the vertex buffer memory - destroy relationship 
}                                                                                                                                                                               // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : CPU can interract with memory
                                                                                                                                                                                // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : Allow placement of data straight into buffer after mapping (otherwise would have to specify manually) 
uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties)
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

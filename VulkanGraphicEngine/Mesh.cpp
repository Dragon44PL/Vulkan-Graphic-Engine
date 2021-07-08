#include "Mesh.h"

Mesh::Mesh()
{

}

Mesh::~Mesh()
{

}

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue,
    VkCommandPool transferCommandPool, std::vector<Vertex>* vertices, std::vector<uint32_t>* indices)
{
    vertexCount = vertices->size();
    indexCount = indices->size();
    this->physicalDevice = physicalDevice;
    this->device = device;
    createVertexBuffer(transferQueue, transferCommandPool, vertices);
    createIndexBuffer(transferQueue, transferCommandPool, indices);

    uboModel.model = glm::mat4(1.0f);
}

void Mesh::setModel(glm::mat4 model)
{
    uboModel.model = model;
}

UboModel Mesh::getModel()
{
    return uboModel;
}

int Mesh::getVertexCount()
{
    return vertexCount;
}

int Mesh::getIndexCount()
{
    return indexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
    return vertexBuffer;
}

VkBuffer Mesh::getIndexBuffer()
{
    return indexBuffer;
}

void Mesh::destroyBuffers()
{
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex> * vertices)
{
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();

    // Staging Buffer - temporary - "stage" vertex data before transferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory staggingBufferMemory;

    // Create Staging Buffer and allocate memory to it
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  : CPU can interact with memory 
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : Allow placement of data straight into buffer after mapping (otherwise would have to specify manually)
    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &staggingBufferMemory);

    // MAP MEMORY TO VERTEX BUFFER
    void * data;                                                                  // 1. Create pointer to a point in normal memory
    vkMapMemory(device, staggingBufferMemory, 0, bufferSize, 0, &data);           // 2. Map the Vertex Buffer Memory to the pointer - create a relationship between these two
    memcpy(data, vertices->data(), (size_t) bufferSize);                          // 3. Copy memory from vertices to the pointer - data is in vertexBufferMemory
    vkUnmapMemory(device, staggingBufferMemory);                                  // 4. Unmap the vertex buffer memory - destroy relationship 

    // Create Buffer with TRANSFER_DST_BIT to mark as recipient of transfer data
    // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : Only visible to GPU
    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, &vertexBufferMemory);

    // Copy staging buffer to vertex buffer on GPU
    copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

    // Clean up staging buffer parts
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, staggingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices)
{
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();

    // Temporary Buffer to Stage vertext data before transferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory staggingBufferMemory;
    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &staggingBufferMemory);

    // MAP MEMORY TO VERTEX BUFFER
    void* data;
    vkMapMemory(device, staggingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices->data(), (size_t)bufferSize);
    vkUnmapMemory(device, staggingBufferMemory);

    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, &indexBufferMemory);

    // Copy staging buffer to GPU access buffer
    copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

    // Destroy + Release staging buffer parts
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, staggingBufferMemory, nullptr);
}

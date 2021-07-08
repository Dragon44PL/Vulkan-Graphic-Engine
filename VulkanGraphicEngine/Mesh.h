#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

struct UboModel {
    // Where the object is positioned in the world
    // Identity matrix : Leave everything where it is
    glm::mat4 model;
};

class Mesh
{
public:
    Mesh();
    Mesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue,
        VkCommandPool transferCommandPool, std::vector<Vertex> * vertices, std::vector<uint32_t> * indices);

    void setModel(glm::mat4 model);
    UboModel getModel();

    int getVertexCount();
    int getIndexCount();
    VkBuffer getVertexBuffer();
    VkBuffer getIndexBuffer();

    void destroyBuffers();

    ~Mesh();

private:
    UboModel uboModel;

    int vertexCount;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    int indexCount;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkPhysicalDevice physicalDevice;
    VkDevice device;

    void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex> * vertices);
    void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices);
};


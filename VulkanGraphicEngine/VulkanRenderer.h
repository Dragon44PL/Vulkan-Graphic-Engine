#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "stb_image.h"

#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <array>

#include "Mesh.h"
#include "Utilities.h"

class VulkanRenderer
{
public:
    VulkanRenderer();

    int init(GLFWwindow* window);
    void updateModel(int modelId, glm::mat4 model);
    void draw();
    void cleanup();

    ~VulkanRenderer();

private:
    GLFWwindow * window;
    int currentFrame = 0;

    // Scene Objects
    std::vector<Mesh> meshes;

    // Scene Settings

    struct UboViewProjection {
        glm::mat4 projection;           // How camera views the world (depth - 3D, flat - 2D)
        glm::mat4 view;                 // Where camera is viewing from and which direction it is viewing
    } uboViewProjection;
    
    // Vulkan Components
    VkInstance instance;

    struct {
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
    } mainDevice;

    VkQueue graphicsQueue;
    VkQueue presentationQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<SwapChainImage> swapChainImages;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;

    VkImage depthBufferImage;
    VkDeviceMemory depthBufferImageMemory;
    VkImageView depthBufferImageView;

    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkBuffer> vpUniformBuffer;
    std::vector<VkDeviceMemory> vpUniformBufferMemory;

    //std::vector<VkBuffer> modelUniformBuffer;
    //std::vector<VkDeviceMemory> modelUniformBufferMemory;

    VkPushConstantRange pushConstantRange;

    //VkDeviceSize minUniformBufferOffset;
    //size_t modelUniformAlignment;
    //Model* modelTransferSpace;

    // - Assets
    std::vector<VkImage> textureImages;
    std::vector<VkDeviceMemory> textureImagesMemory;

    VkFormat swapChainFormat;
    VkExtent2D swapChainExtent;

    // Synchronisation
    std::vector<VkSemaphore> imageAvailable;
    std::vector<VkSemaphore> renderFinished;
    std::vector<VkFence> drawFences;

    // Pipeline
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;

    // Pools
    VkCommandPool graphicsCommandPool;

    // Vulkan Functions
    // - Create Functions
    void createInstance();
    void createLogicalDevice();
    void createSurface();
    void createSwapchain();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createPushConstantRange();
    void createGraphicsPipeline();
    void createDepthBufferImage();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSynchronisation();

    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();

    void updateUniformBuffers(uint32_t imageIndex);

    // - Record Functions
    void recordCommands(uint32_t currentImage);

    // - Get Functions
    void getPhysicalDevice();

    // - Allocate Functions
    void allocateDynamicBufferTransferSpace();

    // - Checker Functions


    // - Support Functions
    //  -- Checker Functions
    bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool checkDeviceSuitable(VkPhysicalDevice device);

    //  -- Getter Functions
    QueueFamilyIndices getQueueFamilies(VkPhysicalDevice physicalDevice);
    SwapChainDetails getSwapChainDetails(VkPhysicalDevice physicalDevice);

    // -- Choose Functions
    VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> formats);
    VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentations);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &surfaceCapabilities);
    VkFormat chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

    // -- Create Functions
    VkImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceMemory *imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkShaderModule createShaderModule(const std::vector<char> shaders);

    int createTexture(std::string filename);

    // -- Loader Functions
    stbi_uc* loadTextureFile(std::string filename, int* width, int* height, VkDeviceSize* imageSize);
};


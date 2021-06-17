#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>

#include "Utilities.h"

class VulkanRenderer
{
public:
    VulkanRenderer();

    int init(GLFWwindow* window);
    void cleanup();

    ~VulkanRenderer();

private:
    GLFWwindow * window;

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

    VkFormat swapChainFormat;
    VkExtent2D swapChainExtent;

    // Vulkan Functions
    // - Create Functions
    void createInstance();
    void createLogicalDevice();
    void createSurface();
    void createSwapchain();
    void createGraphicsPipeline();

    // - Get Functions
    void getPhysicalDevice();

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

    // -- Create Functions
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkShaderModule createShaderModule(const std::vector<char> shaders);

};


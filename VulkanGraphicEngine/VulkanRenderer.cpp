
#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer()
{

}

int VulkanRenderer::init(GLFWwindow* window)
{
    this->window = window;

    try
    {
        createInstance();
        createSurface();
        getPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
    }
    catch (const std::runtime_error &e)
    {
        printf("ERROR: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::cleanup()
{
    for (SwapChainImage image : swapChainImages)
    {
        vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
    }

    vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(mainDevice.logicalDevice, nullptr);
    vkDestroyInstance(instance, nullptr);
}

VulkanRenderer::~VulkanRenderer()
{

}

void VulkanRenderer::createInstance()
{
    // Create VkApplication Info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Graphic Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    // Create VkInstanceInfo
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> instanceExtensions = std::vector<const char*>();
    
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (size_t i = 0; i < glfwExtensionCount; i++)
    {
        instanceExtensions.push_back(glfwExtensions[i]);
    }

    if (!checkInstanceExtensionSupport(&instanceExtensions))
    {
        throw std::runtime_error("vkInstance does not support required extensions");
    }

    createInfo.enabledExtensionCount = instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan Instance");
    }
}

void VulkanRenderer::createLogicalDevice()
{
    QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };

    for (int queueFamilyIndex : queueFamilyIndices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        float priority = 1.0f;                                             // 1 = highest available priority
        queueCreateInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Informations to create logical device
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Logical Device");
    }

    vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::createSurface()
{
    // Creating Surface By GLFW
    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a surface!");
    }
}

void VulkanRenderer::createSwapchain()
{
    SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

    // Find Best Surface Format
    VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.surfaceFormats);
    // Find Best Presentation Mode
    VkPresentModeKHR presentationMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
    // Find Swap Chain Image Resolution
    VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

    // One more than the minimum to allow triple buffering
    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

    if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
    {
        imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainInfo = {};
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = surface;
    swapChainInfo.imageFormat = surfaceFormat.format;
    swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainInfo.presentMode = presentationMode;
    swapChainInfo.imageExtent = extent;
    swapChainInfo.minImageCount = imageCount;
    swapChainInfo.imageArrayLayers = 1;                                                     // Number of layers for each image in chain
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;                         // What attachment images will be used as
    swapChainInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;     // Trasnform to perform on swap chain images
    swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;                       // How to handle blending images with external graphics (e.g. other windows)
    swapChainInfo.clipped = VK_TRUE;                                                        // Whether to clip parts of image not in view (e.g behind another window, off screen) - not processing/showing that parts which are not actually visible

    QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

    if (!indices.sameFamily())
    {
        uint32_t queueFamilyIndices[] = {
            (uint32_t) indices.graphicsFamily,
            (uint32_t) indices.presentationFamily
        };

        swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;            // Image Share handling
        swapChainInfo.queueFamilyIndexCount = 2;                                // Number of queues to share images between
        swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;                 // Array of queues to share between
    }
    else
    {
        swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainInfo.queueFamilyIndexCount = 0;
        swapChainInfo.pQueueFamilyIndices = nullptr;
    }

    // If old swap chain been destroyed and this one replaces it, then link old one to quickly hand over responsibilities (e.g resizing, destroying swapchain and recreate)
    swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Faile to create a swapchain!");
    }

    swapChainFormat = surfaceFormat.format;
    swapChainExtent = extent;

    // Get SwapChain Images 
    uint32_t swapChainImageCount = 0;
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);
    std::vector<VkImage> images(swapChainImageCount);
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());

    for (VkImage image : images)
    {
        SwapChainImage swapChainImage = {};
        swapChainImage.image = image;
        swapChainImage.imageView = createImageView(image, swapChainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        swapChainImages.push_back(swapChainImage);
    }
}

void VulkanRenderer::getPhysicalDevice()
{
    uint32_t devicesCount = 0;
    vkEnumeratePhysicalDevices(instance, &devicesCount, nullptr);

    if (devicesCount == 0)
    {
        throw std::runtime_error("Cannot find any GPUs that support Vulkan Instance");
    }

    std::vector<VkPhysicalDevice> physicalDevices(devicesCount);
    vkEnumeratePhysicalDevices(instance, &devicesCount, physicalDevices.data());

    //TODO: Get recommended device
    mainDevice.physicalDevice = physicalDevices[0];
    for (const auto& physicalDevice : physicalDevices)
    {
        if (checkDeviceSuitable(physicalDevice))
        {
            mainDevice.physicalDevice = physicalDevice;
            break;
        }
    }
}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    for (const auto& checkExtension : *checkExtensions) {

        bool hasExtension = false;
        for (const auto& extension : extensions)
        {
            if (strcmp(checkExtension, extension.extensionName))
            {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension)
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    // Failure if no extension found
    if (extensionCount == 0)
    {
        return false;
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    for (const auto &deviceExtension : deviceExtensions)
    {
        bool hasExtension = false;
        for (const auto& extension : extensions)
        {
            if (strcmp(deviceExtension, extension.extensionName))
            {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension)
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
    /*
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    */

    QueueFamilyIndices indices = getQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainValid = false;

    if (extensionsSupported)
    {
        SwapChainDetails swapChainDetails = getSwapChainDetails(device);
        swapChainValid = !swapChainDetails.presentationModes.empty() && swapChainDetails.surfaceFormats.empty();
    }

    return indices.isValid() && extensionsSupported && swapChainValid;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice physicalDevice)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyList.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilyList)
    {
        // Check if the Device Queue Family supports defined queues and 
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        // Check If Queue Familly supports presentation
        VkBool32 presentationSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentationSupport);
        if (queueFamily.queueCount > 0 && presentationSupport)
        {
            indices.presentationFamily = i;
        }

        // Check if the Queue Family is found for given device is found
        if (indices.isValid())
        {
            break;
        }

        i++;
    }

    return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice physicalDevice)
{
    SwapChainDetails swapChainDetails;

    //Get the Surface Capabilities for the given surface on the given physical device
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &swapChainDetails.surfaceCapabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    // If formats returned, get lists of formats
    if (formatCount != 0)
    {
        swapChainDetails.surfaceFormats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, swapChainDetails.surfaceFormats.data());   
    }

    uint32_t presentationCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentationCount, nullptr);

    // If presentation modes returned, get list of presentation modes
    if (presentationCount != 0)
    {
        swapChainDetails.presentationModes.resize(formatCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentationCount, swapChainDetails.presentationModes.data());
    }

    return swapChainDetails;
}

// Format      :  VK_FORMAT_R8G8B8A8_UNORM
// Color Space :  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> formats)
{
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }

    for (const auto &format : formats)
    {
        if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    return formats[0];
}

// Returns:
// If mode found :   VK_PRESENT_MODE_MAILBOX_KHR
// If not        :   VK_PRESENT_MODE_FIFO_KHR
VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentations)
{
    for (const auto &presentation : presentations)
    {
        if (presentation == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return presentation;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR & surfaceCapabilities)
{
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return surfaceCapabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D extent = {};
        extent.width = static_cast<uint32_t>(width);
        extent.height = static_cast<uint32_t>(height);

        extent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, extent.height));

        return extent;
    }
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;   // Allows remapping of rgba components to other rgba values
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // Subresources allow the view to view only a part of an image
    imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;      // Which aspect of image to view (e.g. COLOR_BIT for viewing colour)
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;              // Start mipmap level to view from
    imageViewCreateInfo.subresourceRange.levelCount = 1;                // Number of mipmap levels to view
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;            // Start array level to view from
    imageViewCreateInfo.subresourceRange.layerCount = 1;                // Number of array levels to view

    // Create image view
    VkImageView imageView;
    VkResult result = vkCreateImageView(mainDevice.logicalDevice, &imageViewCreateInfo, nullptr, &imageView);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create an Image View!");
    }

    return imageView;
}

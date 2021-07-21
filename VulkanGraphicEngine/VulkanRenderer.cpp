
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
        createRenderPass();
        createDescriptorSetLayout();
        createPushConstantRange();
        createGraphicsPipeline();
        createDepthBufferImage();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createTextureSampler();
        //allocateDynamicBufferTransferSpace();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createSynchronisation();

        // First  : Angle of the camera
        // Second : Aspect Ratio
        // Third  : How close could be seen
        // Fourth : Haw far could be seen
        uboViewProjection.projection = glm::perspective(glm::radians(40.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);

        // First  : Where camera is
        // Second : What camera is looking at
        // Third  : Angle of camera
        uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // Reverse the Y-axis
        // GLM is not working with Vulkan - upside down
        uboViewProjection.projection[1][1] *= -1;

        // Vertex Data
        std::vector<Vertex> meshVertices = {
            {{-0.4, 0.4, 0.0}, {0.0f, 0.0f, 0.1f}, {1.0f, 1.0f}},             // 0
            {{-0.4, -0.4, 0.0}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },           // 1
            {{ 0.4, -0.4, 0.0}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},            // 2    
            {{ 0.4, 0.4, 0.0}, {0.0f, 0.0f, 0.1f}, {0.0f, 1.0f}},             // 3
        };

        std::vector<Vertex> anotherMeshVertices = {
            {{-0.25, 0.6, 0.0}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},            // 0
            {{-0.25, -0.4, 0.0}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},           // 1
            {{0.25, -0.6, 0.0}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},            // 2    
            {{0.25, 0.6, 0.0}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},             // 3
        };

        // Index Data
        std::vector<uint32_t> meshIndices = {
            0, 1, 2,
            2, 3, 0
        };

        int firstTexture = createTexture("wall_brick_plain.tga");
        int secondTexture = createTexture("wall_brick_plain.tga");

        meshes.push_back(Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue,
            graphicsCommandPool, &meshVertices, &meshIndices, firstTexture));

        meshes.push_back(Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue,
            graphicsCommandPool, &anotherMeshVertices, &meshIndices, secondTexture));
    }
    catch (const std::runtime_error &e)
    {
        printf("ERROR: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 model)
{
    if (modelId >= meshes.size())
        return;

    meshes[modelId].setModel(model);
}

void VulkanRenderer::draw()
{
    // Wait for given fence to signal (open) from last draw before continuing
    vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
    // Manually reset (close) fences
    vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

    // 1. Get next available image to draw to and set something to signal when we're finished with the image (a semaphore)
    uint32_t imageIndex;                // Index of the next image to be draw to
    vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

    recordCommands(imageIndex);
    updateUniformBuffers(imageIndex);

    // 2. Submit a command buffer to queue for execution, make sure it waits for the image to be signalled as available before drawing and signals when it has finished rendering
    // Queue Submission Info
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;                            // Number of semaphores to wait for
    submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];   // List of semaphores
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.pWaitDstStageMask = waitStages;                    // Wait stages - if pipeline reaches this stages, then looks if "imageAvailable" semaphore is true, stages to check semaphores at
    submitInfo.commandBufferCount = 1;                            // Number of command Buffers to submit
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];     // Command Buffer to submit
    submitInfo.signalSemaphoreCount = 1;                          // Number of semaphores to signal
    submitInfo.pSignalSemaphores = &renderFinished[currentFrame]; // Semaphores to signal when command buffer finishes

    VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit Command Buffer to Queue!");
    }

    // 3. Present image to the screen when it signalled finished rendering
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;                             // Number of semaphores to wait for
    presentInfo.pWaitSemaphores = &renderFinished[currentFrame];    // Semaphores to wait for
    presentInfo.swapchainCount = 1;                                 // Number of swapchains to present to
    presentInfo.pSwapchains = &swapchain;                           // Swapchain to present images to
    presentInfo.pImageIndices = &imageIndex;                        // Index of images in swapchains to present

    result = vkQueuePresentKHR(presentationQueue, &presentInfo);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present Image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::cleanup()
{
    // Wait before no action being run on device before destroying
    vkDeviceWaitIdle(mainDevice.logicalDevice);

    //_aligned_free(modelTransferSpace);

    vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

    vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

    for (size_t i = 0; i < textureImages.size(); i++)
    {
        vkDestroyImageView(mainDevice.logicalDevice, textureImageView[i], nullptr);
        vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
        vkFreeMemory(mainDevice.logicalDevice, textureImagesMemory[i], nullptr);
    }

    vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView, nullptr);
    vkDestroyImage(mainDevice.logicalDevice, depthBufferImage, nullptr);
    vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory, nullptr);

    vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
        vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
        //vkDestroyBuffer(mainDevice.logicalDevice, modelUniformBuffer[i], nullptr);
        //vkFreeMemory(mainDevice.logicalDevice, modelUniformBufferMemory[i], nullptr);
    }
    for (size_t i = 0; i < meshes.size(); i++)
    {
        meshes[i].destroyBuffers();
    }
    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
        vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
        vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
    }
    vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
    for (auto framebuffer : swapChainFramebuffers)
    {
        vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
    }
    vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
    for (auto image : swapChainImages)
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
    deviceFeatures.samplerAnisotropy = VK_TRUE;                             // Enable Anisotropy

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

void VulkanRenderer::createRenderPass()
{
    // ATTACHMENTS
    // Colour Attachment of render pass
    VkAttachmentDescription colourAttachment = {};
    colourAttachment.format = swapChainFormat;                                      // Format to use for attachments
    colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                               // Number of samples to write for multisampling
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                          // clear - when we start render pass, clear up images andd start drawing again - what to do with attachments before rendering
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;                        // store - store somewhere data - what to do with attachments after rendering
    colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;               // what to do with stencil before rendering
    colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;             // what to do with stencil after rendering

    // Framebuffer will be stored as an image, but images can be given different data layouts
    // to give optimal use for certain operations
    colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                     // Image data layout before render pass starts
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;                 // Image data layout after render pass (to change to)

    // Depth Attachment of render pass
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = chooseSupportedFormat(
        { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // REFERENCES
    // Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
    VkAttachmentReference colourAttachmentReference = {};
    colourAttachmentReference.attachment = 0;                                       // Position of attachment in array
    colourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;    // Optimal for colourAttachment

    // Depth Attachment reference
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Information about particular subpasses
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;                    // Pipeline type subpass is to be bound to
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colourAttachmentReference;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;

    // Need to determine when layout transitions occur using subpass dependencies -
    std::array<VkSubpassDependency, 2> subpassDependencies;

    // Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    // Transition must happen after...
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;                                                             // Anything that takes place outside the subpasses - outside of the renderpass
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;                                          // At the end of the pipeline - Which stage of the pipeline has to happen before this transition can take place
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;                                                    // Has to be read from before we can do that conversion

    // Transition must happen before...
    subpassDependencies[0].dstSubpass = 0;                                                                               // First Subpass on the list
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;                                 // Before the color output of the main subpass
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;   // Conversion has to happen before we get to the color output and before it attempts to read and write to the attachment
    subpassDependencies[0].dependencyFlags = 0;

    // Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    // Transition must happen after...
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Transition must happen before...
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = 0;

    std::array<VkAttachmentDescription, 2> renderPassAttachments = { colourAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
    renderPassCreateInfo.pAttachments = renderPassAttachments.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Render Pass!");
    }
}

void VulkanRenderer::createDescriptorSetLayout()
{
    // UNIFORM VALUES DESCRIPTOR SET LAYOUT
    // ViewProjection Binding Info
    VkDescriptorSetLayoutBinding vpLayoutBinding = {};
    vpLayoutBinding.binding = 0;                                           // Binding point in shader (designated in binding number in shader)
    vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;    // Type of Descriptor (uniform, dynamic uniform, image sampler, etc)
    vpLayoutBinding.descriptorCount = 1;                                   // Number of descriptors for binding - just one data (mvp)
    vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;               // Shader stage to bind to
    vpLayoutBinding.pImmutableSamplers = nullptr;                          // For texture : Can make sampler data unchangeable (immutable) by specifying in layout

    // Model Binding Info
    /*VkDescriptorSetLayoutBinding modelLayoutBinding = {};
    modelLayoutBinding.binding = 1;
    modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelLayoutBinding.descriptorCount = 1;
    modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    modelLayoutBinding.pImmutableSamplers = nullptr;
    */

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
        vpLayoutBinding, //modelLayoutBinding
    };

    // Create Descriptor Set Layout with given bindings
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pBindings = layoutBindings.data();
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());

    VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Descriptor Set Layout!");
    }

    // CREATE TEXTURE SAMPLER DESCRIPTOR SET LAYOUT

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
    textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutCreateInfo.bindingCount = 1;
    textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

    result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, & samplerSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Sampler Descriptor Set Layout");
    }
}

void VulkanRenderer::createPushConstantRange()
{
    // Define push constant values
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;          // Shader stage push constant will go to
    pushConstantRange.offset = 0;                                       // Offset into given data to pass to push constant
    pushConstantRange.size = sizeof(Model);                             // Size of data being passed
}

void VulkanRenderer::createGraphicsPipeline()
{
    // Read in SPIR-V code of shaders
    auto vertexShaderCode = readFile("Shaders/vert.spv");
    auto fragmentShaderCode = readFile("Shaders/frag.spv");

    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

    // Vertex Stage Creation Information
    VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
    vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;                              // Shader Stage Name
    vertexShaderCreateInfo.module = vertexShaderModule;                                     // Shader Module to be used by stage
    vertexShaderCreateInfo.pName = "main";                                                  // Entry point in to shader

    // Fragment Stage Creation Information
    VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
    fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;                              // Shader Stage Name
    fragmentShaderCreateInfo.module = fragmentShaderModule;                                     // Shader Module to be used by stage
    fragmentShaderCreateInfo.pName = "main";                                                    // Entry point in to shader

    // Shader Create Infos for Pipeline
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

    // How to data for a single vertex (including info such as position, colour, texture coords, normals etc.)
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;                             // Can bind multiple streams of data, this defines which one
    bindingDescription.stride = sizeof(Vertex);                 // Size of a single Vertex object
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // How to move between data after each vertex
                                                                // VK_VERTEX_INPUT_RATE_VERTEX   -  Move on to the next vertex
                                                                // VK_VERTEX_INPUT_RATE_INSTANCE -  Move to a vertex for the next instance

    // How the data for an attribute is defined within a vertex
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

    // Position Attribute
    attributeDescriptions[0].binding = 0;                               // Which binding the data is at (should be the same as above)
    attributeDescriptions[0].location = 0;                              // Location in shader where data will be read from
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;    // Format the data will take (also helps define size of data)
    attributeDescriptions[0].offset = offsetof(Vertex, pos);            // Where this attribute is defined in the data for a single vertex

    // Colour Attribute
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, col);

    // Texture Attribute
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, tex);

     // VERTEX INPUT
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;                                         // Description About the actual data itself (e.g. data spacing/stride informations)
    vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();                              // Vertex attribute descriptions (data format and where to bind to/from)
    vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());

    // INPUT ASSEMBLY
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
    inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;                                  // Allowing overriding of "strip" topology to start new primitives

    // VIEWPORT & SCISSOR  -- RENDER IMAGE TO PROPER PLACE ON THE SCREEN (E.G MIDDLE OF) - LOCAL MULTIPLAYER GAMES WITH SPLITTED SCREEN INTO NUMBER OF PLAYERS
    VkViewport viewport = {};
    viewport.x = 0.0f;                                   // Starting x position
    viewport.y = 0.0f;                                   // Starting y position
    viewport.width = (float) swapChainExtent.width;      // Viewport width of image width
    viewport.height = (float) swapChainExtent.height;    // Viewport height of image height
    viewport.minDepth = 0.0f;                            // Min framebuffer depth
    viewport.maxDepth = 1.0f;                            // Max framebuffer depth

    // Create a scissor info struct - everything between offset and extent is going to be visible
    VkRect2D scissor = {};                              
    scissor.offset = { 0,0 };                           // Offset to use region from
    scissor.extent = swapChainExtent;                   // Extent to describe region to use, starting at offset

    VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
    viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportCreateInfo.scissorCount = 1;
    viewportCreateInfo.pViewports = &viewport;
    viewportCreateInfo.scissorCount = 1;
    viewportCreateInfo.pScissors = &scissor;

    /*
    // -- DYNAMIC STATES --
    // Dynamic states to enable
    std::vector<VkDynamicState> dynamicStatesEnables;
    dynamicStatesEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);          // Dynamic Viewport - Can resize in command buffer with vkCmdSetViewport(commandBuffer, 0, 1, &newVievport);  
    dynamicStatesEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);           // Dynamic Scissor - Can resize in command buffer with vkCmdSetScissor(commandBuffer, 0, 1 &newScissor);

    // When resizing - destroy current swapchain and then pass it to new swapchain with resized images - resize everything

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStatesEnables.size());
    dynamicStateCreateInfo.pDynamicStates = dynamicStatesEnables.data();
    */

    // -- RASTERIZER - creating fragments from data
    VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
    rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationCreateInfo.depthClampEnable = VK_FALSE;                    // Change if fragments beyond near/far planes are clipped (default) or clamped to plane - need to enable feature for physical device
    rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;             // Discard all data and not create fragments - when only calculations from previous stages, no rendering out on the screen. Without framebuffer output
    rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;             // How to handle filling points between vertices - Fill: Fill everything, Line: Fill lines between vertex
    rasterizationCreateInfo.lineWidth = 1.0f;                               // How thick lines should be when drawn
    rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;               // Behaviour when trying to draw back side of polygons - BACK_BIT: do not draw
    rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;    // Which side of polygon is a front size
    rasterizationCreateInfo.depthBiasEnable = VK_FALSE;                     // Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping)

    // -- MULTISAMPLING
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
    multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;               // Enable Multisample shading or not
    multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // Number of samples to use per fragment - how many to take from sort of the surrounding area, in center of that particular fragment - won't take any other samples

    // -- BLENDING
    // Blending decides how to blend a new colour being written to a fragment, with the old value - combined color of top color and colour under the first one

    // Blend attachment state - how blending is handled
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    // Which colors are we going to apply doing blending operations
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_TRUE;

    // Blending uses equation: (srcColorBlendFactor * new colour) colorBlendOp (dstColorBlendFactor * old colour)
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;

    // Summarised: (VK_BLEND_FACTOR_SRC_ALPHA * new colour) VK_BLEND_OP_ADD (VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA * old colour)
    //             (new colour alpha * new colour) + ((1 - new colour alpha) * old colour)

    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;   // replace old alpha (dst) with the new one (src)
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    // Summarised: (1 * new alpha) + (0 * old alpha) = new alpha

    VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {};
    colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendCreateInfo.logicOpEnable = VK_FALSE;      // Alternative to calculations is to use logical operations
    colorBlendCreateInfo.attachmentCount = 1;
    colorBlendCreateInfo.pAttachments = &colorBlendAttachmentState;

    // -- PIPELINE LAYOUT

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
        descriptorSetLayout, samplerSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    // Create pipeline Layout
    VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Pipeline Layout!");
    }

    // -- DEPTH STENCIL TESTING
    // TODO: Set Up depth stencil testing
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;                                             // Enable checking depth to determine fragment write
    depthStencilInfo.depthWriteEnable = VK_TRUE;                                            // Enable writing to depth buffer (to replace old values)
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;                                   // Comparision operation that allows an overwrite
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;                                      // Depth Bound Test : Does the depth value exist between two bounds
    depthStencilInfo.stencilTestEnable = VK_FALSE;                                           // Enable Stencil Test

    // Create Graphics Pipeline
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.stageCount = 2;                                              // Number of shader stages
    graphicsPipelineCreateInfo.pStages = shaderStages;
    graphicsPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
    graphicsPipelineCreateInfo.pViewportState = &viewportCreateInfo;
    graphicsPipelineCreateInfo.pDynamicState = nullptr;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
    graphicsPipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilInfo;
    graphicsPipelineCreateInfo.layout = pipelineLayout;
    graphicsPipelineCreateInfo.renderPass = renderPass;                                     // Render pass description the pipeline is compatible with
    graphicsPipelineCreateInfo.subpass = 0;                                                 // Subpass of render pass to use with pipeline

    // Pipeline Derivatives : Can Create multiple pipelines that derive from one another for optimisation 
    graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;                         // Existing pipeline to derive from - referenced original when created - does not take so much memory - good when creating a lot of pipelines
    graphicsPipelineCreateInfo.basePipelineIndex = -1;                                      // Or index of pipeline being created to derive from - creating multiple pipelines at once - index of pipelines that others would be based on

    // VkPipelineCache - Create or re-create pipeline using cache - multiple pipelines - speed up creation
    result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Graphics Pipeline!");
    }

    // Destroying Shader Modules
    vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
}

void VulkanRenderer::createDepthBufferImage()
{
    // Get supported format for depth buffer
    VkFormat depthFormat = chooseSupportedFormat(
        { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    depthBufferImage = createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMemory);

    // Create Depth Buffer Image View
    depthBufferImageView = createImageView(depthBufferImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::createFramebuffers()
{
    swapChainFramebuffers.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
    {
        // Proper order - first in render pass, first in framebuffer
        std::array<VkImageView, 2> attachments = {
            swapChainImages[i].imageView,
            depthBufferImageView
        };

        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;                                              // Render pass layout the Framebuffer will be used with
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();                                    // List of attachments (1:1 with Render Pass)
        framebufferCreateInfo.width = swapChainExtent.width;                                        // Framebuffer width
        framebufferCreateInfo.height = swapChainExtent.height;                                      // Framebuffer height
        framebufferCreateInfo.layers = 1;                                                           // Framebuffer Layers

        VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFramebuffers[i]);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a Framebuffer!");
        }
    }
}

void VulkanRenderer::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;      // Command Buffers could be reset - possibility to re-record
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;         // Queue Family type that buffers from this command pool will use

    VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &commandPoolCreateInfo, nullptr, &graphicsCommandPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Command Pool!");
    }
}

void VulkanRenderer::createCommandBuffers()
{
    // One for each framebuffer
    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo cbAllocateInfo = {};
    cbAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocateInfo.commandPool = graphicsCommandPool;
    cbAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;                             // Primary - only executed by the queue, can't be called by other buffers
                                                                                        // Secondary - executed by other CommandBuffer, can't pass to the queue, secondaries are executed by primary
    cbAllocateInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocateInfo, commandBuffers.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Command Buffers!");
    }
}

void VulkanRenderer::createSynchronisation()
{
    imageAvailable.resize(MAX_FRAME_DRAWS);
    renderFinished.resize(MAX_FRAME_DRAWS);
    drawFences.resize(MAX_FRAME_DRAWS);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a semaphore!");
        }
    }
}

void VulkanRenderer::createTextureSampler()
{
    // Sampler Creation Info
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;                         // How to render when image is magnified on screen (how to draw, when texture is closer to the screen)
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;                         // How to render when image is minified on screen (how to draw, when texture is further to the screen)
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;        // How to handle texture wrap in U (x) direction
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;        // How to handle texture wrap in U (y) direction
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;        // How to handle texture wrap in U (z) direction
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;       // Border beyond texture (only works for border clamp)
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;                   // Whether coords should be normalized (between 0 and 1)
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;           // Mipmap interpolation mode
    samplerCreateInfo.mipLodBias = 0.0f;                                    // Level of details bias for mip level
    samplerCreateInfo.minLod = 0.0f;                                        // Minimum level of Detail to pick mip level
    samplerCreateInfo.maxLod = 0.0f;                                        // Maximum level of Detail to pick mip level
    samplerCreateInfo.anisotropyEnable = VK_TRUE;                           // Enable Anisotropy
    samplerCreateInfo.maxAnisotropy = 16;                                   // Anisotropy sample level

    VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Texture Sampler!");
    }
}

void VulkanRenderer::createUniformBuffers()
{
    // View Proejction Buffer Size
    VkDeviceSize vpbufferSize = sizeof(UboViewProjection);

    // Model Buffer Size
    //VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;

    // One uniform buffer for each image (and by extension, command buffer)
    vpUniformBuffer.resize(swapChainImages.size());
    vpUniformBufferMemory.resize(swapChainImages.size());

    //modelUniformBuffer.resize(swapChainImages.size());
    //modelUniformBufferMemory.resize(swapChainImages.size());

    // Create Uniform Buffer
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpbufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffer[i], &vpUniformBufferMemory[i]);

        /*createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelUniformBuffer[i], &modelUniformBufferMemory[i]);*/
    }
    
}

void VulkanRenderer::createDescriptorPool()
{
    // CREATE UNIFORM DESCRIPTOR POOL
    // Create Descriptor Pool Size
    // ViewProjection Pool
    // Type of descriptor + how many DESCRIPTORS, not Descriptor Sets (combined makes the pool size)
    VkDescriptorPoolSize vpDescriptorPoolSize = {};
    vpDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpDescriptorPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

    // Dynamic Pool
    /*VkDescriptorPoolSize modelPoolSize = {};
    modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelPoolSize.descriptorCount = static_cast<uint32_t>(modelUniformBuffer.size());*/

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = { 
        vpDescriptorPoolSize, //modelPoolSize
    };

    // Create Descriptor Pool
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());                    // Maximum number of descriptor sets that can be created from pool
    descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();                                    // Pool sizes to create pool with
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());          // Amount of pool sizes being passed

    VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Descriptor Pool!");
    }

    // CREATE SAMPLER DESCRIPTOR POOL
    // Texture Sampler Pool
    VkDescriptorPoolSize samplerPoolSize = {};
    samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerPoolSize.descriptorCount = MAX_OBJECTS;                                  // One Texture per Object

    VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
    samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    samplerPoolCreateInfo.maxSets = MAX_OBJECTS;                                    // One Set for each object - Texture atlas : One texture combined of multiple textures, smaller amount of sets, or array
    samplerPoolCreateInfo.poolSizeCount = 1;
    samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

    result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Sampler Descriptor Pool");
    }
}

void VulkanRenderer::createDescriptorSets()
{
    // Resize Descriptor Set list so one for every buffer
    descriptorSets.resize(swapChainImages.size());

    // Create list of copies of descriptionSetLayout
    // Every Descriptor Set uses different layout - first descriptor Set - first Layout etc - one to one relationship
    std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), descriptorSetLayout);

    // Descriptor Set Allocation Info
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;                                          // Pool to allocate descriptor sets from
    descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());       // Number of sets to allocate
    descriptorSetAllocateInfo.pSetLayouts = setLayouts.data();                                          // Layouts to use to allocate sets

    // Allocate Descriptor Sets
    VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocateInfo, descriptorSets.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Descriptor Sets!");
    }

    // Update all of descriptor sets buffer bindings
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        // ViewProejction Descriptor
        // Buffer Info and Data Offset Info
        VkDescriptorBufferInfo vpBufferInfo = {};
        vpBufferInfo.buffer = vpUniformBuffer[i];                           // Buffer to get data from
        vpBufferInfo.offset = 0;                                            // Position of start of data
        vpBufferInfo.range = sizeof(UboViewProjection);                     // Size of data

        // Information about connection between binding and buffer
        VkWriteDescriptorSet vpSetWrite = {};
        vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vpSetWrite.dstSet = descriptorSets[i];                              // Descriptor Set to update
        vpSetWrite.dstBinding = 0;                                          // Binding to update (mateches with binding on layout/shader)
        vpSetWrite.dstArrayElement = 0;                                     // Index in array to update
        vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;      // Type of descriptor
        vpSetWrite.descriptorCount = 1;                                     // Amount to update
        vpSetWrite.pBufferInfo = &vpBufferInfo;                             // Information about buffer data to bind

        // Model Descriptor
        // Model Buffer Binding Info
        /*VkDescriptorBufferInfo modelBufferInfo = {};
        modelBufferInfo.buffer = modelUniformBuffer[i];
        modelBufferInfo.offset = 0;
        modelBufferInfo.range = modelUniformAlignment;

        VkWriteDescriptorSet modelSetWrite = {};
        modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        modelSetWrite.dstSet = descriptorSets[i];
        modelSetWrite.dstBinding = 1;
        modelSetWrite.dstArrayElement = 0;
        modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        modelSetWrite.descriptorCount = 1;
        modelSetWrite.pBufferInfo = &modelBufferInfo;*/

        std::vector<VkWriteDescriptorSet> setWrites = { 
            vpSetWrite, //modelSetWrite
        };

        // Update Decriptor Sets with new buffer/binding info
        vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
    }
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
    // Update all uniform buffer memory with current ModelViewProjection matrix data
    void* data;
    vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
    memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
    vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

    // Copy model data
    /*
    for (size_t i = 0; i < meshes.size(); i++)
    {
        Model* model = (Model *) ( (uint64_t) modelTransferSpace + (i * modelUniformAlignment));
        *model = meshes[i].getModel();
    }

    vkMapMemory(mainDevice.logicalDevice, modelUniformBufferMemory[imageIndex], 0, modelUniformAlignment * meshes.size(), 0, &data);
    memcpy(data, modelTransferSpace, modelUniformAlignment * meshes.size());
    vkUnmapMemory(mainDevice.logicalDevice, modelUniformBufferMemory[imageIndex]);
    */
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
    // Information about how to begin each command buffer
    VkCommandBufferBeginInfo bufferBeginInfo = {};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;    // Buffer can be resubmitted when it has already been submitted and is awaiting execution

    // Information about how to begin a render pass (only needed for graphical applications)
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset = { 0, 0 };                        // Start Point of render pass in pixels
    renderPassBeginInfo.renderArea.extent = swapChainExtent;                 // Size of region to run render pass on (starting at offset)

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = { 0.6f, 0.65f, 0.4f, 1.0f };
    clearValues[1].depthStencil.depth = 1.0f;

    renderPassBeginInfo.pClearValues = clearValues.data();                              // List of clear values
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    
    renderPassBeginInfo.framebuffer = swapChainFramebuffers[currentImage];

    // Starts recording commands to command buffer
    // Reset command buffers if created
    VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to start recording a Command Buffer!");
    }

    // Begin Render Pass
    // Cmd - commands to record
    // renderPass.loadOp called
    vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind Pipeline to be used in render pass (to draw to at the moment)
    vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        for (size_t j = 0; j < meshes.size(); j++)
        {
            VkBuffer vertexBuffers[] = { meshes[j].getVertexBuffer() };                             // Buffers to bind
            VkDeviceSize offsets[] = { 0 };                                                         // Offsets into buffers being bound

            vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);     // Command to bind Vertex Buffer before drawing with them

            // Bind Mesh Index Buffer with 0 offset and using uint32 type
            vkCmdBindIndexBuffer(commandBuffers[currentImage], meshes[j].getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            // Dynamic Offset Amount
            // uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * j;

            // "Push" constants to given shader stage directly (no buffer)
            Model model = meshes[j].getModel();
            vkCmdPushConstants(
                commandBuffers[currentImage],
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,                                         // Stage to push constants to
                0,                                                                  // Offset of push constants to update
                sizeof(Model),                                                      // Size of data being pushed
                &model);                                                            // Actual data being pushed (can be array)

            std::array<VkDescriptorSet, 2> descriptorSetGroup = { descriptorSets[currentImage], samplerDescriptorSets[meshes[j].getTextureIndex()] };

            // Bind Descriptor Sets
            // Graphics Pipeline
            vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

            // Execute Pipeline
            // Vertex Count - Number of vertex to draw
            // Instance Count - good for drawing object multiple times - calling shaders multiple times - offsets in shaders
            //vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(firstMesh.getVertexCount()), 1, 0, 0);

            vkCmdDrawIndexed(commandBuffers[currentImage], meshes[j].getIndexCount(), 1, 0, 0, 0);
        }

    // End Render Pass
    // renderPass.storeOp called
    vkCmdEndRenderPass(commandBuffers[currentImage]);

    // Stops recording commands to command buffer
    result = vkEndCommandBuffer(commandBuffers[currentImage]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to end recording a Command Buffer");
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

    // Get properties of physical device
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);
    //minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
    // Calculate alignment of model data
    // For 32 -> 000100000 -> (minus 1) -> 000011111 -> (bitwise) -> 11110000 (mask)
    /*modelUniformAlignment = (sizeof(Model) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);

    // Create space in memory to hold dynamic buffer that is aligned to our required alignment and hold MAX_OBJECTS
    modelTransferSpace = (Model*)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);*/
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
    */

    // Information about what the device can do (geo shader, tess shader, wide lines, etc.)
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    QueueFamilyIndices indices = getQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainValid = false;
    if (extensionsSupported)
    {
        SwapChainDetails swapChainDetails = getSwapChainDetails(device);
        swapChainValid = !swapChainDetails.presentationModes.empty() && swapChainDetails.surfaceFormats.empty();
    }

    return indices.isValid() && extensionsSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
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

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
    for (VkFormat format : formats)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

        // Depending on tiling choice, need to check for different bit flag
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find a matching format!");
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceMemory* imageMemory)
{
    // Create Image

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;                   // 3D - Volumetric Data
    imageCreateInfo.extent.width = width;                           // Width of image extent
    imageCreateInfo.extent.height = height;                         // Height of image extent
    imageCreateInfo.extent.depth = 1;                               // Depth of image (just 1, no 3D aspect)
    imageCreateInfo.mipLevels = 1;                                  // Number of mipmap levels
    imageCreateInfo.arrayLayers = 1;                                // Number of levels in image array
    imageCreateInfo.format = format;
    imageCreateInfo.tiling = tiling;                                // How image data should be "tiled" (aranged for optimal reading)
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // Layout of image data on creation
    imageCreateInfo.usage = useFlags;                               // Bit flags defining what image will be used for
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;                // Number of samples for multisampling
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;        // Whether image can be shared between queues

    VkImage image;
    VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Image!");
    }

    // Create Memory for image

    // Get Memory Requirements for a type of image
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propertyFlags);

    result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocateInfo, nullptr, imageMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allcoate memory for image!");
    }

    // Connect memory for image
    vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

    return image;
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

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char> shaders)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = shaders.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(shaders.data());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a shader module!");
    }

    return shaderModule;
}

int VulkanRenderer::createTextureImage(std::string filename)
{
    int width, height;
    VkDeviceSize imageSize;
    stbi_uc* imageData = loadTextureFile(filename, &width, &height, &imageSize);

    // Create staging buffer to hold loaded data, ready to copy to device
    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingBufferMemory;
    createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &imageStagingBuffer, &imageStagingBufferMemory);

    // Copy image data to staging buffer
    void* data;
    vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, imageData, static_cast<size_t>(imageSize));
    vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

    // Free original image data
    stbi_image_free(imageData);

    // Create image to hold final data
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;

    textureImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureImageMemory);


    // Transition image to be DST for copy operation
    transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy data to image
    copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, textureImage, width, height);

    // Transition image to be shader readable for shader usage
    transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Add texture data to vector for reference
    textureImages.push_back(textureImage);
    textureImagesMemory.push_back(textureImageMemory);

    // Destroy staging buffer
    vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
    vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

    // Return index of new texture image
    return textureImages.size() - 1;
}

int VulkanRenderer::createTexture(std::string filename)
{
    // Create Texture Image 
    int textureImageLoc = createTextureImage(filename);

    // Create Image View
    VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    textureImageView.push_back(imageView);

    // Create texture descriptor
    int textureDescriptorLoc = createTextureDescriptor(imageView);

    // Return location of set with texture
    return textureDescriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImage)
{
    VkDescriptorSet descriptorSet;
    
    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = samplerDescriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &samplerSetLayout;

    // Allocate Descriptor Set
    VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Texture Descriptor Sets!");
    }

    // Texture Image Info
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;   // Image layout when in use - transition from DESTINATION_OPTIMAL TO SHADER_READ_ONLY_OPTIMAL
    imageInfo.imageView = textureImage;                                 // Image to bind to set   
    imageInfo.sampler = textureSampler;                                 // Sampler to use for set

    // Descriptor Write Info
    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    // Update new descriptor set
    vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

    // Add descriptor set to list
    samplerDescriptorSets.push_back(descriptorSet);

    // Return descriptor set location
    return samplerDescriptorSets.size() - 1;
}

stbi_uc* VulkanRenderer::loadTextureFile(std::string filename, int * width, int * height, VkDeviceSize* imageSize)
{
    // Number of channels image uses
    int channels;
    
    // load pixel data for image
    std::string fileLoc = "Textures/" + filename;
    stbi_uc* image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

    if (!image)
    {
        throw std::runtime_error("Failed to load a Texture file (" + filename + ")");
    }

    // Mulitply by number of channels
    *imageSize = *width * *height * 4;

    return image;
}
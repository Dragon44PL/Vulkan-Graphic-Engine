#pragma once

const std::vector<const char* > deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
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

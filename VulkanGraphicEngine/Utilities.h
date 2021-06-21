#pragma once

#include <fstream>

const int MAX_FRAME_DRAWS = 2;

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

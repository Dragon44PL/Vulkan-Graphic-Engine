#pragma once

#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <iostream>
#include <string>
#include <iostream>

#include "VulkanRenderer.h"

const std::string title = "Test Window";
GLFWwindow* window;
VulkanRenderer vulkanRenderer;

void initWindow(std::string name = "", const int width = 800, const int height = 600) {

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
}

int main() {

    initWindow(title.c_str(), 800, 600);

    if (vulkanRenderer.init(window) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    // Rotation
    float angle = 0.0f;
    float deltaTime = 0.0f;
    float lastTime = 0.0f;

    // Frames Per Second

    float framesDeltaTime = 0.0f;
    float framesLastTime = 0.0f;
    int framesCounter = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float now = glfwGetTime();
        deltaTime = now - lastTime;
        lastTime = now;

        framesDeltaTime = now - framesLastTime;

        angle += 10.0f * deltaTime;
        if (angle > 360.0f) {
            angle -= 360.0f;
        }

        framesCounter += 1;
        if (framesDeltaTime >= 1.0f)
        {
            double fps = double(framesCounter) / deltaTime;
            std::string windowTitle = title + " [ fps: " + std::to_string(fps) + " ]";
            glfwSetWindowTitle(window, windowTitle.c_str());
            framesCounter = 0;
            framesLastTime = 0;
        }

        glm::mat4 firstModel(1.0f);
        glm::mat4 secondModel(1.0f);

        //firstModel = glm::translate(firstModel, glm::vec3(-2.0f, 0.0f, -5.0f));
        firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));

        //secondModel = glm::translate(secondModel, glm::vec3(2.0f, 0.0f, -5.0f));
        secondModel = glm::rotate(secondModel, glm::radians(-angle * 100), glm::vec3(0.0f, 0.0f, 1.0f));

        vulkanRenderer.updateModel(0, firstModel);
        vulkanRenderer.updateModel(1, secondModel);

        vulkanRenderer.draw();
    }

    vulkanRenderer.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#ifndef VULKANABSTRACTION_H
#define VULKANABSTRACTION_H

#include <SauronLT.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>

#include <iostream>
#include <vector>
#include <cstring>
#include <optional>
#include <algorithm>
#include <set>
#include <limits>
#include <fstream>
#include <array>

#define RETURN_IF(x) if(x) return;
#define RETURN_AND_PRINT_ERROR_IF(x, y) if(x) { std::cerr << "[ERROR] " << y << std::endl; return; }
#define EXIT_IF(x) if(x) exit(-1);
#define EXIT_AND_PRINT_ERROR_IF(x, y) if(x) { std::cerr << "[FATAL ERROR] " << y << std::endl; exit(-1); }
#define RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(x, y) if(x != VK_SUCCESS) { std::cerr << "[ERROR] " << y << " (Vulkan error: " << x << ")" << std::endl; return; }
#define EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(x, y) if(x != VK_SUCCESS) { std::cerr << "[FATAL ERROR] " << y << " (Vulkan error: " << x << ")" << std::endl; exit(-1); }
const int MAX_FRAMES_IN_FLIGHT = 2;

namespace VKA {
    struct Vertex {
        glm::vec2 pos;
        glm::vec3 color;

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

            // Position
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            // Color
            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            return attributeDescriptions;
        }
    };


    void Init(const char* applicationName, GLFWwindow* window);
    void Draw(GLFWwindow* window);
    void WaitForDevice();
    void Shutdown();
}

#endif //VULKANABSTRACTION_H
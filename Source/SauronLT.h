#ifndef SAURON_H
#define SAURON_H

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <iostream>

#ifndef NDEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

#define EXIT_MSG_IF(x, y) if (x) {std::cerr<<"[ERROR] "<<y<<std::endl; exit(EXIT_FAILURE);}
#define RETURN_MSG_IF(x, y) if (x) {std::cerr<<"[ERROR] "<<y<<std::endl; return;}
#define RETURN_FALSE_MSG_IF(x, y) if (x) {std::cerr<<"[ERROR] "<<y<<std::endl; return false;}
#define VK_CHECK_RETURN_FALSE_MSG_IF(x, y) if (x != VK_SUCCESS) {std::cerr<<"[VULKAN ERROR] "<<y<<" (Error code: "<<x<<")"<<std::endl; return false;}
#define VK_CHECK_RETURN_MSG_IF(x, y) if (x != VK_SUCCESS) {std::cerr<<"[VULKAN ERROR] "<<y<<" (Error code: "<<x<<")"<<std::endl; return;}
#define EXIT_IF(x) if (x) {exit(EXIT_FAILURE);}
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    std::cerr << "[VULKAN ERROR] VkResult = " << err << std::endl;
    EXIT_IF(err < 0)
}

namespace SauronLT {
    void Init(int windowWidth, int windowHeight, const char* appName);
    void Shutdown();
    bool Running();
    void BeginFrame();
    void EndFrame();
    void SetBackground(const ImVec4& color);
    VkDevice GetDevice();
    VkPhysicalDevice GetPhysicalDevice();
}


#endif //SAURON_H
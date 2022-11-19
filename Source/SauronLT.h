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

namespace SauronLT {
    enum class ImageFormat
    {
        None = 0,
        RGBA,
        RGBA32F
    };

    class Image
    {
    public:
        Image(const std::string& path);
        Image(uint32_t width, uint32_t height, ImageFormat format, const void* data = nullptr);
        ~Image();

        void SetData(const void* data);

        VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }

        void Resize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        // Either release manually, or release by destruction
        void Release();
    private:
        void AllocateMemory(uint64_t size);
    private:
        uint32_t m_Width = 0, m_Height = 0;

        VkImage m_Image = nullptr;
        VkImageView m_ImageView = nullptr;
        VkDeviceMemory m_Memory = nullptr;
        VkSampler m_Sampler = nullptr;

        ImageFormat m_Format = ImageFormat::None;

        VkBuffer m_StagingBuffer = nullptr;
        VkDeviceMemory m_StagingBufferMemory = nullptr;

        size_t m_AlignedSize = 0;

        VkDescriptorSet m_DescriptorSet = nullptr;

        std::string m_Filepath;
    };

    void Init(int windowWidth, int windowHeight, const char* appName);
    void Shutdown();
    bool Running();
    void BeginFrame();
    void EndFrame();
    void SetBackground(const ImVec4& color);
}


#endif //SAURON_H
#include <vector>
#include <functional>
#include "SauronLT.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static void glfw_error_callback(int error, const char* description)
{
    std::cerr << "[GLFW ERROR] " << error << ": " << description << std::endl;
}

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    std::cerr << "[VULKAN ERROR] VkResult = " << err << std::endl;
    EXIT_IF(err < 0)
}

#ifdef IMGUI_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    std::cerr << "[VULKAN ERROR] Debug report from ObjectType: " << objectType << "\nMessage: " << pMessage << std::endl;
    return VK_FALSE;
}
#endif

namespace SauronLT {
    static VkAllocationCallbacks*   s_Allocator = nullptr;
    static VkInstance               s_Instance = VK_NULL_HANDLE;
    static VkPhysicalDevice         s_PhysicalDevice = VK_NULL_HANDLE;
    static VkDevice                 s_Device = VK_NULL_HANDLE;
    static uint32_t                 s_QueueFamily = (uint32_t)-1;
    static VkQueue                  s_Queue = VK_NULL_HANDLE;
    static VkDebugReportCallbackEXT s_DebugReport = VK_NULL_HANDLE;
    static VkPipelineCache          s_PipelineCache = VK_NULL_HANDLE;
    static VkDescriptorPool         s_DescriptorPool = VK_NULL_HANDLE;

    static ImGui_ImplVulkanH_Window s_MainWindowData;
    static int                      s_MinImageCount = 2;
    static bool                     s_SwapChainRebuild = false;
    static GLFWwindow*              s_Window = nullptr;

    static bool                     s_Initialized = false;
    static ImVec4                   s_BackgroundColor;
    static                          std::vector<std::vector<std::function<void()>>> s_ResourceFreeQueue;


    // HELPER
    static VkCommandPool GetCurrentCommandPool() {
        return s_MainWindowData.Frames[s_MainWindowData.FrameIndex].CommandPool;
    }

    static VkCommandBuffer GetCurrentCommandBuffer() {
        return s_MainWindowData.Frames[s_MainWindowData.FrameIndex].CommandBuffer;
    }

    bool Running() {
        return !glfwWindowShouldClose(s_Window);
    }

    void SetBackground(const ImVec4& color) {
//        s_MainWindowData.ClearValue.color.float32[0] = color.x;
//        s_MainWindowData.ClearValue.color.float32[1] = color.y;
//        s_MainWindowData.ClearValue.color.float32[2] = color.z;
//        s_MainWindowData.ClearValue.color.float32[3] = color.w;
        s_BackgroundColor = color;
    }


    // SETUP
    static bool SetupVulkan()
    {
        VkResult err;

        // Setup Vulkan
        if (!glfwVulkanSupported())
        {
            std::cerr << "[GLFW ERROR] Vulkan Not Supported.\n";
            return false;
        }
        uint32_t extensions_count = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

        // Create Vulkan Instance
        {
            VkInstanceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info.enabledExtensionCount = extensions_count;
            create_info.ppEnabledExtensionNames = extensions;
    #ifdef IMGUI_VULKAN_DEBUG_REPORT
            // Enabling validation layers
            const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
            create_info.enabledLayerCount = 1;
            create_info.ppEnabledLayerNames = layers;

            // Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
            const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (extensions_count + 1));
            memcpy(extensions_ext, extensions, extensions_count * sizeof(const char*));
            extensions_ext[extensions_count] = "VK_EXT_debug_report";
            create_info.enabledExtensionCount = extensions_count + 1;
            create_info.ppEnabledExtensionNames = extensions_ext;

            // Create Vulkan Instance
            err = vkCreateInstance(&create_info, s_Allocator, &s_Instance);
            VK_CHECK_RETURN_FALSE_MSG_IF(err, "Failed to create Vulkan Instance.");
            free(extensions_ext);

            // Get the function pointer (required for any extensions)
            auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(s_Instance, "vkCreateDebugReportCallbackEXT");
            RETURN_FALSE_MSG_IF(vkCreateDebugReportCallbackEXT == nullptr, "Failed to load function for creating debug report callback.")

            // Setup the debug report callback
            VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debug_report_ci.pfnCallback = debug_report;
            debug_report_ci.pUserData = nullptr;
            err = vkCreateDebugReportCallbackEXT(s_Instance, &debug_report_ci, s_Allocator, &s_DebugReport);
            VK_CHECK_RETURN_FALSE_MSG_IF(err, "Failed to create Debug Report.");
    #else
            // Create Vulkan Instance without any debug feature
            err = vkCreateInstance(&create_info, s_Allocator, &s_Instance);
            VK_CHECK_RETURN_FALSE_MSG_IF(err, "Failed to create Vulkan Instance.");
            IM_UNUSED(s_DebugReport);
    #endif
        }

        // Select GPU
        {
            uint32_t gpu_count;
            err = vkEnumeratePhysicalDevices(s_Instance, &gpu_count, nullptr);
            VK_CHECK_RETURN_FALSE_MSG_IF(err, "Failed to enumerate physical devices.");
            RETURN_FALSE_MSG_IF(gpu_count == 0, "Failed to find GPUs.")

            auto* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
            err = vkEnumeratePhysicalDevices(s_Instance, &gpu_count, gpus);
            VK_CHECK_RETURN_FALSE_MSG_IF(err, "Failed to enumerate physical devices.");

            int use_gpu = 0;
            for (int i = 0; i < (int)gpu_count; i++)
            {
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(gpus[i], &properties);
                if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    use_gpu = i;
                    break;
                }
            }

            s_PhysicalDevice = gpus[use_gpu];
            free(gpus);
        }

        // Select graphics queue family
        {
            uint32_t count;
            vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &count, nullptr);
            auto* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
            vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &count, queues);
            for (uint32_t i = 0; i < count; i++)
                if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    s_QueueFamily = i;
                    break;
                }
            free(queues);
            RETURN_FALSE_MSG_IF(s_QueueFamily == (uint32_t)-1, "Failed to find fitting Queue Family.")
        }

        // Create Logical Device (with 1 queue)
        {
            int device_extension_count = 1;
            const char* device_extensions[] = { "VK_KHR_swapchain" };
            const float queue_priority[] = { 1.0f };

            VkDeviceQueueCreateInfo queue_info[1] = {};
            queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info[0].queueFamilyIndex = s_QueueFamily;
            queue_info[0].queueCount = 1;
            queue_info[0].pQueuePriorities = queue_priority;

            VkDeviceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
            create_info.pQueueCreateInfos = queue_info;
            create_info.enabledExtensionCount = device_extension_count;
            create_info.ppEnabledExtensionNames = device_extensions;

            err = vkCreateDevice(s_PhysicalDevice, &create_info, s_Allocator, &s_Device);
            VK_CHECK_RETURN_FALSE_MSG_IF(err, "Failed to create logical device.");
            vkGetDeviceQueue(s_Device, s_QueueFamily, 0, &s_Queue);
        }

        // Create Descriptor Pool
        {
            VkDescriptorPoolSize pool_sizes[] =
                    {
                            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
                    };
            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
            pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
            pool_info.pPoolSizes = pool_sizes;
            err = vkCreateDescriptorPool(s_Device, &pool_info, s_Allocator, &s_DescriptorPool);
            VK_CHECK_RETURN_FALSE_MSG_IF(err, "Failed to create Descriptor Pool.");
        }

        return true;
    }

    static bool SetupVulkanWindow(VkSurfaceKHR surface, int width, int height)
    {
        s_MainWindowData.Surface = surface;

        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(s_PhysicalDevice, s_QueueFamily, s_MainWindowData.Surface, &res);
        RETURN_FALSE_MSG_IF(res != VK_TRUE, "No WSI support on physical device 0.")

        // Select Surface Format
        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        s_MainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(s_PhysicalDevice, s_MainWindowData.Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

        VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };

        s_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(s_PhysicalDevice, s_MainWindowData.Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));

        ImGui_ImplVulkanH_CreateOrResizeWindow(s_Instance, s_PhysicalDevice, s_Device, &s_MainWindowData, s_QueueFamily, s_Allocator, width, height, s_MinImageCount);

        return true;
    }

    void Init(int windowWidth, int windowHeight, const char* appName) {
        RETURN_MSG_IF(s_Initialized, "Sauron can't be initialized twice.")

        // Setup GLFW window
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit()) {
            std::cerr << "[GLFW ERROR] Failed to initialize GLFW.\n";
            return;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        s_Window = glfwCreateWindow(windowWidth, windowHeight, appName, nullptr, nullptr);
        RETURN_MSG_IF(!SetupVulkan(), "Failed to set up Vulkan.")

        // Create Window Surface
        VkSurfaceKHR surface;
        VkResult err = glfwCreateWindowSurface(s_Instance, s_Window, s_Allocator, &surface);
        VK_CHECK_RETURN_MSG_IF(err, "Failed to create window surface.")

        // Create Framebuffers
        int w, h;
        glfwGetFramebufferSize(s_Window, &w, &h);
        RETURN_MSG_IF(!SetupVulkanWindow(surface, w, h), "Failed to set up Vulkan.")

        glfwSetKeyCallback(s_Window, SauronLT::Input::keyCallback);
        glfwSetMouseButtonCallback(s_Window, SauronLT::Input::mouseButtonCallback);
        glfwSetCursorPosCallback(s_Window, SauronLT::Input::mouseCallback);
        glfwSetScrollCallback(s_Window, SauronLT::Input::mouseScrollCallback);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ImGui::StyleColorsLight();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(s_Window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = s_Instance;
        init_info.PhysicalDevice = s_PhysicalDevice;
        init_info.Device = s_Device;
        init_info.QueueFamily = s_QueueFamily;
        init_info.Queue = s_Queue;
        init_info.PipelineCache = s_PipelineCache;
        init_info.DescriptorPool = s_DescriptorPool;
        init_info.Subpass = 0;
        init_info.MinImageCount = s_MinImageCount;
        init_info.ImageCount = s_MainWindowData.ImageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = s_Allocator;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info, s_MainWindowData.RenderPass);

        s_ResourceFreeQueue.resize(s_MinImageCount);

        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;
        ImFont* robotoFont = io.Fonts->AddFontFromFileTTF(R"(Resources/Fonts/RandyGG.ttf)", 17.0f);
        RETURN_MSG_IF(robotoFont == nullptr, "Failed to load RandyGG font.")
        io.FontDefault = robotoFont;

        // Upload Fonts
        {
            VkCommandPool command_pool = GetCurrentCommandPool();
            VkCommandBuffer command_buffer = GetCurrentCommandBuffer();

            err = vkResetCommandPool(s_Device, command_pool, 0);
            RETURN_MSG_IF(err != VK_SUCCESS, "Failed to upload fonts.")
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(command_buffer, &begin_info);
            RETURN_MSG_IF(err != VK_SUCCESS, "Failed to upload fonts.")

            ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

            VkSubmitInfo end_info = {};
            end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            end_info.commandBufferCount = 1;
            end_info.pCommandBuffers = &command_buffer;
            err = vkEndCommandBuffer(command_buffer);
            RETURN_MSG_IF(err != VK_SUCCESS, "Failed to upload fonts.")
            err = vkQueueSubmit(s_Queue, 1, &end_info, VK_NULL_HANDLE);
            RETURN_MSG_IF(err != VK_SUCCESS, "Failed to upload fonts.")

            err = vkDeviceWaitIdle(s_Device);
            RETURN_MSG_IF(err != VK_SUCCESS, "Failed to upload fonts.")
            ImGui_ImplVulkan_DestroyFontUploadObjects();
        }

        std::cout << "Successfully initialized Vulkan.\n";
        s_Initialized = true;
    }


    // CLEANUP
    void Shutdown()
    {
        RETURN_MSG_IF(!s_Initialized, "Sauron can't be shutdown before initialization.")

        VkResult err = vkDeviceWaitIdle(s_Device);
        check_vk_result(err);

        // Free resources in queue
        for (auto& queue : s_ResourceFreeQueue)
        {
            for (auto& func : queue)
                func();
        }
        s_ResourceFreeQueue.clear();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        ImGui_ImplVulkanH_DestroyWindow(s_Instance, s_Device, &s_MainWindowData, s_Allocator);

        vkDestroyDescriptorPool(s_Device, s_DescriptorPool, s_Allocator);

    #ifdef IMGUI_VULKAN_DEBUG_REPORT
        auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(s_Instance, "vkDestroyDebugReportCallbackEXT");
        vkDestroyDebugReportCallbackEXT(s_Instance, s_DebugReport, s_Allocator);
    #endif

        vkDestroyDevice(s_Device, s_Allocator);
        vkDestroyInstance(s_Instance, s_Allocator);

        glfwDestroyWindow(s_Window);
        glfwTerminate();

        s_Initialized = false;
    }

    static void Render(ImDrawData* draw_data)
    {
        VkResult err;

        VkSemaphore image_acquired_semaphore  = s_MainWindowData.FrameSemaphores[s_MainWindowData.SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore = s_MainWindowData.FrameSemaphores[s_MainWindowData.SemaphoreIndex].RenderCompleteSemaphore;
        err = vkAcquireNextImageKHR(s_Device, s_MainWindowData.Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &s_MainWindowData.FrameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        {
            s_SwapChainRebuild = true;
            return;
        }
        VK_CHECK_RETURN_MSG_IF(err, "Failed to acquire next image.")


        ImGui_ImplVulkanH_Frame* fd = &s_MainWindowData.Frames[s_MainWindowData.FrameIndex];
        {
            err = vkWaitForFences(s_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
            check_vk_result(err);

            err = vkResetFences(s_Device, 1, &fd->Fence);
            check_vk_result(err);
        }

        // Free resources in queue
        for (auto& func : s_ResourceFreeQueue[s_MainWindowData.FrameIndex])
            func();
        s_ResourceFreeQueue[s_MainWindowData.FrameIndex].clear();

        {
            err = vkResetCommandPool(s_Device, fd->CommandPool, 0);
            check_vk_result(err);
            VkCommandBufferBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
            check_vk_result(err);
        }

        {
            VkRenderPassBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            info.renderPass = s_MainWindowData.RenderPass;
            info.framebuffer = fd->Framebuffer;
            info.renderArea.extent.width = s_MainWindowData.Width;
            info.renderArea.extent.height = s_MainWindowData.Height;
            info.clearValueCount = 1;
            info.pClearValues = &s_MainWindowData.ClearValue;
            vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
        }

        // Record dear imgui primitives into command buffer
        ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

        // Submit command buffer
        vkCmdEndRenderPass(fd->CommandBuffer);
        {
            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = &image_acquired_semaphore;
            info.pWaitDstStageMask = &wait_stage;
            info.commandBufferCount = 1;
            info.pCommandBuffers = &fd->CommandBuffer;
            info.signalSemaphoreCount = 1;
            info.pSignalSemaphores = &render_complete_semaphore;

            err = vkEndCommandBuffer(fd->CommandBuffer);
            check_vk_result(err);
            err = vkQueueSubmit(s_Queue, 1, &info, fd->Fence);
            check_vk_result(err);
        }
    }

    static void Present()
    {
        if (s_SwapChainRebuild)
            return;
        VkSemaphore render_complete_semaphore = s_MainWindowData.FrameSemaphores[s_MainWindowData.SemaphoreIndex].RenderCompleteSemaphore;
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &render_complete_semaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &s_MainWindowData.Swapchain;
        info.pImageIndices = &s_MainWindowData.FrameIndex;
        VkResult err = vkQueuePresentKHR(s_Queue, &info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        {
            s_SwapChainRebuild = true;
            return;
        }
        check_vk_result(err);
        s_MainWindowData.SemaphoreIndex = (s_MainWindowData.SemaphoreIndex + 1) % s_MainWindowData.ImageCount; // Now we can use the next set of semaphores
    }

    void BeginFrame() {
        glfwPollEvents();

        // Resize swap chain?
        if (s_SwapChainRebuild)
        {
            int width, height;
            glfwGetFramebufferSize(s_Window, &width, &height);
            if (width > 0 && height > 0)
            {
                ImGui_ImplVulkan_SetMinImageCount(s_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(s_Instance, s_PhysicalDevice, s_Device, &s_MainWindowData,
                                                       s_QueueFamily, s_Allocator, width, height, s_MinImageCount);
                s_MainWindowData.FrameIndex = 0;
                s_SwapChainRebuild = false;
            }
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, IM_COL32(s_BackgroundColor.x * 255, s_BackgroundColor.y * 255, s_BackgroundColor.z * 255, s_BackgroundColor.w * 255));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar();

        ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("VulkanAppDockspace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        ImGui::PopStyleColor();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    }

    void EndFrame() {
        ImGui::PopStyleVar();
        ImGui::End();

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            Render(draw_data);
        }
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        if (!is_minimized) {
            Present();
        }
    }

    GLFWwindow *GetWindow() {
        return s_Window;
    }

    // IMAGE

    static uint32_t BytesPerPixel(ImageFormat format)
    {
        switch (format)
        {
            case ImageFormat::RGBA:    return 4;
            case ImageFormat::RGBA32F: return 16;
            default: break;
        }
        return 0;
    }

    static VkFormat ToVulkanFormat(ImageFormat format)
    {
        switch (format)
        {
            case ImageFormat::RGBA:    return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
            default: break;
        }
        return (VkFormat)0;
    }

    static uint32_t GetVulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
    {
        VkPhysicalDeviceMemoryProperties prop;
        vkGetPhysicalDeviceMemoryProperties(s_PhysicalDevice, &prop);
        for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        {
            if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
                return i;
        }

        return 0xffffffff;
    }

    Image::Image(const std::string& path)
            : m_Filepath(path)
    {
        int width, height, channels;
        uint8_t* data = nullptr;

        if (stbi_is_hdr(m_Filepath.c_str()))
        {
            data = (uint8_t*)stbi_loadf(m_Filepath.c_str(), &width, &height, &channels, 4);
            m_Format = ImageFormat::RGBA32F;
        }
        else
        {
            data = stbi_load(m_Filepath.c_str(), &width, &height, &channels, 4);
            m_Format = ImageFormat::RGBA;
        }

        m_Width = width;
        m_Height = height;

        AllocateMemory(m_Width * m_Height * BytesPerPixel(m_Format));
        SetData(data);
    }

    Image::Image(uint32_t width, uint32_t height, ImageFormat format, const void* data)
            : m_Width(width), m_Height(height), m_Format(format)
    {
        AllocateMemory(m_Width * m_Height * BytesPerPixel(m_Format));
        if (data)
            SetData(data);
    }

    Image::~Image()
    {
        if (m_Image)
            Release();
    }

    void Image::AllocateMemory(uint64_t size)
    {
        VkDevice device = s_Device;

        VkResult err;

        VkFormat vulkanFormat = ToVulkanFormat(m_Format);

        // Create the Image
        {
            VkImageCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = vulkanFormat;
            info.extent.width = m_Width;
            info.extent.height = m_Height;
            info.extent.depth = 1;
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            err = vkCreateImage(device, &info, nullptr, &m_Image);
            check_vk_result(err);
            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(device, m_Image, &req);
            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = req.size;
            alloc_info.memoryTypeIndex = GetVulkanMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
            err = vkAllocateMemory(device, &alloc_info, nullptr, &m_Memory);
            check_vk_result(err);
            err = vkBindImageMemory(device, m_Image, m_Memory, 0);
            check_vk_result(err);
        }

        // Create the Image View:
        {
            VkImageViewCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = m_Image;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = vulkanFormat;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;
            err = vkCreateImageView(device, &info, nullptr, &m_ImageView);
            check_vk_result(err);
        }

        // Create sampler:
        {
            VkSamplerCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            info.magFilter = VK_FILTER_LINEAR;
            info.minFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.minLod = -1000;
            info.maxLod = 1000;
            info.maxAnisotropy = 1.0f;
            err = vkCreateSampler(device, &info, nullptr, &m_Sampler);
            check_vk_result(err);
        }

        // Create the Descriptor Set:
        m_DescriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(m_Sampler, m_ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void Image::Release()
    {
        // TODO fix validation errors
        s_ResourceFreeQueue[s_MainWindowData.FrameIndex].emplace_back(
                ([sampler = m_Sampler, imageView = m_ImageView, image = m_Image,
                memory = m_Memory, stagingBuffer = m_StagingBuffer, stagingBufferMemory = m_StagingBufferMemory]()
        {
            vkDestroySampler(s_Device, sampler, nullptr);
            vkDestroyImageView(s_Device, imageView, nullptr);
            vkDestroyImage(s_Device, image, nullptr);
            vkFreeMemory(s_Device, memory, nullptr);
            vkDestroyBuffer(s_Device, stagingBuffer, nullptr);
            vkFreeMemory(s_Device, stagingBufferMemory, nullptr);
        }
        ));

//        vkDestroySampler(s_Device, m_Sampler, nullptr);
//        vkDestroyImageView(s_Device, m_ImageView, nullptr);
//        vkDestroyImage(s_Device, m_Image, nullptr);
//        vkFreeMemory(s_Device, m_Memory, nullptr);
//        vkDestroyBuffer(s_Device, m_StagingBuffer, nullptr);
//        vkFreeMemory(s_Device, m_StagingBufferMemory, nullptr);
//        ImGui_ImplVulkan_RemoveTexture(m_DescriptorSet);

        m_Sampler = nullptr;
        m_ImageView = nullptr;
        m_Image = nullptr;
        m_Memory = nullptr;
        m_StagingBuffer = nullptr;
        m_StagingBufferMemory = nullptr;
    }

    void Image::SetData(const void* data)
    {
        size_t upload_size = m_Width * m_Height * BytesPerPixel(m_Format);

        VkResult err;

        if (!m_StagingBuffer)
        {
            // Create the Upload Buffer
            {
                VkBufferCreateInfo buffer_info = {};
                buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                buffer_info.size = upload_size;
                buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                err = vkCreateBuffer(s_Device, &buffer_info, nullptr, &m_StagingBuffer);
                check_vk_result(err);
                VkMemoryRequirements req;
                vkGetBufferMemoryRequirements(s_Device, m_StagingBuffer, &req);
                m_AlignedSize = req.size;
                VkMemoryAllocateInfo alloc_info = {};
                alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                alloc_info.allocationSize = req.size;
                alloc_info.memoryTypeIndex = GetVulkanMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
                err = vkAllocateMemory(s_Device, &alloc_info, nullptr, &m_StagingBufferMemory);
                check_vk_result(err);
                err = vkBindBufferMemory(s_Device, m_StagingBuffer, m_StagingBufferMemory, 0);
                check_vk_result(err);
            }
        }

        // Upload to Buffer
        {
            char* map = NULL;
            err = vkMapMemory(s_Device, m_StagingBufferMemory, 0, m_AlignedSize, 0, (void**)(&map));
            check_vk_result(err);
            memcpy(map, data, upload_size);
            VkMappedMemoryRange range[1] = {};
            range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range[0].memory = m_StagingBufferMemory;
            range[0].size = m_AlignedSize;
            err = vkFlushMappedMemoryRanges(s_Device, 1, range);
            check_vk_result(err);
            vkUnmapMemory(s_Device, m_StagingBufferMemory);
        }


        // Copy to Image
        {
            VkCommandPool command_pool = s_MainWindowData.Frames[s_MainWindowData.FrameIndex].CommandPool;
            VkCommandBuffer command_buffer;
            {
                VkCommandBufferAllocateInfo alloc_info{};
                alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                alloc_info.commandPool = command_pool;
                alloc_info.commandBufferCount = 1;

                err = vkAllocateCommandBuffers(s_Device, &alloc_info, &command_buffer);
                check_vk_result(err);

                VkCommandBufferBeginInfo begin_info = {};
                begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                err = vkBeginCommandBuffer(command_buffer, &begin_info);
                check_vk_result(err);
            }

            VkImageMemoryBarrier copy_barrier = {};
            copy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            copy_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            copy_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.image = m_Image;
            copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_barrier.subresourceRange.levelCount = 1;
            copy_barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copy_barrier);

            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = m_Width;
            region.imageExtent.height = m_Height;
            region.imageExtent.depth = 1;
            vkCmdCopyBufferToImage(command_buffer, m_StagingBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier use_barrier = {};
            use_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            use_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            use_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            use_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            use_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            use_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.image = m_Image;
            use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            use_barrier.subresourceRange.levelCount = 1;
            use_barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                                 0, nullptr, 0, nullptr, 1, &use_barrier);

            // End command buffer
            {
                VkSubmitInfo end_info = {};
                end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                end_info.commandBufferCount = 1;
                end_info.pCommandBuffers = &command_buffer;
                err = vkEndCommandBuffer(command_buffer);
                check_vk_result(err);
                err = vkQueueSubmit(s_Queue, 1, &end_info, VK_NULL_HANDLE);
                check_vk_result(err);
                err = vkDeviceWaitIdle(s_Device);
                check_vk_result(err);
            }
        }
    }

    void Image::Resize(uint32_t width, uint32_t height)
    {
        if (m_Image && m_Width == width && m_Height == height)
            return;

        m_Width = width;
        m_Height = height;

        Release();
        AllocateMemory(m_Width * m_Height * BytesPerPixel(m_Format));
    }
}
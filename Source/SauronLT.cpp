#include "SauronLT.h"

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    std::cerr << "[VULKAN ERROR] VkResult = " << err << std::endl;
    EXIT_IF(err < 0)
}

static void glfw_error_callback(int error, const char* description)
{
    std::cerr << "[GLFW ERROR] " << error << ": " << description << std::endl;
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
    static ImFont*                  s_MainFont = nullptr;

    static bool                     s_Initialized = false;


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

    void SetClearColor(const ImVec4& color) {
        s_MainWindowData.ClearValue.color.float32[0] = color.x;
        s_MainWindowData.ClearValue.color.float32[1] = color.y;
        s_MainWindowData.ClearValue.color.float32[2] = color.z;
        s_MainWindowData.ClearValue.color.float32[3] = color.w;
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

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();

        ImGui::StyleColorsLight();

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

        io.Fonts->AddFontDefault();
        s_MainFont = io.Fonts->AddFontFromFileTTF(R"(Resources/Fonts/RandyGG.ttf)", 17.0f);
        RETURN_MSG_IF(s_MainFont == nullptr, "Failed to load RandyGG font.")

        /*
        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;
        ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
        io.FontDefault = MainFont;
        */

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
        VK_CHECK_RETURN_MSG_IF(err, "Couldn't wait for device.")

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

        ImGui::PushFont(s_MainFont);
    }

    void EndFrame() {
        ImGui::PopFont();
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {


            Render(draw_data);
            Present();
        }
    }
}
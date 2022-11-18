#include <VKA.h>

// IF ANYTHING DON'T MAKE SENSE GO HERE: https://vulkan-tutorial.com/en/Drawing_a_triangle/
namespace VKA {
#ifdef NDEBUG
    const bool s_EnableValidationLayers = false;
#else
    const bool s_EnableValidationLayers = true;
#endif
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    static bool s_Initialized = false;
    static VkInstance s_Instance = VK_NULL_HANDLE;
    static VkDebugUtilsMessengerEXT s_DebugMessenger = VK_NULL_HANDLE;
    static VkPhysicalDevice s_PhysicalDevice = VK_NULL_HANDLE;
    static VkDevice s_Device = VK_NULL_HANDLE;
    static VkQueue s_GraphicsQueue = VK_NULL_HANDLE;
    static VkSurfaceKHR s_Surface = VK_NULL_HANDLE;
    static VkQueue s_PresentQueue = VK_NULL_HANDLE;
    static VkSwapchainKHR s_SwapChain = VK_NULL_HANDLE;
    static std::vector<VkImage> s_SwapChainImages;
    static VkFormat s_SwapChainImageFormat;
    static VkExtent2D s_SwapChainExtent;
    static std::vector<VkImageView> s_SwapChainImageViews;
    static VkRenderPass s_RenderPass;
    static VkPipelineLayout s_PipelineLayout;
    static VkPipeline s_GraphicsPipeline;
    static std::vector<VkFramebuffer> s_SwapChainFramebuffers;
    static VkCommandPool s_CommandPool;
    static std::vector<VkCommandBuffer> s_CommandBuffers;
    static std::vector<VkSemaphore> s_ImageAvailableSemaphores;
    static std::vector<VkSemaphore> s_RenderFinishedSemaphores;
    static std::vector<VkFence> s_InFlightFences;
    static uint32_t s_CurrentFrame = 0;
    static VkBuffer s_VertexBuffer;
    static VkDeviceMemory s_VertexBufferMemory;
    static bool s_FramebufferResized = false;

    const static std::vector<const char *> s_ValidationLayers = {
            "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> s_DeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        EXIT_AND_PRINT_ERROR_IF(!file.is_open(), (std::string("Failed to open shader: ") + filename).c_str())

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
        // TODO fix newline shit
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            std::string str = std::string(pCallbackData->pMessage);
//            printf("%s", pCallbackData->pMessage);
            std::cerr << "[Validation layer] " << str << std::endl;
        }
        return VK_FALSE;
    }
    static bool validationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : s_ValidationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }
    static VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    static void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }
    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (int i = 0; i < (int)queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, s_Surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
            if (indices.isComplete()) {
                break;
            }
        }

        return indices;
    }
    static bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(s_DeviceExtensions.begin(), s_DeviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, s_Surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }
    static bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }
    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }
    static VkExtent2D chooseSwapExtent(GLFWwindow* window, const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }
    static VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = (const uint32_t*)(code.data());
        VkShaderModule shaderModule;

        VkResult result = vkCreateShaderModule(s_Device, &createInfo, nullptr, &shaderModule);
        EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create shader module, exit.")

        return shaderModule;
    }
    static void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to begin recording command buffer, exit!")

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = s_RenderPass;
        renderPassInfo.framebuffer = s_SwapChainFramebuffers[imageIndex];

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = s_SwapChainExtent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_GraphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)(s_SwapChainExtent.width);
        viewport.height = (float)(s_SwapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = s_SwapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = {s_VertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdDraw(commandBuffer, static_cast<uint32_t>(3), 1, 0, 0);
        // TODO use vector for vertexCount

        vkCmdEndRenderPass(commandBuffer);

        result = vkEndCommandBuffer(commandBuffer);
        EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to record command buffer, exit!")
    }
    static void createSwapChain(GLFWwindow* window) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(s_PhysicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(window, swapChainSupport.capabilities);
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = s_Surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(s_PhysicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(s_Device, &createInfo, nullptr, &s_SwapChain);
        EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create swap chain, VKA::Init() aborted.")

        s_SwapChainImageFormat = surfaceFormat.format;
        s_SwapChainExtent = extent;

        vkGetSwapchainImagesKHR(s_Device, s_SwapChain, &imageCount, nullptr);
        s_SwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(s_Device, s_SwapChain, &imageCount, s_SwapChainImages.data());
    }
    static void createImageViews() {
        s_SwapChainImageViews.resize(s_SwapChainImages.size());

        for (size_t i = 0; i < s_SwapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = s_SwapChainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = s_SwapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            VkResult result = vkCreateImageView(s_Device, &createInfo, nullptr, &s_SwapChainImageViews[i]);
            EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create swap chain, VKA::Init() aborted.")
        }
    }
    static void createFramebuffers() {
        s_SwapChainFramebuffers.resize(s_SwapChainImageViews.size());

        for (size_t i = 0; i < s_SwapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                    s_SwapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = s_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = s_SwapChainExtent.width;
            framebufferInfo.height = s_SwapChainExtent.height;
            framebufferInfo.layers = 1;

            VkResult result = vkCreateFramebuffer(s_Device, &framebufferInfo, nullptr, &s_SwapChainFramebuffers[i]);
            EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create framebuffer, VKA::Init() aborted.")
        }
    }
    static void cleanupSwapChain() {
        for (auto & swapChainFramebuffer : s_SwapChainFramebuffers) {
            vkDestroyFramebuffer(s_Device, swapChainFramebuffer, nullptr);
        }

        for (auto & swapChainImageView : s_SwapChainImageViews) {
            vkDestroyImageView(s_Device, swapChainImageView, nullptr);
        }

        vkDestroySwapchainKHR(s_Device, s_SwapChain, nullptr);
    }
    static void recreateSwapChain(GLFWwindow* window)
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(s_Device);

        cleanupSwapChain();

        createSwapChain(window);
        createImageViews();
        createFramebuffers();
    }
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        (*(bool*)glfwGetWindowUserPointer(window)) = true;
    }
    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(s_PhysicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        EXIT_AND_PRINT_ERROR_IF(1, "Failed to find suitable memory type.")
    }


    void Init(const char* applicationName, GLFWwindow* window) {
        glfwSetWindowUserPointer(window, &s_FramebufferResized);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        VkResult result;
        // - Instance
        {
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = applicationName;
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_0;

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;

            uint32_t glfwExtensionCount = 0;
            const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            // includes basic glfwExtensions, more can be added later
            std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            if (s_EnableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            createInfo.enabledExtensionCount = (uint32_t) extensions.size();
            createInfo.ppEnabledExtensionNames = extensions.data();

            RETURN_AND_PRINT_ERROR_IF(s_EnableValidationLayers && !validationLayerSupport(), "Validation layers requested, but not supported, VKA::Init() aborted.");

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            if (s_EnableValidationLayers) {
                createInfo.enabledLayerCount = (uint32_t) (s_ValidationLayers.size());
                createInfo.ppEnabledLayerNames = s_ValidationLayers.data();

                debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debugCreateInfo.pfnUserCallback = debugCallback;
                debugCreateInfo.pUserData = nullptr; // Optional
                createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debugCreateInfo;
            } else {
                createInfo.enabledLayerCount = 0;

                createInfo.pNext = nullptr;
            }

            result = vkCreateInstance(&createInfo, nullptr, &s_Instance);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create Vulkan instance, VKA::Init() aborted.")
        }
        // - Debug Messenger
        if (s_EnableValidationLayers) {
            VkDebugUtilsMessengerCreateInfoEXT createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = debugCallback;
            createInfo.pUserData = nullptr; // Optional

            result = createDebugUtilsMessengerEXT(s_Instance, &createInfo, nullptr, &s_DebugMessenger);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to set up debug messenger, VKA::Init() aborted.");
        }
        // - Surface
        {
            result = glfwCreateWindowSurface(s_Instance, window, nullptr, &s_Surface);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create window surface, VKA::Init() aborted.")
        }
        // - Physical device picking
        {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(s_Instance, &deviceCount, nullptr);

            RETURN_AND_PRINT_ERROR_IF(deviceCount == 0, "Failed to find GPUs with Vulkan support, VKA::Init() aborted.")

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(s_Instance, &deviceCount, devices.data());

            for (const auto& device : devices) {
                if (isDeviceSuitable(device)) {
                    s_PhysicalDevice = device;
                    break;
                }
            }
            RETURN_AND_PRINT_ERROR_IF(s_PhysicalDevice == VK_NULL_HANDLE, "Failed to find a suitable GPU, VKA::Init() aborted.")
#ifndef NDEBUG
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(s_PhysicalDevice, &deviceProperties);
            std::cout << "Using device: " << deviceProperties.deviceName << "\n";
#endif
        }
        // - Logical device picking
        {
            QueueFamilyIndices indices = findQueueFamilies(s_PhysicalDevice);
//            RETURN_AND_PRINT_ERsROR_IF(!indices.isComplete(), "Failed to find all the queues, VKA::Init() aborted.")

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

            float queuePriority = 1.0f;
            for (uint32_t queueFamily : uniqueQueueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;
                queueCreateInfos.push_back(queueCreateInfo);
            }

            VkPhysicalDeviceFeatures deviceFeatures{};

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.queueCreateInfoCount = (uint32_t)(queueCreateInfos.size());
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.pEnabledFeatures = &deviceFeatures;

            createInfo.enabledExtensionCount = (uint32_t)(s_DeviceExtensions.size());
            createInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();

            if (s_EnableValidationLayers) {
                createInfo.enabledLayerCount = (uint32_t)(s_ValidationLayers.size());
                createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
            } else {
                createInfo.enabledLayerCount = 0;
            }

            result = vkCreateDevice(s_PhysicalDevice, &createInfo, nullptr, &s_Device);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create logical device, VKA::Init() aborted.")

            vkGetDeviceQueue(s_Device, indices.graphicsFamily.value(), 0, &s_GraphicsQueue);
            vkGetDeviceQueue(s_Device, indices.presentFamily.value(), 0, &s_PresentQueue);
        }
        // - Swap chain
        {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(s_PhysicalDevice);

            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
            VkExtent2D extent = chooseSwapExtent(window, swapChainSupport.capabilities);
            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

            if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
                imageCount = swapChainSupport.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = s_Surface;

            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            QueueFamilyIndices indices = findQueueFamilies(s_PhysicalDevice);
            uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

            if (indices.graphicsFamily != indices.presentFamily) {
                createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                createInfo.queueFamilyIndexCount = 2;
                createInfo.pQueueFamilyIndices = queueFamilyIndices;
            } else {
                createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0; // Optional
                createInfo.pQueueFamilyIndices = nullptr; // Optional
            }
            createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = VK_NULL_HANDLE;

            result = vkCreateSwapchainKHR(s_Device, &createInfo, nullptr, &s_SwapChain);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create swap chain, VKA::Init() aborted.")

            s_SwapChainImageFormat = surfaceFormat.format;
            s_SwapChainExtent = extent;

            vkGetSwapchainImagesKHR(s_Device, s_SwapChain, &imageCount, nullptr);
            s_SwapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(s_Device, s_SwapChain, &imageCount, s_SwapChainImages.data());
        }
        // - Image views
        {
            s_SwapChainImageViews.resize(s_SwapChainImages.size());

            for (size_t i = 0; i < s_SwapChainImages.size(); i++) {
                VkImageViewCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                createInfo.image = s_SwapChainImages[i];

                createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                createInfo.format = s_SwapChainImageFormat;
                createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                createInfo.subresourceRange.baseMipLevel = 0;
                createInfo.subresourceRange.levelCount = 1;
                createInfo.subresourceRange.baseArrayLayer = 0;
                createInfo.subresourceRange.layerCount = 1;

                result = vkCreateImageView(s_Device, &createInfo, nullptr, &s_SwapChainImageViews[i]);
                RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create swap chain, VKA::Init() aborted.")
            }
        }
        // - Render passes
        {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = s_SwapChainImageFormat;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            result = vkCreateRenderPass(s_Device, &renderPassInfo, nullptr, &s_RenderPass);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create render pass, VKA::Init() aborted.")


        }
        // - Graphics Pipeline
        {
            auto vertShaderCode = readFile("Resources/Shader/vert.spv");
            auto fragShaderCode = readFile("Resources/Shader/frag.spv");

            VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
            VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main"; // entry point

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main"; // entry point

            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();

//            vertexInputInfo.vertexBindingDescriptionCount = 1;
//            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
//            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
//            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            std::vector<VkDynamicState> dynamicStates = {
                    VK_DYNAMIC_STATE_VIEWPORT,
                    VK_DYNAMIC_STATE_SCISSOR
            };

            VkPipelineDynamicStateCreateInfo dynamicState{};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = (uint32_t)(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;
//            if viewport/scissor was fixed
//            viewportState.pViewports = &viewport;
//            viewportState.pScissors = &scissor;

            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f; // Optional
            rasterizer.depthBiasClamp = 0.0f; // Optional
            rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisampling.minSampleShading = 1.0f; // Optional
            multisampling.pSampleMask = nullptr; // Optional
            multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
            multisampling.alphaToOneEnable = VK_FALSE; // Optional

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_TRUE;
//            colorBlendAttachment.blendEnable = VK_FALSE; // disables beldending
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f; // Optional
            colorBlending.blendConstants[1] = 0.0f; // Optional
            colorBlending.blendConstants[2] = 0.0f; // Optional
            colorBlending.blendConstants[3] = 0.0f; // Optional

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 0; // Optional
            pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
            pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
            pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

            result = vkCreatePipelineLayout(s_Device, &pipelineLayoutInfo, nullptr, &s_PipelineLayout);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create pipeline layout, VKA::Init() aborted.")

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = nullptr; // Optional
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = s_PipelineLayout;
            pipelineInfo.renderPass = s_RenderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
            pipelineInfo.basePipelineIndex = -1; // Optional
            result = vkCreateGraphicsPipelines(s_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &s_GraphicsPipeline);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create graphics pipeline, VKA::Init() aborted.")

            vkDestroyShaderModule(s_Device, fragShaderModule, nullptr);
            vkDestroyShaderModule(s_Device, vertShaderModule, nullptr);
        }
        // - Framebuffer
        {
            s_SwapChainFramebuffers.resize(s_SwapChainImageViews.size());

            for (size_t i = 0; i < s_SwapChainImageViews.size(); i++) {
                VkImageView attachments[] = {
                        s_SwapChainImageViews[i]
                };

                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = s_RenderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = attachments;
                framebufferInfo.width = s_SwapChainExtent.width;
                framebufferInfo.height = s_SwapChainExtent.height;
                framebufferInfo.layers = 1;

                result = vkCreateFramebuffer(s_Device, &framebufferInfo, nullptr, &s_SwapChainFramebuffers[i]);
                RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create framebuffer, VKA::Init() aborted.")
            }
        }
        // - Command Pool
        {
            QueueFamilyIndices queueFamilyIndices = findQueueFamilies(s_PhysicalDevice);

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

            result = vkCreateCommandPool(s_Device, &poolInfo, nullptr, &s_CommandPool);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create command pool, VKA::Init() aborted.")
        }
        // - Command Buffer
        {
            s_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = s_CommandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = (uint32_t) s_CommandBuffers.size();

            result = vkAllocateCommandBuffers(s_Device, &allocInfo, s_CommandBuffers.data());
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create command buffer, VKA::Init() aborted.")
        }
        // - Sync Objects
        {
            s_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            s_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            s_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                bool success = vkCreateSemaphore(s_Device, &semaphoreInfo, nullptr, &s_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                               vkCreateSemaphore(s_Device, &semaphoreInfo, nullptr, &s_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
                               vkCreateFence(s_Device, &fenceInfo, nullptr, &s_InFlightFences[i]) != VK_SUCCESS;
                RETURN_AND_PRINT_ERROR_IF(success, "Failed to create semaphores, VKA::Init() aborted.")
            }
        }
        // - Vertex Buffer
        {
            const std::vector<Vertex> vertices = {
                    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                    {{0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
                    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
            };

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(vertices[0]) * vertices.size();
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            result = vkCreateBuffer(s_Device, &bufferInfo, nullptr, &s_VertexBuffer);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to create vertex buffer.")

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(s_Device, s_VertexBuffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            result = vkAllocateMemory(s_Device, &allocInfo, nullptr, &s_VertexBufferMemory);
            RETURN_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to allocate vertex buffer memory.")

            vkBindBufferMemory(s_Device, s_VertexBuffer, s_VertexBufferMemory, 0);

            void* data;
            vkMapMemory(s_Device, s_VertexBufferMemory, 0, bufferInfo.size, 0, &data);
            memcpy(data, vertices.data(), (size_t) bufferInfo.size);
            vkUnmapMemory(s_Device, s_VertexBufferMemory);
        }

        std::cout << "Successfully initialized Vulkan." << "\n";
        s_Initialized = true;
    }

    void Draw(GLFWwindow* window) {
        vkWaitForFences(s_Device, 1, &s_InFlightFences[s_CurrentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(s_Device, s_SwapChain, UINT64_MAX, s_ImageAvailableSemaphores[s_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain(window);
            return;
        }
        else EXIT_AND_PRINT_ERROR_IF(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR, "Failed to acquire swap chain image, exit.")

        vkResetFences(s_Device, 1, &s_InFlightFences[s_CurrentFrame]);

        vkResetCommandBuffer(s_CommandBuffers[s_CurrentFrame], 0);
        recordCommandBuffer(s_CommandBuffers[s_CurrentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {s_ImageAvailableSemaphores[s_CurrentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &s_CommandBuffers[s_CurrentFrame];

        VkSemaphore signalSemaphores[] = {s_RenderFinishedSemaphores[s_CurrentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        result = vkQueueSubmit(s_GraphicsQueue, 1, &submitInfo, s_InFlightFences[s_CurrentFrame]);
        EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to submit draw command buffer.")

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {s_SwapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional

        result = vkQueuePresentKHR(s_PresentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || s_FramebufferResized) {
            s_FramebufferResized = false;
            recreateSwapChain(window);
        }
        else EXIT_CHECK_VK_ERROR_AND_PRINT_MESSAGE(result, "Failed to present swap chain image, exit.")

        s_CurrentFrame = (s_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void WaitForDevice() {
        vkDeviceWaitIdle(s_Device);
    }

    void Shutdown() {
        RETURN_AND_PRINT_ERROR_IF(!s_Initialized, "VKA must be initialized to call VKA::Shutdown().")

        // Vulkan
        cleanupSwapChain();

//        vkDestroyBuffer(s_Device, s_VertexBuffer, nullptr);
//        vkFreeMemory(s_Device, s_VertexBufferMemory, nullptr);

        vkDestroyPipeline(s_Device, s_GraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(s_Device, s_PipelineLayout, nullptr);

        vkDestroyRenderPass(s_Device, s_RenderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(s_Device, s_RenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(s_Device, s_ImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(s_Device, s_InFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(s_Device, s_CommandPool, nullptr);

        vkDestroyDevice(s_Device, nullptr);

        if (s_EnableValidationLayers) {
            destroyDebugUtilsMessengerEXT(s_Instance, s_DebugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(s_Instance, s_Surface, nullptr);
        vkDestroyInstance(s_Instance, nullptr);

        s_Initialized = false;
    }
}

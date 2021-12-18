#define GLFW_INCLUDE_VULKAN

#include "GLFW/glfw3.h"
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <vulkan/vulkan.h>

/**
 * @brief
 * @param vulkanInstance
 * @param debugMessenger
 * @param pAllocator
 */
void destroyDebugUtilsMessenger(VkInstance vulkanInstance,
                                VkDebugUtilsMessengerEXT debugMessenger,
                                const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(vulkanInstance,
                                                                            "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(vulkanInstance, debugMessenger, pAllocator);
    } else
    {
        std::cerr << "Could not find vkDestroyDebugUtilsMessenger" << std::endl;
    }
}

VkResult createDebugUtilsMessenger(VkInstance vulkanInstance,
                                   const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                   const VkAllocationCallbacks *pAllocator,
                                   VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(vulkanInstance,
                                                                           "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(vulkanInstance, pCreateInfo, pAllocator, pDebugMessenger);
    } else
    {
        std::cerr << "Could not find vkCreateDebugUtilsMessengerEXT" << std::endl;
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

class VulkanProgram
{
public:

    /**
     * Run the main Vulkan program
     */
    void run()
    {
        // Setup phase
        createGLFWWindow();
        createVulkanInstance();
        surfaceVulkanAndWindow();

        createDevice();
        createSwapchain();
        createGraphicsPipeline();
        createShaderPipeline();
        createFramebuffer();
        createCmdPool();

        createSemaphores();

        // Running phase
        vulkanProgramLoop();

        // Terminating phase
        cleanup();
    }

private:
    GLFWwindow *window = nullptr;

    /**
     * A structure contains all the objects that are needed for a vulkan program
     */
    struct VulkanProgramInfo
    {
        // Vulkan instance where the whole program begins
        VkInstance vulkanInstance = VK_NULL_HANDLE;

        // The logical device that abstract the chosen GPU
        VkDevice GPUDevice = VK_NULL_HANDLE;

        // The chosen physical GPU for rendering
        VkPhysicalDevice chosenGPU = VK_NULL_HANDLE;

        // Command pool that stores command buffers
        VkCommandPool cmdPool = VK_NULL_HANDLE;

        // A list of command buffers which got allocated in <cmdPool>
		std::vector<VkCommandBuffer> cmdBuffers{};

        // A list of instance layers names
        std::vector<const char *> enabledLayers
                {
                };

        // A list of enabled instance extensions
        std::vector<const char *> enabledInstanceExtensions
                {
                        "VK_EXT_debug_utils",
                };

        // A list of enabled device extensions
        std::vector<const char *> enabledDeviceExtensions
                {
                        VK_KHR_SWAPCHAIN_EXTENSION_NAME
                };

        VkDebugUtilsMessengerEXT debugMessenger{};
        uint32_t graphicsQueueFamilyIndex{};
        VkSurfaceKHR vulkanSurface = VK_NULL_HANDLE;
        VkSwapchainKHR vulkanSwapchain = VK_NULL_HANDLE;
        VkFormat vulkanSwapchainFormat{};

        // A list of vulkan images stored in swapchain
        std::vector<VkImage> swapchainImages{};

        // Swapchain extent info
        VkExtent2D swapchainExtent{};

        // A list of vulkan image views
        std::vector<VkImageView> imageViews{};

        // Shader modules
        VkShaderModule vertShaderModule = VK_NULL_HANDLE;
        VkShaderModule fragShaderModule = VK_NULL_HANDLE;

		VkRenderPass renderPass = VK_NULL_HANDLE;

        // Pipeline layout
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline graphicsPipeline{};
		std::vector<VkFramebuffer> swapchainFramebuffers{};

        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
        VkQueue presentAndGraphicsQueue = VK_NULL_HANDLE;

    } vulkanProgramInfo;

    VkResult vkResult{};

    /**
     * Create a window
     */
    void createGLFWWindow()
    {
        if (!glfwInit())
        {
            std::cout << "GLFW window creation failed" << std::endl;
            exit(-1);
        }

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window = glfwCreateWindow(800, 500, "Vulkan Program", nullptr, nullptr);

        if (!window)
        {
            std::cout << "Failed to create window" << std::endl;
            exit(-1);
        }

        uint32_t glfwExtensionCount = 0;
        const char **extensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        for (std::size_t i = 0; i < glfwExtensionCount; i++)
        {
            vulkanProgramInfo.enabledInstanceExtensions.push_back(extensions[i]);
        }

    }

    /**
     * Main loop of this program
     */
    void vulkanProgramLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(vulkanProgramInfo.GPUDevice);
    }

    void createSemaphores()
    {
        VkSemaphoreCreateInfo imageAvailableSemaphoreCreateInfo{};
        VkSemaphoreCreateInfo renderFinishedSemaphoreCreateInfo{};

        imageAvailableSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        renderFinishedSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        vkResult = vkCreateSemaphore(vulkanProgramInfo.GPUDevice,
                                     &imageAvailableSemaphoreCreateInfo,
                                     nullptr,
                                     &vulkanProgramInfo.imageAvailableSemaphore);

        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to create imageAvailableSemaphore" << std::endl;
            exit(-1);
        }

        vkResult = vkCreateSemaphore(vulkanProgramInfo.GPUDevice,
                                     &imageAvailableSemaphoreCreateInfo,
                                     nullptr,
                                     &vulkanProgramInfo.renderFinishedSemaphore);

        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to create renderFinishedSemaphore" << std::endl;
            exit(-1);
        }
    }

    void drawFrame()
    {
        uint32_t imageIndex;
        vkAcquireNextImageKHR(vulkanProgramInfo.GPUDevice,
                              vulkanProgramInfo.vulkanSwapchain,
                              UINT64_MAX,
                              vulkanProgramInfo.imageAvailableSemaphore,
                              VK_NULL_HANDLE,
                              &imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {vulkanProgramInfo.imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &vulkanProgramInfo.cmdBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = {vulkanProgramInfo.renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkGetDeviceQueue(vulkanProgramInfo.GPUDevice,
                         vulkanProgramInfo.graphicsQueueFamilyIndex,
                         0,
                         &vulkanProgramInfo.presentAndGraphicsQueue);

        vkResult = vkQueueSubmit(vulkanProgramInfo.presentAndGraphicsQueue,
                                 1,
                                 &submitInfo,
                                 VK_NULL_HANDLE);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapchains[] = {vulkanProgramInfo.vulkanSwapchain};
        presentInfo.pSwapchains = swapchains;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(vulkanProgramInfo.presentAndGraphicsQueue, &presentInfo);
        vkQueueWaitIdle(vulkanProgramInfo.presentAndGraphicsQueue);
    }

    /**
     * Create vulkan swapchain for presentation
     */
    void createSwapchain()
    {
        VkSwapchainCreateInfoKHR swapchainCreateInfo{};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.pNext = nullptr;
        swapchainCreateInfo.surface = vulkanProgramInfo.vulkanSurface;

        // -------------------------------------------------------------
        // Get and fill image format and image colorspace information
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanProgramInfo.chosenGPU,
                                             vulkanProgramInfo.vulkanSurface,
                                             &formatCount,
                                             nullptr);


        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanProgramInfo.chosenGPU,
                                             vulkanProgramInfo.vulkanSurface,
                                             &formatCount,
                                             surfaceFormats.data());

        swapchainCreateInfo.imageFormat = surfaceFormats[0].format;
        swapchainCreateInfo.imageColorSpace = surfaceFormats[0].colorSpace;

        // Record swapchain format for later use in render pipeline
        vulkanProgramInfo.vulkanSwapchainFormat = swapchainCreateInfo.imageFormat;
        // ------------------------------------------
        // Query for surface capabilities
        VkSurfaceCapabilitiesKHR surfaceCapabilities{};

        vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanProgramInfo.chosenGPU,
                                                             vulkanProgramInfo.vulkanSurface,
                                                             &surfaceCapabilities);
        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to get surface capabilities" << std::endl;
            exit(-1);
        }

        // NEEDRESEARCH double buffering and triple buffering. Which one is better?
        swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;

        // Choose the extent of swapchain
        VkExtent2D swapchainExtent{};

        // Check what is the value of currentExtent
        // If it is special value, choose any value fit for window
        if (surfaceCapabilities.currentExtent.width == UINT32_MAX)
        {
            swapchainExtent.width = 1600;
            swapchainExtent.height = 1000;

            swapchainExtent.width = std::max(surfaceCapabilities.minImageExtent.width,
                                             std::min(surfaceCapabilities.maxImageExtent.width,
                                                      swapchainExtent.width));

            swapchainExtent.height = std::max(surfaceCapabilities.minImageExtent.height,
                                              std::min(surfaceCapabilities.maxImageExtent.height,
                                                       swapchainExtent.height));
        }
            // If it is specified, just follow whatever is specified
        else
        {
            swapchainExtent = surfaceCapabilities.currentExtent;
        }

        vulkanProgramInfo.swapchainExtent = swapchainExtent;

        swapchainCreateInfo.imageExtent = swapchainExtent;

        // Other parameters
        // Number of views
        swapchainCreateInfo.imageArrayLayers = 1;

        // Usage of images
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Sharing mode
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Orientation
        swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;

        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainCreateInfo.clipped = VK_TRUE;
        swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;


        // After create info is filled, create swapchain
        vkResult = vkCreateSwapchainKHR(vulkanProgramInfo.GPUDevice,
                                        &swapchainCreateInfo,
                                        nullptr,
                                        &vulkanProgramInfo.vulkanSwapchain);

        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to create swapchain" << std::endl;
            exit(-1);
        }

        // Get access to vulkan images stored in swapchain
        uint32_t swapchainImageCount = 0;
        vkResult = vkGetSwapchainImagesKHR(vulkanProgramInfo.GPUDevice,
                                           vulkanProgramInfo.vulkanSwapchain,
                                           &swapchainImageCount,
                                           nullptr);

        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to count number of swapchain images" << std::endl;
            exit(-1);
        }

        vulkanProgramInfo.swapchainImages.resize(swapchainImageCount);

        vkResult = vkGetSwapchainImagesKHR(vulkanProgramInfo.GPUDevice,
                                           vulkanProgramInfo.vulkanSwapchain,
                                           &swapchainImageCount,
                                           vulkanProgramInfo.swapchainImages.data());
        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to get swapchain images" << std::endl;
            exit(-1);
        }

        // Create image views of images in swapchain
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.format = swapchainCreateInfo.imageFormat;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.pNext = nullptr;

        vulkanProgramInfo.imageViews.resize(vulkanProgramInfo.swapchainImages.size());
        for (std::size_t i = 0; i < vulkanProgramInfo.imageViews.size(); i++)
        {
            imageViewCreateInfo.image = vulkanProgramInfo.swapchainImages[i];
            vkResult = vkCreateImageView(vulkanProgramInfo.GPUDevice,
                                         &imageViewCreateInfo,
                                         nullptr,
                                         &vulkanProgramInfo.imageViews[i]);

            if (vkResult != VK_SUCCESS)
            {
                std::cout << "Failed to create vulkan image view [" << i << "]" << std::endl;
                exit(-1);
            }
        }
    }

    /**
     * Create surface between vulkan instance and window created by glfw
     */
    void surfaceVulkanAndWindow()
    {
        vkResult = glfwCreateWindowSurface(vulkanProgramInfo.vulkanInstance,
                                           window,
                                           nullptr,
                                           &vulkanProgramInfo.vulkanSurface);
        if (vkResult != VK_SUCCESS)
        {
            std::cout << "failed to create surface between vulkan instance and glfw window" << std::endl;
            exit(-1);
        }
    }

    /**
     * Create a vulkan instance to start the whole vulkan program
     */
    void createVulkanInstance()
    {
        // Get a list of instance layer names for instance
        //  Standard procedure for getting layer info
        std::vector<VkLayerProperties> supportedLayers{};
        uint32_t supportedLayerCount{};

        vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr);

        supportedLayers.resize(supportedLayerCount);
        vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data());

        // Check if required layer list is in the name list
        if (!enabledLayersSupported(supportedLayers, vulkanProgramInfo.enabledLayers))
        {
            std::cout << "Error: some layers are not supported" << std::endl;
            exit(-1);
        }

        // Get a list of instance extension names
        std::vector<VkExtensionProperties> instanceExtensionPropertiesList{};
        uint32_t instanceExtensionPropertiesCount{};

        if (VK_SUCCESS == vkEnumerateInstanceExtensionProperties(nullptr,
                                                                 &instanceExtensionPropertiesCount,
                                                                 nullptr))
        {
            instanceExtensionPropertiesList.resize(instanceExtensionPropertiesCount);
        } else
        {
            std::cout << "Failed to count number of instance extensions" << std::endl;
            exit(-1);
        }

        if (VK_SUCCESS != vkEnumerateInstanceExtensionProperties(nullptr,
                                                                 &instanceExtensionPropertiesCount,
                                                                 instanceExtensionPropertiesList.data()))
        {
            std::cout << "Failed to get a list of instance extensions" << std::endl;
            exit(-1);
        }

        // Check if every instance extension is supported
        if (!checkEnabledExtensionsSupported(instanceExtensionPropertiesList,
                                             vulkanProgramInfo.enabledInstanceExtensions))
        {
            std::cout << "Not all required instance extensions supported" << std::endl;
            exit(-1);
        }

        // Create <VkDebugUtilsMessengerCreateInfoEXT>. This one is also going to be chained with instance create
        // info so debug messenger can detect what is wrong with <VkCreateInstance> and <VkDestroyInstance>
        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};

        debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugMessengerCreateInfo.pNext = nullptr;
        debugMessengerCreateInfo.flags = 0;
        debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

        debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        debugMessengerCreateInfo.pfnUserCallback = debugMessengerCallback;
        debugMessengerCreateInfo.pUserData = nullptr;

        // Now everything is supported and debug messenger is created start by creating <VkInstanceCreateInfo>
        VkInstanceCreateInfo vulkanInstanceCreateInfo{};

        vulkanInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vulkanInstanceCreateInfo.pNext = &debugMessengerCreateInfo;
        vulkanInstanceCreateInfo.flags = 0;
        vulkanInstanceCreateInfo.pApplicationInfo = nullptr;
        vulkanInstanceCreateInfo.enabledLayerCount = (uint32_t) vulkanProgramInfo.enabledLayers.size();
        vulkanInstanceCreateInfo.ppEnabledLayerNames = vulkanProgramInfo.enabledLayers.data();
        vulkanInstanceCreateInfo.enabledExtensionCount = (uint32_t) vulkanProgramInfo.enabledInstanceExtensions.size();
        vulkanInstanceCreateInfo.ppEnabledExtensionNames = vulkanProgramInfo.enabledInstanceExtensions.data();

        // Create vulkan instance
        if (VK_SUCCESS != vkCreateInstance(&vulkanInstanceCreateInfo,
                                           nullptr,
                                           &vulkanProgramInfo.vulkanInstance))
        {
            std::cout << "Failed to create vulkan instance" << std::endl;
            exit(-1);
        }

        if (VK_SUCCESS != createDebugUtilsMessenger(vulkanProgramInfo.vulkanInstance,
                                                    &debugMessengerCreateInfo,
                                                    nullptr,
                                                    &vulkanProgramInfo.debugMessenger))
        {
            std::cerr << "Failed to create debug utils messenger";
        }
    }

    /**
     * Create Logical device that abstracts chosen GPU device (physical device)
     */
    void createDevice()
    {
        uint32_t physicalDeviceNum{};
        std::vector<VkPhysicalDevice> availablePhysicalDevices{};

        vkResult = vkEnumeratePhysicalDevices(vulkanProgramInfo.vulkanInstance,
                                              &physicalDeviceNum,
                                              nullptr);
        if (vkResult != VK_SUCCESS)
        {
            std::cerr << "Failed to count physical device number" << std::endl;
            exit(-1);
        }

        availablePhysicalDevices.resize(physicalDeviceNum);

        vkResult = vkEnumeratePhysicalDevices(vulkanProgramInfo.vulkanInstance,
                                              &physicalDeviceNum,
                                              availablePhysicalDevices.data());

        VkPhysicalDeviceProperties physicalDeviceProperties{};
        vkGetPhysicalDeviceProperties(availablePhysicalDevices[0],
                                      &physicalDeviceProperties);
        if (vkResult != VK_SUCCESS)
        {
            std::cerr << "Failed to get a list of physical devices" << std::endl;
            exit(-1);
        }

        // Pick the chosen GPU device
        vulkanProgramInfo.chosenGPU = availablePhysicalDevices[0];

        // After a physical device is picked, check its queue family information
        uint32_t queueFamilyCount = 0;
        std::vector<VkQueueFamilyProperties> queueFamilyPropertiesList{};

        vkGetPhysicalDeviceQueueFamilyProperties(vulkanProgramInfo.chosenGPU,
                                                 &queueFamilyCount,
                                                 nullptr);

        queueFamilyPropertiesList.resize(queueFamilyCount);

        vkGetPhysicalDeviceQueueFamilyProperties(vulkanProgramInfo.chosenGPU,
                                                 &queueFamilyCount,
                                                 queueFamilyPropertiesList.data());

        uint32_t graphicsQueueFamilyIndex = -1;
        // Choose the first queue family that supports graphics and presentation
        VkBool32 presentationSupport = VK_FALSE;
        bool foundQueue = false;

        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyPropertiesList.size(); queueFamilyIndex++)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(vulkanProgramInfo.chosenGPU,
                                                 queueFamilyIndex,
                                                 vulkanProgramInfo.vulkanSurface,
                                                 &presentationSupport);

            if (queueFamilyPropertiesList[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                presentationSupport == VK_TRUE)
            {
                graphicsQueueFamilyIndex = queueFamilyIndex;
                foundQueue = true;
                break;
            }
        }

        if (!foundQueue)
        {
            std::cout << "Failed to find queue that satisfy requirements" << std::endl;
            exit(-1);
        }
        vulkanProgramInfo.graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;

        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.flags = 0;
        queueCreateInfo.pNext = nullptr;
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};

        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = nullptr;
        deviceCreateInfo.flags = 0;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.ppEnabledLayerNames = nullptr;
        deviceCreateInfo.pEnabledFeatures = nullptr;
        deviceCreateInfo.enabledLayerCount = 0;
        deviceCreateInfo.ppEnabledLayerNames = nullptr;

        // Enable device layers
        uint32_t deviceExtensionCount = 0;
        std::vector<VkExtensionProperties> availableDeviceExtensions{};

        vkEnumerateDeviceExtensionProperties(vulkanProgramInfo.chosenGPU,
                                             nullptr,
                                             &deviceExtensionCount,
                                             nullptr);

        availableDeviceExtensions.resize(deviceExtensionCount);

        vkResult = vkEnumerateDeviceExtensionProperties(vulkanProgramInfo.chosenGPU,
                                                        nullptr,
                                                        &deviceExtensionCount,
                                                        availableDeviceExtensions.data());
        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to get a list of device extensions" << std::endl;
            exit(-1);
        }

        checkEnabledExtensionsSupported(availableDeviceExtensions,
                                        vulkanProgramInfo.enabledDeviceExtensions);
        deviceCreateInfo.ppEnabledExtensionNames = vulkanProgramInfo.enabledDeviceExtensions.data();
        deviceCreateInfo.enabledExtensionCount = (uint32_t) vulkanProgramInfo.enabledDeviceExtensions.size();

        vkResult = vkCreateDevice(vulkanProgramInfo.chosenGPU,
                                  &deviceCreateInfo,
                                  nullptr,
                                  &vulkanProgramInfo.GPUDevice);


        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to create gpu device" << std::endl;
            exit(-1);
        }

    }

    /**
     * Create command pool where command buffers get allocated
     */
    void createCmdPool()
    {
        VkCommandPoolCreateInfo cmdPoolCreateInfo{};
        cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCreateInfo.pNext = nullptr;
        cmdPoolCreateInfo.flags = 0;
        cmdPoolCreateInfo.queueFamilyIndex = vulkanProgramInfo.graphicsQueueFamilyIndex;

        vkResult = vkCreateCommandPool(vulkanProgramInfo.GPUDevice,
                                       &cmdPoolCreateInfo,
                                       nullptr,
                                       &vulkanProgramInfo.cmdPool);

        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to create command pool" << std::endl;
            exit(-1);
        }

		vulkanProgramInfo.cmdBuffers.resize(vulkanProgramInfo.swapchainFramebuffers.size());

        VkCommandBufferAllocateInfo cmdBufferAllocateInfo{};
        cmdBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAllocateInfo.pNext = nullptr;
        cmdBufferAllocateInfo.commandBufferCount = (uint32_t) vulkanProgramInfo.swapchainFramebuffers.size();
        cmdBufferAllocateInfo.commandPool = vulkanProgramInfo.cmdPool;
        cmdBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        vkResult = vkAllocateCommandBuffers(vulkanProgramInfo.GPUDevice,
                                            &cmdBufferAllocateInfo,
                                            vulkanProgramInfo.cmdBuffers.data());

        for (std::size_t i = 0; i < vulkanProgramInfo.cmdBuffers.size(); i++)
        {
            VkCommandBufferBeginInfo bufferBeginInfo{};
            bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            vkResult = vkBeginCommandBuffer(vulkanProgramInfo.cmdBuffers[i],
                                            &bufferBeginInfo);

            if (vkResult != VK_SUCCESS)
            {
                std::cout << "Failed to begin command buffer" << std::endl;
                exit(-1);
            }

            // Begin render passes
            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderPass = vulkanProgramInfo.renderPass;
            renderPassBeginInfo.framebuffer = vulkanProgramInfo.swapchainFramebuffers[i];
            renderPassBeginInfo.renderArea.offset = {0, 0};
            renderPassBeginInfo.renderArea.extent = vulkanProgramInfo.swapchainExtent;
            VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(vulkanProgramInfo.cmdBuffers[i],
                                 &renderPassBeginInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(vulkanProgramInfo.cmdBuffers[i],
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              vulkanProgramInfo.graphicsPipeline);

            vkCmdDraw(vulkanProgramInfo.cmdBuffers[i],
                      3,
                      1,
                      0,
                      0);

            vkCmdEndRenderPass(vulkanProgramInfo.cmdBuffers[i]);

            vkResult = vkEndCommandBuffer(vulkanProgramInfo.cmdBuffers[i]);
            if (vkResult != VK_SUCCESS)
            {
                std::cout << "Failed to record command buffer" << std::endl;
                exit(-1);
            }
        }
    }

    /**
     * Create vulkan rendering pipeline
     */
    void createShaderPipeline()
    {
        auto vertShaderCode = readFile("../src/vert.spv");
        auto fragShaderCode = readFile("../src/frag.spv");

        vulkanProgramInfo.vertShaderModule = createShaderModule(vertShaderCode);
        vulkanProgramInfo.fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vulkanProgramInfo.vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = vulkanProgramInfo.fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        // How vertices should be assembled
        VkPipelineInputAssemblyStateCreateInfo assemblyInfo{};
        assemblyInfo.pNext = nullptr;
        assemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assemblyInfo.flags = 0;
        assemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        assemblyInfo.primitiveRestartEnable = VK_FALSE;

        // Set up viewport
        VkViewport viewport{};
        viewport.width = (float) vulkanProgramInfo.swapchainExtent.width;
        viewport.height = (float) vulkanProgramInfo.swapchainExtent.height;
        viewport.x = 0;
        viewport.y = 0;
        viewport.maxDepth = 1.0f;
        viewport.minDepth = 0.0f;

        // Scissor
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vulkanProgramInfo.swapchainExtent;

        // Viewport create info
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // Set up rasterization stage
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		// Multisample State info
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        // Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // No dynamic states

        // Set up pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        vkResult = vkCreatePipelineLayout(vulkanProgramInfo.GPUDevice,
                                          &pipelineLayoutInfo,
                                          nullptr,
                                          &vulkanProgramInfo.pipelineLayout);

        if (vkResult != VK_SUCCESS)
        {
            std::cout << "Failed to create vulkan pipeline" << std::endl;
            exit(-1);
        }

		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
		graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		graphicsPipelineCreateInfo.stageCount = 2;
		graphicsPipelineCreateInfo.pStages = shaderStages;
		graphicsPipelineCreateInfo.pVertexInputState = &vertexInputInfo;
		graphicsPipelineCreateInfo.pInputAssemblyState = &assemblyInfo;
		graphicsPipelineCreateInfo.pViewportState = &viewportState;
		graphicsPipelineCreateInfo.pRasterizationState = &rasterizer;
		graphicsPipelineCreateInfo.pMultisampleState = &multisampling;
		graphicsPipelineCreateInfo.pColorBlendState = &colorBlending;
		graphicsPipelineCreateInfo.pDynamicState = nullptr;
		graphicsPipelineCreateInfo.layout = vulkanProgramInfo.pipelineLayout;
		graphicsPipelineCreateInfo.renderPass = vulkanProgramInfo.renderPass;
		graphicsPipelineCreateInfo.subpass = 0;

		vkResult = vkCreateGraphicsPipelines(vulkanProgramInfo.GPUDevice,
											 VK_NULL_HANDLE,
											 1,
											 &graphicsPipelineCreateInfo,
											 nullptr,
											 &vulkanProgramInfo.graphicsPipeline);

		if (vkResult != VK_SUCCESS)
		{
			std::cout << "Failed to create vulkan pipeline" << std::endl;
			exit(-1);
		}

        vkDestroyShaderModule(vulkanProgramInfo.GPUDevice,
                              vulkanProgramInfo.fragShaderModule,
                              nullptr);

        vkDestroyShaderModule(vulkanProgramInfo.GPUDevice,
                              vulkanProgramInfo.vertShaderModule,
                              nullptr);
    }

    /**
     * Create graphics pipeline. This includes render pass.
     */
    void createGraphicsPipeline()
    {
        // set up attachment description
        VkAttachmentDescription attachmentDescription{};
        attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescription.format = vulkanProgramInfo.vulkanSwapchainFormat;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Attachment reference for subpasses
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Subpass that exist in a render pass
		VkSubpassDescription subpassDescription{};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &attachmentDescription;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDescription;
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &dependency;

		vkResult = vkCreateRenderPass(vulkanProgramInfo.GPUDevice,
									  &renderPassCreateInfo,
									  nullptr,
									  &vulkanProgramInfo.renderPass);
		if (vkResult != VK_SUCCESS)
		{
			std::cout << "Failed to create render pass" << std::endl;
			exit(-1);
		}
    }

	void createFramebuffer()
	{
		vulkanProgramInfo.swapchainFramebuffers.resize(vulkanProgramInfo.imageViews.size());
		for (size_t i = 0; i < vulkanProgramInfo.imageViews.size(); i++)
		{
			VkImageView attachment[] =
			{
				vulkanProgramInfo.imageViews[i]
			};

			VkFramebufferCreateInfo framebufferCreatInfo{};
			framebufferCreatInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreatInfo.renderPass = vulkanProgramInfo.renderPass;
			framebufferCreatInfo.attachmentCount = 1;
			framebufferCreatInfo.height = vulkanProgramInfo.swapchainExtent.height;
			framebufferCreatInfo.width = vulkanProgramInfo.swapchainExtent.width;
			framebufferCreatInfo.pAttachments = attachment;
			framebufferCreatInfo.layers = 1;

			// Create swapchain framebuffers
			vkResult = vkCreateFramebuffer(vulkanProgramInfo.GPUDevice,
										  &framebufferCreatInfo,
										  nullptr,
										  &vulkanProgramInfo.swapchainFramebuffers[i]);
			if (vkResult != VK_SUCCESS)
			{
				std::cout << "Failed to create swapchain framebuffer" << std::endl;
				exit(-1);
			}

		}
	}
    /**
     * Janitor to free up any created or allocated memory
     */
    void cleanup() const
    {
        vkDestroySemaphore(vulkanProgramInfo.GPUDevice,
                           vulkanProgramInfo.imageAvailableSemaphore,
                           nullptr);

        vkDestroySemaphore(vulkanProgramInfo.GPUDevice,
                           vulkanProgramInfo.renderFinishedSemaphore,
                           nullptr);

		for (const VkFramebuffer& framebuffer : vulkanProgramInfo.swapchainFramebuffers)
		{
			vkDestroyFramebuffer(vulkanProgramInfo.GPUDevice,
								 framebuffer,
								 nullptr);
		}

        for (const VkImageView &imageView: vulkanProgramInfo.imageViews)
        {
            vkDestroyImageView(vulkanProgramInfo.GPUDevice,
                               imageView,
                               nullptr);
        }

        vkDestroySwapchainKHR(vulkanProgramInfo.GPUDevice,
                              vulkanProgramInfo.vulkanSwapchain,
                              nullptr);

        vkDestroyCommandPool(vulkanProgramInfo.GPUDevice,
                             vulkanProgramInfo.cmdPool,
                             nullptr);

		vkDestroyPipeline(vulkanProgramInfo.GPUDevice,
						  vulkanProgramInfo.graphicsPipeline,
						  nullptr);

        vkDestroyPipelineLayout(vulkanProgramInfo.GPUDevice,
                                vulkanProgramInfo.pipelineLayout,
                                nullptr);

		vkDestroyRenderPass(vulkanProgramInfo.GPUDevice,
							vulkanProgramInfo.renderPass,
							nullptr);


        vkDestroyDevice(vulkanProgramInfo.GPUDevice, nullptr);

        destroyDebugUtilsMessenger(vulkanProgramInfo.vulkanInstance,
                                   vulkanProgramInfo.debugMessenger,
                                   nullptr);

        vkDestroySurfaceKHR(vulkanProgramInfo.vulkanInstance,
                            vulkanProgramInfo.vulkanSurface,
                            nullptr);

        vkDestroyInstance(vulkanProgramInfo.vulkanInstance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();


    }

    // ================================================================================
    // The followings are a bunch of helper methods
    // ================================================================================

    /**
     *
     * @param code
     * @return
     */
    VkShaderModule createShaderModule(const std::vector<char> &code) const
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(vulkanProgramInfo.GPUDevice,
                                 &createInfo,
                                 nullptr,
                                 &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    static std::vector<char> readFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            std::cout << "failed to open file!" << std::endl;
            exit(-1);
        }

        std::streamsize fileSize = (std::streamsize) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    /**
     * A callback for debug messenger
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
            __attribute__((unused)) VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            __attribute__((unused)) VkDebugUtilsMessageTypeFlagsEXT messageTypes,
            const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
            __attribute__((unused)) void *pUserData)
    {
        std::cerr << "Validation Layer Debug Message: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    /**
     * Check if all the instance extensions in <enabledExtensions> is in <supportedExtensions>
     */
    static bool checkEnabledExtensionsSupported(const std::vector<VkExtensionProperties> &supportedExtensions,
                                                const std::vector<const char *> &enabledExtensions)
    {
        bool currExtensionSupported = false;

        for (const char *const &enabledExtension: enabledExtensions)
        {
            for (const VkExtensionProperties &supportedExtension: supportedExtensions)
            {
                if (strcmp(supportedExtension.extensionName, enabledExtension) == 0)
                {
                    currExtensionSupported = true;
                }
            }

            // If after comparing <currExtensionSupported> is still <false>,
            // then no matching extension name found
            if (!currExtensionSupported)
            {
                return false;
            }
        }

        // If reached here, it means all extensions have found their matching pair
        return true;
    }

    /**
     * Check if all the layers in <enabledLayers> is in <supportedLayers>
     */
    static bool enabledLayersSupported(const std::vector<VkLayerProperties> &supportedLayers,
                                       const std::vector<const char *> &enabledLayers)
    {
        bool currLayerSupported;

        for (const char *const &enabledLayer: enabledLayers)
        {
            currLayerSupported = false;

            // Check current enabled layer against every layer in supported layers
            for (const VkLayerProperties &supportedLayer: supportedLayers)
            {
                if (strcmp(enabledLayer, supportedLayer.layerName) == 0)
                {
                    currLayerSupported = true;
                }
            }

            // At the end, if <currLayerSupported> is still false, it means no matching supported layer.
            if (!currLayerSupported)
            {
                return false;
            }
        }

        // If executes here, it means all the layers passed the check above.
        // So all layers is supported.
        return true;
    }

};

int main()
{
    VulkanProgram vulkanProgram{};
    vulkanProgram.run();
    return 0;
}

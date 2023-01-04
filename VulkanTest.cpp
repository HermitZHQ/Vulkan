// 目前这是我上传的第一版vulkan代码，连三角形还画不出来，目前主要用来初始化仓库
// 要点记录，这里的vulkan封装是基于glfw的，可以到官方下载，另外还需要的三方库有glm
// 这些三方库都统一放到VulkanSDK/1.xxx.xxxx目录下的Third-Party目录中（新建目录）
// 子目录新建Bin32，Bin64（32和64主要用于区分版本，目前我只创建了Bin64用于存放lib和dll）和Include

// 这里没有显示的包含vk的头文件，因为我们使用了glfw的框架，但是要让vk生效的话，必须在包含glfw的头文件前，声明GLFW_INCLUDE_VULKAN的宏定义，这样才能正常使用
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <fstream>

const int WIDTH = 800;
const int HEIGHT = 600;

// 关于vk pipeline的一些重点知识：
// 如果之前使用过旧的API(OpenGL和Direct3D),那么将可以随意通过glBlendFunc和OMSetBlendState调用更改管线设置。Vulkan中的图形管线几乎不可改变，因此如果需要更改着色器，绑定到不同的帧缓冲区或者更改混合函数，则必须从头创建管线。
// 缺点是必须创建一些管线，这些管线代表在渲染操作中使用的不同的组合状态。但是由于所有管线的操作都是提前知道的，所以可以通过驱动程序更好的优化它

// KHR vk的默认调试层，应该是开启了所有的调试的
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    // readFile函数将会从文件中读取所有的二进制数据，并用std::vector字节集合管理。我们使用两个标志用以打开文件:
    // ate:在文件末尾开始读取
    // binary : 以二进制格式去读文件(避免字符格式的转义)
    // 从文件末尾开始读取的优点是我们可以使用读取位置来确定文件的大小并分配缓冲区
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    // 之后我们可以追溯到文件的开头，同时读取所有的字节
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    // 最后关闭文件，返回字节数据
    file.close();
    return buffer;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    // vk中的很多函数都不是可以直接调用的，需要先通过vkGetInstanceProcAddr来获取到函数后，才能使用
    // 而vkGetInstancePorcAddr是从vk的dll中加载出来的函数，vulkan-1.dll
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

// 队列簇
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class HelloTriangleApplication {
public:
    void run() {
        // glfw初始化窗口相关，不重要，例行调用
        initWindow();
        // vk相关初始化，重要，需要逐步理解
        initVulkan();
        // glfw的消息循环，例行调用
        mainLoop();
        // vk的相关资源释放，重要
        cleanup();
    }

private:
    GLFWwindow* window;

    VkInstance instance;// 实例
    VkDebugUtilsMessengerEXT debugMessenger;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;// 物理设备
    VkDevice device;// 逻辑设备

    VkQueue graphicsQueue;// 用于记录图形队列的索引
    VkQueue presentQueue;// 还有一件事，就是修改逻辑设备的创建过程，以创建呈现队列并获取VkQueue句柄，还是要添加一个变量

    // Vulkan Window Surface，到目前为止，我们了解到Vulkan是一个与平台特性无关联的API集合。它不能直接与窗口系统进行交互。为了将渲染结果呈现到屏幕，需要建立Vulkan与窗体系统之间的连接，我们需要使用WSI(窗体系统集成)扩展。
    // 在本小节中，我们将讨论第一个，即VK_KHR_surface(VK_KHR_win32_surface)。它暴露了VkSurfaceKHR，它代表surface的一个抽象类型，用以呈现渲染图像使用。
    // 我们程序中将要使用到的surface是由我们已经引入的GLFW扩展及其打开的相关窗体支持的。简单来说surface就是Vulkan与窗体系统的连接桥梁。

    // 虽然VkSurfaceKHR对象及其用法与平台无关联，但创建过程需要依赖具体的窗体系统的细节。
    // 比如，在Windows平台中，它需要WIndows上的HWND和HMODULE句柄。因此针对特定平台提供相应的扩展，在Windows上为VK_KHR_win32_surface，它自动包含在glfwGetRequiredInstanceExtensions列表中

    // 我们将会演示如何使用特定平台的扩展来创建Windows上的surface桥，但是不会在教程中实际使用它。使用GLFW这样的库避免了编写没有任何意义的跨平台相关代码。
    // GLFW实际上通过glfwCreateWindowSurface很好的处理了平台差异性。当然了，比较理想是在依赖它们帮助我们完成具体工作之前，了解一下背后的实现是有帮助的。

    // 因为一个窗体surface是一个Vulkan对象，它需要填充VkWin32SurfaceCreateInfoKHR结构体，这里有两个比较重要的参数:hwnd和hinstance。如果熟悉windows下开发应该知道，这些是窗口和进程的句柄。
    //     VkWin32SurfaceCreateInfoKHR createInfo;
    //     createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    //     createInfo.hwnd = glfwGetWin32Window(window);
    //     createInfo.hinstance = GetModuleHandle(nullptr);
    //     glfwGetWin32Window函数用于从GLFW窗体对象获取原始的HWND。GetModuleHandle函数返回当前进程的HINSTANCE句柄
    // 填充完结构体之后，可以利用vkCreateWin32SurfaceKHR创建surface桥，和之前获取创建、销毁DebugReportCallEXT一样，这里同样需要通过instance获取创建surface用到的函数。这里涉及到的参数分别为instance, surface创建的信息，自定义分配器和最终保存surface的句柄变量。
    // 
    //     auto CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
    // 
    //     if (!CreateWin32SurfaceKHR || CreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
    //         throw std::runtime_error("failed to create window surface!");
    //     }

    // 该过程与其他平台类似，比如Linux，使用X11界面窗体系统，可以通过vkCreateXcbSurfaceKHR函数建立连接。
    // glfwCreateWindowSurface函数根据不同平台的差异性，在实现细节上会有所不同。我们现在将其整合到我们的程序中。从initVulkan中添加一个函数createSurface, 安排在createInstnace和setupDebugCallback函数之后。

    // VK_KHR_surface扩展是一个instance级扩展，我们目前为止已经启用过它，它包含在glfwGetRequiredInstanceExtensions返回的列表中
    VkSurfaceKHR surface;// surface，和系统强相关的，所以这并不是vk的核心，而如果要进行显示的话，我们的交换链需要和surface打交道

    VkSwapchainKHR swapChain;// 现在添加一个类成员变量存储VkSwapchainKHR对象:
    // 交换链创建后，需要获取VkImage相关的句柄。它会在后续渲染的章节中引用。添加类成员变量存储该句柄:
    std::vector<VkImage> swapChainImages;// 图像被交换链创建，也会在交换链销毁的同时自动清理，所以我们不需要添加任何清理代码。
    // Vulkan 图像与视图，使用任何的VkImage，包括在交换链或者渲染管线中的，我们都需要创建VkImageView对象。
    // 从字面上理解它就是一个针对图像的视图或容器，通过它具体的渲染管线才能够读写渲染数据，换句话说VkImage不能与渲染管线进行交互
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    // 如果仅仅是为了测试交换链的有效性是远远不够的，因为它还不能很好的与窗体surface兼容。创建交换链同样也需要很多设置，所以我们需要了解一些有关设置的细节
    // 基本上有三大类属性需要设置:

    // 基本的surface功能属性(min / max number of images in swap chain, min / max width and height of images)
    //    Surface格式(pixel format, color space)
    //    有效的presentation模式
    //    与findQueueFamilies类似，我们使用结构体一次性的传递详细的信息。三类属性封装在如下结构体中：
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    // 这是vulkan相关很重要的一个函数，包含了各种vulkan需要的初始化，目前还不完全
    void initVulkan() {
        // 实例的创建，vulkan中很重要的概念
        createInstance();
        // 需要在instance创建之后立即创建窗体surface，因为它会影响物理设备的选择。之所以在本小节将surface创建逻辑纳入讨论范围，是因为窗体surface对于渲染、呈现方式是一个比较大的课题，
        // 如果过早的在创建物理设备加入这部分内容，会混淆基本的物理设备设置工作。另外窗体surface本身对于Vulkan也是非强制的。Vulkan允许这样做，不需要同OpenGL一样必须要创建窗体surface。
        createSurface();
        // debug的设置函数，需要开启validation layer的相关设置，才能正常调试，默认vk是不开启调试层的
        setupDebugMessenger();
        // 物理设备的创建
        pickPhysicalDevice();
        // 逻辑设备的创建
        createLogicalDevice();
        // 创建交换链
        createSwapChain();
        // 创建图像视图
        createImageViews();
        // 创建图形管线
        createGraphicsPipeline();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        // 与图像不同的是，图像视图需要明确的创建过程，所以在程序退出的时候，我们需要添加一个循环去销毁他们
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroySwapchainKHR(device, swapChain, nullptr);

        // device和实例都在后面摧毁，因为前面还有些资源的释放需要用到他们，比如swapchain
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        // 基本上目前看到的情况，所有实例以外的vk创建obj，都应该在销毁vk实例前，进行销毁，否则debug的回调是可以看到错误信息的
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void createSurface() {
        // 参数是VkInstance,GLFW窗体的指针，自定义分配器和用于存储VkSurfaceKHR变量的指针。对于不同平台统一返回VkResult。GLFW没有提供专用的函数销毁surface,但是可以简单的通过Vulkan原始的API完成:
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            // 检查物理设备的适配性，是否满足要求
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily.value(), (uint32_t)indices.presentFamily.value() };
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        // 接着，我们需要多个VkDeviceQueueCreateInfo结构体来从两个簇创建队列。比较好的做法时创建一套所需队列各不相同的队列簇
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : queueFamilyIndices) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }


        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;

        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures = {};

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

//         createInfo.pQueueCreateInfos = &queueCreateInfo;
//         createInfo.queueCreateInfoCount = 1;
        // 动态创建2个队列簇，一个是graphics的，一个是present的
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = 0;

        // 逻辑设备创建时，必须明确带上swapchain的ext
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // 验证层的开启与否也需要明确
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        // swapchain中3个关键的参数，分别通过3个函数封装分别处理
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // 实际上还有一些小事情需要确定，但是比较简单，所以没有单独创建函数。第一个是交换链中的图像数量，可以理解为队列的长度。它指定运行时图像的最小数量，我们将尝试大于1的图像数量，以实现三重缓冲。
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // 对于maxImageCount数值为0代表除了内存之外没有限制，这就是为什么我们需要检查。
        // 与Vulkan其他对象的创建过程一样，创建交换链也需要填充大量的结构体:
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        // 在指定交换链绑定到具体的surface之后，需要指定交换链图像有关的详细信息:
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        // 尝试移除createInfo.imageExtent = extent;并在validation layers开启的条件下，validation layers会立刻捕获到有帮助的异常信息:
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // imageArrayLayers指定每个图像组成的层数。除非我们开发3D应用程序，否则始终为1。imageUsage位字段指定在交换链中对图像进行的具体操作。在本小节中，我们将直接对它们进行渲染，这意味着它们作为颜色附件。
        // 也可以首先将图像渲染为单独的图像，进行后处理操作。在这种情况下可以使用像VK_IMAGE_USAGE_TRANSFER_DST_BIT这样的值，并使用内存操作将渲染的图像传输到交换链图像队列。
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily.value(), (uint32_t)indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;// 共享模式：并发
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;// 共享模式：独占，目前初始化流程完了以后是自动的选择的这种
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // 接下来，我们需要指定如何处理跨多个队列簇的交换链图像。如果graphics队列簇与presentation队列簇不同，会出现如下情形。我们将从graphics队列中绘制交换链的图像，然后在另一个presentation队列中提交他们。多队列处理图像有两种方法:

        // VK_SHARING_MODE_EXCLUSIVE: 同一时间图像只能被一个队列簇占用，如果其他队列簇需要其所有权需要明确指定。这种方式提供了最好的性能。
        // VK_SHARING_MODE_CONCURRENT : 图像可以被多个队列簇访问，不需要明确所有权从属关系。
        // 在本小节中，如果队列簇不同，将会使用concurrent模式，避免处理图像所有权从属关系的内容，因为这些会涉及不少概念，建议后续的章节讨论。
        // Concurrent模式需要预先指定队列簇所有权从属关系，通过queueFamilyIndexCount和pQueueFamilyIndices参数进行共享。如果graphics队列簇和presentation队列簇相同，我们需要使用exclusive模式，因为concurrent模式需要至少两个不同的队列簇。
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        // 如果交换链支持(supportedTransforms in capabilities),我们可以为交换链图像指定某些转换逻辑，比如90度顺时针旋转或者水平反转。如果不需要任何transoform操作，可以简单的设置为currentTransoform。
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // 混合Alpha字段指定alpha通道是否应用与与其他的窗体系统进行混合操作。如果忽略该功能，简单的填VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR。
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        // presentMode指向自己。如果clipped成员设置为VK_TRUE，意味着我们不关心被遮蔽的像素数据，比如由于其他的窗体置于前方时或者渲染的部分内容存在于可是区域之外，除非真的需要读取这些像素获数据进行处理，否则可以开启裁剪获得最佳性能。
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // 最后一个字段oldSwapChain。Vulkan运行时，交换链可能在某些条件下被替换，比如窗口调整大小或者交换链需要重新分配更大的图像队列。在这种情况下，交换链实际上需要重新分配创建，并且必须在此字段中指定对旧的引用，用以回收资源。
        // 这是一个比较复杂的话题，我们会在后面的章节中详细介绍。现在假设我们只会创建一个交换链。

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // 我们在createSwapChain函数下面添加代码获取句柄，在vkCreateSwapchainKHR后调用。获取句柄的操作同之前获取数组集合的操作非常类似。
        // 首先通过调用vkGetSwapchainImagesKHR获取交换链中图像的数量，并根据数量设置合适的容器大小保存获取到的句柄集合。
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    }

    void createImageViews() {
        // 我们需要做的第一件事情需要定义保存图像视图集合的大小:
        swapChainImageViews.resize(swapChainImages.size());
        // 下一步，循环迭代所有的交换链图像
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            // 创建图像视图的参数被定义在VkImageViewCreateInfo结构体中。前几个参数的填充非常简单、直接
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            // 其中viewType和format字段用于描述图像数据该被如何解释。viewType参数允许将图像定义为1D textures, 2D textures,3D textures 和cube maps
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            // components字段允许调整颜色通道的最终的映射逻辑。比如，我们可以将所有颜色通道映射为红色通道，以实现单色纹理。我们也可以将通道映射具体的常量数值0和1。在章节中我们使用默认的映射策略
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // subresourceRangle字段用于描述图像的使用目标是什么，以及可以被访问的有效区域。我们的图像将会作为color targets，没有任何mipmapping levels 或是多层 multiple layers
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            // 如果在编写沉浸式的3D应用程序，比如VR，就需要创建支持多层的交换链。并且通过不同的层为每一个图像创建多个视图，以满足不同层的图像在左右眼渲染时对视图的需要
            
            // 创建图像视图调用vkCreateImageView函数
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }

            // 拥有了图像视图后，使用图像作为贴图已经足够了，但是它还没有准备好作为渲染的 target
            // 它需要更多的间接步骤去准备，其中一个就是 framebuffer，被称作帧缓冲区。但首先我们要设置图形管线
        }

    }

    // 在将代码传递给渲染管线之前，我们必须将其封装到VkShaderModule对象中。让我们创建一个辅助函数createShaderModule实现该逻辑
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        // 创建shader module是比较简单的，我们仅仅需要指定二进制码缓冲区的指针和它的具体长度。这些信息被填充在VkShaderModuleCreateInfo结构体中。
        // 需要留意的是字节码的大小是以字节指定的，但是字节码指针是一个uint32_t类型的指针，而不是一个char指针。所以我们使用reinterpret_cast进行转换指针。
        // 如下所示，当需要转换时，还需要确保数据满足uint32_t的对齐要求。幸运的是，数据存储在std::vector中，默认分配器已经确保数据满足最差情况下的对齐要求
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();

        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        // 调用vkCreateShaderMoudle创建VkShaderModule
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        // 参数与之前创建对象功能类似:逻辑设备，创建对象信息结构体的指针，自定义分配器和保存结果的句柄变量。在shader module创建完毕后，可以对二进制码的缓冲区进行立即的释放。最后不要忘记返回创建好的shader module
        return shaderModule;
    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("./sample01_vert.spv");
        auto fragShaderCode = readFile("./sample01_frag.spv");

        // shader module对象仅仅在渲染管线处理过程中需要，所以我们会在createGraphicsPipeline函数中定义本地变量保存它们，而不是定义类成员变量持有它们的句柄
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;

        // 调用加载shader module的辅助函数:
        vertShaderModule = createShaderModule(vertShaderCode);
        fragShaderModule = createShaderModule(fragShaderCode);

        // 在图形管线创建完成且createGraphicsPipeline函数返回的时候，它们应该被清理掉，所以在该函数后删除它们
        //vkDestroyShaderModule(device, fragShaderModule, nullptr);
        //vkDestroyShaderModule(device, vertShaderModule, nullptr);

        // VkShaderModule对象只是字节码缓冲区的一个包装容器。着色器并没有彼此链接，甚至没有给出目的。
        // 通过VkPipelineShaderStageCreateInfo结构将着色器模块分配到管线中的顶点或者片段着色器阶段。
        // VkPipelineShaderStageCreateInfo结构体是实际管线创建过程的一部分

        // 我们首先在createGraphicsPipeline函数中填写顶点着色器结构体
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        // 除了强制的sType成员外，第一个需要告知Vulkan将在哪个流水线阶段使用着色器。在上一个章节的每个可编程阶段都有一个对应的枚举值
        // 接下来的两个成员指定包含代码的着色器模块和调用的主函数。这意味着可以将多个片段着色器组合到单个着色器模块中，并使用不同的入口点来区分它们的行为。在这种情况下，我们坚持使用标准main函数作为入口
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        // 还有一个可选成员，pSpecializationInfo,在这里我们不会使用它，但是值得讨论一下。它允许为着色器指定常量值。
        // 使用单个着色器模块，通过为其中使用不同的常量值，可以在流水线创建时对行为进行配置。
        // 这比在渲染时使用变量配置着色器更有效率，因为编译器可以进行优化，例如消除if值判断的语句。如果没有这样的常量，可以将成员设置为nullptr，我们的struct结构体初始化自动进行

        // 修改结构体满足片段着色器的需要
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        // 完成两个结构体的创建，并通过数组保存，这部分引用将会在实际的管线创建开始
        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
    }

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        // 检查设备扩展的支持（主要包含swapchain的扩展）
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // 检查交换链的支持情况
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        // 如果swapChainAdequate条件足够，那么对应的支持的足够的，但是根据不同的模式仍然有不同的最佳选择。我们编写一组函数，通过进一步的设置查找最匹配的交换链。这里有三种类型的设置去确定:
        //    Surface格式(color depth)
        //        Presentation mode(conditions for “swapping” image to the screen)
        //        Swap extent(resolution of images in swap chain)
        //        首先在脑海中对每一个设置都有一个理想的数值，如果达成一致我们就使用，否则我们一起创建一些逻辑去找到更好的规则、数值。


        // 满足多方面的设备要求后，才能算最终的通过检测（队列簇，扩展支持，交换链支持）
        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // required里面其实就是装的swapchain的扩展
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        // 如果从查询到的available extension中找到了required的话，就erase，这样整个required的set就是empty了，也就说明查询通过了
        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    // 检测交换链的性能支持
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // 每个VkSurfaceFormatKHR结构都包含一个format和一个colorSpace成员。format成员变量指定色彩通道和类型。比如，VK_FORMAT_B8G8R8A8_UNORM代表了我们使用B,G,R和alpha次序的通道，且每一个通道为无符号8bit整数，每个像素总计32bits。colorSpace成员描述SRGB颜色空间是否通过VK_COLOR_SPACE_SRGB_NONLINEAR_KHR标志支持。需要注意的是在较早版本的规范中，这个标志名为VK_COLORSPACE_SRGB_NONLINEAR_KHR
        // 如果可以我们尽可能使用SRGB(彩色语言协议)，因为它会得到更容易感知的、精确的色彩。直接与SRGB颜色打交道是比较有挑战的，所以我们使用标准的RGB作为颜色格式，这也是通常使用的一个格式VK_FORMAT_B8G8R8A8_UNORM。
        // 最理想的情况是surface没有设置任何偏向性的格式，这个时候Vulkan会通过仅返回一个VkSurfaceFormatKHR结构表示，且该结构的format成员设置为VK_FORMAT_UNDEFINED

        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
            return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        }

        // 如果不能自由的设置格式，那么我们可以通过遍历列表设置具有偏向性的组合
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        // 如果以上两种方式都失效了，这个时候我们可以通过“优良”进行打分排序，但是大多数情况下会选择第一个格式作为理想的选择。
        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
            return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        }

        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    // presentation模式对于交换链是非常重要的，因为它代表了在屏幕呈现图像的条件。在Vulkan中有四个模式可以使用:
    // VK_PRESENT_MODE_IMMEDIATE_KHR: 应用程序提交的图像被立即传输到屏幕呈现，这种模式可能会造成撕裂效果。
    // VK_PRESENT_MODE_FIFO_KHR : 交换链被看作一个队列，当显示内容需要刷新的时候，显示设备从队列的前面获取图像，并且程序将渲染完成的图像插入队列的后面。如果队列是满的程序会等待。这种规模与视频游戏的垂直同步很类似。显示设备的刷新时刻被成为“垂直中断”。
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR : 该模式与上一个模式略有不同的地方为，如果应用程序存在延迟，即接受最后一个垂直同步信号时队列空了，将不会等待下一个垂直同步信号，而是将图像直接传送。这样做可能导致可见的撕裂效果。
    // VK_PRESENT_MODE_MAILBOX_KHR : 这是第二种模式的变种。当交换链队列满的时候，选择新的替换旧的图像，从而替代阻塞应用程序的情形。这种模式通常用来实现三重缓冲区，与标准的垂直同步双缓冲相比，它可以有效避免延迟带来的撕裂效果。
    // 逻辑上看仅仅VR_PRESENT_MODE_FIFO_KHR模式保证可用性，所以我们再次增加一个函数查找最佳的模式 :

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
        // 我个人认为三级缓冲是一个非常好的策略。它允许我们避免撕裂，同时仍然保持相对低的延迟，通过渲染尽可能新的图像，直到接受垂直同步信号。所以我们看一下列表，它是否可用:
        //for (const auto& availablePresentMode : availablePresentModes) {
        //    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        //        return availablePresentMode;
        //    }
        //}

        // 遗憾的是，一些驱动程序目前并不支持VK_PRESENT_MODE_FIFO_KHR, 除此之外如果VK_PRESENT_MODE_MAILBOX_KHR也不可用，我们更倾向使用VK_PRESENT_MODE_IMMEDIATE_KHR:
        VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
            else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                bestMode = availablePresentMode;
            }
        }

        return bestMode;
    }

    // 交换范围是指交换链图像的分辨率，它几乎总是等于我们绘制窗体的分辨率。分辨率的范围被定义在VkSurfaceCapabilitiesKHR结构体中。Vulkan告诉我们通过设置currentExtent成员的width和height来匹配窗体的分辨率。
    // 然而，一些窗体管理器允许不同的设置，意味着将currentExtent的width和height设置为特殊的数值表示:uint32_t的最大值。在这种情况下，我们参考窗体minImageExtent和maxImageExtent选择最匹配的分辨率。
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            VkExtent2D actualExtent = { WIDTH, HEIGHT };

            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        // 查找物理设备的队列簇
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // 检测得到的队列中，是否支持图形队列，其他常用队列还有compute和transfer，但是我们在3d绘制程序中，肯定是需要优先支持图形（Graphics）的
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        // 这里调用了glfw的接口，内嵌处理了下平台相关的扩展查询，其实这里也可以自己查询的，后面找到相关内容来这里补充一下
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        // 这里根据是否开启验证层，决定是否添加debug utils的扩展，说明这个extensions是人为控制的，一般可能和平台相关和debug相关的多一些
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
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

    // 调试回调函数，需要提前设置开启，就可以接收到vk的各种层级的调试信息了（verbose，debug，warning，error等）
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};

int main() {
    // 对vulkan的处理过程进行了封装，保持main函数中的主体循环看起来尽量的简洁易懂
    // 本例结合了glfw的窗口创建（这些窗口其实是和vulkan无关的系统操作相关）
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
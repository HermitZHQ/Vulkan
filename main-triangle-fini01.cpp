// 目前这是我上传的第一版vulkan代码，连三角形还画不出来，目前主要用来初始化仓库
// 要点记录，这里的vulkan封装是基于glfw的，可以到官方下载，另外还需要的三方库有glm
// 这些三方库都统一放到VulkanSDK/1.xxx.xxxx目录下的Third-Party目录中（新建目录）
// 子目录新建Bin32，Bin64（32和64主要用于区分版本，目前我只创建了Bin64用于存放lib和dll）和Include

// 这里没有显示的包含vk的头文件，因为我们使用了glfw的框架，但是要让vk生效的话，必须在包含glfw的头文件前，声明GLFW_INCLUDE_VULKAN的宏定义，这样才能正常使用

// 定义这个宏，只是为了防止打开代码，因为main函数会重复，我只是另外新建了一个cpp来记录完成triangle的第一个vk版本
#ifdef NEVER_OPEN_THIS_MACRO

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

    // 可以在着色器中使用uniform，它是类似与动态状态变量的全局变量，可以在绘画时修改，可以更改着色器的行为而无需重新创建它们。它们通常用于将变换矩阵传递到顶点着色器或者在片段着色器冲创建纹理采样器。
    // 这些uniform数值需要在管线创建过程中，通过VkPipelineLayout对象指定。即使在后续内容中用到，我们也仍然需要创建一个空的pipeline layout
    VkPipelineLayout pipelineLayout;

    VkSwapchainKHR swapChain;// 现在添加一个类成员变量存储VkSwapchainKHR对象:
    // 交换链创建后，需要获取VkImage相关的句柄。它会在后续渲染的章节中引用。添加类成员变量存储该句柄:
    std::vector<VkImage> swapChainImages;// 图像被交换链创建，也会在交换链销毁的同时自动清理，所以我们不需要添加任何清理代码。
    // Vulkan 图像与视图，使用任何的VkImage，包括在交换链或者渲染管线中的，我们都需要创建VkImageView对象。
    // 从字面上理解它就是一个针对图像的视图或容器，通过它具体的渲染管线才能够读写渲染数据，换句话说VkImage不能与渲染管线进行交互
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapChainExtent = { WIDTH, HEIGHT };// 这里需要设置，教程里面没有地方进行设置，就是0，0

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

    // 在render pass创建阶段我们指定了具体的附件，并通过VkFramebuffer对象包装绑定。帧缓冲区对象引用表示为附件的所有的VkImageView对象。
    // 在我们的例子中只会使用一个帧缓冲区:color attachment。然而我们作为附件的图像依赖交换链用于呈现时返回的图像。这意味着我们必须为交换链中的所有图像创建一个帧缓冲区，并在绘制的时候使用对应的图像
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkPipeline graphicsPipeline;
    // Vulkan 渲染通道，在我们完成管线的创建工作之前，我们需要告诉Vulkan渲染时候使用的framebuffer帧缓冲区附件相关信息。
    // 我们需要指定多少个颜色和深度缓冲区将会被使用，指定多少个采样器被用到及在整个渲染操作中相关的内容如何处理。
    // 所有的这些信息都被封装在一个叫做 render pass 的对象中
    VkRenderPass renderPass;

    VkCommandPool commandPool;
    // 现在我们开始分配命令缓冲区并通过它们记录绘制指令。因为其中一个绘图命令需要正确绑定VkFrameBuffer，我们实际上需要为每一个交换链中的图像记录一个命令缓冲区。
    // 最后创建一个VkCommandBuffer对象列表作为成员变量。命令缓冲区会在common pool销毁的时候自动释放系统资源，所以我们不需要明确编写cleanup逻辑
    std::vector<VkCommandBuffer> commandBuffers;

    // 信号量
    // 在获得一个图像时，我们需要发出一个信号量准备进行渲染，另一个信号量的发出用于渲染结束，准备进行呈现presentation。创建两个成员变量存储信号量对象 :
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

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
        // 创建渲染通道
        createRenderPass();
        // 创建图形管线
        createGraphicsPipeline();
        // 创建帧缓冲
        createFramebuffers();
        // 创建命令缓冲
        createCommandPool();
        // 现在开始使用一个createCommandBuffers函数来分配和记录每一个交换链图像将要应用的命令
        createCommandBuffers();
        // 为了创建信号量semaphores，我们将要新增本系列教程最后一个函数: createSemaphores:
        createSemaphores();

        submitDrawCommands();
    }

    void submitDrawCommands() {

        // flags标志位参数用于指定如何使用命令缓冲区。可选的参数类型如下:
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: 命令缓冲区将在执行一次后立即重新记录。
        // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT : 这是一个辅助缓冲区，它限制在在一个渲染通道中。
        // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : 命令缓冲区也可以重新提交，同时它也在等待执行。
        // 我们使用了最后一个标志，因为我们可能已经在下一帧的时候安排了绘制命令，而最后一帧尚未完成。pInheritanceInfo参数与辅助缓冲区相关。它指定从主命令缓冲区继承的状态
        for (size_t i = 0; i < commandBuffers.size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            beginInfo.pInheritanceInfo = nullptr; // Optional

            vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

            // 启动渲染通道--------
            // 绘制开始于调用vkCmdBeginRenderPass开启渲染通道。render pass使用VkRenderPassBeginInfo结构体填充配置信息作为调用时使用的参数
            // 结构体第一个参数传递为绑定到对应附件的渲染通道本身。我们为每一个交换链的图像创建帧缓冲区，并指定为颜色附件
            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[i];

            // 后两个参数定义了渲染区域的大小。渲染区域定义着色器加载和存储将要发生的位置。区域外的像素将具有未定的值。为了最佳的性能它的尺寸应该与附件匹配
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = swapChainExtent;

            // 最后两个参数定义了用于VK_ATTACHMENT_LOAD_OP_CLEAR的清除值，我们将其用作颜色附件的加载操作。为了简化操作，我们定义了clear color为100%黑色
            VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            // 渲染通道现在可以启用。所有可以被记录的命令，被识别的前提是使用vkCmd前缀。它们全部返回void，所以在结束记录之前不会有任何错误处理。
            // 对于每个命令，第一个参数总是记录该命令的命令缓冲区。
            // 第二个参数指定我们传递的渲染通道的具体信息。最后的参数控制如何提供render pass将要应用的绘制命令。它使用以下数值任意一个 :
            // VK_SUBPASS_CONTENTS_INLINE: 渲染过程命令被嵌入在主命令缓冲区中，没有辅助缓冲区执行。
            // VK_SUBPASS_CONTENTS_SECONDARY_COOMAND_BUFFERS : 渲染通道命令将会从辅助命令缓冲区执行。
            // 我们不会使用辅助命令缓冲区，所以我们选择第一个。
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // 第二个参数指定具体管线类型，graphics or compute pipeline。
            // 我们告诉Vulkan在图形管线中每一个操作如何执行及哪个附件将会在片段着色器中使用，所以剩下的就是告诉它绘制三角形
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            // 实际的vkCmdDraw函数有点与字面意思不一致，它是如此简单，仅因为我们提前指定所有渲染相关的信息。它有如下的参数需要指定，除了命令缓冲区:
            // vertexCount: 即使我们没有顶点缓冲区，但是我们仍然有3个定点需要绘制。
            // instanceCount: 用于instanced 渲染，如果没有使用请填1。
            // firstVertex : 作为顶点缓冲区的偏移量，定义gl_VertexIndex的最小值。
            // firstInstance : 作为instanced 渲染的偏移量，定义了gl_InstanceIndex的最小值。
            vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
            // render pass执行完绘制，可以结束渲染作业
            vkCmdEndRenderPass(commandBuffers[i]);
            // 并停止记录命令缓冲区的工作:
            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void drawFrame() {
        // 同步交换链事件有两种方法:栅栏和信号量。它们都是可以通过使用一个操作信号，负责协调操作的对象。另一个操作等待栅栏或者信号量从无信号状态转变到有信号状态
        // 不同之处在于可以在应用程序中调用vkWaitForFence进入栅栏状态，而信号量不可以。
        // 栅栏主要用于应用程序自身与渲染操作进行同步，而信号量用于在命令队列内或者跨命令队列同步操作。我们期望同步绘制与呈现的队列操作，所以使用信号量最合适

        // 就像之前说到的，drawFrame函数需要做的第一件事情就是从交换链中获取图像。回想一下交换链是一个扩展功能，所以我们必须使用具有vk* KHR命名约定的函数:
        // vkAcquireNextImageKHR函数前两个参数是我们希望获取到图像的逻辑设备和交换链。第三个参数指定获取有效图像的操作timeout，单位纳秒。我们使用64位无符号最大值禁止timeout
        // 
        // 接下来的两个参数指定使用的同步对象，当presentation引擎完成了图像的呈现后会使用该对象发起信号。这就是开始绘制的时间点。它可以指定一个信号量semaphore或者栅栏或者两者。出于目的性，我们会使用imageAvailableSemaphore
        // 最后的参数指定交换链中成为available状态的图像对应的索引。其中索引会引用交换链图像数组swapChainImages的图像VkImage。我们使用这个索引选择正确的命令缓冲区
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        // 提交命令缓冲区
        // 队列提交和同步通过VkSubmitInfo结构体进行参数配置。
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // 前三个参数指定在执行开始之前要等待的哪个信号量及要等待的通道的哪个阶段。
        // 为了向图像写入颜色，我们会等待图像状态变为available，所我们指定写入颜色附件的图形管线阶段。
        // 理论上这意味着，具体的顶点着色器开始执行，而图像不可用。waitStages数组对应pWaitSemaphores中具有相同索引的信号量
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        // 接下来的两个参数指定哪个命令缓冲区被实际提交执行。如初期提到的，我们应该提交命令缓冲区，它将我们刚获取的交换链图像做为颜色附件进行绑定。
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        // ------结束信号
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        // signalSemaphoreCount和pSignalSemaphores参数指定了当命令缓冲区执行结束向哪些信号量发出信号。根据我们的需要使用renderFinishedSemaphore
        // 使用vkQueueSubmit函数向图像队列提交命令缓冲区。当开销负载比较大的时候，处于效率考虑，函数可以持有VkSubmitInfo结构体数组。
        // 最后一个参数引用了一个可选的栅栏，当命令缓冲区执行完毕时候它会被发送信号。我们使用信号量进行同步，所以我们需要传递VK_NULL_HANDLE
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // ------呈现
        // 绘制帧最后一个步骤是将结果提交到交换链，使其最终显示在屏幕上。Presentation通过VkPresentInfoKHR结构体配置，具体位置在drawFrame函数最后。
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        // 前两个参数指定在进行presentation之前要等待的信号量，就像VkSubmitInfo一样。
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        // 接下来的两个参数指定用于提交图像的交换链和每个交换链图像索引。大多数情况下仅一个。
        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        // 最后一个可选参数pResults，它允许指定一组VkResult值，以便在presentation成功时检查每个独立的交换链。如果只使用单个交换链，则不需要，因为可以简单的使用当前函数的返回值
        presentInfo.pResults = nullptr; // Optional
        // vkQueuePresentKHR函数提交请求呈现交换链中的图像。我们在下一个章节为vkAcquireNextImageKHR和vkQueuePresentKHR可以添加错误处理。
        // 因为它们失败并不一定意味着程序应该终止，与我们迄今为止看到的功能不同。
        // 如果一切顺利，当再次运行程序时候，应该可以看到一下内容（也就是三角形绘制出来了）
        // 当时最后画面出来前，我还调试了很久，光看validation的信息其实不够，有些虽然有error，但是并不影响显示，首先我们可以调试的是clearcolor有没有生效，有的话
        // 第二步才是三角形，三角形当时因为没有修改shader，所以绘制是黑色，和clearcolor一样，我就以为出问题了，结果都已经画上了
        // 这下大概对vk的绘制精华有一定总结了，主要的绘制还是在submitDrawCommands和drawFrame里面，涉及到绘制信号同步，已经绘制命令的发送方式！！
        vkQueuePresentKHR(presentQueue, &presentInfo);

        // 如果运行时启用了validation layers并监视应用程序的内存使用情况，你会发现它在慢慢增加。原因是validation layers的实现期望与GPU同步。
        // 虽然在技术上是不需要的，但是一旦这样做，每一针帧不会出现明显的性能影响。
        // 我们可以在开始绘制下一帧之前明确的等待presentation完成:
        vkQueueWaitIdle(presentQueue);
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // drawFrame函数将会执行如下操作:
            // 从交换链中获取一个图像
            // 在帧缓冲区中，使用作为附件的图像来执行命令缓冲区中的命令
            // 为了最终呈现，将图像返还到交换链
            // 每个事件派发都有一个函数调用来对应，但它们的执行是异步的。函数调用将在操作实际完成之前返回，并且执行顺序也是未定义的。这是不理想的，因为每一个操作都取决于前一个操作
            drawFrame();

            // 在很多应用程序的的状态也会在每一帧更新。为此更高效的绘制一阵的方式如下：
            //void drawFrame() {
            //    updateAppState();

            //    vkQueueWaitIdle(presentQueue);    vkAcquireNextImageKHR(...)

            //        submitDrawCommands();

            //    vkQueuePresentKHR(presentQueue, &presentInfo);
            //}
            // 该方法允许我们更新应用程序的状态，比如运行游戏的AI协同程序，而前一帧被渲染。这样，始终保持CPU和GPU处于工作状态。
        }

        // 需要了解的是drawFrame函数中所有的操作都是异步的。意味着当程序退出mainLoop，绘制和呈现操作可能仍然在执行。所以清理该部分的资源是不友好的。
        // 为了解决这个问题，我们应该在退出mainLoop销毁窗体前等待逻辑设备的操作完成
        vkDeviceWaitIdle(device);
        // 也可以使用vkQueueWaitIdle等待特定命令队列中的操作完成。这些功能可以作为一个非常基本的方式来执行同步。这个时候窗体关闭后该问题不会出现
    }

    void cleanup() {
        // 与图像不同的是，图像视图需要明确的创建过程，所以在程序退出的时候，我们需要添加一个循环去销毁他们
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }

        // 删除帧缓冲区
        for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
        }
        // 删除图形管线
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        // 删除管线布局
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        // 删除渲染通道
        vkDestroyRenderPass(device, renderPass, nullptr);

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroySwapchainKHR(device, swapChain, nullptr);

        // 删除命令池
        vkDestroyCommandPool(device, commandPool, nullptr);

        // 删除信号量
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

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

    void createRenderPass() {
        // 在我们的例子中，我们将只有一个颜色缓冲区附件，它由交换链中的一个图像所表示
        // format是颜色附件的格式，它应该与交换链中图像的格式相匹配，同时我们不会做任何多重采样的工作，所以采样器设置为1
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        // loadOp和storeOp决定了渲染前和渲染后数据在对应附件的操作行为。对于 loadOp 我们有如下选项
        // VK_ATTACHMENT_LOAD_OP_LOAD: 保存已经存在于当前附件的内容
        // VK_ATTACHMENT_LOAD_OP_CLEAR: 起始阶段以一个常量清理附件内容
        // VK_ATTACHMENT_LOAD_OP_DONT_CARE : 存在的内容未定义，忽略它们
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // 在绘制新的一帧内容之前，我们要做的是使用清理操作来清理帧缓冲区framebuffer为黑色。同时对于 storeOp 仅有两个选项
        // VK_ATTACHMENT_STORE_OP_STORE: 渲染的内容会存储在内存，并在之后进行读取操作
        // VK_ATTACHMENT_STORE_OP_DONT_CARE : 帧缓冲区的内容在渲染操作完毕后设置为undefined
        // 我们要做的是渲染一个三角形在屏幕上，所以我们选择存储操作
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        // loadOp和storeOp应用在颜色和深度数据，同时stencilLoadOp / stencilStoreOp应用在模版数据。
        // 我们的应用程序不会做任何模版缓冲区的操作，所以它的loading和storing无关紧要
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        // 纹理和帧缓冲区在Vulkan中通常用VkImage 对象配以某种像素格式来代表。但是像素在内存中的布局可以基于需要对image图像进行的操作发生内存布局的变化
        // 一些常用的布局:
        // VK_IMAGE_LAYOUT_COLOR_ATTACHMET_OPTIMAL: 图像作为颜色附件
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : 图像在交换链中被呈现
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : 图像作为目标，用于内存COPY操作
        // 我们会深入讨论这些内容在纹理章节，现在最重要的是为需要转变的图像指定合适的layout布局进行操作
        // initialLayout指定图像在开始进入渲染通道render pass前将要使用的布局结构。finalLayout指定当渲染通道结束自动变换时使用的布局。
        // 使用VK_IMAGE_LAYOUT_UNDEFINED设置initialLayout，意为不关心图像之前的布局。特殊值表明图像的内容不确定会被保留，但是这并不总要，因为无论如何我们都要清理它。
        // 我们希望图像渲染完毕后使用交换链进行呈现，这就解释了为什么finalLayout要设置为VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        //colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // 子通道和附件引用
        // 一个单独的渲染通道可以由多个子通道组成。子通道是渲染操作的一个序列。子通道作用与后续的渲染操作，并依赖之前渲染通道输出到帧缓冲区的内容。
        // 比如说后处理效果的序列通常每一步都依赖之前的操作。
        // 如果将这些渲染操作分组到一个渲染通道中，通过Vulkan将通道中的渲染操作进行重排序，可以节省内存从而获得更好的性能。对于我们要绘制的三角形，我们只需要一个子通道
        // 每个子通道引用一个或者多个之前使用结构体描述的附件。这些引用本身就是VkAttachmentReference结构体

        // attachment附件参数通过附件描述符集合中的索引来持有。我们的集合是由一个VkAttachmentDesription组成的，所以它的索引为0。layout为附件指定子通道在持有引用时候的layout。
        // 当子通道开始的时候Vulkan会自动转变附件到这个layout。因为我们期望附件起到颜色缓冲区的作用，layout设置为VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL会给我们最好的性能
        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // 子通道使用VkSubpassDescription结构体描述
        VkSubpassDescription subpass = {};
        //  Vulkan在未来可能会支持关于compute subpasses的功能，所以在这里我们明确指定graphics subpass图形子通道。下一步为它指定颜色附件的引用
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        // 附件在数组中的索引直接从片段着色器引用，其layout(location = 0) out vec4 outColor 指令
        // 可以被子通道引用的附件类型如下:
        // pInputAttachments: 附件从着色器中读取
        // pResolveAttachments : 附件用于颜色附件的多重采样
        // pDepthStencilAttachment : 附件用于深度和模版数据
        // pPreserveAttachments : 附件不被子通道使用，但是数据被保存

        // 渲染通道对象创建通过填充VkRenderPassCreateInfo结构体，并配合相关附件和子通道来完成。VkAttachmentReference对象引用附件数组
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        // Subpass 依赖性
        // 请记住，渲染通道中的子通道会自动处理布局的变换。这些变换通过子通道的依赖关系进行控制，它们指定了彼此之间内存和执行的依赖关系。现在只有一个子通道，但是在此子通道之前和之后的操作也被视为隐式“子通道”。
        // 有两个内置的依赖关系在渲染通道开始和渲染通道结束处理转换，但是前者不会在当下发生。
        // 假设转换发生在管线的起始阶段，但是我们还没有获取图像！有两个方法处理这个问题可以将imageAvailableSemaphore的waitStages更改为VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT，确保图像有效之前渲染通道不会开始
        // 或者我们让渲染通道等待VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT阶段。我觉得使用第二个选项，因为可以比较全面的了解subpass依赖关系及其工作方式。
        // 子通道依赖关系可以通过VkSubpassDependency结构体指定，在createRenderPass函数中添加 :

        // 前两个参数指定依赖的关系和从属子通道的索引。特殊值VK_SUBPASS_EXTERNAL是指在渲染通道之前或者之后的隐式子通道，取决于它是否在srcSubpass或者dstSubPass中指定。
        // 索引0指定我们的子通道，这是第一个也是唯一的。dstSubpass必须始终高于srcSubPass以防止依赖关系出现循环
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        // 接下来的两个参数字段指定要等待的操作和这些操作发生的阶段。在我们可以访问对象之前，我们需要等待交换链完成对应图像的读取操作。这可以通过等待颜色附件输出的阶段来实现。
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        // 在颜色附件阶段的操作及涉及颜色附件的读取和写入的操作应该等待。这些设置将阻止转换发生，直到实际需要(并允许):当我们需要写入颜色时候。
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        // VkRenderPassCreateInfo结构体有两个字段指定依赖的数组。
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
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

        // 固有管线包含以下设置------
        // Vulkan 固有功能，早起的图形API在图形渲染管线的许多阶段提供了默认的状态。在Vulkan中，从viewport的大小到混色函数，需要凡事做到亲历亲为
        // 1.顶点输入
        // 2.输入组件
        // 3.视口和剪裁
        // 4.光栅化
        // 5.重采样
        // 6.深度和模版测试
        // 7.颜色混合
        // 8.动态修改
        // 9.管道布局

        // 顶点输入------
        // VkPipelineVertexInputStateCreateInfo结构体描述了顶点数据的格式，该结构体数据传递到vertex shader中。它以两种方式进行描述 :
        // Bindings:根据数据的间隙，确定数据是每个顶点或者是每个instance(instancing)
        // Attribute 描述 : 描述将要进行绑定及加载属性的顶点着色器中的相关属性类型。
        // 因为我们将顶点数据硬编码到vertex shader中，所以我们将要填充的结构体没有顶点数据去加载。我们将会在vertex buffer章节中回来操作
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional
        // pVertexBindingDescriptions和pVertexAttributeDescriptions成员指向结构体数组，用于进一步描述加载的顶点数据信息。在createGraphicsPipeline函数中的shaderStages数组后添加该结构体????

        // 输入组件------
        // VkPipelineInputAssemblyStateCreateInfo结构体描述两件事情:顶点数据以什么类型的几何图元拓扑进行绘制及是否启用顶点索重新开始图元。图元的拓扑结构类型topology枚举值如下
        // VK_PRIMITIVE_TOPOLOGY_POINT_LIST: 顶点到点
        // VK_PRIMITIVE_TOPOLOGY_LINE_LIST : 两点成线，顶点不共用
        // VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : 两点成线，每个线段的结束顶点作为下一个线段的开始顶点
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : 三点成面，顶点不共用
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : 每个三角形的第二个、第三个顶点都作为下一个三角形的前两个顶点

        // 正常情况下，顶点数据按照缓冲区中的序列作为索引，但是也可以通过element buffer缓冲区自行指定顶点数据的索引。通过复用顶点数据提升性能。
        // 如果设置primitiveRestartEnable成员为VK_TRUE，可以通过0xFFFF或者0xFFFFFFFF作为特殊索引来分解线和三角形在_STRIP模式下的图元拓扑结构（这个应该是个什么特殊用法，感觉暂时不用太关注）

        // 通过本教程绘制三角形，所以我们坚持按照如下格式填充数据结构
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // 视口和裁剪------
        // Viewport用于描述framebuffer作为渲染输出结果目标区域。它的数值在本教程中总是设置在(0, 0)和(width, height)
        // 记得交换链和它的images图像大小WIDTH和HEIGHT会根据不同的窗体而不同。交换链图像将会在帧缓冲区framebuffers使用，所以我们应该坚持它们的大小
        // minDepth和maxDepth数值指定framebuffer中深度的范围。这些数值必须收敛在[0.0f, 1.0f]区间冲，但是minDepth可能会大于maxDepth。如果你不做任何指定，建议使用标准的数值0.0f和1.0f
        // viewports定义了image图像到framebuffer帧缓冲区的转换关系，裁剪矩形定义了哪些区域的像素被存储。任何在裁剪巨型外的像素都会在光栅化阶段丢弃
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        // 在本教程中我们需要将图像绘制到完整的帧缓冲区framebuffer中，所以我们定义裁剪矩形覆盖到整体图像
        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;
        // viewport和裁剪矩形需要借助VkPipelineViewportStateCreateInfo结构体联合使用。可以使用多viewports和裁剪矩形在一些图形卡，通过数组引用。使用该特性需要GPU支持该功能，具体看逻辑设备的创建
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // 光栅化------
        // 光栅化通过顶点着色器及具体的几何算法将顶点进行塑形，并将图形传递到片段着色器进行着色工作。
        // 它也会执行深度测试depth testing、面裁切face culling和裁剪测试，它可以对输出的片元进行配置，决定是否输出整个图元拓扑或者是边框(线框渲染)。
        // 所有的配置通过VkPipelineRasterizationStateCreateInfo结构体定义
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // 它的depthClampEnable设置为VK_TRUE，超过远近裁剪面的片元会进行收敛，而不是丢弃它们。它在特殊的情况下比较有用，像阴影贴图。使用该功能需要得到GPU的支持
        rasterizer.depthClampEnable = VK_FALSE;
        // 如果rasterizerDiscardEnable设置为VK_TRUE，那么几何图元永远不会传递到光栅化阶段。这是基本的禁止任何输出到framebuffer帧缓冲区的方法
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        // polygonMode决定几何产生图片的内容。下列有效模式:
        // VK_POLYGON_MODE_FILL: 多边形区域填充
        // VK_POLYGON_MODE_LINE : 多边形边缘线框绘制
        // VK_POLYGON_MODE_POINT : 多边形顶点作为描点绘制
        // 使用任何模式填充需要开启GPU功能
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // lineWidth成员是直接填充的，根据片元的数量描述线的宽度。最大的线宽支持取决于硬件，任何大于1.0的线宽需要开启GPU的wideLines特性支持
        rasterizer.lineWidth = 1.0f;
        // cullMode变量用于决定面裁剪的类型方式。可以禁止culling，裁剪front faces，cull back faces 或者全部。
        // frontFace用于描述作为front-facing面的顶点的顺序，可以是顺时针也可以是逆时针
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // 光栅化可以通过添加常量或者基于片元的斜率来更改深度值。一些时候对于阴影贴图是有用的，但是我们不会在章节中使用，设置depthBiasEnable为VK_FALSE
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // 重采样------
        // VkPipelineMultisampleStateCreateInfo结构体用于配置多重采样。所谓多重采样是抗锯齿anti-aliasing的一种实现。
        // 它通过组合多个多边形的片段着色器结果，光栅化到同一个像素。这主要发生在边缘，这也是最引人注目的锯齿出现的地方。
        // 如果只有一个多边形映射到像素是不需要多次运行片段着色器进行采样的，相比高分辨率来说，它会花费较低的开销。开启该功能需要GPU支持
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional
        // 在本教程中我们不会使用多重采样，但是可以随意的尝试，具体的参数请参阅规范

        // 深度和模板测试------
        // 如果使用depth 或者 stencil缓冲区，需要使用VkPipelineDepthStencilStateCreateInfo配置。我们现在不需要使用，所以简单的传递nullptr，关于这部分会专门在深度缓冲区章节中讨论
        // 下面的参数是我自己尝试性添加的
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.pNext = nullptr;

        // 颜色混合------
        // 片段着色器输出具体的颜色，它需要与帧缓冲区framebuffer中已经存在的颜色进行混合。这个转换的过程成为混色，它有两种方式:

        // 将old和new颜色进行混合产出一个最终的颜色
        // 使用按位操作混合old和new颜色的值
        // 有两个结构体用于配置颜色混合。第一个结构体VkPipelineColorBlendAttachmentState包括了每个附加到帧缓冲区的配置。第二个结构体VkPipelineColorBlendStateCreateInfo包含了全局混色的设置。在我们的例子中仅使用第一种方式
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        /*这种针对每个帧缓冲区配置混色的方式，使用如下伪代码进行说明:
        if (blendEnable) {
            finalColor.rgb = (srcColorBlendFactor * newColor.rgb) < colorBlendOp > (dstColorBlendFactor * oldColor.rgb);
            finalColor.a = (srcAlphaBlendFactor * newColor.a) < alphaBlendOp > (dstAlphaBlendFactor * oldColor.a);
        }
        else {
            finalColor = newColor;
        }

        finalColor = finalColor & colorWriteMask;
        如果blendEnable设置为VK_FALSE, 那么从片段着色器输出的新颜色不会发生变化，否则两个混色操作会计算新的颜色。所得到的结果与colorWriteMask进行AND运算，以确定实际传递的通道。

            大多数的情况下使用混色用于实现alpha blending，新的颜色与旧的颜色进行混合会基于它们的opacity透明通道。finalColor作为最终的输出 :

        finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
        finalColor.a = newAlpha.a;
        可以通过一下参数完成:

        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        可以在规范中找到所有有关VkBlendFactor和VkBlendOp的枚举值。

            第二个结构体持有所有帧缓冲区的引用，它允许设置混合操作的常量，该常量可以作为后续计算的混合因子 :

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional
        如果需要使用第二种方式设置混合操作(bitwise combination), 需要设置logicOpEnable为VK_TURE。二进制位操作在logicOp字段中指定。在第一种方式中会自动禁止，等同于为每一个附加的帧缓冲区framebuffer关闭混合操作，blendEnable为VK_FALSE。colorWriteMask掩码会用确定帧缓冲区中具体哪个通道的颜色受到影响。它也可以在两种方式下禁止，截至目前，片段缓冲区向帧缓冲区中输出的颜色不会进行任何变化
        */
        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // 动态修改------
        // 之前创建的一些结构体的状态可以在运行时动态修改，而不必重新创建。比如viewport的大小, line width和blend constants。如果需要进行这样的操作，需要填充VkPipelineDynamicStateCreateInfo结构体
        // 下面是示例代码，我这里是不需要马上进行动态修改的，所以先注释掉
        /*
        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH
        };

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;
        */

        // 管道布局------
        // 该结构体还指定了push常量，这是将动态值传递给着色器的另一个方式。pipeline layout可以在整个程序的生命周期内引用，所以它在程序退出的时候进行销毁
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // 然而，在我们可以最终创建图形管线之前，还有一个对象需要创建，它就是render pass------
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        // 现在开始引用之前的VkPipelineShaderStageCreateInfo结构体数组
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; // Optional

        pipelineInfo.layout = pipelineLayout;

        // 最后我们需要引用render pass和图形管线将要使用的子通道sub pass的索引
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        // 实际上还有两个参数:basePipelineHandle 和 basePipelineIndex。Vulkan允许您通过已经存在的管线创建新的图形管线。
        // 这种衍生出新管线的想法在于，当要创建的管线与现有管道功能相同时，获得较低的开销，同时也可以更快的完成管线切换，当它们来自同一个父管线。
        // 可以通过basePipelineHandle指定现有管线的句柄，也可以引用由basePipelineIndex所以创建的另一个管线。目前只有一个管线，所以我们只需要指定一个空句柄和一个无效的索引。
        // 只有在VkGraphicsPipelineCreateInfo的flags字段中也指定了VK_PIPELINE_CREATE_DERIVATIVE_BIT标志时，才需要使用这些值
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        // vkCreateGraphicsPipelines函数在Vulkan中比起一般的创建对象函数需要更多的参数。它可以用来传递多个VkGraphicsPipelineCreateInfo对象并创建多个VkPipeline对象。
        // 我们传递VK_NULL_HANDLE参数作为第二个参数，作为可选VkPipelineCache对象的引用。
        // 管线缓存可以用于存储和复用与通过多次调用vkCreateGraphicsPipelines函数相关的数据，甚至在程序执行的时候缓存到一个文件中。这样可以加速后续的管线创建逻辑。具体的内容我们会在管线缓存章节介绍
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        // 现在运行程序，确认所有工作正常，并创建图形管线成功！我们已经无比接近在屏幕上绘制出东西来了。在接下来的几个章节中，我们将从交换链图像中设置实际的帧缓冲区，并准备绘制命令
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        // 我们接下来迭代左右的图像视图并通过它们创建对应的framebuffers
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };

            // 如你所见，创建framebuffers是非常直接的。首先需要指定framebuffer需要兼容的renderPass。我们只能使用与其兼容的渲染通道的帧缓冲区，这大体上意味着它们使用相同的附件数量和类型
            // attachmentCount和pAttachments参数指定在渲染通道的pAttachment数组中绑定到相应的附件描述的VkImageView对象。
            // width和height参数是容易理解的，layer是指定图像数组中的层数。我们的交换链图像是单个图像，因此层数为1
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        // 有两个标志位用于command pools:
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: 提示命令缓冲区非常频繁的重新记录新命令(可能会改变内存分配行为)
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 允许命令缓冲区单独重新记录，没有这个标志，所有的命令缓冲区都必须一起重置
        // 我们仅仅在程序开始的时候记录命令缓冲区，并在主循环体main loop中多次执行，因此我们不会使用这些标志。

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        poolInfo.flags = 0; // Optional
        // 命令缓冲区通过将其提交到其中一个设备队列上来执行，如我们检索的graphics和presentation队列。
        // 每个命令对象池只能分配在单一类型的队列上提交的命令缓冲区，换句话说要分配的命令需要与队列类型一致。我们要记录绘制的命令，这就说明为什么要选择图形队列簇的原因

        // 通过vkCreateCommandPool函数完成command pool创建工作。它不需要任何特殊的参数设置。命令将被整个程序的生命周期使用以完成屏幕的绘制工作，所以对象池应该被在最后销毁:
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size());

        // level参数指定分配的命令缓冲区的主从关系。
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY: 可以提交到队列执行，但不能从其他的命令缓冲区调用。
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY : 无法直接提交，但是可以从主命令缓冲区调用。
        // 我们不会在这里使用辅助缓冲区功能，但是可以想像，对于复用主缓冲区的常用操作很有帮助
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void createSemaphores() {
        // 创建信号量对象需要填充VkSemaphoreCreateInfo结构体，但是在当前版本的API中，实际上不需要填充任何字段，除sType:
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // Vulkan API未来版本或者扩展中或许会为flags和pNext参数增加功能选项。创建信号量对象的过程很熟悉了，在这里使用vkCreateSemaphore:
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {

            throw std::runtime_error("failed to create semaphores!");
        }

        // 在程序结束时，当所有命令完成并不需要同步时，应该清除信号量，在cleanup中执行
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
        for (const auto& extension : availableExtensions) {
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
            if (queueFamily.queueCount > 0 && presentSupport
                && indices.graphicsFamily != i/*这里是我自己加的，突然冒出来个不是unique的错误，没太明白*/)
            {
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

#endif
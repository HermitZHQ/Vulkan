// Ŀǰ�������ϴ��ĵ�һ��vulkan���룬�������λ�����������Ŀǰ��Ҫ������ʼ���ֿ�
// Ҫ���¼�������vulkan��װ�ǻ���glfw�ģ����Ե��ٷ����أ����⻹��Ҫ����������glm
// ��Щ�����ⶼͳһ�ŵ�VulkanSDK/1.xxx.xxxxĿ¼�µ�Third-PartyĿ¼�У��½�Ŀ¼��
// ��Ŀ¼�½�Bin32��Bin64��32��64��Ҫ�������ְ汾��Ŀǰ��ֻ������Bin64���ڴ��lib��dll����Include

// ����û����ʾ�İ���vk��ͷ�ļ�����Ϊ����ʹ����glfw�Ŀ�ܣ�����Ҫ��vk��Ч�Ļ��������ڰ���glfw��ͷ�ļ�ǰ������GLFW_INCLUDE_VULKAN�ĺ궨�壬������������ʹ��

// ��������ֻ꣬��Ϊ�˷�ֹ�򿪴��룬��Ϊmain�������ظ�����ֻ�������½���һ��cpp����¼���triangle�ĵ�һ��vk�汾
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

// ����vk pipeline��һЩ�ص�֪ʶ��
// ���֮ǰʹ�ù��ɵ�API(OpenGL��Direct3D),��ô����������ͨ��glBlendFunc��OMSetBlendState���ø��Ĺ������á�Vulkan�е�ͼ�ι��߼������ɸı䣬��������Ҫ������ɫ�����󶨵���ͬ��֡���������߸��Ļ�Ϻ�����������ͷ�������ߡ�
// ȱ���Ǳ��봴��һЩ���ߣ���Щ���ߴ�������Ⱦ������ʹ�õĲ�ͬ�����״̬�������������й��ߵĲ���������ǰ֪���ģ����Կ���ͨ������������õ��Ż���

// KHR vk��Ĭ�ϵ��Բ㣬Ӧ���ǿ��������еĵ��Ե�
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

    // readFile����������ļ��ж�ȡ���еĶ��������ݣ�����std::vector�ֽڼ��Ϲ�������ʹ��������־���Դ��ļ�:
    // ate:���ļ�ĩβ��ʼ��ȡ
    // binary : �Զ����Ƹ�ʽȥ���ļ�(�����ַ���ʽ��ת��)
    // ���ļ�ĩβ��ʼ��ȡ���ŵ������ǿ���ʹ�ö�ȡλ����ȷ���ļ��Ĵ�С�����仺����
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    // ֮�����ǿ���׷�ݵ��ļ��Ŀ�ͷ��ͬʱ��ȡ���е��ֽ�
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    // ���ر��ļ��������ֽ�����
    file.close();
    return buffer;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    // vk�еĺܶຯ�������ǿ���ֱ�ӵ��õģ���Ҫ��ͨ��vkGetInstanceProcAddr����ȡ�������󣬲���ʹ��
    // ��vkGetInstancePorcAddr�Ǵ�vk��dll�м��س����ĺ�����vulkan-1.dll
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

// ���д�
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
        // glfw��ʼ��������أ�����Ҫ�����е���
        initWindow();
        // vk��س�ʼ������Ҫ����Ҫ�����
        initVulkan();
        // glfw����Ϣѭ�������е���
        mainLoop();
        // vk�������Դ�ͷţ���Ҫ
        cleanup();
    }

private:
    GLFWwindow* window;

    VkInstance instance;// ʵ��
    VkDebugUtilsMessengerEXT debugMessenger;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;// �����豸
    VkDevice device;// �߼��豸

    VkQueue graphicsQueue;// ���ڼ�¼ͼ�ζ��е�����
    VkQueue presentQueue;// ����һ���£������޸��߼��豸�Ĵ������̣��Դ������ֶ��в���ȡVkQueue���������Ҫ���һ������

    // Vulkan Window Surface����ĿǰΪֹ�������˽⵽Vulkan��һ����ƽ̨�����޹�����API���ϡ�������ֱ���봰��ϵͳ���н�����Ϊ�˽���Ⱦ������ֵ���Ļ����Ҫ����Vulkan�봰��ϵͳ֮������ӣ�������Ҫʹ��WSI(����ϵͳ����)��չ��
    // �ڱ�С���У����ǽ����۵�һ������VK_KHR_surface(VK_KHR_win32_surface)������¶��VkSurfaceKHR��������surface��һ���������ͣ����Գ�����Ⱦͼ��ʹ�á�
    // ���ǳ����н�Ҫʹ�õ���surface���������Ѿ������GLFW��չ����򿪵���ش���֧�ֵġ�����˵surface����Vulkan�봰��ϵͳ������������

    // ��ȻVkSurfaceKHR�������÷���ƽ̨�޹�����������������Ҫ��������Ĵ���ϵͳ��ϸ�ڡ�
    // ���磬��Windowsƽ̨�У�����ҪWIndows�ϵ�HWND��HMODULE������������ض�ƽ̨�ṩ��Ӧ����չ����Windows��ΪVK_KHR_win32_surface�����Զ�������glfwGetRequiredInstanceExtensions�б���

    // ���ǽ�����ʾ���ʹ���ض�ƽ̨����չ������Windows�ϵ�surface�ţ����ǲ����ڽ̳���ʵ��ʹ������ʹ��GLFW�����Ŀ�����˱�дû���κ�����Ŀ�ƽ̨��ش��롣
    // GLFWʵ����ͨ��glfwCreateWindowSurface�ܺõĴ�����ƽ̨�����ԡ���Ȼ�ˣ��Ƚ����������������ǰ���������ɾ��幤��֮ǰ���˽�һ�±����ʵ�����а����ġ�

    // ��Ϊһ������surface��һ��Vulkan��������Ҫ���VkWin32SurfaceCreateInfoKHR�ṹ�壬�����������Ƚ���Ҫ�Ĳ���:hwnd��hinstance�������Ϥwindows�¿���Ӧ��֪������Щ�Ǵ��ںͽ��̵ľ����
    //     VkWin32SurfaceCreateInfoKHR createInfo;
    //     createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    //     createInfo.hwnd = glfwGetWin32Window(window);
    //     createInfo.hinstance = GetModuleHandle(nullptr);
    //     glfwGetWin32Window�������ڴ�GLFW��������ȡԭʼ��HWND��GetModuleHandle�������ص�ǰ���̵�HINSTANCE���
    // �����ṹ��֮�󣬿�������vkCreateWin32SurfaceKHR����surface�ţ���֮ǰ��ȡ����������DebugReportCallEXTһ��������ͬ����Ҫͨ��instance��ȡ����surface�õ��ĺ����������漰���Ĳ����ֱ�Ϊinstance, surface��������Ϣ���Զ�������������ձ���surface�ľ��������
    // 
    //     auto CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
    // 
    //     if (!CreateWin32SurfaceKHR || CreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
    //         throw std::runtime_error("failed to create window surface!");
    //     }

    // �ù���������ƽ̨���ƣ�����Linux��ʹ��X11���洰��ϵͳ������ͨ��vkCreateXcbSurfaceKHR�����������ӡ�
    // glfwCreateWindowSurface�������ݲ�ͬƽ̨�Ĳ����ԣ���ʵ��ϸ���ϻ�������ͬ���������ڽ������ϵ����ǵĳ����С���initVulkan�����һ������createSurface, ������createInstnace��setupDebugCallback����֮��

    // VK_KHR_surface��չ��һ��instance����չ������ĿǰΪֹ�Ѿ����ù�������������glfwGetRequiredInstanceExtensions���ص��б���
    VkSurfaceKHR surface;// surface����ϵͳǿ��صģ������Ⲣ����vk�ĺ��ģ������Ҫ������ʾ�Ļ������ǵĽ�������Ҫ��surface�򽻵�

    // ��������ɫ����ʹ��uniform�����������붯̬״̬������ȫ�ֱ����������ڻ滭ʱ�޸ģ����Ը�����ɫ������Ϊ���������´������ǡ�����ͨ�����ڽ��任���󴫵ݵ�������ɫ��������Ƭ����ɫ���崴�������������
    // ��Щuniform��ֵ��Ҫ�ڹ��ߴ��������У�ͨ��VkPipelineLayout����ָ������ʹ�ں����������õ�������Ҳ��Ȼ��Ҫ����һ���յ�pipeline layout
    VkPipelineLayout pipelineLayout;

    VkSwapchainKHR swapChain;// �������һ�����Ա�����洢VkSwapchainKHR����:
    // ��������������Ҫ��ȡVkImage��صľ���������ں�����Ⱦ���½������á�������Ա�����洢�þ��:
    std::vector<VkImage> swapChainImages;// ͼ�񱻽�����������Ҳ���ڽ��������ٵ�ͬʱ�Զ������������ǲ���Ҫ����κ�������롣
    // Vulkan ͼ������ͼ��ʹ���κε�VkImage�������ڽ�����������Ⱦ�����еģ����Ƕ���Ҫ����VkImageView����
    // �����������������һ�����ͼ�����ͼ��������ͨ�����������Ⱦ���߲��ܹ���д��Ⱦ���ݣ����仰˵VkImage��������Ⱦ���߽��н���
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapChainExtent = { WIDTH, HEIGHT };// ������Ҫ���ã��̳�����û�еط��������ã�����0��0

    // ���������Ϊ�˲��Խ���������Ч����ԶԶ�����ģ���Ϊ�������ܺܺõ��봰��surface���ݡ�����������ͬ��Ҳ��Ҫ�ܶ����ã�����������Ҫ�˽�һЩ�й����õ�ϸ��
    // ��������������������Ҫ����:

    // ������surface��������(min / max number of images in swap chain, min / max width and height of images)
    //    Surface��ʽ(pixel format, color space)
    //    ��Ч��presentationģʽ
    //    ��findQueueFamilies���ƣ�����ʹ�ýṹ��һ���ԵĴ�����ϸ����Ϣ���������Է�װ�����½ṹ���У�
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // ��render pass�����׶�����ָ���˾���ĸ�������ͨ��VkFramebuffer�����װ�󶨡�֡�������������ñ�ʾΪ���������е�VkImageView����
    // �����ǵ�������ֻ��ʹ��һ��֡������:color attachment��Ȼ��������Ϊ������ͼ���������������ڳ���ʱ���ص�ͼ������ζ�����Ǳ���Ϊ�������е�����ͼ�񴴽�һ��֡�����������ڻ��Ƶ�ʱ��ʹ�ö�Ӧ��ͼ��
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkPipeline graphicsPipeline;
    // Vulkan ��Ⱦͨ������������ɹ��ߵĴ�������֮ǰ��������Ҫ����Vulkan��Ⱦʱ��ʹ�õ�framebuffer֡���������������Ϣ��
    // ������Ҫָ�����ٸ���ɫ����Ȼ��������ᱻʹ�ã�ָ�����ٸ����������õ�����������Ⱦ��������ص�������δ���
    // ���е���Щ��Ϣ������װ��һ������ render pass �Ķ�����
    VkRenderPass renderPass;

    VkCommandPool commandPool;
    // �������ǿ�ʼ�������������ͨ�����Ǽ�¼����ָ���Ϊ����һ����ͼ������Ҫ��ȷ��VkFrameBuffer������ʵ������ҪΪÿһ���������е�ͼ���¼һ�����������
    // ��󴴽�һ��VkCommandBuffer�����б���Ϊ��Ա�����������������common pool���ٵ�ʱ���Զ��ͷ�ϵͳ��Դ���������ǲ���Ҫ��ȷ��дcleanup�߼�
    std::vector<VkCommandBuffer> commandBuffers;

    // �ź���
    // �ڻ��һ��ͼ��ʱ��������Ҫ����һ���ź���׼��������Ⱦ����һ���ź����ķ���������Ⱦ������׼�����г���presentation������������Ա�����洢�ź������� :
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    // ����vulkan��غ���Ҫ��һ�������������˸���vulkan��Ҫ�ĳ�ʼ����Ŀǰ������ȫ
    void initVulkan() {
        // ʵ���Ĵ�����vulkan�к���Ҫ�ĸ���
        createInstance();
        // ��Ҫ��instance����֮��������������surface����Ϊ����Ӱ�������豸��ѡ��֮�����ڱ�С�ڽ�surface�����߼��������۷�Χ������Ϊ����surface������Ⱦ�����ַ�ʽ��һ���Ƚϴ�Ŀ��⣬
        // ���������ڴ��������豸�����ⲿ�����ݣ�����������������豸���ù��������ⴰ��surface�������VulkanҲ�Ƿ�ǿ�Ƶġ�Vulkan����������������ҪͬOpenGLһ������Ҫ��������surface��
        createSurface();
        // debug�����ú�������Ҫ����validation layer��������ã������������ԣ�Ĭ��vk�ǲ��������Բ��
        setupDebugMessenger();
        // �����豸�Ĵ���
        pickPhysicalDevice();
        // �߼��豸�Ĵ���
        createLogicalDevice();
        // ����������
        createSwapChain();
        // ����ͼ����ͼ
        createImageViews();
        // ������Ⱦͨ��
        createRenderPass();
        // ����ͼ�ι���
        createGraphicsPipeline();
        // ����֡����
        createFramebuffers();
        // ���������
        createCommandPool();
        // ���ڿ�ʼʹ��һ��createCommandBuffers����������ͼ�¼ÿһ��������ͼ��ҪӦ�õ�����
        createCommandBuffers();
        // Ϊ�˴����ź���semaphores�����ǽ�Ҫ������ϵ�н̳����һ������: createSemaphores:
        createSemaphores();

        submitDrawCommands();
    }

    void submitDrawCommands() {

        // flags��־λ��������ָ�����ʹ�������������ѡ�Ĳ�����������:
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: �����������ִ��һ�κ��������¼�¼��
        // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT : ����һ������������������������һ����Ⱦͨ���С�
        // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : �������Ҳ���������ύ��ͬʱ��Ҳ�ڵȴ�ִ�С�
        // ����ʹ�������һ����־����Ϊ���ǿ����Ѿ�����һ֡��ʱ�����˻�����������һ֡��δ��ɡ�pInheritanceInfo�����븨����������ء���ָ��������������̳е�״̬
        for (size_t i = 0; i < commandBuffers.size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            beginInfo.pInheritanceInfo = nullptr; // Optional

            vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

            // ������Ⱦͨ��--------
            // ���ƿ�ʼ�ڵ���vkCmdBeginRenderPass������Ⱦͨ����render passʹ��VkRenderPassBeginInfo�ṹ�����������Ϣ��Ϊ����ʱʹ�õĲ���
            // �ṹ���һ����������Ϊ�󶨵���Ӧ��������Ⱦͨ����������Ϊÿһ����������ͼ�񴴽�֡����������ָ��Ϊ��ɫ����
            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[i];

            // ������������������Ⱦ����Ĵ�С����Ⱦ��������ɫ�����غʹ洢��Ҫ������λ�á�����������ؽ�����δ����ֵ��Ϊ����ѵ��������ĳߴ�Ӧ���븽��ƥ��
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = swapChainExtent;

            // ���������������������VK_ATTACHMENT_LOAD_OP_CLEAR�����ֵ�����ǽ���������ɫ�����ļ��ز�����Ϊ�˼򻯲��������Ƕ�����clear colorΪ100%��ɫ
            VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            // ��Ⱦͨ�����ڿ������á����п��Ա���¼�������ʶ���ǰ����ʹ��vkCmdǰ׺������ȫ������void�������ڽ�����¼֮ǰ�������κδ�����
            // ����ÿ�������һ���������Ǽ�¼����������������
            // �ڶ�������ָ�����Ǵ��ݵ���Ⱦͨ���ľ�����Ϣ�����Ĳ�����������ṩrender pass��ҪӦ�õĻ��������ʹ��������ֵ����һ�� :
            // VK_SUBPASS_CONTENTS_INLINE: ��Ⱦ�������Ƕ��������������У�û�и���������ִ�С�
            // VK_SUBPASS_CONTENTS_SECONDARY_COOMAND_BUFFERS : ��Ⱦͨ�������Ӹ����������ִ�С�
            // ���ǲ���ʹ�ø��������������������ѡ���һ����
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // �ڶ�������ָ������������ͣ�graphics or compute pipeline��
            // ���Ǹ���Vulkan��ͼ�ι�����ÿһ���������ִ�м��ĸ�����������Ƭ����ɫ����ʹ�ã�����ʣ�µľ��Ǹ���������������
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            // ʵ�ʵ�vkCmdDraw�����е���������˼��һ�£�������˼򵥣�����Ϊ������ǰָ��������Ⱦ��ص���Ϣ���������µĲ�����Ҫָ���������������:
            // vertexCount: ��ʹ����û�ж��㻺����������������Ȼ��3��������Ҫ���ơ�
            // instanceCount: ����instanced ��Ⱦ�����û��ʹ������1��
            // firstVertex : ��Ϊ���㻺������ƫ����������gl_VertexIndex����Сֵ��
            // firstInstance : ��Ϊinstanced ��Ⱦ��ƫ������������gl_InstanceIndex����Сֵ��
            vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
            // render passִ������ƣ����Խ�����Ⱦ��ҵ
            vkCmdEndRenderPass(commandBuffers[i]);
            // ��ֹͣ��¼��������Ĺ���:
            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void drawFrame() {
        // ͬ���������¼������ַ���:դ�����ź��������Ƕ��ǿ���ͨ��ʹ��һ�������źţ�����Э�������Ķ�����һ�������ȴ�դ�������ź��������ź�״̬ת�䵽���ź�״̬
        // ��֮ͬ�����ڿ�����Ӧ�ó����е���vkWaitForFence����դ��״̬�����ź��������ԡ�
        // դ����Ҫ����Ӧ�ó�����������Ⱦ��������ͬ�������ź�����������������ڻ��߿��������ͬ����������������ͬ����������ֵĶ��в���������ʹ���ź��������

        // ����֮ǰ˵���ģ�drawFrame������Ҫ���ĵ�һ��������Ǵӽ������л�ȡͼ�񡣻���һ�½�������һ����չ���ܣ��������Ǳ���ʹ�þ���vk* KHR����Լ���ĺ���:
        // vkAcquireNextImageKHR����ǰ��������������ϣ����ȡ��ͼ����߼��豸�ͽ�����������������ָ����ȡ��Чͼ��Ĳ���timeout����λ���롣����ʹ��64λ�޷������ֵ��ֹtimeout
        // 
        // ����������������ָ��ʹ�õ�ͬ�����󣬵�presentation���������ͼ��ĳ��ֺ��ʹ�øö������źš�����ǿ�ʼ���Ƶ�ʱ��㡣������ָ��һ���ź���semaphore����դ���������ߡ�����Ŀ���ԣ����ǻ�ʹ��imageAvailableSemaphore
        // ���Ĳ���ָ���������г�Ϊavailable״̬��ͼ���Ӧ���������������������ý�����ͼ������swapChainImages��ͼ��VkImage������ʹ���������ѡ����ȷ���������
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        // �ύ�������
        // �����ύ��ͬ��ͨ��VkSubmitInfo�ṹ����в������á�
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // ǰ��������ָ����ִ�п�ʼ֮ǰҪ�ȴ����ĸ��ź�����Ҫ�ȴ���ͨ�����ĸ��׶Ρ�
        // Ϊ����ͼ��д����ɫ�����ǻ�ȴ�ͼ��״̬��Ϊavailable��������ָ��д����ɫ������ͼ�ι��߽׶Ρ�
        // ����������ζ�ţ�����Ķ�����ɫ����ʼִ�У���ͼ�񲻿��á�waitStages�����ӦpWaitSemaphores�о�����ͬ�������ź���
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        // ����������������ָ���ĸ����������ʵ���ύִ�С�������ᵽ�ģ�����Ӧ���ύ����������������Ǹջ�ȡ�Ľ�����ͼ����Ϊ��ɫ�������а󶨡�
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        // ------�����ź�
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        // signalSemaphoreCount��pSignalSemaphores����ָ���˵��������ִ�н�������Щ�ź��������źš��������ǵ���Ҫʹ��renderFinishedSemaphore
        // ʹ��vkQueueSubmit������ͼ������ύ������������������رȽϴ��ʱ�򣬴���Ч�ʿ��ǣ��������Գ���VkSubmitInfo�ṹ�����顣
        // ���һ������������һ����ѡ��դ�������������ִ�����ʱ�����ᱻ�����źš�����ʹ���ź�������ͬ��������������Ҫ����VK_NULL_HANDLE
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // ------����
        // ����֡���һ�������ǽ�����ύ����������ʹ��������ʾ����Ļ�ϡ�Presentationͨ��VkPresentInfoKHR�ṹ�����ã�����λ����drawFrame�������
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        // ǰ��������ָ���ڽ���presentation֮ǰҪ�ȴ����ź���������VkSubmitInfoһ����
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        // ����������������ָ�������ύͼ��Ľ�������ÿ��������ͼ�����������������½�һ����
        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        // ���һ����ѡ����pResults��������ָ��һ��VkResultֵ���Ա���presentation�ɹ�ʱ���ÿ�������Ľ����������ֻʹ�õ���������������Ҫ����Ϊ���Լ򵥵�ʹ�õ�ǰ�����ķ���ֵ
        presentInfo.pResults = nullptr; // Optional
        // vkQueuePresentKHR�����ύ������ֽ������е�ͼ����������һ���½�ΪvkAcquireNextImageKHR��vkQueuePresentKHR������Ӵ�����
        // ��Ϊ����ʧ�ܲ���һ����ζ�ų���Ӧ����ֹ������������Ϊֹ�����Ĺ��ܲ�ͬ��
        // ���һ��˳�������ٴ����г���ʱ��Ӧ�ÿ��Կ���һ�����ݣ�Ҳ���������λ��Ƴ����ˣ�
        // ��ʱ��������ǰ���һ������˺ܾã��⿴validation����Ϣ��ʵ��������Щ��Ȼ��error�����ǲ���Ӱ����ʾ���������ǿ��Ե��Ե���clearcolor��û����Ч���еĻ�
        // �ڶ������������Σ������ε�ʱ��Ϊû���޸�shader�����Ի����Ǻ�ɫ����clearcolorһ�����Ҿ���Ϊ�������ˣ�������Ѿ�������
        // ���´�Ŷ�vk�Ļ��ƾ�����һ���ܽ��ˣ���Ҫ�Ļ��ƻ�����submitDrawCommands��drawFrame���棬�漰�������ź�ͬ�����Ѿ���������ķ��ͷ�ʽ����
        vkQueuePresentKHR(presentQueue, &presentInfo);

        // �������ʱ������validation layers������Ӧ�ó�����ڴ�ʹ���������ᷢ�������������ӡ�ԭ����validation layers��ʵ��������GPUͬ����
        // ��Ȼ�ڼ������ǲ���Ҫ�ģ�����һ����������ÿһ��֡����������Ե�����Ӱ�졣
        // ���ǿ����ڿ�ʼ������һ֮֡ǰ��ȷ�ĵȴ�presentation���:
        vkQueueWaitIdle(presentQueue);
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // drawFrame��������ִ�����²���:
            // �ӽ������л�ȡһ��ͼ��
            // ��֡�������У�ʹ����Ϊ������ͼ����ִ����������е�����
            // Ϊ�����ճ��֣���ͼ�񷵻���������
            // ÿ���¼��ɷ�����һ��������������Ӧ�������ǵ�ִ�����첽�ġ��������ý��ڲ���ʵ�����֮ǰ���أ�����ִ��˳��Ҳ��δ����ġ����ǲ�����ģ���Ϊÿһ��������ȡ����ǰһ������
            drawFrame();

            // �ںܶ�Ӧ�ó���ĵ�״̬Ҳ����ÿһ֡���¡�Ϊ�˸���Ч�Ļ���һ��ķ�ʽ���£�
            //void drawFrame() {
            //    updateAppState();

            //    vkQueueWaitIdle(presentQueue);    vkAcquireNextImageKHR(...)

            //        submitDrawCommands();

            //    vkQueuePresentKHR(presentQueue, &presentInfo);
            //}
            // �÷����������Ǹ���Ӧ�ó����״̬������������Ϸ��AIЭͬ���򣬶�ǰһ֡����Ⱦ��������ʼ�ձ���CPU��GPU���ڹ���״̬��
        }

        // ��Ҫ�˽����drawFrame���������еĲ��������첽�ġ���ζ�ŵ������˳�mainLoop�����ƺͳ��ֲ���������Ȼ��ִ�С���������ò��ֵ���Դ�ǲ��Ѻõġ�
        // Ϊ�˽��������⣬����Ӧ�����˳�mainLoop���ٴ���ǰ�ȴ��߼��豸�Ĳ������
        vkDeviceWaitIdle(device);
        // Ҳ����ʹ��vkQueueWaitIdle�ȴ��ض���������еĲ�����ɡ���Щ���ܿ�����Ϊһ���ǳ������ķ�ʽ��ִ��ͬ�������ʱ����رպ�����ⲻ�����
    }

    void cleanup() {
        // ��ͼ��ͬ���ǣ�ͼ����ͼ��Ҫ��ȷ�Ĵ������̣������ڳ����˳���ʱ��������Ҫ���һ��ѭ��ȥ��������
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }

        // ɾ��֡������
        for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
        }
        // ɾ��ͼ�ι���
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        // ɾ�����߲���
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        // ɾ����Ⱦͨ��
        vkDestroyRenderPass(device, renderPass, nullptr);

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroySwapchainKHR(device, swapChain, nullptr);

        // ɾ�������
        vkDestroyCommandPool(device, commandPool, nullptr);

        // ɾ���ź���
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

        // device��ʵ�����ں���ݻ٣���Ϊǰ�滹��Щ��Դ���ͷ���Ҫ�õ����ǣ�����swapchain
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        // ������Ŀǰ���������������ʵ�������vk����obj����Ӧ��������vkʵ��ǰ���������٣�����debug�Ļص��ǿ��Կ���������Ϣ��
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
        // ������VkInstance,GLFW�����ָ�룬�Զ�������������ڴ洢VkSurfaceKHR������ָ�롣���ڲ�ͬƽ̨ͳһ����VkResult��GLFWû���ṩר�õĺ�������surface,���ǿ��Լ򵥵�ͨ��Vulkanԭʼ��API���:
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
            // ��������豸�������ԣ��Ƿ�����Ҫ��
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

        // ���ţ�������Ҫ���VkDeviceQueueCreateInfo�ṹ�����������ش������С��ȽϺõ�����ʱ����һ��������и�����ͬ�Ķ��д�
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

        // ��̬����2�����дأ�һ����graphics�ģ�һ����present��
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = 0;

        // �߼��豸����ʱ��������ȷ����swapchain��ext
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // ��֤��Ŀ������Ҳ��Ҫ��ȷ
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

        // swapchain��3���ؼ��Ĳ������ֱ�ͨ��3��������װ�ֱ���
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // ʵ���ϻ���һЩС������Ҫȷ�������ǱȽϼ򵥣�����û�е���������������һ���ǽ������е�ͼ���������������Ϊ���еĳ��ȡ���ָ������ʱͼ�����С���������ǽ����Դ���1��ͼ����������ʵ�����ػ��塣
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // ����maxImageCount��ֵΪ0��������ڴ�֮��û�����ƣ������Ϊʲô������Ҫ��顣
        // ��Vulkan��������Ĵ�������һ��������������Ҳ��Ҫ�������Ľṹ��:
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        // ��ָ���������󶨵������surface֮����Ҫָ��������ͼ���йص���ϸ��Ϣ:
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        // �����Ƴ�createInfo.imageExtent = extent;����validation layers�����������£�validation layers�����̲����а������쳣��Ϣ:
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // imageArrayLayersָ��ÿ��ͼ����ɵĲ������������ǿ���3DӦ�ó��򣬷���ʼ��Ϊ1��imageUsageλ�ֶ�ָ���ڽ������ж�ͼ����еľ���������ڱ�С���У����ǽ�ֱ�Ӷ����ǽ�����Ⱦ������ζ��������Ϊ��ɫ������
        // Ҳ�������Ƚ�ͼ����ȾΪ������ͼ�񣬽��к������������������¿���ʹ����VK_IMAGE_USAGE_TRANSFER_DST_BIT������ֵ����ʹ���ڴ��������Ⱦ��ͼ���䵽������ͼ����С�
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily.value(), (uint32_t)indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;// ����ģʽ������
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;// ����ģʽ����ռ��Ŀǰ��ʼ�����������Ժ����Զ���ѡ�������
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // ��������������Ҫָ����δ���������дصĽ�����ͼ�����graphics���д���presentation���дز�ͬ��������������Ρ����ǽ���graphics�����л��ƽ�������ͼ��Ȼ������һ��presentation�������ύ���ǡ�����д���ͼ�������ַ���:

        // VK_SHARING_MODE_EXCLUSIVE: ͬһʱ��ͼ��ֻ�ܱ�һ�����д�ռ�ã�����������д���Ҫ������Ȩ��Ҫ��ȷָ�������ַ�ʽ�ṩ����õ����ܡ�
        // VK_SHARING_MODE_CONCURRENT : ͼ����Ա�������дط��ʣ�����Ҫ��ȷ����Ȩ������ϵ��
        // �ڱ�С���У�������дز�ͬ������ʹ��concurrentģʽ�����⴦��ͼ������Ȩ������ϵ�����ݣ���Ϊ��Щ���漰���ٸ������������½����ۡ�
        // Concurrentģʽ��ҪԤ��ָ�����д�����Ȩ������ϵ��ͨ��queueFamilyIndexCount��pQueueFamilyIndices�������й������graphics���дغ�presentation���д���ͬ��������Ҫʹ��exclusiveģʽ����Ϊconcurrentģʽ��Ҫ����������ͬ�Ķ��дء�
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        // ���������֧��(supportedTransforms in capabilities),���ǿ���Ϊ������ͼ��ָ��ĳЩת���߼�������90��˳ʱ����ת����ˮƽ��ת���������Ҫ�κ�transoform���������Լ򵥵�����ΪcurrentTransoform��
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // ���Alpha�ֶ�ָ��alphaͨ���Ƿ�Ӧ�����������Ĵ���ϵͳ���л�ϲ�����������Ըù��ܣ��򵥵���VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR��
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        // presentModeָ���Լ������clipped��Ա����ΪVK_TRUE����ζ�����ǲ����ı��ڱε��������ݣ��������������Ĵ�������ǰ��ʱ������Ⱦ�Ĳ������ݴ����ڿ�������֮�⣬���������Ҫ��ȡ��Щ���ػ����ݽ��д���������Կ����ü����������ܡ�
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // ���һ���ֶ�oldSwapChain��Vulkan����ʱ��������������ĳЩ�����±��滻�����細�ڵ�����С���߽�������Ҫ���·�������ͼ����С�����������£�������ʵ������Ҫ���·��䴴�������ұ����ڴ��ֶ���ָ���Ծɵ����ã����Ի�����Դ��
        // ����һ���Ƚϸ��ӵĻ��⣬���ǻ��ں�����½�����ϸ���ܡ����ڼ�������ֻ�ᴴ��һ����������

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // ������createSwapChain����������Ӵ����ȡ�������vkCreateSwapchainKHR����á���ȡ����Ĳ���֮ͬǰ��ȡ���鼯�ϵĲ����ǳ����ơ�
        // ����ͨ������vkGetSwapchainImagesKHR��ȡ��������ͼ����������������������ú��ʵ�������С�����ȡ���ľ�����ϡ�
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    }

    void createImageViews() {
        // ������Ҫ���ĵ�һ��������Ҫ���屣��ͼ����ͼ���ϵĴ�С:
        swapChainImageViews.resize(swapChainImages.size());
        // ��һ����ѭ���������еĽ�����ͼ��
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            // ����ͼ����ͼ�Ĳ�����������VkImageViewCreateInfo�ṹ���С�ǰ�������������ǳ��򵥡�ֱ��
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            // ����viewType��format�ֶ���������ͼ�����ݸñ���ν��͡�viewType��������ͼ����Ϊ1D textures, 2D textures,3D textures ��cube maps
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            // components�ֶ����������ɫͨ�������յ�ӳ���߼������磬���ǿ��Խ�������ɫͨ��ӳ��Ϊ��ɫͨ������ʵ�ֵ�ɫ��������Ҳ���Խ�ͨ��ӳ�����ĳ�����ֵ0��1�����½�������ʹ��Ĭ�ϵ�ӳ�����
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // subresourceRangle�ֶ���������ͼ���ʹ��Ŀ����ʲô���Լ����Ա����ʵ���Ч�������ǵ�ͼ�񽫻���Ϊcolor targets��û���κ�mipmapping levels ���Ƕ�� multiple layers
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            // ����ڱ�д����ʽ��3DӦ�ó��򣬱���VR������Ҫ����֧�ֶ��Ľ�����������ͨ����ͬ�Ĳ�Ϊÿһ��ͼ�񴴽������ͼ�������㲻ͬ���ͼ������������Ⱦʱ����ͼ����Ҫ

            // ����ͼ����ͼ����vkCreateImageView����
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }

            // ӵ����ͼ����ͼ��ʹ��ͼ����Ϊ��ͼ�Ѿ��㹻�ˣ���������û��׼������Ϊ��Ⱦ�� target
            // ����Ҫ����ļ�Ӳ���ȥ׼��������һ������ framebuffer��������֡������������������Ҫ����ͼ�ι���
        }

    }

    // �ڽ����봫�ݸ���Ⱦ����֮ǰ�����Ǳ��뽫���װ��VkShaderModule�����С������Ǵ���һ����������createShaderModuleʵ�ָ��߼�
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        // ����shader module�ǱȽϼ򵥵ģ����ǽ�����Ҫָ���������뻺������ָ������ľ��峤�ȡ���Щ��Ϣ�������VkShaderModuleCreateInfo�ṹ���С�
        // ��Ҫ��������ֽ���Ĵ�С�����ֽ�ָ���ģ������ֽ���ָ����һ��uint32_t���͵�ָ�룬������һ��charָ�롣��������ʹ��reinterpret_cast����ת��ָ�롣
        // ������ʾ������Ҫת��ʱ������Ҫȷ����������uint32_t�Ķ���Ҫ�����˵��ǣ����ݴ洢��std::vector�У�Ĭ�Ϸ������Ѿ�ȷ�����������������µĶ���Ҫ��
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();

        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        // ����vkCreateShaderMoudle����VkShaderModule
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        // ������֮ǰ��������������:�߼��豸������������Ϣ�ṹ���ָ�룬�Զ���������ͱ������ľ����������shader module������Ϻ󣬿��ԶԶ�������Ļ����������������ͷš����Ҫ���Ƿ��ش����õ�shader module
        return shaderModule;
    }

    void createRenderPass() {
        // �����ǵ������У����ǽ�ֻ��һ����ɫ���������������ɽ������е�һ��ͼ������ʾ
        // format����ɫ�����ĸ�ʽ����Ӧ���뽻������ͼ��ĸ�ʽ��ƥ�䣬ͬʱ���ǲ������κζ��ز����Ĺ��������Բ���������Ϊ1
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        // loadOp��storeOp��������Ⱦǰ����Ⱦ�������ڶ�Ӧ�����Ĳ�����Ϊ������ loadOp ����������ѡ��
        // VK_ATTACHMENT_LOAD_OP_LOAD: �����Ѿ������ڵ�ǰ����������
        // VK_ATTACHMENT_LOAD_OP_CLEAR: ��ʼ�׶���һ����������������
        // VK_ATTACHMENT_LOAD_OP_DONT_CARE : ���ڵ�����δ���壬��������
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // �ڻ����µ�һ֡����֮ǰ������Ҫ������ʹ���������������֡������framebufferΪ��ɫ��ͬʱ���� storeOp ��������ѡ��
        // VK_ATTACHMENT_STORE_OP_STORE: ��Ⱦ�����ݻ�洢���ڴ棬����֮����ж�ȡ����
        // VK_ATTACHMENT_STORE_OP_DONT_CARE : ֡����������������Ⱦ������Ϻ�����Ϊundefined
        // ����Ҫ��������Ⱦһ������������Ļ�ϣ���������ѡ��洢����
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        // loadOp��storeOpӦ������ɫ��������ݣ�ͬʱstencilLoadOp / stencilStoreOpӦ����ģ�����ݡ�
        // ���ǵ�Ӧ�ó��򲻻����κ�ģ�滺�����Ĳ�������������loading��storing�޹ؽ�Ҫ
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        // �����֡��������Vulkan��ͨ����VkImage ��������ĳ�����ظ�ʽ�����������������ڴ��еĲ��ֿ��Ի�����Ҫ��imageͼ����еĲ��������ڴ沼�ֵı仯
        // һЩ���õĲ���:
        // VK_IMAGE_LAYOUT_COLOR_ATTACHMET_OPTIMAL: ͼ����Ϊ��ɫ����
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : ͼ���ڽ������б�����
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : ͼ����ΪĿ�꣬�����ڴ�COPY����
        // ���ǻ�����������Щ�����������½ڣ���������Ҫ����Ϊ��Ҫת���ͼ��ָ�����ʵ�layout���ֽ��в���
        // initialLayoutָ��ͼ���ڿ�ʼ������Ⱦͨ��render passǰ��Ҫʹ�õĲ��ֽṹ��finalLayoutָ������Ⱦͨ�������Զ��任ʱʹ�õĲ��֡�
        // ʹ��VK_IMAGE_LAYOUT_UNDEFINED����initialLayout����Ϊ������ͼ��֮ǰ�Ĳ��֡�����ֵ����ͼ������ݲ�ȷ���ᱻ�����������Ⲣ����Ҫ����Ϊ����������Ƕ�Ҫ��������
        // ����ϣ��ͼ����Ⱦ��Ϻ�ʹ�ý��������г��֣���ͽ�����ΪʲôfinalLayoutҪ����ΪVK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        //colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // ��ͨ���͸�������
        // һ����������Ⱦͨ�������ɶ����ͨ����ɡ���ͨ������Ⱦ������һ�����С���ͨ���������������Ⱦ������������֮ǰ��Ⱦͨ�������֡�����������ݡ�
        // ����˵����Ч��������ͨ��ÿһ��������֮ǰ�Ĳ�����
        // �������Щ��Ⱦ�������鵽һ����Ⱦͨ���У�ͨ��Vulkan��ͨ���е���Ⱦ�������������򣬿��Խ�ʡ�ڴ�Ӷ���ø��õ����ܡ���������Ҫ���Ƶ������Σ�����ֻ��Ҫһ����ͨ��
        // ÿ����ͨ������һ�����߶��֮ǰʹ�ýṹ�������ĸ�������Щ���ñ������VkAttachmentReference�ṹ��

        // attachment��������ͨ�����������������е����������С����ǵļ�������һ��VkAttachmentDesription��ɵģ�������������Ϊ0��layoutΪ����ָ����ͨ���ڳ�������ʱ���layout��
        // ����ͨ����ʼ��ʱ��Vulkan���Զ�ת�丽�������layout����Ϊ����������������ɫ�����������ã�layout����ΪVK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL���������õ�����
        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // ��ͨ��ʹ��VkSubpassDescription�ṹ������
        VkSubpassDescription subpass = {};
        //  Vulkan��δ�����ܻ�֧�ֹ���compute subpasses�Ĺ��ܣ�����������������ȷָ��graphics subpassͼ����ͨ������һ��Ϊ��ָ����ɫ����������
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        // �����������е�����ֱ�Ӵ�Ƭ����ɫ�����ã���layout(location = 0) out vec4 outColor ָ��
        // ���Ա���ͨ�����õĸ�����������:
        // pInputAttachments: ��������ɫ���ж�ȡ
        // pResolveAttachments : ����������ɫ�����Ķ��ز���
        // pDepthStencilAttachment : ����������Ⱥ�ģ������
        // pPreserveAttachments : ����������ͨ��ʹ�ã��������ݱ�����

        // ��Ⱦͨ�����󴴽�ͨ�����VkRenderPassCreateInfo�ṹ�壬�������ظ�������ͨ������ɡ�VkAttachmentReference�������ø�������
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        // Subpass ������
        // ���ס����Ⱦͨ���е���ͨ�����Զ������ֵı任����Щ�任ͨ����ͨ����������ϵ���п��ƣ�����ָ���˱˴�֮���ڴ��ִ�е�������ϵ������ֻ��һ����ͨ���������ڴ���ͨ��֮ǰ��֮��Ĳ���Ҳ����Ϊ��ʽ����ͨ������
        // ���������õ�������ϵ����Ⱦͨ����ʼ����Ⱦͨ����������ת��������ǰ�߲����ڵ��·�����
        // ����ת�������ڹ��ߵ���ʼ�׶Σ��������ǻ�û�л�ȡͼ�������������������������Խ�imageAvailableSemaphore��waitStages����ΪVK_PIPELINE_STAGE_TOP_OF_PIPE_BIT��ȷ��ͼ����Ч֮ǰ��Ⱦͨ�����Ὺʼ
        // ������������Ⱦͨ���ȴ�VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT�׶Ρ��Ҿ���ʹ�õڶ���ѡ���Ϊ���ԱȽ�ȫ����˽�subpass������ϵ���乤����ʽ��
        // ��ͨ��������ϵ����ͨ��VkSubpassDependency�ṹ��ָ������createRenderPass��������� :

        // ǰ��������ָ�������Ĺ�ϵ�ʹ�����ͨ��������������ֵVK_SUBPASS_EXTERNAL��ָ����Ⱦͨ��֮ǰ����֮�����ʽ��ͨ����ȡ�������Ƿ���srcSubpass����dstSubPass��ָ����
        // ����0ָ�����ǵ���ͨ�������ǵ�һ��Ҳ��Ψһ�ġ�dstSubpass����ʼ�ո���srcSubPass�Է�ֹ������ϵ����ѭ��
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        // �����������������ֶ�ָ��Ҫ�ȴ��Ĳ�������Щ���������Ľ׶Ρ������ǿ��Է��ʶ���֮ǰ��������Ҫ�ȴ���������ɶ�Ӧͼ��Ķ�ȡ�����������ͨ���ȴ���ɫ��������Ľ׶���ʵ�֡�
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        // ����ɫ�����׶εĲ������漰��ɫ�����Ķ�ȡ��д��Ĳ���Ӧ�õȴ�����Щ���ý���ֹת��������ֱ��ʵ����Ҫ(������):��������Ҫд����ɫʱ��
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        // VkRenderPassCreateInfo�ṹ���������ֶ�ָ�����������顣
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("./sample01_vert.spv");
        auto fragShaderCode = readFile("./sample01_frag.spv");

        // shader module�����������Ⱦ���ߴ����������Ҫ���������ǻ���createGraphicsPipeline�����ж��屾�ر����������ǣ������Ƕ������Ա�����������ǵľ��
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;

        // ���ü���shader module�ĸ�������:
        vertShaderModule = createShaderModule(vertShaderCode);
        fragShaderModule = createShaderModule(fragShaderCode);

        // ��ͼ�ι��ߴ��������createGraphicsPipeline�������ص�ʱ������Ӧ�ñ�������������ڸú�����ɾ������
        //vkDestroyShaderModule(device, fragShaderModule, nullptr);
        //vkDestroyShaderModule(device, vertShaderModule, nullptr);

        // VkShaderModule����ֻ���ֽ��뻺������һ����װ��������ɫ����û�б˴����ӣ�����û�и���Ŀ�ġ�
        // ͨ��VkPipelineShaderStageCreateInfo�ṹ����ɫ��ģ����䵽�����еĶ������Ƭ����ɫ���׶Ρ�
        // VkPipelineShaderStageCreateInfo�ṹ����ʵ�ʹ��ߴ������̵�һ����

        // ����������createGraphicsPipeline��������д������ɫ���ṹ��
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        // ����ǿ�Ƶ�sType��Ա�⣬��һ����Ҫ��֪Vulkan�����ĸ���ˮ�߽׶�ʹ����ɫ��������һ���½ڵ�ÿ���ɱ�̽׶ζ���һ����Ӧ��ö��ֵ
        // ��������������Աָ�������������ɫ��ģ��͵��õ�������������ζ�ſ��Խ����Ƭ����ɫ����ϵ�������ɫ��ģ���У���ʹ�ò�ͬ����ڵ����������ǵ���Ϊ������������£����Ǽ��ʹ�ñ�׼main������Ϊ���
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        // ����һ����ѡ��Ա��pSpecializationInfo,���������ǲ���ʹ����������ֵ������һ�¡�������Ϊ��ɫ��ָ������ֵ��
        // ʹ�õ�����ɫ��ģ�飬ͨ��Ϊ����ʹ�ò�ͬ�ĳ���ֵ����������ˮ�ߴ���ʱ����Ϊ�������á�
        // �������Ⱦʱʹ�ñ���������ɫ������Ч�ʣ���Ϊ���������Խ����Ż�����������ifֵ�жϵ���䡣���û�������ĳ��������Խ���Ա����Ϊnullptr�����ǵ�struct�ṹ���ʼ���Զ�����

        // �޸Ľṹ������Ƭ����ɫ������Ҫ
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        // ��������ṹ��Ĵ�������ͨ�����鱣�棬�ⲿ�����ý�����ʵ�ʵĹ��ߴ�����ʼ
        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // ���й��߰�����������------
        // Vulkan ���й��ܣ������ͼ��API��ͼ����Ⱦ���ߵ����׶��ṩ��Ĭ�ϵ�״̬����Vulkan�У���viewport�Ĵ�С����ɫ��������Ҫ��������������Ϊ
        // 1.��������
        // 2.�������
        // 3.�ӿںͼ���
        // 4.��դ��
        // 5.�ز���
        // 6.��Ⱥ�ģ�����
        // 7.��ɫ���
        // 8.��̬�޸�
        // 9.�ܵ�����

        // ��������------
        // VkPipelineVertexInputStateCreateInfo�ṹ�������˶������ݵĸ�ʽ���ýṹ�����ݴ��ݵ�vertex shader�С��������ַ�ʽ�������� :
        // Bindings:�������ݵļ�϶��ȷ��������ÿ�����������ÿ��instance(instancing)
        // Attribute ���� : ������Ҫ���а󶨼��������ԵĶ�����ɫ���е�����������͡�
        // ��Ϊ���ǽ���������Ӳ���뵽vertex shader�У��������ǽ�Ҫ���Ľṹ��û�ж�������ȥ���ء����ǽ�����vertex buffer�½��л�������
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional
        // pVertexBindingDescriptions��pVertexAttributeDescriptions��Աָ��ṹ�����飬���ڽ�һ���������صĶ���������Ϣ����createGraphicsPipeline�����е�shaderStages�������Ӹýṹ��????

        // �������------
        // VkPipelineInputAssemblyStateCreateInfo�ṹ��������������:����������ʲô���͵ļ���ͼԪ���˽��л��Ƽ��Ƿ����ö��������¿�ʼͼԪ��ͼԪ�����˽ṹ����topologyö��ֵ����
        // VK_PRIMITIVE_TOPOLOGY_POINT_LIST: ���㵽��
        // VK_PRIMITIVE_TOPOLOGY_LINE_LIST : ������ߣ����㲻����
        // VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : ������ߣ�ÿ���߶εĽ���������Ϊ��һ���߶εĿ�ʼ����
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : ������棬���㲻����
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : ÿ�������εĵڶ��������������㶼��Ϊ��һ�������ε�ǰ��������

        // ��������£��������ݰ��ջ������е�������Ϊ����������Ҳ����ͨ��element buffer����������ָ���������ݵ�������ͨ�����ö��������������ܡ�
        // �������primitiveRestartEnable��ԱΪVK_TRUE������ͨ��0xFFFF����0xFFFFFFFF��Ϊ�����������ֽ��ߺ���������_STRIPģʽ�µ�ͼԪ���˽ṹ�����Ӧ���Ǹ�ʲô�����÷����о���ʱ����̫��ע��

        // ͨ�����̳̻��������Σ��������Ǽ�ְ������¸�ʽ������ݽṹ
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // �ӿںͲü�------
        // Viewport��������framebuffer��Ϊ��Ⱦ������Ŀ������������ֵ�ڱ��̳�������������(0, 0)��(width, height)
        // �ǵý�����������imagesͼ���СWIDTH��HEIGHT����ݲ�ͬ�Ĵ������ͬ��������ͼ�񽫻���֡������framebuffersʹ�ã���������Ӧ�ü�����ǵĴ�С
        // minDepth��maxDepth��ֵָ��framebuffer����ȵķ�Χ����Щ��ֵ����������[0.0f, 1.0f]����壬����minDepth���ܻ����maxDepth������㲻���κ�ָ��������ʹ�ñ�׼����ֵ0.0f��1.0f
        // viewports������imageͼ��framebuffer֡��������ת����ϵ���ü����ζ�������Щ��������ر��洢���κ��ڲü�����������ض����ڹ�դ���׶ζ���
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        // �ڱ��̳���������Ҫ��ͼ����Ƶ�������֡������framebuffer�У��������Ƕ���ü����θ��ǵ�����ͼ��
        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;
        // viewport�Ͳü�������Ҫ����VkPipelineViewportStateCreateInfo�ṹ������ʹ�á�����ʹ�ö�viewports�Ͳü�������һЩͼ�ο���ͨ���������á�ʹ�ø�������ҪGPU֧�ָù��ܣ����忴�߼��豸�Ĵ���
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // ��դ��------
        // ��դ��ͨ��������ɫ��������ļ����㷨������������Σ�����ͼ�δ��ݵ�Ƭ����ɫ��������ɫ������
        // ��Ҳ��ִ����Ȳ���depth testing�������face culling�Ͳü����ԣ������Զ������ƬԪ�������ã������Ƿ��������ͼԪ���˻����Ǳ߿�(�߿���Ⱦ)��
        // ���е�����ͨ��VkPipelineRasterizationStateCreateInfo�ṹ�嶨��
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // ����depthClampEnable����ΪVK_TRUE������Զ���ü����ƬԪ����������������Ƕ������ǡ��������������±Ƚ����ã�����Ӱ��ͼ��ʹ�øù�����Ҫ�õ�GPU��֧��
        rasterizer.depthClampEnable = VK_FALSE;
        // ���rasterizerDiscardEnable����ΪVK_TRUE����ô����ͼԪ��Զ���ᴫ�ݵ���դ���׶Ρ����ǻ����Ľ�ֹ�κ������framebuffer֡�������ķ���
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        // polygonMode�������β���ͼƬ�����ݡ�������Чģʽ:
        // VK_POLYGON_MODE_FILL: ������������
        // VK_POLYGON_MODE_LINE : ����α�Ե�߿����
        // VK_POLYGON_MODE_POINT : ����ζ�����Ϊ������
        // ʹ���κ�ģʽ�����Ҫ����GPU����
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // lineWidth��Ա��ֱ�����ģ�����ƬԪ�����������ߵĿ�ȡ������߿�֧��ȡ����Ӳ�����κδ���1.0���߿���Ҫ����GPU��wideLines����֧��
        rasterizer.lineWidth = 1.0f;
        // cullMode�������ھ�����ü������ͷ�ʽ�����Խ�ֹculling���ü�front faces��cull back faces ����ȫ����
        // frontFace����������Ϊfront-facing��Ķ����˳�򣬿�����˳ʱ��Ҳ��������ʱ��
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // ��դ������ͨ����ӳ������߻���ƬԪ��б�����������ֵ��һЩʱ�������Ӱ��ͼ�����õģ��������ǲ������½���ʹ�ã�����depthBiasEnableΪVK_FALSE
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // �ز���------
        // VkPipelineMultisampleStateCreateInfo�ṹ���������ö��ز�������ν���ز����ǿ����anti-aliasing��һ��ʵ�֡�
        // ��ͨ����϶������ε�Ƭ����ɫ���������դ����ͬһ�����ء�����Ҫ�����ڱ�Ե����Ҳ��������עĿ�ľ�ݳ��ֵĵط���
        // ���ֻ��һ�������ӳ�䵽�����ǲ���Ҫ�������Ƭ����ɫ�����в����ģ���ȸ߷ֱ�����˵�����Ứ�ѽϵ͵Ŀ����������ù�����ҪGPU֧��
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional
        // �ڱ��̳������ǲ���ʹ�ö��ز��������ǿ�������ĳ��ԣ�����Ĳ�������Ĺ淶

        // ��Ⱥ�ģ�����------
        // ���ʹ��depth ���� stencil����������Ҫʹ��VkPipelineDepthStencilStateCreateInfo���á��������ڲ���Ҫʹ�ã����Լ򵥵Ĵ���nullptr�������ⲿ�ֻ�ר������Ȼ������½�������
        // ����Ĳ��������Լ���������ӵ�
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.pNext = nullptr;

        // ��ɫ���------
        // Ƭ����ɫ������������ɫ������Ҫ��֡������framebuffer���Ѿ����ڵ���ɫ���л�ϡ����ת���Ĺ��̳�Ϊ��ɫ���������ַ�ʽ:

        // ��old��new��ɫ���л�ϲ���һ�����յ���ɫ
        // ʹ�ð�λ�������old��new��ɫ��ֵ
        // �������ṹ������������ɫ��ϡ���һ���ṹ��VkPipelineColorBlendAttachmentState������ÿ�����ӵ�֡�����������á��ڶ����ṹ��VkPipelineColorBlendStateCreateInfo������ȫ�ֻ�ɫ�����á������ǵ������н�ʹ�õ�һ�ַ�ʽ
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        /*�������ÿ��֡���������û�ɫ�ķ�ʽ��ʹ������α�������˵��:
        if (blendEnable) {
            finalColor.rgb = (srcColorBlendFactor * newColor.rgb) < colorBlendOp > (dstColorBlendFactor * oldColor.rgb);
            finalColor.a = (srcAlphaBlendFactor * newColor.a) < alphaBlendOp > (dstAlphaBlendFactor * oldColor.a);
        }
        else {
            finalColor = newColor;
        }

        finalColor = finalColor & colorWriteMask;
        ���blendEnable����ΪVK_FALSE, ��ô��Ƭ����ɫ�����������ɫ���ᷢ���仯������������ɫ����������µ���ɫ�����õ��Ľ����colorWriteMask����AND���㣬��ȷ��ʵ�ʴ��ݵ�ͨ����

            ������������ʹ�û�ɫ����ʵ��alpha blending���µ���ɫ��ɵ���ɫ���л�ϻ�������ǵ�opacity͸��ͨ����finalColor��Ϊ���յ���� :

        finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
        finalColor.a = newAlpha.a;
        ����ͨ��һ�²������:

        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        �����ڹ淶���ҵ������й�VkBlendFactor��VkBlendOp��ö��ֵ��

            �ڶ����ṹ���������֡�����������ã����������û�ϲ����ĳ������ó���������Ϊ��������Ļ������ :

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
        �����Ҫʹ�õڶ��ַ�ʽ���û�ϲ���(bitwise combination), ��Ҫ����logicOpEnableΪVK_TURE��������λ������logicOp�ֶ���ָ�����ڵ�һ�ַ�ʽ�л��Զ���ֹ����ͬ��Ϊÿһ�����ӵ�֡������framebuffer�رջ�ϲ�����blendEnableΪVK_FALSE��colorWriteMask�������ȷ��֡�������о����ĸ�ͨ������ɫ�ܵ�Ӱ�졣��Ҳ���������ַ�ʽ�½�ֹ������Ŀǰ��Ƭ�λ�������֡���������������ɫ��������κα仯
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

        // ��̬�޸�------
        // ֮ǰ������һЩ�ṹ���״̬����������ʱ��̬�޸ģ����������´���������viewport�Ĵ�С, line width��blend constants�������Ҫ���������Ĳ�������Ҫ���VkPipelineDynamicStateCreateInfo�ṹ��
        // ������ʾ�����룬�������ǲ���Ҫ���Ͻ��ж�̬�޸ĵģ�������ע�͵�
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

        // �ܵ�����------
        // �ýṹ�廹ָ����push���������ǽ���ֵ̬���ݸ���ɫ������һ����ʽ��pipeline layout����������������������������ã��������ڳ����˳���ʱ���������
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // Ȼ���������ǿ������մ���ͼ�ι���֮ǰ������һ��������Ҫ������������render pass------
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        // ���ڿ�ʼ����֮ǰ��VkPipelineShaderStageCreateInfo�ṹ������
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; // Optional

        pipelineInfo.layout = pipelineLayout;

        // ���������Ҫ����render pass��ͼ�ι��߽�Ҫʹ�õ���ͨ��sub pass������
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        // ʵ���ϻ�����������:basePipelineHandle �� basePipelineIndex��Vulkan������ͨ���Ѿ����ڵĹ��ߴ����µ�ͼ�ι��ߡ�
        // �����������¹��ߵ��뷨���ڣ���Ҫ�����Ĺ��������йܵ�������ͬʱ����ýϵ͵Ŀ�����ͬʱҲ���Ը������ɹ����л�������������ͬһ�������ߡ�
        // ����ͨ��basePipelineHandleָ�����й��ߵľ����Ҳ����������basePipelineIndex���Դ�������һ�����ߡ�Ŀǰֻ��һ�����ߣ���������ֻ��Ҫָ��һ���վ����һ����Ч��������
        // ֻ����VkGraphicsPipelineCreateInfo��flags�ֶ���Ҳָ����VK_PIPELINE_CREATE_DERIVATIVE_BIT��־ʱ������Ҫʹ����Щֵ
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        // vkCreateGraphicsPipelines������Vulkan�б���һ��Ĵ�����������Ҫ����Ĳ������������������ݶ��VkGraphicsPipelineCreateInfo���󲢴������VkPipeline����
        // ���Ǵ���VK_NULL_HANDLE������Ϊ�ڶ�����������Ϊ��ѡVkPipelineCache��������á�
        // ���߻���������ڴ洢�͸�����ͨ����ε���vkCreateGraphicsPipelines������ص����ݣ������ڳ���ִ�е�ʱ�򻺴浽һ���ļ��С��������Լ��ٺ����Ĺ��ߴ����߼���������������ǻ��ڹ��߻����½ڽ���
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        // �������г���ȷ�����й���������������ͼ�ι��߳ɹ��������Ѿ��ޱȽӽ�����Ļ�ϻ��Ƴ��������ˡ��ڽ������ļ����½��У����ǽ��ӽ�����ͼ��������ʵ�ʵ�֡����������׼����������
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        // ���ǽ������������ҵ�ͼ����ͼ��ͨ�����Ǵ�����Ӧ��framebuffers
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };

            // ��������������framebuffers�Ƿǳ�ֱ�ӵġ�������Ҫָ��framebuffer��Ҫ���ݵ�renderPass������ֻ��ʹ��������ݵ���Ⱦͨ����֡�����������������ζ������ʹ����ͬ�ĸ�������������
            // attachmentCount��pAttachments����ָ������Ⱦͨ����pAttachment�����а󶨵���Ӧ�ĸ���������VkImageView����
            // width��height�������������ģ�layer��ָ��ͼ�������еĲ��������ǵĽ�����ͼ���ǵ���ͼ����˲���Ϊ1
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

        // ��������־λ����command pools:
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: ��ʾ��������ǳ�Ƶ�������¼�¼������(���ܻ�ı��ڴ������Ϊ)
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : ������������������¼�¼��û�������־�����е��������������һ������
        // ���ǽ����ڳ���ʼ��ʱ���¼���������������ѭ����main loop�ж��ִ�У�������ǲ���ʹ����Щ��־��

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        poolInfo.flags = 0; // Optional
        // �������ͨ�������ύ������һ���豸��������ִ�У������Ǽ�����graphics��presentation���С�
        // ÿ����������ֻ�ܷ����ڵ�һ���͵Ķ������ύ��������������仰˵Ҫ�����������Ҫ���������һ�¡�����Ҫ��¼���Ƶ�������˵��ΪʲôҪѡ��ͼ�ζ��дص�ԭ��

        // ͨ��vkCreateCommandPool�������command pool����������������Ҫ�κ�����Ĳ������á�����������������������ʹ���������Ļ�Ļ��ƹ��������Զ����Ӧ�ñ����������:
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size());

        // level����ָ�������������������ӹ�ϵ��
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY: �����ύ������ִ�У������ܴ�����������������á�
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY : �޷�ֱ���ύ�����ǿ��Դ�������������á�
        // ���ǲ���������ʹ�ø������������ܣ����ǿ������񣬶��ڸ������������ĳ��ò������а���
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
        // �����ź���������Ҫ���VkSemaphoreCreateInfo�ṹ�壬�����ڵ�ǰ�汾��API�У�ʵ���ϲ���Ҫ����κ��ֶΣ���sType:
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // Vulkan APIδ���汾������չ�л����Ϊflags��pNext�������ӹ���ѡ������ź�������Ĺ��̺���Ϥ�ˣ�������ʹ��vkCreateSemaphore:
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {

            throw std::runtime_error("failed to create semaphores!");
        }

        // �ڳ������ʱ��������������ɲ�����Ҫͬ��ʱ��Ӧ������ź�������cleanup��ִ��
    }

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        // ����豸��չ��֧�֣���Ҫ����swapchain����չ��
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // ��齻������֧�����
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        // ���swapChainAdequate�����㹻����ô��Ӧ��֧�ֵ��㹻�ģ����Ǹ��ݲ�ͬ��ģʽ��Ȼ�в�ͬ�����ѡ�����Ǳ�дһ�麯����ͨ����һ�������ò�����ƥ��Ľ��������������������͵�����ȥȷ��:
        //    Surface��ʽ(color depth)
        //        Presentation mode(conditions for ��swapping�� image to the screen)
        //        Swap extent(resolution of images in swap chain)
        //        �������Ժ��ж�ÿһ�����ö���һ���������ֵ��������һ�����Ǿ�ʹ�ã���������һ�𴴽�һЩ�߼�ȥ�ҵ����õĹ�����ֵ��


        // ����෽����豸Ҫ��󣬲��������յ�ͨ����⣨���дأ���չ֧�֣�������֧�֣�
        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // required������ʵ����װ��swapchain����չ
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        // ����Ӳ�ѯ����available extension���ҵ���required�Ļ�����erase����������required��set����empty�ˣ�Ҳ��˵����ѯͨ����
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    // ��⽻����������֧��
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
        // ÿ��VkSurfaceFormatKHR�ṹ������һ��format��һ��colorSpace��Ա��format��Ա����ָ��ɫ��ͨ�������͡����磬VK_FORMAT_B8G8R8A8_UNORM����������ʹ��B,G,R��alpha�����ͨ������ÿһ��ͨ��Ϊ�޷���8bit������ÿ�������ܼ�32bits��colorSpace��Ա����SRGB��ɫ�ռ��Ƿ�ͨ��VK_COLOR_SPACE_SRGB_NONLINEAR_KHR��־֧�֡���Ҫע������ڽ���汾�Ĺ淶�У������־��ΪVK_COLORSPACE_SRGB_NONLINEAR_KHR
        // ����������Ǿ�����ʹ��SRGB(��ɫ����Э��)����Ϊ����õ������׸�֪�ġ���ȷ��ɫ�ʡ�ֱ����SRGB��ɫ�򽻵��ǱȽ�����ս�ģ���������ʹ�ñ�׼��RGB��Ϊ��ɫ��ʽ����Ҳ��ͨ��ʹ�õ�һ����ʽVK_FORMAT_B8G8R8A8_UNORM��
        // ������������surfaceû�������κ�ƫ���Եĸ�ʽ�����ʱ��Vulkan��ͨ��������һ��VkSurfaceFormatKHR�ṹ��ʾ���Ҹýṹ��format��Ա����ΪVK_FORMAT_UNDEFINED

        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
            return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        }

        // ����������ɵ����ø�ʽ����ô���ǿ���ͨ�������б����þ���ƫ���Ե����
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        // ����������ַ�ʽ��ʧЧ�ˣ����ʱ�����ǿ���ͨ�������������д�����򣬵��Ǵ��������»�ѡ���һ����ʽ��Ϊ�����ѡ��
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

    // presentationģʽ���ڽ������Ƿǳ���Ҫ�ģ���Ϊ������������Ļ����ͼ�����������Vulkan�����ĸ�ģʽ����ʹ��:
    // VK_PRESENT_MODE_IMMEDIATE_KHR: Ӧ�ó����ύ��ͼ���������䵽��Ļ���֣�����ģʽ���ܻ����˺��Ч����
    // VK_PRESENT_MODE_FIFO_KHR : ������������һ�����У�����ʾ������Ҫˢ�µ�ʱ����ʾ�豸�Ӷ��е�ǰ���ȡͼ�񣬲��ҳ�����Ⱦ��ɵ�ͼ�������еĺ��档������������ĳ����ȴ������ֹ�ģ����Ƶ��Ϸ�Ĵ�ֱͬ�������ơ���ʾ�豸��ˢ��ʱ�̱���Ϊ����ֱ�жϡ���
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR : ��ģʽ����һ��ģʽ���в�ͬ�ĵط�Ϊ�����Ӧ�ó�������ӳ٣����������һ����ֱͬ���ź�ʱ���п��ˣ�������ȴ���һ����ֱͬ���źţ����ǽ�ͼ��ֱ�Ӵ��͡����������ܵ��¿ɼ���˺��Ч����
    // VK_PRESENT_MODE_MAILBOX_KHR : ���ǵڶ���ģʽ�ı��֡�����������������ʱ��ѡ���µ��滻�ɵ�ͼ�񣬴Ӷ��������Ӧ�ó�������Ρ�����ģʽͨ������ʵ�����ػ����������׼�Ĵ�ֱͬ��˫������ȣ���������Ч�����ӳٴ�����˺��Ч����
    // �߼��Ͽ�����VR_PRESENT_MODE_FIFO_KHRģʽ��֤�����ԣ����������ٴ�����һ������������ѵ�ģʽ :

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
        // �Ҹ�����Ϊ����������һ���ǳ��õĲ��ԡ����������Ǳ���˺�ѣ�ͬʱ��Ȼ������Ե͵��ӳ٣�ͨ����Ⱦ�������µ�ͼ��ֱ�����ܴ�ֱͬ���źš��������ǿ�һ���б����Ƿ����:
        //for (const auto& availablePresentMode : availablePresentModes) {
        //    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        //        return availablePresentMode;
        //    }
        //}

        // �ź����ǣ�һЩ��������Ŀǰ����֧��VK_PRESENT_MODE_FIFO_KHR, ����֮�����VK_PRESENT_MODE_MAILBOX_KHRҲ�����ã����Ǹ�����ʹ��VK_PRESENT_MODE_IMMEDIATE_KHR:
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

    // ������Χ��ָ������ͼ��ķֱ��ʣ����������ǵ������ǻ��ƴ���ķֱ��ʡ��ֱ��ʵķ�Χ��������VkSurfaceCapabilitiesKHR�ṹ���С�Vulkan��������ͨ������currentExtent��Ա��width��height��ƥ�䴰��ķֱ��ʡ�
    // Ȼ����һЩ�������������ͬ�����ã���ζ�Ž�currentExtent��width��height����Ϊ�������ֵ��ʾ:uint32_t�����ֵ������������£����ǲο�����minImageExtent��maxImageExtentѡ����ƥ��ķֱ��ʡ�
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
        // ���������豸�Ķ��д�
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // ���õ��Ķ����У��Ƿ�֧��ͼ�ζ��У��������ö��л���compute��transfer������������3d���Ƴ����У��϶�����Ҫ����֧��ͼ�Σ�Graphics����
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport
                && indices.graphicsFamily != i/*���������Լ��ӵģ�ͻȻð����������unique�Ĵ���û̫����*/)
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
        // ���������glfw�Ľӿڣ���Ƕ��������ƽ̨��ص���չ��ѯ����ʵ����Ҳ�����Լ���ѯ�ģ������ҵ�������������ﲹ��һ��
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        // ��������Ƿ�����֤�㣬�����Ƿ����debug utils����չ��˵�����extensions����Ϊ���Ƶģ�һ����ܺ�ƽ̨��غ�debug��صĶ�һЩ
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

    // ���Իص���������Ҫ��ǰ���ÿ������Ϳ��Խ��յ�vk�ĸ��ֲ㼶�ĵ�����Ϣ�ˣ�verbose��debug��warning��error�ȣ�
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};

int main() {
    // ��vulkan�Ĵ�����̽����˷�װ������main�����е�����ѭ�������������ļ���׶�
    // ���������glfw�Ĵ��ڴ�������Щ������ʵ�Ǻ�vulkan�޹ص�ϵͳ������أ�
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
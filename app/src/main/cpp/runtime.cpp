#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define TAG "SwitchRuntime"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace {

std::mutex g_mutex;
ANativeWindow* g_window = nullptr;
std::thread g_thread;
bool g_running = false;
std::atomic<bool> g_recreate{false};
VkInstance g_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
std::string g_report_dir;
FILE* g_report = nullptr;

void report(const char* fmt, ...) {
    char text[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    LOGI("%s", text);
    if (g_report) {
        fprintf(g_report, "%s\n", text);
        fflush(g_report);
    }
}

const char* result_name(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        default: return "VK_ERROR_OTHER";
    }
}

// NVN translation targets these capabilities; log them explicitly so the
// adapter layer can be designed against real hardware numbers.
void dump_capabilities(VkPhysicalDevice dev) {
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(dev, &p);
    uint32_t api = p.apiVersion;
    report("device name=%s type=%d vendor=0x%04x device=0x%04x api=%u.%u.%u driver=%u",
           p.deviceName, (int)p.deviceType, p.vendorID, p.deviceID,
           VK_VERSION_MAJOR(api), VK_VERSION_MINOR(api), VK_VERSION_PATCH(api),
           p.driverVersion);

    const VkPhysicalDeviceLimits& L = p.limits;
    report("limits maxImageDimension2D=%u maxImageDimensionCube=%u maxColorAttachments=%u",
           L.maxImageDimension2D, L.maxImageDimensionCube, L.maxColorAttachments);
    report("limits maxFramebufferWidth=%u maxFramebufferHeight=%u maxViewportWidth=%u",
           L.maxFramebufferWidth, L.maxFramebufferHeight, L.maxViewportDimensions[0]);
    report("limits maxComputeWorkGroupInvocations=%u maxComputeSharedMemory=%u maxStorageBufferRange=%u",
           L.maxComputeWorkGroupInvocations, L.maxComputeSharedMemorySize,
           (unsigned)L.maxStorageBufferRange);
    report("limits maxVertexOutputComponents=%u maxFragmentOutputAttachments=%u maxVertexInputBindings=%u",
           L.maxVertexOutputComponents, L.maxFragmentOutputAttachments,
           L.maxVertexInputBindings);
    report("limits maxSamplerAnisotropy=%.1f bufferImageGranularity=%u nonCoherentAtomSize=%u",
           L.maxSamplerAnisotropy, (unsigned)L.bufferImageGranularity,
           (unsigned)L.nonCoherentAtomSize);

    VkPhysicalDeviceFeatures f{};
    vkGetPhysicalDeviceFeatures(dev, &f);
    report("features astc=%d etc2=%d bc=%d anisotropy=%d depthClamp=%d "
           "fillModeNonSolid=%d geometryShader=%d tessellation=%d wideLines=%d "
           "shaderStorageImageWriteWithoutFormat=%d independentBlend=%d multiViewport=%d",
           f.textureCompressionASTC_LDR, f.textureCompressionETC2,
           f.textureCompressionBC, f.samplerAnisotropy, f.depthClamp,
           f.fillModeNonSolid, f.geometryShader, f.tessellationShader,
           f.wideLines, f.shaderStorageImageWriteWithoutFormat,
           f.independentBlend, f.multiViewport);

    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(dev, &mem);
    report("memory heapCount=%u typeCount=%u", mem.memoryHeapCount, mem.memoryTypeCount);
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
        report("  heap[%u] size=%lluMB deviceLocal=%d", i,
               (unsigned long long)(mem.memoryHeaps[i].size / (1024 * 1024)),
               (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? 1 : 0);
    }

    uint32_t qcount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, queues.data());
    for (uint32_t i = 0; i < qcount; ++i) {
        report("  queue[%u] flags=0x%02x count=%u timestampBits=%u",
               i, queues[i].queueFlags, queues[i].queueCount,
               queues[i].timestampValidBits);
    }

    uint32_t ecount = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &ecount, nullptr);
    std::vector<VkExtensionProperties> exts(ecount);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &ecount, exts.data());
    std::string vkNvnRelevant;
    const char* interesting[] = {
        "VK_KHR_swapchain", "VK_KHR_maintenance1", "VK_KHR_maintenance2",
        "VK_KHR_maintenance3", "VK_KHR_maintenance4", "VK_KHR_shader_float16_int8",
        "VK_KHR_16bit_storage", "VK_KHR_8bit_storage", "VK_KHR_push_descriptor",
        "VK_KHR_descriptor_update_template", "VK_KHR_draw_indirect_count",
        "VK_KHR_sampler_mirror_clamp_to_edge", "VK_EXT_descriptor_indexing",
        "VK_EXT_host_query_reset", "VK_EXT_robustness2", "VK_EXT_texture_compression_astc_hdr",
        "VK_EXT_queue_family_foreign", "VK_ANDROID_external_memory_android_hardware_buffer",
        "VK_EXT_external_memory_dma_buf", "VK_KHR_dedicated_allocation",
        "VK_KHR_get_memory_requirements2", "VK_KHR_image_format_list",
        "VK_KHR_imageless_framebuffer", "VK_KHR_depth_stencil_resolve",
        "VK_KHR_timeline_semaphore", "VK_KHR_buffer_device_address",
        "VK_KHR_shader_float_controls", "VK_KHR_spirv_1_4", "VK_KHR_uniform_buffer_standard_layout",
        "VK_EXT_shader_subgroup_ballot", "VK_KHR_vulkan_memory_model",
        "VK_EXT_astc_decode_mode", "VK_EXT_memory_budget", "VK_EXT_sampler_filter_minmax",
    };
    for (const char* want : interesting) {
        for (const auto& e : exts) {
            if (strcmp(e.extensionName, want) == 0) {
                if (!vkNvnRelevant.empty()) vkNvnRelevant += " ";
                vkNvnRelevant += want;
                break;
            }
        }
    }
    report("device extensionCount=%u", ecount);
    report("relevant extensions: %s", vkNvnRelevant.c_str());
}

struct SwapchainBundle {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{0, 0};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t imageCount = 0;
};

}  // namespace

namespace {

VkDevice g_device = VK_NULL_HANDLE;
VkQueue g_queue = VK_NULL_HANDLE;
uint32_t g_queue_family = UINT32_MAX;
SwapchainBundle g_swap;
VkCommandPool g_cmd_pool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> g_cmd;
VkSemaphore g_acquire_sem = VK_NULL_HANDLE;
VkSemaphore g_render_sem = VK_NULL_HANDLE;
VkFence g_in_flight = VK_NULL_HANDLE;
uint32_t g_frame = 0;

bool create_device(VkPhysicalDevice phys) {
    uint32_t qcount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qcount, queues.data());

    for (uint32_t i = 0; i < qcount; ++i) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, g_surface, &present);
        if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
            g_queue_family = i;
            break;
        }
    }
    if (g_queue_family == UINT32_MAX) {
        report("FAIL no graphics+present queue family");
        return false;
    }
    report("selected queue family=%u", g_queue_family);

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = g_queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    const char* device_exts[] = {"VK_KHR_swapchain"};
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = device_exts;
    dci.pEnabledFeatures = &features;

    VkResult r = vkCreateDevice(phys, &dci, nullptr, &g_device);
    if (r != VK_SUCCESS) {
        report("FAIL vkCreateDevice %s", result_name(r));
        return false;
    }
    vkGetDeviceQueue(g_device, g_queue_family, 0, &g_queue);
    report("created device and graphics queue");
    return true;
}

bool create_swapchain(VkPhysicalDevice phys) {
    VkSurfaceCapabilitiesKHR caps{};
    VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, g_surface, &caps);
    if (r != VK_SUCCESS) {
        report("FAIL surface capabilities %s", result_name(r));
        return false;
    }
    report("surface minImageCount=%u maxImageCount=%u currentExtent=%ux%u transform=0x%02x",
           caps.minImageCount, caps.maxImageCount, caps.currentExtent.width,
           caps.currentExtent.height, caps.currentTransform);

    uint32_t fcount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, g_surface, &fcount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fcount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, g_surface, &fcount, formats.data());
    if (formats.empty()) {
        report("FAIL no surface formats");
        return false;
    }
    g_swap.format = formats[0].format;
    VkColorSpaceKHR space = formats[0].colorSpace;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            g_swap.format = f.format;
            space = f.colorSpace;
            break;
        }
    }
    report("chosen surface format=%d colorspace=%d (available=%u)",
           (int)g_swap.format, (int)space, fcount);

    uint32_t pmcount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, g_surface, &pmcount, nullptr);
    std::vector<VkPresentModeKHR> modes(pmcount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, g_surface, &pmcount, modes.data());
    std::string mode_text;
    for (auto m : modes) {
        if (!mode_text.empty()) mode_text += " ";
        mode_text += std::to_string((int)m);
    }
    report("present modes (%u): %s", pmcount, mode_text.c_str());

    VkPresentModeKHR present = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { present = m; break; }
    }

    g_swap.extent = caps.currentExtent;
    if (g_swap.extent.width == UINT32_MAX) {
        g_swap.extent.width = (uint32_t)ANativeWindow_getWidth(g_window);
        g_swap.extent.height = (uint32_t)ANativeWindow_getHeight(g_window);
    }
    g_swap.imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && g_swap.imageCount > caps.maxImageCount) {
        g_swap.imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = g_surface;
    sci.minImageCount = g_swap.imageCount;
    sci.imageFormat = g_swap.format;
    sci.imageColorSpace = space;
    sci.imageExtent = g_swap.extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    sci.presentMode = present;
    sci.clipped = VK_TRUE;

    r = vkCreateSwapchainKHR(g_device, &sci, nullptr, &g_swap.swapchain);
    if (r != VK_SUCCESS) {
        report("FAIL vkCreateSwapchainKHR %s", result_name(r));
        return false;
    }
    report("swapchain created: %ux%u images=%u presentMode=%d",
           g_swap.extent.width, g_swap.extent.height, g_swap.imageCount, (int)present);

    uint32_t actual = 0;
    vkGetSwapchainImagesKHR(g_device, g_swap.swapchain, &actual, nullptr);
    g_swap.images.resize(actual);
    vkGetSwapchainImagesKHR(g_device, g_swap.swapchain, &actual, g_swap.images.data());
    return true;
}

bool create_render_pass() {
    VkAttachmentDescription color{};
    color.format = g_swap.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;

    VkResult r = vkCreateRenderPass(g_device, &rpci, nullptr, &g_swap.renderPass);
    if (r != VK_SUCCESS) {
        report("FAIL vkCreateRenderPass %s", result_name(r));
        return false;
    }
    return true;
}

bool create_views_and_framebuffers() {
    g_swap.views.resize(g_swap.images.size());
    g_swap.framebuffers.resize(g_swap.images.size());
    for (size_t i = 0; i < g_swap.images.size(); ++i) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = g_swap.images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = g_swap.format;
        vci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkResult r = vkCreateImageView(g_device, &vci, nullptr, &g_swap.views[i]);
        if (r != VK_SUCCESS) {
            report("FAIL vkCreateImageView[%zu] %s", i, result_name(r));
            return false;
        }
        VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = g_swap.renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &g_swap.views[i];
        fci.width = g_swap.extent.width;
        fci.height = g_swap.extent.height;
        fci.layers = 1;
        r = vkCreateFramebuffer(g_device, &fci, nullptr, &g_swap.framebuffers[i]);
        if (r != VK_SUCCESS) {
            report("FAIL vkCreateFramebuffer[%zu] %s", i, result_name(r));
            return false;
        }
    }
    report("framebuffers ready: %zu", g_swap.framebuffers.size());
    return true;
}

bool create_sync_and_commands() {
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = g_queue_family;
    VkResult r = vkCreateCommandPool(g_device, &pci, nullptr, &g_cmd_pool);
    if (r != VK_SUCCESS) {
        report("FAIL vkCreateCommandPool %s", result_name(r));
        return false;
    }
    g_cmd.resize(g_swap.images.size());
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = g_cmd_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)g_cmd.size();
    r = vkAllocateCommandBuffers(g_device, &ai, g_cmd.data());
    if (r != VK_SUCCESS) {
        report("FAIL vkAllocateCommandBuffers %s", result_name(r));
        return false;
    }

    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(g_device, &sci, nullptr, &g_acquire_sem) != VK_SUCCESS ||
        vkCreateSemaphore(g_device, &sci, nullptr, &g_render_sem) != VK_SUCCESS ||
        vkCreateFence(g_device, &fci, nullptr, &g_in_flight) != VK_SUCCESS) {
        report("FAIL sync object creation");
        return false;
    }
    return true;
}

void record_clear(uint32_t index) {
    VkCommandBuffer cmd = g_cmd[index];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clear{};
    // A stable animated tint makes it obvious the surface is live across frames.
    clear.color.float32[0] = 0.05f + 0.25f * (float)((g_frame / 90) % 2);
    clear.color.float32[1] = 0.35f;
    clear.color.float32[2] = 0.55f;
    clear.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = g_swap.renderPass;
    rbi.framebuffer = g_swap.framebuffers[index];
    rbi.renderArea = {{0, 0}, g_swap.extent};
    rbi.clearValueCount = 1;
    rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

void destroy_swapchain() {
    if (!g_device) return;
    if (g_cmd_pool) { vkDestroyCommandPool(g_device, g_cmd_pool, nullptr); g_cmd_pool = VK_NULL_HANDLE; }
    g_cmd.clear();
    for (auto fb : g_swap.framebuffers) if (fb) vkDestroyFramebuffer(g_device, fb, nullptr);
    for (auto v : g_swap.views) if (v) vkDestroyImageView(g_device, v, nullptr);
    if (g_swap.renderPass) vkDestroyRenderPass(g_device, g_swap.renderPass, nullptr);
    if (g_swap.swapchain) vkDestroySwapchainKHR(g_device, g_swap.swapchain, nullptr);
    if (g_acquire_sem) { vkDestroySemaphore(g_device, g_acquire_sem, nullptr); g_acquire_sem = VK_NULL_HANDLE; }
    if (g_render_sem) { vkDestroySemaphore(g_device, g_render_sem, nullptr); g_render_sem = VK_NULL_HANDLE; }
    if (g_in_flight) { vkDestroyFence(g_device, g_in_flight, nullptr); g_in_flight = VK_NULL_HANDLE; }
    g_swap = SwapchainBundle{};
}

void stop_locked() {
    g_running = false;
    destroy_swapchain();
    if (g_device) { vkDestroyDevice(g_device, nullptr); g_device = VK_NULL_HANDLE; }
    if (g_surface && g_instance) vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
    g_surface = VK_NULL_HANDLE;
    if (g_instance) vkDestroyInstance(g_instance, nullptr);
    g_instance = VK_NULL_HANDLE;
    if (g_window) ANativeWindow_release(g_window);
    g_window = nullptr;
    g_queue = VK_NULL_HANDLE;
    g_queue_family = UINT32_MAX;
}

void render_loop(VkPhysicalDevice phys) {
    auto start = std::chrono::steady_clock::now();
    uint64_t frames = 0;
    while (true) {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!g_running) break;
            if (g_recreate.exchange(false)) {
                vkDeviceWaitIdle(g_device);
                destroy_swapchain();
                if (!create_swapchain(phys) || !create_render_pass() ||
                    !create_views_and_framebuffers() || !create_sync_and_commands()) {
                    report("FAIL swapchain recreation");
                    g_running = false;
                    break;
                }
            }

            vkWaitForFences(g_device, 1, &g_in_flight, VK_TRUE, UINT64_MAX);
            vkResetFences(g_device, 1, &g_in_flight);

            uint32_t index = 0;
            VkResult r = vkAcquireNextImageKHR(g_device, g_swap.swapchain, UINT64_MAX,
                                               g_acquire_sem, VK_NULL_HANDLE, &index);
            if (r == VK_ERROR_OUT_OF_DATE_KHR) { g_recreate = true; continue; }
            if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
                report("FAIL vkAcquireNextImageKHR %s", result_name(r));
                g_running = false;
                break;
            }

            record_clear(index);

            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.waitSemaphoreCount = 1;
            si.pWaitSemaphores = &g_acquire_sem;
            si.pWaitDstStageMask = &wait_stage;
            si.commandBufferCount = 1;
            si.pCommandBuffers = &g_cmd[index];
            si.signalSemaphoreCount = 1;
            si.pSignalSemaphores = &g_render_sem;
            vkQueueSubmit(g_queue, 1, &si, g_in_flight);

            VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            pi.waitSemaphoreCount = 1;
            pi.pWaitSemaphores = &g_render_sem;
            pi.swapchainCount = 1;
            pi.pSwapchains = &g_swap.swapchain;
            pi.pImageIndices = &index;
            r = vkQueuePresentKHR(g_queue, &pi);
            if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
                g_recreate = true;
            } else if (r != VK_SUCCESS) {
                report("FAIL vkQueuePresentKHR %s", result_name(r));
                g_running = false;
                break;
            }

            ++g_frame;
            ++frames;
            if (frames % 120 == 0) {
                auto now = std::chrono::steady_clock::now();
                double secs = std::chrono::duration<double>(now - start).count();
                report("presented %llu frames, %.1f fps (one clear-only frame in flight)",
                       (unsigned long long)frames, frames / (secs > 0 ? secs : 1.0));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_mrckr_switchruntime_MainActivity_nativeSetReportDir(JNIEnv* env, jobject, jstring dir) {
    const char* text = env->GetStringUTFChars(dir, nullptr);
    if (text) {
        g_report_dir = text;
        env->ReleaseStringUTFChars(dir, text);
    }
    if (!g_report_dir.empty()) {
        std::string path = g_report_dir + "/vulkan_report.txt";
        g_report = fopen(path.c_str(), "w");
        report("=== Switch Android Runtime capability report ===");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_mrckr_switchruntime_MainActivity_nativeSetSurface(JNIEnv* env, jobject, jobject surface) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_recreate.exchange(false)) {
        // A fresh surface arrives after the old one was destroyed.
    }
    if (g_instance && g_surface) {
        destroy_swapchain();
    }
    g_window = ANativeWindow_fromSurface(env, surface);
    if (!g_window) { report("FAIL ANativeWindow_fromSurface"); return; }

    if (!g_instance) {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Switch Android Runtime";
        app.applicationVersion = 1;
        app.pEngineName = "SAR Phase 2";
        app.engineVersion = 1;
        app.apiVersion = VK_API_VERSION_1_1;

        const char* extensions[] = {"VK_KHR_surface", "VK_KHR_android_surface"};
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo = &app;
        ci.enabledExtensionCount = 2;
        ci.ppEnabledExtensionNames = extensions;
        VkResult r = vkCreateInstance(&ci, nullptr, &g_instance);
        if (r != VK_SUCCESS) { report("FAIL vkCreateInstance %s", result_name(r)); stop_locked(); return; }
        report("instance created apiVersion=1.1");
    }

    if (!g_surface) {
        VkAndroidSurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
        sci.window = g_window;
        VkResult r = vkCreateAndroidSurfaceKHR(g_instance, &sci, nullptr, &g_surface);
        if (r != VK_SUCCESS) { report("FAIL vkCreateAndroidSurfaceKHR %s", result_name(r)); stop_locked(); return; }
        report("android surface created (%d x %d px)",
               ANativeWindow_getWidth(g_window), ANativeWindow_getHeight(g_window));
    }

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(g_instance, &count, nullptr);
    if (count == 0) { report("FAIL no physical devices"); stop_locked(); return; }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(g_instance, &count, devices.data());
    VkPhysicalDevice phys = devices[0];
    report("physical device count=%u (using index 0)", count);
    dump_capabilities(phys);

    if (!g_device) {
        if (!create_device(phys)) { stop_locked(); return; }
    }
    if (!g_swap.swapchain) {
        if (!create_swapchain(phys) || !create_render_pass() ||
            !create_views_and_framebuffers() || !create_sync_and_commands()) {
            report("FAIL swapchain setup");
            stop_locked();
            return;
        }
        report("Phase 2 pipeline ready: clear + present each frame");
    }

    if (!g_running) {
        g_running = true;
        if (g_thread.joinable()) g_thread.join();
        g_thread = std::thread(render_loop, phys);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_mrckr_switchruntime_MainActivity_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    std::unique_lock<std::mutex> lock(g_mutex);
    if (!g_running && !g_instance) return;
    g_running = false;
    lock.unlock();
    if (g_thread.joinable()) g_thread.join();
    lock.lock();
    stop_locked();
    report("surface destroyed; resources released");
}

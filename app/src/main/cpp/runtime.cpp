#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <vulkan/vulkan.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#define TAG "SwitchRuntime"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace {
std::mutex g_mutex;
ANativeWindow* g_window = nullptr;
std::thread g_thread;
bool g_running = false;
VkInstance g_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;

void report_device(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(device, &p);
    uint32_t api = p.apiVersion;
    LOGI("Vulkan device: %s, vendor=0x%04x, api=%u.%u.%u", p.deviceName,
         p.vendorID, VK_VERSION_MAJOR(api), VK_VERSION_MINOR(api), VK_VERSION_PATCH(api));
}

void render_loop() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!g_running) break;
        }
        // Phase 1 deliberately stops at instance/device capability discovery.
        // Swapchain and render-pass work will be added after a real device trace.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void stop_locked() {
    g_running = false;
    if (g_surface && g_instance) vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
    g_surface = VK_NULL_HANDLE;
    if (g_instance) vkDestroyInstance(g_instance, nullptr);
    g_instance = VK_NULL_HANDLE;
    if (g_window) ANativeWindow_release(g_window);
    g_window = nullptr;
}
}

extern "C" JNIEXPORT void JNICALL
Java_org_mrckr_switchruntime_MainActivity_nativeSetSurface(JNIEnv* env, jobject, jobject surface) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_running) stop_locked();
    g_window = ANativeWindow_fromSurface(env, surface);
    if (!g_window) { LOGE("ANativeWindow_fromSurface failed"); return; }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Switch Android Runtime";
    app.applicationVersion = 1;
    app.pEngineName = "SAR Phase 1";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;

    const char* extensions[] = {"VK_KHR_surface", "VK_KHR_android_surface"};
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = 2;
    ci.ppEnabledExtensionNames = extensions;
    VkResult r = vkCreateInstance(&ci, nullptr, &g_instance);
    if (r != VK_SUCCESS) { LOGE("vkCreateInstance failed: %d", r); stop_locked(); return; }

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(g_instance, &count, nullptr);
    LOGI("Vulkan physical device count: %u", count);
    std::vector<VkPhysicalDevice> devices(count);
    if (count) {
        vkEnumeratePhysicalDevices(g_instance, &count, devices.data());
        for (auto d : devices) report_device(d);
    }
    LOGI("Runtime Phase 1 surface received; Vulkan capability probe complete");
    g_running = true;
    g_thread = std::thread(render_loop);
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
}

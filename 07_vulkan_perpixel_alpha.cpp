// Translucent Vulkan window.
//
// Window setup from test5.cpp (BLACK_BRUSH + DwmEnableBlurBehindWindow),
// plus a minimal Vulkan swapchain that uploads a procedural per-pixel image
// (radial alpha falloff: opaque red at center, transparent at the corners)
// using VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR if supported by the driver.
// On NVIDIA, this requires a recent driver (595+).
//
// The per-pixel content is generated once on the CPU into a host-visible
// staging buffer and copied to the swapchain image each frame with
// vkCmdCopyBufferToImage. No shaders, render passes, or pipelines required.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <cstdlib>
#include <cmath>
#include <cstdint>

static void Fatal(const char* msg) {
    MessageBoxA(nullptr, msg, "Vulkan Transparency Test", MB_OK | MB_ICONERROR);
    exit(1);
}

#define VK_CHECK(call) do {              \
    VkResult r_ = (call);               \
    if (r_ != VK_SUCCESS) {             \
        std::string m_ = std::string(__FILE__) + ":" + std::to_string(__LINE__) \
            + ":\n" + #call + "\nreturned " + std::to_string(r_); \
        Fatal(m_.c_str());               \
    }                                    \
} while(0)

static BOOL g_running = TRUE;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            g_running = FALSE;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_running = FALSE;
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {

    // ---- Window ----

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.lpszClassName  = L"VulkanTransparencyTest";
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);

    // Resizing is intentionally disabled (no WS_THICKFRAME, no WS_MAXIMIZEBOX).
    // This example focuses on Win32 + DWM per-pixel alpha compositing with Vulkan;
    // handling WM_SIZE properly would require swapchain recreation logic
    // (vkDeviceWaitIdle, destroy old swapchain, re-query surface caps, rebuild
    // images/views, handle VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR from
    // vkAcquireNextImageKHR and vkQueuePresentKHR, and skip rendering when the
    // client area is 0x0). That's ~60+ lines of Vulkan boilerplate that would
    // distract from the transparency setup this example is meant to demonstrate.
    DWORD dwStyle   = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD dwExStyle = 0;

    RECT rect = {0, 0, 800, 600};
    AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

    HWND hwnd = CreateWindowEx(
        dwExStyle, wc.lpszClassName, L"Vulkan Transparency Test",
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;

    // Enable per-pixel alpha compositing in DWM.
    {
        HRGN region = CreateRectRgn(0, 0, -1, -1);
        DWM_BLURBEHIND bb = {};
        bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable  = TRUE;
        bb.hRgnBlur = region;
        DwmEnableBlurBehindWindow(hwnd, &bb);
        DeleteObject(region);
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ---- Vulkan instance ----

    const char* instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName   = "VulkanTransparencyTest";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceCI = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCI.pApplicationInfo        = &appInfo;
    instanceCI.enabledExtensionCount   = 2;
    instanceCI.ppEnabledExtensionNames = instanceExtensions;

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &instance));

    // ---- Physical device ----

    uint32_t gpuCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
    if (gpuCount == 0)
        Fatal("No Vulkan physical devices found.");
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data()));
    VkPhysicalDevice physicalDevice = physicalDevices[0];

    // ---- Find a graphics queue family ----

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t queueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueFamily = i;
            break;
        }
    }
    if (queueFamily == UINT32_MAX)
        Fatal("No graphics queue family found.");

    // ---- Surface ----

    VkWin32SurfaceCreateInfoKHR surfaceCI = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    surfaceCI.hinstance = hInstance;
    surfaceCI.hwnd      = hwnd;

    VkSurfaceKHR surface;
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surfaceCI, nullptr, &surface));

    VkBool32 presentSupported = VK_FALSE;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, surface, &presentSupported));
    if (!presentSupported)
        Fatal("Selected queue family does not support presentation.");

    // ---- Device ----

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCI = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueCI.queueFamilyIndex = queueFamily;
    queueCI.queueCount       = 1;
    queueCI.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceCI = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCI.queueCreateInfoCount    = 1;
    deviceCI.pQueueCreateInfos       = &queueCI;
    deviceCI.enabledExtensionCount   = 1;
    deviceCI.ppEnabledExtensionNames = deviceExtensions;

    VkDevice device;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device));

    VkQueue queue;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    // ---- Swapchain ----
    //
    // NOTE: This swapchain is created once and never recreated. The window
    // style above disables resizing so currentExtent stays valid for the
    // lifetime of the program. If you copy this code into an app that allows
    // resizing, you MUST:
    //   - Handle VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR from
    //     vkAcquireNextImageKHR and vkQueuePresentKHR (don't VK_CHECK them).
    //   - On those results (or on WM_SIZE), vkDeviceWaitIdle, destroy the old
    //     swapchain + image views, re-query surface caps, and create a new one.
    //   - Skip rendering when currentExtent is 0x0 (minimized window).
    // None of that is done here on purpose -- it would dwarf the Win32/DWM
    // transparency code this file is meant to illustrate.

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    if (!(surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR))
        Fatal("VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR not supported by this driver.\n"
              "Make sure your driver version supports pre-multiplied alpha compositing.");

    VkSwapchainCreateInfoKHR swapchainCI = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchainCI.surface          = surface;
    swapchainCI.minImageCount    = surfaceCaps.minImageCount < 2 ? 2 : surfaceCaps.minImageCount;
    swapchainCI.imageFormat      = VK_FORMAT_B8G8R8A8_UNORM;
    swapchainCI.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D extent = surfaceCaps.currentExtent;
    swapchainCI.imageExtent      = extent;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCI.preTransform     = surfaceCaps.currentTransform;
    swapchainCI.compositeAlpha   = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    swapchainCI.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCI.clipped          = VK_TRUE;

    VkSwapchainKHR swapchain;
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));

    uint32_t imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
    std::vector<VkImage> swapchainImages(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));

    // ---- Staging buffer with procedural per-pixel content ----
    //
    // BGRA8, pre-multiplied alpha (matches VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR).
    // Filled once on the CPU; copied to the swapchain image each frame.

    const VkDeviceSize bufferSize =
        VkDeviceSize(extent.width) * VkDeviceSize(extent.height) * 4;

    VkBufferCreateInfo bufferCI = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCI.size        = bufferSize;
    bufferCI.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer;
    VK_CHECK(vkCreateBuffer(device, &bufferCI, nullptr, &stagingBuffer));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    const VkMemoryPropertyFlags wantedProps =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        const bool typeAllowed = (memReqs.memoryTypeBits & (1u << i)) != 0;
        const bool propsMatch  =
            (memProps.memoryTypes[i].propertyFlags & wantedProps) == wantedProps;
        if (typeAllowed && propsMatch) {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == UINT32_MAX)
        Fatal("No host-visible/coherent memory type found for staging buffer.");

    VkMemoryAllocateInfo memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memAlloc.allocationSize  = memReqs.size;
    memAlloc.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory stagingMemory;
    VK_CHECK(vkAllocateMemory(device, &memAlloc, nullptr, &stagingMemory));
    VK_CHECK(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

    // Fill the buffer once: radial alpha falloff, opaque red at center,
    // transparent at the corners. Stored pre-multiplied (R = R*A).
    {
        void* mapped = nullptr;
        VK_CHECK(vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &mapped));
        uint8_t* pixels = static_cast<uint8_t*>(mapped);

        const float cx = extent.width  * 0.5f;
        const float cy = extent.height * 0.5f;
        const float maxR = std::sqrt(cx * cx + cy * cy);

        for (uint32_t y = 0; y < extent.height; y++) {
            for (uint32_t x = 0; x < extent.width; x++) {
                const float dx = float(x) - cx;
                const float dy = float(y) - cy;
                const float r  = std::sqrt(dx * dx + dy * dy) / maxR; // 0..1
                float a = 1.0f - r;
                if (a < 0.0f) a = 0.0f;

                // Pre-multiplied red.
                const uint8_t A = uint8_t(a * 255.0f + 0.5f);
                const uint8_t R = A;
                const uint8_t G = 0;
                const uint8_t B = 0;

                // VK_FORMAT_B8G8R8A8_UNORM byte order: B, G, R, A.
                uint8_t* p = pixels + (y * extent.width + x) * 4;
                p[0] = B;
                p[1] = G;
                p[2] = R;
                p[3] = A;
            }
        }
        vkUnmapMemory(device, stagingMemory);
    }

    // ---- Command pool & buffer ----

    VkCommandPoolCreateInfo poolCI = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolCI.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolCI.queueFamilyIndex = queueFamily;

    VkCommandPool commandPool;
    VK_CHECK(vkCreateCommandPool(device, &poolCI, nullptr, &commandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cmdAllocInfo.commandPool        = commandPool;
    cmdAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd));

    // ---- Fence ----

    VkFenceCreateInfo fenceCI = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    VK_CHECK(vkCreateFence(device, &fenceCI, nullptr, &fence));

    // ---- Main loop ----

    while (g_running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!g_running) break;

        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                       VK_NULL_HANDLE, fence, &imageIndex));
        VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device, 1, &fence));

        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        // Transition to TRANSFER_DST_OPTIMAL
        {
            VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.srcAccessMask       = 0;
            barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image               = swapchainImages[imageIndex];
            barrier.subresourceRange    = range;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }

        // Copy the procedural per-pixel image from the staging buffer.
        {
            VkBufferImageCopy region = {};
            region.bufferOffset      = 0;
            region.bufferRowLength   = 0; // tightly packed
            region.bufferImageHeight = 0;
            region.imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            region.imageOffset       = { 0, 0, 0 };
            region.imageExtent       = { extent.width, extent.height, 1 };

            vkCmdCopyBufferToImage(cmd, stagingBuffer,
                                   swapchainImages[imageIndex],
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1, &region);
        }

        // Transition to PRESENT_SRC
        {
            VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask       = 0;
            barrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image               = swapchainImages[imageIndex];
            barrier.subresourceRange    = range;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
        VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device, 1, &fence));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains    = &swapchain;
        presentInfo.pImageIndices  = &imageIndex;

        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));
    }

    // ---- Cleanup ----

    vkDeviceWaitIdle(device);

    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, hInstance);

    return 0;
}

//
// Created by nio on 2025/7/7.
//

#include "Surface.h"
#include <stdexcept>

namespace VkCore {

    VkSurfaceKHR createSurface(VkInstance instance, const Window &window) {
        if (!window.isValid()) {
            throw std::runtime_error("Invalid window handle");
        }

        VkSurfaceKHR surface = VK_NULL_HANDLE;

#if defined(_WIN32)
        HWND hwnd = static_cast<HWND>(window.getNativeHandle());
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    VkWin32SurfaceCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = hInstance,
        .hwnd = hwnd,
    };
    if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Win32 Vulkan surface.");
    }
#elif defined(__ANDROID__)

        auto* nativeWindow = static_cast<ANativeWindow*>(window.getNativeHandle());
    VkAndroidSurfaceCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .window = nativeWindow,
    };
    if (vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Android Vulkan surface.");
    }

#elif defined(__linux__)
        ::Window x11Window = reinterpret_cast<::Window>(window.getNativeHandle());
    ::Display* display = window.x11Display_;
    VkXlibSurfaceCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .dpy = display,
        .window = x11Window,
    };
    if (vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create X11 Vulkan surface.");
    }

#else
#error "Unsupported platform for Vulkan surface creation"
#endif

        return surface;
    }

} // VKCore
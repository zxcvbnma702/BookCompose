/*
 * Vulkan Surface 实现文件
 * 
 * 这个文件实现了跨平台的 Vulkan 表面创建功能。
 * 根据编译时的平台宏定义，自动选择合适的表面创建 API。
 * 
 * 支持的平台：
 * - Windows (_WIN32): 使用 Win32 Surface
 * - Android (__ANDROID__): 使用 Android Surface  
 * - Linux (__linux__): 使用 X11 Surface
 * 
 * Created by nio on 2025/7/7.
 */

#include "Surface.h"
#include <stdexcept>

namespace VkCore {

    /*
     * 创建跨平台的 Vulkan 表面
     * 
     * 工作流程：
     * 1. 验证窗口句柄的有效性
     * 2. 根据编译平台选择相应的表面创建方法
     * 3. 填充平台特定的创建信息结构体
     * 4. 调用平台特定的 Vulkan API 创建表面
     * 5. 错误处理和返回结果
     */
    VkSurfaceKHR createSurface(VkInstance instance, const Window &window) {
        // 第1步：验证输入参数
        if (!window.isValid()) {
            throw std::runtime_error("Invalid window handle");
        }

        // 初始化表面句柄为空值
        VkSurfaceKHR surface = VK_NULL_HANDLE;

// 第2步：根据平台选择表面创建方法

#if defined(_WIN32)
        // Windows 平台：使用 Win32 Surface
        // 获取 Windows 窗口句柄（HWND）和应用程序实例（HINSTANCE）
        HWND hwnd = static_cast<HWND>(window.getNativeHandle());
        HINSTANCE hInstance = GetModuleHandle(nullptr);  // 获取当前模块句柄
        
        // 填充 Win32 表面创建信息结构体
        VkWin32SurfaceCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // 结构体类型
            .pNext = nullptr,                                          // 扩展信息指针
            .flags = 0,                                                // 创建标志（通常为0）
            .hinstance = hInstance,                                    // 应用程序实例句柄
            .hwnd = hwnd,                                              // 窗口句柄
        };
        
        // 调用 Vulkan API 创建 Win32 表面
        if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Win32 Vulkan surface.");
        }
#elif defined(__ANDROID__)
        // Android 平台：使用 Android Surface
        // 获取 Android 原生窗口指针（ANativeWindow*）
        auto* nativeWindow = static_cast<ANativeWindow*>(window.getNativeHandle());
        
        // 填充 Android 表面创建信息结构体
        VkAndroidSurfaceCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,  // 结构体类型
            .pNext = nullptr,                                            // 扩展信息指针
            .flags = 0,                                                  // 创建标志（通常为0）
            .window = nativeWindow,                                      // Android 原生窗口指针
        };
        
        // 调用 Vulkan API 创建 Android 表面
        if (vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Android Vulkan surface.");
        }

#elif defined(__linux__)
        // Linux 平台：使用 X11 Surface
        // 获取 X11 窗口 ID 和显示服务器连接
        ::Window x11Window = reinterpret_cast<::Window>(window.getNativeHandle());
        ::Display* display = window.getX11Display();  // X11 显示服务器连接
        
        // 填充 X11 表面创建信息结构体
        VkXlibSurfaceCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,  // 结构体类型
            .pNext = nullptr,                                         // 扩展信息指针
            .flags = 0,                                               // 创建标志（通常为0）
            .dpy = display,                                           // X11 显示服务器连接
            .window = x11Window,                                      // X11 窗口 ID
        };
        
        // 调用 Vulkan API 创建 X11 表面
        if (vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create X11 Vulkan surface.");
        }

#else
        // 编译时错误：不支持的平台
        // 如果编译到这里，说明当前平台不在支持列表中
        #error "Unsupported platform for Vulkan surface creation"
#endif

        // 第3步：返回创建成功的表面句柄
        return surface;
    }
    
    /*
     * 创建 Vulkan 表面 - 原生句柄版本实现
     * 
     * 这个重载版本为向后兼容性而设计，直接接受原生窗口句柄。
     * 根据编译平台自动选择合适的表面创建 API。
     */
    VkSurfaceKHR createSurface(VkInstance instance, void* nativeWindowHandle) {
        // 验证输入参数
        if (nativeWindowHandle == nullptr) {
            throw std::runtime_error("Invalid native window handle: null pointer");
        }
        
        // 初始化表面句柄
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        
        // 根据平台创建表面
#if defined(_WIN32)
        // Windows 平台：直接使用 HWND
        HWND hwnd = static_cast<HWND>(nativeWindowHandle);
        HINSTANCE hInstance = GetModuleHandle(nullptr);
        
        VkWin32SurfaceCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .hinstance = hInstance,
            .hwnd = hwnd,
        };
        
        if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Win32 Vulkan surface from native handle.");
        }
        
#elif defined(__ANDROID__)
        // Android 平台：直接使用 ANativeWindow*
        auto* nativeWindow = static_cast<ANativeWindow*>(nativeWindowHandle);
        
        VkAndroidSurfaceCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .window = nativeWindow,
        };
        
        if (vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Android Vulkan surface from native handle.");
        }
        
#else
        // Linux 和其他平台需要额外的参数，不能用单个句柄创建
        throw std::runtime_error("This platform requires additional parameters for surface creation. Use the appropriate overload.");
#endif
        
        return surface;
    }
    
//#if defined(__linux__)
//    /*
//     * Linux X11 平台的表面创建实现
//     */
//    VkSurfaceKHR createSurface(VkInstance instance, void* display, unsigned long window) {
//        // 验证输入参数
//        if (display == nullptr) {
//            throw std::runtime_error("Invalid X11 display: null pointer");
//        }
//
//        if (window == 0) {
//            throw std::runtime_error("Invalid X11 window: zero window ID");
//        }
//
//        VkSurfaceKHR surface = VK_NULL_HANDLE;
//
//        // 转换参数类型
//        ::Display* x11Display = static_cast<::Display*>(display);
//        ::Window x11Window = static_cast<::Window>(window);
//
//        // 创建 X11 表面
//        VkXlibSurfaceCreateInfoKHR createInfo{
//            .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
//            .pNext = nullptr,
//            .flags = 0,
//            .dpy = x11Display,
//            .window = x11Window,
//        };
//
//        if (vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
//            throw std::runtime_error("Failed to create X11 Vulkan surface.");
//        }
//
//        return surface;
//    }
//#endif

} // namespace VkCore
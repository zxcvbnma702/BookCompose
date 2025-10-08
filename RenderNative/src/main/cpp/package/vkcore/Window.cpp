/*
 * Vulkan Window 窗口抽象类实现文件
 * 
 * 这个文件实现了跨平台的窗口管理功能，处理不同操作系统的窗口句柄
 * 和资源管理。每个平台都有其特定的窗口系统和生命周期管理需求。
 * 
 * 平台实现说明：
 * 
 * **Windows 平台：**
 * - 使用 HWND 作为窗口句柄
 * - 不需要特殊的资源管理（系统自动处理）
 * - 窗口生命周期由 Windows 消息循环管理
 * 
 * **Android 平台：**
 * - 使用 ANativeWindow* 作为窗口句柄
 * - 需要引用计数管理：acquire/release
 * - 窗口生命周期与 Activity 生命周期相关
 * 
 * **Linux X11 平台：**
 * - 使用 Display* 和 Window ID 的组合
 * - Display 连接需要特殊处理
 * - 窗口生命周期由 X11 事件循环管理
 * 
 * Created by nio on 2025/7/6.
 */

#include "Window.h"

// 平台特定的额外头文件
#if defined(__ANDROID__)
#include <android/native_window_jni.h>  // JNI 相关功能（如果需要与 Java 层交互）
#endif

/*
 * ========== Window 类成员方法实现 ==========
 */

/*
 * 析构函数实现
 * 
 * 负责释放平台特定的窗口资源。不同平台有不同的资源管理需求：
 * - Windows: HWND 由系统管理，无需手动释放
 * - Android: 需要调用 ANativeWindow_release() 减少引用计数
 * - Linux: Display 和 Window 由 X11 管理，无需手动释放
 */
VkCore::Window::~Window() {
#if defined(__ANDROID__)
    // Android 平台：释放原生窗口引用
    if (platform_ == Platform::Android && nativeHandle_) {
        auto *window = static_cast<ANativeWindow *>(nativeHandle_);
        // 减少引用计数，当计数为0时系统会自动销毁窗口
        ANativeWindow_release(window);
    }
#endif
    // Windows 和 Linux 平台不需要特殊的清理操作
}

/*
 * ========== 平台特定的工厂方法实现 ==========
 */

#if defined(__ANDROID__)
/*
 * Android 平台工厂方法实现
 * 
 * Android 使用 ANativeWindow 来表示原生窗口。这个窗口通常从 Java 层
 * 通过 JNI 传递过来，例如从 SurfaceView 或 TextureView 获取。
 * 
 * 引用计数管理：
 * - ANativeWindow 使用引用计数进行生命周期管理
 * - acquire: 增加引用计数，表示我们要使用这个窗口
 * - release: 减少引用计数，当计数为0时系统销毁窗口
 */
VkCore::Window VkCore::Window::fromAndroid(ANativeWindow *window) {
    if (window) {
        // 增加引用计数，确保在我们使用期间窗口不会被销毁
        ANativeWindow_acquire(window);
    }
    // 使用标准构造函数创建 Window 对象
    return {static_cast<void*>(window), Platform::Android};
}
#endif

#if defined(_WIN32)
/*
 * Windows 平台工厂方法实现
 * 
 * Windows 使用 HWND（Handle to Window）来标识窗口。HWND 是一个系统句柄，
 * 由 Windows 操作系统管理其生命周期。我们只需要存储这个句柄，不需要
 * 进行额外的资源管理。
 * 
 * 典型的 HWND 来源：
 * - CreateWindow() / CreateWindowEx() API
 * - 游戏引擎或 GUI 框架提供的窗口句柄
 * - Win32 消息循环中的窗口句柄
 */
VkCore::Window VkCore::Window::fromWin32(HWND hwnd) {
    return Window(static_cast<void*>(hwnd), Platform::Win32);
}
#endif

//#if defined(__linux__)
///*
// * Linux X11 平台工厂方法实现
// *
// * X11 窗口系统使用两个关键组件：
// * 1. Display*: 与 X 服务器的连接，包含显示器配置和通信信息
// * 2. Window: 窗口 ID，是一个整数标识符
// *
// * 为什么需要 Display？
// * - 创建 Vulkan 表面时需要查询窗口属性
// * - X11 的所有操作都需要通过 Display 连接进行
// * - 一个应用可能连接到多个显示器，每个都有自己的 Display
// *
// * 注意：Display 连接的生命周期通常由应用程序的主窗口管理代码负责，
// * 不应该在 Window 对象中释放。
// */
//VkCore::Window VkCore::Window::fromX11(Display* display, ::Window window) {
//    // 使用 Linux 专用的构造函数，传递 Display 连接
//    return Window(reinterpret_cast<void*>(window), Platform::X11, display);
//}
//#endif


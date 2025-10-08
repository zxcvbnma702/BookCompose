/*
 * Vulkan Window 窗口抽象类
 * 
 * 什么是 Window 类？
 * 
 * Window 类是一个跨平台的窗口系统抽象层，它封装了不同操作系统的原生窗口句柄。
 * 在 Vulkan 应用程序中，我们需要一个窗口来显示渲染结果，但不同平台的窗口系统
 * 有很大差异：
 * 
 * 平台差异：
 * - Windows: 使用 HWND (Window Handle) 来标识窗口
 * - Android: 使用 ANativeWindow* 来访问原生窗口
 * - Linux: 使用 X11 的 Display* 和 Window ID 的组合
 * 
 * Window 类的作用：
 * 1. **统一接口**：为不同平台提供一致的窗口访问方式
 * 2. **生命周期管理**：自动管理窗口资源的获取和释放
 * 3. **类型安全**：避免直接使用 void* 指针
 * 4. **平台检测**：运行时识别窗口所属的平台
 * 
 * 使用场景：
 * - 创建 Vulkan Surface（表面）
 * - 窗口大小查询和事件处理
 * - 跨平台图形应用程序开发
 * 
 * Created by nio on 2025/7/6.
 */

#ifndef BOOKCOMPOSE_WINDOW_H
#define BOOKCOMPOSE_WINDOW_H

// 平台特定的头文件包含
#if defined(_WIN32)
#include <windows.h>        // Windows API，提供 HWND 类型
#elif defined(__ANDROID__)
#include <android/native_window.h>  // Android NDK，提供 ANativeWindow 类型
#elif defined(__linux__)
#include <X11/Xlib.h>       // X11 窗口系统，提供 Display 和 Window 类型
#else
#error "Unsupported platform"  // 编译时错误：不支持的平台
#endif

namespace VkCore {

    /*
     * 跨平台窗口抽象类
     * 
     * 这个类封装了不同操作系统的原生窗口句柄，提供统一的接口
     * 来访问窗口资源。它使用 RAII（资源获取即初始化）原则
     * 来管理窗口的生命周期。
     */
    class Window {
    public:
        /*
         * 默认构造函数
         * 创建一个无效的窗口对象，需要通过平台特定的工厂方法来初始化
         */
        Window() = default;

        /*
         * 析构函数
         * 自动释放平台特定的窗口资源
         * - Android: 调用 ANativeWindow_release() 释放原生窗口引用
         * - Windows: 不需要特殊处理（HWND 由系统管理）
         * - Linux: 不需要特殊处理（Display 和 Window 由 X11 管理）
         */
        ~Window();

        // ========== 平台特定的窗口创建工厂方法 ==========

#if defined(_WIN32)
        /*
         * 从 Windows HWND 创建窗口对象
         * 
         * @param hwnd Windows 窗口句柄，由 CreateWindow() 等 API 创建
         * @return Window 封装了 HWND 的窗口对象
         * 
         * 使用示例：
         * ```cpp
         * HWND handle = CreateWindow(...);
         * Window window = Window::fromWin32(handle);
         * ```
         */
        static Window fromWin32(HWND hwnd);
#endif

#if defined(__ANDROID__)
        /*
         * 从 Android ANativeWindow 创建窗口对象
         * 
         * @param window Android 原生窗口指针，通常从 Java 层传递过来
         * @return Window 封装了 ANativeWindow 的窗口对象
         * 
         * 注意事项：
         * - 会自动调用 ANativeWindow_acquire() 增加引用计数
         * - 析构时会调用 ANativeWindow_release() 减少引用计数
         * 
         * 使用示例：
         * ```cpp
         * // 从 JNI 获取原生窗口
         * ANativeWindow* nativeWin = ANativeWindow_fromSurface(env, surface);
         * Window window = Window::fromAndroid(nativeWin);
         * ```
         */
        static Window fromAndroid(ANativeWindow *window);
#endif

//#if defined(__linux__)
//        /*
//         * 从 X11 Display 和 Window ID 创建窗口对象
//         *
//         * @param display X11 显示服务器连接，用于访问窗口属性
//         * @param window X11 窗口 ID，标识具体的窗口
//         * @return Window 封装了 X11 窗口信息的窗口对象
//         *
//         * X11 窗口系统说明：
//         * - Display: 与 X 服务器的连接，一个应用可以连接多个显示器
//         * - Window: 窗口 ID，是一个整数标识符
//         *
//         * 使用示例：
//         * ```cpp
//         * Display* display = XOpenDisplay(nullptr);
//         * ::Window x11Window = XCreateSimpleWindow(...);
//         * Window window = Window::fromX11(display, x11Window);
//         * ```
//         */
//        static Window fromX11(Display* display, ::Window window);
//#endif

        // ========== 通用访问方法 ==========

        /*
         * 获取原生窗口句柄
         * 
         * 返回平台特定的原生窗口句柄：
         * - Windows: HWND (强制转换为 void*)
         * - Android: ANativeWindow* (强制转换为 void*)
         * - Linux: X11 Window ID (强制转换为 void*)
         * 
         * @return 原生窗口句柄的通用指针，需要根据平台转换为具体类型
         * 
         * 使用示例：
         * ```cpp
         * Window window = Window::fromWin32(hwnd);
         * HWND handle = static_cast<HWND>(window.getNativeHandle());
         * ```
         */
        [[nodiscard]] void *getNativeHandle() const { return nativeHandle_; }

        /*
         * 检查窗口是否有效
         * 
         * @return true 如果窗口句柄有效，false 如果窗口未初始化或无效
         * 
         * 注意：这只检查句柄是否为 nullptr，不检查窗口是否真的存在
         */
        [[nodiscard]] bool isValid() const { return nativeHandle_ != nullptr; }

//#if defined(__linux__)
//        /*
//         * 获取 X11 显示服务器连接（仅 Linux 平台）
//         *
//         * X11 窗口系统需要 Display 连接来查询窗口属性和创建 Vulkan 表面。
//         * 这是 Linux 平台特有的需求。
//         *
//         * @return X11 Display 连接指针
//         */
//        [[nodiscard]] Display* getX11Display() const { return x11Display_; }
//#endif

    private:
        /*
         * 平台枚举类型
         * 用于在运行时识别窗口对象所属的平台
         */
        enum class Platform {
            Unknown,    // 未知平台或未初始化
            Win32,      // Windows 平台
            Android,    // Android 平台
            X11         // Linux X11 平台
        };

        /*
         * 私有构造函数
         * 只能通过平台特定的工厂方法调用，确保窗口对象的正确初始化
         * 
         * @param nativeHandle 原生窗口句柄
         * @param platform 窗口所属的平台
         */
        Window(void *nativeHandle, Platform platform) 
            : nativeHandle_(nativeHandle), platform_(platform) {}

//#if defined(__linux__)
//        /*
//         * Linux 平台的额外构造函数
//         * X11 窗口系统需要额外的 Display 连接信息
//         *
//         * @param nativeHandle X11 窗口 ID（转换为 void*）
//         * @param platform 平台类型（应该是 Platform::X11）
//         * @param display X11 显示服务器连接
//         */
//        Window(void *nativeHandle, Platform platform, Display* display)
//            : nativeHandle_(nativeHandle), platform_(platform), x11Display_(display) {}
//#endif

        // ========== 成员变量 ==========

        /*
         * 原生窗口句柄
         * 存储平台特定的窗口句柄，使用 void* 进行类型擦除：
         * - Windows: HWND (强制转换为 void*)
         * - Android: ANativeWindow* (强制转换为 void*)  
         * - Linux: X11 Window ID (强制转换为 void*)
         */
        void *nativeHandle_ = nullptr;

        /*
         * 窗口所属的平台
         * 用于在析构和其他操作中进行平台特定的处理
         */
        Platform platform_ = Platform::Unknown;

//#if defined(__linux__)
//        /*
//         * X11 显示服务器连接（仅 Linux 平台）
//         *
//         * 在 X11 窗口系统中，Display 连接包含了与 X 服务器通信所需的信息。
//         * 创建 Vulkan 表面时需要这个连接来访问窗口属性。
//         *
//         * 注意：这个指针不归 Window 对象所有，不应该在析构函数中释放。
//         * Display 连接通常由应用程序的主窗口管理代码负责管理。
//         */
//        Display* x11Display_ = nullptr;
//#endif
    };

} // namespace VkCore

#endif //BOOKCOMPOSE_WINDOW_H

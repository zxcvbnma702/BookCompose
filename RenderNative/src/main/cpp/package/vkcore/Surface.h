/*
 * Vulkan Surface 头文件
 * 
 * 什么是 Vulkan Surface？
 * Surface（表面）是 Vulkan 与操作系统窗口系统的连接桥梁。它是一个抽象层，
 * 允许 Vulkan 将渲染结果显示到屏幕上。不同的操作系统有不同的窗口系统：
 * - Windows: Win32 API
 * - Android: ANativeWindow
 * - Linux: X11/Wayland
 * 
 * Surface 的作用：
 * 1. 连接 Vulkan 实例与操作系统的窗口
 * 2. 提供渲染目标，让 GPU 知道往哪里输出图像
 * 3. 管理显示缓冲区（通过交换链）
 * 4. 处理不同平台的显示差异
 * 
 * Created by nio on 2025/7/7.
 */

#ifndef BOOKCOMPOSE_SURFACE_H
#define BOOKCOMPOSE_SURFACE_H

#include "Common.h"  // Vulkan 公共定义和工具
#include "Window.h"  // 窗口抽象类

namespace VkCore {
    
    /*
     * 创建 Vulkan 表面 - Window 对象版本
     * 
     * 这个函数会根据当前平台自动选择合适的表面创建方法：
     * - Windows: 使用 vkCreateWin32SurfaceKHR
     * - Android: 使用 vkCreateAndroidSurfaceKHR  
     * - Linux: 使用 vkCreateXlibSurfaceKHR
     * 
     * @param instance - Vulkan 实例，表面需要关联到特定的实例
     * @param window - 窗口对象，包含平台特定的窗口句柄
     * @return VkSurfaceKHR - 创建的 Vulkan 表面句柄
     * 
     * @throws std::runtime_error - 当窗口无效或表面创建失败时抛出异常
     * 
     * 注意事项：
     * - 表面必须在交换链创建之前创建
     * - 表面的生命周期应该与窗口保持一致
     * - 不同平台的表面不可互换
     */
    VkSurfaceKHR createSurface(VkInstance instance, const Window &window);
    
    /*
     * 创建 Vulkan 表面 - 原生句柄版本
     * 
     * 这个重载版本直接接受原生窗口句柄，适用于向后兼容和简化集成。
     * 根据编译时的平台宏自动选择合适的表面创建方法。
     * 
     * @param instance - Vulkan 实例，表面需要关联到特定的实例
     * @param nativeWindowHandle - 原生窗口句柄：
     *                            - Windows: HWND (窗口句柄)
     *                            - Android: ANativeWindow* (原生窗口指针)
     *                            - Linux: 需要额外的 display 参数，使用另一个重载
     * @return VkSurfaceKHR - 创建的 Vulkan 表面句柄
     * 
     * @throws std::runtime_error - 当窗口句柄无效或表面创建失败时抛出异常
     * 
     * 适用场景：
     * - 集成到现有代码库中，不想修改窗口管理逻辑
     * - 简单的单平台应用程序
     * - 向后兼容已有的 Context 接口
     */
    VkSurfaceKHR createSurface(VkInstance instance, void* nativeWindowHandle);
    
#if defined(__linux__)
    /*
     * Linux X11 平台的表面创建函数
     * 
     * Linux 下的 X11 窗口系统需要额外的 Display 连接信息，
     * 因此提供专门的重载版本。
     * 
     * @param instance - Vulkan 实例
     * @param display - X11 显示服务器连接
     * @param window - X11 窗口 ID
     * @return VkSurfaceKHR - 创建的 Vulkan 表面句柄
     */
    VkSurfaceKHR createSurface(VkInstance instance, void* display, unsigned long window);
#endif
}

#endif //BOOKCOMPOSE_SURFACE_H

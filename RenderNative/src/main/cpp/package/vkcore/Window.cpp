//
// Created by nio on 2025/7/6.
//

#include "Window.h"

#if defined(__ANDROID__)

#include <android/native_window_jni.h>  // If interacting with Java via JNI

#endif

VkCore::Window::~Window() {
#if defined(__ANDROID__)
    if (platform_ == Platform::Android && nativeHandle_) {
        auto *window = static_cast<ANativeWindow *>(nativeHandle_);
        ANativeWindow_release(window);
    }
#endif
}

#if defined(__ANDROID__)

VkCore::Window VkCore::Window::fromAndroid(ANativeWindow *window) {
    if(window){
        ANativeWindow_acquire(window);
    }
    return {static_cast<void*>(window), Platform::Android};
}

#elif defined(_WIN32)

VkCore::Window VkCore::Window::fromWin32(HWND hwnd) {
    return Window(static_cast<void*>(hwnd), Platform::Win32);
}

#elif defined(___linux__)

Window Window::fromX11(::Display* display, ::Window window) {
    Window result;
    result.nativeHandle_ = reinterpret_cast<void*>(window);
    result.x11Display_ = display;
    result.platform_ = Platform::X11;
    return result;
}

#endif


//
// Created by nio on 2025/7/6.
//

#ifndef BOOKCOMPOSE_WINDOW_H
#define BOOKCOMPOSE_WINDOW_H

#if defined(_WIN32)
#include <windows.h>
#elif defined(__ANDROID__)
#include <android/native_window.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#else
#error "Unsupported platform"
#endif

namespace VkCore {

    class Window {
    public:
        Window() = default;

        ~Window();

#if defined(_WIN32)
        static Window fromWin32(HWND hwnd);
#elif defined(__ANDROID__)
        static Window fromAndroid(ANativeWindow *window);
#elif defined(__linux__)
        static Window fromX11(Display* display, Window window);
#endif

        [[nodiscard]] void *getNativeHandle() const { return nativeHandle_; }

        [[nodiscard]] bool isValid() const { return nativeHandle_ != nullptr; }

    private:
        enum class Platform {
            Unknown, Win32, Android
        };

        Window(void *nativeHandle, Platform platform) : nativeHandle_(nativeHandle),
                                                        platform_(platform) {}

        void *nativeHandle_ = nullptr;
        Platform platform_ = Platform::Unknown;
    };

} // vkCore

#endif //BOOKCOMPOSE_WINDOW_H

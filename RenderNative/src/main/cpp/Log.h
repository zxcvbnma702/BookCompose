//
// Created by Nio on 2024/11/4.
//

#pragma once
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC

#include <android/log.h>
#include <cassert>
#include <cstdio>

#define APP_NAME "RENDER-LEARNING"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, APP_NAME, "------|" __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, APP_NAME,"------|" __VA_ARGS__))

#define CALL_VK(func)                                         \
  if (VK_SUCCESS != (func)) {                                 \
    LOGE("Vulkan Error File[%s], line[%d]", __FILE__, __LINE__);     \
    assert(false);                                            \
  }

// 定义 GL_CHECK 宏
#define GL_CHECK(stmt) do { \
        stmt; \
        checkGLError(#stmt, __FILE__, __LINE__); \
    } while (0)

// A macro to check value is VK_SUCCESS
#define VK_CHECK(x) CALL_VK(x)
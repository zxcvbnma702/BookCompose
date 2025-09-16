 //
// Created by nio on 2025/7/24.
//

#ifndef BOOKCOMPOSE_COMMON_H
#define BOOKCOMPOSE_COMMON_H

#ifdef _WIN32
#if !defined(VK_USE_PLATFORM_WIN32_KHR)
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#endif

#include <vulkan/vulkan.h>

#if defined(__ANDROID__)
#include <vulkan/vulkan_android.h>
#endif

#include "volk.h"

/**
 * <b> volk.h </b> 和  <b> vulkan_android.h </b> 由于定义了宏 VK_NO_PROTOTYPES，
 * 使得他们有了互相引用的问题，所以只能 先引入 <b> vulkan.h </b> 使得 <b> vulkan_android.h </b> 有了 vulkan api 的引用，
 * 然后再引入 <b> volk.h </b> 使得 <b> volk.h </b> 有了 <code> PFN_vkCreateAndroidSurfaceKHR</code> 的引用。
 */

#ifdef __ANDROID__
#define TAG "OPENXR_SAMPLE"
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#else
#define LOGE(format, ...)                 \
  do {                                    \
    fprintf(stderr, format, __VA_ARGS__); \
    fprintf(stderr, "\n");                \
  } while (0)
#define LOGW(format, ...) LOGE(format, __VA_ARGS__)
#define LOGI(format, ...) LOGE(format, __VA_ARGS__)
#define LOGD(format, ...) LOGE(format, __VA_ARGS__)
#endif

#define CALL_VK(func)                                         \
  if (VK_SUCCESS != (func)) {                                 \
    LOGE("Vulkan Error File[%s], line[%d]", __FILE__, __LINE__);     \
    assert(false);                                            \
  }

#define VK_CHECK(x) CALL_VK(x)
//#include "Log.h"

namespace VkCore{
    VkImageViewType imageTypeToImageViewType(VkImageType imageType, VkImageCreateFlags flags,
                                             bool multiview);

    uint32_t bytesPerPixel(VkFormat format);
}

#endif //BOOKCOMPOSE_COMMON_H

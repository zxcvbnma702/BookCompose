//
// Created by nio on 2025/7/7.
//

#ifndef BOOKCOMPOSE_SURFACE_H
#define BOOKCOMPOSE_SURFACE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include "Window.h"

namespace VkCore {
    VkSurfaceKHR createSurface(VkInstance instance, const Window &window);
}

#endif //BOOKCOMPOSE_SURFACE_H

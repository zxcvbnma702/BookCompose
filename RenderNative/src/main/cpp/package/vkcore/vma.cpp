//
// Created by nio on 2025/7/28.
//
#define VMA_IMPLEMENTATION  // needed for vma, needs to be before vk_mem_alloc.h
// and in single cpp file
#define VMA_STATIC_VULKAN_FUNCTIONS 0

#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#include "../third_party/vk_mem_alloc.h"
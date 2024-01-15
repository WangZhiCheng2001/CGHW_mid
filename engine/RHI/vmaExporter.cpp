#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#define VMA_IMPLEMENTATION
#define VMA_BUFFER_DEVICE_ADDRESS 0
#include <vma/vk_mem_alloc.h>
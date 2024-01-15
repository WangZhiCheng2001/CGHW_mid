#pragma once

#include "memoryAllocator.hpp"
#include "samplerPool.hpp"

struct Buffer
{
public:
    Buffer(const Buffer &) = delete;
    Buffer(Buffer &&) = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer &operator=(Buffer &&) = delete;

    Buffer(vk::Buffer buffer_, MemoryHandle memHandle_, std::shared_ptr<vk::Device> device_, MemoryAllocator *memAllocator_)
        : m_buffer(buffer_), m_memHandle(memHandle_), m_deviceHandle(device_), m_memAllocator(memAllocator_) {}
    ~Buffer()
    {
        m_deviceHandle->destroyBuffer(m_buffer, allocationCallbacks);
        m_memAllocator->freeMemory(m_memHandle);
    }

    void *map()
    {
        void *pData = m_memAllocator->map(m_memHandle).value;
        return pData;
    }
    void unmap()
    {
        m_memAllocator->unmap(m_memHandle);
    }

    operator vk::Buffer() const { return m_buffer; }
    operator MemoryHandle() const { return m_memHandle; }

protected:
    vk::Buffer m_buffer{};
    MemoryHandle m_memHandle{nullptr};

    std::shared_ptr<vk::Device> m_deviceHandle;
    MemoryAllocator *m_memAllocator{nullptr};
};

struct Image
{
public:
    Image(const Image &) = delete;
    Image(Image &&) = delete;
    Image &operator=(const Image &) = delete;
    Image &operator=(Image &&) = delete;

    Image(vk::Image image_, MemoryHandle memHandle_, std::shared_ptr<vk::Device> device_, MemoryAllocator *memAllocator_)
        : m_image(image_), m_memHandle(memHandle_), m_deviceHandle(device_), m_memAllocator(memAllocator_) {}
    ~Image()
    {
        m_deviceHandle->destroyImage(m_image, allocationCallbacks);
        m_memAllocator->freeMemory(m_memHandle);
    }

    void *map()
    {
        void *pData = m_memAllocator->map(m_memHandle).value;
        return pData;
    }
    void unmap()
    {
        m_memAllocator->unmap(m_memHandle);
    }

    operator vk::Image() const { return m_image; }
    operator MemoryHandle() const { return m_memHandle; }

protected:
    vk::Image m_image{};
    MemoryHandle m_memHandle{nullptr};

    std::shared_ptr<vk::Device> m_deviceHandle;
    MemoryAllocator *m_memAllocator{nullptr};
};

struct Texture
{
public:
    Texture(const Texture &) = delete;
    Texture(Texture &&) = delete;
    Texture &operator=(const Texture &) = delete;
    Texture &operator=(Texture &&) = delete;

    Texture(vk::Image image_, MemoryHandle memHandle_, vk::DescriptorImageInfo descriptor_, std::shared_ptr<vk::Device> device_, MemoryAllocator *memAllocator_, SamplerPool *samplerPool_)
        : m_image(image_), m_memHandle(memHandle_), descriptor(descriptor_), m_deviceHandle(device_), m_memAllocator(memAllocator_), m_samplerPool(samplerPool_) {}
    ~Texture()
    {
        m_deviceHandle->destroyImageView(descriptor.imageView, allocationCallbacks);
        m_deviceHandle->destroyImage(m_image, allocationCallbacks);
        m_memAllocator->freeMemory(m_memHandle);

        if (descriptor.sampler)
        {
            m_samplerPool->releaseSampler(descriptor.sampler);
        }
    }

    void *map()
    {
        void *pData = m_memAllocator->map(m_memHandle).value;
        return pData;
    }
    void unmap()
    {
        m_memAllocator->unmap(m_memHandle);
    }

    operator vk::Image() const { return m_image; }
    operator MemoryHandle() const { return m_memHandle; }
    operator vk::DescriptorImageInfo() const { return descriptor; }

    vk::DescriptorImageInfo descriptor{};

protected:
    vk::Image m_image{};
    MemoryHandle m_memHandle{nullptr};

    std::shared_ptr<vk::Device> m_deviceHandle;
    MemoryAllocator *m_memAllocator{nullptr};
    SamplerPool *m_samplerPool{nullptr};
};
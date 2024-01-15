#pragma once

#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>

#include <simple_vulkan_packing.h>

struct RenderContext
{
public:
    RenderContext() = default;
    RenderContext(const std::vector<const char *> &instanceExtensions, const std::vector<const char *> &instanceLayers,
                  const std::vector<const char *> &deviceExtensions, const std::vector<const char *> &deviceLayers);
    ~RenderContext() { destroy(); }

    void init(const std::vector<const char *> &instanceExtensions, const std::vector<const char *> &instanceLayers,
              const std::vector<const char *> &deviceExtensions, const std::vector<const char *> &deviceLayers);

    //--------------------------------------------------------------------------------------------------
    // Basic buffer creation
    std::shared_ptr<Buffer> createBuffer(const vk::BufferCreateInfo &info_,
                                         const vk::MemoryPropertyFlags memUsage_ = vk::MemoryPropertyFlagBits::eDeviceLocal);

    //--------------------------------------------------------------------------------------------------
    // Simple buffer creation
    // implicitly sets VK_BUFFER_USAGE_TRANSFER_DST_BIT
    std::shared_ptr<Buffer> createBuffer(vk::DeviceSize size_ = 0,
                                         vk::BufferUsageFlags usage_ = vk::BufferUsageFlags(),
                                         const vk::MemoryPropertyFlags memUsage_ = vk::MemoryPropertyFlagBits::eDeviceLocal);

    //--------------------------------------------------------------------------------------------------
    // Simple buffer creation with data uploaded through staging manager
    // implicitly sets VK_BUFFER_USAGE_TRANSFER_DST_BIT
    std::shared_ptr<Buffer> createBuffer(const vk::DeviceSize &size_,
                                         const void *data_,
                                         vk::BufferUsageFlags usage_,
                                         const vk::MemoryPropertyFlags memUsage_ = vk::MemoryPropertyFlagBits::eDeviceLocal);

    //--------------------------------------------------------------------------------------------------
    // Simple buffer creation with data uploaded through staging manager
    // implicitly sets VK_BUFFER_USAGE_TRANSFER_DST_BIT
    template <typename T>
    std::shared_ptr<Buffer> createBuffer(const std::vector<T> &data_,
                                         vk::BufferUsageFlags usage_,
                                         vk::MemoryPropertyFlags memUsage_ = vk::MemoryPropertyFlagBits::eDeviceLocal)
    {
        return createBuffer(sizeof(T) * data_.size(), data_.data(), usage_, memUsage_);
    }

    //--------------------------------------------------------------------------------------------------
    // Basic image creation
    std::shared_ptr<Image> createImage(const vk::ImageCreateInfo &info_, const vk::MemoryPropertyFlags memUsage_ = vk::MemoryPropertyFlagBits::eDeviceLocal);

    //--------------------------------------------------------------------------------------------------
    // Create an image with data uploaded through staging manager
    std::shared_ptr<Image> createImage(size_t size_,
                                       const void *data_,
                                       const vk::ImageCreateInfo &info_,
                                       const vk::ImageLayout &layout_ = vk::ImageLayout::eShaderReadOnlyOptimal);

    std::shared_ptr<Texture> createTexture(const vk::ImageCreateInfo &info_,
                                           const vk::ImageLayout &layout_ = vk::ImageLayout::eShaderReadOnlyOptimal,
                                           bool isCube = false,
                                           const vk::MemoryPropertyFlags memUsage_ = vk::MemoryPropertyFlagBits::eDeviceLocal);
    std::shared_ptr<Texture> createTexture(const vk::ImageCreateInfo &info_,
                                           const vk::SamplerCreateInfo &samplerCreateInfo,
                                           const vk::ImageLayout &layout_ = vk::ImageLayout::eShaderReadOnlyOptimal,
                                           bool isCube = false,
                                           const vk::MemoryPropertyFlags memUsage_ = vk::MemoryPropertyFlagBits::eDeviceLocal);

    //--------------------------------------------------------------------------------------------------
    // shortcut that creates the image for the texture
    // - creates the image
    // - creates the texture part by associating image and sampler
    //
    std::shared_ptr<Texture> createTexture(size_t size_,
                                           const void *data_,
                                           const vk::ImageCreateInfo &info_,
                                           const vk::SamplerCreateInfo &samplerCreateInfo,
                                           const vk::ImageLayout &layout_ = vk::ImageLayout::eShaderReadOnlyOptimal,
                                           bool isCube = false);

    //--------------------------------------------------------------------------------------------------
    // Acquire a sampler with the provided information
    // Every acquire must have an appropriate release for appropriate internal reference counting
    vk::Sampler acquireSampler(const vk::SamplerCreateInfo &info) { return m_samplerPool.acquireSampler(info); }
    void releaseSampler(vk::Sampler sampler) { m_samplerPool.releaseSampler(sampler); }

    std::shared_ptr<vk::Instance> getInstanceHandle() const noexcept { return m_instanceHandle; }
    std::shared_ptr<vk::PhysicalDevice> getAdapterHandle() const noexcept { return m_adapterHandle; }
    std::shared_ptr<vk::Device> getDeviceHandle() const noexcept { return m_deviceHandle; }
    std::shared_ptr<QueueInstance> getQueueInstanceHandle(vk::QueueFlagBits type, bool mustSeparate = true) const;
    std::shared_ptr<vk::PipelineCache> getPipelineCacheHandle() const noexcept { return std::make_shared<vk::PipelineCache>(m_pipelineCacheHandle); }

    void destroy();

private:
    MemoryHandle allocateMemory(const MemoryAllocateInfo &allocateInfo);
    void createBufferEx(const vk::BufferCreateInfo &info_, vk::Buffer &buffer);
    void createImageEx(const vk::ImageCreateInfo &info_, vk::Image &image);

    std::vector<vk::CommandBuffer> allocateInternalTransferBuffer(size_t count = 1U);

    std::shared_ptr<vk::Instance> m_instanceHandle;
    std::shared_ptr<vk::PhysicalDevice> m_adapterHandle;
    std::shared_ptr<vk::Device> m_deviceHandle;
    vk::PipelineCache m_pipelineCacheHandle;

    QueueInstance m_graphicsQueueHandle;
    std::optional<QueueInstance> m_computeQueueHandle{};
    std::optional<QueueInstance> m_transferQueueHandle{};

    vk::CommandPool m_internalTransferPool{};
    vk::Fence m_internalTransferFence{};

    vk::PhysicalDeviceMemoryProperties m_memoryProperties{};
    VmaAllocator m_vma{nullptr};
    std::unique_ptr<MemoryAllocator> m_memAlloc;
    // std::unique_ptr<StagingHelper> m_staging;
    SamplerPool m_samplerPool;

#ifdef NDEBUG
#if defined(VK_EXT_debug_utils)
    vk::DebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
#else if defined(VK_EXT_debug_report)
    vk::DebugReportCallbackEXT m_debugReporter{VK_NULL_HANDLE};
#endif
#endif
};
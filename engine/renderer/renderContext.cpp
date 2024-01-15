#include <exception>

#include "renderContext.h"

#ifdef NDEBUG
#if defined(VK_EXT_debug_utils)
PFN_vkCreateDebugUtilsMessengerEXT pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(VkInstance instance,
                                                              const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                                              const VkAllocationCallbacks *pAllocator,
                                                              VkDebugUtilsMessengerEXT *pMessenger)
{
    return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks const *pAllocator)
{
    return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}
#elif defined(VK_EXT_debug_report)
PFN_vkCreateDebugReportCallbackEXT pfnVkCreateDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT pfnVkDestroyDebugReportCallbackEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugReportCallbackEXT(VkInstance instance,
                                                              const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
                                                              const VkAllocationCallbacks *pAllocator,
                                                              VkDebugReportCallbackEXT *pCallback)
{
    return pfnVkCreateDebugReportCallbackEXT(instance, pCreateInfo, pAllocator, pCallback);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDebugReportCallbackEXT(VkInstance instance,
                                                           VkDebugReportCallbackEXT callback,
                                                           const VkAllocationCallbacks *pAllocator)
{
    return pfnVkDestroyDebugReportCallbackEXT(instance, callback, pAllocator);
}
#endif
#endif

RenderContext::RenderContext(const std::vector<const char *> &instanceExtensions, const std::vector<const char *> &instanceLayers,
                             const std::vector<const char *> &deviceExtensions, const std::vector<const char *> &deviceLayers)
{
    init(instanceExtensions, instanceLayers, deviceExtensions, deviceLayers);
}

void RenderContext::init(const std::vector<const char *> &instanceExtensions, const std::vector<const char *> &instanceLayers,
                         const std::vector<const char *> &deviceExtensions, const std::vector<const char *> &deviceLayers)
{
    vk::ApplicationInfo appInfo;
    appInfo.setApiVersion(VK_API_VERSION_1_3);
    vk::InstanceCreateInfo instanceCreateInfo;
    std::vector<const char *> _instanceExtensions{instanceExtensions};
    std::vector<const char *> _instanceLayers{instanceLayers};

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    auto instanceExtProperties = vk::enumerateInstanceExtensionProperties();
    for (const auto &property : instanceExtProperties)
    {
        if (property.extensionName == VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
        {
            instanceCreateInfo.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
            _instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        }
    }
#endif
#ifdef NDEBUG
#if defined(VK_EXT_debug_utils)
    _instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    _instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
#else if defined(VK_EXT_debug_report)
    _instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    _instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif
#endif
    instanceCreateInfo.setPApplicationInfo(&appInfo)
        .setPEnabledExtensionNames(_instanceExtensions)
        .setPEnabledLayerNames(_instanceLayers);
#ifdef NDEBUG
    std::vector enabledFeatures{vk::ValidationFeatureEnableEXT::eDebugPrintf};
    vk::ValidationFeaturesEXT validationFeatures{};
    validationFeatures.setEnabledValidationFeatures(enabledFeatures);
    instanceCreateInfo.setPNext(&validationFeatures);
#endif
    m_instanceHandle = std::make_shared<vk::Instance>(vk::createInstance(instanceCreateInfo, allocationCallbacks));

#ifdef NDEBUG
#if defined(VK_EXT_debug_utils)
    pfnVkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(m_instanceHandle->getProcAddr("vkCreateDebugUtilsMessengerEXT"));
    if (!pfnVkCreateDebugUtilsMessengerEXT)
    {
        std::cerr << "GetInstanceProcAddr: Unable to find pfnVkCreateDebugUtilsMessengerEXT function." << std::endl;
        abort();
    }

    pfnVkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(m_instanceHandle->getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
    if (!pfnVkDestroyDebugUtilsMessengerEXT)
    {
        std::cerr << "GetInstanceProcAddr: Unable to find pfnVkDestroyDebugUtilsMessengerEXT function." << std::endl;
        abort();
    }

    vk::DebugUtilsMessengerCreateInfoEXT messengerCreateInfo;
    messengerCreateInfo.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    messengerCreateInfo.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    messengerCreateInfo.setPfnUserCallback(debug_message);
    messengerCreateInfo.setPUserData(nullptr);
    m_debugMessenger = m_instanceHandle->createDebugUtilsMessengerEXT(messengerCreateInfo, allocationCallbacks);
#else if defined(VK_EXT_debug_report)
    pfnVkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(m_instanceHandle->getProcAddr("vkCreateDebugReportCallbackEXT"));
    if (!pfnVkCreateDebugReportCallbackEXT)
    {
        std::cerr << "GetInstanceProcAddr: Unable to find pfnVkCreateDebugReportCallbackEXT function." << std::endl;
        abort();
    }

    pfnVkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(m_instanceHandle->getProcAddr("vkDestroyDebugReportCallbackEXT"));
    if (!pfnVkDestroyDebugReportCallbackEXT)
    {
        std::cerr << "GetInstanceProcAddr: Unable to find pfnVkDestroyDebugReportCallbackEXT function." << std::endl;
        abort();
    }

    vk::DebugReportCallbackCreateInfoEXT callbackCreateInfo;
    callbackCreateInfo.setFlags(vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eError);
    callbackCreateInfo.setPfnCallback(debug_report);
    callbackCreateInfo.setPUserData(nullptr);
    m_debugReporter = m_instanceHandle->createDebugReportCallbackEXT(callbackCreateInfo, allocationCallbacks);
#endif
#endif

    auto adapters = m_instanceHandle->enumeratePhysicalDevices();
    for (const auto &adapter : adapters)
    {
        auto property = adapter.getProperties2();
        if (property.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            m_adapterHandle = std::make_shared<vk::PhysicalDevice>(adapter);
    }
    if (!adapters.empty() && !m_adapterHandle)
        m_adapterHandle = std::make_shared<vk::PhysicalDevice>(adapters[0]);
    auto adapterFeatures = m_adapterHandle->getFeatures2<vk::PhysicalDeviceFeatures2,
                                                         vk::PhysicalDeviceVulkan11Features,
                                                         vk::PhysicalDeviceVulkan12Features,
                                                         vk::PhysicalDeviceVulkan13Features,
                                                         vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT>();

    auto queueFamilyProperties = m_adapterHandle->getQueueFamilyProperties2();
    for (auto i = 0; i < queueFamilyProperties.size(); ++i)
    {
        if (queueFamilyProperties[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics)
            m_graphicsQueueHandle.queue_family_index = i;
        if (m_graphicsQueueHandle.queue_family_index != i &&
            (queueFamilyProperties[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eCompute))
            m_computeQueueHandle->queue_family_index = i;
        if (m_graphicsQueueHandle.queue_family_index != i &&
            m_computeQueueHandle->queue_family_index != i &&
            (queueFamilyProperties[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eTransfer))
            m_transferQueueHandle->queue_family_index = i;
    }

    std::vector<const char *> _deviceExtensions{deviceExtensions};
    std::vector<const char *> _deviceLayers{deviceLayers};
    _deviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    _deviceExtensions.emplace_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    auto deviceExtProperties = m_adapterHandle->enumerateDeviceExtensionProperties();
    for (const auto &property : deviceExtProperties)
    {
        if (property.extensionName == VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)
        {
            _deviceExtensions.emplace_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }
    }
#endif

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{};
    queueCreateInfos.resize(1 + m_computeQueueHandle.has_value() + m_transferQueueHandle.has_value());
    queueCreateInfos[0].setQueueCount(1U).setQueueFamilyIndex(m_graphicsQueueHandle.queue_family_index).setQueuePriorities(m_graphicsQueueHandle.queue_priority);
    if (m_computeQueueHandle.has_value())
        queueCreateInfos[1].setQueueCount(1U).setQueueFamilyIndex(m_computeQueueHandle->queue_family_index).setQueuePriorities(m_computeQueueHandle->queue_priority);
    if (m_transferQueueHandle.has_value())
        queueCreateInfos[2].setQueueCount(1U).setQueueFamilyIndex(m_transferQueueHandle->queue_family_index).setQueuePriorities(m_transferQueueHandle->queue_priority);
    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.setQueueCreateInfos(queueCreateInfos)
        .setPEnabledExtensionNames(_deviceExtensions);
    if (!_deviceLayers.empty())
        deviceCreateInfo.setPEnabledLayerNames(_deviceLayers);
    deviceCreateInfo.setPNext(&adapterFeatures.get<vk::PhysicalDeviceFeatures2>());
    m_deviceHandle = std::make_shared<vk::Device>(m_adapterHandle->createDevice(deviceCreateInfo, allocationCallbacks));
    m_graphicsQueueHandle.queue_handle = std::make_shared<vk::Queue>(m_deviceHandle->getQueue(m_graphicsQueueHandle.queue_family_index, 0U));
    if (m_computeQueueHandle.has_value())
        m_computeQueueHandle->queue_handle = std::make_shared<vk::Queue>(m_deviceHandle->getQueue(m_computeQueueHandle->queue_family_index, 0U));
    if (m_transferQueueHandle.has_value())
        m_transferQueueHandle->queue_handle = std::make_shared<vk::Queue>(m_deviceHandle->getQueue(m_transferQueueHandle->queue_family_index, 0U));

    vk::CommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_family_index);
    m_internalTransferPool = m_deviceHandle->createCommandPool(poolCreateInfo, allocationCallbacks);

    vk::FenceCreateInfo fenceCreateInfo{};
    m_internalTransferFence = m_deviceHandle->createFence(fenceCreateInfo, allocationCallbacks);

    m_samplerPool.init(m_deviceHandle);

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = *m_adapterHandle;
    allocatorInfo.device = *m_deviceHandle;
    allocatorInfo.instance = *m_instanceHandle;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_vma);

    m_memAlloc = std::make_unique<MemoryAllocator>(m_adapterHandle, m_deviceHandle, m_vma);
    m_adapterHandle->getMemoryProperties(&m_memoryProperties);

    vk::PipelineCacheCreateInfo pipelineCacheCreateInfo{};
    m_pipelineCacheHandle = m_deviceHandle->createPipelineCache(pipelineCacheCreateInfo, allocationCallbacks);
}

std::shared_ptr<Buffer> RenderContext::createBuffer(const vk::BufferCreateInfo &info_,
                                                    const vk::MemoryPropertyFlags memUsage_)
{
    // Buffer resultBuffer;
    vk::Buffer bufferObject{};
    // Create Buffer (can be overloaded)
    createBufferEx(info_, bufferObject);

    // Find memory requirements
    vk::BufferMemoryRequirementsInfo2 bufferReqs{};
    vk::MemoryDedicatedRequirements dedicatedReqs{};
    vk::MemoryRequirements2 memReqs{};
    memReqs.pNext = &dedicatedReqs;
    bufferReqs.buffer = bufferObject;

    m_deviceHandle->getBufferMemoryRequirements2(&bufferReqs, &memReqs);

    // Build up allocation info
    MemoryAllocateInfo allocInfo(memReqs.memoryRequirements, memUsage_, false);

    if (info_.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        allocInfo.setAllocationFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
    if (dedicatedReqs.requiresDedicatedAllocation)
        allocInfo.setDedicatedBuffer(bufferObject);

    // Allocate memory
    auto memHandle = allocateMemory(allocInfo);
    if (memHandle)
    {
        const auto memInfo = m_memAlloc->getMemoryInfo(memHandle);
        // Bind memory to buffer
        m_deviceHandle->bindBufferMemory(bufferObject, memInfo.memory, memInfo.offset);
    }
    else
    {
        m_deviceHandle->destroyBuffer(bufferObject);
        m_memAlloc->freeMemory(memHandle);
    }

    return std::make_shared<Buffer>(bufferObject, memHandle, m_deviceHandle, m_memAlloc.get());
}

std::shared_ptr<Buffer> RenderContext::createBuffer(vk::DeviceSize size_,
                                                    vk::BufferUsageFlags usage_,
                                                    const vk::MemoryPropertyFlags memUsage_)
{
    vk::BufferCreateInfo info{};
    info.setSize(size_)
        .setUsage(usage_ | vk::BufferUsageFlagBits::eTransferDst);

    return createBuffer(info, memUsage_);
}

std::shared_ptr<Buffer> RenderContext::createBuffer(const vk::DeviceSize &size_,
                                                    const void *data_,
                                                    vk::BufferUsageFlags usage_,
                                                    const vk::MemoryPropertyFlags memUsage_)
{
    vk::BufferCreateInfo info{};
    info.setSize(size_)
        .setUsage(usage_ | vk::BufferUsageFlagBits::eTransferDst);
    auto resultBuffer = createBuffer(info, memUsage_);

    auto stagingBuffer = createBuffer(size_, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    auto mapped = stagingBuffer->map();
    memcpy(mapped, data_, size_);
    stagingBuffer->unmap();

    {
        auto scopedBuffer = allocateInternalTransferBuffer().front();
        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        scopedBuffer.begin(beginInfo);

        vk::BufferCopy region{};
        region.setSize(size_);
        scopedBuffer.copyBuffer(*stagingBuffer, *resultBuffer, region);

        scopedBuffer.end();
        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers(scopedBuffer);
        getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
        m_deviceHandle->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
        m_deviceHandle->resetFences(m_internalTransferFence);
    }

    return resultBuffer;
}

std::shared_ptr<Image> RenderContext::createImage(const vk::ImageCreateInfo &info_, const vk::MemoryPropertyFlags memUsage_)
{
    vk::Image imageObject;
    // Create image
    createImageEx(info_, imageObject);

    // Find memory requirements
    vk::ImageMemoryRequirementsInfo2 imageReqs{};
    vk::MemoryDedicatedRequirements dedicatedReqs{};
    vk::MemoryRequirements2 memReqs{};
    memReqs.pNext = &dedicatedReqs;
    imageReqs.image = imageObject;

    m_deviceHandle->getImageMemoryRequirements2(&imageReqs, &memReqs);

    // Build up allocation info
    MemoryAllocateInfo allocInfo(memReqs.memoryRequirements, memUsage_, true);
    if (dedicatedReqs.requiresDedicatedAllocation)
    {
        allocInfo.setDedicatedImage(imageObject);
    }

    // Allocate memory
    auto memHandle = allocateMemory(allocInfo);
    if (memHandle)
    {
        const auto memInfo = m_memAlloc->getMemoryInfo(memHandle);
        // Bind memory to image
        m_deviceHandle->bindImageMemory(imageObject, memInfo.memory, memInfo.offset);
    }
    else
    {
        m_deviceHandle->destroyImage(imageObject, allocationCallbacks);
        m_memAlloc->freeMemory(memHandle);
    }

    return std::make_shared<Image>(imageObject, memHandle, m_deviceHandle, m_memAlloc.get());
}

std::shared_ptr<Image> RenderContext::createImage(size_t size_,
                                                  const void *data_,
                                                  const vk::ImageCreateInfo &info_,
                                                  const vk::ImageLayout &layout_)
{
    auto resultImage = createImage(info_, vk::MemoryPropertyFlagBits::eDeviceLocal);

    // copy data by staging buffer
    auto stagingBuffer = createBuffer(size_, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    auto mapped = stagingBuffer->map();
    memcpy(mapped, data_, size_);
    stagingBuffer->unmap();
    {
        auto scopedBuffer = allocateInternalTransferBuffer(3);
        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        scopedBuffer[0].begin(beginInfo);
        cmdBarrierImageLayout(scopedBuffer[0], *resultImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        scopedBuffer[0].end();
        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers(scopedBuffer[0]);
        getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
        m_deviceHandle->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
        m_deviceHandle->resetFences(m_internalTransferFence);

        // TODO: support more copy aspects
        scopedBuffer[1].begin(beginInfo);
        vk::BufferImageCopy region{};
        region.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, info_.mipLevels, 0, info_.arrayLayers))
            .setImageExtent(info_.extent);
        scopedBuffer[1].copyBufferToImage(*stagingBuffer, *resultImage, vk::ImageLayout::eTransferDstOptimal, region);
        scopedBuffer[1].end();
        submitInfo.setCommandBuffers(scopedBuffer[1]);
        getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
        m_deviceHandle->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
        m_deviceHandle->resetFences(m_internalTransferFence);

        scopedBuffer[2].begin(beginInfo);
        cmdBarrierImageLayout(scopedBuffer[2], *resultImage, vk::ImageLayout::eTransferDstOptimal, layout_);
        scopedBuffer[2].end();
        submitInfo.setCommandBuffers(scopedBuffer[2]);
        getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
        m_deviceHandle->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
        m_deviceHandle->resetFences(m_internalTransferFence);
    }

    // // generate mipmaps
    // auto barrier = makeImageMemoryBarrier(image.image, vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eTransferRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor);
    // vk::DependencyInfo depInfo{};
    // depInfo.setImageMemoryBarriers(barrier);
    // vk::ImageBlit blit{};
    // blit.setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
    //     .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1));
    // {
    //     m_deviceHandle->resetCommandPool(m_internalTransferPool);
    //     vk::CommandBufferBeginInfo beginInfo{};
    //     beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    //     m_internalTransferBuffer.begin(beginInfo);

    //     auto mipWidth = desc.width, mipHeight = desc.height;
    //     for (auto i = 1; i < mipLevel; ++i)
    //     {
    //         barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
    //             .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
    //             .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
    //             .setDstAccessMask(vk::AccessFlagBits2::eTransferRead);
    //         barrier.subresourceRange.baseMipLevel = i - 1;
    //         m_internalTransferBuffer.pipelineBarrier2(depInfo);

    //         blit.srcOffsets[1] = vk::Offset3D{mipWidth, mipHeight, 1};
    //         blit.srcSubresource.mipLevel = i - 1;
    //         blit.dstOffsets[1] = vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
    //         blit.dstSubresource.mipLevel = i;

    //         m_internalTransferBuffer.blitImage(image.image, vk::ImageLayout::eTransferSrcOptimal,
    //                                            image.image, vk::ImageLayout::eTransferDstOptimal,
    //                                            blit, vk::Filter::eLinear);

    //         barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
    //             .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
    //             .setSrcAccessMask(vk::AccessFlagBits2::eTransferRead)
    //             .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite);
    //         m_internalTransferBuffer.pipelineBarrier2(depInfo);

    //         if (mipWidth > 1)
    //             mipWidth >>= 1;
    //         if (mipHeight > 1)
    //             mipHeight >>= 1;
    //     }
    //     barrier.subresourceRange.baseMipLevel = mipLevel - 1;
    //     barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
    //         .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    //         .setSrcAccessMask(vk::AccessFlagBits2::eTransferRead)
    //         .setDstAccessMask(vk::AccessFlagBits2::eShaderRead);
    //     m_internalTransferBuffer.pipelineBarrier2(depInfo);

    //     m_internalTransferBuffer.end();
    //     vk::SubmitInfo submitInfo{};
    //     submitInfo.setCommandBuffers(m_internalTransferBuffer);
    //     getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
    //     m_deviceHandle->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
    // }

    return resultImage;
}

std::shared_ptr<Texture> RenderContext::createTexture(const vk::ImageCreateInfo &info_,
                                                      const vk::ImageLayout &layout_,
                                                      bool isCube,
                                                      const vk::MemoryPropertyFlags memUsage_)
{
    // step 1: create Image
    vk::Image imageObject;
    // Create image
    createImageEx(info_, imageObject);

    // Find memory requirements
    vk::ImageMemoryRequirementsInfo2 imageReqs{};
    vk::MemoryDedicatedRequirements dedicatedReqs{};
    vk::MemoryRequirements2 memReqs{};
    memReqs.pNext = &dedicatedReqs;
    imageReqs.image = imageObject;

    m_deviceHandle->getImageMemoryRequirements2(&imageReqs, &memReqs);

    // Build up allocation info
    MemoryAllocateInfo allocInfo(memReqs.memoryRequirements, memUsage_, true);
    if (dedicatedReqs.requiresDedicatedAllocation)
    {
        allocInfo.setDedicatedImage(imageObject);
    }

    // Allocate memory
    auto memHandle = allocateMemory(allocInfo);
    if (memHandle)
    {
        const auto memInfo = m_memAlloc->getMemoryInfo(memHandle);
        // Bind memory to image
        m_deviceHandle->bindImageMemory(imageObject, memInfo.memory, memInfo.offset);
    }
    else
    {
        m_deviceHandle->destroyImage(imageObject, allocationCallbacks);
        m_memAlloc->freeMemory(memHandle);
    }

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(imageObject)
        .setFormat(info_.format)
        .setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
    switch (info_.imageType)
    {
    case vk::ImageType::e1D:
        viewInfo.viewType = (info_.arrayLayers > 1 ? vk::ImageViewType::e1DArray : vk::ImageViewType::e1D);
        break;
    case vk::ImageType::e2D:
        viewInfo.viewType = isCube ? vk::ImageViewType::eCube : (info_.arrayLayers > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D);
        break;
    case vk::ImageType::e3D:
        viewInfo.viewType = vk::ImageViewType::e3D;
        break;
    default:
        assert(0);
    }

    vk::DescriptorImageInfo descInfo{};
    descInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    descInfo.imageView = m_deviceHandle->createImageView(viewInfo, allocationCallbacks);

    return std::make_shared<Texture>(imageObject, memHandle, descInfo, m_deviceHandle, m_memAlloc.get(), &m_samplerPool);
}

std::shared_ptr<Texture> RenderContext::createTexture(const vk::ImageCreateInfo &info_,
                                                      const vk::SamplerCreateInfo &samplerCreateInfo,
                                                      const vk::ImageLayout &layout_,
                                                      bool isCube,
                                                      const vk::MemoryPropertyFlags memUsage_)
{
    auto resultTexture = createTexture(info_, layout_, isCube, memUsage_);
    resultTexture->descriptor.sampler = m_samplerPool.acquireSampler(samplerCreateInfo);

    return resultTexture;
}

std::shared_ptr<Texture> RenderContext::createTexture(size_t size_,
                                                      const void *data_,
                                                      const vk::ImageCreateInfo &info_,
                                                      const vk::SamplerCreateInfo &samplerCreateInfo,
                                                      const vk::ImageLayout &layout_,
                                                      bool isCube)
{
    auto resultTexture = createTexture(info_, layout_, isCube);

    // copy data by staging buffer
    auto stagingBuffer = createBuffer(size_, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    auto mapped = stagingBuffer->map();
    memcpy(mapped, data_, size_);
    stagingBuffer->unmap();
    {
        auto scopedBuffer = allocateInternalTransferBuffer(3);
        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        scopedBuffer[0].begin(beginInfo);
        cmdBarrierImageLayout(scopedBuffer[0], *resultTexture, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        scopedBuffer[0].end();
        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers(scopedBuffer[0]);
        getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
        m_deviceHandle->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
        m_deviceHandle->resetFences(m_internalTransferFence);

        // TODO: support more copy aspects
        scopedBuffer[1].begin(beginInfo);
        vk::BufferImageCopy region{};
        region.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, info_.mipLevels, 0, info_.arrayLayers))
            .setImageExtent(info_.extent);
        scopedBuffer[1].copyBufferToImage(*stagingBuffer, *resultTexture, vk::ImageLayout::eTransferDstOptimal, region);
        scopedBuffer[1].end();
        getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
        m_deviceHandle->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
        m_deviceHandle->resetFences(m_internalTransferFence);

        scopedBuffer[2].begin(beginInfo);
        cmdBarrierImageLayout(scopedBuffer[2], *resultTexture, vk::ImageLayout::eTransferDstOptimal, layout_);
        scopedBuffer[2].end();
        submitInfo.setCommandBuffers(scopedBuffer[2]);
        getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
        m_deviceHandle->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
        m_deviceHandle->resetFences(m_internalTransferFence);
    }

    return resultTexture;
}

std::shared_ptr<QueueInstance> RenderContext::getQueueInstanceHandle(vk::QueueFlagBits type, bool mustSeparate) const
{
    switch (type)
    {
    case vk::QueueFlagBits::eGraphics:
        return std::make_shared<QueueInstance>(m_graphicsQueueHandle);
    case vk::QueueFlagBits::eCompute:
        if (m_computeQueueHandle.has_value())
            return std::make_shared<QueueInstance>(m_computeQueueHandle.value());
        else if (!mustSeparate)
            return std::make_shared<QueueInstance>(m_graphicsQueueHandle);
        else
            throw std::logic_error("Failed to get compute queue instance: It has not been initialized!");
    case vk::QueueFlagBits::eTransfer:
        if (m_transferQueueHandle.has_value())
            return std::make_shared<QueueInstance>(m_transferQueueHandle.value());
        else if (!mustSeparate)
            return std::make_shared<QueueInstance>(m_graphicsQueueHandle);
        else
            throw std::logic_error("Failed to get transfer queue instance: It has not been initialized!");
    default:
        throw std::logic_error("Failed to get queue instance: Currently not supported queue type!");
    }
}

void RenderContext::destroy()
{
    m_memAlloc.reset();
    vmaDestroyAllocator(m_vma);

    if (m_deviceHandle)
    {
        m_deviceHandle->destroyPipelineCache(m_pipelineCacheHandle, allocationCallbacks);
        m_deviceHandle->destroyFence(m_internalTransferFence, allocationCallbacks);
        m_deviceHandle->destroyCommandPool(m_internalTransferPool, allocationCallbacks);
        m_deviceHandle->destroy(allocationCallbacks);
    }
#ifdef NDEBUG
#if defined(VK_EXT_debug_utils)
    m_instanceHandle->destroy(m_debugMessenger, allocationCallbacks);
#else if defined(VK_EXT_debug_report)
    m_instanceHandle->destroy(m_debugReporter, allocationCallbacks);
#endif
#endif
    if (m_instanceHandle)
        m_instanceHandle->destroy(allocationCallbacks);
}

MemoryHandle RenderContext::allocateMemory(const MemoryAllocateInfo &allocateInfo)
{
    return m_memAlloc->allocMemory(allocateInfo).value;
}

void RenderContext::createBufferEx(const vk::BufferCreateInfo &info_, vk::Buffer &buffer)
{
    buffer = m_deviceHandle->createBuffer(info_, allocationCallbacks);
}

void RenderContext::createImageEx(const vk::ImageCreateInfo &info_, vk::Image &image)
{
    image = m_deviceHandle->createImage(info_, allocationCallbacks);
}

std::vector<vk::CommandBuffer> RenderContext::allocateInternalTransferBuffer(size_t count)
{
    m_deviceHandle->resetCommandPool(m_internalTransferPool);
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(m_internalTransferPool).setCommandBufferCount(count).setLevel(vk::CommandBufferLevel::ePrimary);
    return m_deviceHandle->allocateCommandBuffers(allocInfo);
}
#include <iomanip>

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "applicationBase.hpp"

struct alignas(16) ScanlineAttribute
{
    glm::vec3 faceNormal;
    float xStart;
    float xEnd;
    int32_t y;
    float zStart;
    float dzdx;
};

constexpr size_t calWorkGroupCount(const size_t &renderSize, const size_t &threadSize)
{
    return (renderSize + threadSize - 1) / threadSize;
}

void ApplicationBase::reloadModel(const std::filesystem::path &filePath)
{
    if (m_vertexBuffer)
        m_vertexBuffer.reset();
    if (m_indexBuffer)
        m_indexBuffer.reset();
    if (m_scanlineBuffer)
        m_scanlineBuffer.reset();
    if (m_hiZOutputVertexBuffer)
        m_hiZOutputVertexBuffer.reset();

    auto [vertices, indices, box] = loadModel(filePath);
    m_vertexBuffer = m_renderContext.createBuffer(vertices, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
    m_indexBuffer = m_renderContext.createBuffer(indices, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
    m_vertexCount = vertices.size();
    m_triangleCount = indices.size() / 3;
    m_bounding = box;
    m_mainCamera.fit(box, glm::mat4(1.f), true, false, (float)m_size.width / m_size.height);

    // create scanline required buffers
    m_scanlineBuffer = m_renderContext.createBuffer(sizeof(ScanlineAttribute) * 1024 * (m_triangleCount / glm::length(box.getExtent())), vk::BufferUsageFlagBits::eStorageBuffer);

    // create hi-z required buffers
    m_hiZOutputVertexBuffer = m_renderContext.createBuffer(sizeof(glm::vec4) * indices.size(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);

    // update geometry descriptors
    std::vector<vk::WriteDescriptorSet> writeDescs{};
    writeDescs.resize(4);
    vk::DescriptorBufferInfo vertexBufferInfo{*m_vertexBuffer, 0ULL, VK_WHOLE_SIZE};
    vk::DescriptorBufferInfo indexBufferInfo{*m_indexBuffer, 0ULL, VK_WHOLE_SIZE};
    writeDescs[0].setDstSet(m_geometrySet).setDstBinding(0).setDescriptorCount(1U).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(vertexBufferInfo);
    writeDescs[1].setDstSet(m_geometrySet).setDstBinding(1).setDescriptorCount(1U).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(indexBufferInfo);
    vk::DescriptorBufferInfo scanlineBufferInfo{*m_scanlineBuffer, 0ULL, VK_WHOLE_SIZE};
    writeDescs[2].setDstSet(m_scanlineSet).setDstBinding(0).setDescriptorCount(1U).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(scanlineBufferInfo);
    vk::DescriptorBufferInfo hiZOutputVertexBufferInfo{*m_hiZOutputVertexBuffer, 0ULL, VK_WHOLE_SIZE};
    writeDescs[3].setDstSet(m_hiZOutputSet).setDstBinding(0).setDescriptorCount(1U).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(hiZOutputVertexBufferInfo);
    m_renderContext.getDeviceHandle()->updateDescriptorSets(writeDescs, {});
}

void ApplicationBase::recreateRenderTargets()
{
    if (m_zBuffer)
        m_zBuffer.reset();
    for (auto i = 0; i < m_zBufferMipViews.size(); ++i)
        if (m_zBufferMipViews[i])
            m_renderContext.getDeviceHandle()->destroy(m_zBufferMipViews[i], allocationCallbacks);
    if (m_colorBuffer)
        m_colorBuffer.reset();
    if (m_colorBufferView)
        m_renderContext.getDeviceHandle()->destroy(m_colorBufferView, allocationCallbacks);
    if (m_scanlineBufferSpinlock)
        m_scanlineBufferSpinlock.reset();
    if (m_scanlineBufferSpinlockView)
        m_renderContext.getDeviceHandle()->destroy(m_scanlineBufferSpinlockView, allocationCallbacks);
    if (m_emptyBuffer)
        m_emptyBuffer.reset();
    if (m_emptyBufferView)
        m_renderContext.getDeviceHandle()->destroy(m_emptyBufferView, allocationCallbacks);

    m_pushConstants.mipLevelCount = static_cast<uint32_t>(std::floor(std::log2(std::max(m_mainWindow.Width, m_mainWindow.Height)))) + 1;
    vk::ImageCreateInfo imageCreateInfo{};
    imageCreateInfo.setImageType(vk::ImageType::e2D)
        .setFormat(vk::Format::eR32Sfloat)
        .setExtent(vk::Extent3D{m_size.width, m_size.height, 1U})
        .setMipLevels(m_pushConstants.mipLevelCount)
        .setArrayLayers(1U)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    m_zBuffer = m_renderContext.createImage(imageCreateInfo);
    imageCreateInfo.setFormat(vk::Format::eR8G8B8A8Unorm).setMipLevels(1U);
    m_colorBuffer = m_renderContext.createImage(imageCreateInfo);
    imageCreateInfo.setFormat(vk::Format::eR32Uint);
    m_scanlineBufferSpinlock = m_renderContext.createImage(imageCreateInfo);
    imageCreateInfo.setFormat(vk::Format::eR32Sfloat).setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst);
    m_emptyBuffer = m_renderContext.createImage(imageCreateInfo);

    m_zBufferMipViews.resize(m_pushConstants.mipLevelCount);
    vk::ImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.setImage(*m_zBuffer)
        .setFormat(vk::Format::eR32Sfloat)
        .setViewType(vk::ImageViewType::e2D)
        .setComponents(vk::ComponentSwizzle::eIdentity)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS));
    for (auto i = 0; i < m_pushConstants.mipLevelCount; ++i)
    {
        viewCreateInfo.subresourceRange.baseMipLevel = i;
        m_zBufferMipViews[i] = m_renderContext.getDeviceHandle()->createImageView(viewCreateInfo, allocationCallbacks);
    }
    viewCreateInfo.setImage(*m_colorBuffer).setFormat(vk::Format::eR8G8B8A8Unorm).subresourceRange.baseMipLevel = 0;
    m_colorBufferView = m_renderContext.getDeviceHandle()->createImageView(viewCreateInfo, allocationCallbacks);
    viewCreateInfo.setImage(*m_scanlineBufferSpinlock).setFormat(vk::Format::eR32Uint).subresourceRange.baseMipLevel = 0;
    m_scanlineBufferSpinlockView = m_renderContext.getDeviceHandle()->createImageView(viewCreateInfo, allocationCallbacks);
    viewCreateInfo.setImage(*m_emptyBuffer).setFormat(vk::Format::eR32Sfloat).subresourceRange.baseMipLevel = 0;
    m_emptyBufferView = m_renderContext.getDeviceHandle()->createImageView(viewCreateInfo, allocationCallbacks);

    // prepare for updating descriptors
    std::vector<vk::DescriptorImageInfo> imageDescs{};
    imageDescs.reserve(m_pushConstants.mipLevelCount);
    for (auto i = 0; i < m_pushConstants.mipLevelCount; ++i)
        imageDescs.emplace_back(vk::Sampler{}, m_zBufferMipViews[i], vk::ImageLayout::eGeneral);
    std::vector<vk::WriteDescriptorSet> writeDescs{};
    writeDescs.reserve(g_predefMaxMipLevel + 3);
    for (auto i = 0; i < g_predefMaxMipLevel; ++i)
        writeDescs.emplace_back(m_zBufferSet, 0, i, vk::DescriptorType::eStorageImage, i < m_pushConstants.mipLevelCount ? imageDescs[i] : imageDescs[m_pushConstants.mipLevelCount - 1]);
    vk::DescriptorImageInfo spinlockInfo{vk::Sampler{}, m_scanlineBufferSpinlockView, vk::ImageLayout::eGeneral};
    writeDescs.emplace_back(m_scanlineSet, 2, 0, vk::DescriptorType::eStorageImage, spinlockInfo);
    vk::DescriptorImageInfo colorInfo{vk::Sampler{}, m_colorBufferView, vk::ImageLayout::eGeneral};
    writeDescs.emplace_back(m_scanlineSet, 3, 0, vk::DescriptorType::eStorageImage, colorInfo);
    vk::DescriptorImageInfo emptyInfo{vk::Sampler{}, m_emptyBufferView, vk::ImageLayout::eGeneral};
    writeDescs.emplace_back(m_hiZOutputSet, 2, 0, vk::DescriptorType::eStorageImage, emptyInfo);
    m_renderContext.getDeviceHandle()->updateDescriptorSets(writeDescs, {});

    // init image layout
    m_renderContext.getDeviceHandle()->resetCommandPool(m_internalTransferPool);
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(m_internalTransferPool).setCommandBufferCount(1U).setLevel(vk::CommandBufferLevel::ePrimary);
    auto transBuffer = m_renderContext.getDeviceHandle()->allocateCommandBuffers(allocInfo).front();
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    transBuffer.begin(beginInfo);

    auto barrierBase = makeImageMemoryBarrier(*m_zBuffer, accessFlagsForImageLayout(vk::ImageLayout::eUndefined), accessFlagsForImageLayout(vk::ImageLayout::eGeneral),
                                              vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                                              vk::ImageAspectFlagBits::eColor)
                           .setSrcStageMask(pipelineStageForLayout(vk::ImageLayout::eUndefined))
                           .setDstStageMask(pipelineStageForLayout(vk::ImageLayout::eGeneral));
    std::vector<vk::ImageMemoryBarrier2> barriers;
    barriers.reserve(g_predefMaxMipLevel + 3);
    for (auto i = 0; i < g_predefMaxMipLevel; ++i)
    {
        barrierBase.subresourceRange.baseMipLevel = i < m_pushConstants.mipLevelCount ? i : m_pushConstants.mipLevelCount - 1;
        barriers.emplace_back(barrierBase);
    }
    barrierBase.subresourceRange.baseMipLevel = 0;
    barrierBase.setImage(*m_scanlineBufferSpinlock);
    barriers.emplace_back(barrierBase);
    barrierBase.setImage(*m_colorBuffer);
    barriers.emplace_back(barrierBase);
    barrierBase.setImage(*m_emptyBuffer);
    barriers.emplace_back(barrierBase);

    vk::DependencyInfo depInfo{};
    depInfo.setImageMemoryBarriers(barriers);
    transBuffer.pipelineBarrier2(depInfo);

    transBuffer.end();
    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(transBuffer);
    m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
    m_renderContext.getDeviceHandle()->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
    m_renderContext.getDeviceHandle()->resetFences(m_internalTransferFence);
}

void ApplicationBase::createStaticResources()
{
    m_scanlineGlobalPropertyBuffer = m_renderContext.createBuffer(sizeof(glm::uvec4), vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
    vk::DescriptorBufferInfo globalBufferInfo{*m_scanlineGlobalPropertyBuffer, 0ULL, sizeof(glm::uvec4)};

    m_hiZIndirectRenderBuffer = m_renderContext.createBuffer(sizeof(glm::uvec4), vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
    vk::DescriptorBufferInfo hiZIndirectBufferInfo{*m_hiZIndirectRenderBuffer, 0ULL, sizeof(glm::uvec4)};

    vk::ImageCreateInfo imageCreateInfo{};
    imageCreateInfo.setImageType(vk::ImageType::e3D)
        .setFormat(vk::Format::eR32Uint)
        .setExtent(vk::Extent3D{1ULL << (m_octreeLevelCount - 1), 1ULL << (m_octreeLevelCount - 1), 1ULL << (m_octreeLevelCount - 1)})
        .setMipLevels(m_octreeLevelCount - m_octreeStartLevel)
        .setArrayLayers(1U)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    m_octreeLinkHeader = m_renderContext.createImage(imageCreateInfo);
    m_octreeMarker = m_renderContext.createImage(imageCreateInfo);

    m_octreeLinkHeaderMipViews.resize(imageCreateInfo.mipLevels);
    m_octreeMarkerMipViews.resize(imageCreateInfo.mipLevels);
    vk::ImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.setImage(*m_octreeLinkHeader)
        .setFormat(vk::Format::eR32Uint)
        .setViewType(vk::ImageViewType::e3D)
        .setComponents(vk::ComponentSwizzle::eIdentity)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS));
    for (auto i = 0; i < imageCreateInfo.mipLevels; ++i)
    {
        viewCreateInfo.subresourceRange.baseMipLevel = i;
        m_octreeLinkHeaderMipViews[i] = m_renderContext.getDeviceHandle()->createImageView(viewCreateInfo, allocationCallbacks);
    }
    viewCreateInfo.setImage(*m_octreeMarker);
    for (auto i = 0; i < imageCreateInfo.mipLevels; ++i)
    {
        viewCreateInfo.subresourceRange.baseMipLevel = i;
        m_octreeMarkerMipViews[i] = m_renderContext.getDeviceHandle()->createImageView(viewCreateInfo, allocationCallbacks);
    }

    m_faceIndicesOfOctree = m_renderContext.createBuffer(sizeof(glm::uvec2) * 256 * 1024 * 1024, vk::BufferUsageFlagBits::eStorageBuffer);
    std::vector<glm::vec4> globalPropertyInitial = {{m_bounding.minPoint, 0}, {m_bounding.maxPoint, 0}};
    vk::DescriptorBufferInfo faceIndicesInfo{*m_faceIndicesOfOctree, 0ULL, VK_WHOLE_SIZE};

    std::vector<vk::WriteDescriptorSet> writeDescs(3);
    writeDescs[0].setDstSet(m_scanlineSet).setDstBinding(1U).setDescriptorType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(1U).setBufferInfo(globalBufferInfo);
    writeDescs[1].setDstSet(m_hiZOutputSet).setDstBinding(1U).setDescriptorType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(1U).setBufferInfo(hiZIndirectBufferInfo);
    writeDescs[2].setDstSet(m_octreeSet).setDstBinding(2).setDescriptorCount(1U).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(faceIndicesInfo);
    m_renderContext.getDeviceHandle()->updateDescriptorSets(writeDescs, {});

    std::vector<vk::DescriptorImageInfo> imageDescs{};
    imageDescs.resize(imageCreateInfo.mipLevels * 2);
    writeDescs.resize(imageCreateInfo.mipLevels * 2);
    for (auto i = 0; i < imageCreateInfo.mipLevels; ++i)
    {
        imageDescs[i] = {vk::Sampler{}, m_octreeLinkHeaderMipViews[i], vk::ImageLayout::eGeneral};
        imageDescs[i + imageCreateInfo.mipLevels] = {vk::Sampler{}, m_octreeMarkerMipViews[i], vk::ImageLayout::eGeneral};
        writeDescs[i].setDstSet(m_octreeSet).setDstBinding(0).setDstArrayElement(i).setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eStorageImage).setImageInfo(imageDescs[i]);
        writeDescs[i + imageCreateInfo.mipLevels].setDstSet(m_octreeSet).setDstBinding(1).setDstArrayElement(i).setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eStorageImage).setImageInfo(imageDescs[i + imageCreateInfo.mipLevels]);
    }
    m_renderContext.getDeviceHandle()->updateDescriptorSets(writeDescs, {});

    // init image layout
    m_renderContext.getDeviceHandle()->resetCommandPool(m_internalTransferPool);
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(m_internalTransferPool).setCommandBufferCount(1U).setLevel(vk::CommandBufferLevel::ePrimary);
    auto transBuffer = m_renderContext.getDeviceHandle()->allocateCommandBuffers(allocInfo).front();
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    transBuffer.begin(beginInfo);

    cmdBarrierImageLayout(transBuffer, *m_octreeLinkHeader, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    cmdBarrierImageLayout(transBuffer, *m_octreeMarker, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

    transBuffer.end();
    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(transBuffer);
    m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_handle->submit(submitInfo, m_internalTransferFence);
    m_renderContext.getDeviceHandle()->waitForFences(m_internalTransferFence, VK_TRUE, UINT64_MAX);
    m_renderContext.getDeviceHandle()->resetFences(m_internalTransferFence);
}

void ApplicationBase::createRenderer()
{
    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.emplace_back(vk::DescriptorType::eStorageImage, 16U);
    poolSizes.emplace_back(vk::DescriptorType::eStorageBuffer, 16U);
    vk::DescriptorPoolCreateInfo descPoolCreateInfo;
    descPoolCreateInfo.setMaxSets(8U)
        .setPoolSizes(poolSizes);
    m_descPool = m_renderContext.getDeviceHandle()->createDescriptorPool(descPoolCreateInfo, allocationCallbacks);

    std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings{};
    vk::DescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
    setLayoutBindings.emplace_back(0, vk::DescriptorType::eStorageImage, g_predefMaxMipLevel, vk::ShaderStageFlagBits::eAll);
    setLayoutCreateInfo.setBindings(setLayoutBindings);
    m_zBufferSetLayout = m_renderContext.getDeviceHandle()->createDescriptorSetLayout(setLayoutCreateInfo, allocationCallbacks);
    setLayoutBindings.clear();
    setLayoutBindings.emplace_back(0, vk::DescriptorType::eStorageBuffer, 1U, vk::ShaderStageFlagBits::eAll);
    setLayoutBindings.emplace_back(1, vk::DescriptorType::eStorageBuffer, 1U, vk::ShaderStageFlagBits::eAll);
    setLayoutCreateInfo.setBindings(setLayoutBindings);
    m_geometrySetLayout = m_renderContext.getDeviceHandle()->createDescriptorSetLayout(setLayoutCreateInfo, allocationCallbacks);
    setLayoutBindings.clear();
    setLayoutBindings.emplace_back(0, vk::DescriptorType::eStorageBuffer, 1U, vk::ShaderStageFlagBits::eAll);
    setLayoutBindings.emplace_back(1, vk::DescriptorType::eStorageBuffer, 1U, vk::ShaderStageFlagBits::eAll);
    setLayoutBindings.emplace_back(2, vk::DescriptorType::eStorageImage, 1U, vk::ShaderStageFlagBits::eAll);
    setLayoutBindings.emplace_back(3, vk::DescriptorType::eStorageImage, 1U, vk::ShaderStageFlagBits::eAll);
    setLayoutCreateInfo.setBindings(setLayoutBindings);
    m_scanlineSetLayout = m_renderContext.getDeviceHandle()->createDescriptorSetLayout(setLayoutCreateInfo, allocationCallbacks);
    setLayoutBindings.clear();
    setLayoutBindings.emplace_back(0, vk::DescriptorType::eStorageBuffer, 1U, vk::ShaderStageFlagBits::eAll);
    setLayoutBindings.emplace_back(1, vk::DescriptorType::eStorageBuffer, 1U, vk::ShaderStageFlagBits::eAll);
    setLayoutBindings.emplace_back(2, vk::DescriptorType::eStorageImage, 1U, vk::ShaderStageFlagBits::eAll);
    m_hiZOutputSetLayout = m_renderContext.getDeviceHandle()->createDescriptorSetLayout(setLayoutCreateInfo, allocationCallbacks);
    setLayoutBindings.clear();
    setLayoutBindings.emplace_back(0, vk::DescriptorType::eStorageImage, m_octreeLevelCount - m_octreeStartLevel, vk::ShaderStageFlagBits::eAll);
    setLayoutBindings.emplace_back(1, vk::DescriptorType::eStorageImage, m_octreeLevelCount - m_octreeStartLevel, vk::ShaderStageFlagBits::eAll);
    setLayoutBindings.emplace_back(2, vk::DescriptorType::eStorageBuffer, 1U, vk::ShaderStageFlagBits::eAll);
    m_octreeSetLayout = m_renderContext.getDeviceHandle()->createDescriptorSetLayout(setLayoutCreateInfo, allocationCallbacks);

    std::vector setLayoutContainer = {m_zBufferSetLayout, m_geometrySetLayout, m_scanlineSetLayout, m_hiZOutputSetLayout, m_octreeSetLayout};
    vk::DescriptorSetAllocateInfo setAllocInfo{};
    setAllocInfo.setDescriptorPool(m_descPool).setSetLayouts(setLayoutContainer);
    auto allocatedSets = m_renderContext.getDeviceHandle()->allocateDescriptorSets(setAllocInfo);
    m_zBufferSet = allocatedSets[0];
    m_geometrySet = allocatedSets[1];
    m_scanlineSet = allocatedSets[2];
    m_hiZOutputSet = allocatedSets[3];
    m_octreeSet = allocatedSets[4];

    vk::CommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eTransfer, false)->queue_family_index);
    m_internalTransferPool = m_renderContext.getDeviceHandle()->createCommandPool(poolCreateInfo, allocationCallbacks);

    vk::FenceCreateInfo fenceCreateInfo{};
    m_internalTransferFence = m_renderContext.getDeviceHandle()->createFence(fenceCreateInfo, allocationCallbacks);

    reloadModel("./resources/models/cgaxis_107_11_cafe_stall_obj.obj");
    // reloadModel("./resources/models/6.837.obj");
    // reloadModel("./resources/models/bunny_1k.obj");
    recreateRenderTargets();
    createStaticResources();

    vk::PushConstantRange pushConstants = {vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0ULL, sizeof(PushConstants)};
    vk::PipelineLayoutCreateInfo layoutCreateInfo{};
    setLayoutContainer = {m_geometrySetLayout, m_zBufferSetLayout};
    layoutCreateInfo.setSetLayouts(setLayoutContainer)
        .setPushConstantRanges(pushConstants);
    m_naiveZBufferPipelineLayout = m_renderContext.getDeviceHandle()->createPipelineLayout(layoutCreateInfo, allocationCallbacks);
    layoutCreateInfo.setSetLayouts(m_zBufferSetLayout);
    m_zPrepassPipelinelayout = m_renderContext.getDeviceHandle()->createPipelineLayout(layoutCreateInfo, allocationCallbacks);
    pushConstants.setStageFlags(vk::ShaderStageFlagBits::eCompute);
    setLayoutContainer = {m_geometrySetLayout, m_scanlineSetLayout, m_zBufferSetLayout};
    layoutCreateInfo.setSetLayouts(setLayoutContainer)
        .setPushConstantRanges(pushConstants);
    m_scanlineZBufferPipelineLayout = m_renderContext.getDeviceHandle()->createPipelineLayout(layoutCreateInfo, allocationCallbacks);
    layoutCreateInfo.setSetLayouts(m_zBufferSetLayout);
    m_zBufferMipMappingPipelineLayout = m_renderContext.getDeviceHandle()->createPipelineLayout(layoutCreateInfo, allocationCallbacks);
    setLayoutContainer = {m_geometrySetLayout, m_octreeSetLayout, m_zBufferSetLayout};
    layoutCreateInfo.setSetLayouts(setLayoutContainer);
    m_octreeInitPipelineLayout = m_renderContext.getDeviceHandle()->createPipelineLayout(layoutCreateInfo, allocationCallbacks);
    setLayoutContainer = {m_geometrySetLayout, m_zBufferSetLayout, m_hiZOutputSetLayout, m_octreeSetLayout};
    pushConstants.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute);
    layoutCreateInfo.setSetLayouts(setLayoutContainer)
        .setPushConstantRanges(pushConstants);
    m_hiZBufferOutputPipelineLayout = m_renderContext.getDeviceHandle()->createPipelineLayout(layoutCreateInfo, allocationCallbacks);
    pushConstants.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    layoutCreateInfo.setSetLayouts({});
    m_defaultPipelineLayout = m_renderContext.getDeviceHandle()->createPipelineLayout(layoutCreateInfo, allocationCallbacks);
    layoutCreateInfo.setSetLayouts(m_scanlineSetLayout)
        .setPushConstantRanges({});
    m_blitPipelineLayout = m_renderContext.getDeviceHandle()->createPipelineLayout(layoutCreateInfo, allocationCallbacks);

    ComputePipelineHelper computeHelper(m_renderContext.getDeviceHandle(), m_scanlineZBufferPipelineLayout);
    computeHelper.setShader(loadFile("./resources/shaders/compiled/scanlineZBufferInit.comp.spv", true));
    m_scanlineZBufferInitPipeline = computeHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());
    computeHelper.setShader(loadFile("./resources/shaders/compiled/scanlineZBufferWork.comp.spv", true));
    m_scanlineZBufferWorkPipeline = computeHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());
    computeHelper.setLayout(m_zBufferMipMappingPipelineLayout);
    computeHelper.setShader(loadFile("./resources/shaders/compiled/zBufferMipMapper.comp.spv", true));
    m_zBufferMipMappingPipeline = computeHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());
    computeHelper.setLayout(m_octreeInitPipelineLayout);
    computeHelper.setShader(loadFile("./resources/shaders/compiled/octreeInit.comp.spv", true));
    m_octreeInitPipeline = computeHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());
    computeHelper.setLayout(m_hiZBufferOutputPipelineLayout);
    computeHelper.setShader(loadFile("./resources/shaders/compiled/naiveHiZBufferWork.comp.spv", true));
    m_naiveHiZBufferWorkPipeline = computeHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());
    computeHelper.setShader(loadFile("./resources/shaders/compiled/optimHiZBufferWork.comp.spv", true));
    m_optimHiZBufferWorkPipeline = computeHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());

    GraphicsPipelineHelper graphicsHelper(m_renderContext.getDeviceHandle(), m_defaultPipelineLayout, m_mainWindow.RenderPass);
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/raster.vert.spv", true), vk::ShaderStageFlagBits::eVertex);
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/raster.frag.spv", true), vk::ShaderStageFlagBits::eFragment);
    graphicsHelper.addBindingDescription(graphicsHelper.makeVertexInputBinding(0, sizeof(glm::vec4)));
    graphicsHelper.addAttributeDescription(graphicsHelper.makeVertexInputAttribute(0, 0, vk::Format::eR32G32B32A32Sfloat, 0));
    graphicsHelper.rasterizationState.setPolygonMode(vk::PolygonMode::eLine);
    m_defaultFramePipeline = graphicsHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());

    graphicsHelper.setLayout(m_naiveZBufferPipelineLayout);
    graphicsHelper.clearShaders();
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/raster.vert.spv", true), vk::ShaderStageFlagBits::eVertex);
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/naiveZBuffer.frag.spv", true), vk::ShaderStageFlagBits::eFragment);
    graphicsHelper.rasterizationState.setPolygonMode(vk::PolygonMode::eFill);
    graphicsHelper.depthStencilState.setDepthTestEnable(VK_FALSE)
        .setDepthWriteEnable(VK_FALSE)
        .setStencilTestEnable(VK_FALSE);
    m_naiveZBufferPipeline = graphicsHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());

    graphicsHelper.setLayout(m_hiZBufferOutputPipelineLayout);
    graphicsHelper.clearShaders();
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/raster.vert.spv", true), vk::ShaderStageFlagBits::eVertex);
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/hiZPostRender.frag.spv", true), vk::ShaderStageFlagBits::eFragment);
    m_hiZBufferPostRenderPipeline = graphicsHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());

    vk::PipelineRenderingCreateInfo renderingInfo{};
    graphicsHelper.setLayout(m_zPrepassPipelinelayout);
    graphicsHelper.setRenderPass({});
    graphicsHelper.clearShaders();
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/zPrepass.vert.spv", true), vk::ShaderStageFlagBits::eVertex);
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/zPrepass.frag.spv", true), vk::ShaderStageFlagBits::eFragment);
    graphicsHelper.setPipelineRenderingCreateInfo(renderingInfo);
    m_zPrepassPipeline = graphicsHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());

    graphicsHelper.setLayout(m_blitPipelineLayout);
    graphicsHelper.setRenderPass(m_mainWindow.RenderPass);
    graphicsHelper.clearShaders();
    graphicsHelper.clearBindingDescriptions();
    graphicsHelper.clearAttributeDescriptions();
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/screenQuad.vert.spv", true), vk::ShaderStageFlagBits::eVertex);
    graphicsHelper.addShader(loadFile("./resources/shaders/compiled/blit.frag.spv", true), vk::ShaderStageFlagBits::eFragment);
    graphicsHelper.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
    m_blitPipeline = graphicsHelper.createPipeline(*m_renderContext.getPipelineCacheHandle());

    m_shaderRWBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eAllGraphics)
        .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eAllGraphics)
        .setSrcAccessMask(vk::AccessFlagBits2::eShaderWrite)
        .setDstAccessMask(vk::AccessFlagBits2::eShaderRead);

    m_imageClearBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
        .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eAllGraphics)
        .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits2::eShaderRead);

    m_zPrepassRenderingInfo.setLayerCount(1U);
}

void ApplicationBase::updateRenderData()
{
    auto matrixView = m_mainCamera.getViewMatrix();
    auto matrixProj = m_mainCamera.getProjectionMatrix();
    m_pushConstants.matrixVP = matrixProj * matrixView;
    m_pushConstants.minBoundWorld = {m_bounding.minPoint, 1};
    m_pushConstants.maxBoundWorld = {m_bounding.maxPoint, 1};
}

void ApplicationBase::clearZBuffer(vk::CommandBuffer &cmdBuffer)
{
    vk::ClearColorValue clearVal{0x7F7FFFFF, 0, 0, 0};
    cmdBuffer.clearColorImage(*m_zBuffer, vk::ImageLayout::eGeneral, clearVal, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
    clearVal = {0, 0, 0, 0};
    if (m_renderingMode == eRenderingMode::RENDERING_MODE_SCANLINE_ZBUFFER)
    {
        cmdBuffer.clearColorImage(*m_scanlineBufferSpinlock, vk::ImageLayout::eGeneral, clearVal, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
        cmdBuffer.clearColorImage(*m_colorBuffer, vk::ImageLayout::eGeneral, clearVal, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
    }
    clearVal = {0x7F7FFFFF, 0, 0, 0};
    if (m_renderingMode >= eRenderingMode::RENDERING_MODE_NAIVE_HI_ZBUFFER)
        cmdBuffer.clearColorImage(*m_emptyBuffer, vk::ImageLayout::eGeneral, clearVal, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
    if (m_renderingMode == eRenderingMode::RENDERING_MODE_OPTIM_HI_ZBUFFER)
    {
        clearVal = {0xFFFFFFFF, 0ULL, 0ULL, 0ULL};
        cmdBuffer.clearColorImage(*m_octreeLinkHeader, vk::ImageLayout::eGeneral, clearVal, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
        clearVal = {0, 0, 0, 0};
        cmdBuffer.clearColorImage(*m_octreeMarker, vk::ImageLayout::eGeneral, clearVal, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
    }
    cmdBuffer.pipelineBarrier2(vk::DependencyInfo{{}, m_imageClearBarrier});
}

void ApplicationBase::render(vk::CommandBuffer &cmdBuffer)
{
    // for all piplines using Z-Buffer, we need to call clear first(for all mip levels)
    if (m_renderingMode >= eRenderingMode::RENDERING_MODE_NAIVE_ZBUFFER)
        clearZBuffer(cmdBuffer);

    if (m_renderingMode == eRenderingMode::RENDERING_MODE_SCANLINE_ZBUFFER)
    {
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_scanlineZBufferInitPipeline);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_scanlineZBufferPipelineLayout, 0, {m_geometrySet, m_scanlineSet, m_zBufferSet}, {});
        cmdBuffer.pushConstants(m_scanlineZBufferPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0ULL, sizeof(PushConstants), &m_pushConstants);
        cmdBuffer.dispatch(calWorkGroupCount(m_triangleCount, 1024), 1, 1);
        cmdBuffer.pipelineBarrier2(vk::DependencyInfo{{}, m_shaderRWBarrier});
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_scanlineZBufferWorkPipeline);
        cmdBuffer.dispatchIndirect(*m_scanlineGlobalPropertyBuffer, 0ULL);
        cmdBuffer.pipelineBarrier2(vk::DependencyInfo{{}, m_shaderRWBarrier});
    }
    else if (m_renderingMode >= eRenderingMode::RENDERING_MODE_NAIVE_HI_ZBUFFER)
    {
        // just a copy in order to pass compile
        vk::DeviceSize offset{0ULL};
        vk::Buffer vertexBuffer{*m_vertexBuffer};
        vk::Buffer indexBuffer{*m_indexBuffer};

        m_zPrepassRenderingInfo.setRenderArea({{}, m_size});
        cmdBuffer.beginRendering(m_zPrepassRenderingInfo);

        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_zPrepassPipeline);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_zPrepassPipelinelayout, 0, m_zBufferSet, {});
        cmdBuffer.pushConstants(m_zPrepassPipelinelayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0ULL, sizeof(PushConstants), &m_pushConstants);
        cmdBuffer.setViewport(0, vk::Viewport(.0f, .0f, m_mainWindow.Width, m_mainWindow.Height, .0f, 1.f));
        cmdBuffer.setScissor(0, vk::Rect2D({}, {static_cast<uint32_t>(m_mainWindow.Width), static_cast<uint32_t>(m_mainWindow.Height)}));
        cmdBuffer.bindVertexBuffers(0, vertexBuffer, offset);
        cmdBuffer.bindIndexBuffer(indexBuffer, offset, vk::IndexType::eUint32);
        cmdBuffer.drawIndexed(m_triangleCount * 3, 1, 0, 0, 0);

        cmdBuffer.endRendering();

        cmdBuffer.pipelineBarrier2(vk::DependencyInfo{{}, m_shaderRWBarrier});

        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_zBufferMipMappingPipeline);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_zBufferMipMappingPipelineLayout, 0, m_zBufferSet, {});
        cmdBuffer.pushConstants(m_zBufferMipMappingPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0ULL, sizeof(PushConstants), &m_pushConstants);
        cmdBuffer.dispatch(calWorkGroupCount(m_size.width, 8), calWorkGroupCount(m_size.height, 8), 1);
        if (m_renderingMode == eRenderingMode::RENDERING_MODE_OPTIM_HI_ZBUFFER)
        {
            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_octreeInitPipeline);
            cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_octreeInitPipelineLayout, 0, {m_geometrySet, m_octreeSet, m_zBufferSet}, {});
            cmdBuffer.pushConstants(m_octreeInitPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0ULL, sizeof(PushConstants), &m_pushConstants);
            cmdBuffer.dispatch(calWorkGroupCount(m_triangleCount * 3, 1024), 1, 1);
        }

        cmdBuffer.pipelineBarrier2(vk::DependencyInfo{{}, m_shaderRWBarrier});

        switch (m_renderingMode)
        {
        case eRenderingMode::RENDERING_MODE_NAIVE_HI_ZBUFFER:
            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_naiveHiZBufferWorkPipeline);
            cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_hiZBufferOutputPipelineLayout, 0, {m_geometrySet, m_zBufferSet, m_hiZOutputSet, m_octreeSet}, {});
            cmdBuffer.pushConstants(m_hiZBufferOutputPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute, 0ULL, sizeof(PushConstants), &m_pushConstants);
            cmdBuffer.dispatch(calWorkGroupCount(m_triangleCount, 1024), 1, 1);
            break;
        case eRenderingMode::RENDERING_MODE_OPTIM_HI_ZBUFFER:
            for (auto i = m_octreeStartLevel; i < m_octreeLevelCount; ++i)
            {
                cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_optimHiZBufferWorkPipeline);
                cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_hiZBufferOutputPipelineLayout, 0, {m_geometrySet, m_zBufferSet, m_hiZOutputSet, m_octreeSet}, {});
                cmdBuffer.pushConstants(m_hiZBufferOutputPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute, 0ULL, sizeof(PushConstants), &m_pushConstants);
                cmdBuffer.dispatch(calWorkGroupCount(1 << i, 8), calWorkGroupCount(1 << i, 8), 1);
                cmdBuffer.pipelineBarrier2(vk::DependencyInfo{{}, m_shaderRWBarrier});
            }
            break;
        }

        cmdBuffer.pipelineBarrier2(vk::DependencyInfo{{}, m_shaderRWBarrier});
    }
}

void ApplicationBase::finalBlit(vk::CommandBuffer &cmdBuffer)
{
    // just a copy in order to pass compile
    vk::DeviceSize offset{0ULL};
    vk::Buffer vertexBuffer{*m_vertexBuffer};
    vk::Buffer indexBuffer{*m_indexBuffer};
    vk::Buffer hiZVertexBuffer{*m_hiZOutputVertexBuffer};

    if (m_renderingMode == eRenderingMode::RENDERING_MODE_SCANLINE_ZBUFFER)
    {
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_blitPipeline);
        cmdBuffer.setViewport(0, vk::Viewport(.0f, .0f, m_mainWindow.Width, m_mainWindow.Height, .0f, 1.f));
        cmdBuffer.setScissor(0, vk::Rect2D({}, {static_cast<uint32_t>(m_mainWindow.Width), static_cast<uint32_t>(m_mainWindow.Height)}));
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_blitPipelineLayout, 0, m_scanlineSet, {});
        cmdBuffer.draw(3, 1, 0, 0);
    }
    else if (m_renderingMode >= eRenderingMode::RENDERING_MODE_NAIVE_HI_ZBUFFER)
    {
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_hiZBufferPostRenderPipeline);
        cmdBuffer.setViewport(0, vk::Viewport(.0f, .0f, m_mainWindow.Width, m_mainWindow.Height, .0f, 1.f));
        cmdBuffer.setScissor(0, vk::Rect2D({}, {static_cast<uint32_t>(m_mainWindow.Width), static_cast<uint32_t>(m_mainWindow.Height)}));
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_hiZBufferOutputPipelineLayout, 0, {m_geometrySet, m_zBufferSet, m_hiZOutputSet, m_octreeSet}, {});
        cmdBuffer.bindVertexBuffers(0, hiZVertexBuffer, offset);
        cmdBuffer.drawIndirect(*m_hiZIndirectRenderBuffer, offset, 1, sizeof(glm::uvec4));
    }
    else
    {
        switch (m_renderingMode)
        {
        case eRenderingMode::RENDERING_MODE_DEFAULT_WIREFRAME:
            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_defaultFramePipeline);
            cmdBuffer.pushConstants(m_defaultPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0ULL, sizeof(PushConstants), &m_pushConstants);
            break;
        case eRenderingMode::RENDERING_MODE_NAIVE_ZBUFFER:
            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_naiveZBufferPipeline);
            cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_naiveZBufferPipelineLayout, 0, {m_geometrySet, m_zBufferSet}, {});
            cmdBuffer.pushConstants(m_naiveZBufferPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0ULL, sizeof(PushConstants), &m_pushConstants);
            break;
        default:
            break;
        }

        cmdBuffer.setViewport(0, vk::Viewport(.0f, .0f, m_mainWindow.Width, m_mainWindow.Height, .0f, 1.f));
        cmdBuffer.setScissor(0, vk::Rect2D({}, {static_cast<uint32_t>(m_mainWindow.Width), static_cast<uint32_t>(m_mainWindow.Height)}));
        cmdBuffer.bindVertexBuffers(0, vertexBuffer, offset);
        cmdBuffer.bindIndexBuffer(indexBuffer, offset, vk::IndexType::eUint32);
        cmdBuffer.drawIndexed(m_triangleCount * 3, 1, 0, 0, 0);
    }
}
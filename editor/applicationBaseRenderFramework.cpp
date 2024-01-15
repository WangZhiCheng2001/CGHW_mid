#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "applicationBase.hpp"

void ApplicationBase::newFrame(SDL_Window *windowHandle)
{
    if (m_swapchainDirty)
    {
        SDL_GetWindowSize(windowHandle, &m_mainWindow.Width, &m_mainWindow.Height);
        m_size = vk::Extent2D{static_cast<uint32_t>(m_mainWindow.Width), static_cast<uint32_t>(m_mainWindow.Height)};
        recreateRenderTargets();
        m_mainCamera.setWindowSize(m_mainWindow.Width, m_mainWindow.Height);
        if (m_size.width > 0 && m_size.height > 0)
        {
            ImGui_ImplVulkan_SetMinImageCount(3);
            ImGui_ImplVulkanH_CreateOrResizeWindow(*m_renderContext.getInstanceHandle(), *m_renderContext.getAdapterHandle(), *m_renderContext.getDeviceHandle(),
                                                   &m_mainWindow, m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eGraphics)->queue_family_index,
                                                   reinterpret_cast<VkAllocationCallbacks *>(&allocationCallbacks),
                                                   m_size.width, m_size.height, 3);
            m_mainWindow.FrameIndex = 0;
            m_swapchainDirty = false;
        }
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void ApplicationBase::presentFrame()
{
    if(m_swapchainDirty)
        return;

    vk::SwapchainKHR swapchain = m_mainWindow.Swapchain;
    vk::Semaphore waitSemaphore = m_mainWindow.FrameSemaphores[m_mainWindow.SemaphoreIndex].RenderCompleteSemaphore;

    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphores(waitSemaphore)
        .setImageIndices(m_mainWindow.FrameIndex)
        .setSwapchains(swapchain);
    auto res = m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eGraphics)->queue_handle->presentKHR(presentInfo);
    if(res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR)
    {
        m_swapchainDirty = true;
        return;
    }
    assert(res >= vk::Result::eSuccess);

    m_mainWindow.SemaphoreIndex = (m_mainWindow.SemaphoreIndex + 1) % m_mainWindow.ImageCount;
}

// there is a chunk of things...
// due to test, only when we drop everything about frame rendering into a single code block can things be correct
// otherwise we will get a lot of reading/writing violation error
// i am not sure what caused the issue
void ApplicationBase::renderFrame()
{
    vk::SwapchainKHR swapchain = m_mainWindow.Swapchain;
    vk::Semaphore acquireSemaphore = m_mainWindow.FrameSemaphores[m_mainWindow.SemaphoreIndex].ImageAcquiredSemaphore;
    vk::Semaphore waitSemaphore = m_mainWindow.FrameSemaphores[m_mainWindow.SemaphoreIndex].RenderCompleteSemaphore;
    vk::ClearValue clearValue = vk::ClearColorValue{.0f, .0f, .0f, 1.f};

    auto [status, res] = m_renderContext.getDeviceHandle()->acquireNextImageKHR(swapchain, UINT64_MAX, acquireSemaphore, {});
    m_mainWindow.FrameIndex = res;
    assert(status >= vk::Result::eSuccess);
    if (status == vk::Result::eErrorOutOfDateKHR || status == vk::Result::eSuboptimalKHR)
    {
        m_swapchainDirty = true;
        return;
    }

    vk::CommandBuffer currentCmdBuffer = m_mainWindow.Frames[m_mainWindow.FrameIndex].CommandBuffer;
    vk::Fence fence = m_mainWindow.Frames[m_mainWindow.FrameIndex].Fence;

    // wait last frame for completing presenting
    m_renderContext.getDeviceHandle()->waitForFences(fence, VK_TRUE, UINT64_MAX);
    m_renderContext.getDeviceHandle()->resetFences(fence);

    // begin internal command buffer
    vk::CommandPool pool = m_mainWindow.Frames[m_mainWindow.FrameIndex].CommandPool;
    m_renderContext.getDeviceHandle()->resetCommandPool(pool);
    currentCmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    render(currentCmdBuffer);

    vk::RenderPass pass = m_mainWindow.RenderPass;
    vk::Framebuffer frameBuffer = m_mainWindow.Frames[m_mainWindow.FrameIndex].Framebuffer;
    vk::Rect2D rect{{}, {static_cast<uint32_t>(m_mainWindow.Width), static_cast<uint32_t>(m_mainWindow.Height)}};
    vk::RenderPassBeginInfo beginInfo{};
    beginInfo.setRenderPass(pass)
        .setFramebuffer(frameBuffer)
        .setRenderArea(rect)
        .setClearValues(clearValue);
    currentCmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

    finalBlit(currentCmdBuffer);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentCmdBuffer);

    currentCmdBuffer.endRenderPass();
    currentCmdBuffer.end();

    vk::PipelineStageFlags stageFlag{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(currentCmdBuffer)
        .setWaitDstStageMask(stageFlag)
        .setWaitSemaphores(acquireSemaphore)
        .setSignalSemaphores(waitSemaphore);
    m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eGraphics)->queue_handle->submit(submitInfo, fence);
    // m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eGraphics)->queue_handle->waitIdle();
}

void ApplicationBase::destroy()
{
    m_renderContext.getDeviceHandle()->waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    ImGui_ImplVulkanH_DestroyWindow(*m_renderContext.getInstanceHandle(), *m_renderContext.getDeviceHandle(),
                                    &m_mainWindow, reinterpret_cast<VkAllocationCallbacks *>(&allocationCallbacks));

    m_vertexBuffer.reset();
    m_indexBuffer.reset();
    m_scanlineBuffer.reset();
    m_scanlineGlobalPropertyBuffer.reset();
    m_hiZOutputVertexBuffer.reset();
    m_hiZIndirectRenderBuffer.reset();
    m_faceIndicesOfOctree.reset();
    m_zBuffer.reset();
    for (auto i = 0; i < m_zBufferMipViews.size(); ++i)
        if (m_zBufferMipViews[i])
            m_renderContext.getDeviceHandle()->destroy(m_zBufferMipViews[i], allocationCallbacks);
    m_octreeLinkHeader.reset();
    for (auto i = 0; i < m_octreeLinkHeaderMipViews.size(); ++i)
        if (m_octreeLinkHeaderMipViews[i])
            m_renderContext.getDeviceHandle()->destroy(m_octreeLinkHeaderMipViews[i], allocationCallbacks);
    m_octreeMarker.reset();
    for (auto i = 0; i < m_octreeMarkerMipViews.size(); ++i)
        if (m_octreeMarkerMipViews[i])
            m_renderContext.getDeviceHandle()->destroy(m_octreeMarkerMipViews[i], allocationCallbacks);
    m_colorBuffer.reset();
    m_renderContext.getDeviceHandle()->destroy(m_colorBufferView, allocationCallbacks);
    m_scanlineBufferSpinlock.reset();
    m_renderContext.getDeviceHandle()->destroy(m_scanlineBufferSpinlockView, allocationCallbacks);
    m_emptyBuffer.reset();
    m_renderContext.getDeviceHandle()->destroy(m_emptyBufferView, allocationCallbacks);

    m_renderContext.getDeviceHandle()->destroy(m_guiDescPool, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_defaultPipelineLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_defaultFramePipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_naiveZBufferPipelineLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_naiveZBufferPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_scanlineZBufferPipelineLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_scanlineZBufferInitPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_scanlineZBufferWorkPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_zPrepassPipelinelayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_zPrepassPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_zBufferMipMappingPipelineLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_zBufferMipMappingPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_octreeInitPipelineLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_octreeInitPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_hiZBufferOutputPipelineLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_naiveHiZBufferWorkPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_optimHiZBufferWorkPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_hiZBufferPostRenderPipeline, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_blitPipelineLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_blitPipeline, allocationCallbacks);

    m_renderContext.getDeviceHandle()->resetDescriptorPool(m_descPool);
    m_renderContext.getDeviceHandle()->destroy(m_descPool, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_zBufferSetLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_geometrySetLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_scanlineSetLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_hiZOutputSetLayout, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_octreeSetLayout, allocationCallbacks);

    m_renderContext.getDeviceHandle()->destroy(m_internalTransferFence, allocationCallbacks);
    m_renderContext.getDeviceHandle()->destroy(m_internalTransferPool, allocationCallbacks);
}
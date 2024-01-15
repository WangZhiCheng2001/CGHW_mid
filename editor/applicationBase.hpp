#pragma once

#include <vulkan/vulkan.hpp>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <glm/ext.hpp>

#include <renderContext.h>
#include <fileLoader.hpp>
#include <modelLoader.hpp>
#include <camera.hpp>

struct PushConstants
{
    glm::mat4 matrixVP;
    glm::vec3 lightDirection{1.f, 1.f, 1.f};
    uint32_t mipLevelCount{~0U};
    glm::vec4 minBoundWorld;
    glm::vec4 maxBoundWorld;
};

// the multi descriptor binding in shader's layout needs a const max size
// max resolution to 2048
constexpr uint32_t g_predefMaxMipLevel = 12U;

enum eRenderingMode
{
    RENDERING_MODE_DEFAULT_WIREFRAME,
    RENDERING_MODE_NAIVE_ZBUFFER,
    RENDERING_MODE_SCANLINE_ZBUFFER,
    RENDERING_MODE_NAIVE_HI_ZBUFFER,
    RENDERING_MODE_OPTIM_HI_ZBUFFER
};

static const char *g_renderingModeText[] = {
    "wireframe view",
    "naive Z-Buffer",
    "scanline Z-Buffer",
    "naive Hierarchical Z-Buffer",
    "optimized Hierarchical Z-Buffer"};

class ApplicationBase
{
public:
    /* window & ui */
    void initSurface(SDL_Window *windowHandle);
    void initSwapchain();
    void initImGui(SDL_Window *windowHandle);

    void displayGui(SDL_Window *windowHandle);

    bool shouldClose() const { return m_shouldWindowClose; }
    bool shouldRender()
    {
        ImGui::Render();

        // handle fraps count
        m_currentCounter = SDL_GetPerformanceCounter();
        m_dirtyTimer += ((m_currentCounter - m_prevCounter) * 1000.f) / m_frequency;
        m_prevCounter = m_currentCounter;
        if (m_dirtyTimer >= 1000.f)
        {
            m_dirtyTimer -= 1000.f;
            m_fraps = m_fpsCounter;
            m_fpsCounter = 1;
        }
        else
            m_fpsCounter++;

        ImDrawData *draw_data = ImGui::GetDrawData();
        return !(draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    }

    void processEvent(SDL_Window *windowHandle);
    void newFrame(SDL_Window *windowHandle);
    void presentFrame();

    /* rendering detail */
    void reloadModel(const std::filesystem::path &filePath);
    void recreateRenderTargets();
    void createStaticResources();
    void createRenderer();
    void updateRenderData();

    void clearZBuffer(vk::CommandBuffer &cmdBuffer);
    void render(vk::CommandBuffer &cmdBuffer);
    void finalBlit(vk::CommandBuffer &cmdBuffer);
    void renderFrame();

    void destroy();

protected:
    /* resources */
    std::shared_ptr<Buffer> m_vertexBuffer;
    std::shared_ptr<Buffer> m_indexBuffer;
    size_t m_vertexCount{};
    size_t m_triangleCount{};
    BoundingBox m_bounding{};
    std::shared_ptr<Buffer> m_scanlineBuffer;               // filled scanline range
    std::shared_ptr<Buffer> m_scanlineGlobalPropertyBuffer; // dispatch parameters, active scanline count in order
    std::shared_ptr<Buffer> m_hiZOutputVertexBuffer;
    std::shared_ptr<Buffer> m_hiZIndirectRenderBuffer;
    size_t m_octreeLevelCount{8};
    size_t m_octreeStartLevel{3};
    std::shared_ptr<Buffer> m_faceIndicesOfOctree;
    std::shared_ptr<Image> m_zBuffer;
    std::vector<vk::ImageView> m_zBufferMipViews{};
    std::shared_ptr<Image> m_scanlineBufferSpinlock{};
    vk::ImageView m_scanlineBufferSpinlockView;
    std::shared_ptr<Image> m_octreeLinkHeader;
    std::vector<vk::ImageView> m_octreeLinkHeaderMipViews{};
    std::shared_ptr<Image> m_octreeMarker;
    std::vector<vk::ImageView> m_octreeMarkerMipViews{};
    std::shared_ptr<Image> m_colorBuffer;
    vk::ImageView m_colorBufferView;
    std::shared_ptr<Image> m_emptyBuffer; // an empty depth buffer for hi-z post rendering
    vk::ImageView m_emptyBufferView;
    PushConstants m_pushConstants{};
    eRenderingMode m_renderingMode{eRenderingMode::RENDERING_MODE_DEFAULT_WIREFRAME};
    Camera m_mainCamera{};

    /* render context */
    RenderContext m_renderContext{};
    vk::CommandPool m_internalTransferPool{};
    vk::Fence m_internalTransferFence{};

    vk::DescriptorPool m_descPool{};
    vk::DescriptorSetLayout m_zBufferSetLayout{};
    vk::DescriptorSet m_zBufferSet{};
    vk::DescriptorSetLayout m_geometrySetLayout{};
    vk::DescriptorSet m_geometrySet{};
    vk::DescriptorSetLayout m_scanlineSetLayout{};
    vk::DescriptorSet m_scanlineSet{};
    vk::DescriptorSetLayout m_hiZOutputSetLayout{};
    vk::DescriptorSet m_hiZOutputSet{};
    vk::DescriptorSetLayout m_octreeSetLayout{};
    vk::DescriptorSet m_octreeSet{};

    vk::RenderingInfo m_zPrepassRenderingInfo{};

    vk::PipelineLayout m_defaultPipelineLayout{};
    vk::Pipeline m_defaultFramePipeline{};
    vk::PipelineLayout m_naiveZBufferPipelineLayout{};
    vk::Pipeline m_naiveZBufferPipeline{};
    vk::PipelineLayout m_scanlineZBufferPipelineLayout{};
    vk::Pipeline m_scanlineZBufferInitPipeline{};
    vk::Pipeline m_scanlineZBufferWorkPipeline{};
    vk::PipelineLayout m_zPrepassPipelinelayout{};
    vk::Pipeline m_zPrepassPipeline{};
    vk::PipelineLayout m_zBufferMipMappingPipelineLayout{};
    vk::Pipeline m_zBufferMipMappingPipeline{};
    vk::PipelineLayout m_octreeInitPipelineLayout{};
    vk::Pipeline m_octreeInitPipeline{};
    vk::PipelineLayout m_hiZBufferOutputPipelineLayout{};
    vk::Pipeline m_naiveHiZBufferWorkPipeline{};
    vk::Pipeline m_optimHiZBufferWorkPipeline{};
    vk::Pipeline m_hiZBufferPostRenderPipeline{};
    vk::PipelineLayout m_blitPipelineLayout{};
    vk::Pipeline m_blitPipeline{};
    vk::MemoryBarrier2 m_shaderRWBarrier{};
    vk::MemoryBarrier2 m_imageClearBarrier{};

    /* ui display */
    ImGui_ImplVulkanH_Window m_mainWindow{};
    vk::Extent2D m_size{};
    vk::DescriptorPool m_guiDescPool{};
    bool m_shouldWindowClose{false};
    bool m_swapchainDirty{false};

    /* misc */
    size_t m_frequency{};
    size_t m_prevCounter{};
    size_t m_currentCounter{};
    float m_dirtyTimer{.0f};
    size_t m_fpsCounter{};
    size_t m_fraps{};
    bool m_leftMouseButton{false};
};
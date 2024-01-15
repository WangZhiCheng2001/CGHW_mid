#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "applicationBase.hpp"

static void check_vk_result(VkResult res)
{
    if (!res)
        return;
    // std::cerr << "[Vulkan] Error: VkResult = " << res << "\n";
    spdlog::error("Vulkan: VkResult = {}.", res);
    if (res < 0)
        abort();
}

void ApplicationBase::initSurface(SDL_Window *windowHandle)
{
    std::vector<const char *> instanceExtensions{};
    uint32_t extensionCount = 0U;
    SDL_Vulkan_GetInstanceExtensions(windowHandle, &extensionCount, nullptr);
    instanceExtensions.resize(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(windowHandle, &extensionCount, instanceExtensions.data());
    m_renderContext.init(instanceExtensions, {}, {VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME}, {});

    if (SDL_Vulkan_CreateSurface(windowHandle, *m_renderContext.getInstanceHandle(), &m_mainWindow.Surface) == SDL_FALSE)
    {
        // std::cerr << "Failed to create Vulkan surface.\n";
        spdlog::critical("Failed to create Vulkan surface from SDL window.");
        abort();
        exit(-1);
    }

    SDL_GetWindowSize(windowHandle, &m_mainWindow.Width, &m_mainWindow.Height);
    m_size = vk::Extent2D{static_cast<uint32_t>(m_mainWindow.Width), static_cast<uint32_t>(m_mainWindow.Height)};

    // init counters
    m_frequency = SDL_GetPerformanceFrequency();
    m_prevCounter = SDL_GetPerformanceCounter();

    // init camera property
    m_mainCamera.setLookat({-1.f, .0f, .0f}, {.0f, .0f, .0f}, {.0f, 1.f, .0f});
    m_mainCamera.setWindowSize(m_mainWindow.Width, m_mainWindow.Height);
    m_mainCamera.setClipPlanes({.01f, 1000.f});
    m_mainCamera.setFov(90.f);
    m_mainCamera.setActionMode(Camera::eCameraActionMode::CAMERA_ACTION_MODE_FOCUS_ON_OBJECT);
}

void ApplicationBase::initSwapchain()
{
    auto surfaceSupportedFormats = m_renderContext.getAdapterHandle()->getSurfaceSupportKHR(m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eGraphics)->queue_family_index, m_mainWindow.Surface);

    const std::vector<VkFormat> requiredFormats{VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
    const VkColorSpaceKHR requiredColorSapce = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    m_mainWindow.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(*m_renderContext.getAdapterHandle(), m_mainWindow.Surface,
                                                                       requiredFormats.data(), requiredFormats.size(),
                                                                       requiredColorSapce);
    // try to find faster mode without vsync, otherwise use vsync mode
    const std::vector<VkPresentModeKHR> requiredPresentModes{VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR};
    m_mainWindow.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(*m_renderContext.getAdapterHandle(), m_mainWindow.Surface,
                                                                   requiredPresentModes.data(), requiredPresentModes.size());

    ImGui_ImplVulkanH_CreateOrResizeWindow(*m_renderContext.getInstanceHandle(), *m_renderContext.getAdapterHandle(), *m_renderContext.getDeviceHandle(),
                                           &m_mainWindow, m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eGraphics)->queue_family_index,
                                           reinterpret_cast<VkAllocationCallbacks *>(&allocationCallbacks),
                                           m_size.width, m_size.height, 3);

    m_mainWindow.ClearValue.color = {.0f, .0f, .0f, 1.f};
}

void ApplicationBase::initImGui(SDL_Window *windowHandle)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;

    // ImGui internal descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 8U);
    vk::DescriptorPoolCreateInfo createInfo;
    createInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(4U)
        .setPoolSizes(poolSizes);
    m_guiDescPool = m_renderContext.getDeviceHandle()->createDescriptorPool(createInfo, allocationCallbacks);

    ImGui_ImplSDL2_InitForVulkan(windowHandle);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = *m_renderContext.getInstanceHandle();
    init_info.PhysicalDevice = *m_renderContext.getAdapterHandle();
    init_info.Device = *m_renderContext.getDeviceHandle();
    init_info.QueueFamily = m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eGraphics)->queue_family_index;
    init_info.Queue = *m_renderContext.getQueueInstanceHandle(vk::QueueFlagBits::eGraphics)->queue_handle;
    init_info.PipelineCache = *m_renderContext.getPipelineCacheHandle();
    init_info.DescriptorPool = m_guiDescPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 3;
    init_info.ImageCount = m_mainWindow.ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = reinterpret_cast<VkAllocationCallbacks *>(&allocationCallbacks);
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, m_mainWindow.RenderPass);
}

void ApplicationBase::displayGui(SDL_Window *windowHandle)
{
    // update title for every 1 sec
    static float dirtyTimer = .0f;

    dirtyTimer += ImGui::GetIO().DeltaTime;
    if (dirtyTimer > 1)
    {
        std::stringstream output;
        output << "Z-Buffer Algorithms on Vulkan";
        output << "\t|\t" << m_size.width << "x" << m_size.height;
        output << "\t|\t" << m_fraps << "FPS / " << std::setprecision(3) << 1000.F / m_fraps << "ms";
        SDL_SetWindowTitle(windowHandle, output.str().c_str());
        dirtyTimer = .0f;
    }

    ImGui::Begin("settings");

    ImGui::Combo("rendering mode", (int *)&m_renderingMode, g_renderingModeText, 5);
    ImGui::InputFloat3("light direction", &m_pushConstants.lightDirection.x);
    ImGui::TextWrapped("vertex count: %llu", m_vertexCount);
    ImGui::TextWrapped("Triangle face count: %llu", m_triangleCount);

    ImGui::End();
}

void ApplicationBase::processEvent(SDL_Window *windowHandle)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);

        // window event
        if (event.type == SDL_QUIT)
            m_shouldWindowClose = true;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(windowHandle))
            m_shouldWindowClose = true;
        if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) && event.window.windowID == SDL_GetWindowID(windowHandle))
            m_swapchainDirty = true;

        // mouse event
        if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
            return;
        m_leftMouseButton = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (event.type == SDL_MOUSEBUTTONDOWN)
            m_mainCamera.setMousePosition({event.button.x, event.button.y});
        if (event.type == SDL_MOUSEMOTION)
            m_mainCamera.mouseMove(event.motion.x, event.motion.y, m_leftMouseButton, false, false);
        if (event.type == SDL_MOUSEWHEEL && event.wheel.y != 0)
            m_mainCamera.wheel(event.wheel.y > 0 ? 1 : -1);
    }
}
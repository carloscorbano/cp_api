#include "cp_api/window/renderer.hpp"
#include "cp_api/window/window.hpp"
#include "cp_api/window/vulkan.hpp"
#include "cp_api/window/renderTargetManager.hpp"
#include "cp_api/core/threadPool.hpp"
#include "cp_api/core/debug.hpp"

#include "cp_api/world/world.hpp"

#include "cp_api/components/transform.hpp"
#include "cp_api/components/camera.hpp"
#include "cp_api/components/dontDestroyOnLoad.hpp"
#include "cp_api/components/uiComponent.hpp"

namespace cp_api {
    Renderer::Renderer(Window& window, World& world, ThreadPool& threadPool) 
        : m_window(window), m_vulkan(window.GetVulkan()), m_world(world), m_threadPool(threadPool) {
        CP_LOG_INFO("Creating renderer object!");
        //setup events.
        setupEventListeners();
        //initialization methods
        createGlobalDescriptorPool();
        createFrames();
        createRenderFinishedSemaphores();
        createCommandResources();
        initImGui();

        m_rtManager = std::make_unique<RenderTargetManager>();
        m_rtManager->Init(&m_vulkan);
        createMainCamera();

        m_renderThreadWorker = std::thread([this]() { submitThreadWork(); });
        CP_LOG_SUCCESS("Successfully created renderer object!");
    }

    Renderer::~Renderer() {
        CP_LOG_INFO("Destroying renderer object!");

        m_renderEnabled.store(false);
        m_renderThreadWorker.join();

        vkDeviceWaitIdle(m_vulkan.GetDevice());
        
        destroyImGui();
        destroyRenderFinishedSemaphores();
        destroyCommandResources();
        destroyMainCamera();
        destroyFrames();
        destroyGlobalDescriptorPool();
        CP_LOG_SUCCESS("Successfully destroyed renderer object!");
    }

    void Renderer::Render() {
        if(!isRenderEnabled()) return;

        m_rtManager->BeginFrame(m_frameCounter);

        //cache
        auto& device = m_vulkan.GetDevice();
        auto& frame = m_frames[m_writeFrameIndex];
        auto& swp = m_vulkan.GetSwapchain();
        
        

        //-----------------------------------------------------------------
        // RECORD COMMANDS
        //-----------------------------------------------------------------
        std::vector<std::future<VkResult>> futures;
        for(uint32_t i = 0; i < MAX_WORKERS_PER_FRAME; ++i)
        {
            auto task = m_threadPool.Submit(TaskPriority::HIGH, [](Renderer* renderer, const Frame& f, const uint32_t& index, Vulkan::Swapchain& swp) -> VkResult
            {
                auto result = renderer->m_vulkan.BeginCommandBuffer(f.workers[index].cb, { swp.colorFormat }, swp.depthFormat, swp.stencilFormat);
                if(result != VK_SUCCESS) return result;
                {
                    //DRAW COMMANDS
                    //Process world result here and record draw calls into f.secondaries[index]
                }
                return vkEndCommandBuffer(f.workers[index].cb);
                return VK_SUCCESS;

            }, this, frame, i, swp);

            futures.push_back(std::move(task));
        }

        for(auto& future : futures)
        {
            if(future.get() != VK_SUCCESS) CP_LOG_THROW("Window workers have failed to complete record task!");
        }

        //-----------------------------------------------------------------
        // IMGUI
        //-----------------------------------------------------------------
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto view = m_world.GetRegistry().view<UICanvas>();
        view.each([](const entt::entity& e, UICanvas& canvas)
        {
            (void)e;
            if(canvas.open)
            {
                ImGui::SetNextWindowPos(canvas.pos, ImGuiCond_Once, canvas.pivot);
                ImGui::SetNextWindowSize(canvas.size, ImGuiCond_Once);

                ImGui::Begin(canvas.name.c_str(), &canvas.open, canvas.flags);
                {
                    for(auto& child : canvas.children)
                    {
                        if(!child->enabled) continue;
                        if(child->sameLine) ImGui::SameLine(child->sameLineSettings.offset, child->sameLineSettings.spacing);

                        if(child->font) ImGui::PushFont(child->font);
                        child->Draw();
                        if(child->font) ImGui::PopFont();
                    }
                }
                ImGui::End();
            }
        });

        ImGui::Render();
        auto& ImGuiCmdBuf = frame.imguiCmdBuffer;

        if(m_vulkan.BeginCommandBuffer(ImGuiCmdBuf, { swp.colorFormat }, swp.depthFormat, swp.stencilFormat) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to begin imgui buffer!");        
        }

        auto drawData = ImGui::GetDrawData();        
        if(drawData) ImGui_ImplVulkan_RenderDrawData(drawData, ImGuiCmdBuf);

        if(vkEndCommandBuffer(ImGuiCmdBuf) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to end ImGui command buffer!");
        }

        //-----------------------------------------------------------------
        // SYNC AND ADVANCE
        //-----------------------------------------------------------------
        //signal timeline semaphore to indicate frame is ready for rendering
        m_vulkan.SignalTimelineSemaphore(m_timelineSem, frame.recordValue);
        
        //wait timeline semaphore 
        m_vulkan.WaitTimelineSemaphores({ m_timelineSem }, { frame.renderValue });

        //update for next frame
        frame.renderValue += (2 * (uint32_t)m_frames.size());
        frame.recordValue += (2 * (uint32_t)m_frames.size());

        //advance
        m_writeFrameIndex = (m_writeFrameIndex + 1) % (uint32_t)m_frames.size();
        m_frameCounter++;
    }

    void Renderer::submitThreadWork() {
        while(!m_window.ShouldClose()) {
            if(!isRenderEnabled()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            //-----------------------------------------------------------------
            // SWAPCHAIN RECREATION
            //-----------------------------------------------------------------
            bool lostSurface = m_surfaceLost.load();

            if(m_swapchainIsDirty.load() || lostSurface) {
                m_renderEnabled.store(false);
                vkDeviceWaitIdle(m_vulkan.GetDevice());

                if(lostSurface) {
                    m_vulkan.RecreateSurface();
                    m_surfaceLost.store(false);
                }

                CP_LOG_INFO("Recreating swapchain...");

                destroyCommandResources();
                destroyRenderFinishedSemaphores();

                m_vulkan.RecreateSwapchain(m_window.IsVSyncEnabled() ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR, true);
                auto& swp = m_vulkan.GetSwapchain();
                const uint32_t w = swp.extent.width;
                const uint32_t h = swp.extent.height;
                
                m_rtManager->InvalidateByResolution(w, h);

                auto result = m_world.GetRegistry().try_get<CameraComponent>((entt::entity)m_mainCameraUID);
                if(result) {
                    result->width = w;
                    result->height = h;
                }

                createCommandResources();
                createRenderFinishedSemaphores();

                m_swapchainIsDirty.store(false);
                m_skipAfterSwapchainRecreation.store(true);
                m_renderEnabled.store(true);
                
                CP_LOG_INFO("Swapchain recreated.");
            }

            //-----------------------------------------------------------------
            // FRAME PROCESSING
            //-----------------------------------------------------------------
            auto& frame = m_frames[m_readFrameIndex];

            //acquire next swapchain image
            uint32_t imageIndex = 0;
            VkResult result = m_vulkan.AcquireSwapchainNextImage(frame.imageAvailable, &imageIndex);
            if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                m_swapchainIsDirty.store(true, std::memory_order_release);
                continue;
            }

            if(result == VK_ERROR_SURFACE_LOST_KHR) {
                m_surfaceLost.store(true);
                m_swapchainIsDirty.store(true);
                continue;
            }

            if (result != VK_SUCCESS) {
                CP_LOG_ERROR("vkAcquireNextImageKHR failed");
                continue;
            }

            //wait until write finishes            
            m_vulkan.WaitTimelineSemaphores( { m_timelineSem }, { frame.recordValue });

            VkCommandBufferBeginInfo pbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            pbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if(vkBeginCommandBuffer(frame.primary, &pbi) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to begin primary command buffer!");
            }
            
            //set swapchain image to layout transfer dst
            auto& swp = m_vulkan.GetSwapchain();

            // --- Begin: render each camera to its own render target ---
            auto& reg = m_world.GetRegistry();
            auto camView = reg.view<CameraComponent, TransformComponent>();

            for (auto e : camView) {
                auto& camComp = camView.get<CameraComponent>(e);
                uint32_t camW = camComp.width;
                uint32_t camH = camComp.height;

                // Acquire or recreate RT for this camera
                RenderTarget* camRT = m_rtManager->Acquire((uint32_t)e, camW, camH, swp.colorFormat, swp.depthFormat);

                auto& colorImage = camRT->GetColorImage();
                auto& depthImage = camRT->GetDepthImage();

                // ensure layouts (reusing your helpers)
                VulkanImage::TransitionImageLayout(frame.primary, colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                VulkanImage::TransitionImageLayout(frame.primary, depthImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

                // Prepare attachments (clear values)
                VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
                colorAttachment.imageView = colorImage.GetView();
                colorAttachment.imageLayout = colorImage.GetLayout();
                colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                VkClearValue clearColor = { {0.0f, 0.2f, 0.3f, 1.0f} };
                colorAttachment.clearValue = clearColor;

                VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
                depthAttachment.imageView = depthImage.GetView();
                depthAttachment.imageLayout = depthImage.GetLayout();
                depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                VkClearValue clearDepth{};
                clearDepth.depthStencil.depth = 1.0f;
                clearDepth.depthStencil.stencil = 0;
                depthAttachment.clearValue = clearDepth;

                VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
                renderingInfo.renderArea.offset = {0,0};
                renderingInfo.renderArea.extent = { camW, camH };
                renderingInfo.layerCount = 1;
                renderingInfo.colorAttachmentCount = 1;
                renderingInfo.pColorAttachments = &colorAttachment;
                renderingInfo.pDepthAttachment = &depthAttachment;
                renderingInfo.pStencilAttachment = &depthAttachment;
                renderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

                bool isMainCamera = (uint32_t)e == m_mainCameraUID;

                vkCmdBeginRendering(frame.primary, &renderingInfo);
                {
                    // NOTE:
                    // - Atualmente você grava worker.cb de forma genérica; se no futuro cada worker
                    //   gravar comandos específicos por câmera, aqui você deve executar apenas
                    //   os command buffers correspondentes a esta câmera.
                    //
                    // For now, execute the worker secondaries as before (they are scene draws).
                    if(m_skipAfterSwapchainRecreation.load()) {
                        m_skipAfterSwapchainRecreation.store(false);
                    } else {
                        std::vector<VkCommandBuffer> cmds;
                        for(auto& worker : frame.workers) {
                            cmds.push_back(worker.cb);
                        }
                        // If you have per-camera secondary buffers in future, only push the ones for this camera.
                        if (&frame.imguiCmdBuffer != VK_NULL_HANDLE && isMainCamera) cmds.push_back(frame.imguiCmdBuffer);
                        vkCmdExecuteCommands(frame.primary, (uint32_t)cmds.size(), cmds.data());
                    }
                }
                vkCmdEndRendering(frame.primary);

                if (isMainCamera) {
                    VulkanImage::TransitionImageLayout(frame.primary, colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                    VulkanImage::TransitionImageLayout(frame.primary, swp.images[imageIndex], swp.colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    VulkanImage::CopyImage(frame.primary, colorImage.GetImage(), swp.images[imageIndex], colorImage.GetExtent().width, colorImage.GetExtent().height);
                    VulkanImage::TransitionImageLayout(frame.primary, swp.images[imageIndex], swp.colorFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
                }
            }
            // --- End: render each camera ---

            if(vkEndCommandBuffer(frame.primary) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to end primary command buffer!");
            }

            VkCommandBufferSubmitInfo cbsi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
            cbsi.commandBuffer = frame.primary;

            VkSemaphoreSubmitInfo waitBinary{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
            waitBinary.semaphore = frame.imageAvailable; // binário
            waitBinary.value = 0;
            waitBinary.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

            VkSemaphoreSubmitInfo signalRenderFinished{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
            signalRenderFinished.semaphore = m_renderFinishedSemaphores[imageIndex];
            signalRenderFinished.value = 0;
            signalRenderFinished.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

            VkSemaphoreSubmitInfo signalTimeline{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
            signalTimeline.semaphore = m_timelineSem;
            signalTimeline.value = frame.renderValue;
            signalTimeline.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

            VkSubmitInfo2 submit2{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
            VkSemaphoreSubmitInfo waits[1] = { waitBinary };
            submit2.waitSemaphoreInfoCount = 1;
            submit2.pWaitSemaphoreInfos = waits;
            submit2.commandBufferInfoCount = 1;
            submit2.pCommandBufferInfos = &cbsi;
            submit2.signalSemaphoreInfoCount = 2;
            VkSemaphoreSubmitInfo signals[] = { signalTimeline, signalRenderFinished };
            submit2.pSignalSemaphoreInfos = signals;

            if(vkQueueSubmit2(m_vulkan.GetQueue(QueueType::GRAPHICS), 1, &submit2, VK_NULL_HANDLE) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to submit to graphics queue!");
            }

            // Apresenta
            VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            present.swapchainCount = 1;
            present.pSwapchains = &swp.handler;
            present.pImageIndices = &imageIndex;
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = &m_renderFinishedSemaphores[imageIndex];
            
            result = vkQueuePresentKHR(m_vulkan.GetQueue(QueueType::PRESENT), &present);

            if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            {
                m_swapchainIsDirty.store(true, std::memory_order_release);
            } else if(result == VK_ERROR_SURFACE_LOST_KHR) {
                CP_LOG_THROW("Failed to present swapchain image: surface lost!");
                m_surfaceLost.store(true, std::memory_order_release);
            }

            //advance
            m_readFrameIndex = (m_readFrameIndex + 1) % (uint32_t)m_frames.size();

            //cleanup unused render targets.
            m_rtManager->PurgeUnused();
        }
    }

#pragma region EVENTS_AND_CALLBACKS
    void Renderer::setupEventListeners() {
        m_window.GetEventDispatcher().Subscribe<onWindowDragStopEvent>([this](const onWindowDragStopEvent& e) {
            // Handle window drag stop event
            CP_LOG_INFO("[RENDERER] Window drag stopped.");
            m_swapchainIsDirty.store(true);
        });

        m_window.GetEventDispatcher().Subscribe<onWindowMinimizeEvent>([this](const onWindowMinimizeEvent& e) {
            // Handle window minimize event
            CP_LOG_INFO("[RENDERER] Window minimized: {}", e.minimized ? "Yes" : "No");
            m_renderEnabled.store(!e.minimized);
            m_swapchainIsDirty.store(true);
        });

        auto& reg = m_world.GetRegistry();
        reg.on_construct<CameraComponent>().connect<&Renderer::onCameraCreationCallback>(this);
        reg.on_destroy<CameraComponent>().connect<&Renderer::onCameraDestructionCallback>(this);
    }

    void Renderer::onCameraCreationCallback(entt::registry& reg, entt::entity e) {
        // Pre-cria render target para a nova câmera (evita stall na primeira render)
        // Assumimos que CameraComponent possui width e height (como no seu createMainCamera).
        if (!reg.valid(e)) return;

        auto &cam = reg.get<CameraComponent>(e);
        // usa formatos do swapchain por padrão; se câmera tiver formatos próprios, adapte.
        auto& swp = m_vulkan.GetSwapchain();
        m_rtManager->Acquire((uint32_t)e, cam.width, cam.height, swp.colorFormat, swp.depthFormat);
    }

    void Renderer::onCameraDestructionCallback(entt::registry& reg, entt::entity e) {
        // Remove o RT associado à câmera destruída
        m_rtManager->Release((uint32_t)e);
    }
#pragma endregion EVENTS_AND_CALLBACKS

#pragma region INITIALIZATION_METHODS
    void Renderer::createGlobalDescriptorPool() {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        
        if (vkCreateDescriptorPool(m_vulkan.GetDevice(), &pool_info, nullptr, &g_descriptorPool) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create global descriptor pool!");
        }
    }

    void Renderer::destroyGlobalDescriptorPool() {
        if(g_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_vulkan.GetDevice(), g_descriptorPool, nullptr);
            g_descriptorPool = VK_NULL_HANDLE;
        }
    }

    void Renderer::createFrames() {
        const auto imageCount = m_vulkan.GetSwapchain().images.size();
        m_frames.resize(imageCount);

        //create sync objects (timeline semaphore)
        VkSemaphoreTypeCreateInfo timelineCreateInfo{};
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = 0; // valor inicial do contador

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semInfo.pNext = &timelineCreateInfo;

        auto& device = m_vulkan.GetDevice();

        if(vkCreateSemaphore(device, &semInfo, nullptr, &m_timelineSem) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create timeline semaphore!");
        }

        for(uint32_t i = 0; i < (uint32_t)m_frames.size(); ++i) {
            Frame& frame = m_frames[i];
            // -------------------------------------------------------
            // TIMELINE VALUES
            // -------------------------------------------------------
            frame.recordValue = 1 + i*2;
            frame.renderValue = 2 + i*2;

            // -------------------------------------------------------
            // SYNC VALUES
            // -------------------------------------------------------
            //binary semaphores
            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreInfo.flags = 0;
            
            if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to create binary semaphore!");
            }
        }
    }

    void Renderer::destroyFrames() {
        // Se não houver device inicializado, não destrói nada

        auto& device = m_vulkan.GetDevice();
        if (device == VK_NULL_HANDLE) return;

        if(m_timelineSem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, m_timelineSem, nullptr);
            m_timelineSem = VK_NULL_HANDLE;
        }

        for (auto& frame : m_frames)
        {
            // timeline counters reset (opcional)
            frame.recordValue = 0;
            frame.renderValue = 0;

            if(frame.imageAvailable != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, frame.imageAvailable, nullptr);
                frame.imageAvailable = VK_NULL_HANDLE;
            }
        }

        // limpa o vetor de frames
        m_frames.clear();
    }

    void Renderer::createMainCamera() {
        auto& swp = m_vulkan.GetSwapchain();
        auto& reg = m_world.GetRegistry();
        auto mainCameraEntity = reg.create();
        auto& camTransform = reg.emplace<TransformComponent>(mainCameraEntity, math::Vec3(0.0f), math::Quat(math::Vec3(0.0f)), math::Vec3(1.0f), physics3D::AABB(math::Vec3(0), math::Vec3(0)));
        auto& camComponent = reg.emplace<CameraComponent>(mainCameraEntity, swp.extent.width, swp.extent.height, CameraType::Perspective);
        reg.emplace<DontDestroyOnLoad>(mainCameraEntity);

        m_mainCameraUID = (uint32_t)mainCameraEntity;
    }

    void Renderer::destroyMainCamera() {
        m_world.GetRegistry().destroy((entt::entity)m_mainCameraUID);
        m_mainCameraUID = 0;
    }

    void Renderer::initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        io.Fonts->AddFontDefault();

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(m_window.GetGLFWHandle(), true);

        auto& swp = m_vulkan.GetSwapchain();

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_vulkan.GetInstance();
        init_info.PhysicalDevice = m_vulkan.GetPhysicalDevice();
        init_info.Device = m_vulkan.GetDevice();
        init_info.QueueFamily = m_vulkan.GetQueueFamilyIndices().graphicsFamily.value();
        init_info.Queue = m_vulkan.GetQueue(QueueType::GRAPHICS);
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = g_descriptorPool;
        init_info.Subpass = 0;
        init_info.MinImageCount = static_cast<uint32_t>(swp.images.size());
        init_info.ImageCount = static_cast<uint32_t>(swp.images.size());
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = nullptr;
        init_info.UseDynamicRendering = VK_TRUE;
        init_info.ApiVersion = VK_VERSION_1_3;
        
        VkPipelineRenderingCreateInfoKHR pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineInfo.colorAttachmentCount = 1;
        pipelineInfo.pColorAttachmentFormats = &swp.colorFormat;
        pipelineInfo.depthAttachmentFormat = swp.depthFormat;
        pipelineInfo.stencilAttachmentFormat = swp.stencilFormat;
        
        init_info.PipelineRenderingCreateInfo = pipelineInfo;
        ImGui_ImplVulkan_Init(&init_info);
    }

    void Renderer::destroyImGui() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Renderer::createRenderFinishedSemaphores() { 
        if(m_renderFinishedSemaphores.size() > 0) destroyRenderFinishedSemaphores();

        m_renderFinishedSemaphores.resize(m_vulkan.GetSwapchain().images.size());

        auto& device = m_vulkan.GetDevice();

        for(auto& semaphore : m_renderFinishedSemaphores) {
            VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            if (vkCreateSemaphore(device, &semInfo, nullptr, &semaphore) != VK_SUCCESS) CP_LOG_THROW("Failed to create render finished semaphore!");
        }
    }

    void Renderer::destroyRenderFinishedSemaphores() { 
        auto& device = m_vulkan.GetDevice();
        for(auto& semaphore : m_renderFinishedSemaphores) {
            if(semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }

        m_renderFinishedSemaphores.clear();
    }

    void Renderer::createCommandResources() {
        auto& device = m_vulkan.GetDevice();
        for(auto& frame : m_frames) {
            // -------------------------------------------------------
            // PRIMARY COMMAND BUFFER (um por frame)
            // -------------------------------------------------------
            {
                VkCommandPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = m_vulkan.GetQueueFamilyIndices().graphicsFamily.value();

                vkCreateCommandPool(device, &poolInfo, nullptr, &frame.primaryCmdPool);

                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = frame.primaryCmdPool;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;

                vkAllocateCommandBuffers(device, &allocInfo, &frame.primary);
            }

            // -------------------------------------------------------
            // WORKERS (command pools + secondary command buffers)
            // -------------------------------------------------------
            for (uint16_t workerID = 0; workerID < MAX_WORKERS_PER_FRAME; workerID++) {
                auto& worker = frame.workers[workerID];

                // command pool
                VkCommandPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = m_vulkan.GetQueueFamilyIndices().graphicsFamily.value();

                vkCreateCommandPool(device, &poolInfo, nullptr, &worker.pool);

                // secondary CB
                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = worker.pool;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
                allocInfo.commandBufferCount = 1;

                vkAllocateCommandBuffers(device, &allocInfo, &worker.cb);
            }

            // -------------------------------------------------------
            // IMGUI (command pool + CB)
            // -------------------------------------------------------
            {
                VkCommandPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = m_vulkan.GetQueueFamilyIndices().graphicsFamily.value();

                vkCreateCommandPool(device, &poolInfo, nullptr, &frame.imguiCmdPool);

                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = frame.imguiCmdPool;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
                allocInfo.commandBufferCount = 1;

                vkAllocateCommandBuffers(device, &allocInfo, &frame.imguiCmdBuffer);
            }
        }
    }
    
    void Renderer::destroyCommandResources() {
        auto& device = m_vulkan.GetDevice();
        for (auto& frame : m_frames) {
            // -------------------------------------------------------
            // IMGUI
            // -------------------------------------------------------
            if (frame.imguiCmdPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(device, frame.imguiCmdPool, nullptr);
                frame.imguiCmdPool = VK_NULL_HANDLE;
                frame.imguiCmdBuffer = VK_NULL_HANDLE;
            }

            // -------------------------------------------------------
            // WORKERS
            // -------------------------------------------------------
            for (auto& worker : frame.workers)
            {
                if (worker.pool != VK_NULL_HANDLE)
                {
                    vkDestroyCommandPool(device, worker.pool, nullptr);
                    worker.pool = VK_NULL_HANDLE;
                    worker.cb = VK_NULL_HANDLE;
                }
            }

            // -------------------------------------------------------
            // PRIMARY CB
            // -------------------------------------------------------
            if (frame.primaryCmdPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(device, frame.primaryCmdPool, nullptr);
                frame.primaryCmdPool = VK_NULL_HANDLE;
                frame.primary = VK_NULL_HANDLE;
            }
        }
    }
#pragma endregion INITIALIZATION_METHODS

}
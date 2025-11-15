#include "cp_api/window/renderer.hpp"
#include "cp_api/core/debug.hpp"
#include "cp_api/window/window.hpp"
#include "cp_api/core/threadPool.hpp"
#include "cp_api/window/vulkan.hpp"

namespace cp_api {

    Renderer::Renderer(Window& window) 
        : m_window(window), m_vulkan(window.GetVulkan()) {
        CP_LOG_INFO("[RENDERER] Renderer created");
        setupEventListeners();
        createFrames();
        createTransferResources();
        initImGui();

        createRenderFinishedSemaphores();

        m_renderThread = std::thread(&Renderer::renderThreadWork, this);
    }

    Renderer::~Renderer() {
        m_renderEnabled.store(false);
        
        if (m_renderThread.joinable())
            m_renderThread.join();

        vkDeviceWaitIdle(m_vulkan.GetDevice());

        destroyRenderFinishedSemaphores();
        cleanupImGui();
        destroyTransferResources();
        destroyFrames();

        CP_LOG_INFO("[RENDERER] Renderer destroyed");
    }

    void Renderer::ProcessWorld(World& world, ThreadPool& threadPool) {
        if(!isRenderEnabled()) return;

        //process world cameras

        //store current frame
        auto& frame = m_frames[m_writeFrameIndex];
        std::vector<std::future<VkResult>> futures;
        for(uint32_t i = 0; i < SIMULTANEOS_WORKERS_RECORDING_COUNT; ++i)
        {
            auto task = threadPool.Submit(TaskPriority::HIGH, [](Renderer* renderer, const Frame& f, const uint32_t& index) -> VkResult
            {
                auto result = renderer->m_vulkan.BeginCommandBuffer(f.secondaries[index], { f.renderTarget.colorFormat }, f.renderTarget.depthFormat);
                if(result != VK_SUCCESS) return result;
                {
                    //DRAW COMMANDS
                    //Process world result here and record draw calls into f.secondaries[index]
                }
                return vkEndCommandBuffer(f.secondaries[index]);
            }, this, frame, i);

            futures.push_back(std::move(task));
        }

        for(auto& future : futures)
        {
            if(future.get() != VK_SUCCESS)
            {
                CP_LOG_THROW("Window workers have failed to complete record task!");
            }
        }

        /// ----------------------------------- IMGUI
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

            // Quantos pontos no gráfico
            static const int num_points = 200;
            static float values[num_points];

            // Gerar dados senoidais animados
            static float phase = 0.0f;
            for (int i = 0; i < num_points; i++) {
                float x = (i / (float)num_points) * 2.0f * 3.14159f; // 0..2π
                values[i] = sinf(x + phase);
            }
            phase += 0.03f; // movimento

            // Criar janela ImGui
            ImGui::Begin("Gráfico Senoidal");

            ImGui::Text("Exemplo de gráfico com curva senoidal animada");
            ImGui::Separator();

            // Exibir gráfico
            ImGui::PlotLines(
                "Seno(x)",
                values,
                num_points,
                0,
                nullptr,
                -1.0f,
                1.0f,
                ImVec2(0, 150) // tamanho do gráfico
            );

            ImGui::End();

        ImGui::Render();
        auto& ImGuiCmdBuf = frame.secondaries[frame.secondaries.size() - 1];

        if(m_vulkan.BeginCommandBuffer(ImGuiCmdBuf, { frame.renderTarget.colorFormat }, frame.renderTarget.depthFormat) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to begin imgui buffer!");        
        }

        auto drawData = ImGui::GetDrawData();        
        if(drawData) ImGui_ImplVulkan_RenderDrawData(drawData, ImGuiCmdBuf);

        if(vkEndCommandBuffer(ImGuiCmdBuf) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to end ImGui command buffer!");
        }

        // ------------------------------------- END IMGUI

        //signal timeline semaphore to indicate frame is ready for rendering
        m_vulkan.SignalTimelineSemaphore(m_timelineSem, frame.recordValue);
        
        //wait timeline semaphore 
        m_vulkan.WaitTimelineSemaphores({ m_timelineSem }, { frame.renderValue });

        //update for next frame
        frame.renderValue += (2 * (uint32_t)m_frames.size());
        frame.recordValue += (2 * (uint32_t)m_frames.size());

        //advance
        m_writeFrameIndex = (m_writeFrameIndex + 1) % (uint32_t)m_frames.size();
    }

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
    }
    
    void Renderer::createFrames() {
        if(m_frames.size() > 0) return;

        m_frames.resize(m_vulkan.GetSwapchain().images.size());

        //create sync objects (timeline semaphore)
        VkSemaphoreTypeCreateInfo timelineCreateInfo{};
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = 0; // valor inicial do contador

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semInfo.pNext = &timelineCreateInfo;

        if(vkCreateSemaphore(m_vulkan.GetDevice(), &semInfo, nullptr, &m_timelineSem) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create timeline semaphore!");
        }

        //binary semaphores
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.flags = 0;
        
        int i = 0;
        for(auto& frame : m_frames) {
            int curIndex = i++;

            if(vkCreateSemaphore(m_vulkan.GetDevice(), &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to create binary semaphore!");
            }

            //timeline sequence
            frame.recordValue = 1 + curIndex*2;
            frame.renderValue = 2 + curIndex*2;
        }
        
        createRenderTargets();
        createCommandResources();
    }

    void Renderer::destroyFrames() {
        for(auto& frame : m_frames) {
            if(frame.imageAvailable != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_vulkan.GetDevice(), frame.imageAvailable, nullptr);
                frame.imageAvailable = VK_NULL_HANDLE;
            }
        }

        destroyCommandResources();
        destroyRenderTargets();

        if(m_timelineSem != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_vulkan.GetDevice(), m_timelineSem, nullptr);
            m_timelineSem = VK_NULL_HANDLE;
        }
    }

    void Renderer::createRenderTargets() {
        auto& swp = m_vulkan.GetSwapchain();
        for(auto& frame : m_frames) {           
            createRenderTargetImages(swp.extent.width, swp.extent.height, swp.format, &frame.renderTarget);
        }
    }

    void Renderer::destroyRenderTargets() {
        for(auto& frame : m_frames) {
            DestroyImage(m_vulkan.GetDevice(), m_vulkan.GetVmaAllocator(), frame.renderTarget.color);
            DestroyImage(m_vulkan.GetDevice(), m_vulkan.GetVmaAllocator(), frame.renderTarget.depth);
        }
    }

    void Renderer::createCommandResources() {
        uint32_t i = 0;
        for(auto& frame : m_frames) {
            uint32_t curIndex = i++;
            frame.cmdPool.resize(SIMULTANEOS_WORKERS_RECORDING_COUNT + 2);
            frame.secondaries.resize(SIMULTANEOS_WORKERS_RECORDING_COUNT + 1);

            // //threading
            for(uint32_t j = 0; j < SIMULTANEOS_WORKERS_RECORDING_COUNT; j++) {
                VkCommandPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = m_vulkan.GetQueueFamilyIndices().graphicsFamily.value();

                if(vkCreateCommandPool(m_vulkan.GetDevice(), &poolInfo, nullptr, &frame.cmdPool[j]) != VK_SUCCESS) {
                    CP_LOG_THROW("Failed to create command pool for frame {}", curIndex);
                }

                //buffer
                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = frame.cmdPool[j];
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
                allocInfo.commandBufferCount = 1;
                if(vkAllocateCommandBuffers(m_vulkan.GetDevice(), &allocInfo, &frame.secondaries[j]) != VK_SUCCESS) {
                    CP_LOG_THROW("Failed to allocate command buffer for frame {}", curIndex);
                }
            }

            //imgui pool
            uint32_t offset = (uint32_t)frame.cmdPool.size() - 2;
            VkCommandPoolCreateInfo imguiPoolInfo {};
            imguiPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            imguiPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            imguiPoolInfo.queueFamilyIndex = m_vulkan.GetQueueFamilyIndices().graphicsFamily.value();
            imguiPoolInfo.pNext = nullptr;
            
            if(vkCreateCommandPool(m_vulkan.GetDevice(), &imguiPoolInfo, nullptr, &frame.cmdPool[offset]) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to create ImGui command pool for frame {}", curIndex);
            }

            //imgui cmd buffer
            VkCommandBufferAllocateInfo imguiAllocInfo{};
            imguiAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            imguiAllocInfo.commandPool = frame.cmdPool[offset];
            imguiAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
            imguiAllocInfo.commandBufferCount = 1;
            if(vkAllocateCommandBuffers(m_vulkan.GetDevice(), &imguiAllocInfo, &frame.secondaries[frame.secondaries.size() - 1]) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to allocate ImGui command buffer for frame {}", curIndex);
            }

            //primary cmd pool
            VkCommandPoolCreateInfo primaryPoolInfo{};
            primaryPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            primaryPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            primaryPoolInfo.queueFamilyIndex = m_vulkan.GetQueueFamilyIndices().graphicsFamily.value();
            if(vkCreateCommandPool(m_vulkan.GetDevice(), &primaryPoolInfo, nullptr, &frame.cmdPool[offset + 1]) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to create primary command pool for frame {}", curIndex);
            }

            //primary cmd buffer
            VkCommandBufferAllocateInfo primaryAllocInfo{};
            primaryAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            primaryAllocInfo.commandPool = frame.cmdPool[offset + 1];
            primaryAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            primaryAllocInfo.commandBufferCount = 1;
            if(vkAllocateCommandBuffers(m_vulkan.GetDevice(), &primaryAllocInfo, &frame.primary) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to allocate primary command buffer for frame {}", curIndex);
            }
        }
    }

    void Renderer::destroyCommandResources() {
        for(auto& frame : m_frames) {
            for(auto& cmdPool : frame.cmdPool) {
                if(cmdPool != VK_NULL_HANDLE) {
                    vkDestroyCommandPool(m_vulkan.GetDevice(), cmdPool, nullptr);
                    cmdPool = VK_NULL_HANDLE;
                }
            }

            frame.cmdPool.clear();
            frame.secondaries.clear();
        }
    }

    void Renderer::createTransferResources() {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_vulkan.GetQueueFamilyIndices().transferFamily.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if(vkCreateCommandPool(m_vulkan.GetDevice(), &poolInfo, nullptr, &m_transferCmdPool) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create transfer command pool!");
        }

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_transferCmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if(vkAllocateCommandBuffers(m_vulkan.GetDevice(), &allocInfo, &m_transferCmdBuffer) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to allocate transfer command buffer!");
        }
    }

    void Renderer::destroyTransferResources() {
        if(m_transferCmdPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_vulkan.GetDevice(), m_transferCmdPool, nullptr);
            m_transferCmdPool = VK_NULL_HANDLE;
        }
    }

    void Renderer::initImGui() {
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
        
        if (vkCreateDescriptorPool(m_vulkan.GetDevice(), &pool_info, nullptr, &m_imguiPool) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create ImGui descriptor pool!");
        }

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

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_vulkan.GetInstance();
        init_info.PhysicalDevice = m_vulkan.GetPhysicalDevice();
        init_info.Device = m_vulkan.GetDevice();
        init_info.QueueFamily = m_vulkan.GetQueueFamilyIndices().graphicsFamily.value();
        init_info.Queue = m_vulkan.GetQueue(QueueType::GRAPHICS);
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = m_imguiPool;
        init_info.Subpass = 0;
        init_info.MinImageCount = static_cast<uint32_t>(m_vulkan.GetSwapchain().images.size());
        init_info.ImageCount = static_cast<uint32_t>(m_vulkan.GetSwapchain().images.size());
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = nullptr;
        init_info.UseDynamicRendering = VK_TRUE;
        
        VkPipelineRenderingCreateInfoKHR pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineInfo.colorAttachmentCount = 1;
        pipelineInfo.pColorAttachmentFormats = &m_frames[0].renderTarget.colorFormat;

        VkFormat depthFormat = m_frames[0].renderTarget.depthFormat;
        VkFormat stencilFormat = m_vulkan.GetStencilFormat(depthFormat);

        pipelineInfo.depthAttachmentFormat = depthFormat;
        pipelineInfo.stencilAttachmentFormat = stencilFormat;
        
        init_info.PipelineRenderingCreateInfo = pipelineInfo;

        ImGui_ImplVulkan_Init(&init_info);
    }

    void Renderer::cleanupImGui() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (m_imguiPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_vulkan.GetDevice(), m_imguiPool, nullptr);
            m_imguiPool = VK_NULL_HANDLE;
        }
    }

    void Renderer::createRenderFinishedSemaphores() {
        m_renderFinishedSemaphores.resize(m_vulkan.GetSwapchain().images.size());
        for(auto& semaphore : m_renderFinishedSemaphores) {
            VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            if (vkCreateSemaphore(m_vulkan.GetDevice(), &semInfo, nullptr, &semaphore) != VK_SUCCESS) CP_LOG_THROW("Failed to create render finished semaphore!");
        }   
    }   

    void Renderer::destroyRenderFinishedSemaphores() {
        for(auto& semaphore : m_renderFinishedSemaphores) {
            if(semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_vulkan.GetDevice(), semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }

        m_renderFinishedSemaphores.clear();
    }

    void Renderer::renderThreadWork() {
        while(!m_window.ShouldClose()) {
            if(!isRenderEnabled()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            checkSwapchainAndRecreation();

            //store current frame
            auto& frame = m_frames[m_readFrameIndex];

            //acquire next swapchain image
            uint32_t imageIndex = 0;
            VkResult result = m_vulkan.AcquireSwapchainNextImage(frame.imageAvailable, &imageIndex);
            if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            {
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
            VkImage& swapchainImage = m_vulkan.GetSwapchain().images[imageIndex];
            
            TransitionImageLayout(frame.primary, frame.renderTarget.color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            TransitionImageLayout(frame.primary, swapchainImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            TransitionImageLayout(frame.primary, frame.renderTarget.depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            // Configura attachment para limpeza
            VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            colorAttachment.imageView = frame.renderTarget.color.view;
            colorAttachment.imageLayout = frame.renderTarget.color.layout;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            
            VkClearValue clearColor = { {0.0f, 0.2f, 0.3f, 1.0f} };
            colorAttachment.clearValue = clearColor;

            VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            depthAttachment.imageView = frame.renderTarget.depth.view;
            depthAttachment.imageLayout = frame.renderTarget.depth.layout;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            VkClearValue clearDepth{};
            clearDepth.depthStencil.depth = 1.0f;
            clearDepth.depthStencil.stencil = 0;
            depthAttachment.clearValue = clearDepth;

            // Dynamic Rendering info
            VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
            renderingInfo.renderArea.offset = {0,0};
            renderingInfo.renderArea.extent = m_vulkan.GetSwapchain().extent;
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;
            renderingInfo.pDepthAttachment = &depthAttachment;
            renderingInfo.pStencilAttachment = &depthAttachment;
            
            renderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT; // IMPORTANTE

            vkCmdBeginRendering(frame.primary, &renderingInfo);
            {
                // Executa secondary command buffer
                if(m_skipAfterSwapchainRecreation.load())
                {
                    m_skipAfterSwapchainRecreation.store(false);
                }
                else
                {
                    vkCmdExecuteCommands(frame.primary, (uint32_t)frame.secondaries.size(), frame.secondaries.data());
                }
            }
            vkCmdEndRendering(frame.primary);

            TransitionImageLayout(frame.primary, frame.renderTarget.color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            CopyImage(frame.primary, frame.renderTarget.color.image, swapchainImage, m_vulkan.GetSwapchain().extent.width, m_vulkan.GetSwapchain().extent.height);
            TransitionImageLayout(frame.primary, swapchainImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            
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
            present.pSwapchains = &m_vulkan.GetSwapchain().handler;
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
        }
    }

    bool Renderer::isRenderEnabled() const {
        return m_renderEnabled.load(std::memory_order_acquire);
    }

    void Renderer::checkSwapchainAndRecreation() {
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
            destroyRenderTargets();
            destroyRenderFinishedSemaphores();

            m_vulkan.RecreateSwapchain(m_window.IsVSyncEnabled() ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR, true);

            createRenderTargets();
            createCommandResources();
            createRenderFinishedSemaphores();

            cleanupImGui();
            initImGui();

            m_swapchainIsDirty.store(false);
            m_skipAfterSwapchainRecreation.store(true);
            m_renderEnabled.store(true);
            
            CP_LOG_INFO("Swapchain recreated.");
        }
    }   

    void Renderer::createRenderTargetImages(const uint32_t& width, const uint32_t& height, const VkFormat& format, RenderTarget* target) {

        if(!target) return;

        auto colorTarget = CreateImage(m_vulkan.GetDevice(), m_vulkan.GetVmaAllocator(),
            width, height, format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY, 
            VK_IMAGE_ASPECT_COLOR_BIT);

        //depth stencil
        VkFormat depthFormats[] = 
        {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT
        };

        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        for (VkFormat dformat : depthFormats) 
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_vulkan.GetPhysicalDevice(), dformat, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                depthFormat = dformat;
                break;
            }
        }

        if(depthFormat == VK_FORMAT_UNDEFINED)
            CP_LOG_THROW("Failed to choose a depth/stencil format");

        VkImageAspectFlags depthAspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat == VK_FORMAT_D24_UNORM_S8_UINT)
        {
            depthAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        auto depthStencilTarget = CreateImage(m_vulkan.GetDevice(), m_vulkan.GetVmaAllocator(),
            width, height, depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            depthAspectMask);

        std::function<std::string(VkFormat)> depthToStr = [](VkFormat format)
        {
            switch (format)
            {
            case VK_FORMAT_D32_SFLOAT_S8_UINT: return "VK_FORMAT_D32_SFLOAT_S8_UINT";
            case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
            case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
            default: return "";
            }
        };

        CP_LOG_INFO("[WINDOW] Created render target, depth format {}", depthToStr(depthFormat));

        target->color = colorTarget;
        target->depth = depthStencilTarget;

        target->colorFormat = format;
        target->depthFormat = depthFormat;

        target->width = width;
        target->height = height;
    }
} // namespace cp_api

#include "cp_api/window/window.hpp"
#include "cp_api/core/debug.hpp"
#include <algorithm>
#include <iostream>
#include "cp_api/world/world.hpp"
#include <chrono>

namespace cp_api {

    int Window::s_glfwInitCount = 0;
    std::vector<Window*> Window::s_windows;

    Window::Window(int width, int height, const char* title) {
        if (s_glfwInitCount++ == 0) {
            if (!glfwInit()) {
                CP_LOG_ERROR("Failed to initialize GLFW");
                throw std::runtime_error("Failed to initialize GLFW");
            }
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        if (!glfwVulkanSupported()) {
            CP_LOG_ERROR("Vulkan not supported");
            throw std::runtime_error("Vulkan not supported");
        }

        m_wndHandle = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!m_wndHandle) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(m_wndHandle, this);

        // Window callbacks
        glfwSetWindowSizeCallback(m_wndHandle, GLFW_WindowSizeCallback);
        glfwSetWindowPosCallback(m_wndHandle, GLFW_WindowPosCallback);
        glfwSetWindowFocusCallback(m_wndHandle, GLFW_WindowFocusCallback);
        glfwSetWindowIconifyCallback(m_wndHandle, GLFW_WindowIconifyCallback);
        glfwSetWindowMaximizeCallback(m_wndHandle, GLFW_WindowMaximizeCallback);
        glfwSetWindowCloseCallback(m_wndHandle, GLFW_WindowCloseCallback);

        centerWindowOnScreen(m_wndHandle);

        s_windows.push_back(this);

        m_input = std::make_unique<InputManager>(m_wndHandle);
        m_vulkan = std::make_unique<Vulkan>(m_wndHandle);


        GetEventDispatcher().Subscribe<onWindowMinimizeEvent>([this](const onWindowMinimizeEvent& e) {
            CP_LOG_INFO("Window minimized: {}", e.minimized);
            m_renderEnabled = !e.minimized;
        });

        GetEventDispatcher().Subscribe<onWindowResizeEvent>([this](const onWindowResizeEvent& e) {
            m_lastDragTime = std::chrono::steady_clock::now();
            m_isDragging.store(true);
            CP_LOG_INFO("Window resize event received. Dragging: {}", m_isDragging.load() ? "true" : "false");
        });

        m_renderThread = std::thread(&Window::renderWorker, this);
    }

    Window::~Window() {
        m_renderThread.join();
        s_windows.erase(std::remove(s_windows.begin(), s_windows.end(), this), s_windows.end());
        if (m_wndHandle)
            glfwDestroyWindow(m_wndHandle);

        if (--s_glfwInitCount == 0)
            glfwTerminate();
    }

    // ----------------- Getters ----------------
    bool Window::ShouldClose() const { return glfwWindowShouldClose(m_wndHandle); }
    int Window::GetWidth() const { int w, h; glfwGetWindowSize(m_wndHandle, &w, &h); return w; }
    int Window::GetHeight() const { int w, h; glfwGetWindowSize(m_wndHandle, &w, &h); return h; }
    float Window::GetAspectRatio() const { int w = GetWidth(), h = GetHeight(); return h != 0 ? float(w)/h : 1.f; }
    bool Window::IsFocused() const { return glfwGetWindowAttrib(m_wndHandle, GLFW_FOCUSED) == GLFW_TRUE; }
    bool Window::IsMinimized() const { return glfwGetWindowAttrib(m_wndHandle, GLFW_ICONIFIED) == GLFW_TRUE; }
    bool Window::IsVisible() const { return glfwGetWindowAttrib(m_wndHandle, GLFW_VISIBLE) == GLFW_TRUE; }

    // ----------------- Appearance -----------------
    void Window::SetTitle(const std::string& title) { glfwSetWindowTitle(m_wndHandle, title.c_str()); }
    void Window::SetOpacity(float alpha) { glfwSetWindowOpacity(m_wndHandle, alpha); }
    void Window::SetAlwaysOnTop(bool enable) { glfwSetWindowAttrib(m_wndHandle, GLFW_FLOATING, enable ? GLFW_TRUE : GLFW_FALSE); }

    // ----------------- Clipboard -----------------
    void Window::SetClipboardText(const char* text) { glfwSetClipboardString(m_wndHandle, text); }
    std::string Window::GetClipboardText() const { 
        const char* txt = glfwGetClipboardString(m_wndHandle); 
        return txt ? txt : ""; 
    }

    // ----------------- DPI -----------------
    void Window::GetContentScale(float& x, float& y) const {
        glfwGetWindowContentScale(m_wndHandle, &x, &y);
    }

    // ----------------- POOL AND UPDATES -----------------
    void Window::Update() { 
        glfwPollEvents(); 
        m_input->update();

        if(m_isDragging.load()) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastDragTime).count();
            if (duration > 200) { // 200 ms after last resize event
                m_isDragging.store(false);
                CP_LOG_INFO("Window resize drag ended. {}", m_isDragging.load() ? "Still dragging." : "Not dragging.");
                m_swapchainIsDirty.store(true);
                m_vulkan->RecreateSwapchain(true);
            }
        }
    }

    void Window::ProcessWorld(World& world) {
        // Placeholder for future window-world interaction logic
    }

    void Window::renderWorker() {
        while(!ShouldClose()) {
            // Placeholder for rendering logic
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Simulate ~60 FPS
        }
    }

    // ----------------- Window Mode -----------------
    void Window::SetWindowMode(WindowMode mode) {
        if (mode == m_mode) return;

        GLFWmonitor* monitor = getMonitorForWindow(m_wndHandle);
        if (!monitor) monitor = glfwGetPrimaryMonitor();

        const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
        int mx, my;
        glfwGetMonitorPos(monitor, &mx, &my);

        if (m_mode == WindowMode::Windowed) {
            glfwGetWindowPos(m_wndHandle, &m_prevX, &m_prevY);
            glfwGetWindowSize(m_wndHandle, &m_prevW, &m_prevH);
        }

        switch (mode) {
            case WindowMode::Windowed:
                glfwSetWindowAttrib(m_wndHandle, GLFW_DECORATED, GLFW_TRUE);
                glfwSetWindowMonitor(m_wndHandle, nullptr, m_prevX, m_prevY, m_prevW, m_prevH, 0);
                break;
            case WindowMode::Borderless:
                glfwSetWindowAttrib(m_wndHandle, GLFW_DECORATED, GLFW_FALSE);
                glfwSetWindowMonitor(m_wndHandle, nullptr, mx, my, videoMode->width, videoMode->height, 0);
                break;
            case WindowMode::Fullscreen:
                glfwSetWindowMonitor(m_wndHandle, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
                break;
        }

        m_mode = mode;

        m_eventDispatcher.Emit<onWindowModeChangedEvent>({ m_wndHandle, m_mode });
    }

    void Window::ToggleFullscreen() {
        if (m_mode == WindowMode::Fullscreen)
            SetWindowMode(WindowMode::Windowed);
        else
            SetWindowMode(WindowMode::Fullscreen);
    }

    // ----------------- Callbacks -----------------
    void Window::GLFW_WindowSizeCallback(GLFWwindow* window, int width, int height) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_eventDispatcher.Emit<onWindowResizeEvent>(onWindowResizeEvent{window, width, height});
        }
    }
    void Window::GLFW_WindowPosCallback(GLFWwindow* window, int xpos, int ypos) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_eventDispatcher.Emit<onWindowMoveEvent>(onWindowMoveEvent{window, xpos, ypos});
        }
    }
    void Window::GLFW_WindowFocusCallback(GLFWwindow* window, int focused) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_eventDispatcher.Emit<onWindowFocusEvent>(onWindowFocusEvent{window, focused == GLFW_TRUE});
        }
    }
    void Window::GLFW_WindowIconifyCallback(GLFWwindow* window, int iconified) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_eventDispatcher.Emit<onWindowMinimizeEvent>(onWindowMinimizeEvent{window, iconified == GLFW_TRUE});
        }
    }
    void Window::GLFW_WindowMaximizeCallback(GLFWwindow* window, int maximized) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_eventDispatcher.Emit<onWindowRestoreEvent>(onWindowRestoreEvent{window});
        }
    }
    void Window::GLFW_WindowCloseCallback(GLFWwindow* window) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            self->m_eventDispatcher.Emit<onWindowCloseEvent>(onWindowCloseEvent{window});
        }
    }

    // ----------------- Helpers -----------------
    GLFWmonitor* Window::getMonitorForWindow(GLFWwindow* window) const {
        int wx, wy, ww, wh;
        glfwGetWindowPos(window, &wx, &wy);
        glfwGetWindowSize(window, &ww, &wh);

        int count;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if (!monitors || count == 0) return glfwGetPrimaryMonitor();

        GLFWmonitor* bestMonitor = nullptr;
        int bestOverlap = 0;

        for (int i = 0; i < count; ++i) {
            int mx, my;
            glfwGetMonitorPos(monitors[i], &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            int mw = mode->width;
            int mh = mode->height;

            int overlap =
                std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
                std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

            if (overlap > bestOverlap) {
                bestOverlap = overlap;
                bestMonitor = monitors[i];
            }
        }

        return bestMonitor ? bestMonitor : glfwGetPrimaryMonitor();
    }

    void Window::centerWindowOnScreen(GLFWwindow* window) const {
        GLFWmonitor* monitor = getMonitorForWindow(window);
        const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);

        int mx, my;
        glfwGetMonitorPos(monitor, &mx, &my);

        int w, h;
        glfwGetWindowSize(window, &w, &h);

        int xpos = mx + (vidmode->width - w) / 2;
        int ypos = my + (vidmode->height - h) / 2;

        glfwSetWindowPos(window, xpos, ypos);
    }

} // namespace cp_api

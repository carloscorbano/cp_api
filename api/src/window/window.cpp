#include "cp_api/window/window.hpp"
#include "cp_api/core/debug.hpp"
#include <algorithm>
#include <iostream>

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
    }

    Window::~Window() {
        s_windows.erase(std::remove(s_windows.begin(), s_windows.end(), this), s_windows.end());
        if (m_wndHandle)
            glfwDestroyWindow(m_wndHandle);

        if (--s_glfwInitCount == 0)
            glfwTerminate();
    }

    // ----------------- Poll / State -----------------
    void Window::Update() const { 
        glfwPollEvents(); 
        m_input->update();
    }

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
        if (m_callbacks.onModeChanged) m_callbacks.onModeChanged(m_wndHandle, m_mode);
    }

    void Window::ToggleFullscreen() {
        if (m_mode == WindowMode::Fullscreen)
            SetWindowMode(WindowMode::Windowed);
        else
            SetWindowMode(WindowMode::Fullscreen);
    }

    // // ----------------- Global hotkeys (F10/F11/F12) -----------------
    // void Window::PollGlobalHotkeys() {
    //     for (auto* win : s_windows) {
    //         if (glfwGetKey(win->m_wndHandle, GLFW_KEY_F10) == GLFW_PRESS)
    //             win->SetWindowMode(WindowMode::Borderless);
    //         if (glfwGetKey(win->m_wndHandle, GLFW_KEY_F11) == GLFW_PRESS)
    //             win->SetWindowMode(WindowMode::Fullscreen);
    //         if (glfwGetKey(win->m_wndHandle, GLFW_KEY_F12) == GLFW_PRESS)
    //             win->SetWindowMode(WindowMode::Windowed);
    //     }
    // }

    // ----------------- Callbacks -----------------
    void Window::GLFW_WindowSizeCallback(GLFWwindow* window, int width, int height) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            if (self->m_callbacks.onResize) self->m_callbacks.onResize(window, width, height);
        }
    }
    void Window::GLFW_WindowPosCallback(GLFWwindow* window, int xpos, int ypos) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            if (self->m_callbacks.onMove) self->m_callbacks.onMove(window, xpos, ypos);
        }
    }
    void Window::GLFW_WindowFocusCallback(GLFWwindow* window, int focused) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            if (self->m_callbacks.onFocus) self->m_callbacks.onFocus(window, focused == GLFW_TRUE);
        }
    }
    void Window::GLFW_WindowIconifyCallback(GLFWwindow* window, int iconified) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            if (self->m_callbacks.onMinimize) self->m_callbacks.onMinimize(window, iconified == GLFW_TRUE);
        }
    }
    void Window::GLFW_WindowMaximizeCallback(GLFWwindow* window, int maximized) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            if (self->m_callbacks.onMaximize) self->m_callbacks.onMaximize(window, maximized == GLFW_TRUE);
        }
    }
    void Window::GLFW_WindowCloseCallback(GLFWwindow* window) {
        if (auto self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
            if (self->m_callbacks.onClose) self->m_callbacks.onClose(window);
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

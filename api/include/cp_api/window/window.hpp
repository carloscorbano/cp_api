#pragma once

#include "renderer.hpp"
#include "inputManager.hpp"
#include "vulkan.hpp"
#include "imgui.inc.hpp"
#include "cp_api/core/events.hpp"
#include <functional>
#include <string>
#include <stdexcept>
#include <vector>
#include <memory>
#include <chrono>

namespace cp_api {

    enum class WindowMode {
        Windowed,
        Borderless,
        Fullscreen
    };

    struct onWindowDragStopEvent : public Event {
        GLFWwindow* window;
        onWindowDragStopEvent(GLFWwindow* wnd) : window(wnd) {}
    };

    struct onWindowModeChangedEvent : public Event {
        GLFWwindow* window;
        WindowMode mode;
        onWindowModeChangedEvent(GLFWwindow* wnd, WindowMode m) : window(wnd), mode(m) {}
    };

    struct onWindowResizeEvent : public Event {
        GLFWwindow* window;
        int width;
        int height;
        onWindowResizeEvent(GLFWwindow* wnd, int w, int h) : window(wnd), width(w), height(h) {}
    };

    struct onWindowMoveEvent : public Event {
        GLFWwindow* window;
        int xpos;
        int ypos;
        onWindowMoveEvent(GLFWwindow* wnd, int x, int y) : window(wnd), xpos(x), ypos(y) {}
    };

    struct onWindowFocusEvent : public Event {
        GLFWwindow* window;
        bool focused;
        onWindowFocusEvent(GLFWwindow* wnd, bool f) : window(wnd), focused(f) {}
    };

    struct onWindowMinimizeEvent : public Event {
        GLFWwindow* window;
        bool minimized;
        onWindowMinimizeEvent(GLFWwindow* wnd, bool m) : window(wnd), minimized(m) {}
    };

    struct onWindowRestoreEvent : public Event {
        GLFWwindow* window;
        onWindowRestoreEvent(GLFWwindow* wnd) : window(wnd) {}
    };

    struct onWindowCloseEvent : public Event {
        GLFWwindow* window;
        onWindowCloseEvent(GLFWwindow* wnd) : window(wnd) {}
    };

    class World;
    class ThreadPool;
    class Window {
    public:
        Window(int width, int height, const char* title);
        ~Window();

        Window(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(const Window&) = delete;
        Window& operator=(Window&&) = delete;

        // Window mode
        void SetWindowMode(WindowMode mode);
        void ToggleFullscreen();
        WindowMode GetWindowMode() const { return m_mode; }

        bool ShouldClose() const;
        void Update();

        void ProcessWorld(World& world, ThreadPool& threadPool);

        // Window state
        int GetWidth() const;
        int GetHeight() const;
        float GetAspectRatio() const;
        bool IsFocused() const;
        bool IsMinimized() const;
        bool IsVisible() const;

        // Appearance
        void SetTitle(const std::string& title);
        void SetOpacity(float alpha);
        void SetAlwaysOnTop(bool enable);

        // Clipboard
        void SetClipboardText(const char* text);
        std::string GetClipboardText() const;

        // DPI
        void GetContentScale(float& x, float& y) const;

        GLFWwindow* GetHandle() const { return m_wndHandle; }

        InputManager& GetInput() { return *m_input; }
        Vulkan& GetVulkan() { return *m_vulkan; }

        EventDispatcher& GetEventDispatcher() { return m_eventDispatcher; }
        Renderer& GetRenderer() { return *m_renderer; }

        bool IsVSyncEnabled() const { return m_vsyncEnabled; }
        void SetVSyncEnabled(bool enabled) { m_vsyncEnabled = enabled; }

        bool IsDragging() const { return m_isDragging.load(); }

        GLFWwindow* GetGLFWHandle() const { return m_wndHandle; }

    private:
        static void GLFW_WindowSizeCallback(GLFWwindow* window, int width, int height);
        static void GLFW_WindowPosCallback(GLFWwindow* window, int xpos, int ypos);
        static void GLFW_WindowFocusCallback(GLFWwindow* window, int focused);
        static void GLFW_WindowIconifyCallback(GLFWwindow* window, int iconified);
        static void GLFW_WindowMaximizeCallback(GLFWwindow* window, int maximized);
        static void GLFW_WindowCloseCallback(GLFWwindow* window);

        GLFWmonitor* getMonitorForWindow(GLFWwindow* window) const;
        void centerWindowOnScreen(GLFWwindow* window) const;

    private:
        GLFWwindow* m_wndHandle = nullptr;
        WindowMode m_mode = WindowMode::Windowed;
        EventDispatcher m_eventDispatcher;

        int m_prevX = 0, m_prevY = 0;
        int m_prevW = 0, m_prevH = 0;

        static int s_glfwInitCount;
        static std::vector<Window*> s_windows; // lista global de janelas para hotkeys

        std::unique_ptr<InputManager> m_input;
        std::unique_ptr<Vulkan> m_vulkan;
        std::unique_ptr<Renderer> m_renderer;
        bool m_vsyncEnabled = true;

        std::chrono::steady_clock::time_point m_lastDragTime;
        std::atomic<bool> m_isDragging { false };
    };

} // namespace cp_api

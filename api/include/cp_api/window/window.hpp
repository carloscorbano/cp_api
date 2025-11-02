#pragma once

#include "glfw.inc.hpp"
#include "inputManager.hpp"
#include <functional>
#include <string>
#include <stdexcept>
#include <vector>
#include <memory>

namespace cp_api {

enum class WindowMode {
    Windowed,
    Borderless,
    Fullscreen
};

struct WindowCallbacks {
    std::function<void(GLFWwindow*, int, int)> onResize;
    std::function<void(GLFWwindow*, int, int)> onMove;
    std::function<void(GLFWwindow*, bool)> onFocus;
    std::function<void(GLFWwindow*, bool)> onMinimize;
    std::function<void(GLFWwindow*, bool)> onMaximize;
    std::function<void(GLFWwindow*, WindowMode)> onModeChanged;
    std::function<void(GLFWwindow*)> onClose;
};

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) = delete;

    // Main loop
    void Update() const;
    bool ShouldClose() const;

    // Window mode
    void SetWindowMode(WindowMode mode);
    void ToggleFullscreen();
    WindowMode GetWindowMode() const { return m_mode; }

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

    // Callbacks
    void SetCallbacks(const WindowCallbacks& callbacks) { m_callbacks = callbacks; }

    GLFWwindow* GetHandle() const { return m_wndHandle; }

    // // Static: handle toggles de fullscreen via tecla
    // static void PollGlobalHotkeys();

    InputManager& GetInput() { return *m_input; }

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
    WindowCallbacks m_callbacks;

    int m_prevX = 0, m_prevY = 0;
    int m_prevW = 0, m_prevH = 0;

    static int s_glfwInitCount;
    static std::vector<Window*> s_windows; // lista global de janelas para hotkeys

    std::unique_ptr<InputManager> m_input;
};

} // namespace cp_api

#pragma once

#include <GLFW/glfw3.h>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <functional>

namespace cp_api {

enum class KeyState { None, Pressed, Released, Held };

struct GamepadState {
    bool present = false;
    std::vector<float> axes;
    std::vector<unsigned char> buttons;
};

class InputManager {
public:
    explicit InputManager(GLFWwindow* window);
    ~InputManager();

    void update();

    // === Consulta de estados ===
    bool isKeyDown(int key) const;
    bool isKeyPressed(int key) const;
    bool isKeyReleased(int key) const;

    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonReleased(int button) const;

    bool isActionDown(const std::string& action) const;
    bool isActionPressed(const std::string& action) const;
    bool isActionReleased(const std::string& action) const;

    void getMousePosition(double& x, double& y) const;
    const GamepadState& getGamepadState(int jid) const;

    // === Bindings de ações ===
    void bindKey(const std::string& action, int key);
    void bindMouseButton(const std::string& action, int button);
    void bindGamepadButton(const std::string& action, int jid, int button);

    void clearBindings();
    void clearBinding(const std::string& action);

    // === Callbacks ===
    std::function<void(const std::string& action, KeyState state)> onAction;
    std::function<void(int key, KeyState state)> onKey;
    std::function<void(int button, KeyState state)> onMouseButton;

private:
    GLFWwindow* m_window;
    std::unordered_map<int, KeyState> m_keyStates;
    std::unordered_map<int, KeyState> m_mouseButtonStates;
    std::unordered_map<int, GamepadState> m_gamepads;

    struct GamepadBinding {
        int jid;
        int button;
    };

    std::unordered_map<std::string, std::unordered_set<int>> m_keyBindings;
    std::unordered_map<std::string, std::unordered_set<int>> m_mouseBindings;
    std::unordered_map<std::string, std::vector<GamepadBinding>> m_gamepadBindings;

    void pollGamepads();
    void processBindings();

    // helpers
    static bool isStateDown(KeyState state);
    static bool isStatePressed(KeyState state);
    static bool isStateReleased(KeyState state);
};

} // namespace cp_api

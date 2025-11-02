#include "cp_api/window/inputManager.hpp"
#include <algorithm>

namespace cp_api {

InputManager::InputManager(GLFWwindow* window)
    : m_window(window) {
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
    glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
}

InputManager::~InputManager() = default;

void InputManager::update() {
    // === Teclado ===
    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
        int state = glfwGetKey(m_window, key);
        KeyState prev = m_keyStates[key];

        KeyState newState = KeyState::None;
        if (state == GLFW_PRESS)
            newState = (prev == KeyState::Pressed || prev == KeyState::Held) ? KeyState::Held : KeyState::Pressed;
        else if (state == GLFW_RELEASE && (prev == KeyState::Pressed || prev == KeyState::Held))
            newState = KeyState::Released;

        m_keyStates[key] = newState;

        if (newState != KeyState::None && onKey)
            onKey(key, newState);
    }

    // === Mouse ===
    for (int button = GLFW_MOUSE_BUTTON_1; button <= GLFW_MOUSE_BUTTON_LAST; ++button) {
        int state = glfwGetMouseButton(m_window, button);
        KeyState prev = m_mouseButtonStates[button];

        KeyState newState = KeyState::None;
        if (state == GLFW_PRESS)
            newState = (prev == KeyState::Pressed || prev == KeyState::Held) ? KeyState::Held : KeyState::Pressed;
        else if (state == GLFW_RELEASE && (prev == KeyState::Pressed || prev == KeyState::Held))
            newState = KeyState::Released;

        m_mouseButtonStates[button] = newState;

        if (newState != KeyState::None && onMouseButton)
            onMouseButton(button, newState);
    }

    // === Gamepads ===
    pollGamepads();

    // === Processa Bindings de Ações ===
    processBindings();
}

void InputManager::pollGamepads() {
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickPresent(jid) == GLFW_TRUE) {
            GamepadState& gp = m_gamepads[jid];
            gp.present = true;

            int axisCount, buttonCount;
            const float* axes = glfwGetJoystickAxes(jid, &axisCount);
            const unsigned char* buttons = glfwGetJoystickButtons(jid, &buttonCount);

            gp.axes.assign(axes, axes + axisCount);
            gp.buttons.assign(buttons, buttons + buttonCount);
        } else {
            m_gamepads[jid].present = false;
        }
    }
}

void InputManager::processBindings() {
    // Teclado
    for (const auto& [action, keys] : m_keyBindings) {
        for (int key : keys) {
            auto it = m_keyStates.find(key);
            if (it != m_keyStates.end() && it->second != KeyState::None && onAction)
                onAction(action, it->second);
        }
    }

    // Mouse
    for (const auto& [action, buttons] : m_mouseBindings) {
        for (int button : buttons) {
            auto it = m_mouseButtonStates.find(button);
            if (it != m_mouseButtonStates.end() && it->second != KeyState::None && onAction)
                onAction(action, it->second);
        }
    }

    // Gamepads
    for (const auto& [action, bindings] : m_gamepadBindings) {
        for (const auto& gb : bindings) {
            const auto& gp = getGamepadState(gb.jid);
            if (!gp.present || gb.button >= (int)gp.buttons.size())
                continue;

            KeyState state = (gp.buttons[gb.button] == GLFW_PRESS) ? KeyState::Pressed : KeyState::None;
            if (state != KeyState::None && onAction)
                onAction(action, state);
        }
    }
}

// === Helper methods for KeyState ===
bool InputManager::isStateDown(KeyState state) { return state == KeyState::Pressed || state == KeyState::Held; }
bool InputManager::isStatePressed(KeyState state) { return state == KeyState::Pressed; }
bool InputManager::isStateReleased(KeyState state) { return state == KeyState::Released; }

// === Teclado ===
bool InputManager::isKeyDown(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && isStateDown(it->second);
}
bool InputManager::isKeyPressed(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && isStatePressed(it->second);
}
bool InputManager::isKeyReleased(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && isStateReleased(it->second);
}

// === Mouse ===
bool InputManager::isMouseButtonDown(int button) const {
    auto it = m_mouseButtonStates.find(button);
    return it != m_mouseButtonStates.end() && isStateDown(it->second);
}
bool InputManager::isMouseButtonPressed(int button) const {
    auto it = m_mouseButtonStates.find(button);
    return it != m_mouseButtonStates.end() && isStatePressed(it->second);
}
bool InputManager::isMouseButtonReleased(int button) const {
    auto it = m_mouseButtonStates.find(button);
    return it != m_mouseButtonStates.end() && isStateReleased(it->second);
}

// === Ações ===
bool InputManager::isActionDown(const std::string& action) const {
    if (auto it = m_keyBindings.find(action); it != m_keyBindings.end())
        for (int key : it->second) if (isKeyDown(key)) return true;

    if (auto it = m_mouseBindings.find(action); it != m_mouseBindings.end())
        for (int btn : it->second) if (isMouseButtonDown(btn)) return true;

    if (auto it = m_gamepadBindings.find(action); it != m_gamepadBindings.end())
        for (const auto& gb : it->second) {
            const auto& gp = getGamepadState(gb.jid);
            if (gp.present && gb.button < (int)gp.buttons.size() && gp.buttons[gb.button] == GLFW_PRESS)
                return true;
        }

    return false;
}

bool InputManager::isActionPressed(const std::string& action) const {
    if (auto it = m_keyBindings.find(action); it != m_keyBindings.end())
        for (int key : it->second) if (isKeyPressed(key)) return true;

    if (auto it = m_mouseBindings.find(action); it != m_mouseBindings.end())
        for (int btn : it->second) if (isMouseButtonPressed(btn)) return true;

    if (auto it = m_gamepadBindings.find(action); it != m_gamepadBindings.end())
        for (const auto& gb : it->second) {
            const auto& gp = getGamepadState(gb.jid);
            if (gp.present && gb.button < (int)gp.buttons.size() && gp.buttons[gb.button] == GLFW_PRESS)
                return true;
        }

    return false;
}

bool InputManager::isActionReleased(const std::string& action) const {
    if (auto it = m_keyBindings.find(action); it != m_keyBindings.end())
        for (int key : it->second) if (isKeyReleased(key)) return true;

    if (auto it = m_mouseBindings.find(action); it != m_mouseBindings.end())
        for (int btn : it->second) if (isMouseButtonReleased(btn)) return true;

    if (auto it = m_gamepadBindings.find(action); it != m_gamepadBindings.end())
        for (const auto& gb : it->second) {
            const auto& gp = getGamepadState(gb.jid);
            if (gp.present && gb.button < (int)gp.buttons.size() && gp.buttons[gb.button] == GLFW_RELEASE)
                return true;
        }

    return false;
}

// === Mouse e Gamepad ===
void InputManager::getMousePosition(double& x, double& y) const { glfwGetCursorPos(m_window, &x, &y); }

const GamepadState& InputManager::getGamepadState(int jid) const {
    static GamepadState empty;
    auto it = m_gamepads.find(jid);
    return it != m_gamepads.end() ? it->second : empty;
}

// === Bindings ===
void InputManager::bindKey(const std::string& action, int key) { m_keyBindings[action].insert(key); }
void InputManager::bindMouseButton(const std::string& action, int button) { m_mouseBindings[action].insert(button); }
void InputManager::bindGamepadButton(const std::string& action, int jid, int button) { m_gamepadBindings[action].push_back({jid, button}); }

void InputManager::clearBindings() { m_keyBindings.clear(); m_mouseBindings.clear(); m_gamepadBindings.clear(); }
void InputManager::clearBinding(const std::string& action) {
    m_keyBindings.erase(action);
    m_mouseBindings.erase(action);
    m_gamepadBindings.erase(action);
}

} // namespace cp_api

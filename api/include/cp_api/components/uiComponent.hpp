#pragma once

#include "cp_api/window/imgui.inc.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <algorithm>

namespace cp_api
{
    /**
     * @brief Classe base para todos os elementos de UI
     */
    struct UICanvasChildren
    {
        virtual void Draw() = 0;
        virtual std::unique_ptr<UICanvasChildren> Clone() const = 0;
        virtual ~UICanvasChildren() = default;

        bool enabled { true };
        bool sameLine { false };
        ImFont* font = nullptr;

        struct SamelineSettings
        {
            float offset = 0.0f;
            float spacing = -1.0f;
        } sameLineSettings;
    };

    /**
     * @brief Representa uma janela ImGui que pode conter elementos
     */
    struct UICanvas
    {
        std::string name;
        bool open = true;
        ImGuiWindowFlags flags = ImGuiWindowFlags_None;

        ImVec2 size = ImVec2(200, 100);
        ImVec2 pos = ImVec2(50, 50);
        ImVec2 pivot = ImVec2(0, 0);

        std::vector<std::unique_ptr<UICanvasChildren>> children;

        UICanvas() = default;

        // Delete copy
        UICanvas(const UICanvas&) = delete;
        UICanvas& operator=(const UICanvas&) = delete;

        // Move
        UICanvas(UICanvas&& other) noexcept
            : name(std::move(other.name)), open(other.open), flags(other.flags),
            size(other.size), pos(other.pos), children(std::move(other.children)) {}

        UICanvas& operator=(UICanvas&& other) noexcept
        {
            if (this != &other)
            {
                name = std::move(other.name);
                open = other.open;
                flags = other.flags;
                size = other.size;
                pos = other.pos;
                children = std::move(other.children);
            }
            return *this;
        }

        template<typename T, typename... Args>
        T& AddChild(Args&&... args)
        {
            static_assert(std::is_base_of<UICanvasChildren, T>::value, "T must derive from UICanvasChildren");

            auto child = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *child; // Pegamos a referência antes de mover
            children.push_back(std::move(child));
            return ref;
        }
    };

    /**
     * @brief Botão simples
     */
    struct UIButton : UICanvasChildren
    {
        std::string label;
        ImVec2 size = ImVec2(100, 50);
        std::function<void()> onClickEvent;

        void Draw() override
        {
            if (ImGui::Button(label.c_str(), size) && onClickEvent)
                onClickEvent();
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIButton>(*this);
        }
    };

    /**
     * @brief Texto simples
     */
    struct UIText : UICanvasChildren
    {
        std::string text;

        void Draw() override
        {
            ImGui::TextUnformatted(text.c_str());
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIText>(*this);
        }
    };

    /**
     * @brief Checkbox
     */
    struct UICheckBox : UICanvasChildren
    {
        std::string label;
        bool* value = nullptr;
        std::function<void(bool)> onChange;

        void Draw() override
        {
            if (!value) return;

            if (ImGui::Checkbox(label.c_str(), value) && onChange)
                onChange(*value);
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UICheckBox>(*this);
        }
    };

    /**
     * @brief Radio Button
     */
    struct UIRadioButton : UICanvasChildren
    {
        std::string label;
        int* current = nullptr;
        int value = 0;
        std::function<void(int)> onChange;

        void Draw() override
        {
            if (!current) return;

            if (ImGui::RadioButton(label.c_str(), *current == value))
            {
                *current = value;
                if (onChange) onChange(*current);
            }
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIRadioButton>(*this);
        }
    };

    /**
     * @brief Slider Float
     */
    struct UISliderFloat : UICanvasChildren
    {
        std::string label;
        float* value = nullptr;
        float min = 0.0f, max = 1.0f;

        void Draw() override
        {
            if (!value) return;
            ImGui::SliderFloat(label.c_str(), value, min, max, "%.3f");
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UISliderFloat>(*this);
        }
    };

    /**
     * @brief Drag Float
     */
    struct UIDragFloat : UICanvasChildren
    {
        std::string label;
        float* value = nullptr;
        float speed = 0.1f;
        float min = 0.0f, max = 0.0f;

        void Draw() override
        {
            if (!value) return;
            ImGui::DragFloat(label.c_str(), value, speed, min, max, "%.3f");
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIDragFloat>(*this);
        }
    };

    /**
     * @brief Input Text
     */
    struct UIInputText : UICanvasChildren
    {
        std::string label;
        char buffer[256];
        std::function<void(const std::string&)> onChange;

        UIInputText() { std::fill(std::begin(buffer), std::end(buffer), '\0'); }

        void Draw() override
        {
            if (ImGui::InputText(label.c_str(), buffer, IM_ARRAYSIZE(buffer)))
            {
                if (onChange) onChange(std::string(buffer));
            }
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIInputText>(*this);
        }
    };

    /**
     * @brief ComboBox
     */
    struct UIComboBox : UICanvasChildren
    {
        std::string label;
        int* currentIndex = nullptr;
        std::vector<std::string> items;

        void Draw() override
        {
            if (!currentIndex) return;

            if (ImGui::BeginCombo(label.c_str(), items[*currentIndex].c_str()))
            {
                for (int i = 0; i < items.size(); i++)
                {
                    bool isSelected = (*currentIndex == i);
                    if (ImGui::Selectable(items[i].c_str(), isSelected))
                        *currentIndex = i;

                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIComboBox>(*this);
        }
    };

    /**
     * @brief Color Picker
     */
    struct UIColorPicker : UICanvasChildren
    {
        std::string label;
        float color[3] = {1.0f, 1.0f, 1.0f};

        void Draw() override
        {
            ImGui::ColorEdit3(label.c_str(), color);
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIColorPicker>(*this);
        }
    };

    /**
     * @brief Progress Bar
     */
    struct UIProgressBar : UICanvasChildren
    {
        float fraction = 0.0f;
        ImVec2 size = ImVec2(0, 0);
        std::string overlay;

        void Draw() override
        {
            ImGui::ProgressBar(fraction, size, overlay.empty() ? nullptr : overlay.c_str());
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIProgressBar>(*this);
        }
    };

    /**
     * @brief Separator
     */
    struct UISeparator : UICanvasChildren
    {
        void Draw() override { ImGui::Separator(); }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UISeparator>(*this);
        }
    };

    /**
     * @brief Collapsing Header
     */
    struct UICollapsingHeader : UICanvasChildren
    {
        std::string label;

        void Draw() override
        {
            ImGui::CollapsingHeader(label.c_str());
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UICollapsingHeader>(*this);
        }
    };

    /**
     * @brief Menu Item
     */
    struct UIMenuItem : UICanvasChildren
    {
        std::string label;
        bool selected = false;
        std::function<void()> onClick;

        void Draw() override
        {
            if (ImGui::MenuItem(label.c_str(), nullptr, selected))
            {
                if (onClick) onClick();
            }
        }

        std::unique_ptr<UICanvasChildren> Clone() const override
        {
            return std::make_unique<UIMenuItem>(*this);
        }
    };

}
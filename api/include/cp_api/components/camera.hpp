#pragma once

#include "cp_api/core/math.hpp"
#include "cp_api/window/renderTarget.hpp"

namespace cp_api {
    enum class CameraType { Perspective, Orthographic };
    struct CameraComponent {
        CameraComponent(uint32_t w = 0, uint32_t h = 0, CameraType t = CameraType::Perspective, bool autoAsp = true)
            : width(w), height(h), type(t), autoAspect(autoAsp)
        {
            if (autoAspect && width > 0 && height > 0) {
                aspect = static_cast<float>(width) / static_cast<float>(height);
            }
        }

        // Projection
        float fov = 60.0f;
        float aspect = 16.0f / 9.0f;
        float zNear = 0.1f;
        float zFar = 1000.0f;
        bool autoAspect = true;

        uint32_t width, height;
        
        CameraType type = CameraType::Perspective;

        // Ortho
        float orthoSize = 10.0f;

        // State
        bool active = true;
        bool primary = false;

        // Movement / control
        float moveSpeed = 5.0f;
        float lookSpeed = 0.1f;

        math::Mat4 GetProjectionMatrix() const {
            if (type == CameraType::Perspective) {
                return glm::perspective(glm::radians(fov), aspect, zNear, zFar);
            } else {
                float half = orthoSize * 0.5f;
                return glm::ortho(-half * aspect, half * aspect, -half, half, zNear, zFar);
            }
        }
    };
} // nam2espace cp_api 
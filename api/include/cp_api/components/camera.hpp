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
        uint32_t viewMask = 0xFFFFFFFF;

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
            glm::mat4 proj;

            if (type == CameraType::Perspective) {
                proj = glm::perspective(glm::radians(fov), aspect, zNear, zFar);
            } else {
                float half = orthoSize * 0.5f;
                proj = glm::ortho(-half * aspect, half * aspect, -half, half, zNear, zFar);
            }

            proj[1][1] *= -1.0f; // necessário para Vulkan

            return proj;
        }

        math::Mat4 GetViewMatrix(const math::Vec3& pos, const math::Quat& rot, const math::Vec3& scale) const {
            // matriz de model
            math::Mat4 T = glm::translate(glm::mat4(1.0f), pos);
            math::Mat4 R = glm::mat4_cast(rot);
            math::Mat4 S = glm::scale(glm::mat4(1.0f), scale);

            math::Mat4 model = T * R * S;

            // View matrix é a inversa
            return glm::inverse(model);
        }

        math::Mat4 GetViewMatrix(const math::Vec3& position, const math::Vec3& forward, const math::Vec3& up) const {
            math::Vec3 eye    = position;
            math::Vec3 center = position + forward; 
            math::Vec3 upv    = up;

            return glm::lookAt(eye, center, upv);
        }
    };
} // nam2espace cp_api 
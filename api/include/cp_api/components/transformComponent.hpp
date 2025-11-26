#pragma once

#include "cp_api/core/math.hpp"
#include "cp_api/physics/aabb.hpp"

namespace cp_api {
    struct TransformComponent {
        TransformComponent(const math::Vec3& position,
                           const math::Quat& rotation,
                           const math::Vec3& scale,
                           const physics3D::AABB& boundary) noexcept
            : position(position), rotation(rotation), scale(scale), boundary(boundary) {}

        math::Vec3 position;
        math::Quat rotation;
        math::Vec3 scale;
        TransformComponent* parent = nullptr;

        physics3D::AABB boundary;

        math::Mat4 GetModelMatrix() const {
            math::Mat4 model{1.0f};

            model = glm::translate(model, position);
            model = glm::mat4_cast(rotation) * model;
            model = glm::scale(model, scale);

            return model;
        }

        math::Mat4 GetWorldMatrix() const {
            if (parent)
                return parent->GetWorldMatrix() * GetModelMatrix();
            return GetModelMatrix();
        }
    };
} // namespace cp_api

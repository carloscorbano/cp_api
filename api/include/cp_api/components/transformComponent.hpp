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

        physics3D::AABB boundary;
    };
} // namespace cp_api

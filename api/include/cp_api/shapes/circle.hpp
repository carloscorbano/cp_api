#pragma once

#include "cp_api/core/math.hpp"
#include "cp_api/physics/aabb.hpp"

namespace cp_api::shapes2D {
    struct Circle {
        Vec2 center;
        float radius;

        Circle() = default;
        Circle(Vec2 center, float radius) : center(center), radius(radius) {}

        bool Intersects(const cp_api::physics2D::AABB& bounds) const {
            // ponto mais próximo do centro dentro do AABB
            float closestX = std::clamp(center.x, bounds.min.x, bounds.max.x);
            float closestY = std::clamp(center.y, bounds.min.y, bounds.max.y);

            // distância quadrada até o ponto mais próximo
            float dx = center.x - closestX;
            float dy = center.y - closestY;
            float distSq = dx * dx + dy * dy;

            return distSq <= radius * radius;
        }
    };
} // namespace cp_api::shapes2D

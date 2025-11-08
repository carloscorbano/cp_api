#pragma once

#include "cp_api/core/math.hpp"

namespace cp_api::shapes3D {
    struct Sphere {
        cp_api::math::Vec3 center;
        float radius;

        Sphere() = default;
        Sphere(const cp_api::math::Vec3& c, float r) : center(c), radius(r) {}

        bool Intersects(const cp_api::physics3D::AABB& box) const {
            // clampa o centro da esfera dentro do AABB
            cp_api::math::Vec3 closest = glm::clamp(center, box.min, box.max);
            // distância entre o centro da esfera e o ponto mais próximo
            cp_api::math::Vec3 diff = center - closest;
            
            return glm::length2(diff) <= (radius * radius);
        }
    };
}
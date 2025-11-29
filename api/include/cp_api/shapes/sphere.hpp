#pragma once

#include "cp_api/core/math.hpp"
#include "cp_api/physics/aabb.hpp"

namespace cp_api::shapes3D {
    struct Sphere {
        Vec3 center;
        float radius;

        Sphere() = default;
        Sphere(const Vec3& c, float r) : center(c), radius(r) {}

        bool Intersects(const cp_api::physics3D::AABB& box) const {
            // clampa o centro da esfera dentro do AABB
            Vec3 closest = glm::clamp(center, box.min, box.max);
            // distância entre o centro da esfera e o ponto mais próximo
            Vec3 diff = center - closest;
            
            return glm::length2(diff) <= (radius * radius);
        }
    };
}
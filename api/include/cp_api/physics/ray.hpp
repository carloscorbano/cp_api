#pragma once

#include "cp_api/core/math.hpp"

namespace cp_api::physics2D {
    // ---------------------------
    // Tipos 2D
    // ---------------------------
    struct Ray {        
        Vec2 origin;
        Vec2 dir; // sempre normalizado
        uint32_t layerMask;

        Ray() = default;
        Ray(const Vec2& o, const Vec2& d, uint32_t layerMask = 0xFFFFFFFF)
            : origin(o), dir(glm::normalize(d)), layerMask(layerMask) {}

        Vec2 GetPoint(float t) const { return origin + dir * t; }
    };

    struct HitInfo {
        float distance = 0.0f;
        float fraction = 0.0f;   // distance / maxDistance (opcional)
        float penetration = 0.0f;
        
        Vec2 point;
        Vec2 normal;

        uint32_t id = 0;  // referência ao objeto atingido
        uint32_t layer = 0;

        void* userData = nullptr;
    };
}

namespace cp_api::physics3D {
    struct Ray {
        Vec3 origin, dir;
        uint32_t layerMask;

        Ray() = default;
        Ray(const Vec3& o, const Vec3& d, uint32_t layerMask = 0xFFFFFFFF) 
            : origin(o), dir(cp_api::math::Normalize(d)), layerMask(layerMask) {}

        Vec3 GetPoint(float t) const { return origin + dir * t; }
    };

    struct HitInfo {
        float distance = 0.0f;
        float fraction = 0.0f;   // distance / maxDistance (opcional)
        float penetration = 0.0f;
        Vec3 point;
        Vec3 normal;

        uint32_t id = 0;  // referência ao objeto atingido
        uint32_t layer = 0;        // camada de colisão (terrain, enemy, etc.)

        void* userData = nullptr;
    };
}
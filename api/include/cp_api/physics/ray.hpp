#pragma once

#include "cp_api/core/math.hpp"

namespace cp_api::physics2D {
    // ---------------------------
    // Tipos 2D
    // ---------------------------
    struct Ray {        
        cp_api::math::Vec2 origin;
        cp_api::math::Vec2 dir; // sempre normalizado
        uint32_t layer;
        uint32_t mask;

        Ray() = default;
        Ray(const cp_api::math::Vec2& o, const cp_api::math::Vec2& d, uint32_t layer = 0, uint32_t mask = 0xFFFFFFFF)
            : origin(o), dir(glm::normalize(d)), layer(layer), mask(mask) {}

        cp_api::math::Vec2 GetPoint(float t) const { return origin + dir * t; }
    };

    struct RayHit {
        bool hit = false;
        float distance = 0.0f;
        float fraction = 0.0f;   // distance / maxDistance (opcional)
        cp_api::math::Vec2 point;
        cp_api::math::Vec2 normal;

        uint32_t hitID = 0;  // referência ao objeto atingido
        uint32_t layer = 0;        // camada de colisão (terrain, enemy, etc.)
        uint32_t mask = 0;
    };
}

namespace cp_api::physics3D {
    struct Ray {
        cp_api::math::Vec3 origin, dir;
        uint32_t layer;
        uint32_t mask;

        Ray() = default;
        Ray(const cp_api::math::Vec3& o, const cp_api::math::Vec3& d, uint32_t layer = 0, uint32_t mask = 0xFFFFFFFF) 
            : origin(o), dir(cp_api::math::Normalize(d)), layer(layer), mask(mask) {}

        cp_api::math::Vec3 GetPoint(float t) const { return origin + dir * t; }
    };

    struct RayHit {
        bool hit = false;
        float distance = 0.0f;
        float fraction = 0.0f;   // distance / maxDistance (opcional)
        cp_api::math::Vec3 point;
        cp_api::math::Vec3 normal;

        uint32_t hitID = 0;  // referência ao objeto atingido
        uint32_t layer = 0;        // camada de colisão (terrain, enemy, etc.)
        uint32_t mask = 0;
    };
}
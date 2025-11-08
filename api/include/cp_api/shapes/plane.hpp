#pragma once

#include "cp_api/core/math.hpp"

namespace cp_api::shapes2D {
    struct Plane {
        cp_api::math::Vec2 normal;
        float distance; // distância ao plano
    };
} // namespace cp_api::shapes2D

namespace cp_api::shapes3D {
    struct Plane {
        cp_api::math::Vec3 normal;
        float distance;
    };
}
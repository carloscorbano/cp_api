#pragma once

#include "cp_api/core/math.hpp"

namespace cp_api::shapes {
    struct Triangle {
        cp_api::math::Vec3 v0, v1, v2;
        cp_api::math::Vec3 normal;
    };
} // namespace cp_api

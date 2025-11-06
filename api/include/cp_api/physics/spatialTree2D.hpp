#pragma once

#include "cp_api/containers/spatialTree.hpp"
#include "ray.hpp"
#include "aabb.hpp"

// Alias para SpatialTree 2D
using SpatialTree2D = cp_api::SpatialTree<cp_api::math::Vec2, cp_api::physics2D::AABB, cp_api::physics2D::Ray, cp_api::physics2D::RayHit, 4>; // 4 filhos por nó
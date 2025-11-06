#pragma once

#include "cp_api/containers/spatialTree.hpp"
#include "ray.hpp"
#include "aabb.hpp"

// Alias para SpatialTree 3D
using SpatialTree3D = cp_api::SpatialTree<cp_api::math::Vec3, cp_api::physics3D::AABB, cp_api::physics3D::Ray, cp_api::physics3D::RayHit, 8>; // 8 filhos por nó (octree)
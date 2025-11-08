#pragma once

#include "cp_api/containers/spatialTree.hpp"
#include "ray.hpp"
#include "aabb.hpp"
#include "cp_api/shapes/circle.hpp"
#include "cp_api/shapes/capsule.hpp"

class SpatialTree2D : public cp_api::SpatialTree<cp_api::math::Vec2, cp_api::physics2D::AABB, cp_api::physics2D::Ray, cp_api::physics2D::HitInfo, 4> {
public:
    void QueryCircle(const cp_api::shapes2D::Circle& circle, std::vector<uint32_t>& outIds, uint32_t queryMask = 0xFFFFFFFF) const;
    void QueryCircle(const cp_api::shapes2D::Circle& circle, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask = 0xFFFFFFFF) const;

    void QueryCapsule(const cp_api::shapes2D::Capsule& capsule, std::vector<uint32_t>& outIds, uint32_t queryMask = 0xFFFFFFFF) const;
    void QueryCapsule(const cp_api::shapes2D::Capsule& capsule, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask = 0xFFFFFFFF) const;

    void QueryBox(const cp_api::physics2D::AABB& range, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask = 0xFFFFFFFF) const;

    void QueryRay(const cp_api::physics2D::Ray& ray, std::vector<cp_api::physics2D::HitInfo>& outInfos, float maxDist, uint32_t queryMask = 0xFFFFFFFF) const;
};
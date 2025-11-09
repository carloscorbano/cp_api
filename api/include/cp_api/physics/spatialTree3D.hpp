#pragma once

#include "cp_api/containers/spatialTree.hpp"
#include "aabb.hpp"
#include "cp_api/shapes/sphere.hpp"
#include "cp_api/shapes/capsule.hpp"
#include "cp_api/shapes/frustum.hpp"

class SpatialTree3D : public cp_api::SpatialTree<cp_api::math::Vec3, cp_api::physics3D::AABB, cp_api::physics3D::Ray, cp_api::physics3D::HitInfo, 8> {
public:
    void QuerySphere(const cp_api::shapes3D::Sphere& sphere, std::vector<uint32_t>& outIds, uint32_t queryMask = 0xFFFFFFFF) const;
    void QuerySphere(const cp_api::shapes3D::Sphere& sphere, std::vector<cp_api::physics3D::HitInfo>& outInfos, uint32_t queryMask = 0xFFFFFFFF) const;
    void QueryCapsule(const cp_api::shapes3D::Capsule& capsule, std::vector<uint32_t>& outIds, uint32_t queryMask = 0xFFFFFFFF) const;
    void QueryCapsule(const cp_api::shapes3D::Capsule& capsule, std::vector<cp_api::physics3D::HitInfo>& outInfos, uint32_t queryMask = 0xFFFFFFFF) const;
    void QueryCube(const cp_api::physics3D::AABB& range, std::vector<cp_api::physics3D::HitInfo>& outInfos, uint32_t queryMask = 0xFFFFFFFF) const;
    void QueryRay(const cp_api::physics3D::Ray& ray, std::vector<cp_api::physics3D::HitInfo>& outInfos, float maxDist, uint32_t queryMask = 0xFFFFFFFF) const;
    void QueryFrustum(const cp_api::shapes3D::Frustum& frustum, std::vector<uint32_t>& outIds, uint32_t queryMask = 0xFFFFFFFF) const;
    void QueryFrustum(const cp_api::shapes3D::Frustum& frustum, std::vector<cp_api::physics3D::HitInfo>& outInfos, uint32_t queryMask = 0xFFFFFFFF) const;
};
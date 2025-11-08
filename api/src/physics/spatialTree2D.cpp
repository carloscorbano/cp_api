#include "cp_api/physics/spatialTree2D.hpp"

#include <algorithm>
#include <unordered_set>
#include <cmath>

void SpatialTree2D::QueryCircle(const cp_api::shapes2D::Circle& circle, std::vector<uint32_t>& outIds, uint32_t queryMask) const {
    cp_api::physics2D::AABB range(
        circle.center - cp_api::math::Vec2(circle.radius),
        circle.center + cp_api::math::Vec2(circle.radius)
    );

    std::vector<uint32_t> candidateIds;
    this->QueryRange(range, candidateIds, queryMask);

    std::unordered_set<uint32_t> uniqueIds;
    for (uint32_t id : candidateIds) {
        const auto entry = this->FindEntry(id);
        if (!entry.has_value()) continue;

        const auto& e = entry->get();
        if (circle.Intersects(e.bounds))
            uniqueIds.insert(id);
    }

    outIds.assign(uniqueIds.begin(), uniqueIds.end());
}

void SpatialTree2D::QueryCircle(const cp_api::shapes2D::Circle& circle, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask) const {
    using namespace cp_api::math;
    using namespace cp_api::physics2D;

    outInfos.clear();

    AABB range(
        circle.center - Vec2(circle.radius),
        circle.center + Vec2(circle.radius)
    );

    std::vector<uint32_t> candidateIds;
    this->QueryRange(range, candidateIds, queryMask);

    for (uint32_t id : candidateIds) {
        const auto entryOpt = this->FindEntry(id);
        if (!entryOpt.has_value()) continue;

        const auto& entry = entryOpt->get();
        const AABB& box = entry.bounds;

        float closestX = std::clamp(circle.center.x, box.min.x, box.max.x);
        float closestY = std::clamp(circle.center.y, box.min.y, box.max.y);
        Vec2 closestPoint(closestX, closestY);

        Vec2 delta = circle.center - closestPoint;
        float distSq = glm::dot(delta, delta);
        float radius = circle.radius;

        if (distSq <= radius * radius) {
            HitInfo hit;
            hit.hit = true;
            hit.id = entry.id;
            hit.layer = entry.layer;
            hit.userData = entry.userData;

            float dist = std::sqrt(distSq);

            if (dist < 1e-6f) {
                // Centro do círculo está dentro do AABB
                hit.normal = Vec2(0, 1);
                hit.penetration = radius;
                hit.point = circle.center;
                hit.distance = 0.0f;
                hit.fraction = 0.0f;
            } else {
                hit.normal = delta / dist;
                hit.penetration = std::max(radius - dist, 0.0f);
                hit.point = circle.center - hit.normal * (radius - hit.penetration);
                hit.distance = dist;
                hit.fraction = dist / radius;
            }

            outInfos.push_back(hit);
        }
    }
}

void SpatialTree2D::QueryCapsule(const cp_api::shapes2D::Capsule& capsule, std::vector<uint32_t>& outIds, uint32_t queryMask) const {
    cp_api::physics2D::AABB range = capsule.GetAABB();

    std::vector<uint32_t> candidates;
    this->QueryRange(range, candidates, queryMask);

    std::unordered_set<uint32_t> uniqueIds;
    for (uint32_t id : candidates) {
        auto entry = FindEntry(id);
        if (!entry.has_value()) continue;
        const auto& e = entry->get();
        if (capsule.Intersects(e.bounds))
            uniqueIds.insert(id);
    }

    outIds.assign(uniqueIds.begin(), uniqueIds.end());
}

void SpatialTree2D::QueryCapsule(const cp_api::shapes2D::Capsule& capsule, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask) const {
    outInfos.clear();

    cp_api::math::Vec2 minBound(
        std::min(capsule.p0.x, capsule.p1.x) - capsule.radius,
        std::min(capsule.p0.y, capsule.p1.y) - capsule.radius
    );
    cp_api::math::Vec2 maxBound(
        std::max(capsule.p0.x, capsule.p1.x) + capsule.radius,
        std::max(capsule.p0.y, capsule.p1.y) + capsule.radius
    );

    cp_api::physics2D::AABB capsuleAABB(minBound, maxBound);
    std::vector<uint32_t> candidateIds;
    this->QueryRange(capsuleAABB, candidateIds, queryMask);

    const auto& p1 = capsule.p0;
    const auto& p2 = capsule.p1;
    float radius = capsule.radius;

    cp_api::math::Vec2 segDir = p2 - p1;
    float segLen = glm::length(segDir);
    if (segLen > 1e-8f) segDir /= segLen; else segDir = cp_api::math::Vec2(0);

    for (uint32_t id : candidateIds) {
        auto entryOpt = this->FindEntry(id);
        if (!entryOpt.has_value()) continue;

        const auto& entry = entryOpt->get();
        const auto& box = entry.bounds;

        cp_api::math::Vec2 boxCenter = (box.min + box.max) * 0.5f;
        cp_api::math::Vec2 boxExtents = (box.max - box.min) * 0.5f;

        cp_api::math::Vec2 segCenter = (p1 + p2) * 0.5f;
        cp_api::math::Vec2 d = segCenter - boxCenter;

        cp_api::math::Vec2 closestOnBox = d;
        closestOnBox.x = std::clamp(closestOnBox.x, -boxExtents.x, boxExtents.x);
        closestOnBox.y = std::clamp(closestOnBox.y, -boxExtents.y, boxExtents.y);
        cp_api::math::Vec2 closestPointBox = boxCenter + closestOnBox;

        cp_api::math::Vec2 segToBox = closestPointBox - p1;
        float t = glm::dot(segToBox, segDir);
        t = glm::clamp(t, 0.0f, segLen);
        cp_api::math::Vec2 closestOnSeg = p1 + segDir * t;

        cp_api::math::Vec2 diff = closestOnSeg - closestPointBox;
        float distSq = glm::dot(diff, diff);
        float dist = std::sqrt(distSq);

        if (dist <= radius) {
            cp_api::physics2D::HitInfo hit;
            hit.hit = true;
            hit.id = entry.id;
            hit.layer = entry.layer;
            hit.userData = entry.userData;
            hit.distance = dist;
            hit.penetration = radius - dist;
            hit.fraction = (radius > 0.0f) ? dist / radius : 0.0f;
            hit.normal = (dist > 1e-6f) ? glm::normalize(diff) : cp_api::math::Vec2(0, 1);
            hit.point = closestPointBox;
            outInfos.push_back(hit);
        }
    }
}

void SpatialTree2D::QueryBox(const cp_api::physics2D::AABB& range, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask) const {
    using namespace cp_api::math;
    using namespace cp_api::physics2D;

    outInfos.clear();

    std::vector<uint32_t> ids;
    this->QueryRange(range, ids, queryMask);

    for (uint32_t id : ids) {
        auto entry = FindEntry(id);
        if (!entry.has_value()) continue;
        const auto& e = entry->get();

        if (!e.bounds.Intersects(range))
            continue;

        HitInfo hit;
        hit.hit = true;
        hit.id = e.id;
        hit.layer = e.layer;
        hit.userData = e.userData;

        Vec2 aCenter = (range.min + range.max) * 0.5f;
        Vec2 bCenter = (e.bounds.min + e.bounds.max) * 0.5f;
        Vec2 diff = bCenter - aCenter;
        Vec2 aHalf = range.GetHalfSize();
        Vec2 bHalf = e.bounds.GetHalfSize();

        Vec2 overlap(
            aHalf.x + bHalf.x - std::abs(diff.x),
            aHalf.y + bHalf.y - std::abs(diff.y)
        );

        if (overlap.x < overlap.y)
            hit.normal = Vec2((diff.x < 0) ? -1.0f : 1.0f, 0.0f);
        else
            hit.normal = Vec2(0.0f, (diff.y < 0) ? -1.0f : 1.0f);

        hit.penetration = std::min(overlap.x, overlap.y);
        hit.point = bCenter - hit.normal * hit.penetration * 0.5f;
        hit.distance = glm::length(diff);
        hit.fraction = 0.0f;

        outInfos.push_back(hit);
    }
}

void SpatialTree2D::QueryRay(const cp_api::physics2D::Ray& ray, std::vector<cp_api::physics2D::HitInfo>& outInfos, float maxDist, uint32_t queryMask) const {
    outInfos.clear();

    std::vector<uint32_t> ids;
    cp_api::physics2D::AABB rayBox(
        ray.origin - cp_api::math::Vec2(maxDist),
        ray.origin + cp_api::math::Vec2(maxDist)
    );
    this->QueryRange(rayBox, ids, queryMask);

    for (uint32_t id : ids) {
        auto entry = FindEntry(id);
        if (!entry.has_value()) continue;
        const auto& e = entry->get();

        cp_api::physics2D::HitInfo hit;
        if (e.bounds.Intersects(ray, hit, maxDist)) {
            hit.id = e.id;
            hit.layer = e.layer;
            hit.userData = e.userData;
            outInfos.push_back(hit);
        }
    }
}

void SpatialTree2D::QueryFrustum(const cp_api::shapes2D::Frustum& frustum, std::vector<uint32_t>& outIds, uint32_t queryMask) const {
      this->Traverse([&](const auto& entry) {
        if ((entry.layer & queryMask) == 0) return true;
        if (frustum.Intersects(entry.bounds))
            outIds.push_back(entry.id);
        return true;
    });
}
void SpatialTree2D::QueryFrustum(const cp_api::shapes2D::Frustum& frustum, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask) const {
    this->Traverse([&](const auto& entry) {
        if ((entry.layer & queryMask) == 0) return true;
        if (frustum.Intersects(entry.bounds)) {
            cp_api::physics2D::HitInfo hit{};
            hit.id = entry.id;
            outInfos.push_back(hit);
        }
        return true;
    });
}
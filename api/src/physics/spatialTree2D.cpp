#include "cp_api/physics/spatialTree2D.hpp"

#include <algorithm> // std::find
#include <unordered_set>

void SpatialTree2D::QueryCircle(const cp_api::shapes2D::Circle& circle, std::vector<uint32_t>& outIds, uint32_t queryMask) const {
    cp_api::physics2D::AABB range(
        circle.center - cp_api::math::Vec2(circle.radius, circle.radius),
        circle.center + cp_api::math::Vec2(circle.radius, circle.radius)
    );

    std::vector<uint32_t> candidateIds;
    this->QueryRange(range, candidateIds, queryMask);

    std::unordered_set<uint32_t> uniqueIds; // evita duplicatas de forma eficiente

    for (uint32_t id : candidateIds) {
        const auto entry = this->FindEntry(id);
        if (!entry.has_value()) continue;

        const auto& e = entry.value().get();

        if (circle.Intersects(e.bounds)) {
            uniqueIds.insert(id); // apenas adiciona, sem precisar verificar duplicata
        }
    }

    outIds.assign(uniqueIds.begin(), uniqueIds.end());
}

void SpatialTree2D::QueryCircle(const cp_api::shapes2D::Circle& circle, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask) const {
    using namespace cp_api::math;
    using namespace cp_api::physics2D;

    outInfos.clear();

    // Broadphase: cria AABB que cobre o círculo
    AABB range(
        circle.center - Vec2(circle.radius, circle.radius),
        circle.center + Vec2(circle.radius, circle.radius)
    );

    // Pega candidatos pela AABB (broadphase)
    std::vector<uint32_t> candidateIds;
    this->QueryRange(range, candidateIds, queryMask);

    for (uint32_t id : candidateIds) {
        const auto entryOpt = this->FindEntry(id);
        if (!entryOpt.has_value()) continue;

        const auto& entry = entryOpt.value().get();
        const AABB& box = entry.bounds;

        // ==== Narrowphase: Circle vs AABB ====
        // Ponto mais próximo do centro do círculo dentro do AABB
        float closestX = std::clamp(circle.center.x, box.min.x, box.max.x);
        float closestY = std::clamp(circle.center.y, box.min.y, box.max.y);
        Vec2 closestPoint(closestX, closestY);

        // Vetor entre o centro e o ponto mais próximo
        Vec2 delta = circle.center - closestPoint;
        float distSq = glm::dot(delta, delta);
        float radius = circle.radius;

        if (distSq <= radius * radius) {
            HitInfo hit;
            hit.hit = true;
            hit.hitID = entry.id;
            hit.layer = entry.layer;

            float dist = std::sqrt(distSq);

            // Se o centro do círculo está dentro do AABB
            if (dist < 1e-6f) {
                hit.normal = Vec2(0, 1); // fallback arbitrário
                hit.penetration = radius;
                hit.point = circle.center;
                hit.distance = 0.0f;
                hit.fraction = 0.0f;
            } else {
                // Normal aponta do AABB → círculo
                hit.normal = delta / dist;

                // Penetração (quanto o círculo invadiu o AABB)
                hit.penetration = radius - dist;
                if (hit.penetration < 0.0f)
                    hit.penetration = 0.0f;

                // Ponto de contato aproximado (na superfície do círculo)
                hit.point = circle.center - hit.normal * (radius - hit.penetration);

                // Distância do centro do círculo ao ponto mais próximo do AABB
                hit.distance = dist;
                hit.fraction = dist / radius; // normaliza entre [0..1]
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

    // Define o AABB de abrangência da cápsula (para broad-phase)
    cp_api::math::Vec2 minBound(
        std::min(capsule.p0.x, capsule.p1.x) - capsule.radius,
        std::min(capsule.p0.y, capsule.p1.y) - capsule.radius
    );
    cp_api::math::Vec2 maxBound(
        std::max(capsule.p0.x, capsule.p1.x) + capsule.radius,
        std::max(capsule.p0.y, capsule.p1.y) + capsule.radius
    );

    cp_api::physics2D::AABB capsuleAABB(minBound, maxBound);

    // Coleta candidatos usando broad-phase
    std::vector<uint32_t> candidateIds;
    this->QueryRange(capsuleAABB, candidateIds, queryMask);

    const auto& p1 = capsule.p0;
    const auto& p2 = capsule.p1;
    float radius = capsule.radius;

    cp_api::math::Vec2 segDir = p2 - p1;
    float segLen = glm::length(segDir);
    if (segLen < 1e-8f)
        segDir = cp_api::math::Vec2(0);
    else
        segDir /= segLen;

    for (uint32_t id : candidateIds)
    {
        auto entryOpt = this->FindEntry(id);
        if (!entryOpt.has_value()) continue;

        const auto& entry = entryOpt.value().get();
        const auto& box = entry.bounds;

        // ---------- cálculo de interseção exata ----------
        cp_api::math::Vec2 boxCenter = (box.min + box.max) * 0.5f;
        cp_api::math::Vec2 boxExtents = (box.max - box.min) * 0.5f;

        cp_api::math::Vec2 segCenter = (p1 + p2) * 0.5f;
        cp_api::math::Vec2 d = segCenter - boxCenter;

        // Ponto mais próximo do box ao segmento
        cp_api::math::Vec2 closestOnBox = d;
        closestOnBox.x = std::clamp(closestOnBox.x, -boxExtents.x, boxExtents.x);
        closestOnBox.y = std::clamp(closestOnBox.y, -boxExtents.y, boxExtents.y);
        cp_api::math::Vec2 closestPointBox = boxCenter + closestOnBox;

        // Ponto mais próximo no segmento à AABB
        cp_api::math::Vec2 segToBox = closestPointBox - p1;
        float t = glm::dot(segToBox, segDir);
        t = glm::clamp(t, 0.0f, segLen);
        cp_api::math::Vec2 closestOnSeg = p1 + segDir * t;

        // Distância entre eles
        cp_api::math::Vec2 diff = closestOnSeg - closestPointBox;
        float distSq = glm::dot(diff, diff);
        float dist = std::sqrt(distSq);

        if (dist <= radius)
        {
            cp_api::physics2D::HitInfo hit;
            hit.hit = true;
            hit.hitID = entry.id;
            hit.layer = entry.layer;
            hit.distance = dist;
            hit.penetration = radius - dist;
            hit.fraction = (radius > 0.0f) ? dist / radius : 0.0f;
            hit.point = closestPointBox;
            hit.normal = (dist > 1e-6f) ? glm::normalize(diff)
                                        : cp_api::math::Vec2(0, 1);

            outInfos.push_back(hit);
        }
    }
}

void SpatialTree2D::QueryBox(const cp_api::physics2D::AABB& range, std::vector<cp_api::physics2D::HitInfo>& outInfos, uint32_t queryMask) const {
    outInfos.clear();

    std::vector<uint32_t> ids;
    this->QueryRange(range, ids, queryMask);

    for (uint32_t id : ids)
    {
        auto entry = FindEntry(id);
        if (!entry.has_value()) continue;
        const auto& e = entry->get();

        if (!e.bounds.Intersects(range))
            continue;

        cp_api::physics2D::HitInfo hit;
        hit.hit = true;
        hit.hitID = e.id;
        hit.layer = e.layer;

        // Centro do AABB e direção da normal
        cp_api::math::Vec2 boxCenter = (e.bounds.min + e.bounds.max) * 0.5f;
        cp_api::math::Vec2 queryCenter = (range.min + range.max) * 0.5f;
        cp_api::math::Vec2 diff = boxCenter - queryCenter;

        hit.distance = glm::length(diff);
        hit.normal = (hit.distance > 1e-6f) ? glm::normalize(diff) : cp_api::math::Vec2(0,1);
        hit.penetration = (e.bounds.GetHalfSize().x + range.GetHalfSize().x) - fabs(diff.x);

        // Ponto aproximado de contato
        hit.point = boxCenter - hit.normal * 0.5f;
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

    for (uint32_t id : ids)
    {
        auto entry = FindEntry(id);
        if (!entry.has_value()) continue;
        const auto& e = entry->get();

        cp_api::physics2D::HitInfo hit;
        if (e.bounds.Intersects(ray, hit, maxDist))
        {
            hit.hitID = e.id;
            hit.layer = e.layer;
            outInfos.push_back(hit);
        }
    }
}
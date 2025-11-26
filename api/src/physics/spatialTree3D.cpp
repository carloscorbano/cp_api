#include "cp_api/physics/spatialTree3D.hpp"

#include <algorithm>
#include <unordered_set>
#include <cmath>
#include "cp_api/core/math.hpp"

void SpatialTree3D::QuerySphere(const cp_api::shapes3D::Sphere& sphere, std::vector<uint32_t>& outIds, uint32_t queryMask) const {
    cp_api::physics3D::AABB range(
        sphere.center - cp_api::math::Vec3(sphere.radius, sphere.radius, sphere.radius),
        sphere.center + cp_api::math::Vec3(sphere.radius, sphere.radius, sphere.radius)
    );

    std::vector<uint32_t> candidateIds;
    this->QueryRange(range, candidateIds, queryMask);

    std::unordered_set<uint32_t> uniqueIds;

    for (uint32_t id : candidateIds) {
        const auto entry = this->FindEntry(id);
        if (!entry.has_value()) continue;

        const auto& e = entry.value().get();

        if (sphere.Intersects(e.bounds)) {
            uniqueIds.insert(id);
        }
    }

    outIds.assign(uniqueIds.begin(), uniqueIds.end());
}

void SpatialTree3D::QuerySphere(const cp_api::shapes3D::Sphere& sphere, std::vector<cp_api::physics3D::HitInfo>& outInfos, uint32_t queryMask) const {
    using namespace cp_api::math;
    using namespace cp_api::physics3D;

    outInfos.clear();

    // Broadphase AABB que cobre a esfera
    cp_api::physics3D::AABB range(
        sphere.center - Vec3(sphere.radius, sphere.radius, sphere.radius),
        sphere.center + Vec3(sphere.radius, sphere.radius, sphere.radius)
    );

    // candidatos
    std::vector<uint32_t> candidateIds;
    this->QueryRange(range, candidateIds, queryMask);

    std::unordered_set<uint32_t> seen;
    for (uint32_t id : candidateIds) {
        if (!seen.insert(id).second) continue; // já processado

        auto entryOpt = this->FindEntry(id);
        if (!entryOpt.has_value()) continue;
        const auto& entry = entryOpt.value().get();
        const auto& box = entry.bounds;

        // ponto mais próximo da esfera ao AABB
        Vec3 closest = glm::clamp(sphere.center, box.min, box.max);
        Vec3 diff = sphere.center - closest;
        float distSq = glm::dot(diff, diff);
        float dist = std::sqrt(distSq);

        if (dist <= sphere.radius) {
            HitInfo hit{};
            hit.id = entry.id;
            hit.layer = entry.layer;
            hit.userData = entry.userData; // ✅ copia o ponteiro

            hit.distance = dist;
            hit.fraction = (sphere.radius > 0.0f) ? (dist / sphere.radius) : 0.0f;
            hit.penetration = sphere.radius - dist;

            if (dist > 1e-6f)
                hit.normal = glm::normalize(diff);
            else {
                // fallback: tenta uma direção coerente apontando para o centro do AABB
                Vec3 toBox = ( (box.min + box.max) * 0.5f ) - sphere.center;
                if (glm::length(toBox) > 1e-6f)
                    hit.normal = glm::normalize(toBox);
                else
                    hit.normal = Vec3(0.0f, 1.0f, 0.0f);
            }

            // ponto de contato: ponto mais próximo no AABB
            hit.point = closest;

            outInfos.push_back(hit);
        }
    }
}

void SpatialTree3D::QueryCapsule(const cp_api::shapes3D::Capsule& capsule, std::vector<uint32_t>& outIds, uint32_t queryMask) const {
    cp_api::physics3D::AABB range = capsule.GetAABB();

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

void SpatialTree3D::QueryCapsule(const cp_api::shapes3D::Capsule& capsule, std::vector<cp_api::physics3D::HitInfo>& outInfos, uint32_t queryMask) const {
    using namespace cp_api::math;
    using namespace cp_api::physics3D;

    outInfos.clear();

    // Broadphase AABB da cápsula
    cp_api::physics3D::AABB range = capsule.GetAABB();
    std::vector<uint32_t> candidateIds;
    this->QueryRange(range, candidateIds, queryMask);

    const Vec3 p0 = capsule.p0;
    const Vec3 p1 = capsule.p1;
    const Vec3 seg = p1 - p0;
    const float segLen = glm::length(seg);
    Vec3 segDir = (segLen > 1e-8f) ? (seg / segLen) : Vec3(0.0f);

    std::unordered_set<uint32_t> seen;
    for (uint32_t id : candidateIds) {
        if (!seen.insert(id).second) continue;

        auto entryOpt = this->FindEntry(id);
        if (!entryOpt.has_value()) continue;
        const auto& entry = entryOpt.value().get();
        const auto& box = entry.bounds;

        // --- Encontrar ponto mais próximo entre segmento e AABB (exato) ---
        // Coordenadas do box
        Vec3 boxCenter = (box.min + box.max) * 0.5f;
        Vec3 boxExtents = (box.max - box.min) * 0.5f;

        // posicionamento do segmento em relação ao box
        Vec3 segCenter = (p0 + p1) * 0.5f;
        Vec3 d = segCenter - boxCenter;

        // clamp d dentro dos extents (acha ponto do box mais próximo do centro do segmento)
        Vec3 closestOnBoxLocal = d;
        closestOnBoxLocal.x = std::clamp(closestOnBoxLocal.x, -boxExtents.x, boxExtents.x);
        closestOnBoxLocal.y = std::clamp(closestOnBoxLocal.y, -boxExtents.y, boxExtents.y);
        closestOnBoxLocal.z = std::clamp(closestOnBoxLocal.z, -boxExtents.z, boxExtents.z);

        Vec3 closestOnBox = boxCenter + closestOnBoxLocal;

        // projetar esse ponto no segmento (encontra o ponto do segmento mais próximo ao box)
        Vec3 segToBox = closestOnBox - p0;
        float t = glm::dot(segToBox, segDir);
        t = std::clamp(t, 0.0f, segLen);
        Vec3 closestOnSeg = p0 + segDir * t;

        // então clampa o ponto do segmento para dentro do box (ponto do box mais próximo ao segmento)
        Vec3 closestPointBox = glm::clamp(closestOnSeg, box.min, box.max);

        // diferença entre os dois pontos -> distância mínima
        Vec3 diff = closestOnSeg - closestPointBox;
        float distSq = glm::dot(diff, diff);
        float dist = std::sqrt(distSq);

        if (dist <= capsule.radius) {
            HitInfo hit{};
            hit.id = entry.id;
            hit.layer = entry.layer;
            hit.userData = entry.userData; // ✅ copia userData

            hit.distance = dist;
            hit.fraction = (capsule.radius > 0.0f) ? (dist / capsule.radius) : 0.0f;
            hit.penetration = capsule.radius - dist;

            if (dist > 1e-6f)
                hit.normal = glm::normalize(diff);
            else {
                // fallback: direção do centro do box ao centro do segmento
                Vec3 toBox = boxCenter - segCenter;
                if (glm::length(toBox) > 1e-6f)
                    hit.normal = glm::normalize(toBox);
                else
                    hit.normal = Vec3(0.0f, 1.0f, 0.0f);
            }

            // ponto de contato no box (mais próximo)
            hit.point = closestPointBox;

            outInfos.push_back(hit);
        }
    }
}

void SpatialTree3D::QueryCube(const cp_api::physics3D::AABB& range, std::vector<cp_api::physics3D::HitInfo>& outInfos, uint32_t queryMask) const {
    using namespace cp_api::math;
    using namespace cp_api::physics3D;

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

        HitInfo hit;
        hit.id = e.id;
        hit.layer = e.layer;
        hit.userData = e.userData; // ✅ copia userData

        Vec3 aCenter = (range.min + range.max) * 0.5f;
        Vec3 bCenter = (e.bounds.min + e.bounds.max) * 0.5f;
        Vec3 diff = bCenter - aCenter;
        Vec3 aHalf = range.GetHalfSize();
        Vec3 bHalf = e.bounds.GetHalfSize();

        // overlap por eixo
        Vec3 overlap = (aHalf + bHalf) - glm::abs(diff);

        // proteção: se overlap negativo (não deveria acontecer por Intersects) zera
        overlap.x = std::max(overlap.x, 0.0f);
        overlap.y = std::max(overlap.y, 0.0f);
        overlap.z = std::max(overlap.z, 0.0f);

        // escolhe eixo de menor penetração para definir normal
        float minOverlap = std::min({ overlap.x, overlap.y, overlap.z });

        if (minOverlap == overlap.x) {
            hit.normal = Vec3((diff.x < 0.0f) ? -1.0f : 1.0f, 0.0f, 0.0f);
            hit.penetration = overlap.x;
        } else if (minOverlap == overlap.y) {
            hit.normal = Vec3(0.0f, (diff.y < 0.0f) ? -1.0f : 1.0f, 0.0f);
            hit.penetration = overlap.y;
        } else {
            hit.normal = Vec3(0.0f, 0.0f, (diff.z < 0.0f) ? -1.0f : 1.0f);
            hit.penetration = overlap.z;
        }

        // ponto aproximado de contato: centro do bloco deslocado pela metade da penetração
        hit.point = bCenter - hit.normal * (hit.penetration * 0.5f);

        hit.distance = glm::length(diff);
        hit.fraction = 0.0f;

        outInfos.push_back(hit);
    }
}

void SpatialTree3D::QueryRay(const cp_api::physics3D::Ray& ray, std::vector<cp_api::physics3D::HitInfo>& outInfos, float maxDist, uint32_t queryMask) const {
    outInfos.clear();

    std::vector<uint32_t> ids;
    cp_api::physics3D::AABB rayBox(
        ray.origin - cp_api::math::Vec3(maxDist, maxDist, maxDist),
        ray.origin + cp_api::math::Vec3(maxDist, maxDist, maxDist)
    );
    this->QueryRange(rayBox, ids, queryMask);

    for (uint32_t id : ids)
    {
        auto entry = FindEntry(id);
        if (!entry.has_value()) continue;
        const auto& e = entry->get();

        cp_api::physics3D::HitInfo hit;
        if (e.bounds.Intersects(ray, hit, maxDist))
        {
            hit.id = e.id;
            hit.layer = e.layer;
            hit.userData = e.userData;
            outInfos.push_back(hit);
        }
    }
}

void SpatialTree3D::QueryFrustum(const cp_api::shapes3D::Frustum& frustum,
                                 std::vector<uint32_t>& outIds,
                                 uint32_t queryMask) const {
    this->Traverse([&](const auto& entry) {
        if ((entry.layer & queryMask) == 0) return true;
        if (frustum.Intersects(entry.bounds))
            outIds.push_back(entry.id);
        return true;
    });
}

void SpatialTree3D::QueryFrustum(const cp_api::shapes3D::Frustum& frustum,
                                 std::vector<cp_api::physics3D::HitInfo>& outInfos,
                                 uint32_t queryMask) const
{
    using namespace cp_api::physics3D;
    using namespace cp_api::math;
    using namespace cp_api::shapes3D;

    this->Traverse([&](const auto& entry)
    {
        if ((entry.layer & queryMask) == 0)
            return true;

        const AABB& box = entry.bounds;

        if (!frustum.Intersects(box))
            return true;

        // ===============================
        //      Construção do HitInfo
        // ===============================

        HitInfo hit{};
        hit.id       = entry.id;
        hit.layer    = entry.layer;
        hit.userData = entry.userData;

        // Centro do bounding box
        Vec3 center = box.Center();
        hit.point = center;

        // -------------------------------------------------------
        // Distância e normal com base em PLANO DO FRUSTUM
        // -------------------------------------------------------
        float minDist = FLT_MAX;
        Vec3  bestNormal = {0,0,1};

        for (const auto& plane : frustum.planes)
        {
            // Distância do centro ao plano (negativo = fora)
            float d = glm::dot(plane.normal, center) + plane.distance;

            if (d < minDist)
            {
                minDist = d;
                bestNormal = plane.normal;
            }
        }

        hit.distance = minDist;          // distância ao clipping mais próximo
        hit.normal   = Normalize(bestNormal);
        hit.penetration = std::max(0.0f, minDist);  // o quanto está "dentro" do frustum

        // fraction baseado em Near/Far (normalizado 0..1)
        float farDist  =
            glm::length(frustum.planes[Frustum::PlaneIndex::Far].normal   * frustum.planes[Frustum::PlaneIndex::Far].distance);
        float nearDist =
            glm::length(frustum.planes[Frustum::PlaneIndex::Near].normal  * frustum.planes[Frustum::PlaneIndex::Near].distance);

        hit.fraction = (minDist - nearDist) / (farDist - nearDist);
        hit.fraction = std::clamp(hit.fraction, 0.0f, 1.0f);

        outInfos.push_back(hit);
        return true;
    });
}

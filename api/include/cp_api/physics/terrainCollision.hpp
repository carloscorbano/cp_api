#pragma once

#include "cp_api/containers/spatialTree.hpp"
#include "cp_api/shapes/triangle.hpp"
#include "cp_api/shapes/sphere.hpp"

#include <algorithm>
#include <vector>
#include <cmath>
#include <limits>
#include <cstdint>

namespace cp_api::physics {
    struct Ray3 {
        math::Vec3 origin, dir;
        Ray3(const math::Vec3& o, const math::Vec3& d) : origin(o), dir(d) {}
    };

    struct RayHit3 {
        math::Vec3 position;
        math::Vec3 normal;
        float distance = std::numeric_limits<float>::max();
        uint32_t triangleId = UINT32_MAX;
        bool hit = false;
    };

    static float pointToTriangleDistanceSq(const math::Vec3& p, const shapes::Triangle& tri, math::Vec3& closest) {
        math::Vec3 ab = tri.v1 - tri.v0;
        math::Vec3 ac = tri.v2 - tri.v0;
        math::Vec3 ap = p - tri.v0;

        float d1 = dot(ab, ap);
        float d2 = dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) { closest = tri.v0; return dot(p - tri.v0, p - tri.v0); }

        math::Vec3 bp = p - tri.v1;
        float d3 = dot(ab, bp);
        float d4 = dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) { closest = tri.v1; return dot(p - tri.v1, p - tri.v1); }

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        {
            float v = d1 / (d1 - d3);
            closest = tri.v0 + v * ab;
            return dot(p - closest, p - closest);
        }

        math::Vec3 cp = p - tri.v2;
        float d5 = dot(ab, cp);
        float d6 = dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) { closest = tri.v2; return dot(p - tri.v2, p - tri.v2); }

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        {
            float w = d2 / (d2 - d6);
            closest = tri.v0 + w * ac;
            return dot(p - closest, p - closest);
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            closest = tri.v1 + w * (tri.v2 - tri.v1);
            return dot(p - closest, p - closest);
        }

        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom;
        float w = vc * denom;
        closest = tri.v0 + ab * v + ac * w;
        return dot(p - closest, p - closest);
    }

    static bool sphereIntersectsTriangle(const shapes::Sphere& s, const shapes::Triangle& tri, math::Vec3& outNormal, float& outPenetration) {
        math::Vec3 closest;
        float distSq = pointToTriangleDistanceSq(s.center, tri, closest);
        if (distSq > s.radius * s.radius)
            return false;

        math::Vec3 dir = s.center - closest;
        float dist = std::sqrt(distSq);
        if (dist > 1e-6f)
            outNormal = math::Normalize(dir);
        else
            outNormal = tri.normal;
        outPenetration = s.radius - dist;
        return true;
    }

    // Ray-triangle (Möller–Trumbore)
    static bool rayIntersectsTriangle(
        const math::Vec3& orig, const math::Vec3& dir,
        const shapes::Triangle& tri, float& outT, math::Vec3& outNormal)
    {
        const float EPS = 1e-6f;
        math::Vec3 edge1 = tri.v1 - tri.v0;
        math::Vec3 edge2 = tri.v2 - tri.v0;
        math::Vec3 pvec = math::Cross(dir, edge2);
        float det = math::Dot(edge1, pvec);
        if (fabs(det) < EPS) return false;
        float invDet = 1.0f / det;
        math::Vec3 tvec = orig - tri.v0;
        float u = math::Dot(tvec, pvec) * invDet;
        if (u < 0.0f || u > 1.0f) return false;
        math::Vec3 qvec = math::Cross(tvec, edge1);
        float v = math::Dot(dir, qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f) return false;
        float t = math::Dot(edge2, qvec) * invDet;
        if (t <= EPS) return false;
        outT = t;
        outNormal = math::Normalize(math::Cross(edge1, edge2));
        return true;
    }

    struct AABB3 {
        math::Vec3 min, max;
        AABB3() {}
        AABB3(const math::Vec3& mi, const math::Vec3& ma) : min(mi), max(ma) {}
        math::Vec3 Center() const { return math::Vec3((min.x+max.x)*0.5f, (min.y+max.y)*0.5f, (min.z+max.z)*0.5f); }

        math::Vec3 Min() const { return min; }
        math::Vec3 Max() const { return max; }

        bool Contains(const math::Vec3& p) const {
            return p.x>=min.x && p.x<=max.x &&
                p.y>=min.y && p.y<=max.y &&
                p.z>=min.z && p.z<=max.z;
        }

        bool Contains(const AABB3& other) const {
            return (other.min.x >= min.x && other.max.x <= max.x) &&
                (other.min.y >= min.y && other.max.y <= max.y) &&
                (other.min.z >= min.z && other.max.z <= max.z);
        }

        bool Intersects(const AABB3& b) const {
            return !(b.max.x<min.x || b.min.x>max.x ||
                    b.max.y<min.y || b.min.y>max.y ||
                    b.max.z<min.z || b.min.z>max.z);
        }

        bool Intersects(const Ray3& ray, RayHit3& hit, float tMax) const {
            float tmin = 0.0f;
            float tmax = tMax;
            int hitAxis = -1;
            float sign = 0.0f;

            for (int i = 0; i < 3; i++) {
                float o = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
                float d = (i == 0) ? ray.dir.x    : (i == 1) ? ray.dir.y    : ray.dir.z;
                float minVal = (i == 0) ? min.x : (i == 1) ? min.y : min.z;
                float maxVal = (i == 0) ? max.x : (i == 1) ? max.y : max.z;

                // Evita divisão por zero
                if (fabs(d) < 1e-8f) {
                    if (o < minVal || o > maxVal) return false;
                    continue;
                }

                float invD = 1.0f / d;
                float t0 = (minVal - o) * invD;
                float t1 = (maxVal - o) * invD;
                if (invD < 0.0f) std::swap(t0, t1);

                // Atualiza tmin e tmax
                if (t0 > tmin) {
                    tmin = t0;
                    hitAxis = i;
                    sign = (invD < 0.0f) ? 1.0f : -1.0f; // Inverte se veio pelo lado negativo
                }
                tmax = std::min(tmax, t1);
                if (tmax < tmin) return false;
            }

            hit.hit = true;
            hit.distance = tmin;
            hit.position = ray.origin + ray.dir * tmin;
            hit.triangleId = UINT32_MAX;

            // Normal
            hit.normal = {0, 0, 0};
            if (hitAxis == 0) hit.normal.x = sign;
            else if (hitAxis == 1) hit.normal.y = sign;
            else if (hitAxis == 2) hit.normal.z = sign;

            return true;
        }
    };

    // ------------------------------------------------------------
    // Classe principal: TerrainCollider
    // ------------------------------------------------------------
    class TerrainCollider {
    public:
        using Tree = SpatialTree<math::Vec3, AABB3, Ray3, RayHit3, 8>;

        TerrainCollider(const AABB3& worldBounds)
            : m_tree(worldBounds, 8, 8) {}

        // Adiciona um triângulo de terreno
        void AddTriangle(const math::Vec3& v0, const math::Vec3& v1, const math::Vec3& v2) {
            shapes::Triangle tri{ v0, v1, v2, math::Normalize(math::Cross(v1 - v0, v2 - v0)) };
            uint32_t id = static_cast<uint32_t>(m_triangles.size());
            m_triangles.push_back(tri);

            math::Vec3 bmin = glm::min(glm::min(v0, v1), v2);
            math::Vec3 bmax = glm::max(glm::max(v0, v1), v2);
            AABB3 aabb{ bmin, bmax };

            m_tree.Insert(id, aabb);
        }

        // Colisão esfera ↔ terreno (retorna true se houve colisão)
        bool CollideSphere(shapes::Sphere& s, math::Vec3& correctionOut)
        {
            AABB3 queryBox{
                s.center - math::Vec3(s.radius),
                s.center + math::Vec3(s.radius)
            };

            std::vector<uint32_t> candidates;
            m_tree.QueryRange(queryBox, candidates);

            bool collided = false;
            correctionOut = math::Vec3(0.0f);

            for (uint32_t id : candidates)
            {
                const shapes::Triangle& tri = m_triangles[id];
                math::Vec3 normal;
                float penetration;

                if (sphereIntersectsTriangle(s, tri, normal, penetration))
                {
                    correctionOut += normal * penetration;
                    collided = true;
                }
            }

            if (collided)
                s.center += correctionOut;

            return collided;
        }

        // Raycast contra terreno (retorna o hit mais próximo)
        bool Raycast(const math::Vec3& origin, const math::Vec3& dir, float maxDist, RayHit3& outHit) {
            math::Vec3 end = origin + dir * maxDist;
            math::Vec3 qmin = glm::min(origin, end);
            math::Vec3 qmax = glm::max(origin, end);
            AABB3 queryBox{ qmin, qmax };

            std::vector<uint32_t> candidates;
            m_tree.QueryRange(queryBox, candidates);

            bool hit = false;
            float bestT = maxDist;

            for (uint32_t id : candidates) {
                float t;
                math::Vec3 n;
                if (rayIntersectsTriangle(origin, dir, m_triangles[id], t, n) && t < bestT) {
                    bestT = t;
                    outHit.hit = true;
                    outHit.distance = t;
                    outHit.triangleId = id;
                    outHit.normal = n;
                    outHit.position = origin + dir * t;
                    hit = true;
                }
            }
            return hit;
        }

    private:
        Tree m_tree;
        std::vector<shapes::Triangle> m_triangles;
    };
}
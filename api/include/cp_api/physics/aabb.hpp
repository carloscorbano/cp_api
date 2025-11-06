#pragma once

namespace cp_api::physics2D {
        struct AABB {
        cp_api::math::Vec2 min, max;

        AABB() = default;
        AABB(const cp_api::math::Vec2& mi, const cp_api::math::Vec2& ma)
            : min(mi), max(ma) {}

        // --- Utilitários ---
        cp_api::math::Vec2 Center() const { return cp_api::math::Vec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f); }
        bool Contains(const cp_api::math::Vec2& p) const { return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y; }

        cp_api::math::Vec2 Min() const { return min; }
        cp_api::math::Vec2 Max() const { return max; }

        bool Contains(const AABB& other) const {
            return (other.min.x >= min.x && other.max.x <= max.x) &&
                (other.min.y >= min.y && other.max.y <= max.y);
        }

        bool Intersects(const AABB& b) const {
            return !(b.max.x < min.x || b.min.x > max.x ||
                    b.max.y < min.y || b.min.y > max.y);
        }

        bool Intersects(const Ray& ray, float tMax) const {
            float tmin = 0.0f;
            float tmax = tMax;

            // Testa cada eixo (X e Y)
            for (int i = 0; i < 2; ++i) {
                float o = (i == 0) ? ray.origin.x : ray.origin.y;
                float d = (i == 0) ? ray.dir.x    : ray.dir.y;
                float minVal = (i == 0) ? min.x   : min.y;
                float maxVal = (i == 0) ? max.x   : max.y;

                if (std::abs(d) < 1e-6f) {
                    // Raio paralelo ao plano: precisa estar dentro do intervalo
                    if (o < minVal || o > maxVal)
                        return false;
                } else {
                    float invD = 1.0f / d;
                    float t0 = (minVal - o) * invD;
                    float t1 = (maxVal - o) * invD;
                    if (invD < 0.0f) std::swap(t0, t1);

                    tmin = std::max(tmin, t0);
                    tmax = std::min(tmax, t1);
                    if (tmax < tmin)
                        return false;
                }
            }
            return true;
        }

        bool Intersects(const Ray& ray, RayHit& hit, float tMax) const {
            float tmin = 0.0f, tmax = tMax;
            int hitAxis = -1;

            for (int i = 0; i < 2; i++) {
                float o = (i == 0) ? ray.origin.x : ray.origin.y;
                float d = (i == 0) ? ray.dir.x : ray.dir.y;
                float minVal = (i == 0) ? min.x : min.y;
                float maxVal = (i == 0) ? max.x : max.y;

                if (std::fabs(d) < 1e-8f) {
                    if (o < minVal || o > maxVal)
                        return false;
                    continue;
                }

                float invD = 1.0f / d;
                float t0 = (minVal - o) * invD;
                float t1 = (maxVal - o) * invD;

                if (invD < 0.0f) std::swap(t0, t1);

                if (t0 > tmin) { 
                    tmin = t0; 
                    hitAxis = i;
                }
                tmax = std::min(tmax, t1);

                if (tmax < tmin)
                    return false;
            }

            if (tmin < 0.0f) tmin = 0.0f;

            hit.hit = true;
            hit.distance = tmin;
            hit.fraction = tmin / tMax;
            hit.point = ray.GetPoint(tmin);

            cp_api::math::Vec2 normal(0, 0);
            float eps = 1e-4f;
            if (fabs(hit.point.x - min.x) < eps) normal.x = -1;
            else if (fabs(hit.point.x - max.x) < eps) normal.x = 1;

            if (fabs(hit.point.y - min.y) < eps) normal.y = -1;
            else if (fabs(hit.point.y - max.y) < eps) normal.y = 1;

            hit.normal = glm::normalize(normal);

            // Preenche layer e collision info no hit
            hit.hitID = 0;

            return true;
        }
    };
} // namespace cp_api::physics2D

namespace cp_api::physics3D {

    struct AABB {
        cp_api::math::Vec3 min, max;
        void* userData = nullptr;    // ponteiro para o dono do AABB (ex: entidade, collider, etc.)

        AABB() = default;
        AABB(const cp_api::math::Vec3& mi, const cp_api::math::Vec3& ma, void* data = nullptr)
            : min(mi), max(ma), userData(data) {}

        // -------------------------------
        // Utilitários básicos
        // -------------------------------
        cp_api::math::Vec3 Center() const {
            return cp_api::math::Vec3((min.x + max.x) * 0.5f,
                        (min.y + max.y) * 0.5f,
                        (min.z + max.z) * 0.5f);
        }

        cp_api::math::Vec3 Extents() const { return (max - min) * 0.5f; }
        cp_api::math::Vec3 Size() const { return max - min; }

        cp_api::math::Vec3 Min() const { return min; }
        cp_api::math::Vec3 Max() const { return max; }

        bool Contains(const cp_api::math::Vec3& p) const {
            return (p.x >= min.x && p.x <= max.x &&
                    p.y >= min.y && p.y <= max.y &&
                    p.z >= min.z && p.z <= max.z);
        }

        bool Contains(const AABB& other) const {
            return (other.min.x >= min.x && other.max.x <= max.x) &&
                   (other.min.y >= min.y && other.max.y <= max.y) &&
                   (other.min.z >= min.z && other.max.z <= max.z);
        }

        bool Intersects(const AABB& b) const {
            return !(b.max.x < min.x || b.min.x > max.x ||
                     b.max.y < min.y || b.min.y > max.y ||
                     b.max.z < min.z || b.min.z > max.z);
        }

        bool Intersects(const Ray& ray, float tMax) const {
            float tmin = 0.0f;
            float tmax = tMax;

            for (int i = 0; i < 3; ++i) {
                float o = (i == 0) ? ray.origin.x :
                        (i == 1) ? ray.origin.y : ray.origin.z;
                float d = (i == 0) ? ray.dir.x :
                        (i == 1) ? ray.dir.y : ray.dir.z;
                float minVal = (i == 0) ? min.x :
                            (i == 1) ? min.y : min.z;
                float maxVal = (i == 0) ? max.x :
                            (i == 1) ? max.y : max.z;

                if (std::abs(d) < 1e-6f) {
                    // Raio paralelo ao plano desse eixo → deve estar dentro do slab
                    if (o < minVal || o > maxVal)
                        return false;
                } else {
                    float invD = 1.0f / d;
                    float t0 = (minVal - o) * invD;
                    float t1 = (maxVal - o) * invD;
                    if (invD < 0.0f) std::swap(t0, t1);

                    tmin = std::max(tmin, t0);
                    tmax = std::min(tmax, t1);
                    if (tmax < tmin)
                        return false;
                }
            }
            return true;
        }

        // -------------------------------
        // Interseção com Ray
        // -------------------------------
        bool Intersects(const Ray& ray, RayHit& hit, float tMax) const {
            float tmin = 0.0f, tmax = tMax;
            cp_api::math::Vec3 normal(0, 0, 0);
            int hitAxis = -1;

            // Teste de interseção com slab em cada eixo (AABB)
            for (int i = 0; i < 3; i++) {
                float o = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
                float d = (i == 0) ? ray.dir.x   : (i == 1) ? ray.dir.y   : ray.dir.z;
                float minVal = (i == 0) ? min.x  : (i == 1) ? min.y      : min.z;
                float maxVal = (i == 0) ? max.x  : (i == 1) ? max.y      : max.z;

                if (std::fabs(d) < 1e-8f) {
                    if (o < minVal || o > maxVal)
                        return false;
                    continue;
                }

                float invD = 1.0f / d;
                float t0 = (minVal - o) * invD;
                float t1 = (maxVal - o) * invD;
                if (invD < 0.0f) std::swap(t0, t1);

                if (t0 > tmin) {
                    tmin = t0;
                    hitAxis = i;
                }
                tmax = std::min(tmax, t1);

                if (tmax < tmin)
                    return false;
            }

            if (tmin < 0.0f)
                tmin = 0.0f;

            hit.hit = true;
            hit.distance = tmin;
            hit.fraction = tmin / tMax;
            hit.point = ray.GetPoint(tmin);

            // --- Cálculo da normal geométrica ---
            float eps = 1e-4f;
            if (std::fabs(hit.point.x - min.x) < eps) normal.x = -1;
            else if (std::fabs(hit.point.x - max.x) < eps) normal.x = 1;

            if (std::fabs(hit.point.y - min.y) < eps) normal.y = -1;
            else if (std::fabs(hit.point.y - max.y) < eps) normal.y = 1;

            if (std::fabs(hit.point.z - min.z) < eps) normal.z = -1;
            else if (std::fabs(hit.point.z - max.z) < eps) normal.z = 1;

            // Se atingir canto ou aresta, a normal fica diagonal — normalize.
            hit.normal = math::Normalize(normal);
            hit.hitID = 0;

            return true;
        }
    };
}
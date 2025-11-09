#pragma once

#include "cp_api/core/math.hpp"
#include "cp_api/physics/aabb.hpp"

namespace cp_api::shapes2D {
    struct Capsule {
        cp_api::math::Vec2 p0;
        cp_api::math::Vec2 p1;
        float radius;

        Capsule() = default;
        Capsule(cp_api::math::Vec2 p0, cp_api::math::Vec2 p1, float radius)
            : p0(p0), p1(p1), radius(radius) {}

        cp_api::physics2D::AABB GetAABB() const {
            cp_api::math::Vec2 minPt = glm::min(p0, p1) - cp_api::math::Vec2(radius);
            cp_api::math::Vec2 maxPt = glm::max(p0, p1) + cp_api::math::Vec2(radius);
            return {minPt, maxPt};
        }

        bool Intersects(const cp_api::physics2D::AABB& box) const {
            using namespace cp_api::math;

            Vec2 seg = p1 - p0;
            Vec2 boxCenter = (box.min + box.max) * 0.5f;
            Vec2 boxHalf   = (box.max - box.min) * 0.5f;

            Vec2 p0Local = p0 - boxCenter;
            Vec2 p1Local = p1 - boxCenter;
            Vec2 d = p1Local - p0Local;

            float t = 0.0f;
            for (int i = 0; i < 2; i++) {
                float start = (i == 0) ? p0Local.x : p0Local.y;
                float dir   = (i == 0) ? d.x : d.y;
                float extent = (i == 0) ? boxHalf.x : boxHalf.y;

                if (start < -extent && dir > 0.0f)
                    t = std::max(t, (-extent - start) / dir);
                else if (start > extent && dir < 0.0f)
                    t = std::max(t, (extent - start) / dir);
            }

            t = glm::clamp(t, 0.0f, 1.0f);
            Vec2 segPoint = p0Local + d * t;
            Vec2 boxPoint = glm::clamp(segPoint, -boxHalf, boxHalf);

            float distSq = glm::length2(segPoint - boxPoint);
            return distSq <= radius * radius;
        }
    };
}

namespace cp_api::shapes3D {

    struct Capsule {
        cp_api::math::Vec3 p0;    // extremidade 1
        cp_api::math::Vec3 p1;    // extremidade 2
        float radius;

        Capsule() = default;
        Capsule(cp_api::math::Vec3 p0, cp_api::math::Vec3 p1, float radius)
            : p0(p0), p1(p1), radius(radius) {}

        // --- Retorna AABB que envolve a cápsula ---
        cp_api::physics3D::AABB GetAABB() const {
            cp_api::math::Vec3 minPt = glm::min(p0, p1) - cp_api::math::Vec3(radius);
            cp_api::math::Vec3 maxPt = glm::max(p0, p1) + cp_api::math::Vec3(radius);
            return {minPt, maxPt};
        }

        // --- Teste rápido e preciso de interseção com AABB ---
        bool Intersects(const cp_api::physics3D::AABB& box) const {
            using namespace cp_api::math;

            // Vetor do segmento
            Vec3 seg = p1 - p0;

            // Centro e half-size do AABB
            Vec3 boxCenter = (box.min + box.max) * 0.5f;
            Vec3 boxHalf   = (box.max - box.min) * 0.5f;

            // Converter para espaço do box
            Vec3 p0Local = p0 - boxCenter;
            Vec3 p1Local = p1 - boxCenter;

            // Vetor do segmento no espaço local
            Vec3 d = p1Local - p0Local;

            // Parâmetro t (0..1) do ponto mais próximo no segmento ao box
            float t = 0.0f;

            // Inicializamos ponto do segmento mais próximo
            Vec3 closest = p0Local;

            // Para cada eixo (x,y,z)
            for (int i = 0; i < 3; i++) {
                float start = (i == 0) ? p0Local.x : (i == 1 ? p0Local.y : p0Local.z);
                float dir   = (i == 0) ? d.x : (i == 1 ? d.y : d.z);
                float extent = (i == 0) ? boxHalf.x : (i == 1 ? boxHalf.y : boxHalf.z);

                if (start < -extent && dir > 0.0f)
                    t = std::max(t, (-extent - start) / dir);
                else if (start > extent && dir < 0.0f)
                    t = std::max(t, (extent - start) / dir);
            }

            t = glm::clamp(t, 0.0f, 1.0f);
            Vec3 segPoint = p0Local + d * t;

            // Clampe ponto para dentro do box
            Vec3 boxPoint = glm::clamp(segPoint, -boxHalf, boxHalf);

            // Distância mínima entre segmento e box
            float distSq = glm::length2(segPoint - boxPoint);

            return distSq <= radius * radius;
        }
    };
}


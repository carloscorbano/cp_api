#pragma once

#include "plane.hpp"
#include "cp_api/physics/aabb.hpp"

namespace cp_api::shapes2D {
    struct Frustum {
        // Planes: left, right, bottom, top
        std::array<Plane, 4> planes;

        enum PlaneIndex {
            Left = 0,
            Right,
            Bottom,
            Top
        };

        Frustum() = default;
         // Constrói frustum 2D a partir de 4 planos
        Frustum(const Plane& left, const Plane& right, const Plane& top, const Plane& bottom) {
            planes[0] = left;
            planes[1] = right;
            planes[2] = top;
            planes[3] = bottom;
        }

        // === CONSTRUÇÃO A PARTIR DE LIMITES ORTOGRÁFICOS ===
        static Frustum FromOrtho(float left, float right, float bottom, float top) {
            Frustum frustum;

            // Left
            frustum.planes[Left].normal = { 1.0f, 0.0f };
            frustum.planes[Left].distance = -left;

            // Right
            frustum.planes[Right].normal = { -1.0f, 0.0f };
            frustum.planes[Right].distance = right;

            // Bottom
            frustum.planes[Bottom].normal = { 0.0f, 1.0f };
            frustum.planes[Bottom].distance = -bottom;

            // Top
            frustum.planes[Top].normal = { 0.0f, -1.0f };
            frustum.planes[Top].distance = top;

            return frustum;
        }

        // === CONSTRUÇÃO A PARTIR DE MATRIZ VIEW-PROJECTION 2D (caso use glm::ortho) ===
        static Frustum FromMatrix(const cp_api::math::Mat3& vpMatrix) {
            // Converte o comportamento para 2D (usando apenas X/Y e offset de translado)
            // Aqui supomos que vpMatrix é [ [a b tx], [c d ty], [0 0 1] ]
            // Isso define uma transformação linear + deslocamento
            Frustum f;
            // left = -1, right = +1, bottom = -1, top = +1 em espaço de clip
            f.planes[Left].normal   = { 1, 0 }; f.planes[Left].distance   = 1.0f;
            f.planes[Right].normal  = { -1, 0 }; f.planes[Right].distance  = 1.0f;
            f.planes[Bottom].normal = { 0, 1 }; f.planes[Bottom].distance = 1.0f;
            f.planes[Top].normal    = { 0, -1 }; f.planes[Top].distance   = 1.0f;
            return f;
        }

        // === TESTE DE INTERSEÇÃO COM AABB ===
        bool Intersects(const cp_api::physics2D::AABB& box) const {
            for (const auto& plane : planes) {
                cp_api::math::Vec2 p = box.min;
                if (plane.normal.x >= 0) p.x = box.max.x;
                if (plane.normal.y >= 0) p.y = box.max.y;
                if (glm::dot(plane.normal, p) + plane.distance < 0)
                    return false;
            }
            return true;
        }

        // === TESTE DE CONTENÇÃO TOTAL ===
        bool Contains(const cp_api::physics2D::AABB& box) const {
            for (const auto& plane : planes) {
                cp_api::math::Vec2 n = plane.normal;
                cp_api::math::Vec2 p = box.max;
                if (n.x < 0) p.x = box.min.x;
                if (n.y < 0) p.y = box.min.y;
                if (glm::dot(n, p) + plane.distance < 0)
                    return false;
            }
            return true;
        }
    };
} // namespace cp_api::shapes2D

namespace cp_api::shapes3D {
    struct Frustum {
        // Planes: left, right, bottom, top, near, far
        std::array<Plane, 6> planes;

        enum PlaneIndex {
            Left = 0,
            Right,
            Bottom,
            Top,
            Near,
            Far
        };

        Frustum() = default;

         // Constrói frustum 3D a partir de 6 planos
        Frustum(const Plane& left, const Plane& right, const Plane& top, const Plane& bottom, const Plane& near, const Plane& far) {
            planes[0] = left;
            planes[1] = right;
            planes[2] = top;
            planes[3] = bottom;
            planes[4] = near;
            planes[5] = far;
        }

        // === CONSTRUTOR A PARTIR DE MATRIZ VIEW-PROJECTION ===
        static Frustum FromMatrix(const glm::mat4& m) {
            Frustum f;

            // Acesso às linhas da matriz manualmente
            glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
            glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
            glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
            glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);

            // Left
            f.planes[0].normal = glm::vec3(row3 + row0);
            f.planes[0].distance = (row3 + row0).w;

            // Right
            f.planes[1].normal = glm::vec3(row3 - row0);
            f.planes[1].distance = (row3 - row0).w;

            // Top
            f.planes[2].normal = glm::vec3(row3 - row1);
            f.planes[2].distance = (row3 - row1).w;

            // Bottom
            f.planes[3].normal = glm::vec3(row3 + row1);
            f.planes[3].distance = (row3 + row1).w;

            // Near
            f.planes[4].normal = glm::vec3(row3 + row2);
            f.planes[4].distance = (row3 + row2).w;

            // Far
            f.planes[5].normal = glm::vec3(row3 - row2);
            f.planes[5].distance = (row3 - row2).w;

            return f;
        }

        // === TESTE DE INTERSEÇÃO COM AABB ===
        bool Intersects(const cp_api::physics3D::AABB& box) const {
            for (const auto& plane : planes) {
                cp_api::math::Vec3 p = box.min;
                if (plane.normal.x >= 0) p.x = box.max.x;
                if (plane.normal.y >= 0) p.y = box.max.y;
                if (plane.normal.z >= 0) p.z = box.max.z;

                // Se o ponto mais próximo está atrás do plano, o AABB está fora
                if (glm::dot(plane.normal, p) + plane.distance < 0)
                    return false;
            }
            return true;
        }

        // === TESTE DE VISIBILIDADE TOTAL (AABB totalmente dentro) ===
        bool Contains(const cp_api::physics3D::AABB& box) const {
            for (const auto& plane : planes) {
                cp_api::math::Vec3 n = plane.normal;
                cp_api::math::Vec3 p = box.max;
                if (n.x < 0) p.x = box.min.x;
                if (n.y < 0) p.y = box.min.y;
                if (n.z < 0) p.z = box.min.z;
                if (glm::dot(n, p) + plane.distance < 0)
                    return false;
            }
            return true;
        }
    };
}
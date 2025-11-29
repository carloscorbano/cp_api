#pragma once

#include "cp_api/core/math.hpp"
#include "cp_api/physics/aabb.hpp"
#include <functional>

namespace cp_api {
    class World;
    struct TransformComponent {
        friend class World;
        TransformComponent(const Vec3& position,
                           const Quat& rotation,
                           const Vec3& scale,
                           const physics3D::AABB& boundary) noexcept
            : position(position), rotation(rotation), scale(scale) {
                this->boundary = physics3D::AABB(position + boundary.min, position + boundary.max, boundary.userData);
            }

        Vec3 position;
        Quat rotation;
        Vec3 scale;
        TransformComponent* parent = nullptr;

        physics3D::AABB boundary;

        void Translate(Vec3 direction, float amount) {
            TransformComponent oldTc = *this;

            position += direction * amount;
            boundary.min += direction * amount;
            boundary.max += direction * amount;

            if(onTransformChangedCallback) {
                onTransformChangedCallback(this->m_entityID, oldTc.position, oldTc.rotation, oldTc.scale, this->position, this->rotation, this->scale, oldTc.boundary, this->boundary);
            }
        }

        Mat4 GetModelMatrix() const {
            Mat4 model{1.0f};

            model = glm::translate(model, position);
            model = glm::mat4_cast(rotation) * model;
            model = glm::scale(model, scale);

            return model;
        }

        Mat4 GetWorldMatrix() const {
            if (parent)
                return parent->GetWorldMatrix() * GetModelMatrix();
            return GetModelMatrix();
        }
    private:
        uint32_t m_entityID;
        std::function<void(uint32_t& id, const Vec3& oldPos, const Quat& oldRot, const Vec3& oldScale, const Vec3& newPos, const Quat& newQuat, const Vec3& newScale, const physics3D::AABB& oldBoundary, const physics3D::AABB& newBoundary)> onTransformChangedCallback;
    };
} // namespace cp_api

#pragma once

#include "cp_api/core/math.hpp"
#include "cp_api/physics/aabb.hpp"
#include "cp_api/core/events.hpp"
#include <functional>

namespace cp_api {
    class World;
    struct onTransformChanged : Event {
        uint32_t id; 
        Vec3 oldPos;
        Quat oldRot;
        Vec3 oldScale;
        Vec3 newPos;
        Quat newRot;
        Vec3 newScale;
        physics3D::AABB oldBoundary;
        physics3D::AABB newBoundary;
    };

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
            onTransformChanged otc{};
            otc.id = m_entityID;
            otc.oldPos = position;
            otc.oldRot = rotation;
            otc.oldScale = scale;
            otc.oldBoundary = boundary;

            position += direction * amount;
            boundary.min += direction * amount;
            boundary.max += direction * amount;

            otc.newPos = position;
            otc.newRot = rotation;
            otc.newScale = scale;
            otc.newBoundary = boundary;
            
            onTransformEvents.Emit<onTransformChanged>(otc);
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
        EventDispatcher onTransformEvents;
    };
} // namespace cp_api

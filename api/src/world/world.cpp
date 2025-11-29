#include "cp_api/world/world.hpp"
#include "cp_api/components/transformComponent.hpp"
#include "cp_api/core/debug.hpp"

namespace cp_api {
    World::World() 
        : m_worldSpace(physics3D::AABB(Vec3(-10'000), Vec3(10'000))) {

        setupCallbacks();
    }

    World::~World() {
        m_registry.clear();
    }
    
    void World::Update(const double& delta) {
    }
    
    void World::FixedUpdate(const double& delta) {

    }

    void World::setupCallbacks() {
        m_registry.on_construct<TransformComponent>().connect<&World::onTransformAddCallback>(this);
        m_registry.on_destroy<TransformComponent>().connect<&World::onTransformRemovedCallback>(this);
    }

    void World::onTransformAddCallback(entt::registry& reg, entt::entity e) {
        TransformComponent& tc = reg.get<TransformComponent>(e);
        tc.m_entityID = (uint32_t)e;
        tc.onTransformChangedCallback = [&](uint32_t& id, 
            const Vec3& oldPos,
            const Quat& oldRot, 
            const Vec3& oldScale, 
            const Vec3& newPos, 
            const Quat& newQuat, 
            const Vec3& newScale, 
            const physics3D::AABB& oldBoundary, 
            const physics3D::AABB& newBoundary) {
                m_worldSpace.Update(id, oldBoundary, newBoundary);
        };

        physics3D::AABB b(tc.position - tc.boundary.Min(), tc.position + tc.boundary.Max());
        m_worldSpace.Insert((uint32_t)e, b, nullptr);
    }

    void World::onTransformRemovedCallback(entt::registry& reg, entt::entity e) {
        TransformComponent& tc = reg.get<TransformComponent>(e);
        m_worldSpace.Remove((uint32_t)e, tc.boundary);
    }
} // namespace cp_api

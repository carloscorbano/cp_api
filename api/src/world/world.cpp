#include "cp_api/world/world.hpp"
#include "cp_api/components/transformComponent.hpp"
#include "cp_api/core/debug.hpp"

namespace cp_api {
    World::World() 
        : m_worldSpace(physics3D::AABB(math::Vec3(-10'000), math::Vec3(10'000))) {

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
        m_worldSpace.Insert((uint32_t)e, tc.boundary, nullptr);
    }

    void World::onTransformRemovedCallback(entt::registry& reg, entt::entity e) {
        TransformComponent& tc = reg.get<TransformComponent>(e);
        m_worldSpace.Remove((uint32_t)e, tc.boundary);
    }
} // namespace cp_api

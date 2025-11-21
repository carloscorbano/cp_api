#pragma once

#include <entt/entt.hpp>
#include "cp_api/physics/spatialTree3D.hpp"

namespace cp_api {
    class World {
    public:
        World();
        ~World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;

        void Update(const double& delta);
        void FixedUpdate(const double& delta);

        entt::registry& GetRegistry() { return m_registry; }
        SpatialTree3D& GetWorldSpace() { return m_worldSpace; }
    private:
        void setupCallbacks();
        void onTransformAddCallback(entt::registry& reg, entt::entity e);
        void onTransformRemovedCallback(entt::registry& reg, entt::entity e);
    private:
        entt::registry m_registry;
        SpatialTree3D m_worldSpace;
    };
} // namespace cp_api 
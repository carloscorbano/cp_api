#pragma once

#include <entt/entt.hpp>

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
    private:
    };
} // namespace cp_api 
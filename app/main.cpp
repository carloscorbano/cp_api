// #include <iostream>
// #include <cp_api/framework.hpp>
// #include <cp_api/core/serializable.hpp>

// int main() {

//     using namespace cp_api;
//     try {
//         cp_api::Framework framework;
//         framework.Init();
//         framework.Run();
//     } catch(const std::exception& e){
//         std::cerr << "Exception: " << e.what() << std::endl;
//         return -1;
//     }

//     return 0;
// }

#include <cp_api/physics/spatialTree2D.hpp>
#include <cp_api/physics/spatialTree3D.hpp>
#include <cp_api/core/debug.hpp>

int main() {
    using namespace cp_api;

    // =========================
    //  SETUP
    // =========================
    physics2D::AABB world2D({-100, -100}, {100, 100});
    physics3D::AABB world3D({-100, -100, -100}, {100, 100, 100});

    SpatialTree2D tree2D(world2D);
    SpatialTree3D tree3D(world3D);

    // =========================
    //  INSERT OBJECTS
    // =========================
    tree2D.Insert(1, physics2D::AABB({1,1}, {2,2}), 0x00000F00);
    tree2D.Insert(2, physics2D::AABB({-1,-1}, {-2,-2}), 0x000000F0);
    tree2D.Insert(3, physics2D::AABB({-10,10}, {-20,-20}), 0x0000000F);

    tree3D.Insert(1, physics3D::AABB({1,1,1}, {2,2,2}), 0x00000F00);
    tree3D.Insert(2, physics3D::AABB({-1,-1,-1}, {-2,-2,-2}), 0x0000000F);
    tree3D.Insert(3, physics3D::AABB({-10,-10,-10}, {-20,-20,-20}), 0x000000F0);

    // =========================
    //  2D QUERIES
    // =========================
    {
        CP_LOG_INFO("=== 2D Circle Query ===");
        std::vector<uint32_t> ids;
        tree2D.QueryCircle(shapes2D::Circle({0,0}, 50), ids, 0x000000F0);
        CP_LOG_INFO("Found {} items (mask 0xF0)", ids.size());

        std::vector<physics2D::HitInfo> hits;
        tree2D.QueryCircle(shapes2D::Circle({0,0}, 5), hits);
        for (auto& h : hits) {
            CP_LOG_INFO("Hit id={} dist={:.2f} pen={:.2f} n=({:.2f},{:.2f})",
                        h.hitID, h.distance, h.penetration, h.normal.x, h.normal.y);
        }
    }

    {
        CP_LOG_INFO("=== 2D Capsule Query ===");
        std::vector<uint32_t> ids;
        tree2D.QueryCapsule(shapes2D::Capsule({0,1}, {0,-1}, 20), ids, 0xFFFFFFFF);
        CP_LOG_INFO("Found {} items", ids.size());
    }

    {
        CP_LOG_INFO("=== 2D Ray Query ===");
        std::vector<physics2D::HitInfo> rayHits;
        physics2D::Ray ray({-5, 0}, {1, 0});
        tree2D.QueryRay(ray, rayHits, 10.0f);
        for (auto& h : rayHits) {
            CP_LOG_INFO("Ray hit id={} dist={:.2f} point=({:.2f},{:.2f})",
                        h.hitID, h.distance, h.point.x, h.point.y);
        }
    }

    {
        CP_LOG_INFO("=== 2D AABB Query ===");
        std::vector<uint32_t> ids;
        physics2D::AABB queryBox({-2,-2}, {2,2});
        tree2D.QueryRange(queryBox, ids, 0xFFFFFFFF);
        CP_LOG_INFO("Found {} AABBs intersecting query box", ids.size());
    }

    // =========================
    //  3D QUERIES
    // =========================
    {
        CP_LOG_INFO("=== 3D Sphere Query ===");
        std::vector<uint32_t> ids;
        tree3D.QuerySphere(shapes3D::Sphere({0,0,0}, 50.5f), ids, 0x0000000F);
        CP_LOG_INFO("Found {} items (mask 0x0F)", ids.size());

        std::vector<physics3D::HitInfo> hits;
        tree3D.QuerySphere(shapes3D::Sphere({0,0,0}, 5.0f), hits);
        for (auto& h : hits) {
            CP_LOG_INFO("Hit id={} dist={:.2f} pen={:.2f} n=({:.2f},{:.2f},{:.2f})",
                        h.hitID, h.distance, h.penetration, h.normal.x, h.normal.y, h.normal.z);
        }
    }

    {
        CP_LOG_INFO("=== 3D Capsule Query ===");
        std::vector<uint32_t> ids;
        tree3D.QueryCapsule(shapes3D::Capsule({0,1,0}, {0,-1,0}, 5), ids, 0xFFFFFFFF);
        CP_LOG_INFO("Found {} items", ids.size());
    }

    {
        CP_LOG_INFO("=== 3D Ray Query ===");
        std::vector<physics3D::HitInfo> rayHits;
        physics3D::Ray ray({-5, 0, 0}, {1, 0, 0});
        tree3D.QueryRay(ray, rayHits, 10.0f);
        for (auto& h : rayHits) {
            CP_LOG_INFO("Ray hit id={} dist={:.2f} point=({:.2f},{:.2f},{:.2f})",
                        h.hitID, h.distance, h.point.x, h.point.y, h.point.z);
        }
    }

    {
        CP_LOG_INFO("=== 3D AABB Query ===");
        std::vector<uint32_t> ids;
        physics3D::AABB queryBox({-2,-2,-2}, {2,2,2});
        tree3D.QueryRange(queryBox, ids, 0xFFFFFFFF);
        CP_LOG_INFO("Found {} AABBs intersecting query box", ids.size());
    }

    return 0;
}

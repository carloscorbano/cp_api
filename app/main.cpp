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

#include <cp_api/core/debug.hpp>
#include <cp_api/physics/spatialTree2D.hpp>
#include <cp_api/physics/spatialTree3D.hpp>

int main() {
    using namespace cp_api;

    physics2D::AABB worldBounds2D({-100,-100}, {100,100});
    SpatialTree2D tree2D(worldBounds2D);

    physics3D::AABB worldBounds3D({-100,-100,-100}, {100,100,100});
    SpatialTree3D tree3D(worldBounds3D);

    tree2D.Insert(100, physics2D::AABB({1,1}, {2,2}), 1, 0xFFFFFFFF);
    tree2D.Insert(101, physics2D::AABB({3,3}, {4,4}), 1, 0xFFFFFFFF);
    
    physics2D::Ray ray({0,0}, {1,1}, 1);
    std::vector<physics2D::RayHit> hits;
    tree2D.Raycast(ray, hits, 100.0f);

    if(hits.empty()) 
        CP_LOG_WARN("HIT ANYTHING!");
    else 
        CP_LOG_WARN("HIT! TOTAL: {}", hits.size());


    tree3D.Insert(1, physics3D::AABB({1,1,1}, {2,2,2}), 1);
    
    physics3D::Ray ray3D({0,0,0}, {1,1,1}, 1);
    std::vector<physics3D::RayHit> hits3D;
    tree3D.Raycast(ray3D, hits3D, 100.0f);

    if(hits3D.empty()) 
        CP_LOG_WARN("HIT ANYTHING!");
    else 
        CP_LOG_WARN("HIT! TOTAL: {}", hits3D.size());


    //TODO: arrumar o calculo da normal!
    return 0;
}
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

#include <iostream>
#include <cp_api/containers/spatialTree2D.hpp>
#include <cp_api/containers/spatialTree3D.hpp>

int main() {
     using namespace cp_api;

    std::cout << "=== SpatialTree 2D ===\n";
    // Cria uma árvore 2D com área de 0,0 a 10,10
    AABB2 bounds2D({0,0}, {10,10});
    SpatialTree2D tree2D(bounds2D);

    // Inserção de itens 2D
    std::vector<Vec2> items2D = {{1,1}, {5,5}, {8,2}};
    for (int i = 0; i < items2D.size(); ++i) {
        tree2D.Insert(i, AABB2(items2D[i], items2D[i])); // ponto como AABB min=max
    }

    // QueryRange 2D
    AABB2 queryArea2D({0,0}, {6,6});
    std::vector<uint32_t> ids;
    tree2D.QueryRange(queryArea2D, ids);
    std::cout << "QueryRange encontrou " << ids.size() << " itens 2D\n";
    for (auto idx : ids)
        std::cout << " - item " << idx << "\n";

    std::cout << "\n=== SpatialTree 3D ===\n";
    // Cria uma árvore 3D com área de 0,0,0 a 10,10,10
    AABB3 bounds3D({0,0,0}, {10,10,10});
    SpatialTree3D tree3D(bounds3D);

    // Inserção de itens 3D
    std::vector<Vec3> items3D = {{1,1,1}, {5,5,5}, {8,2,3}};
    for (int i = 0; i < items3D.size(); ++i) {
        tree3D.Insert(i, AABB3(items3D[i], items3D[i])); // ponto como AABB min=max
    }

    // QueryRange 3D
    AABB3 queryArea3D({0,0,0}, {8,6,6});
    std::vector<uint32_t> ids2;
    tree3D.QueryRange(queryArea3D, ids2);
    std::cout << "QueryRange encontrou " << ids2.size() << " itens 3D\n";
    for (auto idx : ids2)
        std::cout << " - item " << idx << "\n";

    return 0;
}
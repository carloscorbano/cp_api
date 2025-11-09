#include <iostream>
#include <cp_api/framework.hpp>
#include <cp_api/core/serializable.hpp>

int main() {
    using namespace cp_api;
    try {
        cp_api::Framework framework;
        framework.Init();
        framework.Run();
    } catch(const std::exception& e){
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

// #include <cp_api/physics/spatialTree2D.hpp>
// #include <cp_api/physics/spatialTree3D.hpp>
// #include <cp_api/core/debug.hpp>

// int main() {
//     using namespace cp_api;

//     // =========================
//     //  SETUP
//     // =========================
//     physics2D::AABB world2D({-100, -100}, {100, 100});
//     physics3D::AABB world3D({-100, -100, -100}, {100, 100, 100});

//     SpatialTree2D tree2D(world2D);
//     SpatialTree3D tree3D(world3D);


//     struct TestUserData {
//         bool b;
//         uint32_t a;
//     };

//     TestUserData t = { true, 150 };

//     // =========================
//     //  INSERT OBJECTS
//     // =========================
//     tree2D.Insert(1, physics2D::AABB({1,1}, {2,2}), &t, 0x00000F00);
//     tree2D.Insert(2, physics2D::AABB({-1,-1}, {-2,-2}), nullptr, 0x000000F0);
//     tree2D.Insert(3, physics2D::AABB({-10,10}, {-20,-20}), nullptr, 0x0000000F);

//     tree3D.Insert(1, physics3D::AABB({1,1,1}, {2,2,2}), nullptr, 0x00000F00);
//     tree3D.Insert(2, physics3D::AABB({-1,-1,-1}, {-2,-2,-2}), nullptr, 0x0000000F);
//     tree3D.Insert(3, physics3D::AABB({-10,-10,-10}, {-20,-20,-20}), nullptr, 0x000000F0);

//     // =========================
//     //  2D QUERIES
//     // =========================
//     {
//         CP_LOG_INFO("=== 2D Circle Query ===");
//         std::vector<uint32_t> ids;
//         tree2D.QueryCircle(shapes2D::Circle({0,0}, 50), ids, 0x000000F0);
//         CP_LOG_INFO("Found {} items (mask 0xF0)", ids.size());

//         std::vector<physics2D::HitInfo> hits;
//         tree2D.QueryCircle(shapes2D::Circle({0,0}, 5), hits);
//         for (auto& h : hits) {
//             CP_LOG_INFO("Hit id={} dist={:.2f} pen={:.2f} n=({:.2f},{:.2f})",
//                         h.id, h.distance, h.penetration, h.normal.x, h.normal.y);
//         }
//     }

//     {
//         CP_LOG_INFO("=== 2D Capsule Query ===");
//         std::vector<uint32_t> ids;
//         tree2D.QueryCapsule(shapes2D::Capsule({0,1}, {0,-1}, 20), ids, 0xFFFFFFFF);
//         CP_LOG_INFO("Found {} items", ids.size());
//     }

//     {
//         CP_LOG_INFO("=== 2D Ray Query ===");
//         std::vector<physics2D::HitInfo> rayHits;
//         physics2D::Ray ray({-5, 0}, {1, 0});
//         tree2D.QueryRay(ray, rayHits, 10.0f);
//         for (auto& h : rayHits) {
//             CP_LOG_INFO("Ray hit id={} dist={:.2f} point=({:.2f},{:.2f})",
//                         h.id, h.distance, h.point.x, h.point.y);
//         }
//     }

//     {
//         CP_LOG_INFO("=== 2D AABB Query ===");
//         std::vector<uint32_t> ids;
//         physics2D::AABB queryBox({-2,-2}, {2,2});
//         tree2D.QueryRange(queryBox, ids, 0xFFFFFFFF);
//         CP_LOG_INFO("Found {} AABBs intersecting query box", ids.size());
//     }

//     // =========================
//     //  3D QUERIES
//     // =========================
//     {
//         CP_LOG_INFO("=== 3D Sphere Query ===");
//         std::vector<uint32_t> ids;
//         tree3D.QuerySphere(shapes3D::Sphere({0,0,0}, 50.5f), ids, 0x0000000F);
//         CP_LOG_INFO("Found {} items (mask 0x0F)", ids.size());

//         std::vector<physics3D::HitInfo> hits;
//         tree3D.QuerySphere(shapes3D::Sphere({0,0,0}, 5.0f), hits);
//         for (auto& h : hits) {
//             CP_LOG_INFO("Hit id={} dist={:.2f} pen={:.2f} n=({:.2f},{:.2f},{:.2f})",
//                         h.hitID, h.distance, h.penetration, h.normal.x, h.normal.y, h.normal.z);
//         }
//     }

//     {
//         CP_LOG_INFO("=== 3D Capsule Query ===");
//         std::vector<uint32_t> ids;
//         tree3D.QueryCapsule(shapes3D::Capsule({0,1,0}, {0,-1,0}, 5), ids, 0xFFFFFFFF);
//         CP_LOG_INFO("Found {} items", ids.size());
//     }

//     {
//         CP_LOG_INFO("=== 3D Ray Query ===");
//         std::vector<physics3D::HitInfo> rayHits;
//         physics3D::Ray ray({-5, 0, 0}, {1, 0, 0});
//         tree3D.QueryRay(ray, rayHits, 10.0f);
//         for (auto& h : rayHits) {
//             CP_LOG_INFO("Ray hit id={} dist={:.2f} point=({:.2f},{:.2f},{:.2f})",
//                         h.hitID, h.distance, h.point.x, h.point.y, h.point.z);
//         }
//     }

//     {
//         CP_LOG_INFO("=== 3D AABB Query ===");
//         std::vector<uint32_t> ids;
//         physics3D::AABB queryBox({-2,-2,-2}, {2,2,2});
//         tree3D.QueryRange(queryBox, ids, 0xFFFFFFFF);
//         CP_LOG_INFO("Found {} AABBs intersecting query box", ids.size());
//     }

//     return 0;
// }


// #include <GLFW/glfw3.h>
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>

// #include <cp_api/physics/spatialTree3D.hpp>
// #include <cp_api/shapes/frustum.hpp>
// #include <iostream>
// #include <vector>
// #include <unordered_set>

// using namespace cp_api;

// // Função utilitária para desenhar um cubo wireframe a partir de um AABB
// void drawWireCube(const physics3D::AABB& box) {
//     const glm::vec3& min = box.min;
//     const glm::vec3& max = box.max;

//     glBegin(GL_LINES);
//     glColor3f(1,1,1);

//     // 12 edges
//     glVertex3f(min.x,min.y,min.z); glVertex3f(max.x,min.y,min.z);
//     glVertex3f(min.x,min.y,min.z); glVertex3f(min.x,max.y,min.z);
//     glVertex3f(min.x,min.y,min.z); glVertex3f(min.x,min.y,max.z);

//     glVertex3f(max.x,max.y,max.z); glVertex3f(min.x,max.y,max.z);
//     glVertex3f(max.x,max.y,max.z); glVertex3f(max.x,min.y,max.z);
//     glVertex3f(max.x,max.y,max.z); glVertex3f(max.x,max.y,min.z);

//     glVertex3f(min.x,max.y,min.z); glVertex3f(max.x,max.y,min.z);
//     glVertex3f(min.x,max.y,min.z); glVertex3f(min.x,max.y,max.z);

//     glVertex3f(max.x,min.y,min.z); glVertex3f(max.x,max.y,min.z);
//     glVertex3f(max.x,min.y,min.z); glVertex3f(max.x,min.y,max.z);

//     glVertex3f(min.x,min.y,max.z); glVertex3f(max.x,min.y,max.z);
//     glVertex3f(min.x,min.y,max.z); glVertex3f(min.x,max.y,max.z);
//     glEnd();
// }

// // Câmera simples
// glm::vec3 camPos(50.0f, 50.0f, 150.0f);
// glm::vec3 camFront(0,0,-1);
// glm::vec3 camUp(0,1,0);
// float yaw = -90.0f, pitch = 0.0f;
// float lastX = 400, lastY = 300;
// bool firstMouse = true;

// // Controle de movimento
// float deltaTime = 0.0f, lastFrame = 0.0f;

// void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
//     if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
//     float xoffset = xpos - lastX;
//     float yoffset = lastY - ypos;
//     lastX = xpos; lastY = ypos;
//     float sensitivity = 0.1f;
//     xoffset *= sensitivity;
//     yoffset *= sensitivity;
//     yaw += xoffset;
//     pitch += yoffset;
//     if(pitch > 89.0f) pitch = 89.0f;
//     if(pitch < -89.0f) pitch = -89.0f;
//     glm::vec3 front;
//     front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
//     front.y = sin(glm::radians(pitch));
//     front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
//     camFront = glm::normalize(front);
// }

// int main() {
//     if (!glfwInit()) return -1;

//     GLFWwindow* window = glfwCreateWindow(800, 600, "SpatialTree 3D Frustum Test", nullptr, nullptr);
//     if (!window) { glfwTerminate(); return -1; }

//     glfwMakeContextCurrent(window);
//     glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
//     glfwSetCursorPosCallback(window, mouse_callback);

//     glEnable(GL_DEPTH_TEST);
//     glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

//     // Criar SpatialTree3D
//     physics3D::AABB worldBounds{{0,0,0},{100,100,100}};
//     SpatialTree3D tree(worldBounds);

//     // Inserir cubos
//     tree.Insert(1, {{10,10,10},{20,20,20}}, nullptr);
//     tree.Insert(2, {{30,30,30},{40,40,40}}, nullptr);
//     tree.Insert(3, {{50,50,50},{60,60,60}}, nullptr);

//     std::unordered_set<uint32_t> inFrustumLastFrame;

//     while(!glfwWindowShouldClose(window)) {
//         float currentFrame = glfwGetTime();
//         deltaTime = currentFrame - lastFrame;
//         lastFrame = currentFrame;

//         // Movimentação
//         float speed = 50.0f * deltaTime;
//         if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += camFront * speed;
//         if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= camFront * speed;
//         if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= glm::normalize(glm::cross(camFront, camUp)) * speed;
//         if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += glm::normalize(glm::cross(camFront, camUp)) * speed;

//         glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//         // View / Projection
//         glm::mat4 proj = glm::perspective(glm::radians(45.0f), 800.0f/600.0f, 0.1f, 300.0f);
//         glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);

//         glMatrixMode(GL_PROJECTION);
//         glLoadMatrixf(&proj[0][0]);
//         glMatrixMode(GL_MODELVIEW);
//         glLoadMatrixf(&view[0][0]);

//         // Criar frustum
//         shapes3D::Frustum frustum = shapes3D::Frustum::FromMatrix(proj * view);

//         std::vector<uint32_t> ids;
//         tree.QueryFrustum(frustum, ids);

//         // Log de entrada/saída no frustum
//         std::unordered_set<uint32_t> currentInFrustum(ids.begin(), ids.end());
//         for(uint32_t id : currentInFrustum) {
//             if(inFrustumLastFrame.find(id) == inFrustumLastFrame.end())
//                 std::cout << "[ENTER] Cubo ID: " << id << " entrou no frustum\n";
//         }
//         for(uint32_t id : inFrustumLastFrame) {
//             if(currentInFrustum.find(id) == currentInFrustum.end())
//                 std::cout << "[EXIT] Cubo ID: " << id << " saiu do frustum\n";
//         }
//         inFrustumLastFrame = currentInFrustum;

//         // Desenhar cubos
//         std::vector<const SpatialTree3D::Node*> leafNodes;
//         tree.GetLeafNodes(leafNodes);
//         for(auto node : leafNodes) {
//             for(const auto& e : node->items) {
//                 drawWireCube(e.bounds);
//             }
//         }

//         glfwSwapBuffers(window);
//         glfwPollEvents();
//     }

//     glfwDestroyWindow(window);
//     glfwTerminate();
//     return 0;
// }

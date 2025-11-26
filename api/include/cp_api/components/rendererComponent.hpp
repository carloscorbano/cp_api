#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>
#include "cp_api/graphics/vkBuffer.hpp"

namespace cp_api {

    // -----------------------------------------------------------------------------
    // Estruturas auxiliares
    // -----------------------------------------------------------------------------

    struct Submesh {
        uint32_t indexOffset = 0;
        uint32_t indexCount  = 0;
        uint32_t materialIndex = 0; // Material para este submesh
    };

    struct MeshData {
        VulkanBuffer vertexBuffer;
        VulkanBuffer indexBuffer;
        uint32_t vertexCount  = 0;
        uint32_t indexCount   = 0;

        std::vector<Submesh> submeshes;
    };

    struct MaterialData {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;

        // Para casos mais avançados (SSR, Shadows, etc)
        uint32_t flags = 0;
    };

    // Push constant para transform ou outro dado pequeno
    struct PCObject {
        glm::mat4 model;
        uint32_t objectID;
    };

    struct PCPush {
        glm::mat4 model;
        glm::mat4 viewProj;
    };

    // LOD system
    struct MeshLOD {
        std::shared_ptr<MeshData> mesh;
        float minDistance = 0.0f;
        float maxDistance = 1e9f;
    };

    class RendererComponent {
    public:
        // Pode ter múltiplos LODs
        std::vector<MeshLOD> lods;

        // Pode ter múltiplos materiais
        std::vector<MaterialData> materials;

        // Buffer para instancing (opcional)
        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        uint32_t instanceCount  = 0;

        // Enable / disable
        bool visible = true;

        // Para debug e profiling
        uint32_t objectID = 0;

        // -----------------------------------------------------------------------------
        // Seleciona o LOD baseado na distância da câmera
        // -----------------------------------------------------------------------------
        MeshData* SelectLOD(float distanceToCamera) const {
            for(const auto& lod : lods) {
                if(distanceToCamera >= lod.minDistance && distanceToCamera <= lod.maxDistance)
                    return lod.mesh.get();
            }
            // fallback: LOD 0
            return lods.empty() ? nullptr : lods[0].mesh.get();
        }

        // -----------------------------------------------------------------------------
        // Draw — compatível com Command Buffers Secundários + Dynamic Rendering
        // -----------------------------------------------------------------------------
       void Draw(
            VkCommandBuffer cb,
            const glm::mat4& modelMatrix,
            const glm::mat4& viewProj,     // NOVO: viewProj da câmera
            float distanceToCamera
        ) const 
        {
            if(!visible || lods.empty()) return;

            MeshData* mesh = SelectLOD(distanceToCamera);
            if(!mesh || mesh->vertexBuffer.GetBuffer() == VK_NULL_HANDLE) return;

            // Bind vertex buffer
            VkDeviceSize offset = 0;
            VkBuffer vbuf = mesh->vertexBuffer.GetBuffer();
            vkCmdBindVertexBuffers(cb, 0, 1, &vbuf, &offset);

            // Bind index buffer if existe
            const bool hasIndex = (mesh->indexBuffer.GetBuffer() != VK_NULL_HANDLE && mesh->indexCount > 0);
            if (hasIndex) {
                vkCmdBindIndexBuffer(cb, mesh->indexBuffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            }

            // Prepare push constants (model + viewProj) — corresponde ao CreateTestPipeline (2 mat4)
            PCPush pc{};
            pc.model = modelMatrix;
            pc.viewProj = viewProj;

            // Iterate submeshes
            for (const auto& sub : mesh->submeshes)
            {
                if (sub.materialIndex >= materials.size()) continue;
                const auto& mat = materials[sub.materialIndex];
                if (mat.pipeline == VK_NULL_HANDLE) continue;

                // Bind pipeline
                vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.pipeline);

                // Bind descriptor set if present (descriptors must be bound inside SECONDARY)
                if (mat.descriptor != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(
                        cb,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        mat.layout,
                        0,              // firstSet
                        1,              // descriptorCount
                        &mat.descriptor,
                        0,
                        nullptr
                    );
                }

                // Push constants (model + viewProj)
                vkCmdPushConstants(
                    cb,
                    mat.layout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(PCPush),
                    &pc
                );

                // Draw
                if (hasIndex) {
                    vkCmdDrawIndexed(
                        cb,
                        sub.indexCount,                            // indexCount for this submesh
                        (instanceCount == 0 ? 1u : instanceCount),
                        sub.indexOffset,                           // firstIndex (baseIndex)
                        0,                                         // vertexOffset
                        0                                          // firstInstance
                    );
                } else {
                    vkCmdDraw(
                        cb,
                        mesh->vertexCount,
                        (instanceCount == 0 ? 1u : instanceCount),
                        0,
                        0
                    );
                }
            }
        }
    };

} // namespace cp_api

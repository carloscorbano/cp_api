#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <limits>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace cp_api {
    /**
     * @brief Generic spatial tree (quadtree/octree) for spatial partitioning and fast queries.
     * @tparam VecT Vector type (Vec2/Vec3)
     * @tparam AABBT Axis-Aligned Bounding Box type
     * @tparam RayT Ray type
     * @tparam RayHitT Raycast hit structure
     * @tparam ChildCount Number of children per node (4 for quadtree, 8 for octree)
     */
    template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
    class SpatialTree
    {
    public:
        struct Entry
        {
            uint32_t id;
            AABBT bounds;
            uint32_t layer{0};
            uint32_t mask{0xFFFFFFFF}; // padrão: colide com tudo
        };

        struct Node
        {
            AABBT bounds;
            int depth{0};
            bool subdivided{false};
            std::vector<Entry> items;
            std::unique_ptr<Node> children[ChildCount]{nullptr};
        };
    public:
        /**
         * @brief Construct a new SpatialTree
         * @param worldBounds Bounds of the entire world.
         * @param capacity Max items per node before subdividing.
         * @param maxDepth Max tree depth.
         */
        SpatialTree(const AABBT& worldBounds, int capacity=4, int maxDepth=8) noexcept;

        /**
         * @brief Destroy the SpatialTree and release all memory.
         */
        ~SpatialTree() noexcept;

        /**
         * @brief Insert an object with its bounding box.
         * @param id Unique object identifier.
         * @param bounds Object bounds.
         */
        void Insert(uint32_t id,const AABBT& bounds, uint32_t layer = 0, uint32_t mask = 0xFFFFFFFF);

        /**
         * @brief Remove an object by ID and bounds.
         * @param id Object ID.
         * @param bounds Previous bounds.
         * @return true if removed.
         */
        bool Remove(uint32_t id,const AABBT& bounds);

        /**
         * @brief Update an object's bounds.
         * @param id Object ID.
         * @param oldBounds Previous bounds.
         * @param newBounds New bounds.
         * @return true if successfully updated.
         */
        bool Update(uint32_t id,const AABBT& oldBounds,const AABBT& newBounds);

        /**
         * @brief Batch update multiple objects.
         * @param oldNewBounds Vector of {old,new} bounds.
         * @param ids Vector of object IDs.
         * @return number of successful updates.
         */
        size_t UpdateMany(const std::vector<std::pair<AABBT,AABBT>>& oldNewBounds,const std::vector<uint32_t>& ids);

        /**
         * @brief Clear all nodes and items from the tree.
         */
        void Clear() noexcept;

        /**
         * @brief Query objects intersecting a bounding box.
         * @param range Query bounds.
         * @param outIds Output vector of object IDs.
         */
        void QueryRange(const AABBT& range,std::vector<uint32_t>& outIds, uint32_t queryMask) const;

        /**
         * @brief Query objects containing a point.
         * @param p Point to test.
         * @param outIds Output vector of IDs.
         */
        void QueryPoint(const VecT& p,std::vector<uint32_t>& outIds) const;

        /**
         * @brief Query with callback for each item in range.
         * @param range Query bounds.
         * @param cb Callback function(uint32_t id, const AABBT& bounds) -> bool
         *           Return false to stop iteration.
         * @return Number of items processed.
         */
        size_t QueryRangeCallback(const AABBT& range,const std::function<bool(uint32_t,const AABBT&)>& cb) const;

        /**
         * @brief Raycast to find all hits.
         * @param ray Ray to cast.
         * @param outHits Output vector of hits.
         * @param tMax Maximum ray distance.
         */
        void Raycast(const RayT& ray,std::vector<RayHitT>& outHits,float tMax=std::numeric_limits<float>::max()) const;

        /**
         * @brief Raycast to find closest hit.
         * @param ray Ray to cast.
         * @param outHit Closest hit output.
         * @param tMax Maximum ray distance.
         * @return true if hit found.
         */
        bool RaycastClosest(const RayT& ray,RayHitT& outHit,float tMax=std::numeric_limits<float>::max()) const;

        /**
         * @brief Raycast returning vector of all hits.
         * @param ray Ray to cast.
         * @param tMax Maximum distance.
         * @return vector of hits sorted by distance.
         */
        std::vector<RayHitT> RaycastAll(const RayT& ray,float tMax=std::numeric_limits<float>::max()) const;

        /**
         * @brief Get total number of leaf nodes.
         */
        size_t GetNodeCount() const noexcept;

        /**
         * @brief Get total number of objects.
         */
        size_t GetItemCount() const noexcept;

        /**
         * @brief Collect all object IDs in the tree.
         */
        void GetAllItems(std::vector<uint32_t>& out) const;

        /**
         * @brief Collect all leaf nodes.
         */
        void GetLeafNodes(std::vector<const Node*>& out) const;

    private:
        std::unique_ptr<Node> m_root;
        int m_capacity;
        int m_maxDepth;
        size_t m_count{0};

        // Internal helpers
        void insert(Node& node,const Entry& e);
        bool remove(Node& node,uint32_t id,const AABBT& bounds);
        void clear(Node& node) noexcept;
        int childIndexFor(const Node& node,const AABBT& b) const;
        void subdivide(Node& node);

        void query(const Node& node,const AABBT& range,std::vector<uint32_t>& out, uint32_t queryMask) const;
        void queryPointNode(const Node& node,const VecT& p,std::vector<uint32_t>& out) const;
        size_t queryRangeCallbackNode(const Node& node,const AABBT& range,const std::function<bool(uint32_t,const AABBT&)>& cb) const;

        void raycastNode(const Node& node,const RayT& ray,std::vector<RayHitT>& out,float tMax) const;
        bool raycastClosestNode(const Node& node,const RayT& ray,float& bestT,RayHitT& out,float tMax) const;

        void collectItems(const Node& node,std::vector<uint32_t>& out) const;
        void collectLeafNodes(const Node& node,std::vector<const Node*>& out) const;

        size_t nodeCount(const Node& node) const noexcept;
    };

    #include "spatialTree.inl"
}

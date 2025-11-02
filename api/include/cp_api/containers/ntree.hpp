#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <stack>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <concepts>

namespace cp_api {

    /**
     * @brief Concept to ensure type T is compatible with nlohmann::json serialization.
     */
    template <typename T>
    concept JsonSerializable = requires(const T& t, nlohmann::json& j) {
        { nlohmann::json(t) } -> std::same_as<nlohmann::json>;
        { t = j.get<T>() };
    };

    /**
     * @class NTree
     * @brief Generic N-ary tree with JSON serialization and moveable subtrees.
     * 
     * @tparam T Type of the data stored in each node. Must be serializable with nlohmann::json.
     */
    template <JsonSerializable T>
    class NTree {
    public:
        /// Forward declaration of Node
        struct Node;
        using NodePtr = std::shared_ptr<Node>;

        /**
         * @struct Node
         * @brief Represents a node in the tree.
         */
        struct Node {
            T data;                            ///< Stored data
            std::weak_ptr<Node> parent;        ///< Weak reference to parent node
            std::vector<NodePtr> children;     ///< List of child nodes
        };

        /// @name Constructors / Destructor
        /// @{
        explicit NTree(const T& rootData = T());
        ~NTree() = default;
        /// @}

        /// @name Basic Operations
        /// @{
        NodePtr GetRoot() const;
        void Clear(bool resetRoot = false);
        NodePtr AddChild(const NodePtr& parent, const T& value);
        bool RemoveChild(const NodePtr& parent, const NodePtr& child);
        bool MoveSubtree(const NodePtr& node, const NodePtr& newParent);
        int Depth(const NodePtr& node) const;
        int Height(const NodePtr& node) const;
        bool IsLeaf(const NodePtr& node) const;
        size_t CountNodes(const NodePtr& node) const;
        void Traverse(const NodePtr& start, const std::function<void(const NodePtr&)>& fn) const;
        NodePtr FindNode(const NodePtr& start, const std::function<bool(const NodePtr&)>& pred) const;
        /// @}

        /// @name JSON Serialization
        /// @{
        nlohmann::json ToJson() const;
        void FromJson(const nlohmann::json& j);
        /// @}

    private:
        NodePtr m_root;

        /// @name Helper Functions
        /// @{
        bool IsDescendant(const NodePtr& node, const NodePtr& candidate) const;
        nlohmann::json NodeToJson(const NodePtr& node) const;
        NodePtr JsonToNode(const nlohmann::json& j, const NodePtr& parent);
        /// @}
    };

} // namespace CPFramework

#include "ntree.inl"  // Include the implementation for template class
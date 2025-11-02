#pragma once
#include "ntree.hpp"

namespace cp_api {
    template <JsonSerializable T>
    NTree<T>::NTree(const T& rootData) {
        m_root = std::make_shared<Node>();
        m_root->data = rootData;
    }

    template <JsonSerializable T>
    typename NTree<T>::NodePtr NTree<T>::GetRoot() const {
        return m_root;
    }

    template <JsonSerializable T>
    void NTree<T>::Clear(bool resetRoot) {
        if (resetRoot) {
            m_root = std::make_shared<Node>();
        } else {
            m_root->children.clear();
            m_root->data = T();
        }
    }

    template <JsonSerializable T>
    typename NTree<T>::NodePtr NTree<T>::AddChild(const NodePtr& parent, const T& value) {
        if (!parent) return nullptr;
        auto child = std::make_shared<Node>();
        child->data = value;
        child->parent = parent;
        parent->children.push_back(child);
        return child;
    }

    template <JsonSerializable T>
    bool NTree<T>::RemoveChild(const NodePtr& parent, const NodePtr& child) {
        if (!parent || !child) return false;
        auto& children = parent->children;
        auto it = std::remove(children.begin(), children.end(), child);
        if (it != children.end()) {
            children.erase(it, children.end());
            return true;
        }
        return false;
    }

    template <JsonSerializable T>
    bool NTree<T>::MoveSubtree(const NodePtr& node, const NodePtr& newParent) {
        if (!node || !newParent || node == m_root) return false;
        if (IsDescendant(node, newParent)) return false;

        auto oldParent = node->parent.lock();
        if (oldParent) RemoveChild(oldParent, node);
        node->parent = newParent;
        newParent->children.push_back(node);
        return true;
    }

    template <JsonSerializable T>
    int NTree<T>::Depth(const NodePtr& node) const {
        int d = 0;
        auto cur = node;
        while (cur && cur != m_root) {
            cur = cur->parent.lock();
            ++d;
        }
        return d;
    }

    template <JsonSerializable T>
    int NTree<T>::Height(const NodePtr& node) const {
        if (!node) return 0;
        int maxChildHeight = 0;
        for (auto& c : node->children) {
            maxChildHeight = std::max(maxChildHeight, Height(c));
        }
        return maxChildHeight + 1;
    }

    template <JsonSerializable T>
    bool NTree<T>::IsLeaf(const NodePtr& node) const {
        return node && node->children.empty();
    }

    template <JsonSerializable T>
    size_t NTree<T>::CountNodes(const NodePtr& node) const {
        if (!node) return 0;
        size_t count = 1;
        for (auto& c : node->children) {
            count += CountNodes(c);
        }
        return count;
    }

    template <JsonSerializable T>
    void NTree<T>::Traverse(const NodePtr& start, const std::function<void(const NodePtr&)>& fn) const {
        if (!start) return;
        std::stack<NodePtr> stack;
        stack.push(start);
        while (!stack.empty()) {
            auto current = stack.top();
            stack.pop();
            fn(current);
            for (auto it = current->children.rbegin(); it != current->children.rend(); ++it)
                stack.push(*it);
        }
    }

    template <JsonSerializable T>
    typename NTree<T>::NodePtr NTree<T>::FindNode(const NodePtr& start, const std::function<bool(const NodePtr&)>& pred) const {
        NodePtr result = nullptr;
        Traverse(start, [&](const NodePtr& n) {
            if (!result && pred(n)) result = n;
        });
        return result;
    }

    template <JsonSerializable T>
    nlohmann::json NTree<T>::ToJson() const {
        return NodeToJson(m_root);
    }

    template <JsonSerializable T>
    void NTree<T>::FromJson(const nlohmann::json& j) {
        m_root = JsonToNode(j, nullptr);
    }

    template <JsonSerializable T>
    bool NTree<T>::IsDescendant(const NodePtr& node, const NodePtr& candidate) const {
        bool found = false;
        Traverse(node, [&](const NodePtr& n) {
            if (n == candidate) found = true;
        });
        return found;
    }

    template <JsonSerializable T>
    nlohmann::json NTree<T>::NodeToJson(const NodePtr& node) const {
        if (!node) return {};
        nlohmann::json j;
        j["data"] = node->data;
        j["children"] = nlohmann::json::array();
        for (auto& c : node->children) {
            j["children"].push_back(NodeToJson(c));
        }
        return j;
    }

    template <JsonSerializable T>
    typename NTree<T>::NodePtr NTree<T>::JsonToNode(const nlohmann::json& j, const NodePtr& parent) {
        auto node = std::make_shared<Node>();
        node->data = j.value("data", T());
        node->parent = parent;
        for (auto& childJson : j.value("children", nlohmann::json::array())) {
            node->children.push_back(JsonToNode(childJson, node));
        }
        return node;
    }

} // namespace CPFramework
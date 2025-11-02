#pragma once
#include <limits>
#include <algorithm>
#include <utility>
#include <vector>
#include <functional>

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::SpatialTree(
    const AABBT& worldBounds,
    int capacity,
    int maxDepth
) noexcept
    : m_root(std::make_unique<Node>()), m_capacity(capacity), m_maxDepth(maxDepth)
{
    m_root->bounds = worldBounds;
    m_root->depth = 0;
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::~SpatialTree() noexcept
{
    Clear();
}

// =============================
// Public API
// =============================
template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::Insert(uint32_t id,const AABBT& bounds)
{
    insert(*m_root, {id, bounds});
    ++m_count;
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
bool cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::Remove(uint32_t id,const AABBT& bounds)
{
    if (remove(*m_root, id, bounds)) {
        --m_count;
        return true;
    }
    return false;
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
bool cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::Update(uint32_t id,const AABBT& oldBounds,const AABBT& newBounds)
{
    if (Remove(id, oldBounds)) {
        Insert(id, newBounds);
        return true;
    }
    return false;
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
size_t cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::UpdateMany(
    const std::vector<std::pair<AABBT,AABBT>>& oldNewBounds,
    const std::vector<uint32_t>& ids)
{
    size_t updated = 0;
    for (size_t i = 0; i < ids.size(); ++i)
        if(Update(ids[i], oldNewBounds[i].first, oldNewBounds[i].second))
            ++updated;
    return updated;
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::Clear() noexcept
{
    clear(*m_root);
    m_count = 0;
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::QueryRange(const AABBT& range,std::vector<uint32_t>& outIds) const
{
    query(*m_root, range, outIds);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::QueryPoint(const VecT& p,std::vector<uint32_t>& outIds) const
{
    queryPointNode(*m_root, p, outIds);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
size_t cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::QueryRangeCallback(
    const AABBT& range,
    const std::function<bool(uint32_t,const AABBT&)>& cb
) const
{
    return queryRangeCallbackNode(*m_root, range, cb);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::Raycast(
    const RayT& ray,
    std::vector<RayHitT>& outHits,
    float tMax
) const
{
    raycastNode(*m_root, ray, outHits, tMax);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
bool cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::RaycastClosest(
    const RayT& ray,
    RayHitT& outHit,
    float tMax
) const
{
    float bestT = tMax;
    return raycastClosestNode(*m_root, ray, bestT, outHit, tMax);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
std::vector<RayHitT> cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::RaycastAll(
    const RayT& ray,
    float tMax
) const
{
    std::vector<RayHitT> hits;
    Raycast(ray, hits, tMax);
    std::sort(hits.begin(), hits.end(), [](const RayHitT& a,const RayHitT& b){ return a.t < b.t; });
    return hits;
}

// =============================
// Node / Item count
// =============================
template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
size_t cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::GetNodeCount() const noexcept
{
    return nodeCount(*m_root);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
size_t cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::GetItemCount() const noexcept
{
    return m_count;
}

// =============================
// Collect all items
// =============================
template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::GetAllItems(std::vector<uint32_t>& out) const
{
    collectItems(*m_root, out);
}

// =============================
// Collect all leaf nodes
// =============================
template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::GetLeafNodes(std::vector<const Node*>& out) const
{
    collectLeafNodes(*m_root, out);
}

// =============================
// Internal Helpers
// =============================
template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::insert(Node& node, const Entry& e)
{
    // Caso o nó já esteja subdividido
    if (node.subdivided)
    {
        int idx = childIndexFor(node, e.bounds);

        // ✅ Se o objeto cabe totalmente em um filho, insere lá
        if (idx >= 0 && node.children[idx]->bounds.Contains(e.bounds))
        {
            insert(*node.children[idx], e);
            return;
        }

        // ❌ Caso o objeto cruze limites → permanece neste nó
    }

    node.items.push_back(e);

    // Subdivisão automática
    if (node.items.size() > m_capacity && node.depth < m_maxDepth)
    {
        if (!node.subdivided)
            subdivide(node);

        // Tentativa de redistribuição dos objetos existentes
        std::vector<Entry> remaining;
        remaining.reserve(node.items.size());

        for (auto& i : node.items)
        {
            int idx = childIndexFor(node, i.bounds);
            if (idx >= 0 && node.children[idx]->bounds.Contains(i.bounds))
                insert(*node.children[idx], i);
            else
                remaining.push_back(std::move(i)); // mantém no pai
        }

        node.items.swap(remaining);
    }
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
bool cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::remove(Node& node, uint32_t id, const AABBT& bounds)
{
    // Remove localmente primeiro
    auto it = std::find_if(node.items.begin(), node.items.end(),
        [id](const Entry& e) { return e.id == id; });

    if (it != node.items.end())
    {
        node.items.erase(it);
        return true;
    }

    bool removed = false;
    if (node.subdivided)
    {
        // Pode estar em múltiplos filhos, então verificar todos que intersectam
        for (int i = 0; i < ChildCount; ++i)
        {
            if (node.children[i] && node.children[i]->bounds.Intersects(bounds))
                removed |= remove(*node.children[i], id, bounds);
        }

        // 🔄 Colapsar se todos filhos ficarem vazios
        bool allEmpty = true;
        for (int i = 0; i < ChildCount; ++i)
        {
            if (node.children[i] &&
                (!node.children[i]->items.empty() || node.children[i]->subdivided))
            {
                allEmpty = false;
                break;
            }
        }

        if (allEmpty)
        {
            for (int i = 0; i < ChildCount; ++i)
                node.children[i].reset();
            node.subdivided = false;
        }
    }

    return removed;
}


template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::clear(Node& node) noexcept
{
    for(int i=0;i<ChildCount;++i) node.children[i].reset();
    node.items.clear();
    node.subdivided=false;
}

// =============================
// Child Index & Subdivide
// =============================
// Versão robusta e compatível com subdivide() do seu código
template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
int cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::childIndexFor(const Node& node,const AABBT& b) const
{
    // Pequeno epsilon para evitar erros por imprecisão de float
    constexpr float eps = 1e-6f;

    VecT c = node.bounds.Center();
    VecT bMin = b.Min();
    VecT bMax = b.Max();

    if constexpr (ChildCount == 4)
    {
        int x, y;

        // X: 0 -> completamente à esquerda (max <= center)
        //    1 -> completamente à direita  (min >= center)
        if (bMax.x <= c.x + eps) x = 0;
        else if (bMin.x >= c.x - eps) x = 1;
        else return -1; // cruza a linha vertical do centro

        // Y: 0 -> completamente "abaixo" (max <= center)
        //    1 -> completamente "acima"  (min >= center)
        if (bMax.y <= c.y + eps) y = 0;
        else if (bMin.y >= c.y - eps) y = 1;
        else return -1; // cruza a linha horizontal do centro

        // A convenção usada em subdivide() é: index = y*2 + x
        return y * 2 + x;
    }
    else if constexpr (ChildCount == 8)
    {
        int x, y, z;

        if (bMax.x <= c.x + eps) x = 0;
        else if (bMin.x >= c.x - eps) x = 1;
        else return -1;

        if (bMax.y <= c.y + eps) y = 0;
        else if (bMin.y >= c.y - eps) y = 1;
        else return -1;

        if (bMax.z <= c.z + eps) z = 0;
        else if (bMin.z >= c.z - eps) z = 1;
        else return -1;

        // Convenção usada originalmente: idx = x + (y<<1) + (z<<2)
        return x + (y << 1) + (z << 2);
    }

    return -1; // para qualquer outro caso
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::subdivide(Node& node)
{
    VecT min = node.bounds.Min();
    VecT max = node.bounds.Max();
    VecT center = node.bounds.Center();

    for(int i=0;i<ChildCount;++i)
    {
        node.children[i] = std::make_unique<Node>();
        node.children[i]->depth = node.depth + 1;

        if constexpr(ChildCount==4) {
            VecT cMin = min, cMax = max;
            if(i==0){ cMax.x=center.x; cMax.y=center.y; }
            else if(i==1){ cMin.x=center.x; cMax.y=center.y; }
            else if(i==2){ cMax.x=center.x; cMin.y=center.y; }
            else if(i==3){ cMin.x=center.x; cMin.y=center.y; }
            node.children[i]->bounds = AABBT(cMin, cMax);
        } else if constexpr(ChildCount==8) {
            VecT cMin = min, cMax = max;
            if(i & 1) cMin.x = center.x; else cMax.x = center.x;
            if(i & 2) cMin.y = center.y; else cMax.y = center.y;
            if(i & 4) cMin.z = center.z; else cMax.z = center.z;
            node.children[i]->bounds = AABBT(cMin, cMax);
        }
    }
    node.subdivided = true;
}

// =============================
// Query / Raycast Helpers
// =============================
template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::query(
    const Node& node,
    const AABBT& range,
    std::vector<uint32_t>& out
) const
{
    if(!node.bounds.Intersects(range)) return;
    for(const auto& e : node.items) if(e.bounds.Intersects(range)) out.push_back(e.id);
    if(node.subdivided) for(int c=0;c<ChildCount;++c) query(*node.children[c], range, out);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::queryPointNode(
    const Node& node,
    const VecT& p,
    std::vector<uint32_t>& out
) const
{
    if(!node.bounds.Contains(p)) return;
    for(const auto& e : node.items) if(e.bounds.Contains(p)) out.push_back(e.id);
    if(node.subdivided) for(int c=0;c<ChildCount;++c) queryPointNode(*node.children[c], p, out);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
size_t cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::queryRangeCallbackNode(
    const Node& node,
    const AABBT& range,
    const std::function<bool(uint32_t,const AABBT&)>& cb
) const
{
    if(!node.bounds.Intersects(range)) return 0;
    size_t count=0;
    for(const auto& e : node.items)
    {
        if(e.bounds.Intersects(range))
        {
            ++count;
            if(!cb(e.id,e.bounds)) return count;
        }
    }
    if(node.subdivided) for(int c=0;c<ChildCount;++c) count+=queryRangeCallbackNode(*node.children[c], range, cb);
    return count;
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::raycastNode(
    const Node& node,
    const RayT& ray,
    std::vector<RayHitT>& out,
    float tMax
) const
{
    if(!node.bounds.Intersects(ray,tMax)) return;
    for(const auto& e : node.items)
    {
        RayHitT hit;
        if(e.bounds.Intersects(ray, hit, tMax)) out.push_back(std::move(hit));
    }
    if(node.subdivided) for(int c=0;c<ChildCount;++c) raycastNode(*node.children[c], ray, out, tMax);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
bool cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::raycastClosestNode(
    const Node& node,
    const RayT& ray,
    float& bestT,
    RayHitT& out,
    float tMax
) const
{
    if(!node.bounds.Intersects(ray,tMax)) return false;
    bool hitSomething=false;
    for(const auto& e : node.items)
    {
        RayHitT hit;
        if(e.bounds.Intersects(ray, hit, bestT) && hit.t < bestT)
        {
            bestT = hit.t;
            out = std::move(hit);
            hitSomething = true;
        }
    }
    if(node.subdivided)
        for(int c=0;c<ChildCount;++c)
            if(raycastClosestNode(*node.children[c], ray, bestT, out, tMax))
                hitSomething = true;
    return hitSomething;
}

// =============================
// Utility Traversals
// =============================
template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::collectItems(
    const Node& node,
    std::vector<uint32_t>& out
) const
{
    for(const auto& e : node.items) out.push_back(e.id);
    if(node.subdivided) for(int c=0;c<ChildCount;++c) collectItems(*node.children[c], out);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
void cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::collectLeafNodes(
    const Node& node,
    std::vector<const Node*>& out
) const
{
    if(!node.subdivided) { out.push_back(&node); return; }
    for(int c=0;c<ChildCount;++c) collectLeafNodes(*node.children[c], out);
}

template<typename VecT, typename AABBT, typename RayT, typename RayHitT, int ChildCount>
size_t cp_api::SpatialTree<VecT,AABBT,RayT,RayHitT,ChildCount>::nodeCount(const Node& node) const noexcept
{
    size_t count = 1;
    if(node.subdivided) for(int c=0;c<ChildCount;++c) count += nodeCount(*node.children[c]);
    return count;
}
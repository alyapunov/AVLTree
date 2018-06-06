#pragma once

#include <cstddef>
#include <utility>
#include <iterator>
#include <cassert>

namespace Avl
{

struct Node
{
    Node* m_Parent; // nullptr for root node
    Node* m_Child[2]; // { left-lesser, right-bigger }
    bool m_ChildBigger[2];
    bool m_IsRight;
};

inline const Node* traverse(const Node* aNode, bool aBackward);
inline Node* traverse(Node* aNode, bool aBackward);

template <class Item>
struct Default
{
    template <class Key>
    static int Compare(const Item& aItem1, const Key& aItem2)
    {
        return aItem1 < aItem2 ? -1 : aItem2 < aItem1 ? 1 : 0;
    }
};

template <class Item, Node Item::*NodeMember, class Comparator = Default<Item>>
class Tree
{
public:
    // Iterators
    template <class TItem, class TNode>
    class iterator_common : std::iterator<std::input_iterator_tag, TItem>
    {
    public:
        explicit iterator_common(TNode* aNode) : m_Node(aNode) {}
        TItem& operator*() const { return *objByNode(m_Node); }
        TItem* operator->() const { return objByNode(m_Node); }
        bool operator==(const iterator_common& aItr) { return m_Node == aItr.m_Node; }
        bool operator!=(const iterator_common& aItr) { return m_Node != aItr.m_Node; }
        iterator_common& operator++() { m_Node = traverse(m_Node, false); return *this; }
        iterator_common operator++(int) { iterator_common aTmp = *this; ++(*this); return aTmp; }
        iterator_common& operator--() { m_Node = traverse(m_Node, true); return *this; }
        iterator_common operator--(int) { iterator_common aTmp = *this; --(*this); return aTmp; }
    private:
        TNode* m_Node;
    };
    using iterator = iterator_common<Item, Node>;
    using const_iterator = iterator_common<const Item, const Node>;

    // Access
    const_iterator begin() const { return const_iterator(m_Min); }
    const_iterator end() const { return const_iterator(nullptr); }
    iterator begin() { return iterator(m_Min); }
    iterator end() { return iterator(nullptr); }
    const_iterator min() const { return const_iterator(m_Min); }
    const_iterator max() const { return const_iterator(m_Max); }
    iterator min() { return iterator(m_Min); }
    iterator max() { return iterator(m_Max); }
    size_t size() const { return m_Size; }

    template <class Key>
    const const_iterator find(const Key& aKey) const { return const_iterator(lookup(aKey)); }
    template <class Key>
    iterator find(const Key& aKey) { return iterator(lookup(aKey)); }

    // Modification
    inline std::pair<iterator, bool> insert(Item& aItem); // bool - success
    inline void replace(Item& aItem, Item& aNewItem);
    inline void erase(Item& aItem);
    void clear() { m_Root = m_Min = m_Max = nullptr; m_Size = 0; }

    // Low level access
    const Item* getRoot() const { return objByNodeSafe(m_Root); }
    static const Item* getLeft(const Item* aItem) { return objByNodeSafe((aItem->*NodeMember).m_Child[0]); }
    static const Item* getRight(const Item* aItem) { return objByNodeSafe((aItem->*NodeMember).m_Child[1]); }
    static bool isLeftBigger(const Item* aItem) { return (aItem->*NodeMember).m_ChildBigger[0]; }
    static bool isRightBigger(const Item* aItem) { return (aItem->*NodeMember).m_ChildBigger[1]; }

    // Debug
    inline int selfCheck() const;

private:
    Node* m_Root = nullptr;
    Node* m_Min = nullptr;
    Node* m_Max = nullptr;
    size_t m_Size = 0;

    static inline const Item* objByNode(const Node* aNode);
    static inline Item* objByNode(Node* aNode);
    static inline const Item* objByNodeSafe(const Node* aNode);
    static inline Item* objByNodeSafe(Node* aNode);
    template <class Key>
    inline const Node* lookup(const Key& aKey) const;
    template <class Key>
    inline Node* lookup(const Key& aKey);
    inline void rebalanceInsert(Node* sNode);
    inline void rebalanceErase(Node* aNode, bool aRight);
    inline void relink(Node* aNode);
    inline void relinkParent(Node* aOldNode, Node* aNewNode);
    inline void relinkParentSafe(Node* aOldNode, Node* aNewNode);
    inline void relinkChild(Node* aNewParent, Node* aNewChild, bool aRight);
    inline void relinkChildSafe(Node* aNewParent, Node* aNewChild, bool aRight);
    inline int checkSubTree(const Node* aNode, size_t& aHeight, size_t& aSize) const;
};

//////////////////////////////////////////////////////////////////
////////////////////////// Implementaion /////////////////////////
//////////////////////////////////////////////////////////////////

template <class Item, Node Item::*NodeMember, class Comparator>
std::pair<typename Tree<Item, NodeMember, Comparator>::iterator, bool>
Tree<Item, NodeMember, Comparator>::insert(Item& aItem)
{
    // Search for a parent for the coming leaf node
    Node** sParentPtr = &m_Root;
    Node* sParent = m_Root;
    bool sIsRight = false;
    Node* sNode = &(aItem.*NodeMember);
    bool sOneDirection[2] = {true, true};
    while (nullptr != *sParentPtr)
    {
        sParent = *sParentPtr;
        int sCmp = Comparator::Compare(aItem, *objByNode(*sParentPtr));
        if (0 == sCmp)
            return std::make_pair(iterator(*sParentPtr), false);
        sOneDirection[sCmp < 0] = false;
        sIsRight = sCmp > 0;
        sParentPtr = &((*sParentPtr)->m_Child)[sIsRight];
    }

    // Insert the leaf node
    sNode->m_Parent = sParent;
    sNode->m_Child[0] = sNode->m_Child[1] = nullptr;
    sNode->m_ChildBigger[0] = sNode->m_ChildBigger[1] = false;
    sNode->m_IsRight = sIsRight;

    m_Size++;
    *sParentPtr = sNode;
    if (sOneDirection[0])
        m_Min = sNode;
    if (sOneDirection[1])
        m_Max = sNode;

    // Rebalance if necessary
    rebalanceInsert(sNode);

    return std::make_pair(iterator(sNode), true);
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::erase(Item& aItem)
{
    m_Size--;
    Node* sNode = &(aItem.*NodeMember);

    if (m_Min == sNode)
        m_Min = nullptr != sNode->m_Child[1] ? sNode->m_Child[1] : sNode->m_Parent;
    if (m_Max == sNode)
        m_Max = nullptr != sNode->m_Child[0] ? sNode->m_Child[0] : sNode->m_Parent;

    // A node from which rebalancing will start.
    Node* sRebalanceNode = sNode->m_Parent;
    // Which child of sRebalanceNode decreased its height.
    bool sRebalanceRight = sNode->m_IsRight;

    if (nullptr == sNode->m_Child[0] && nullptr == sNode->m_Child[1])
    {
        // Leaf. Just unlink it from parent; beware of the only root node.
        if (nullptr == sNode->m_Parent)
            m_Root = nullptr;
        else
            sNode->m_Parent->m_Child[sNode->m_IsRight] = nullptr;
    }
    else
    {
        // Not leaf. Find closest by value node from bigger subtree (sReplacement)
        // Remove sReplacement from the tree and then replace sNode with sReplacement.
        bool sRight = sNode->m_ChildBigger[0];
        bool sLeft = !sRight;

        Node* sReplacement = sNode->m_Child[sLeft];
        while (nullptr != sReplacement->m_Child[sRight])
            sReplacement = sReplacement->m_Child[sRight];
        sRebalanceNode = sReplacement->m_Parent;
        sRebalanceRight = sReplacement->m_IsRight;
        if (nullptr != sReplacement->m_Child[sLeft])
        {
            // Not leaf again. Good news is that left child is a leaf node.
            assert(nullptr == sReplacement->m_Child[sLeft]->m_Child[0] &&
                   nullptr == sReplacement->m_Child[sLeft]->m_Child[1]);
            relinkParent(sReplacement, sReplacement->m_Child[sLeft]);
        }
        else
        {
            // Found leaf replacement. Just unlink it from parent.
            sReplacement->m_Parent->m_Child[sReplacement->m_IsRight] = nullptr;
        }

        // We are about to replace sNode, check links.
        if (sRebalanceNode == sNode)
        {
            sRebalanceNode = sReplacement;
            sReplacement->m_ChildBigger[0] = sReplacement->m_ChildBigger[1] = false;
        }

        // Replace sNode with sReplacement
        *sReplacement = *sNode;
        relink(sReplacement);
    }

    rebalanceErase(sRebalanceNode, sRebalanceRight);
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::rebalanceInsert(Node* sNode)
{
    // A child node sNode of sParent node has just increased its height. Rebalance it recursively.
    while (nullptr != sNode->m_Parent)
    {
        // Let's think that sNode is the right child of sParent.
        // Due to node implementation a mirror balancing will have the same code.
        bool sRight = sNode->m_IsRight;
        bool sLeft = !sRight;
        Node* sParent = sNode->m_Parent;

        if (sParent->m_ChildBigger[sLeft])
        {
            // Another sibling of sNode was bigger. Now it's not.
            sParent->m_ChildBigger[sLeft] = false;
            // sParent did not increased its height. Nothing to do.
            return;
        }
        else if (!sParent->m_ChildBigger[sRight])
        {
            // Well-balanced subtree sParent became not well balanced.
            // No rotation is needed, but sParent increased its height, so continue with it.
            sParent->m_ChildBigger[sRight] = true;
            sNode = sParent;
            continue;
        }

        // Note that in any case sNode grew because of just on its subtrees, not both.
        assert(sNode->m_ChildBigger[0] != sNode->m_ChildBigger[1]);

        // The right child of sParent has +2 height than the left. This must be fixed via rotation.
        if (sNode->m_ChildBigger[sRight])
        {
            // Right child of sNode is bigger than left. Make 'single' rotation.
            /* On the pseudograffiti below (P) - sParent, (N) - sNode, (L),(C) and (R) - other nodes.
            *
             *          (P)                            (N)
             *        /     \                       /      \
             *      (L)     (N)                   (P)       (R)
             *      / \    /   \      --->       /  \       /  \
             *     /___\ (C)   (R)             (L)  (C)    /    \
             *           / \   / \             / \  / \   /      \
             *          /___\ /   \           /___\/___\ /________\
             *               /     \
             *              /_______\
             */
            relinkParentSafe(sParent, sNode);
            relinkChildSafe(sParent, sNode->m_Child[sLeft], sRight);
            relinkChild(sNode, sParent, sLeft);
            sNode->m_ChildBigger[0] = sNode->m_ChildBigger[1] = false;
            sParent->m_ChildBigger[0] = sParent->m_ChildBigger[1] = false;
            // Note that we have just fixed the growth of subtree, so exit.
            return;
        }
        else
        {
            // Left child of sNode is bigger than right. Make 'double' rotation.
            /* On the pseudograffiti below (P) - sParent, (N) - sNode, (L),(C),(C1),(C2) and (R) - other nodes.
             *
             *   (1)     (P)                            (C)
             *        /      \                       /       \
             *      (L)       (N)                 (P)         (N)
             *      / \      /   \      -->      /    \      /    \
             *     /___\   (C)   (R)           (L)   (C1)  (C2)    (R)
             *            /   \   | \          / \   /__\  /  \    / \
             *          (C1)  (C2)|  \        /___\       /____\  /   \
             *          /__\  /  \|___\                          /_____\
             *               /____\
             *
             *   (2)     (P)                            (C)
             *        /      \                       /       \
             *      (L)       (N)                 (P)         (N)
             *      / \      /   \      -->      /    \      /    \
             *     /___\   (C)   (R)           (L)   (C1)  (C2)    (R)
             *            /   \   | \          / \   /  \  /__\    / \
             *          (C1)  (C2)|  \        /___\ /____\        /   \
             *          /  \  /__\|___\                          /_____\
             *         /____\
             *
             *   (3)     (P)                            (C)
             *               \                       /       \
             *                (N)                 (P)         (N)
             *               /          -->
             *             (C)
             */
            // Note that, like children of sNode, only one child of (C) grew
            // OR (C) is a new node with both empty children.

            Node* sCenter = sNode->m_Child[sLeft]; // (C) in the picture
            assert((nullptr == sCenter->m_Child[0] && nullptr == sCenter->m_Child[1]) ||
                   (sCenter->m_ChildBigger[0] != sCenter->m_ChildBigger[1]));
            relinkParentSafe(sParent, sCenter);
            relinkChildSafe(sParent, sCenter->m_Child[sLeft], sRight);
            relinkChildSafe(sNode, sCenter->m_Child[sRight], sLeft);
            relinkChild(sCenter, sParent, sLeft);
            relinkChild(sCenter, sNode, sRight);
            sParent->m_ChildBigger[sRight] = false;
            sParent->m_ChildBigger[sLeft] = sCenter->m_ChildBigger[sRight];
            sNode->m_ChildBigger[sLeft] = false;
            sNode->m_ChildBigger[sRight] = sCenter->m_ChildBigger[sLeft];
            sCenter->m_ChildBigger[0] = sCenter->m_ChildBigger[1] = false;
            // Note that we have just fixed the growth of subtree, so exit.
            return;
        }
    }
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::rebalanceErase(Node* sParent, bool sRight)
{
    // Let's think that right subtree of sParent became smaller.
    bool sLeft = !sRight;
    while (nullptr != sParent)
    {
        if (sParent->m_ChildBigger[sRight])
        {
            // That child subtree was bigger. Now it's not.
            sParent->m_ChildBigger[sRight] = false;
            sRight = sParent->m_IsRight;
            sLeft = !sRight;
            sParent = sParent->m_Parent;
            continue;
        }
        else if (!sParent->m_ChildBigger[sLeft])
        {
            // sParent was well-balanced. Now it's not.
            sParent->m_ChildBigger[sLeft] = true;
            break;
        }

        // Now left subtree of sParent has +2 height that right. Need balance.
        Node* sNode = sParent->m_Child[sLeft];
        if (!sNode->m_ChildBigger[sRight])
        {
            // Right child of sNode is not bigger than left. Make 'single' rotation.
            /* On the pseudograffiti below (P) - sParent, (N) - sNode, (L),(C) and (R) - other nodes.
             *
             *   (1)        (P)                             (N)
             *            /    \                         /      \
             *          (N)     (R)                  (L)          (P)
             *         /   \    / \       --->       / \        /    \
             *       (L)   (C) /___\                /   \    (C)     (R)
             *       / \   / \                     /     \   / \     / \
             *      /   \ /___\                   /_______\ /___\   /___\
             *     /     \
             *    /_______\
             *
             *   (2)          (P)                           (N)
             *              /     \                       /      \
             *            (N)      (R)                (L)          (P)
             *          /    \     / \     --->       / \        /    \
             *       (L)      (C) /___\              /   \    (C)     (R)
             *       / \      / \                   /     \   / \     / \
             *      /   \    /   \                 /_______\ /   \   /___\
             *     /     \  /     \                         /     \
             *    /_______\/_______\                       /_______\
             */
            bool sNodeWasBalanced = !sNode->m_ChildBigger[sLeft];
            relinkParentSafe(sParent, sNode);
            relinkChildSafe(sParent, sNode->m_Child[sRight], sLeft);
            relinkChild(sNode, sParent, sRight);
            sNode->m_ChildBigger[sLeft] = sParent->m_ChildBigger[sRight] = false;
            sNode->m_ChildBigger[sRight] = sParent->m_ChildBigger[sLeft] = sNodeWasBalanced;
            if (sNodeWasBalanced)
                return; // (2) Subtree height is unchanged.
            sRight = sNode->m_IsRight;
            sLeft = !sRight;
            sParent = sNode->m_Parent;
        }
        else
        {
            // Right child of sNode is bigger than left. Make 'double' rotation.
            /* On the pseudograffiti below (P) - sParent, (N) - sNode, (L),(C),(C1),(C2) and (R) - other nodes.
             *
             *   (1)          (P)                            (C)
             *              /     \                       /      \
             *            (N)      (R)                (N)          (P)
             *          /    \     / \     --->      /  \        /    \
             *       (L)      (C) /___\            (L)  (C1)   (C2)   (R)
             *       / \      / \                  / \   / \   / \    / \
             *      /___\  (C1) (C2)              /___\ /___\ /___\  /___\
             *            /  \  /  \
             *           /____\/____\
             *
             *   (2)          (P)                            (C)
             *              /     \                       /      \
             *            (N)      (R)                (N)          (P)
             *          /    \     / \     --->      /  \        /    \
             *       (L)      (C) /___\            (L)  (C1)   (C2)   (R)
             *       / \      / \                  / \  /__\   / \    / \
             *      /___\  (C1) (C2)              /___\       /___\  /___\
             *             /__\ /  \
             *                 /____\
             *
             *   (3)          (P)                            (C)
             *              /     \                       /      \
             *            (N)      (R)                (N)          (P)
             *          /    \     / \     --->      /  \        /    \
             *       (L)      (C) /___\            (L)  (C1)   (C2)   (R)
             *       / \      / \                  / \  /  \   /__\   / \
             *      /___\  (C1) (C2)              /___\/____\        /___\
             *            /  \  /__\
             *           /____\
             *
             *   (4)          (P)                            (C)
             *              /                             /      \
             *            (N)              --->       (N)          (P)
             *               \
             *                (C)
             */
            Node* sCenter = sNode->m_Child[sRight]; // (C) in the picture
            relinkParentSafe(sParent, sCenter);
            relinkChildSafe(sParent, sCenter->m_Child[sRight], sLeft);
            relinkChildSafe(sNode, sCenter->m_Child[sLeft], sRight);
            relinkChild(sCenter, sParent, sRight);
            relinkChild(sCenter, sNode, sLeft);
            sParent->m_ChildBigger[sLeft] = sNode->m_ChildBigger[sRight] = false;
            sParent->m_ChildBigger[sRight] = sCenter->m_ChildBigger[sLeft];
            sNode->m_ChildBigger[sLeft] = sCenter->m_ChildBigger[sRight];
            sCenter->m_ChildBigger[0] = sCenter->m_ChildBigger[1] = false;

            sRight = sCenter->m_IsRight;
            sLeft = !sRight;
            sParent = sCenter->m_Parent;
        }
    }
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::replace(Item& aItem, Item& aNewItem)
{
    Node* sNode = &(aItem.*NodeMember);
    Node* sNewNode = &(aNewItem.*NodeMember);
    *sNewNode = *sNode;
    relink(sNewNode);

    if (m_Min == sNode)
        m_Min = sNewNode;
    if (m_Max == sNode)
        m_Max = sNewNode;
}

template <class Item, Node Item::*NodeMember, class Comparator>
const Item* Tree<Item, NodeMember, Comparator>::objByNode(const Node* aNode)
{
    const uintptr_t sOffset = reinterpret_cast<uintptr_t>(&(reinterpret_cast<const Item*>(0)->*NodeMember));
    return reinterpret_cast<const Item*>(reinterpret_cast<const char*>(aNode) - sOffset);
}

template <class Item, Node Item::*NodeMember, class Comparator>
Item* Tree<Item, NodeMember, Comparator>::objByNode(Node* aNode)
{
    return const_cast<Item*>(objByNode(const_cast<const Node*>(aNode)));
}

template <class Item, Node Item::*NodeMember, class Comparator>
const Item* Tree<Item, NodeMember, Comparator>::objByNodeSafe(const Node* aNode)
{
    return nullptr == aNode ? nullptr : objByNode(aNode);
}

template <class Item, Node Item::*NodeMember, class Comparator>
Item* Tree<Item, NodeMember, Comparator>::objByNodeSafe(Node* aNode)
{
    return nullptr == aNode ? nullptr : objByNode(aNode);
}

template <class Item, Node Item::*NodeMember, class Comparator>
template <class Key>
const Node* Tree<Item, NodeMember, Comparator>::lookup(const Key& aKey) const
{
    const Node* sNode = m_Root;
    while (nullptr != sNode)
    {
        int sCmp = Comparator::Compare(*objByNode(sNode), aKey);
        if (0 == sCmp)
            break;
        sNode = sNode->m_Child[sCmp < 0];
    }
    return sNode;
}

template <class Item, Node Item::*NodeMember, class Comparator>
template <class Key>
Node* Tree<Item, NodeMember, Comparator>::lookup(const Key& aKey)
{
    const Tree<Item, NodeMember, Comparator>* sConstThis = this;
    return const_cast<Node*>(sConstThis->lookup(aKey));
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::relink(Node* aNode)
{
    if (nullptr != aNode->m_Parent)
        aNode->m_Parent->m_Child[aNode->m_IsRight] = aNode;
    else
        m_Root = aNode;
    if (nullptr != aNode->m_Child[0])
        aNode->m_Child[0]->m_Parent = aNode;
    if (nullptr != aNode->m_Child[1])
        aNode->m_Child[1]->m_Parent = aNode;
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::relinkParent(Node* aOldNode, Node* aNewNode)
{
    aNewNode->m_Parent = aOldNode->m_Parent;
    aNewNode->m_IsRight = aOldNode->m_IsRight;
    aNewNode->m_Parent->m_Child[aNewNode->m_IsRight] = aNewNode;
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::relinkParentSafe(Node* aOldNode, Node* aNewNode)
{
    aNewNode->m_Parent = aOldNode->m_Parent;
    aNewNode->m_IsRight = aOldNode->m_IsRight;
    if (nullptr != aNewNode->m_Parent)
        aNewNode->m_Parent->m_Child[aNewNode->m_IsRight] = aNewNode;
    else
        m_Root = aNewNode;
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::relinkChild(Node* aNewParent, Node* aNewChild, bool aRight)
{
    aNewParent->m_Child[aRight] = aNewChild;
    aNewChild->m_Parent = aNewParent;
    aNewChild->m_IsRight = aRight;
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::relinkChildSafe(Node* aNewParent, Node* aNewChild, bool aRight)
{
    aNewParent->m_Child[aRight] = aNewChild;
    if (nullptr != aNewChild)
    {
        aNewChild->m_Parent = aNewParent;
        aNewChild->m_IsRight = aRight;
    }
}

template <class Item, Node Item::*NodeMember, class Comparator>
int Tree<Item, NodeMember, Comparator>::selfCheck() const
{
    size_t sHeight, sSize;
    int sRes = checkSubTree(m_Root, sHeight, sSize);
    if (size() != sSize)
        sRes |= 1 << 0;
    const Node *sMin = m_Root, *sMax = m_Root;
    while (nullptr != sMin && nullptr != sMin->m_Child[0])
        sMin = sMin->m_Child[0];
    while (nullptr != sMax && nullptr != sMax->m_Child[1])
        sMax = sMax->m_Child[1];
    if (sMin != m_Min)
        sRes |= 1 << 1;
    if (sMax != m_Max)
        sRes |= 1 << 2;
    return sRes;
}

template <class Item, Node Item::*NodeMember, class Comparator>
int Tree<Item, NodeMember, Comparator>::checkSubTree(const Node* aNode, size_t& aHeight, size_t& aSize) const
{
    if (nullptr == aNode)
    {
        aHeight = 0;
        aSize = 0;
        return 0;
    }

    int sRes = 0;
    if (nullptr != aNode->m_Child[0] && aNode != aNode->m_Child[0]->m_Parent)
        sRes |= 1 << 4;
    if (nullptr != aNode->m_Child[1] && aNode != aNode->m_Child[1]->m_Parent)
        sRes |= 1 << 5;
    if (nullptr != aNode->m_Child[0] && aNode->m_Child[0]->m_IsRight)
        sRes |= 1 << 6;
    if (nullptr != aNode->m_Child[1] && !aNode->m_Child[1]->m_IsRight)
        sRes |= 1 << 7;

    if (nullptr != aNode->m_Child[0])
    {
        int sCmp = Comparator::Compare(*objByNode(aNode->m_Child[0]), *objByNode(aNode));
        if (sCmp == 0)
            sRes |= 1 << 8;
        else if (sCmp > 0)
            sRes |= 1 << 9;
    }
    if (nullptr != aNode->m_Child[1])
    {
        int sCmp = Comparator::Compare(*objByNode(aNode), *objByNode(aNode->m_Child[1]));
        if (sCmp == 0)
            sRes |= 1 << 10;
        else if (sCmp > 0)
            sRes |= 1 << 11;
    }

    size_t sHeight0, sHeight1, sSize0, sSize1;
    sRes |= checkSubTree(aNode->m_Child[0], sHeight0, sSize0);
    sRes |= checkSubTree(aNode->m_Child[1], sHeight1, sSize1);
    aHeight = 1 + (sHeight0 > sHeight1 ? sHeight0 : sHeight1);
    aSize = 1 + sSize0 + sSize1;

    if (sHeight0 == sHeight1)
    {
        if (aNode->m_ChildBigger[0])
            sRes |= 1 << 12;
        if (aNode->m_ChildBigger[1])
            sRes |= 1 << 13;
    }
    else if (sHeight0 > sHeight1)
    {
        // Left is bigger
        if (!aNode->m_ChildBigger[0])
            sRes |= 1 << 14;
        if (aNode->m_ChildBigger[1])
            sRes |= 1 << 15;
    }
    else
    {
        // Right is bigger
        if (aNode->m_ChildBigger[0])
            sRes |= 1 << 16;
        if (!aNode->m_ChildBigger[1])
            sRes |= 1 << 17;
    }
    if (sHeight0 > sHeight1 + 1)
    {
        // Left too big
        sRes |= 1 << 18;
    }
    else if (sHeight1 > sHeight0 + 1)
    {
        // Right too big
        sRes |= 1 << 19;
    }
    return sRes;
}

const Node* traverse(const Node* aNode, bool aBackward)
{
    // Let's think that traverse is always left-to-right
    bool sLeft = aBackward;
    bool sRight = !aBackward;

    if (nullptr != aNode->m_Child[sRight])
    {
        aNode = aNode->m_Child[sRight];
        while (nullptr != aNode->m_Child[sLeft])
            aNode = aNode->m_Child[sLeft];
        return aNode;
    }

    while (true)
    {
        bool sParentBigger = aNode->m_IsRight == sLeft;
        aNode = aNode->m_Parent;
        if (nullptr == aNode || sParentBigger)
            return aNode;
    }
}

Node* traverse(Node* aNode, bool aBackward)
{
    return const_cast<Node*>(traverse(const_cast<const Node*>(aNode), aBackward));
}

}; // namespace Avl
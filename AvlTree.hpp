#pragma once

#include <cstddef>
#include <utility>
#include <iterator>
#include <cassert>

namespace Avl
{

struct Node
{
    Node* m_Parent;
    Node* m_Child[2]; // { left-lesser, right-bigger }
    int32_t m_Balance; // >0 -> right subtree is bigger
    bool m_Way; // Position in m_Parent->m_Child[] array
    bool m_IsRoot; // true if the node is a member m_Root in TreeCommon class
    bool m_IsMinMax[2]; // = {isMin, isMax}
};

class TreeCommon
{
protected:
    Node m_Root;
    Node* m_Min = nullptr;
    Node* m_Max = nullptr;
    size_t m_Size = 0;
    TreeCommon() { m_Root.m_Child[0] = nullptr; m_Root.m_IsRoot = true; }

    inline void insert(Node* aParent, Node* aNode, bool aWay, bool aMin, bool aMax);
    inline void replace(Node* aNode, Node* aNewNode);
    inline void erase(Node* aNode);
    void clear() { m_Root.m_Child[0] = m_Min = m_Max = nullptr; m_Size = 0; }

    static inline const Node* step(const Node* aNode, bool aWay);
    static inline Node* step(Node* aNode, bool aWay);

private:
    inline void link(Node* aParent, Node* aChild, bool aWay);
    inline void linkSafe(Node* aParent, Node* aChild, bool aWay);
    inline void rotate(Node* aParent, bool aWay);
    inline void rebalanceInsert(Node* aNode);
    inline void rebalanceErase(Node* aParent, bool aWay);
};

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
class Tree : private TreeCommon
{
public:
    // Iterators
    // Iterators have decrement operators but they are not actually
    // standard bidirectional iterators since they cannot iterate from end() position.
    template <class TItem, class TNode>
    class iterator_common : std::iterator<std::input_iterator_tag, TItem>
    {
    public:
        iterator_common(TNode* aNode) : m_Node(aNode) {}
        TItem& operator*() const { return *item(m_Node); }
        TItem* operator->() const { return item(m_Node); }
        bool operator==(const iterator_common& aItr) { return m_Node == aItr.m_Node; }
        bool operator!=(const iterator_common& aItr) { return m_Node != aItr.m_Node; }
        iterator_common& operator++() { m_Node = step(m_Node, true); return *this; }
        iterator_common operator++(int) { iterator_common aTmp = *this; ++(*this); return aTmp; }
        iterator_common& operator--() { m_Node = step(m_Node, false); return *this; }
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
    using TreeCommon::clear;

    // Low level access
    const Item* getRoot() const { return itemSafe(m_Root.m_Child[0]); }
    const Item* getLeft(const Item* aItem) const { return itemSafe((aItem->*NodeMember).m_Child[0]); }
    const Item* getRight(const Item* aItem) const { return itemSafe((aItem->*NodeMember).m_Child[1]); }
    static bool isLeftBigger(const Item* aItem) { return (aItem->*NodeMember).m_Balance < 0; }
    static bool isRightBigger(const Item* aItem) { return (aItem->*NodeMember).m_Balance > 0; }

    // Debug
    inline int selfCheck() const;

private:
    static inline const Item* item(const Node* aNode);
    static inline Item* item(Node* aNode);
    static inline const Item* itemSafe(const Node* aNode);
    static inline Item* itemSafe(Node* aNode);
    template <class Key>
    inline const Node* lookup(const Key& aKey) const;
    template <class Key>
    inline Node* lookup(const Key& aKey);
    inline int checkSubTree(const Node* aNode, int& aHeight, size_t& aSize) const;
};

//////////////////////////////////////////////////////////////////
////////////////////////// Implementaion /////////////////////////
//////////////////////////////////////////////////////////////////

const Node* TreeCommon::step(const Node* aNode, bool aWay)
{
    if (aNode->m_IsMinMax[aWay])
        return nullptr;

    if (nullptr != aNode->m_Child[aWay])
    {
        aNode = aNode->m_Child[aWay];
        while (nullptr != aNode->m_Child[!aWay])
            aNode = aNode->m_Child[!aWay];
        return aNode;
    }

    while (aNode->m_Way == aWay)
        aNode = aNode->m_Parent;
    return aNode->m_Parent;
}

Node* TreeCommon::step(Node* aNode, bool aWay)
{
    return const_cast<Node*>(step(const_cast<const Node*>(aNode), aWay));
}

void TreeCommon::link(Node *aParent, Node *aChild, bool aWay)
{
    aParent->m_Child[aWay] = aChild;
    aChild->m_Parent = aParent;
    aChild->m_Way = aWay;
}

void TreeCommon::linkSafe(Node *aParent, Node *aChild, bool aWay)
{
    aParent->m_Child[aWay] = aChild;
    if (nullptr == aChild)
        return;
    aChild->m_Parent = aParent;
    aChild->m_Way = aWay;
}

void TreeCommon::rotate(Node *aParent, bool aWay)
{
    /* Pseudograffity for aWay == true:
     *
     *         P             N
     *        / \           / \
     *       N   B   -->   A   P
     *      / \               / \
     *     A   C             C   B
     */
    Node* sNode = aParent->m_Child[!aWay];
    assert(nullptr != sNode);

    link(aParent->m_Parent, sNode, aParent->m_Way);
    linkSafe(aParent, sNode->m_Child[aWay], !aWay);
    linkSafe(sNode, aParent, aWay);

    // The formulas below are empiric and are even valid for non balanced trees.
    int32_t sMirror = aWay ? 1 : -1; // TODO optimize?
    aParent->m_Balance += sMirror - sMirror * std::min(0, sMirror * sNode->m_Balance);  // TODO optimize?
    sNode->m_Balance += sMirror + sMirror * std::max(0, sMirror * aParent->m_Balance);  // TODO optimize?
}

void TreeCommon::rebalanceInsert(Node* aNode)
{
    while (!aNode->m_Parent->m_IsRoot)
    {
        bool sWay = aNode->m_Way;
        Node* sParent = aNode->m_Parent;
        int32_t sMirror = sWay ? 1 : -1; // TODO optimize?
        sParent->m_Balance += sMirror;

        if (sParent->m_Balance == 0)
        {
            // sParent was not equal-balanced, now it is; its height didn't change.
            return;
        }

        if (sParent->m_Balance == sMirror)
        {
            // sParent was equal-balanced, now it's not; its height increased.
            aNode = sParent;
            continue;
        }

        // Rotation is needed
        assert(sParent->m_Balance * sMirror == 2);
        if (sMirror * aNode->m_Balance < 0)
            rotate(aNode, sWay); // Double rotation
        rotate(sParent, !sWay);

        // In any case rotation decreased sParent's height, so done here.
        return;
    }
}

void TreeCommon::rebalanceErase(Node* aParent, bool aWay)
{
    while (!aParent->m_IsRoot)
    {
        int32_t sMirror = aWay ? 1 : -1; // TODO optimize?
        aParent->m_Balance -= sMirror;

        if (aParent->m_Balance == 0)
        {
            // sParent was not equal-balanced, now it is; its height decreased.
            aWay = aParent->m_Way;
            aParent = aParent->m_Parent;
            continue;
        }

        if (aParent->m_Balance == -sMirror)
        {
            // sParent was equal-balanced, now it's not; its height didn't change.
            return;
        }

        // Rotation is needed
        Node* sNextParent = aParent->m_Parent;
        bool sNextWay = aParent->m_Way;
        assert(aParent->m_Balance * sMirror == -2);
        Node* sSibling = aParent->m_Child[!aWay];
        bool sFinalRotation = sSibling->m_Balance == 0;
        if (sMirror * sSibling->m_Balance > 0)
            rotate(sSibling, !aWay); // Double rotation
        rotate(aParent, aWay);

        if (sFinalRotation)
        {
            // In this case the height of sParent did not change.
            return;
        }

        aParent = sNextParent;
        aWay = sNextWay;
    }
}

void TreeCommon::insert(Node* aParent, Node* aNode, bool aWay, bool aMin, bool aMax)
{
    if (aMin)
    {
        if (nullptr != m_Min)
            m_Min->m_IsMinMax[0] = false;
        m_Min = aNode;
    }
    if (aMax)
    {
        if (nullptr != m_Max)
            m_Max->m_IsMinMax[1] = false;
        m_Max = aNode;
    }
    m_Size++;
    link(aParent, aNode, aWay);
    aNode->m_Child[0] = aNode->m_Child[1] = nullptr;
    aNode->m_Balance = 0;
    aNode->m_Way = aWay;
    aNode->m_IsRoot = false;
    aNode->m_IsMinMax[0] = aMin;
    aNode->m_IsMinMax[1] = aMax;
    rebalanceInsert(aNode);
}

void TreeCommon::erase(Node *aNode)
{
    m_Size--;
    if (0 == m_Size)
    {
        m_Min = m_Max = nullptr;
    }
    else
    {
        if (m_Min == aNode)
        {
            m_Min = nullptr != aNode->m_Child[1] ? aNode->m_Child[1] : aNode->m_Parent;
            m_Min->m_IsMinMax[0] = true;
        }
        if (m_Max == aNode)
        {
            m_Max = nullptr != aNode->m_Child[0] ? aNode->m_Child[0] : aNode->m_Parent;
            m_Max->m_IsMinMax[1] = true;
        }
    }

    if (nullptr == aNode->m_Child[0] && nullptr == aNode->m_Child[1])
    {
        // Leaf. Just unlink it from parent.
        aNode->m_Parent->m_Child[aNode->m_Way] = nullptr;
        rebalanceErase(aNode->m_Parent, aNode->m_Way);
    }
    else
    {
        // Find closest by value node from bigger subtree (sReplacement).
        // Remove sReplacement from the tree and then replace sNode with sReplacement.
        bool sWay = aNode->m_Balance >= 0;
        Node* sReplacement = step(aNode, sWay);
        Node* sTail = sReplacement->m_Child[sWay]; // Is leaf or nullptr
        assert(nullptr == sTail || (nullptr == sTail->m_Child[0] && nullptr == sTail->m_Child[1]));
        linkSafe(sReplacement->m_Parent, sTail, sReplacement->m_Way);
        rebalanceErase(sReplacement->m_Parent, sReplacement->m_Way);
        sReplacement->m_Balance = aNode->m_Balance;
        link(aNode->m_Parent, sReplacement, aNode->m_Way);
        linkSafe(sReplacement, aNode->m_Child[0], false);
        linkSafe(sReplacement, aNode->m_Child[1], true);
    }
}

inline void TreeCommon::replace(Node *aNode, Node* aNewNode)
{
    if (m_Min == aNode)
        m_Min = aNewNode;
    if (m_Max == aNode)
        m_Max = aNewNode;
    *aNewNode = *aNode;
    if (nullptr != aNewNode->m_Child[0])
        aNewNode->m_Child[0]->m_Parent = aNewNode;
    if (nullptr != aNewNode->m_Child[1])
        aNewNode->m_Child[1]->m_Parent = aNewNode;
    aNewNode->m_Parent->m_Child[aNewNode->m_Way] = aNewNode;
}

template <class Item, Node Item::*NodeMember, class Comparator>
std::pair<typename Tree<Item, NodeMember, Comparator>::iterator, bool>
Tree<Item, NodeMember, Comparator>::insert(Item& aItem)
{
    // Search for a parent for the coming leaf node
    Node* sParent = &m_Root;
    bool sWay = false;
    bool sOneWay[2] = {true, true};
    while (nullptr != sParent->m_Child[sWay])
    {
        sParent = sParent->m_Child[sWay];
        int sCmp = Comparator::Compare(aItem, *item(sParent));
        if (0 == sCmp)
            return std::make_pair(iterator(sParent), false);
        sOneWay[sCmp < 0] = false;
        sWay = sCmp > 0;
    }
    Node* sNode = &(aItem.*NodeMember);
    TreeCommon::insert(sParent, sNode, sWay, sOneWay[0], sOneWay[1]);
    return std::make_pair(iterator(sNode), true);
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::erase(Item& aItem)
{
    TreeCommon::erase(&(aItem.*NodeMember));
}

template <class Item, Node Item::*NodeMember, class Comparator>
void Tree<Item, NodeMember, Comparator>::replace(Item& aItem, Item& aNewItem)
{
    TreeCommon::replace(&(aItem.*NodeMember), &(aNewItem.*NodeMember));
}

template <class Item, Node Item::*NodeMember, class Comparator>
const Item* Tree<Item, NodeMember, Comparator>::item(const Node* aNode)
{
    const uintptr_t sOffset = reinterpret_cast<uintptr_t>(&(reinterpret_cast<const Item*>(0)->*NodeMember));
    return reinterpret_cast<const Item*>(reinterpret_cast<const char*>(aNode) - sOffset);
}

template <class Item, Node Item::*NodeMember, class Comparator>
Item* Tree<Item, NodeMember, Comparator>::item(Node* aNode)
{
    return const_cast<Item*>(item(const_cast<const Node*>(aNode)));
}

template <class Item, Node Item::*NodeMember, class Comparator>
const Item* Tree<Item, NodeMember, Comparator>::itemSafe(const Node* aNode)
{
    return nullptr == aNode ? nullptr : item(aNode);
}

template <class Item, Node Item::*NodeMember, class Comparator>
Item* Tree<Item, NodeMember, Comparator>::itemSafe(Node* aNode)
{
    return nullptr == aNode ? nullptr : item(aNode);
}

template <class Item, Node Item::*NodeMember, class Comparator>
template <class Key>
const Node* Tree<Item, NodeMember, Comparator>::lookup(const Key& aKey) const
{
    const Node* sNode = m_Root.m_Child[0];
    while (nullptr != sNode)
    {
        int sCmp = Comparator::Compare(*item(sNode), aKey);
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
int Tree<Item, NodeMember, Comparator>::selfCheck() const
{
    int sHeight;
    size_t sSize;
    int sRes = checkSubTree(m_Root.m_Child[0], sHeight, sSize);
    if (size() != sSize)
        sRes |= 1 << 0;
    const Node *sMin = m_Root.m_Child[0], *sMax = m_Root.m_Child[0];
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
int Tree<Item, NodeMember, Comparator>::checkSubTree(const Node* aNode, int& aHeight, size_t& aSize) const
{
    if (nullptr == aNode)
    {
        aHeight = 0;
        aSize = 0;
        return 0;
    }

    int sRes = 0;
    if ((aNode == m_Min) != aNode->m_IsMinMax[0])
        sRes |= 1 << 3;
    if ((aNode == m_Max) != aNode->m_IsMinMax[1])
        sRes |= 1 << 3;

    if (nullptr != aNode->m_Child[0] && aNode != aNode->m_Child[0]->m_Parent)
        sRes |= 1 << 4;
    if (nullptr != aNode->m_Child[1] && aNode != aNode->m_Child[1]->m_Parent)
        sRes |= 1 << 5;
    if (nullptr != aNode->m_Child[0] && aNode->m_Child[0]->m_Way)
        sRes |= 1 << 6;
    if (nullptr != aNode->m_Child[1] && !aNode->m_Child[1]->m_Way)
        sRes |= 1 << 7;

    if (nullptr != aNode->m_Child[0])
    {
        int sCmp = Comparator::Compare(*item(aNode->m_Child[0]), *item(aNode));
        if (sCmp == 0)
            sRes |= 1 << 8;
        else if (sCmp > 0)
            sRes |= 1 << 9;
    }
    if (nullptr != aNode->m_Child[1])
    {
        int sCmp = Comparator::Compare(*item(aNode), *item(aNode->m_Child[1]));
        if (sCmp == 0)
            sRes |= 1 << 10;
        else if (sCmp > 0)
            sRes |= 1 << 11;
    }

    int sHeight0, sHeight1;
    size_t sSize0, sSize1;
    sRes |= checkSubTree(aNode->m_Child[0], sHeight0, sSize0);
    sRes |= checkSubTree(aNode->m_Child[1], sHeight1, sSize1);
    aHeight = 1 + (sHeight0 > sHeight1 ? sHeight0 : sHeight1);
    aSize = 1 + sSize0 + sSize1;

    if (aNode->m_Balance != sHeight1 - sHeight0)
        sRes |= 1 << 12;
    if (aNode->m_Balance < -1)
    {
        // Left too big
        sRes |= 1 << 13;
    }
    else if (aNode->m_Balance > 1)
    {
        // Right too big
        sRes |= 1 << 14;
    }
    return sRes;
}

}; // namespace Avl
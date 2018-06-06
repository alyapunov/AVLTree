#include <AvlTree.hpp>

#include <chrono>
#include <iostream>
#include <set>

// Helpers
static size_t SideEffect = 0;

static const size_t COUNT = 4 * 1024 * 1024;
static char SimpleBuffer[COUNT * 64];
static size_t SimpleBufferPos = 0;

static void* simpleAlloc(size_t aSize)
{
    assert(SimpleBufferPos + aSize <= sizeof(SimpleBuffer));
    void* sRes = SimpleBuffer + SimpleBufferPos;
    SimpleBufferPos += aSize;
    return sRes;
}

template <class T>
static T* simpleAlloc()
{
    return static_cast<T*>(simpleAlloc(sizeof(T)));
}

template <class T>
struct StdAllocator
{
    typedef T value_type;
    StdAllocator() = default;
    template <class U> constexpr StdAllocator(const StdAllocator<U>&) noexcept {}
    T* allocate(std::size_t n) { assert(n == 1); (void)n; return simpleAlloc<T>(); }
    void deallocate(T*, std::size_t) noexcept { }
};

static size_t simpleReset()
{
    size_t sRes = SimpleBufferPos;
    SimpleBufferPos = 0;
    return sRes;
}

static void checkpoint(const char* aText, size_t aOpCount)
{
    using namespace std::chrono;
    high_resolution_clock::time_point now = high_resolution_clock::now();
    static high_resolution_clock::time_point was;
    duration<double> time_span = duration_cast<duration<double>>(now - was);
    if (0 != aOpCount)
    {
        double Mrps = aOpCount / 1000000. / time_span.count();
        std::cout << aText << ": " << Mrps << " Mrps" << std::endl;
    }
    was = now;
}

// Avl tree with size_t key
struct Test
{
    size_t m_Value;
    Avl::Node m_Node;
    bool operator<(const Test& a) const { return m_Value < a.m_Value; }
    bool operator<(size_t a) const { return m_Value < a; }
    friend bool operator<(size_t a, const Test& b) { return a < b.m_Value; }
};

using Tree_t = Avl::Tree<Test, &Test::m_Node>;

static void alv_test()
{
    Tree_t sTree;
    checkpoint("", 0);

    for (size_t i = 0; i < COUNT; i++)
    {
        Test* t = simpleAlloc<Test>();
        t->m_Value = i;
        sTree.insert(*t);
    }
    checkpoint("AVL insert", COUNT);

    for (size_t i = 0; i < COUNT; i++)
    {
        SideEffect ^= sTree.find(i)->m_Value;
    }
    checkpoint("AVL find", COUNT);

    for (Test& t : sTree)
    {
        SideEffect ^= t.m_Value;
    }
    checkpoint("AVL iteration", COUNT);

    for (size_t i = 0; i < COUNT; i++)
    {
        sTree.erase(*sTree.begin());
    }
    checkpoint("AVL erase", COUNT);

    std::cout << "AVL memory: " << simpleReset() / 1024 << "kB" << std::endl;
}

// Set size_t
using Set_t = std::set<size_t, std::less<size_t>, StdAllocator<size_t>>;

static void set_test()
{
    Set_t sSet;
    checkpoint("", 0);

    for (size_t i = 0; i < COUNT; i++)
    {
        sSet.insert(i);
    }
    checkpoint("Set insert", COUNT);

    for (size_t i = 0; i < COUNT; i++)
    {
        SideEffect ^= *sSet.find(i);
    }
    checkpoint("Set find", COUNT);

    for (size_t i : sSet)
    {
        SideEffect ^= i;
    }
    checkpoint("Set iteration", COUNT);

    for (size_t i = 0; i < COUNT; i++)
    {
        sSet.erase(*sSet.begin());
    }
    checkpoint("Set erase", COUNT);

    std::cout << "Set memory: " << simpleReset() / 1024 << "kB" << std::endl;
}

int main()
{
    alv_test();
    set_test();
    std::cout << "Side effect (ignore it): " << SideEffect << std::endl;
}

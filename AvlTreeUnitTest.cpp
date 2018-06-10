#include <AvlTree.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>

int rc = 0;

void check(bool exp, const char* funcname, const char *filename, int line)
{
    if (!exp)
    {
        rc = 1;
        std::cerr << "Check failed in " << funcname << " at " << filename << ":" << line << std::endl;
    }
}

template<class T>
void check(const T& x, const T& y, const char* funcname, const char *filename, int line)
{
    if (x != y)
    {
        rc = 1;
        std::cerr << "Check failed: " << x << " != " << y <<  " in " << funcname << " at " << filename << ":" << line << std::endl;
    }
}

#define CHECK(...) check(__VA_ARGS__, __func__, __FILE__, __LINE__)

struct Announcer
{
    const char* m_Func;
    explicit Announcer(const char* aFunc) : m_Func(aFunc)
    {
        std::cout << "======================= Test \"" << m_Func << "\" started =======================" << std::endl;
    }
    ~Announcer()
    {
        std::cout << "======================= Test \"" << m_Func << "\" finished ======================" << std::endl;
    }
};

#define ANNOUNCE() Announcer sAnn(__func__)

struct Test
{
    Test(size_t aValue = 0) : m_Value(aValue), m_Data(generateData()) {}

    size_t m_Value;
    size_t m_Data;
    static size_t generateData() { static size_t sData; return sData++; }
    Avl::Node m_Node;
    bool operator<(const Test& a) const { return m_Value < a.m_Value; }
    bool operator<(size_t a) const { return m_Value < a; }
    friend bool operator<(size_t a, const Test& b) { return a < b.m_Value; }
};

using Tree_t = Avl::Tree<Test, &Test::m_Node>;

size_t debugPrintLevel(const Tree_t& aTree, const Test* aNode, size_t aLevel, size_t aWidth)
{
    if (nullptr == aNode)
    {
        std::string sSpaces(aWidth, ' ');
        std::cout << sSpaces;
        return 0;
    }
    else if (0 == aLevel)
    {
        std::string sSpaces(aWidth, ' ');

        size_t sValueDigits = 1;
        size_t sTmp = aNode->m_Value;
        if (sTmp != 0)
        {
            sTmp /= 10;
            while (sTmp != 0)
            {
                sValueDigits++;
                sTmp /= 10;
            }
        }
        if (aTree.isLeftBigger(aNode) || aTree.isRightBigger(aNode))
            ++sValueDigits;
        size_t sSpacesLeft = (aWidth - sValueDigits) / 2;
        size_t sSpacesRight = (aWidth - sValueDigits + 1) / 2;
        if (sValueDigits > aWidth)
        {
            sSpacesLeft = 0;
            sSpacesRight = 1;
        }
        std::cout << sSpaces.data() + aWidth - sSpacesLeft;
        if (aTree.isLeftBigger(aNode))
            std::cout << "*";
        std::cout << aNode->m_Value;
        if (aTree.isRightBigger(aNode))
            std::cout << "*";
        std::cout << sSpaces.data() + aWidth - sSpacesRight;
        return 1;
    }
    else
    {
        size_t sRes = 0;
        sRes += debugPrintLevel(aTree, aTree.getLeft(aNode), aLevel - 1, aWidth / 2);
        sRes += debugPrintLevel(aTree, aTree.getRight(aNode), aLevel - 1, aWidth / 2);
        return sRes;
    }
}

static void debugPrint(const Tree_t& aTree)
{
    const size_t sWidth = 100;
    std::string sSpaces(sWidth, '*');
    std::cout << sSpaces << std::endl;

    for (size_t sLevel = 0; ; sLevel++)
    {
        size_t sCount = debugPrintLevel(aTree, aTree.getRoot(), sLevel, sWidth);
        std::cout << std::endl;
        if (0 == sCount)
            break;
    }
}

const size_t SIMPLE_SIZE = 25;

static void checkLowAccess(const Tree_t& aTree, const Test* aNode, const Test* aBegin, const Test* aEnd)
{
    if (nullptr == aNode)
    {
        CHECK(aBegin == aEnd);
        return;
    }
    CHECK(aBegin != aEnd);
    const Test* aMiddle = std::lower_bound(aBegin, aEnd, *aNode);
    CHECK(aMiddle >= aBegin);
    CHECK(aMiddle < aEnd);
    checkLowAccess(aTree, aTree.getLeft(aNode), aBegin, aMiddle);
    checkLowAccess(aTree, aTree.getRight(aNode), aMiddle + 1, aEnd);
}

static void checkSimple(const Tree_t& aTree, Test* aBegin, Test* aEnd)
{
    size_t sSize = aEnd - aBegin;
    Test sLocal[SIMPLE_SIZE];
    for (size_t i = 0; i < sSize; i++)
        sLocal[i] = aBegin[i];
    std::sort(sLocal, sLocal + sSize);

    CHECK(aTree.size() == sSize);
    if (0 == aTree.size())
    {
        CHECK(aTree.min() == aTree.end());
        CHECK(aTree.max() == aTree.end());
    }
    else {
        CHECK(aTree.min()->m_Value == sLocal[0].m_Value);
        CHECK(aTree.max()->m_Value == sLocal[sSize - 1].m_Value);
    }
    Tree_t::const_iterator sItr = aTree.begin();
    size_t i = 0;
    for (; i < sSize && sItr != aTree.end(); ++i, ++sItr)
        CHECK(sItr->m_Value, sLocal[i].m_Value);
    CHECK(i, sSize);
    CHECK(sItr == aTree.end());

    sItr = aTree.begin();
    Tree_t::const_iterator sItrLast = aTree.end();
    i = 0;
    for (; i < sSize && sItr != aTree.end(); ++i)
    {
        CHECK(sItr->m_Value, sLocal[i].m_Value);
        sItrLast = sItr++;
    }
    CHECK(i, sSize);
    CHECK(sItr == aTree.end());
    CHECK(sItrLast == aTree.max());

    sItr = aTree.max();
    i = sSize;
    for (; i > 0 && sItr != aTree.end(); --i, --sItr)
        CHECK(sItr->m_Value, sLocal[i - 1].m_Value);
    CHECK(i == 0);
    CHECK(sItr == aTree.end());

    sItr = aTree.max();
    sItrLast = aTree.end();
    i = sSize;
    for (; i > 0 && sItr != aTree.end(); --i)
    {
        CHECK(sItr->m_Value, sLocal[i - 1].m_Value);
        sItrLast = sItr--;
    }
    CHECK(i == 0);
    CHECK(sItr == aTree.end());
    CHECK(sItrLast == aTree.min());

    checkLowAccess(aTree, aTree.getRoot(), sLocal, sLocal + sSize);
}

static void simple()
{
    ANNOUNCE();

    {
        std::cout << " *** Inserting from left to right, erasing from left to right *** " << std::endl;
        Test sTest[SIMPLE_SIZE];
        for (size_t i = 0; i < SIMPLE_SIZE; i++)
            sTest[i].m_Value = i + 1;

        Tree_t sTree;
        checkSimple(sTree, sTest, sTest);
        CHECK(sTree.selfCheck(), 0);
        for (size_t i = 0; i < SIMPLE_SIZE; i++)
        {
            std::cout << "Inserting " << sTest[i].m_Value << std::endl;
            auto sRes = sTree.insert(sTest[i]);
            debugPrint(sTree);
            CHECK(sRes.second);
            CHECK(&*sRes.first == &sTest[i]);
            CHECK(sTree.selfCheck(), 0);
            checkSimple(sTree, sTest, sTest + i + 1);
        }

        for (size_t i = 0; i < SIMPLE_SIZE; i++)
        {
            std::cout << "Erasing " << sTest[i].m_Value << std::endl;
            sTree.erase(sTest[i]);
            debugPrint(sTree);
            CHECK(sTree.selfCheck(), 0);
            checkSimple(sTree, sTest + i + 1, sTest + SIMPLE_SIZE);
        }

        for (size_t i = 0; i < SIMPLE_SIZE; i++)
            CHECK(sTest[i].m_Value == i + 1);
    }

    {
        std::cout << " *** Inserting from right to left, erasing from right to left *** " << std::endl;
        Test sTest[SIMPLE_SIZE];
        for (size_t i = 0; i < SIMPLE_SIZE; i++)
            sTest[i].m_Value = SIMPLE_SIZE - i;

        Tree_t sTree;
        checkSimple(sTree, sTest, sTest);
        CHECK(sTree.selfCheck(), 0);
        for (size_t i = 0; i < SIMPLE_SIZE; i++)
        {
            std::cout << "Inserting " << sTest[i].m_Value << std::endl;
            auto sRes = sTree.insert(sTest[i]);
            debugPrint(sTree);
            CHECK(sRes.second);
            CHECK(&*sRes.first == &sTest[i]);
            CHECK(sTree.selfCheck(), 0);
            checkSimple(sTree, sTest, sTest + i + 1);
        }

        for (size_t i = 0; i < SIMPLE_SIZE; i++)
        {
            std::cout << "Erasing " << sTest[i].m_Value << std::endl;
            sTree.erase(sTest[i]);
            debugPrint(sTree);
            CHECK(sTree.selfCheck(), 0);
            checkSimple(sTree, sTest + i + 1, sTest + SIMPLE_SIZE);
        }

        for (size_t i = 0; i < SIMPLE_SIZE; i++)
            CHECK(sTest[i].m_Value == SIMPLE_SIZE - i);
    }

    {
        std::cout << " *** Inserting into center, erasing from marges *** " << std::endl;
        Test sTest[SIMPLE_SIZE];
        for (size_t i = 0; i < SIMPLE_SIZE; i++)
            sTest[i].m_Value = (i % 2 == 0) ? i / 2 + 1 : SIMPLE_SIZE - i / 2;

        Tree_t sTree;
        CHECK(sTree.selfCheck(), 0);
        for (size_t i = 0; i < SIMPLE_SIZE; i++)
        {
            std::cout << "Inserting " << sTest[i].m_Value << std::endl;
            auto sRes = sTree.insert(sTest[i]);
            debugPrint(sTree);
            CHECK(sRes.second);
            CHECK(&*sRes.first == &sTest[i]);
            CHECK(sTree.selfCheck(), 0);
            checkSimple(sTree, sTest, sTest + i + 1);
        }

        for (size_t i = 0; i < SIMPLE_SIZE; i++)
        {
            std::cout << "Erasing " << sTest[i].m_Value << std::endl;
            sTree.erase(sTest[i]);
            debugPrint(sTree);
            CHECK(sTree.selfCheck(), 0);
            checkSimple(sTree, sTest + i + 1, sTest + SIMPLE_SIZE);
        }

        for (size_t i = 0; i < SIMPLE_SIZE; i++)
            CHECK(sTest[i].m_Value == ((i % 2 == 0) ? i / 2 + 1 : SIMPLE_SIZE - i / 2));
    }

    {
        std::cout << " *** Inserting into center, erasing from center *** " << std::endl;
        Test sTest[SIMPLE_SIZE];
        for (size_t i = 0; i < SIMPLE_SIZE; i++)
            sTest[i].m_Value = (i % 2 == 0) ? i / 2 + 1 : SIMPLE_SIZE - i / 2;

        Tree_t sTree;
        CHECK(sTree.selfCheck(), 0);
        for (size_t i = 0; i < SIMPLE_SIZE; i++)
        {
            std::cout << "Inserting " << sTest[i].m_Value << std::endl;
            auto sRes = sTree.insert(sTest[i]);
            debugPrint(sTree);
            CHECK(sRes.second);
            CHECK(&*sRes.first == &sTest[i]);
            CHECK(sTree.selfCheck(), 0);
            checkSimple(sTree, sTest, sTest + i + 1);
        }

        for (size_t j = 0; j < SIMPLE_SIZE; j++)
        {
            size_t i = SIMPLE_SIZE - 1 - j;
            std::cout << "Erasing " << sTest[i].m_Value << std::endl;
            sTree.erase(sTest[i]);
            debugPrint(sTree);
            CHECK(sTree.selfCheck(), 0);
            checkSimple(sTree, sTest, sTest + i);
        }

        for (size_t i = 0; i < SIMPLE_SIZE; i++)
            CHECK(sTest[i].m_Value == ((i % 2 == 0) ? i / 2 + 1 : SIMPLE_SIZE - i / 2));
    }
}

static void massive()
{
    ANNOUNCE();

    const size_t SIZE_LIMIT = 128;
    const size_t ITERATIONS = 4 * 1024 * 1024;

    Tree_t sTree;
    std::set<size_t> sRef;

    for (size_t i = 0; i < ITERATIONS; i++)
    {
        size_t r = rand() % SIZE_LIMIT;
        //std::cout << "Roll " << r << std::endl;

        Tree_t::iterator sTreeItr = sTree.find(r);
        std::set<size_t>::iterator sRefItr = sRef.find(r);
        CHECK(sTreeItr == sTree.end(), sRefItr == sRef.end());

        bool sFound = sTreeItr != sTree.end();
        if (sFound)
        {
            if (rand() % 16 == 0)
            {
                //std::cout << "Replacing " << r << std::endl;
                Test* sNew = new Test(r);
                std::pair<Tree_t::iterator, bool> sInsRes = sTree.insert(*sNew);
                CHECK(sInsRes.first == sTreeItr);
                CHECK(sInsRes.second, false);
                sTree.replace(*sTreeItr, *sNew);
                sTreeItr->m_Value = SIZE_MAX;
                delete &*sTreeItr;
                //debugPrint(sTree);
            }
            else
            {
                //std::cout << "Erasing " << r << std::endl;
                sTree.erase(*sTreeItr);
                delete &*sTreeItr;
                sRef.erase(sRefItr);
                //debugPrint(sTree);
            }
        }
        else
        {
            if (rand() % 16 == 0)
            {
                //std::cout << "Inserting " << r << std::endl;
                Test* sNew = new Test(r);
                std::pair<Tree_t::iterator, bool> sInsRes = sTree.insert(*sNew);
                CHECK(sInsRes.first->m_Value, r);
                CHECK(sInsRes.second, true);
                sRef.insert(r);
                //debugPrint(sTree);
            }
            else
            {
                //std::cout << "Skipping " << r << std::endl;
                continue;
            }
        }

        CHECK(sTree.selfCheck(), 0);
        CHECK(sTree.size(), sRef.size());
        if (0 == sTree.size())
        {
            CHECK(sTree.min() == sTree.end());
            CHECK(sTree.max() == sTree.end());
        }
        else
        {
            CHECK(sTree.min()->m_Value, *sRef.begin());
            CHECK(sTree.max()->m_Value, *sRef.rbegin());
        }
    }
}

int main()
{
    simple();
    massive();

    std::cout << (0 == rc ? "Success" : "Finished with errors") << std::endl;
    return rc;
}
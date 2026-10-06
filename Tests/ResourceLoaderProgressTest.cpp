#include "doctest/doctest.h"

#include "resource/detail/LoaderCore.h"

using gameres::detail::computeEntryProgress;
using gameres::detail::computeWeightedProgress;
using gameres::detail::Entry;

namespace
{
// computeEntryProgress/computeWeightedProgress 只读取 finished/weight/children，不会访问 resource。
Entry makeLeaf(bool finished, float weight = 1.0f)
{
    Entry e;
    e.finished = finished;
    e.weight   = weight;
    return e;
}
}  // namespace

TEST_SUITE("ResourceLoader.Progress")
{
    TEST_CASE("leaf entry is 0 until finished, then 1")
    {
        CHECK_EQ(computeEntryProgress(makeLeaf(false)), doctest::Approx(0.0f));
        CHECK_EQ(computeEntryProgress(makeLeaf(true)), doctest::Approx(1.0f));
    }

    TEST_CASE("entry with children averages child progress")
    {
        Entry child1 = makeLeaf(true);
        Entry child2 = makeLeaf(false);
        Entry child3 = makeLeaf(true);
        Entry child4 = makeLeaf(false);

        Entry parent;
        parent.children = {&child1, &child2, &child3, &child4};
        CHECK_EQ(computeEntryProgress(parent), doctest::Approx(0.5f));
    }

    TEST_CASE("nested children are aggregated recursively")
    {
        Entry grandchild1 = makeLeaf(true);
        Entry grandchild2 = makeLeaf(false);

        Entry child1;
        child1.children = {&grandchild1, &grandchild2};  // 0.5

        Entry child2 = makeLeaf(true);  // 1.0

        Entry root;
        root.children = {&child1, &child2};  // (0.5 + 1.0) / 2
        CHECK_EQ(computeEntryProgress(root), doctest::Approx(0.75f));
    }

    TEST_CASE("weighted progress across multiple roots")
    {
        Entry light = makeLeaf(true, 1.0f);
        Entry heavy = makeLeaf(false, 3.0f);

        std::vector<Entry*> roots{&light, &heavy};
        // 重任务权重更大：轻任务完成只占 1/4
        CHECK_EQ(computeWeightedProgress(roots), doctest::Approx(0.25f));
    }

    TEST_CASE("composite root contributes partial progress by its weight")
    {
        Entry child1 = makeLeaf(true);
        Entry child2 = makeLeaf(false);

        Entry composite;
        composite.weight   = 2.0f;
        composite.children = {&child1, &child2};  // 0.5

        Entry leaf = makeLeaf(false, 2.0f);

        std::vector<Entry*> roots{&composite, &leaf};
        // (2 * 0.5 + 2 * 0) / 4
        CHECK_EQ(computeWeightedProgress(roots), doctest::Approx(0.25f));
    }

    TEST_CASE("zero total weight falls back to finished-count ratio")
    {
        Entry a = makeLeaf(true, 0.0f);
        Entry b = makeLeaf(false, 0.0f);

        std::vector<Entry*> roots{&a, &b};
        CHECK_EQ(computeWeightedProgress(roots), doctest::Approx(0.5f));
    }

    TEST_CASE("no roots means fully finished")
    {
        std::vector<Entry*> roots;
        CHECK_EQ(computeWeightedProgress(roots), doctest::Approx(1.0f));
    }
}

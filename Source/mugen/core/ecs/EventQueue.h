#pragma once

#include "../reflect/Reflect.h"

#include <vector>

namespace mugen
{
// 按入队顺序保存一种事件的值队列；作为单例登记并进入快照。
template <class T>
struct EventQueue
{
    // 事件按入队顺序排列，入队顺序就是处理顺序。
    std::vector<T> items;

    // 追加一个事件。
    void push(T value) { items.push_back(std::move(value)); }
    // 清空全部事件。
    void clear() { items.clear(); }
    // 返回事件数量。
    std::size_t size() const { return items.size(); }
    // 队列是否为空。
    bool empty() const { return items.empty(); }

    MG_REFLECT(EventQueue, items)
};
}  // namespace mugen

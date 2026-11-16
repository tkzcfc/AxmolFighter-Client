#pragma once

#include <string>

#include "FairyGUI.h"

namespace gameui
{
using namespace fairygui;

// FairyGUI 节点查找。按名称深度优先遍历子树，命中后尝试转成 T。
// 同名节点类型不符时继续往下找（包括该节点的子孙，以及 GLoader 装载的组件），全部找不到返回 nullptr。
class FGUIUtils
{
public:
    template <typename T>
    static T* findChild(GComponent* component, const std::string& name)
    {
        return findInChildren<T>(component, name);
    }

private:
    template <typename T>
    static T* findInChildren(GComponent* component, const std::string& name)
    {
        for (GObject* child : component->getChildren())
        {
            if (T* found = matchOrDescend<T>(child, name))
                return found;
        }
        return nullptr;
    }

    template <typename T>
    static T* matchOrDescend(GObject* object, const std::string& name)
    {
        if (object->name == name)
        {
            if (T* typed = object->as<T>())
                return typed;
        }

        if (GComponent* nested = object->as<GComponent>())
            return findInChildren<T>(nested, name);

        if (GLoader* loader = object->as<GLoader>())
        {
            if (GComponent* content = loader->getComponent())
                return matchOrDescend<T>(content, name);
        }

        return nullptr;
    }
};

}  // namespace gameui

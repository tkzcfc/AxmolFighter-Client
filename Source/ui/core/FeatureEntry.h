#pragma once

#include "ui/core/UIWidget.h"

#include <any>
#include <memory>

namespace gameui
{

// Feature identifiers opened from town menus and other global entry points.
enum class FeatureId
{
    Dress,    // 装扮
    Weapon,   // 武装
    Equip,    // 装备
    Partner,  // 伙伴
    Skill,    // 技能
};

// Global feature open API.
//
// Sync vs async: open() is synchronous. FGUIPackageManager::load and UIManager widget
// creation are already sync on the main thread; there is no package IO wait or network
// gate before the widget exists. Callers that need a post-open hook can use the returned
// weak_ptr (or the widget's onShow). Do not add an async overload unless package loading
// or open must wait on IO/network.
class FeatureEntry
{
public:
    // Opens the Screen/Panel/Dialog for `id`.
    // `params` carries any constructor payload; open() any_casts it per feature.
    // Dress accepts FashionOpenParams or mugen::FashionPosition (opens dress-up for that part).
    // Returns the opened UIWidget (as weak_ptr, matching UIManager::open), or empty if
    // the feature is not implemented yet / open was refused.
    static std::weak_ptr<UIWidget> open(FeatureId id, std::any params = {});
};

}  // namespace gameui

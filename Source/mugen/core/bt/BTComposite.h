#pragma once

#include "mugen/core/bt/BTCondition.h"
#include "mugen/core/bt/BTNode.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class BTComposite : public BTNode
{
public:
    typedef BTNode Super;

    BTComposite() {}
    virtual ~BTComposite() {}

    void addCondition(std::unique_ptr<BTCondition> cond);
    void addChild(std::unique_ptr<BTNode> child);
    void clearChildren();
    size_t childCount() const { return children.size(); }

    bool checkAll(BTContext& ctx) override;

    /** 在 BehaviorTreeComponent.selectorMemory 中的槽位（建树时分配） */
    int32_t memorySlot = -1;

protected:
    void enterConditions(BTContext& ctx);
    void exitConditions(BTContext& ctx);

    std::vector<std::unique_ptr<BTCondition>> conditions;
    std::vector<std::unique_ptr<BTNode>> children;
    bool conditionsEntered = false;
};

NS_MG_END

#pragma once

#include "mugen/core/bt/BTCondition.h"
#include "mugen/core/bt/BTNode.h"

#include <vector>

NS_MG_BEGIN

class BTComposite : public BTNode
{
public:
    typedef BTNode Super;

public:
    BTComposite();

    virtual ~BTComposite();

    BTComposite(const BTComposite&)            = delete;
    BTComposite& operator=(const BTComposite&) = delete;

    void addCondition(BTCondition* cond);

    void addChild(BTNode* child);

    void clearConditions();

    void clearChildren();

    BTNode* getChild(int32_t childIndex) { return m_children[childIndex]; }

    size_t childCount() const { return m_children.size(); }

    bool check(BTContext& ctx);

protected:
    void onExit(BTContext& ctx) override;

    bool conditionsHold(BTContext& ctx);

    MG_SYNTHESIZE_READONLY_BY_REF(std::vector<BTCondition*>, m_conditions, Conditions)
    MG_SYNTHESIZE_READONLY_BY_REF(std::vector<BTNode*>, m_children, Children)

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END

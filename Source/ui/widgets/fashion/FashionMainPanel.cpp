#include "FashionMainPanel.h"

#include "AppContext.h"
#include "FashionDressPanel.h"

#include "ui/utils/UiQualityRes.h"
#include "mugen/avatar/render/Avatar.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/ReflectedConfig.h"
#include "fairygui/GLoader3D.h"

namespace gameui
{

std::weak_ptr<FashionMainPanel> FashionMainPanel::s_active;

namespace
{
constexpr std::array<std::pair<const char*, mugen::FashionPosition>, 7> kPartSlots = {{
    {"fasionItem_Wing", mugen::FashionPosition::kWing},
    {"fasionItem_Clothes", mugen::FashionPosition::kClothes},
    {"fasionItem_Hat", mugen::FashionPosition::kHat},
    {"fasionItem_Hair", mugen::FashionPosition::kHair},
    {"fasionItem_Face", mugen::FashionPosition::kFace},
    {"fasionItem_kHalo", mugen::FashionPosition::kHalo},
    {"fasionItem_Phantom", mugen::FashionPosition::kPhantom},
}};
}  // namespace

GComponent* FashionMainPanel::container() const
{
    return getChild<GLoader>("container")->getComponent();
}

void FashionMainPanel::requestRefresh()
{
    if (auto self = s_active.lock())
    {
        self->refreshSlots();
        self->refreshAvatar();
    }
}

void FashionMainPanel::onCreate()
{
    s_active = std::static_pointer_cast<FashionMainPanel>(shared_from_this());
    addClickListener(getChild<GButton>("btnBack"), AX_CALLBACK_1(FashionMainPanel::onClickBack, this));

    auto* root = container();
    m_avatarLoader = root->getChild("loaderAvatar")->as<GLoader3D>();

    for (const auto& [childName, position] : kPartSlots)
    {
        auto* button = root->getChild(childName)->as<GButton>();
        addClickListener(button, [this, position](EventContext* ctx) { onClickPart(ctx, position); });
    }

    if (auto* walk = root->getChild("btnToWalk"))
        addClickListener(walk->as<GButton>(), AX_CALLBACK_1(FashionMainPanel::onClickWalk, this));
}

void FashionMainPanel::onShow()
{
    refreshSlots();
    refreshAvatar();
}

void FashionMainPanel::onDestroy()
{
    if (s_active.lock().get() == this)
        s_active.reset();
    if (m_avatarLoader)
        m_avatarLoader->setContent(nullptr);
    m_avatarLoader = nullptr;
    m_avatar       = nullptr;
}

void FashionMainPanel::onClickBack(EventContext*)
{
    close();
}

void FashionMainPanel::onClickPart(EventContext*, mugen::FashionPosition position)
{
    getUIManager()->open<FashionDressPanel>(std::optional<mugen::FashionPosition>{position});
}

void FashionMainPanel::onClickWalk(EventContext*)
{
    m_walkPose = !m_walkPose;
    refreshAvatar();
}

void FashionMainPanel::refreshSlots()
{
    auto* session = AppContext::get().gameSession();
    auto* root    = container();
    for (const auto& [childName, position] : kPartSlots)
    {
        auto* button = root->getChild(childName)->as<GButton>();
        auto* icon   = button->getChild("icon")->as<GLoader>();
        auto* c_has_icon = button->getController("c_has_icon");

        int32_t fashionId = 0;
        if (session)
        {
            for (const auto& f : session->selectedCharacter.fashions)
            {
                if (f.worn && f.position == static_cast<int32_t>(position) && f.configID > 0)
                {
                    fashionId = f.configID;
                    break;
                }
            }
        }

        if (fashionId > 0)
        {
            c_has_icon->setSelectedIndex(0);
            // Icon path may be absent in Content; leave empty when load fails.
            const auto* cfg = mugen::conf::Config::instance().find<mugen::conf::FashionConfig>(
                {fashionId, session ? session->selectedCharacter.classID : 0});
            if (!cfg)
                cfg = mugen::conf::Config::instance().find<mugen::conf::FashionConfig>({fashionId, 0});
            if (cfg && !cfg->icon.empty())
                icon->setURL(cfg->icon);
            else
                icon->setURL("");
            if (auto* quality = button->getChild("loaderQuality"))
            {
                const int32_t q = cfg ? cfg->quality : 0;
                quality->as<GLoader>()->setURL(
                    UiQualityRes::backgroundUrl(UiQualityRes::PlaceScenario::Fashion, q));
            }
        }
        else
        {
            c_has_icon->setSelectedIndex(1);
        }
    }
}

void FashionMainPanel::refreshAvatar()
{
    auto* session = AppContext::get().gameSession();
    if (!session || !m_avatarLoader)
        return;

    mugen::FashionAppearance appearance;
    appearance.roleId = mugen::type_conversions::toRoleConfigId(
        static_cast<mugen::CharacterClass>(session->selectedCharacter.classID));
    for (const auto& skin : session->selectedCharacter.defaultSkins)
        appearance.baseFashion[static_cast<mugen::FashionPosition>(skin.position)] = skin.resFashionId;
    for (const auto& f : session->selectedCharacter.fashions)
    {
        if (f.worn && f.configID > 0)
            appearance.equipFashion[static_cast<mugen::FashionPosition>(f.position)] = f.configID;
    }

    if (m_avatar)
    {
        if (m_avatar->setAppearance(appearance))
        {
            m_avatar->setMotion(m_walkPose ? "walk" : "stand", "", true);
            return;
        }
        m_avatarLoader->setContent(nullptr);
        m_avatar = nullptr;
    }

    auto* avatar = mugen::Avatar::create(appearance);
    if (!avatar)
    {
        AXLOGW("FashionMainPanel: avatar create failed");
        return;
    }
    avatar->setMotion(m_walkPose ? "walk" : "stand", "", true);
    avatar->setAutoPlay(true);
    avatar->setPosition(m_avatarLoader->getWidth() * 0.5f, -m_avatarLoader->getHeight());
    m_avatarLoader->setContent(avatar);
    m_avatar = avatar;
}

}  // namespace gameui

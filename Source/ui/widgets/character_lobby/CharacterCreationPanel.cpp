#include "CharacterCreationPanel.h"
#include "ui/views/CharacterLobbyView.h"
#include "ui/widgets/common/MessageDialog.h"
#include "mugen/render/SpineSkeletonLoader.h"
#include "mugen/conf/GameDef.h"
#include "ui/core/AudioManager.h"
#include <net/client_game.pb.h>
#include "CharacterCreationChooseClouthPanel.h"

namespace gameui
{

constexpr int kProfessionCount = 3;

// 角色spine动画偏移,缩放以及动画信息配置
struct CharacterCreationSpineConfig
{
    // spine模型资源id
    int32_t id;
    // spine模型缩放系数
    ax::Vec2 scale;
    // spine模型偏移位置
    ax::Vec2 offset;
    // spine模型默认播放动画名称
    std::string animationName;
};

// 角色对应的职业配置
struct CharacterCreationProfession
{
    // 职业名称
    std::string name;
    // 难度系数,范围[1, 5]
    int difficulty;
    // 职业描述
    std::string description;
    // 职业名称图片
    std::string icon;
    // 职业模型资源路径
    CharacterCreationSpineConfig spineConfig;
    // 视频路径
    std::string video;
};

// 角色创建配置
struct CharacterCreationRole
{
    CharacterCreationProfession professions[kProfessionCount];
    // 属性描述图片
    std::string attributeIcon;
};

struct CharacterCreationConfig
{
    CharacterCreationRole roles[mugen::CharacterClass::kCount];
};

static const CharacterCreationConfig kCharacterCreationConfig = {
    {  // roles array
     { // roles[0]
      {// professions
       {"神月",
        2,
        "使用长剑为武器，可以操控温度极高的蓝色火焰，擅长进行近战冲锋和爆发。",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_shenyue",
        {10012, {1.0f, 1.0f}, {-60.0f, 20.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_shenyue.mp4"},
       {"苍炎骑士",
        4,
        "火力全开的蓝焰剑士，将高温的火焰具形化，火焰护体，龙焰之盾，成为战场上的防御核心",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_cangyanqishi",
        {10308, {1.1f, 1.1f}, {50.0f, -85.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_cangyanqishi.mp4"},
       {"次元领主",
        3,
        "掌握时空的双刃剑士，额外使用次元之刃，斩破时空，穿梭次元，是为战场上的的绝对王牌",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_ciyuanlingzhu",
        {10309, {1.0f, 1.0f}, {20.0f, -40.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_ciyuanlingzhu.mp4"}},
      "ui://CharacterLobby/ui_juesechuangjian_juexingfangxiang_jindutiao_shuxing"},
     { // roles[1]
      {// professions
       {"LING",
        3,
        "使用刺刃为武器，可以进行能量具现的暗器攻击，擅长隐身刺杀和造物穿刺。",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_ling",
        {10063, {1.0f, 1.0f}, {-20.0f, 25.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_ling.mp4"},
       {"逐影之心",
        5,
        "恪守使命的致命兵器，领悟影的含义，流血中毒，减速禁锢，化身折磨对手的暗影毒刺",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_zhuyingzhixin",
        {10310, {1.0f, 1.0f}, {50.0f, 0.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_zhuyingzhixin.mp4"},
       {"神兵之眼",
        2,
        "火力全开的最终堡垒，增强具现化的异能，圣矛穿刺，飞刃旋舞，成为横扫千军的毁灭重炮",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_shenbingzhiyan",
        {10311, {1.0f, 1.0f}, {20.0f, 0.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_shenbingzhiyan.mp4"}},
      "ui://CharacterLobby/ui_juesechuangjian_juexingfangxiang_jindutiao_shuxing_02"},
     { // roles[2]
      {// professions
       {"未开放",
        3,
        "使用刺刃为武器，可以进行能量具现的暗器攻击，擅长隐身刺杀和造物穿刺。",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_ling",
        {10063, {1.0f, 1.0f}, {-20.0f, 25.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_ling.mp4"},
       {"逐影之心",
        5,
        "恪守使命的致命兵器，领悟影的含义，流血中毒，减速禁锢，化身折磨对手的暗影毒刺",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_zhuyingzhixin",
        {10310, {1.0f, 1.0f}, {50.0f, 0.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_zhuyingzhixin.mp4"},
       {"神兵之眼",
        2,
        "火力全开的最终堡垒，增强具现化的异能，圣矛穿刺，飞刃旋舞，成为横扫千军的毁灭重炮",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_shenbingzhiyan",
        {10311, {1.0f, 1.0f}, {20.0f, 0.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_shenbingzhiyan.mp4"}},
      "ui://CharacterLobby/ui_juesechuangjian_juexingfangxiang_jindutiao_shuxing_02"},
     
     { // roles[3]
      {// professions
       {"莉莉姆",
        1,
        "使用魔杖为武器，可以进行召唤虚空，雷光之力的异能攻击，擅长能量控制与大范围魔法",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_lilimu",
        {10360, {1.0f, 1.0f}, {-20.0f, -475.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_lilimu.mp4"},
       {"虚空魔女",
        4,
        "掌控暗黑虚空能量的术士，召唤怨灵与魔兽攻击敌人，是敌人眼中最恐怖的存在",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_xukongmonv",
        {10361, {1.0f, 1.0f}, {30.0f, -400.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_xukongmonv.mp4"},
       {"雷光神使",
        1,
        "驱使雷电的元素使者，以雷光之力惩戒对手，召唤雷电精灵，伴随身边作战",
        "ui://CharacterLobby/ui_juesechuangjian_zhujiemian_mingzi_leiguangshenshi",
        {10362, {1.0f, 1.0f}, {7.0f, -350.0f}, "0"},
        "video/video_player/ui_zhiyejieshao_leiguangshenshi.mp4"}},
      "ui://CharacterLobby/ui_juesechuangjian_juexingfangxiang_jindutiao_shuxing_03"}}};

void CharacterCreationPanel::onCreate()
{
    AudioManager::getInstance()->pauseBGM();

    auto backButton = this->getChild<GButton>("backButton");
    this->addClickListener(backButton, AX_CALLBACK_1(CharacterCreationPanel::onClickBackButton, this));

    auto container = this->getChild<GComponent>("container");

    // 职业选择列表
    m_roleTypeSelector = container->getChild("roleTypeSelector")->as<GComponent>();
    for (int32_t i = 0; i < 4; ++i)
    {
        auto btn = m_roleTypeSelector->getChild("n" + std::to_string(i))->as<GButton>();
        this->addClickListener(btn, [this, i](EventContext* context) { this->onClickRoleTypeButton(context, i); });
    }

    for (int32_t i = 0; i < kProfessionCount; ++i)
    {
        auto professionButton = container->getChild("professionButton" + std::to_string(i))->as<GButton>();
        this->addClickListener(professionButton,
                               [this, i](EventContext* context) { this->onClickProfessionButton(context, i); });
    }

    // 创建角色按钮
    auto createButton = container->getChild("createButton")->as<GButton>();
    this->addClickListener(createButton, AX_CALLBACK_1(CharacterCreationPanel::onClickCreateButton, this));

    this->updateUI();
}

void CharacterCreationPanel::onDestroy()
{
    AudioManager::getInstance()->resumeBGM();
}

void CharacterCreationPanel::updateUI()
{
    for (int32_t i = 0; i < mugen::CharacterClass::kCount; ++i)
    {
        auto btn = m_roleTypeSelector->getChild("n" + std::to_string(i))->as<GButton>();
        btn->setTouchable(i != m_curRoleTypeIndex);
        btn->setSelected(i == m_curRoleTypeIndex);
    }
    auto& curRoleConfig       = kCharacterCreationConfig.roles[m_curRoleTypeIndex];
    auto& curProfessionConfig = curRoleConfig.professions[m_curProfessionIndex];
    auto container            = this->getChild<GComponent>("container");

    for (int32_t i = 0; i < 3; ++i)
    {
        auto professionButton = container->getChild("professionButton" + std::to_string(i))->as<GButton>();
        professionButton->setTouchable(i != m_curProfessionIndex);
        professionButton->setSelected(i == m_curProfessionIndex);
        professionButton->setText(curRoleConfig.professions[i].name);
    }

    // 属性描述图片
    auto loaderAttr = container->getChild("loaderAttr")->as<GLoader>();
    loaderAttr->setURL(curRoleConfig.attributeIcon);

    // 职业名称
    auto loaderName = container->getChild("loaderName")->as<GLoader>();
    loaderName->setURL(curProfessionConfig.icon);

    // 职业描述
    auto textDesc = container->getChild("textDesc")->as<GTextField>();
    textDesc->setText(curProfessionConfig.description);

    // 操作难度
    auto cStar = container->getController("c_star");
    cStar->setSelectedIndex(curProfessionConfig.difficulty);

    // 职业模型
    auto loaderAvatar = container->getChild("loaderAvatar")->as<GLoader3D>();

    auto skeletonAnimation = mugen::SpineSkeletonLoader::createSkeletonAnimation(curProfessionConfig.spineConfig.id);
    skeletonAnimation->setPosition(ax::Vec2(loaderAvatar->getWidth() * 0.5f, -loaderAvatar->getHeight()) +
                                   curProfessionConfig.spineConfig.offset);
    skeletonAnimation->setScale(curProfessionConfig.spineConfig.scale.x, curProfessionConfig.spineConfig.scale.y);
    // 有出现动画
    if (m_curProfessionIndex == 0)
    {
        std::string animationName = curProfessionConfig.spineConfig.animationName;

        skeletonAnimation->setAnimation(0, "chuxian", false);
        skeletonAnimation->setCompleteListener([skeletonAnimation, animationName](spine::TrackEntry* entry) {
            if (entry->getAnimation()->getName() == "chuxian")
            {
                skeletonAnimation->setAnimation(0, animationName, true);
                skeletonAnimation->setCompleteListener(nullptr);
            }
        });
    }
    else
    {
        skeletonAnimation->setAnimation(0, curProfessionConfig.spineConfig.animationName, true);
    }

    loaderAvatar->setContent(skeletonAnimation);

    // 视频
    if (m_mediaPlayer == nullptr)
    {
        auto loaderVedio = container->getChild("loaderVedio")->as<GLoader3D>();
        m_mediaPlayer    = ax::ui::MediaPlayer::create();
        m_mediaPlayer->setContentSize(loaderVedio->getSize());
        m_mediaPlayer->setPosition(ax::Vec2(loaderVedio->getWidth() * 0.5f, -loaderVedio->getHeight() * 0.5f));
        loaderVedio->setContent(m_mediaPlayer);
    }
    m_mediaPlayer->setURL(curProfessionConfig.video);
    m_mediaPlayer->play();
}

void CharacterCreationPanel::onClickCreateButton(EventContext* context)
{
    getUIManager()->open<CharacterCreationChooseClouthPanel>(static_cast<mugen::CharacterClass>(m_curRoleTypeIndex + 1));
    this->close();
}

void CharacterCreationPanel::onClickBackButton(EventContext* context)
{
    this->close();
}

void CharacterCreationPanel::onClickRoleTypeButton(EventContext* context, int32_t index)
{
    if (index >= mugen::CharacterClass::kCount || (index + 1) == mugen::CharacterClass::kFighter)
    {
        MessageDialog::show("敬请期待");
        return;
    }

    if (m_curRoleTypeIndex == index)
        return;

    m_curRoleTypeIndex   = index;
    m_curProfessionIndex = 0;
    updateUI();
}

void CharacterCreationPanel::onClickProfessionButton(EventContext* context, int32_t index)
{
    if (m_curProfessionIndex == index)
        return;
    m_curProfessionIndex = index;
    updateUI();
}

}  // namespace gameui

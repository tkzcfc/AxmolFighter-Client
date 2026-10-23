#include "WeaponSkillPreviewDialog.h"

#include "ui/widgets/common/MessageDialog.h"

namespace gameui
{

namespace
{
constexpr const char* kPlaceholderVideo = "video/ui_jinengyulan_jingqingqidai.mp4";
}

WeaponSkillPreviewDialog::WeaponSkillPreviewDialog(std::string videoPath) : m_videoPath(std::move(videoPath))
{
    m_options.hasBackground     = true;
    m_options.backgroundOpacity = 180;
    m_options.closeOnClickBg    = true;
    m_playAnimation             = false;
}

GComponent* WeaponSkillPreviewDialog::onCreateContent()
{
    // Same centering as form dialog: no pivot-as-anchor. CharacterCreationPanel hosts
    // MediaPlayer under a stable fullscreen Panel; pivot transforms break native AV layers.
    auto* comp = UIPackage::createObject("Panel_Weapon", "WeaponSkillPreviewDialog")->as<GComponent>();
    const float x = (m_root->getWidth() - comp->getWidth()) * 0.5f;
    const float y = (m_root->getHeight() - comp->getHeight()) * 0.5f;
    comp->setPosition(x, y);
    comp->addRelation(m_root, RelationType::Center_Center);
    return comp;
}

void WeaponSkillPreviewDialog::onCreate()
{
    addClickListener(getChild<GButton>("btnClose"), AX_CALLBACK_1(WeaponSkillPreviewDialog::onClickClose, this));
    m_loaderVideo = getChild<GLoader3D>("loaderVideo");
}

void WeaponSkillPreviewDialog::onShow()
{
    playVideo();
}

void WeaponSkillPreviewDialog::playVideo()
{
    if (!m_loaderVideo)
    {
        AXLOGE("WeaponSkillPreviewDialog: loaderVideo missing");
        return;
    }

    auto* fu = ax::FileUtils::getInstance();
    std::string path = m_videoPath;
    if (path.empty() || !fu->isFileExist(path))
    {
        AXLOGW("WeaponSkillPreviewDialog: video missing '{}', fallback to placeholder", path);
        path = kPlaceholderVideo;
    }

    if (!fu->isFileExist(path))
    {
        AXLOGE("WeaponSkillPreviewDialog: placeholder video also missing");
        MessageDialog::show("技能预览视频缺失");
        return;
    }

    // Recreate each show (heiyue CREATEVIDEO style) so Mac AVPlayer does not keep a dead URL.
    if (m_mediaPlayer)
    {
        m_loaderVideo->setContent(nullptr);
        m_mediaPlayer = nullptr;
    }

    // Mirror CharacterCreationPanel::updateUI video block exactly:
    // MediaPlayer into FairyGUI GLoader3D, size/pos from loader, file:// full path, loop+play.
    m_mediaPlayer = ax::ui::MediaPlayer::create();
    m_mediaPlayer->setContentSize(m_loaderVideo->getSize());
    m_mediaPlayer->setPosition(ax::Vec2(m_loaderVideo->getWidth() * 0.5f, -m_loaderVideo->getHeight() * 0.5f));
    m_mediaPlayer->setLooping(true);
    m_mediaPlayer->setUserInputEnabled(false);
    m_loaderVideo->setContent(m_mediaPlayer);

    const std::string full = fu->fullPathForFilename(path);
    AXLOGI("WeaponSkillPreviewDialog: play relative='{}' full='{}'", path, full);
    m_mediaPlayer->setURL("file://" + full);
    m_mediaPlayer->play();
}

void WeaponSkillPreviewDialog::onDestroy()
{
    if (m_mediaPlayer)
    {
        m_mediaPlayer->stop();
        m_mediaPlayer = nullptr;
    }
    if (m_loaderVideo)
        m_loaderVideo->setContent(nullptr);
    m_loaderVideo = nullptr;
}

void WeaponSkillPreviewDialog::onClickClose(EventContext*)
{
    close();
}

}  // namespace gameui

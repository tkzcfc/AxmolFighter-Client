#include "WeaponSkillPreviewDialog.h"

#include "ui/widgets/common/MessageDialog.h"

#include "fairygui/GLoader3D.h"

namespace gameui
{

WeaponSkillPreviewDialog::WeaponSkillPreviewDialog(std::string videoPath) : m_videoPath(std::move(videoPath))
{
    m_options.hasBackground     = true;
    m_options.backgroundOpacity = 180;
    m_options.closeOnClickBg    = true;
    m_playAnimation             = false;
}

void WeaponSkillPreviewDialog::onCreate()
{
    addClickListener(getChild<GButton>("btnClose"), AX_CALLBACK_1(WeaponSkillPreviewDialog::onClickClose, this));
}

void WeaponSkillPreviewDialog::onShow()
{
    auto* loader = getChild<GLoader3D>("loaderVideo");
    if (!loader)
        return;

    std::string path = m_videoPath;
    if (path.empty() || !ax::FileUtils::getInstance()->isFileExist(path))
    {
        AXLOGW("WeaponSkillPreviewDialog: video missing '{}', fallback to placeholder", path);
        path = "video/ui_jinengyulan_jingqingqidai.mp4";
    }

    if (!ax::FileUtils::getInstance()->isFileExist(path))
    {
        AXLOGE("WeaponSkillPreviewDialog: placeholder video also missing");
        MessageDialog::show("技能预览视频缺失");
        return;
    }

    if (!m_mediaPlayer)
    {
        m_mediaPlayer = ax::ui::MediaPlayer::create();
        m_mediaPlayer->setContentSize(loader->getSize());
        m_mediaPlayer->setPosition(ax::Vec2(loader->getWidth() * 0.5f, -loader->getHeight() * 0.5f));
        m_mediaPlayer->setLooping(true);
        loader->setContent(m_mediaPlayer);
    }

    const std::string full = ax::FileUtils::getInstance()->fullPathForFilename(path);
    AXLOGI("WeaponSkillPreviewDialog: play {}", full);
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
}

void WeaponSkillPreviewDialog::onClickClose(EventContext*)
{
    close();
}

}  // namespace gameui

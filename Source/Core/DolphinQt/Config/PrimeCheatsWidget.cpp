#include "PrimeCheatsWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/ConfigManager.h"
#include "Core/Config/MainSettings.h"
#include "Common/Config/Config.h"

PrimeCheatsWidget::PrimeCheatsWidget()
{
  CreateWidgets();
  OnLoadConfig();
  ConnectWidgets();
  AddDescriptions();
}

void PrimeCheatsWidget::CreateWidgets()
{
  auto* group_box = new QGroupBox(tr("Cheats"));
  auto* main_layout = new QVBoxLayout;
  auto* layout = new QVBoxLayout;

  group_box->setLayout(layout);

  m_checkbox_noclip = new QCheckBox(tr("Noclip"));
  m_checkbox_invulnerability = new QCheckBox(tr("Invulnerability"));
  m_checkbox_skipcutscenes = new QCheckBox(tr("Skippable Cutscenes"));
  m_checkbox_scandash = new QCheckBox(tr("Restore Scan Dash"));
  m_checkbox_skipportalmp2 = new QCheckBox(tr("Skip MP2 Portal Cutscene"));
  m_checkbox_friendvouchers = new QCheckBox(tr("Remove Friend Vouchers Requirement (Trilogy Only)"));
  m_checkbox_hudmemo = new QCheckBox(tr("Disable Hud Popup on Pickup Acquire"));
  m_checkbox_hypermode = new QCheckBox(tr("Unlock Hypermode Difficulty"));

  layout->addWidget(m_checkbox_noclip);
  layout->addWidget(m_checkbox_invulnerability);
  layout->addWidget(m_checkbox_skipcutscenes);
  layout->addWidget(m_checkbox_scandash);
  layout->addWidget(m_checkbox_skipportalmp2);
  layout->addWidget(m_checkbox_friendvouchers);
  layout->addWidget(m_checkbox_hudmemo);
  layout->addWidget(m_checkbox_hypermode);

  main_layout->addWidget(group_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void PrimeCheatsWidget::ConnectWidgets()
{
  connect(m_checkbox_noclip, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_invulnerability, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_skipcutscenes, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_scandash, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_skipportalmp2, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_friendvouchers, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_hudmemo, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
  connect(m_checkbox_hypermode, &QCheckBox::toggled, this, &PrimeCheatsWidget::OnSaveConfig);
}

void PrimeCheatsWidget::OnSaveConfig()
{
  Config::SetBaseOrCurrent(Config::PRIMEHACK_NOCLIP, m_checkbox_noclip->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_INVULNERABILITY, m_checkbox_invulnerability->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_SKIPPABLE_CUTSCENES, m_checkbox_skipcutscenes->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_RESTORE_SCANDASH, m_checkbox_scandash->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_SKIPMP2_PORTAL, m_checkbox_skipportalmp2->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_FRIENDVOUCHERS, m_checkbox_friendvouchers->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_DISABLE_HUDMEMO, m_checkbox_hudmemo->isChecked());
  Config::SetBaseOrCurrent(Config::PRIMEHACK_UNLOCK_HYPERMODE, m_checkbox_hypermode->isChecked());
  Config::Save();
}

void PrimeCheatsWidget::OnLoadConfig()
{
  m_checkbox_noclip->setChecked(Config::Get(Config::PRIMEHACK_NOCLIP));
  m_checkbox_invulnerability->setChecked(Config::Get(Config::PRIMEHACK_INVULNERABILITY));
  m_checkbox_skipcutscenes->setChecked(Config::Get(Config::PRIMEHACK_SKIPPABLE_CUTSCENES));
  m_checkbox_scandash->setChecked(Config::Get(Config::PRIMEHACK_RESTORE_SCANDASH));
  m_checkbox_skipportalmp2->setChecked(Config::Get(Config::PRIMEHACK_SKIPMP2_PORTAL));
  m_checkbox_friendvouchers->setChecked(Config::Get(Config::PRIMEHACK_FRIENDVOUCHERS));
  m_checkbox_hudmemo->setChecked(Config::Get(Config::PRIMEHACK_DISABLE_HUDMEMO));
  m_checkbox_hypermode->setChecked(Config::Get(Config::PRIMEHACK_UNLOCK_HYPERMODE));
}

void PrimeCheatsWidget::AddDescriptions()
{
  static const char TR_NOCLIP[] =
    QT_TR_NOOP("Source Engine style noclip, fly through walls using the movement keys!");
  static const char TR_INVULNERABILITY[] =
    QT_TR_NOOP("Become resistant to all sources of damage. Projectiles literally will bounce off you!");
  static const char TR_SKIPCUTSCENES[] =
    QT_TR_NOOP("Make most cutscenes skippable. The button to do so varies from each game. It is usually the Jump key or the Menu button.");
  static const char TR_SCANDASH[] =
    QT_TR_NOOP("Re-enable the ability to dash with the scan visor. This is a speed-running trick in the original release, and was subsequently patched in later releases.");
  static const char TR_SKIPPORTAL[] =
    QT_TR_NOOP("Skips having to watch the portal cutscenes in Metroid Prime 2 (Trilogy), allowing you to teleport immediately.");
  static const char TR_FRIENDVOUCHERS[] =
    QT_TR_NOOP("Removes the friend voucher cost from all purchasable extras. This is on by default as friend-vouchers are impossible to obtain.");
  static const char TR_HUDMEMO[] =
    QT_TR_NOOP("Removes the item pickup screen and explanation screen for powerups.");
  static const char TR_HYPERMODE[] =
    QT_TR_NOOP("Unlock Hypermode Difficulty.");

  m_checkbox_noclip->setToolTip(tr(TR_NOCLIP));
  m_checkbox_invulnerability->setToolTip(tr(TR_INVULNERABILITY));
  m_checkbox_skipcutscenes->setToolTip(tr(TR_SKIPCUTSCENES));
  m_checkbox_scandash->setToolTip(tr(TR_SCANDASH));
  m_checkbox_skipportalmp2->setToolTip(tr(TR_SKIPPORTAL));
  m_checkbox_friendvouchers->setToolTip(tr(TR_FRIENDVOUCHERS));
  m_checkbox_hudmemo->setToolTip(tr(TR_HUDMEMO));
  m_checkbox_hypermode->setToolTip(tr(TR_HYPERMODE));
}

void PrimeCheatsWidget::showEvent(QShowEvent*)
{
  OnLoadConfig();
}

void PrimeCheatsWidget::enterEvent(QEvent*)
{
  OnLoadConfig();
}

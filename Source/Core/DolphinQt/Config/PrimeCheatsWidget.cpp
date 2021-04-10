#include "PrimeCheatsWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/ConfigManager.h"

PrimeCheatsWidget::PrimeCheatsWidget()
{
  CreateWidgets();
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

  layout->addWidget(m_checkbox_noclip);
  layout->addWidget(m_checkbox_invulnerability);
  layout->addWidget(m_checkbox_skipcutscenes);
  layout->addWidget(m_checkbox_scandash);
  layout->addWidget(m_checkbox_skipportalmp2);
  layout->addWidget(m_checkbox_friendvouchers);
  layout->addWidget(m_checkbox_hudmemo);

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
}

void PrimeCheatsWidget::OnSaveConfig()
{
  auto& settings = SConfig::GetInstance();
  settings.bPrimeNoclip = m_checkbox_noclip->isChecked();
  settings.bPrimeInvulnerability = m_checkbox_invulnerability->isChecked();
  settings.bPrimeSkipCutscene = m_checkbox_skipcutscenes->isChecked();
  settings.bPrimeRestoreDashing = m_checkbox_scandash->isChecked();
  settings.bPrimePortalSkip = m_checkbox_skipportalmp2->isChecked();
  settings.bPrimeFriendVouchers = m_checkbox_friendvouchers->isChecked();
  settings.bDisableHudMemoPopup = m_checkbox_hudmemo->isChecked();

  settings.SaveSettings();
}

void PrimeCheatsWidget::OnLoadConfig()
{
  auto& settings = SConfig::GetInstance();
  m_checkbox_noclip->setChecked(settings.bPrimeNoclip);
  m_checkbox_invulnerability->setChecked(settings.bPrimeInvulnerability);
  m_checkbox_skipcutscenes->setChecked(settings.bPrimeSkipCutscene);
  m_checkbox_scandash->setChecked(settings.bPrimeRestoreDashing);
  m_checkbox_skipportalmp2->setChecked(settings.bPrimePortalSkip);
  m_checkbox_friendvouchers->setChecked(settings.bPrimeFriendVouchers);
  m_checkbox_hudmemo->setChecked(settings.bDisableHudMemoPopup);
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

  m_checkbox_noclip->setToolTip(tr(TR_NOCLIP));
  m_checkbox_invulnerability->setToolTip(tr(TR_INVULNERABILITY));
  m_checkbox_skipcutscenes->setToolTip(tr(TR_SKIPCUTSCENES));
  m_checkbox_scandash->setToolTip(tr(TR_SCANDASH));
  m_checkbox_skipportalmp2->setToolTip(tr(TR_SKIPPORTAL));
  m_checkbox_friendvouchers->setToolTip(tr(TR_FRIENDVOUCHERS));
  m_checkbox_hudmemo->setToolTip(tr(TR_HUDMEMO));
}

void PrimeCheatsWidget::showEvent(QShowEvent*)
{
  OnLoadConfig();
}

void PrimeCheatsWidget::enterEvent(QEvent*)
{
  OnLoadConfig();
}

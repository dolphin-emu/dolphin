// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/GeneralPane.h"

#include <map>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

#include "UICommon/AutoUpdate.h"
#ifdef USE_DISCORD_PRESENCE
#include "UICommon/DiscordPresence.h"
#endif

constexpr int AUTO_UPDATE_DISABLE_INDEX = 0;
constexpr int AUTO_UPDATE_BETA_INDEX = 1;
constexpr int AUTO_UPDATE_DEV_INDEX = 2;

constexpr const char* AUTO_UPDATE_DISABLE_STRING = "";
constexpr const char* AUTO_UPDATE_BETA_STRING = "beta";
constexpr const char* AUTO_UPDATE_DEV_STRING = "dev";

constexpr int FALLBACK_REGION_NTSCJ_INDEX = 0;
constexpr int FALLBACK_REGION_NTSCU_INDEX = 1;
constexpr int FALLBACK_REGION_PAL_INDEX = 2;
constexpr int FALLBACK_REGION_NTSCK_INDEX = 3;

GeneralPane::GeneralPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadConfig();

  ConnectLayout();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &GeneralPane::OnEmulationStateChanged);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &GeneralPane::LoadConfig);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
}

void GeneralPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  // Create layout here
  CreateBasic();

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
    CreateAutoUpdate();

  CreateFallbackRegion();

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  CreateAnalytics();
#endif

  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void GeneralPane::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;
  const bool hardcore = AchievementManager::GetInstance().IsHardcoreModeActive();

  m_checkbox_dualcore->setEnabled(!running);
  m_checkbox_cheats->setEnabled(!running && !hardcore);
  m_checkbox_override_region_settings->setEnabled(!running);
#ifdef USE_DISCORD_PRESENCE
  m_checkbox_discord_presence->setEnabled(!running);
#endif
  m_combobox_fallback_region->setEnabled(!running);
}

void GeneralPane::ConnectLayout()
{
  connect(m_checkbox_dualcore, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_checkbox_cheats, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_checkbox_override_region_settings, &QCheckBox::stateChanged, this,
          &GeneralPane::OnSaveConfig);
  connect(m_checkbox_auto_disc_change, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
#ifdef USE_DISCORD_PRESENCE
  connect(m_checkbox_discord_presence, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
#endif

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    connect(m_combobox_update_track, &QComboBox::currentIndexChanged, this,
            &GeneralPane::OnSaveConfig);
    connect(&Settings::Instance(), &Settings::AutoUpdateTrackChanged, this,
            &GeneralPane::LoadConfig);
  }

  // Advanced
  connect(m_combobox_speedlimit, &QComboBox::currentIndexChanged, [this]() {
    Config::SetBaseOrCurrent(Config::MAIN_EMULATION_SPEED,
                             m_combobox_speedlimit->currentIndex() * 0.1f);
    Config::Save();
  });

  connect(m_combobox_fallback_region, &QComboBox::currentIndexChanged, this,
          &GeneralPane::OnSaveConfig);
  connect(&Settings::Instance(), &Settings::FallbackRegionChanged, this, &GeneralPane::LoadConfig);

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  connect(&Settings::Instance(), &Settings::AnalyticsToggled, this, &GeneralPane::LoadConfig);
  connect(m_checkbox_enable_analytics, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_button_generate_new_identity, &QPushButton::clicked, this,
          &GeneralPane::GenerateNewIdentity);
#endif
}

void GeneralPane::CreateBasic()
{
  auto* basic_group = new QGroupBox(tr("Basic Settings"));
  auto* basic_group_layout = new QVBoxLayout;
  basic_group->setLayout(basic_group_layout);
  m_main_layout->addWidget(basic_group);

  m_checkbox_dualcore = new QCheckBox(tr("Enable Dual Core (speedup)"));
  basic_group_layout->addWidget(m_checkbox_dualcore);

  m_checkbox_cheats = new QCheckBox(tr("Enable Cheats"));
  basic_group_layout->addWidget(m_checkbox_cheats);

  m_checkbox_override_region_settings = new QCheckBox(tr("Allow Mismatched Region Settings"));
  basic_group_layout->addWidget(m_checkbox_override_region_settings);

  m_checkbox_auto_disc_change = new QCheckBox(tr("Change Discs Automatically"));
  basic_group_layout->addWidget(m_checkbox_auto_disc_change);

#ifdef USE_DISCORD_PRESENCE
  m_checkbox_discord_presence = new QCheckBox(tr("Show Current Game on Discord"));
  basic_group_layout->addWidget(m_checkbox_discord_presence);
#endif

  auto* speed_limit_layout = new QFormLayout;
  speed_limit_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  speed_limit_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  basic_group_layout->addLayout(speed_limit_layout);

  m_combobox_speedlimit = new QComboBox();

  m_combobox_speedlimit->addItem(tr("Unlimited"));
  for (int i = 10; i <= 200; i += 10)  // from 10% to 200%
  {
    QString str;
    if (i != 100)
      str = QStringLiteral("%1%").arg(i);
    else
      str = tr("%1% (Normal Speed)").arg(i);

    m_combobox_speedlimit->addItem(str);
  }

  speed_limit_layout->addRow(tr("&Speed Limit:"), m_combobox_speedlimit);
}

void GeneralPane::CreateAutoUpdate()
{
  auto* auto_update_group = new QGroupBox(tr("Auto Update Settings"));
  auto* auto_update_group_layout = new QFormLayout;
  auto_update_group->setLayout(auto_update_group_layout);
  m_main_layout->addWidget(auto_update_group);

  auto_update_group_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  auto_update_group_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  m_combobox_update_track = new QComboBox(this);

  auto_update_group_layout->addRow(tr("&Auto Update:"), m_combobox_update_track);

  for (const QString& option :
       {tr("Don't Update"), tr("Beta (once a month)"), tr("Dev (multiple times a day)")})
    m_combobox_update_track->addItem(option);
}

void GeneralPane::CreateFallbackRegion()
{
  auto* fallback_region_group = new QGroupBox(tr("Fallback Region"));
  auto* fallback_region_group_layout = new QVBoxLayout;
  fallback_region_group->setLayout(fallback_region_group_layout);
  m_main_layout->addWidget(fallback_region_group);

  auto* fallback_region_dropdown_layout = new QFormLayout;
  fallback_region_dropdown_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  fallback_region_dropdown_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  fallback_region_group_layout->addLayout(fallback_region_dropdown_layout);

  m_combobox_fallback_region = new QComboBox(this);
  fallback_region_dropdown_layout->addRow(tr("Fallback Region:"), m_combobox_fallback_region);

  for (const QString& option : {tr("NTSC-J"), tr("NTSC-U"), tr("PAL"), tr("NTSC-K")})
    m_combobox_fallback_region->addItem(option);

  auto* fallback_region_description =
      new QLabel(tr("Dolphin will use this for titles whose region cannot be determined "
                    "automatically."));
  fallback_region_description->setWordWrap(true);
  fallback_region_group_layout->addWidget(fallback_region_description);
}

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::CreateAnalytics()
{
  auto* analytics_group = new QGroupBox(tr("Usage Statistics Reporting Settings"));
  auto* analytics_group_layout = new QVBoxLayout;
  analytics_group->setLayout(analytics_group_layout);
  m_main_layout->addWidget(analytics_group);

  m_checkbox_enable_analytics = new QCheckBox(tr("Enable Usage Statistics Reporting"));
  m_button_generate_new_identity =
      new NonDefaultQPushButton(tr("Generate a New Statistics Identity"));
  analytics_group_layout->addWidget(m_checkbox_enable_analytics);
  analytics_group_layout->addWidget(m_button_generate_new_identity);
}
#endif

void GeneralPane::LoadConfig()
{
  const QSignalBlocker blocker(this);

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    const auto track = Settings::Instance().GetAutoUpdateTrack().toStdString();

    // If the track doesn't match any known value, set to "beta" which is the
    // default config value on Dolphin release builds.
    if (track == AUTO_UPDATE_DISABLE_STRING)
      SignalBlocking(m_combobox_update_track)->setCurrentIndex(AUTO_UPDATE_DISABLE_INDEX);
    else if (track == AUTO_UPDATE_DEV_STRING)
      SignalBlocking(m_combobox_update_track)->setCurrentIndex(AUTO_UPDATE_DEV_INDEX);
    else
      SignalBlocking(m_combobox_update_track)->setCurrentIndex(AUTO_UPDATE_BETA_INDEX);
  }

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  SignalBlocking(m_checkbox_enable_analytics)
      ->setChecked(Settings::Instance().IsAnalyticsEnabled());
#endif
  SignalBlocking(m_checkbox_dualcore)->setChecked(Config::Get(Config::MAIN_CPU_THREAD));
  SignalBlocking(m_checkbox_cheats)->setChecked(Settings::Instance().GetCheatsEnabled());
  SignalBlocking(m_checkbox_override_region_settings)
      ->setChecked(Config::Get(Config::MAIN_OVERRIDE_REGION_SETTINGS));
  SignalBlocking(m_checkbox_auto_disc_change)
      ->setChecked(Config::Get(Config::MAIN_AUTO_DISC_CHANGE));

#ifdef USE_DISCORD_PRESENCE
  SignalBlocking(m_checkbox_discord_presence)
      ->setChecked(Config::Get(Config::MAIN_USE_DISCORD_PRESENCE));
#endif
  int selection = qRound(Config::Get(Config::MAIN_EMULATION_SPEED) * 10);
  if (selection < m_combobox_speedlimit->count())
    SignalBlocking(m_combobox_speedlimit)->setCurrentIndex(selection);

  const auto fallback = Settings::Instance().GetFallbackRegion();
  if (fallback == DiscIO::Region::NTSC_J)
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_NTSCJ_INDEX);
  else if (fallback == DiscIO::Region::NTSC_U)
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_NTSCU_INDEX);
  else if (fallback == DiscIO::Region::PAL)
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_PAL_INDEX);
  else if (fallback == DiscIO::Region::NTSC_K)
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_NTSCK_INDEX);
  else
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_NTSCJ_INDEX);
}

static QString UpdateTrackFromIndex(int index)
{
  QString value;

  switch (index)
  {
  case AUTO_UPDATE_DISABLE_INDEX:
    value = QString::fromStdString(AUTO_UPDATE_DISABLE_STRING);
    break;
  case AUTO_UPDATE_BETA_INDEX:
    value = QString::fromStdString(AUTO_UPDATE_BETA_STRING);
    break;
  case AUTO_UPDATE_DEV_INDEX:
    value = QString::fromStdString(AUTO_UPDATE_DEV_STRING);
    break;
  }

  return value;
}

static DiscIO::Region UpdateFallbackRegionFromIndex(int index)
{
  DiscIO::Region value = DiscIO::Region::Unknown;

  switch (index)
  {
  case FALLBACK_REGION_NTSCJ_INDEX:
    value = DiscIO::Region::NTSC_J;
    break;
  case FALLBACK_REGION_NTSCU_INDEX:
    value = DiscIO::Region::NTSC_U;
    break;
  case FALLBACK_REGION_PAL_INDEX:
    value = DiscIO::Region::PAL;
    break;
  case FALLBACK_REGION_NTSCK_INDEX:
    value = DiscIO::Region::NTSC_K;
    break;
  default:
    value = DiscIO::Region::NTSC_J;
  }

  return value;
}

void GeneralPane::OnSaveConfig()
{
  Config::ConfigChangeCallbackGuard config_guard;

  auto& settings = SConfig::GetInstance();
  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    Settings::Instance().SetAutoUpdateTrack(
        UpdateTrackFromIndex(m_combobox_update_track->currentIndex()));
  }

#ifdef USE_DISCORD_PRESENCE
  Discord::SetDiscordPresenceEnabled(m_checkbox_discord_presence->isChecked());
#endif

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  Settings::Instance().SetAnalyticsEnabled(m_checkbox_enable_analytics->isChecked());
  DolphinAnalytics::Instance().ReloadConfig();
#endif
  Config::SetBaseOrCurrent(Config::MAIN_CPU_THREAD, m_checkbox_dualcore->isChecked());
  Settings::Instance().SetCheatsEnabled(m_checkbox_cheats->isChecked());
  Config::SetBaseOrCurrent(Config::MAIN_OVERRIDE_REGION_SETTINGS,
                           m_checkbox_override_region_settings->isChecked());
  Config::SetBase(Config::MAIN_AUTO_DISC_CHANGE, m_checkbox_auto_disc_change->isChecked());
  Config::SetBaseOrCurrent(Config::MAIN_ENABLE_CHEATS, m_checkbox_cheats->isChecked());
  Settings::Instance().SetFallbackRegion(
      UpdateFallbackRegionFromIndex(m_combobox_fallback_region->currentIndex()));

  settings.SaveSettings();
}

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::GenerateNewIdentity()
{
  DolphinAnalytics::Instance().GenerateNewIdentity();
  DolphinAnalytics::Instance().ReloadConfig();
  ModalMessageBox message_box(this);
  message_box.setIcon(QMessageBox::Information);
  message_box.setWindowTitle(tr("Identity Generation"));
  message_box.setText(tr("New identity generated."));
  SetQWidgetWindowDecorations(&message_box);
  message_box.exec();
}
#endif

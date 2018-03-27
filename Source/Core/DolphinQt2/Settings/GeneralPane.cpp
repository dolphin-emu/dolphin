// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Settings/GeneralPane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/Analytics.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt2/Settings.h"
#include "UICommon/AutoUpdate.h"

constexpr int AUTO_UPDATE_DISABLE_INDEX = 0;
constexpr int AUTO_UPDATE_STABLE_INDEX = 1;
constexpr int AUTO_UPDATE_BETA_INDEX = 2;
constexpr int AUTO_UPDATE_DEV_INDEX = 3;

constexpr const char* AUTO_UPDATE_DISABLE_STRING = "";
constexpr const char* AUTO_UPDATE_STABLE_STRING = "stable";
constexpr const char* AUTO_UPDATE_BETA_STRING = "beta";
constexpr const char* AUTO_UPDATE_DEV_STRING = "dev";

GeneralPane::GeneralPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadConfig();

  ConnectLayout();
}

void GeneralPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  // Create layout here
  CreateBasic();

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
    CreateAutoUpdate();

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  CreateAnalytics();
#endif
  CreateAdvanced();

  m_main_layout->setContentsMargins(0, 0, 0, 0);
  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void GeneralPane::ConnectLayout()
{
  connect(m_checkbox_dualcore, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_checkbox_cheats, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    connect(m_combobox_update_track,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &GeneralPane::OnSaveConfig);
    connect(&Settings::Instance(), &Settings::AutoUpdateTrackChanged, this,
            &GeneralPane::LoadConfig);
  }

  // Advanced
  connect(m_combobox_speedlimit,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this]() { OnSaveConfig(); });
  connect(m_radio_interpreter, &QRadioButton::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_radio_cached_interpreter, &QRadioButton::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_radio_jit, &QRadioButton::toggled, this, &GeneralPane::OnSaveConfig);

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  connect(&Settings::Instance(), &Settings::AnalyticsToggled, this, &GeneralPane::LoadConfig);
  connect(m_checkbox_enable_analytics, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_button_generate_new_identity, &QPushButton::pressed, this,
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

  auto* speed_limit_layout = new QFormLayout;
  basic_group_layout->addLayout(speed_limit_layout);

  m_combobox_speedlimit = new QComboBox();

  m_combobox_speedlimit->addItem(tr("Unlimited"));
  for (int i = 10; i <= 200; i += 10)  // from 10% to 200%
  {
    QString str;
    if (i != 100)
      str.sprintf("%i%%", i);
    else
      str.sprintf("%i%% (Normal Speed)", i);

    m_combobox_speedlimit->addItem(str);
  }

  speed_limit_layout->addRow(tr("&Speed Limit:"), m_combobox_speedlimit);
}

void GeneralPane::CreateAutoUpdate()
{
  auto* auto_update_group = new QGroupBox(tr("Auto Update Settings"));
  auto* layout = new QFormLayout;
  auto_update_group->setLayout(layout);
  m_main_layout->addWidget(auto_update_group);

  m_combobox_update_track = new QComboBox(this);

  layout->addRow(tr("&Auto Update:"), m_combobox_update_track);

  for (const QString& option : {tr("Don't Update"), tr("Stable (once a year)"),
                                tr("Beta (once a month)"), tr("Dev (multiple times a day)")})
    m_combobox_update_track->addItem(option);
}

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::CreateAnalytics()
{
  auto* analytics_group = new QGroupBox(tr("Usage Statistics Reporting Settings"));
  auto* analytics_group_layout = new QVBoxLayout;
  analytics_group->setLayout(analytics_group_layout);
  m_main_layout->addWidget(analytics_group);

  m_checkbox_enable_analytics = new QCheckBox(tr("Enable Usage Statistics Reporting"));
  m_button_generate_new_identity = new QPushButton(tr("Generate a New Statistics Identity"));
  analytics_group_layout->addWidget(m_checkbox_enable_analytics);
  analytics_group_layout->addWidget(m_button_generate_new_identity);
}
#endif

void GeneralPane::CreateAdvanced()
{
  auto* advanced_group = new QGroupBox(tr("Advanced Settings"));
  auto* advanced_group_layout = new QVBoxLayout;
  advanced_group->setLayout(advanced_group_layout);
  m_main_layout->addWidget(advanced_group);

  // Speed Limit
  auto* engine_group = new QGroupBox(tr("CPU Emulation Engine"));
  auto* engine_group_layout = new QVBoxLayout;
  engine_group->setLayout(engine_group_layout);
  advanced_group_layout->addWidget(engine_group);

  m_radio_interpreter = new QRadioButton(tr("Interpreter (slowest)"));
  m_radio_cached_interpreter = new QRadioButton(tr("Cached Interpreter (slower)"));
  m_radio_jit = new QRadioButton(tr("JIT Recompiler (recommended)"));

  engine_group_layout->addWidget(m_radio_interpreter);
  engine_group_layout->addWidget(m_radio_cached_interpreter);
  engine_group_layout->addWidget(m_radio_jit);
}

void GeneralPane::LoadConfig()
{
  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    const auto track = Settings::Instance().GetAutoUpdateTrack().toStdString();

    if (track == AUTO_UPDATE_DISABLE_STRING)
      m_combobox_update_track->setCurrentIndex(AUTO_UPDATE_DISABLE_INDEX);
    else if (track == AUTO_UPDATE_STABLE_STRING)
      m_combobox_update_track->setCurrentIndex(AUTO_UPDATE_STABLE_INDEX);
    else if (track == AUTO_UPDATE_BETA_STRING)
      m_combobox_update_track->setCurrentIndex(AUTO_UPDATE_BETA_INDEX);
    else
      m_combobox_update_track->setCurrentIndex(AUTO_UPDATE_DEV_INDEX);
  }

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  m_checkbox_enable_analytics->setChecked(Settings::Instance().IsAnalyticsEnabled());
#endif
  m_checkbox_dualcore->setChecked(SConfig::GetInstance().bCPUThread);
  m_checkbox_cheats->setChecked(Settings::Instance().GetCheatsEnabled());
  int selection = qRound(SConfig::GetInstance().m_EmulationSpeed * 10);
  if (selection < m_combobox_speedlimit->count())
    m_combobox_speedlimit->setCurrentIndex(selection);
  m_checkbox_dualcore->setChecked(SConfig::GetInstance().bCPUThread);

  switch (SConfig::GetInstance().iCPUCore)
  {
  case PowerPC::CPUCore::CORE_INTERPRETER:
    m_radio_interpreter->setChecked(true);
    break;
  case PowerPC::CPUCore::CORE_CACHEDINTERPRETER:
    m_radio_cached_interpreter->setChecked(true);
    break;
  case PowerPC::CPUCore::CORE_JIT64:
    m_radio_jit->setChecked(true);
    break;
  case PowerPC::CPUCore::CORE_JITARM64:
    // TODO: Implement JITARM
    break;
  default:
    break;
  }
}

static QString UpdateTrackFromIndex(int index)
{
  QString value;

  switch (index)
  {
  case AUTO_UPDATE_DISABLE_INDEX:
    value = QString::fromStdString(AUTO_UPDATE_DISABLE_STRING);
    break;
  case AUTO_UPDATE_STABLE_INDEX:
    value = QString::fromStdString(AUTO_UPDATE_STABLE_STRING);
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

void GeneralPane::OnSaveConfig()
{
  auto& settings = SConfig::GetInstance();
  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    Settings::Instance().SetAutoUpdateTrack(
        UpdateTrackFromIndex(m_combobox_update_track->currentIndex()));
  }

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  Settings::Instance().SetAnalyticsEnabled(m_checkbox_enable_analytics->isChecked());
#endif
  settings.bCPUThread = m_checkbox_dualcore->isChecked();
  Settings::Instance().SetCheatsEnabled(m_checkbox_cheats->isChecked());
  settings.m_EmulationSpeed = m_combobox_speedlimit->currentIndex() * 0.1f;
  int engine_value = 0;
  if (m_radio_interpreter->isChecked())
    engine_value = PowerPC::CPUCore::CORE_INTERPRETER;
  else if (m_radio_cached_interpreter->isChecked())
    engine_value = PowerPC::CPUCore::CORE_CACHEDINTERPRETER;
  else if (m_radio_jit->isChecked())
    engine_value = PowerPC::CPUCore::CORE_JIT64;
  else
    engine_value = PowerPC::CPUCore::CORE_JIT64;

  settings.iCPUCore = engine_value;
  settings.SaveSettings();
}

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::GenerateNewIdentity()
{
  DolphinAnalytics::Instance()->GenerateNewIdentity();
  QMessageBox message_box;
  message_box.setIcon(QMessageBox::Information);
  message_box.setWindowTitle(tr("Identity Generation"));
  message_box.setText(tr("New identity generated."));
  message_box.exec();
}
#endif

// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Settings/GeneralPane.h"

#include <QButtonGroup>
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
#include "DolphinQt2/QtUtils/Bind.h"
#include "DolphinQt2/Settings.h"

GeneralPane::GeneralPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  ConnectLayout();
}

void GeneralPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  // Create layout here
  CreateBasic();
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  CreateAnalytics();
#endif
  CreateAdvanced();
  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void GeneralPane::ConnectLayout()
{
  // Basic
  BindControlToValue(m_checkbox_dualcore, SConfig::GetInstance().bCPUThread);
  BindControlToValue(m_checkbox_cheats, SConfig::GetInstance().bEnableCheats);
  BindControlToValue(m_combobox_speedlimit, SConfig::GetInstance().m_EmulationSpeed,
                     [](float speedlimit) { return static_cast<int>(std::round(speedlimit * 10)); },
                     [](int index) { return index * 0.1f; });

  // Advanced
  auto* cpu_emulator_engine = new QButtonGroup(this);
  cpu_emulator_engine->addButton(m_radio_interpreter, PowerPC::CPUCore::CORE_INTERPRETER);
  cpu_emulator_engine->addButton(m_radio_cached_interpreter,
                                 PowerPC::CPUCore::CORE_CACHEDINTERPRETER);
  cpu_emulator_engine->addButton(m_radio_jit, PowerPC::CPUCore::CORE_JIT64);
  // TODO: Implement JITARM
  BindControlToValue(cpu_emulator_engine, SConfig::GetInstance().iCPUCore);
  BindControlToValue(m_checkbox_force_ntsc, SConfig::GetInstance().bForceNTSCJ);

// Analytics
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  BindControlToValue(m_checkbox_enable_analytics, SConfig::GetInstance().m_analytics_enabled);
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

  auto* speed_limit_layout = new QFormLayout;
  basic_group_layout->addLayout(speed_limit_layout);

  m_combobox_speedlimit = new QComboBox();
  m_combobox_speedlimit->setMaximumWidth(300);

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

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::CreateAnalytics()
{
  auto* analytics_group = new QGroupBox(tr("Usage Statistics Reporting"));
  auto* analytics_group_layout = new QVBoxLayout;
  analytics_group->setLayout(analytics_group_layout);
  m_main_layout->addWidget(analytics_group);

  m_checkbox_enable_analytics = new QCheckBox(tr("Enable Usage Statistics Reporting"));
  m_button_generate_new_identity = new QPushButton(tr("Generate a New Statistics Identity"));
  m_button_generate_new_identity->setMaximumWidth(300);
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

  // NTSC-J
  m_checkbox_force_ntsc = new QCheckBox(tr("Force Console as NTSC-J"));
  advanced_group_layout->addWidget(m_checkbox_force_ntsc);
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

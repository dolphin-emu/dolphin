// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Settings/MiscPane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/Settings.h"

static const std::map<PowerPC::CPUCore, const char*> CPU_CORE_NAMES = {
    {PowerPC::CPUCore::Interpreter, QT_TR_NOOP("Interpreter (slowest)")},
    {PowerPC::CPUCore::CachedInterpreter, QT_TR_NOOP("Cached Interpreter (slower)")},
    {PowerPC::CPUCore::JIT64, QT_TR_NOOP("JIT Recompiler for x86-64 (recommended)")},
    {PowerPC::CPUCore::JITARM64, QT_TR_NOOP("JIT Recompiler for ARM64 (recommended)")},
};

MiscPane::MiscPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  Update();

  ConnectLayout();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &MiscPane::Update);
}

void MiscPane::CreateLayout()
{
  auto* main_layout = new QVBoxLayout();
  setLayout(main_layout);

  auto* cpu_options = new QGroupBox(tr("CPU Options"));
  auto* cpu_options_layout = new QVBoxLayout();
  cpu_options->setLayout(cpu_options_layout);
  main_layout->addWidget(cpu_options);

  QGridLayout* cpu_emulation_layout = new QGridLayout();
  QLabel* cpu_emulation_engine_label = new QLabel(tr("CPU Emulation Engine:"));
  m_cpu_emulation_engine_combobox = new QComboBox(this);
  for (PowerPC::CPUCore cpu_core : PowerPC::AvailableCPUCores())
  {
    m_cpu_emulation_engine_combobox->addItem(tr(CPU_CORE_NAMES.at(cpu_core)));
  }
  cpu_emulation_layout->addWidget(cpu_emulation_engine_label, 0, 0);
  cpu_emulation_layout->addWidget(m_cpu_emulation_engine_combobox, 0, 1, Qt::AlignLeft);
  cpu_options_layout->addLayout(cpu_emulation_layout);

  m_enable_mmu_checkbox = new QCheckBox(tr("Enable MMU"));
  m_enable_mmu_checkbox->setToolTip(tr(
      "Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = Fast)"));
  cpu_options_layout->addWidget(m_enable_mmu_checkbox);

  auto* rtc_options = new QGroupBox(tr("Custom RTC Options"));
  rtc_options->setLayout(new QVBoxLayout());
  main_layout->addWidget(rtc_options);

  m_custom_rtc_checkbox = new QCheckBox(tr("Enable Custom RTC"));
  rtc_options->layout()->addWidget(m_custom_rtc_checkbox);

  m_custom_rtc_datetime = new QDateTimeEdit();

  // Show seconds
  m_custom_rtc_datetime->setDisplayFormat(m_custom_rtc_datetime->displayFormat().replace(
      QStringLiteral("mm"), QStringLiteral("mm:ss")));

  if (!m_custom_rtc_datetime->displayFormat().contains(QStringLiteral("yyyy")))
  {
    // Always show the full year, no matter what the locale specifies. Otherwise, two-digit years
    // will always be interpreted as in the 21st century.
    m_custom_rtc_datetime->setDisplayFormat(m_custom_rtc_datetime->displayFormat().replace(
        QStringLiteral("yy"), QStringLiteral("yyyy")));
  }
  m_custom_rtc_datetime->setDateTimeRange(QDateTime({2000, 1, 1}, {0, 0, 0}, Qt::UTC),
                                          QDateTime({2099, 12, 31}, {23, 59, 59}, Qt::UTC));
  m_custom_rtc_datetime->setTimeSpec(Qt::UTC);
  rtc_options->layout()->addWidget(m_custom_rtc_datetime);

  auto* custom_rtc_description =
      new QLabel(tr("This setting allows you to set a custom real time clock (RTC) separate from "
                    "your current system time.\n\nIf unsure, leave this unchecked."));
  custom_rtc_description->setWordWrap(true);
  rtc_options->layout()->addWidget(custom_rtc_description);

  main_layout->addStretch(1);
}

void MiscPane::ConnectLayout()
{
  connect(m_cpu_emulation_engine_combobox,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [](int index) {
            SConfig::GetInstance().cpu_core = PowerPC::AvailableCPUCores()[index];
            Config::SetBaseOrCurrent(Config::MAIN_CPU_CORE, PowerPC::AvailableCPUCores()[index]);
          });

  connect(m_enable_mmu_checkbox, &QCheckBox::toggled, this,
          [](bool checked) { SConfig::GetInstance().bMMU = checked; });

  m_custom_rtc_checkbox->setChecked(SConfig::GetInstance().bEnableCustomRTC);
  connect(m_custom_rtc_checkbox, &QCheckBox::toggled, [this](bool enable_custom_rtc) {
    SConfig::GetInstance().bEnableCustomRTC = enable_custom_rtc;
    Update();
  });

  QDateTime initial_date_time;
  initial_date_time.setSecsSinceEpoch(SConfig::GetInstance().m_customRTCValue);
  m_custom_rtc_datetime->setDateTime(initial_date_time);
  connect(m_custom_rtc_datetime, &QDateTimeEdit::dateTimeChanged, [this](QDateTime date_time) {
    SConfig::GetInstance().m_customRTCValue = static_cast<u32>(date_time.toSecsSinceEpoch());
    Update();
  });
}

void MiscPane::Update()
{
  const bool running = Core::GetState() != Core::State::Uninitialized;
  const bool enable_custom_rtc_widgets = SConfig::GetInstance().bEnableCustomRTC && !running;

  const std::vector<PowerPC::CPUCore>& available_cpu_cores = PowerPC::AvailableCPUCores();
  for (size_t i = 0; i < available_cpu_cores.size(); ++i)
  {
    if (available_cpu_cores[i] == SConfig::GetInstance().cpu_core)
      m_cpu_emulation_engine_combobox->setCurrentIndex(int(i));
  }
  m_cpu_emulation_engine_combobox->setEnabled(!running);

  m_enable_mmu_checkbox->setChecked(SConfig::GetInstance().bMMU);
  m_enable_mmu_checkbox->setEnabled(!running);

  m_custom_rtc_checkbox->setEnabled(!running);
  m_custom_rtc_datetime->setEnabled(enable_custom_rtc_widgets);
}

// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Settings/AdvancedPane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/Settings.h"

static const std::map<PowerPC::CPUCore, const char*> CPU_CORE_NAMES = {
    {PowerPC::CPUCore::Interpreter, QT_TR_NOOP("Interpreter (slowest)")},
    {PowerPC::CPUCore::CachedInterpreter, QT_TR_NOOP("Cached Interpreter (slower)")},
    {PowerPC::CPUCore::JIT64, QT_TR_NOOP("JIT Recompiler (recommended)")},
    {PowerPC::CPUCore::JITARM64, QT_TR_NOOP("JIT Arm64 (experimental)")},
};

AdvancedPane::AdvancedPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  Update();

  ConnectLayout();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &AdvancedPane::Update);
}

void AdvancedPane::CreateLayout()
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

  auto* clock_override = new QGroupBox(tr("Clock Override"));
  auto* clock_override_layout = new QVBoxLayout();
  clock_override->setLayout(clock_override_layout);
  main_layout->addWidget(clock_override);

  m_cpu_clock_override_checkbox = new QCheckBox(tr("Enable Emulated CPU Clock Override"));
  clock_override_layout->addWidget(m_cpu_clock_override_checkbox);

  auto* cpu_clock_override_slider_layout = new QHBoxLayout();
  cpu_clock_override_slider_layout->setContentsMargins(0, 0, 0, 0);
  clock_override_layout->addLayout(cpu_clock_override_slider_layout);

  m_cpu_clock_override_slider = new QSlider(Qt::Horizontal);
  m_cpu_clock_override_slider->setRange(0, 150);
  cpu_clock_override_slider_layout->addWidget(m_cpu_clock_override_slider);

  m_cpu_clock_override_slider_label = new QLabel();
  cpu_clock_override_slider_layout->addWidget(m_cpu_clock_override_slider_label);

  auto* cpu_clock_override_description =
      new QLabel(tr("Adjusts the emulated CPU's clock rate.\n\n"
                    "Higher values may make variable-framerate games run at a higher framerate, "
                    "at the expense of performance. Lower values may activate a game's "
                    "internal frameskip, potentially improving performance.\n\n"
                    "WARNING: Changing this from the default (100%) can and will "
                    "break games and cause glitches. Do so at your own risk. "
                    "Please do not report bugs that occur with a non-default clock."));
  cpu_clock_override_description->setWordWrap(true);
  clock_override_layout->addWidget(cpu_clock_override_description);

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
  m_custom_rtc_datetime->setDateRange({2000, 1, 1}, {2099, 12, 31});
  rtc_options->layout()->addWidget(m_custom_rtc_datetime);

  auto* custom_rtc_description =
      new QLabel(tr("This setting allows you to set a custom real time clock (RTC) separate from "
                    "your current system time.\n\nIf unsure, leave this unchecked."));
  custom_rtc_description->setWordWrap(true);
  rtc_options->layout()->addWidget(custom_rtc_description);

  main_layout->addStretch(1);
}

void AdvancedPane::ConnectLayout()
{
  connect(m_cpu_emulation_engine_combobox,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int index) {
            SConfig::GetInstance().cpu_core = PowerPC::AvailableCPUCores()[index];
            Config::SetBaseOrCurrent(Config::MAIN_CPU_CORE, PowerPC::AvailableCPUCores()[index]);
          });

  m_cpu_clock_override_checkbox->setChecked(SConfig::GetInstance().m_OCEnable);
  connect(m_cpu_clock_override_checkbox, &QCheckBox::toggled, [this](bool enable_clock_override) {
    SConfig::GetInstance().m_OCEnable = enable_clock_override;
    Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK_ENABLE, enable_clock_override);
    Update();
  });

  connect(m_cpu_clock_override_slider, &QSlider::valueChanged, [this](int oc_factor) {
    // Vaguely exponential scaling?
    const float factor = std::exp2f((m_cpu_clock_override_slider->value() - 100.f) / 25.f);
    SConfig::GetInstance().m_OCFactor = factor;
    Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK, factor);
    Update();
  });

  m_custom_rtc_checkbox->setChecked(SConfig::GetInstance().bEnableCustomRTC);
  connect(m_custom_rtc_checkbox, &QCheckBox::toggled, [this](bool enable_custom_rtc) {
    SConfig::GetInstance().bEnableCustomRTC = enable_custom_rtc;
    Update();
  });

  QDateTime initial_date_time;
  initial_date_time.setTime_t(SConfig::GetInstance().m_customRTCValue);
  m_custom_rtc_datetime->setDateTime(initial_date_time);
  connect(m_custom_rtc_datetime, &QDateTimeEdit::dateTimeChanged, [this](QDateTime date_time) {
    SConfig::GetInstance().m_customRTCValue = date_time.toTime_t();
    Update();
  });
}

void AdvancedPane::Update()
{
  const bool running = Core::GetState() != Core::State::Uninitialized;
  const bool enable_cpu_clock_override_widgets = SConfig::GetInstance().m_OCEnable;
  const bool enable_custom_rtc_widgets = SConfig::GetInstance().bEnableCustomRTC && !running;

  const std::vector<PowerPC::CPUCore>& available_cpu_cores = PowerPC::AvailableCPUCores();
  for (int i = 0; i < available_cpu_cores.size(); ++i)
  {
    if (available_cpu_cores[i] == SConfig::GetInstance().cpu_core)
      m_cpu_emulation_engine_combobox->setCurrentIndex(i);
  }
  m_cpu_emulation_engine_combobox->setEnabled(!running);

  QFont bf = font();
  bf.setBold(Config::GetActiveLayerForConfig(Config::MAIN_OVERCLOCK_ENABLE) !=
             Config::LayerType::Base);
  m_cpu_clock_override_checkbox->setFont(bf);
  m_cpu_clock_override_checkbox->setChecked(enable_cpu_clock_override_widgets);

  m_cpu_clock_override_slider->setEnabled(enable_cpu_clock_override_widgets);
  m_cpu_clock_override_slider_label->setEnabled(enable_cpu_clock_override_widgets);

  {
    const QSignalBlocker blocker(m_cpu_clock_override_slider);
    m_cpu_clock_override_slider->setValue(
        static_cast<int>(std::round(std::log2f(SConfig::GetInstance().m_OCFactor) * 25.f + 100.f)));
  }

  m_cpu_clock_override_slider_label->setText([] {
    int core_clock = SystemTimers::GetTicksPerSecond() / std::pow(10, 6);
    int percent = static_cast<int>(std::round(SConfig::GetInstance().m_OCFactor * 100.f));
    int clock = static_cast<int>(std::round(SConfig::GetInstance().m_OCFactor * core_clock));
    return tr("%1 % (%2 MHz)").arg(QString::number(percent), QString::number(clock));
  }());

  m_custom_rtc_checkbox->setEnabled(!running);
  m_custom_rtc_datetime->setEnabled(enable_custom_rtc_widgets);
}

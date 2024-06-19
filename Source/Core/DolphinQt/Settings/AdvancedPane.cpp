// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
#include "Core/System.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

static const std::map<PowerPC::CPUCore, const char*> CPU_CORE_NAMES = {
    {PowerPC::CPUCore::Interpreter, QT_TR_NOOP("Interpreter (slowest)")},
    {PowerPC::CPUCore::CachedInterpreter, QT_TR_NOOP("Cached Interpreter (slower)")},
    {PowerPC::CPUCore::JIT64, QT_TR_NOOP("JIT Recompiler for x86-64 (recommended)")},
    {PowerPC::CPUCore::JITARM64, QT_TR_NOOP("JIT Recompiler for ARM64 (recommended)")},
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

  auto* cpu_options_group = new QGroupBox(tr("CPU Options"));
  auto* cpu_options_group_layout = new QVBoxLayout();
  cpu_options_group->setLayout(cpu_options_group_layout);
  main_layout->addWidget(cpu_options_group);

  auto* cpu_emulation_engine_layout = new QFormLayout;
  cpu_emulation_engine_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  cpu_emulation_engine_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  cpu_options_group_layout->addLayout(cpu_emulation_engine_layout);

  m_cpu_emulation_engine_combobox = new QComboBox(this);
  cpu_emulation_engine_layout->addRow(tr("CPU Emulation Engine:"), m_cpu_emulation_engine_combobox);
  for (PowerPC::CPUCore cpu_core : PowerPC::AvailableCPUCores())
  {
    m_cpu_emulation_engine_combobox->addItem(tr(CPU_CORE_NAMES.at(cpu_core)));
  }

  m_enable_mmu_checkbox = new ConfigBool(tr("Enable MMU"), Config::MAIN_MMU);
  m_enable_mmu_checkbox->SetDescription(
      tr("Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = "
         "Fast)<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  cpu_options_group_layout->addWidget(m_enable_mmu_checkbox);

  m_pause_on_panic_checkbox = new ConfigBool(tr("Pause on Panic"), Config::MAIN_PAUSE_ON_PANIC);
  m_pause_on_panic_checkbox->SetDescription(
      tr("Pauses the emulation if a Read/Write or Unknown Instruction panic occurs.<br>Enabling "
         "will affect performance.<br>The performance impact is the same as having Enable MMU "
         "on.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  cpu_options_group_layout->addWidget(m_pause_on_panic_checkbox);

  m_accurate_cpu_cache_checkbox =
      new ConfigBool(tr("Enable Write-Back Cache (slow)"), Config::MAIN_ACCURATE_CPU_CACHE);
  m_accurate_cpu_cache_checkbox->SetDescription(
      tr("Enables emulation of the CPU write-back cache.<br>Enabling will have a significant "
         "impact on performance.<br>This should be left disabled unless absolutely "
         "needed.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  cpu_options_group_layout->addWidget(m_accurate_cpu_cache_checkbox);

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
  m_cpu_clock_override_slider->setRange(1, 400);
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

  auto* ram_override = new QGroupBox(tr("Memory Override"));
  auto* ram_override_layout = new QVBoxLayout();
  ram_override->setLayout(ram_override_layout);
  main_layout->addWidget(ram_override);

  m_ram_override_checkbox = new QCheckBox(tr("Enable Emulated Memory Size Override"));
  ram_override_layout->addWidget(m_ram_override_checkbox);

  auto* mem1_override_slider_layout = new QHBoxLayout();
  mem1_override_slider_layout->setContentsMargins(0, 0, 0, 0);
  ram_override_layout->addLayout(mem1_override_slider_layout);

  m_mem1_override_slider = new QSlider(Qt::Horizontal);
  m_mem1_override_slider->setRange(24, 64);
  mem1_override_slider_layout->addWidget(m_mem1_override_slider);

  m_mem1_override_slider_label = new QLabel();
  mem1_override_slider_layout->addWidget(m_mem1_override_slider_label);

  auto* mem2_override_slider_layout = new QHBoxLayout();
  mem2_override_slider_layout->setContentsMargins(0, 0, 0, 0);
  ram_override_layout->addLayout(mem2_override_slider_layout);

  m_mem2_override_slider = new QSlider(Qt::Horizontal);
  m_mem2_override_slider->setRange(64, 128);
  mem2_override_slider_layout->addWidget(m_mem2_override_slider);

  m_mem2_override_slider_label = new QLabel();
  mem2_override_slider_layout->addWidget(m_mem2_override_slider_label);

  auto* ram_override_description =
      new QLabel(tr("Adjusts the amount of RAM in the emulated console.\n\n"
                    "WARNING: Enabling this will completely break many games. Only a small number "
                    "of games can benefit from this."));

  ram_override_description->setWordWrap(true);
  ram_override_layout->addWidget(ram_override_description);

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

void AdvancedPane::ConnectLayout()
{
  connect(m_cpu_emulation_engine_combobox, &QComboBox::currentIndexChanged, [](int index) {
    const auto cpu_cores = PowerPC::AvailableCPUCores();
    if (index >= 0 && static_cast<size_t>(index) < cpu_cores.size())
      Config::SetBaseOrCurrent(Config::MAIN_CPU_CORE, cpu_cores[index]);
  });

  connect(m_cpu_clock_override_checkbox, &QCheckBox::toggled, [this](bool enable_clock_override) {
    Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK_ENABLE, enable_clock_override);
    Update();
  });

  connect(m_cpu_clock_override_slider, &QSlider::valueChanged, [this](int oc_factor) {
    const float factor = m_cpu_clock_override_slider->value() / 100.f;
    Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK, factor);
    Update();
  });

  connect(m_ram_override_checkbox, &QCheckBox::toggled, [this](bool enable_ram_override) {
    Config::SetBaseOrCurrent(Config::MAIN_RAM_OVERRIDE_ENABLE, enable_ram_override);
    Update();
  });

  connect(m_mem1_override_slider, &QSlider::valueChanged, [this](int slider_value) {
    const u32 mem1_size = m_mem1_override_slider->value() * 0x100000;
    Config::SetBaseOrCurrent(Config::MAIN_MEM1_SIZE, mem1_size);
    Update();
  });

  connect(m_mem2_override_slider, &QSlider::valueChanged, [this](int slider_value) {
    const u32 mem2_size = m_mem2_override_slider->value() * 0x100000;
    Config::SetBaseOrCurrent(Config::MAIN_MEM2_SIZE, mem2_size);
    Update();
  });

  connect(m_custom_rtc_checkbox, &QCheckBox::toggled, [this](bool enable_custom_rtc) {
    Config::SetBaseOrCurrent(Config::MAIN_CUSTOM_RTC_ENABLE, enable_custom_rtc);
    Update();
  });

  connect(m_custom_rtc_datetime, &QDateTimeEdit::dateTimeChanged, [this](QDateTime date_time) {
    Config::SetBaseOrCurrent(Config::MAIN_CUSTOM_RTC_VALUE,
                             static_cast<u32>(date_time.toSecsSinceEpoch()));
    Update();
  });
}

void AdvancedPane::Update()
{
  const bool running = !Core::IsUninitialized(Core::System::GetInstance());
  const bool enable_cpu_clock_override_widgets = Config::Get(Config::MAIN_OVERCLOCK_ENABLE);
  const bool enable_ram_override_widgets = Config::Get(Config::MAIN_RAM_OVERRIDE_ENABLE);
  const bool enable_custom_rtc_widgets = Config::Get(Config::MAIN_CUSTOM_RTC_ENABLE) && !running;

  const auto available_cpu_cores = PowerPC::AvailableCPUCores();
  const auto cpu_core = Config::Get(Config::MAIN_CPU_CORE);
  for (size_t i = 0; i < available_cpu_cores.size(); ++i)
  {
    if (available_cpu_cores[i] == cpu_core)
      m_cpu_emulation_engine_combobox->setCurrentIndex(int(i));
  }
  m_cpu_emulation_engine_combobox->setEnabled(!running);
  m_enable_mmu_checkbox->setEnabled(!running);
  m_pause_on_panic_checkbox->setEnabled(!running);

  {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(Config::MAIN_OVERCLOCK_ENABLE) !=
               Config::LayerType::Base);

    const QSignalBlocker blocker(m_cpu_clock_override_checkbox);
    m_cpu_clock_override_checkbox->setFont(bf);
    m_cpu_clock_override_checkbox->setChecked(enable_cpu_clock_override_widgets);
  }

  m_cpu_clock_override_slider->setEnabled(enable_cpu_clock_override_widgets);
  m_cpu_clock_override_slider_label->setEnabled(enable_cpu_clock_override_widgets);

  {
    const QSignalBlocker blocker(m_cpu_clock_override_slider);
    m_cpu_clock_override_slider->setValue(
        static_cast<int>(std::round(Config::Get(Config::MAIN_OVERCLOCK) * 100.f)));
  }

  m_cpu_clock_override_slider_label->setText([] {
    int core_clock =
        Core::System::GetInstance().GetSystemTimers().GetTicksPerSecond() / std::pow(10, 6);
    int percent = static_cast<int>(std::round(Config::Get(Config::MAIN_OVERCLOCK) * 100.f));
    int clock = static_cast<int>(std::round(Config::Get(Config::MAIN_OVERCLOCK) * core_clock));
    return tr("%1% (%2 MHz)").arg(QString::number(percent), QString::number(clock));
  }());

  m_ram_override_checkbox->setEnabled(!running);
  SignalBlocking(m_ram_override_checkbox)->setChecked(enable_ram_override_widgets);

  m_mem1_override_slider->setEnabled(enable_ram_override_widgets && !running);
  m_mem1_override_slider_label->setEnabled(enable_ram_override_widgets && !running);

  {
    const QSignalBlocker blocker(m_mem1_override_slider);
    const u32 mem1_size = Config::Get(Config::MAIN_MEM1_SIZE) / 0x100000;
    m_mem1_override_slider->setValue(mem1_size);
  }

  m_mem1_override_slider_label->setText([] {
    const u32 mem1_size = Config::Get(Config::MAIN_MEM1_SIZE) / 0x100000;
    return tr("%1 MB (MEM1)").arg(QString::number(mem1_size));
  }());

  m_mem2_override_slider->setEnabled(enable_ram_override_widgets && !running);
  m_mem2_override_slider_label->setEnabled(enable_ram_override_widgets && !running);

  {
    const QSignalBlocker blocker(m_mem2_override_slider);
    const u32 mem2_size = Config::Get(Config::MAIN_MEM2_SIZE) / 0x100000;
    m_mem2_override_slider->setValue(mem2_size);
  }

  m_mem2_override_slider_label->setText([] {
    const u32 mem2_size = Config::Get(Config::MAIN_MEM2_SIZE) / 0x100000;
    return tr("%1 MB (MEM2)").arg(QString::number(mem2_size));
  }());

  m_custom_rtc_checkbox->setEnabled(!running);
  SignalBlocking(m_custom_rtc_checkbox)->setChecked(Config::Get(Config::MAIN_CUSTOM_RTC_ENABLE));

  QDateTime initial_date_time;
  initial_date_time.setSecsSinceEpoch(Config::Get(Config::MAIN_CUSTOM_RTC_VALUE));
  m_custom_rtc_datetime->setEnabled(enable_custom_rtc_widgets);
  SignalBlocking(m_custom_rtc_datetime)->setDateTime(initial_date_time);
}

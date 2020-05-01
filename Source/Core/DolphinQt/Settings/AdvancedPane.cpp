// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/AdvancedPane.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

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

  main_layout->addStretch(1);
}

void AdvancedPane::ConnectLayout()
{
  connect(m_cpu_clock_override_checkbox, &QCheckBox::toggled, [this](bool enable_clock_override) {
    Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK_ENABLE, enable_clock_override);
    Update();
  });

  connect(m_cpu_clock_override_slider, &QSlider::valueChanged, [this](int oc_factor) {
    // Vaguely exponential scaling?
    const float factor = std::exp2f((m_cpu_clock_override_slider->value() - 100.f) / 25.f);
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
}

void AdvancedPane::Update()
{
  const bool running = Core::GetState() != Core::State::Uninitialized;
  const bool enable_cpu_clock_override_widgets = Config::Get(Config::MAIN_OVERCLOCK_ENABLE);
  const bool enable_ram_override_widgets = Config::Get(Config::MAIN_RAM_OVERRIDE_ENABLE);

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
    m_cpu_clock_override_slider->setValue(static_cast<int>(
        std::round(std::log2f(Config::Get(Config::MAIN_OVERCLOCK)) * 25.f + 100.f)));
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
}

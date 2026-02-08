// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/AdvancedPane.h"

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTimeZone>
#include <QVBoxLayout>
#include <cmath>

#include "Common/Config/Config.h"
#include "Common/Config/Enums.h"
#include "Common/FileUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigFloatSlider.h"
#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"
#include "DolphinQt/QtUtils/AnalyticsPrompt.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

#include "UICommon/UICommon.h"

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

  std::vector<std::pair<QString, PowerPC::CPUCore>> emulation_engine_choices;
  for (PowerPC::CPUCore cpu_core : PowerPC::AvailableCPUCores())
    emulation_engine_choices.emplace_back(tr(CPU_CORE_NAMES.at(cpu_core)), cpu_core);
  m_cpu_emulation_engine_combobox =
      new ConfigChoiceMap<PowerPC::CPUCore>(emulation_engine_choices, Config::MAIN_CPU_CORE);
  cpu_emulation_engine_layout->addRow(tr("CPU Emulation Engine:"), m_cpu_emulation_engine_combobox);

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

  auto* const timing_group = new QGroupBox(tr("Timing"));
  main_layout->addWidget(timing_group);
  auto* timing_group_layout = new QVBoxLayout{timing_group};
  auto* const correct_time_drift =
      // i18n: Correct is a verb
      new ConfigBool{tr("Correct Time Drift"), Config::MAIN_CORRECT_TIME_DRIFT};
  correct_time_drift->SetDescription(
      // i18n: Internet play refers to services like Wiimmfi, not the NetPlay feature in Dolphin
      tr("Allow the emulated console to run fast after stutters,"
         "<br>pursuing accurate overall elapsed time unless paused or speed-adjusted."
         "<br><br>This may be useful for internet play."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  timing_group_layout->addWidget(correct_time_drift);

  auto* const rush_frame_presentation =
      // i18n: "Rush" is a verb
      new ConfigBool{tr("Rush Frame Presentation"), Config::MAIN_RUSH_FRAME_PRESENTATION};
  rush_frame_presentation->SetDescription(
      tr("Limits throttling between input and frame output,"
         " speeding through emulation to reach presentation,"
         " displaying sooner, and thus reducing input latency."
         "<br><br>This will generally make frame pacing worse."
         "<br>This setting can work either with or without Immediately Present XFB."
         "<br>An Audio Buffer Size of at least 80 ms is recommended to ensure full effect."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  timing_group_layout->addWidget(rush_frame_presentation);

  auto* const smooth_early_presentation =
      // i18n: "Smooth" is a verb
      new ConfigBool{tr("Smooth Early Presentation"), Config::MAIN_SMOOTH_EARLY_PRESENTATION};
  smooth_early_presentation->SetDescription(
      tr("Adaptively adjusts the timing of early frame presentation."
         "<br><br>This can improve frame pacing with Immediately Present XFB"
         " and/or Rush Frame Presentation,"
         " while still maintaining most of the input latency benefits."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));
  timing_group_layout->addWidget(smooth_early_presentation);

  // Make all labels the same width, so that the sliders are aligned.
  const QFontMetrics font_metrics{font()};
  const int label_width = font_metrics.boundingRect(QStringLiteral(" 500% (000.00 VPS)")).width();

  auto* clock_override = new QGroupBox(tr("Clock Override"));
  auto* clock_override_layout = new QVBoxLayout();
  clock_override->setLayout(clock_override_layout);
  main_layout->addWidget(clock_override);

  m_cpu_clock_override_checkbox =
      new ConfigBool(tr("Enable Emulated CPU Clock Override"), Config::MAIN_OVERCLOCK_ENABLE);
  clock_override_layout->addWidget(m_cpu_clock_override_checkbox);
  connect(m_cpu_clock_override_checkbox, &QCheckBox::toggled, this, &AdvancedPane::Update);

  auto* cpu_clock_override_slider_layout = new QHBoxLayout();
  cpu_clock_override_slider_layout->setContentsMargins(0, 0, 0, 0);
  clock_override_layout->addLayout(cpu_clock_override_slider_layout);

  m_cpu_clock_override_slider = new ConfigFloatSlider(0.01f, 4.0f, Config::MAIN_OVERCLOCK, 0.01f);
  cpu_clock_override_slider_layout->addWidget(m_cpu_clock_override_slider);

  m_cpu_label = new QLabel();
  m_cpu_label->setFixedWidth(label_width);
  m_cpu_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  cpu_clock_override_slider_layout->addWidget(m_cpu_label);

  std::function<void()> cpu_text = [this]() {
    const float multi = Config::Get(Config::MAIN_OVERCLOCK);
    const int percent = static_cast<int>(std::round(multi * 100.f));
    const int core_clock =
        Core::System::GetInstance().GetSystemTimers().GetTicksPerSecond() / std::pow(10, 6);
    const int clock = static_cast<int>(std::round(multi * core_clock));
    m_cpu_label->setText(tr("%1% (%2 MHz)").arg(QString::number(percent), QString::number(clock)));
  };

  cpu_text();
  connect(m_cpu_clock_override_slider, &QSlider::valueChanged, this, cpu_text);

  m_cpu_clock_override_checkbox->SetDescription(
      tr("Adjusts the emulated CPU's clock rate.<br><br>"
         "On games that have an unstable frame rate despite full emulation speed, "
         "higher values can improve their performance, requiring a powerful device. "
         "Lower values reduce the emulated console's performance, but improve the "
         "emulation speed.<br><br>"
         "WARNING: Changing this from the default (100%) can and will "
         "break games and cause glitches. Do so at your own risk. "
         "Please do not report bugs that occur with a non-default clock."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));

  auto* vi_rate_override = new QGroupBox(tr("VBI Frequency Override"));
  auto* vi_rate_override_layout = new QVBoxLayout();
  vi_rate_override->setLayout(vi_rate_override_layout);
  main_layout->addWidget(vi_rate_override);

  m_vi_rate_override_checkbox =
      new ConfigBool(tr("Enable VBI Frequency Override"), Config::MAIN_VI_OVERCLOCK_ENABLE);
  vi_rate_override_layout->addWidget(m_vi_rate_override_checkbox);
  connect(m_vi_rate_override_checkbox, &QCheckBox::toggled, this, &AdvancedPane::Update);

  auto* vi_rate_override_slider_layout = new QHBoxLayout();
  vi_rate_override_slider_layout->setContentsMargins(0, 0, 0, 0);
  vi_rate_override_layout->addLayout(vi_rate_override_slider_layout);

  m_vi_rate_override_slider = new ConfigFloatSlider(0.01f, 5.0f, Config::MAIN_VI_OVERCLOCK, 0.01f);
  vi_rate_override_slider_layout->addWidget(m_vi_rate_override_slider);

  m_vi_label = new QLabel();
  m_vi_label->setFixedWidth(label_width);
  m_vi_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  vi_rate_override_slider_layout->addWidget(m_vi_label);
  std::function<void()> vi_text = [this]() {
    const int percent =
        static_cast<int>(std::round(Config::Get(Config::MAIN_VI_OVERCLOCK) * 100.f));
    float vps =
        static_cast<float>(Core::System::GetInstance().GetVideoInterface().GetTargetRefreshRate());
    if (vps == 0.0f || !Config::Get(Config::MAIN_VI_OVERCLOCK_ENABLE))
      vps = 59.94f * Config::Get(Config::MAIN_VI_OVERCLOCK);
    m_vi_label->setText(
        tr("%1% (%2 VPS)").arg(QString::number(percent), QString::number(vps, 'f', 2)));
  };

  vi_text();
  connect(m_vi_rate_override_slider, &QSlider::valueChanged, this, vi_text);

  m_vi_rate_override_checkbox->SetDescription(
      tr("Adjusts the VBI frequency. Also adjusts the emulated CPU's "
         "clock rate, to keep it relatively the same.<br><br>"
         "Makes games run at a different frame rate, making the emulation less "
         "demanding when lowered, or improving smoothness when increased. This may "
         "affect gameplay speed, as it is often tied to the frame rate.<br><br>"
         "WARNING: Changing this from the default (100%) can and will "
         "break games and cause glitches. Do so at your own risk. "
         "Please do not report bugs that occur with a non-default frequency."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));

  auto* ram_override = new QGroupBox(tr("Memory Override"));
  auto* ram_override_layout = new QVBoxLayout();
  ram_override->setLayout(ram_override_layout);
  main_layout->addWidget(ram_override);

  m_ram_override_checkbox =
      new ConfigBool(tr("Enable Emulated Memory Size Override"), Config::MAIN_RAM_OVERRIDE_ENABLE);
  ram_override_layout->addWidget(m_ram_override_checkbox);
  connect(m_ram_override_checkbox, &QCheckBox::toggled, this, &AdvancedPane::Update);

  auto* mem1_override_slider_layout = new QHBoxLayout();
  mem1_override_slider_layout->setContentsMargins(0, 0, 0, 0);
  ram_override_layout->addLayout(mem1_override_slider_layout);

  m_mem1_override_slider = new ConfigSliderU32(24, 64, Config::MAIN_MEM1_SIZE, 0x100000);
  mem1_override_slider_layout->addWidget(m_mem1_override_slider);

  m_mem1_label =
      new QLabel(tr("%1 MB (MEM1)").arg(QString::number(m_mem1_override_slider->value())));
  m_mem1_label->setFixedWidth(label_width);
  m_mem1_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mem1_override_slider_layout->addWidget(m_mem1_label);
  connect(m_mem1_override_slider, &QSlider::valueChanged, this, [this](int value) {
    m_mem1_label->setText(tr("%1 MB (MEM1)").arg(QString::number(value)));
  });

  auto* mem2_override_slider_layout = new QHBoxLayout();
  mem2_override_slider_layout->setContentsMargins(0, 0, 0, 0);
  ram_override_layout->addLayout(mem2_override_slider_layout);

  m_mem2_override_slider = new ConfigSliderU32(64, 128, Config::MAIN_MEM2_SIZE, 0x100000);
  mem2_override_slider_layout->addWidget(m_mem2_override_slider);

  m_mem2_label =
      new QLabel(tr("%1 MB (MEM2)").arg(QString::number(m_mem2_override_slider->value())));
  m_mem2_label->setFixedWidth(label_width);
  m_mem2_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  mem2_override_slider_layout->addWidget(m_mem2_label);
  connect(m_mem2_override_slider, &QSlider::valueChanged, this, [this](int value) {
    m_mem2_label->setText(tr("%1 MB (MEM2)").arg(QString::number(value)));
  });

  m_ram_override_checkbox->SetDescription(
      tr("Adjusts the amount of RAM in the emulated console.<br><br>"
         "<b>WARNING</b>: Enabling this will completely break many games.<br>Only a small "
         "number "
         "of games can benefit from this."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));

  auto* rtc_options = new QGroupBox(tr("Custom RTC Options"));
  rtc_options->setLayout(new QVBoxLayout());
  main_layout->addWidget(rtc_options);

  m_custom_rtc_checkbox = new ConfigBool(tr("Enable Custom RTC"), Config::MAIN_CUSTOM_RTC_ENABLE);
  rtc_options->layout()->addWidget(m_custom_rtc_checkbox);
  connect(m_custom_rtc_checkbox, &QCheckBox::toggled, this, &AdvancedPane::Update);

  m_custom_rtc_datetime = new QDateTimeEdit();

  // Show seconds
  m_custom_rtc_datetime->setDisplayFormat(m_custom_rtc_datetime->displayFormat().replace(
      QStringLiteral("mm"), QStringLiteral("mm:ss")));

  QtUtils::ShowFourDigitYear(m_custom_rtc_datetime);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  m_custom_rtc_datetime->setDateTimeRange(QDateTime({2000, 1, 1}, {0, 0, 0}, QTimeZone::UTC),
                                          QDateTime({2099, 12, 31}, {23, 59, 59}, QTimeZone::UTC));
#else
  m_custom_rtc_datetime->setDateTimeRange(QDateTime({2000, 1, 1}, {0, 0, 0}, Qt::UTC),
                                          QDateTime({2099, 12, 31}, {23, 59, 59}, Qt::UTC));
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  m_custom_rtc_datetime->setTimeZone(QTimeZone::UTC);
#else
  m_custom_rtc_datetime->setTimeSpec(Qt::UTC);
#endif
  rtc_options->layout()->addWidget(m_custom_rtc_datetime);

  m_custom_rtc_checkbox->SetDescription(
      tr("This setting allows you to set a custom real time clock (RTC) separate from "
         "your current system time."
         "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>"));

  auto* reset_group = new QGroupBox(tr("Reset Dolphin Settings"));
  reset_group->setLayout(new QVBoxLayout());
  main_layout->addWidget(reset_group);

  m_reset_button = new NonDefaultQPushButton(tr("Reset All Settings"));
  connect(m_reset_button, &QPushButton::clicked, this, &AdvancedPane::OnResetButtonClicked);

  reset_group->layout()->addWidget(m_reset_button);

  main_layout->addStretch(1);
}

void AdvancedPane::ConnectLayout()
{
  m_ram_override_checkbox->setChecked(Config::Get(Config::MAIN_RAM_OVERRIDE_ENABLE));
  connect(m_ram_override_checkbox, &QCheckBox::toggled, [this](bool enable_ram_override) {
    Config::SetBaseOrCurrent(Config::MAIN_RAM_OVERRIDE_ENABLE, enable_ram_override);
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
  const bool is_uninitialized = Core::IsUninitialized(Core::System::GetInstance());
  const bool enable_cpu_clock_override_widgets = Config::Get(Config::MAIN_OVERCLOCK_ENABLE);
  const bool enable_vi_rate_override_widgets = Config::Get(Config::MAIN_VI_OVERCLOCK_ENABLE);
  const bool enable_ram_override_widgets = Config::Get(Config::MAIN_RAM_OVERRIDE_ENABLE);
  const bool enable_custom_rtc_widgets =
      Config::Get(Config::MAIN_CUSTOM_RTC_ENABLE) && is_uninitialized;

  m_cpu_emulation_engine_combobox->setEnabled(is_uninitialized);
  m_enable_mmu_checkbox->setEnabled(is_uninitialized);
  m_pause_on_panic_checkbox->setEnabled(is_uninitialized);

  {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(Config::MAIN_OVERCLOCK_ENABLE) !=
               Config::LayerType::Base);

    const QSignalBlocker blocker(m_cpu_clock_override_checkbox);
    m_cpu_clock_override_checkbox->setFont(bf);
    m_cpu_clock_override_checkbox->setChecked(enable_cpu_clock_override_widgets);
  }

  m_cpu_clock_override_slider->setEnabled(enable_cpu_clock_override_widgets);
  m_cpu_label->setEnabled(enable_cpu_clock_override_widgets);

  QFont vi_bf = font();
  vi_bf.setBold(Config::GetActiveLayerForConfig(Config::MAIN_VI_OVERCLOCK_ENABLE) !=
                Config::LayerType::Base);
  m_vi_rate_override_checkbox->setFont(vi_bf);
  m_vi_rate_override_checkbox->setChecked(enable_vi_rate_override_widgets);

  m_vi_rate_override_slider->setEnabled(enable_vi_rate_override_widgets);
  m_vi_label->setEnabled(enable_vi_rate_override_widgets);

  m_ram_override_checkbox->setEnabled(is_uninitialized);
  SignalBlocking(m_ram_override_checkbox)->setChecked(enable_ram_override_widgets);

  m_mem1_override_slider->setEnabled(enable_ram_override_widgets && is_uninitialized);
  m_mem1_label->setEnabled(enable_ram_override_widgets && is_uninitialized);

  m_mem2_override_slider->setEnabled(enable_ram_override_widgets && is_uninitialized);
  m_mem2_label->setEnabled(enable_ram_override_widgets && is_uninitialized);

  m_custom_rtc_checkbox->setEnabled(is_uninitialized);
  SignalBlocking(m_custom_rtc_checkbox)->setChecked(Config::Get(Config::MAIN_CUSTOM_RTC_ENABLE));

  QDateTime initial_date_time;
  initial_date_time.setSecsSinceEpoch(Config::Get(Config::MAIN_CUSTOM_RTC_VALUE));
  m_custom_rtc_datetime->setEnabled(enable_custom_rtc_widgets);
  SignalBlocking(m_custom_rtc_datetime)->setDateTime(initial_date_time);

  m_reset_button->setEnabled(is_uninitialized);
}

void AdvancedPane::OnResetButtonClicked()
{
  if (ModalMessageBox::question(
          this, tr("Reset Dolphin Settings"),
          tr("Are you sure you want to restore all Dolphin settings to their default "
             "values? This action cannot be undone!\n"
             "All customizations or changes you have made will be lost.\n\n"
             "Do you want to proceed?"),
          ModalMessageBox::StandardButtons(ModalMessageBox::Yes | ModalMessageBox::No),
          ModalMessageBox::No, Qt::WindowModality::WindowModal) == ModalMessageBox::No)
  {
    return;
  }

  SConfig::ResetAllSettings();
  UICommon::SetUserDirectory(File::GetUserPath(D_USER_IDX));

  emit Settings::Instance().ConfigChanged();

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  ShowAnalyticsPrompt(this);
#endif
}

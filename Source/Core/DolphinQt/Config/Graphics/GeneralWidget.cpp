// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GeneralWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"
#include "DolphinQt/Config/ConfigControls/ConfigRadio.h"
#include "DolphinQt/Config/GameConfigWidget.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

GeneralWidget::GeneralWidget(GraphicsPane* gfx_pane) : m_game_layer{gfx_pane->GetConfigLayer()}
{
  CreateWidgets();
  ConnectWidgets();
  AddDescriptions();

  connect(gfx_pane, &GraphicsPane::BackendChanged, this, &GeneralWidget::OnBackendChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });
  OnEmulationStateChanged(!Core::IsUninitialized(Core::System::GetInstance()));
}

void GeneralWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Basic Section
  auto* m_video_box = new QGroupBox(tr("Basic"));
  auto* const video_layout = new QGridLayout{m_video_box};

  std::vector<std::pair<QString, QString>> options;
  for (auto& backend : VideoBackendBase::GetAvailableBackends())
  {
    options.push_back(std::make_pair(tr(backend->GetDisplayName().c_str()),
                                     QString::fromStdString(backend->GetConfigName())));
  }
  m_backend_combo = new ConfigStringChoice(options, Config::MAIN_GFX_BACKEND, m_game_layer);
  m_previous_backend = m_backend_combo->currentIndex();

  m_aspect_combo = new ConfigChoice({tr("Auto"), tr("Force 16:9"), tr("Force 4:3"),
                                     tr("Stretch to Window"), tr("Custom"), tr("Custom (Stretch)")},
                                    Config::GFX_ASPECT_RATIO, m_game_layer);
  m_custom_aspect_label = new QLabel(tr("Custom Aspect Ratio:"));
  constexpr int MAX_CUSTOM_ASPECT_RATIO_RESOLUTION = 10000;
  m_custom_aspect_width = new ConfigInteger(1, MAX_CUSTOM_ASPECT_RATIO_RESOLUTION,
                                            Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH, m_game_layer);
  m_custom_aspect_height = new ConfigInteger(1, MAX_CUSTOM_ASPECT_RATIO_RESOLUTION,
                                             Config::GFX_CUSTOM_ASPECT_RATIO_HEIGHT, m_game_layer);
  ToggleCustomAspectRatio(m_aspect_combo->currentIndex());

  m_adapter_combo = new ToolTipComboBox;
  m_enable_vsync = new ConfigBool(tr("V-Sync"), Config::GFX_VSYNC, m_game_layer);
  m_enable_fullscreen =
      new ConfigBool(tr("Start in Fullscreen"), Config::MAIN_FULLSCREEN, m_game_layer);

  video_layout->addWidget(new QLabel(tr("Backend:")), 0, 0);
  video_layout->addWidget(m_backend_combo, 0, 1, 1, -1);

  video_layout->addWidget(new QLabel(tr("Adapter:")), 1, 0);
  video_layout->addWidget(m_adapter_combo, 1, 1, 1, -1);

  video_layout->addWidget(new QLabel(tr("Aspect Ratio:")), 2, 0);
  video_layout->addWidget(m_aspect_combo, 2, 1, 1, -1);

  video_layout->addWidget(m_custom_aspect_label, 3, 0);
  video_layout->addWidget(m_custom_aspect_width, 3, 1);
  video_layout->addWidget(m_custom_aspect_height, 3, 2);

  auto* const basic_grid = new QGridLayout;
  video_layout->addLayout(basic_grid, video_layout->rowCount(), 0, 1, -1);
  basic_grid->addWidget(m_enable_vsync, 0, 0);
  basic_grid->addWidget(m_enable_fullscreen, 0, 1);

  auto* const precision_timing =
      new ConfigBool(tr("Precision Frame Timing"), Config::MAIN_PRECISION_FRAME_TIMING);
  precision_timing->SetDescription(
      tr("Uses high resolution timers and \"busy waiting\" for improved frame pacing."
         "<br><br>This will marginally increase power usage."
         "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>"));
  basic_grid->addWidget(precision_timing, 1, 0);

  // Other
  auto* m_options_box = new QGroupBox(tr("Other"));
  auto* m_options_layout = new QGridLayout();

  m_autoadjust_window_size = new ConfigBool(tr("Auto-Adjust Window Size"),
                                            Config::MAIN_RENDER_WINDOW_AUTOSIZE, m_game_layer);
  m_render_main_window =
      new ConfigBool(tr("Render to Main Window"), Config::MAIN_RENDER_TO_MAIN, m_game_layer);

  m_options_box->setLayout(m_options_layout);

  m_options_layout->addWidget(m_render_main_window, 0, 0);
  m_options_layout->addWidget(m_autoadjust_window_size, 0, 1);

  // Other
  auto* shader_compilation_box = new QGroupBox(tr("Shader Compilation"));
  auto* shader_compilation_layout = new QGridLayout();

  const std::array<const char*, 4> modes = {{
      QT_TR_NOOP("Specialized (Default)"),
      QT_TR_NOOP("Exclusive Ubershaders"),
      QT_TR_NOOP("Hybrid Ubershaders"),
      QT_TR_NOOP("Skip Drawing"),
  }};
  for (size_t i = 0; i < modes.size(); i++)
  {
    m_shader_compilation_mode[i] = new ConfigRadioInt(
        tr(modes[i]), Config::GFX_SHADER_COMPILATION_MODE, static_cast<int>(i), m_game_layer);
    shader_compilation_layout->addWidget(m_shader_compilation_mode[i], static_cast<int>(i / 2),
                                         static_cast<int>(i % 2));
  }
  m_wait_for_shaders = new ConfigBool(tr("Compile Shaders Before Starting"),
                                      Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING, m_game_layer);
  shader_compilation_layout->addWidget(m_wait_for_shaders);
  shader_compilation_box->setLayout(shader_compilation_layout);

  main_layout->addWidget(m_video_box);
  main_layout->addWidget(m_options_box);
  main_layout->addWidget(shader_compilation_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void GeneralWidget::ConnectWidgets()
{
  // Video Backend
  connect(m_backend_combo, &QComboBox::currentIndexChanged, this, &GeneralWidget::BackendWarning);
  connect(m_adapter_combo, &QComboBox::currentIndexChanged, this, [&](int index) {
    Config::SetBaseOrCurrent(Config::GFX_ADAPTER, index);
    emit BackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));
  });
  connect(m_aspect_combo, &QComboBox::currentIndexChanged, this,
          &GeneralWidget::ToggleCustomAspectRatio);
}

void GeneralWidget::ToggleCustomAspectRatio(int index)
{
  const bool is_custom_aspect_ratio = (index == static_cast<int>(AspectMode::Custom)) ||
                                      (index == static_cast<int>(AspectMode::CustomStretch));
  m_custom_aspect_label->setHidden(!is_custom_aspect_ratio);
  m_custom_aspect_width->setHidden(!is_custom_aspect_ratio);
  m_custom_aspect_height->setHidden(!is_custom_aspect_ratio);
}

void GeneralWidget::BackendWarning()
{
  if (Config::GetActiveLayerForConfig(Config::MAIN_GFX_BACKEND) == Config::LayerType::Base)
  {
    auto warningMessage = VideoBackendBase::GetAvailableBackends()[m_backend_combo->currentIndex()]
                              ->GetWarningMessage();
    if (warningMessage)
    {
      ModalMessageBox confirm_sw(this);

      confirm_sw.setIcon(QMessageBox::Warning);
      confirm_sw.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      confirm_sw.setWindowTitle(tr("Confirm backend change"));
      confirm_sw.setText(tr(warningMessage->c_str()));

      if (confirm_sw.exec() != QMessageBox::Yes)
      {
        m_backend_combo->setCurrentIndex(m_previous_backend);
        return;
      }
    }
  }

  m_previous_backend = m_backend_combo->currentIndex();
  emit BackendChanged(m_backend_combo->currentData().toString());
}

void GeneralWidget::OnEmulationStateChanged(bool running)
{
  m_backend_combo->setEnabled(!running);
  m_render_main_window->setEnabled(!running);
  m_enable_fullscreen->setEnabled(!running);

  const bool supports_adapters = !g_backend_info.Adapters.empty();
  m_adapter_combo->setEnabled(!running && supports_adapters);

  std::string current_backend = m_backend_combo->currentData().toString().toStdString();
  if (Config::Get(Config::MAIN_GFX_BACKEND) != current_backend)
  {
    {
      const QSignalBlocker blocker(m_backend_combo);
      m_backend_combo->Load();
    }
    emit BackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));
  }
}

void GeneralWidget::AddDescriptions()
{
  // We need QObject::tr
  static const char TR_BACKEND_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which graphics API to use internally.<br><br>The software renderer is extremely "
      "slow and only useful for debugging, so any of the other backends are "
      "recommended. Different games and different GPUs will behave differently on each "
      "backend, so for the best emulation experience it is recommended to try each and "
      "select the backend that is least problematic.<br><br><dolphin_emphasis>If unsure, "
      "select %1.</dolphin_emphasis>");
  static const char TR_FULLSCREEN_DESCRIPTION[] =
      QT_TR_NOOP("Uses the entire screen for rendering.<br><br>If disabled, a "
                 "render window will be created instead.<br><br><dolphin_emphasis>If "
                 "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_AUTOSIZE_DESCRIPTION[] =
      QT_TR_NOOP("Automatically adjusts the window size to the internal resolution.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_RENDER_TO_MAINWINDOW_DESCRIPTION[] =
      QT_TR_NOOP("Uses the main Dolphin window for rendering rather than "
                 "a separate render window.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_ASPECT_RATIO_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which aspect ratio to use for displaying the game."
      "<br><br>The aspect ratio of the image sent out by the original consoles varied depending on "
      "the game and rarely exactly matched 4:3 or 16:9. Some of the image would be cut off by the "
      "edges of the TV, or the image wouldn't fill the TV entirely. By default, Dolphin shows the "
      "whole image without distorting its proportions, which means it's normal for the image to "
      "not entirely fill your display."
      "<br><br><b>Auto</b>: Mimics a TV with either a 4:3 or 16:9 aspect ratio, depending on which "
      "type of TV the game seems to be targeting."
      "<br><br><b>Force 16:9</b>: Mimics a TV with a 16:9 (widescreen) aspect ratio."
      "<br><br><b>Force 4:3</b>: Mimics a TV with a 4:3 aspect ratio."
      "<br><br><b>Stretch to Window</b>: Stretches the image to the window size. "
      "This will usually distort the image's proportions."
      "<br><br><b>Custom</b>: Mimics a TV with the specified aspect ratio. "
      "This is mostly intended to be used with aspect ratio cheats/mods."
      "<br><br><b>Custom (Stretch)</b>: Similar to `Custom`, but stretches the image to the "
      "specified aspect ratio. This will usually distort the image's proportions, and should not "
      "be used under normal circumstances."
      "<br><br><dolphin_emphasis>If unsure, select Auto.</dolphin_emphasis>");
  static const char TR_VSYNC_DESCRIPTION[] = QT_TR_NOOP(
      "Waits for vertical blanks in order to prevent tearing.<br><br>Decreases performance "
      "if emulation speed is below 100%.<br><br><dolphin_emphasis>If unsure, leave "
      "this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_SHADER_COMPILE_SPECIALIZED_DESCRIPTION[] =
      QT_TR_NOOP("Ubershaders are never used. Stuttering will occur during shader "
                 "compilation, but GPU demands are low.<br><br>Recommended for low-end hardware. "
                 "<br><br><dolphin_emphasis>If unsure, select this mode.</dolphin_emphasis>");
  // The "very powerful GPU" mention below is by 2021 PC GPU standards
  static const char TR_SHADER_COMPILE_EXCLUSIVE_UBER_DESCRIPTION[] = QT_TR_NOOP(
      "Ubershaders will always be used. Provides a near stutter-free experience at the cost of "
      "very high GPU performance requirements.<br><br><dolphin_emphasis>Don't use this unless you "
      "encountered stuttering with Hybrid Ubershaders and have a very powerful "
      "GPU.</dolphin_emphasis>");
  static const char TR_SHADER_COMPILE_HYBRID_UBER_DESCRIPTION[] = QT_TR_NOOP(
      "Ubershaders will be used to prevent stuttering during shader compilation, but "
      "specialized shaders will be used when they will not cause stuttering.<br><br>In the "
      "best case it eliminates shader compilation stuttering while having minimal "
      "performance impact, but results depend on video driver behavior.");
  static const char TR_SHADER_COMPILE_SKIP_DRAWING_DESCRIPTION[] = QT_TR_NOOP(
      "Prevents shader compilation stuttering by not rendering waiting objects. Can work in "
      "scenarios where Ubershaders doesn't, at the cost of introducing visual glitches and broken "
      "effects.<br><br><dolphin_emphasis>Not recommended, only use if the other "
      "options give poor results.</dolphin_emphasis>");
  static const char TR_SHADER_COMPILE_BEFORE_START_DESCRIPTION[] =
      QT_TR_NOOP("Waits for all shaders to finish compiling before starting a game. Enabling this "
                 "option may reduce stuttering or hitching for a short time after the game is "
                 "started, at the cost of a longer delay before the game starts. For systems with "
                 "two or fewer cores, it is recommended to enable this option, as a large shader "
                 "queue may reduce frame rates.<br><br><dolphin_emphasis>Otherwise, if "
                 "unsure, leave this unchecked.</dolphin_emphasis>");

  m_backend_combo->SetTitle(tr("Backend"));
  m_backend_combo->SetDescription(
      tr(TR_BACKEND_DESCRIPTION)
          .arg(QString::fromStdString(VideoBackendBase::GetDefaultBackendDisplayName())));

  m_adapter_combo->SetTitle(tr("Adapter"));

  m_aspect_combo->SetTitle(tr("Aspect Ratio"));
  m_aspect_combo->SetDescription(tr(TR_ASPECT_RATIO_DESCRIPTION));

  m_custom_aspect_width->SetTitle(tr("Custom Aspect Ratio Width"));
  m_custom_aspect_height->SetTitle(tr("Custom Aspect Ratio Height"));

  m_enable_vsync->SetDescription(tr(TR_VSYNC_DESCRIPTION));

  m_enable_fullscreen->SetDescription(tr(TR_FULLSCREEN_DESCRIPTION));

  m_autoadjust_window_size->SetDescription(tr(TR_AUTOSIZE_DESCRIPTION));

  m_render_main_window->SetDescription(tr(TR_RENDER_TO_MAINWINDOW_DESCRIPTION));

  m_shader_compilation_mode[0]->SetDescription(tr(TR_SHADER_COMPILE_SPECIALIZED_DESCRIPTION));

  m_shader_compilation_mode[1]->SetDescription(tr(TR_SHADER_COMPILE_EXCLUSIVE_UBER_DESCRIPTION));

  m_shader_compilation_mode[2]->SetDescription(tr(TR_SHADER_COMPILE_HYBRID_UBER_DESCRIPTION));

  m_shader_compilation_mode[3]->SetDescription(tr(TR_SHADER_COMPILE_SKIP_DRAWING_DESCRIPTION));

  m_wait_for_shaders->SetDescription(tr(TR_SHADER_COMPILE_BEFORE_START_DESCRIPTION));
}

void GeneralWidget::OnBackendChanged(const QString& backend_name)
{
  const QSignalBlocker blocker(m_adapter_combo);

  m_adapter_combo->clear();

  const auto& adapters = g_backend_info.Adapters;

  for (const auto& adapter : adapters)
    m_adapter_combo->addItem(QString::fromStdString(adapter));

  const bool supports_adapters = !adapters.empty();

  const int adapter_index = Config::Get(Config::GFX_ADAPTER);
  if (adapter_index < m_adapter_combo->count())
    m_adapter_combo->setCurrentIndex(adapter_index);

  m_adapter_combo->setEnabled(supports_adapters &&
                              Core::IsUninitialized(Core::System::GetInstance()));

  static constexpr char TR_ADAPTER_AVAILABLE_DESCRIPTION[] =
      QT_TR_NOOP("Selects a hardware adapter to use.<br><br>"
                 "<dolphin_emphasis>If unsure, select the first one.</dolphin_emphasis>");
  static constexpr char TR_ADAPTER_UNAVAILABLE_DESCRIPTION[] =
      QT_TR_NOOP("Selects a hardware adapter to use.<br><br>"
                 "<dolphin_emphasis>%1 doesn't support this feature.</dolphin_emphasis>");

  m_adapter_combo->SetDescription(supports_adapters ?
                                      tr(TR_ADAPTER_AVAILABLE_DESCRIPTION) :
                                      tr(TR_ADAPTER_UNAVAILABLE_DESCRIPTION)
                                          .arg(tr(g_video_backend->GetDisplayName().c_str())));
}

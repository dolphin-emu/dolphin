// Copyright 2017 Dolphin Emulator Project5~5~5~
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsChoice.h"
#include "DolphinQt/Config/Graphics/GraphicsRadio.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

#include "UICommon/VideoUtils.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

GeneralWidget::GeneralWidget(X11Utils::XRRConfiguration* xrr_config, GraphicsWindow* parent)
    : GraphicsWidget(parent), m_xrr_config(xrr_config)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();
  emit BackendChanged(QString::fromStdString(SConfig::GetInstance().m_strVideoBackend));

  connect(parent, &GraphicsWindow::BackendChanged, this, &GeneralWidget::OnBackendChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state != Core::State::Uninitialized); });
  OnEmulationStateChanged(Core::GetState() != Core::State::Uninitialized);
}

void GeneralWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Basic Section
  auto* m_video_box = new QGroupBox(tr("Basic"));
  m_video_layout = new QGridLayout();

  m_backend_combo = new QComboBox();
  m_aspect_combo =
      new GraphicsChoice({tr("Auto"), tr("Force 16:9"), tr("Force 4:3"), tr("Stretch to Window")},
                         Config::GFX_ASPECT_RATIO);
  m_adapter_combo = new QComboBox;
  m_enable_vsync = new GraphicsBool(tr("V-Sync"), Config::GFX_VSYNC);
  m_enable_fullscreen = new GraphicsBool(tr("Use Fullscreen"), Config::MAIN_FULLSCREEN);

  m_video_box->setLayout(m_video_layout);

  for (auto& backend : g_available_video_backends)
    m_backend_combo->addItem(tr(backend->GetDisplayName().c_str()),
                             QVariant(QString::fromStdString(backend->GetName())));

  m_video_layout->addWidget(new QLabel(tr("Backend:")), 0, 0);
  m_video_layout->addWidget(m_backend_combo, 0, 1);

  m_video_layout->addWidget(new QLabel(tr("Adapter:")), 1, 0);
  m_video_layout->addWidget(m_adapter_combo, 1, 1);

  m_video_layout->addWidget(new QLabel(tr("Aspect Ratio:")), 3, 0);
  m_video_layout->addWidget(m_aspect_combo, 3, 1);

  m_video_layout->addWidget(m_enable_vsync, 4, 0);
  m_video_layout->addWidget(m_enable_fullscreen, 4, 1);

  // Other
  auto* m_options_box = new QGroupBox(tr("Other"));
  auto* m_options_layout = new QGridLayout();

  m_show_fps = new GraphicsBool(tr("Show FPS"), Config::GFX_SHOW_FPS);
  m_show_ping = new GraphicsBool(tr("Show NetPlay Ping"), Config::GFX_SHOW_NETPLAY_PING);
  m_log_render_time =
      new GraphicsBool(tr("Log Render Time to File"), Config::GFX_LOG_RENDER_TIME_TO_FILE);
  m_autoadjust_window_size =
      new GraphicsBool(tr("Auto-Adjust Window Size"), Config::MAIN_RENDER_WINDOW_AUTOSIZE);
  m_show_messages =
      new GraphicsBool(tr("Show NetPlay Messages"), Config::GFX_SHOW_NETPLAY_MESSAGES);
  m_render_main_window = new GraphicsBool(tr("Render to Main Window"), Config::MAIN_RENDER_TO_MAIN);

  m_options_box->setLayout(m_options_layout);

  m_options_layout->addWidget(m_show_fps, 0, 0);
  m_options_layout->addWidget(m_log_render_time, 0, 1);

  m_options_layout->addWidget(m_render_main_window, 1, 0);
  m_options_layout->addWidget(m_autoadjust_window_size, 1, 1);

  m_options_layout->addWidget(m_show_messages, 2, 0);
  m_options_layout->addWidget(m_show_ping, 2, 1);

  // Other
  auto* shader_compilation_box = new QGroupBox(tr("Shader Compilation"));
  auto* shader_compilation_layout = new QGridLayout();

  const std::array<const char*, 4> modes = {{
      "Synchronous",
      "Synchronous (Ubershaders)",
      "Asynchronous (Ubershaders)",
      "Asynchronous (Skip Drawing)",
  }};
  for (size_t i = 0; i < modes.size(); i++)
  {
    m_shader_compilation_mode[i] = new GraphicsRadioInt(
        tr(modes[i]), Config::GFX_SHADER_COMPILATION_MODE, static_cast<int>(i));
    shader_compilation_layout->addWidget(m_shader_compilation_mode[i], static_cast<int>(i / 2),
                                         static_cast<int>(i % 2));
  }
  m_wait_for_shaders = new GraphicsBool(tr("Compile Shaders Before Starting"),
                                        Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING);
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
  connect(m_backend_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &GeneralWidget::SaveSettings);
  connect(m_adapter_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, [](int index) {
            g_Config.iAdapter = index;
            Config::SetBaseOrCurrent(Config::GFX_ADAPTER, index);
          });
}

void GeneralWidget::LoadSettings()
{
  // Video Backend
  m_backend_combo->setCurrentIndex(m_backend_combo->findData(
      QVariant(QString::fromStdString(SConfig::GetInstance().m_strVideoBackend))));
}

void GeneralWidget::SaveSettings()
{
  // Video Backend
  const auto current_backend = m_backend_combo->currentData().toString().toStdString();
  if (SConfig::GetInstance().m_strVideoBackend != current_backend)
  {
    if (current_backend == "Software Renderer")
    {
      ModalMessageBox confirm_sw(this);

      confirm_sw.setIcon(QMessageBox::Warning);
      confirm_sw.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      confirm_sw.setWindowTitle(tr("Confirm backend change"));
      confirm_sw.setText(tr("The software renderer is significantly slower than other "
                            "backends and is only recommended for debugging purposes.\n\nDo you "
                            "really want to enable software rendering? If unsure, select 'No'."));

      if (confirm_sw.exec() != QMessageBox::Yes)
      {
        m_backend_combo->setCurrentIndex(m_backend_combo->findData(
            QVariant(QString::fromStdString(SConfig::GetInstance().m_strVideoBackend))));
        return;
      }
    }
    emit BackendChanged(QString::fromStdString(current_backend));
  }
}

void GeneralWidget::OnEmulationStateChanged(bool running)
{
  m_backend_combo->setEnabled(!running);
  m_render_main_window->setEnabled(!running);

  m_adapter_combo->setEnabled(!running);
}

void GeneralWidget::AddDescriptions()
{
// We need QObject::tr
#if defined(_WIN32)
  static const char TR_BACKEND_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which graphics API to use internally.\n\nThe software renderer is extremely "
      "slow and only useful for debugging, so either OpenGL, Direct3D, or Vulkan are "
      "recommended. Different games and different GPUs will behave differently on each "
      "backend, so for the best emulation experience it is recommended to try each and "
      "select the backend that is least problematic.\n\nIf unsure, select OpenGL.");
#else
  static const char TR_BACKEND_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which graphics API to use internally.\n\nThe software renderer is extremely "
      "slow and only useful for debugging, so any of the other backends are "
      "recommended.\n\nIf unsure, select OpenGL.");
#endif
  static const char TR_ADAPTER_DESCRIPTION[] =
      QT_TR_NOOP("Selects a hardware adapter to use.\n\nIf unsure, select the first one.");
  static const char TR_FULLSCREEN_DESCRIPTION[] =
      QT_TR_NOOP("Uses the entire screen for rendering.\n\nIf disabled, a "
                 "render window will be created instead.\n\nIf unsure, leave this unchecked.");
  static const char TR_AUTOSIZE_DESCRIPTION[] =
      QT_TR_NOOP("Automatically adjusts the window size to the internal resolution.\n\nIf unsure, "
                 "leave this unchecked.");

  static const char TR_RENDER_TO_MAINWINDOW_DESCRIPTION[] =
      QT_TR_NOOP("Uses the main Dolphin window for rendering rather than "
                 "a separate render window.\n\nIf unsure, leave this unchecked.");
  static const char TR_ASPECT_RATIO_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which aspect ratio to use when rendering.\n\nAuto: Uses the native aspect "
      "ratio\nForce 16:9: Mimics an analog TV with a widescreen aspect ratio.\nForce 4:3: "
      "Mimics a standard 4:3 analog TV.\nStretch to Window: Stretches the picture to the "
      "window size.\n\nIf unsure, select Auto.");
  static const char TR_VSYNC_DESCRIPTION[] =
      QT_TR_NOOP("Waits for vertical blanks in order to prevent tearing.\n\nDecreases performance "
                 "if emulation speed is below 100%.\n\nIf unsure, leave this unchecked.");
  static const char TR_SHOW_FPS_DESCRIPTION[] =
      QT_TR_NOOP("Shows the number of frames rendered per second as a measure of "
                 "emulation speed.\n\nIf unsure, leave this unchecked.");
  static const char TR_SHOW_NETPLAY_PING_DESCRIPTION[] =
      QT_TR_NOOP("Shows the player's maximum ping while playing on "
                 "NetPlay.\n\nIf unsure, leave this unchecked.");
  static const char TR_LOG_RENDERTIME_DESCRIPTION[] =
      QT_TR_NOOP("Logs the render time of every frame to User/Logs/render_time.txt.\n\nUse this "
                 "feature when to measure the performance of Dolphin.\n\nIf "
                 "unsure, leave this unchecked.");
  static const char TR_SHOW_NETPLAY_MESSAGES_DESCRIPTION[] =
      QT_TR_NOOP("Shows chat messages, buffer changes, and desync alerts "
                 "while playing NetPlay.\n\nIf unsure, leave this unchecked.");
  static const char TR_SHADER_COMPILE_SYNC_DESCRIPTION[] =
      QT_TR_NOOP("Ubershaders are never used. Stuttering will occur during shader "
                 "compilation, but GPU demands are low.\n\nRecommended for low-end hardware. "
                 "\n\nIf unsure, select this mode.");
  static const char TR_SHADER_COMPILE_SYNC_UBER_DESCRIPTION[] = QT_TR_NOOP(
      "Ubershaders will always be used. Provides a near stutter-free experience at the cost of "
      "high GPU performance requirements.\n\nOnly recommended for high-end systems.");
  static const char TR_SHADER_COMPILE_ASYNC_UBER_DESCRIPTION[] =
      QT_TR_NOOP("Ubershaders will be used to prevent stuttering during shader compilation, but "
                 "specialized shaders will be used when they will not cause stuttering.\n\nIn the "
                 "best case it eliminates shader compilation stuttering while having minimal "
                 "performance impact, but results depend on video driver behavior.");
  static const char TR_SHADER_COMPILE_ASYNC_SKIP_DESCRIPTION[] = QT_TR_NOOP(
      "Prevents shader compilation stuttering by not rendering waiting objects. Can work in "
      "scenarios where Ubershaders doesn't, at the cost of introducing visual glitches and broken "
      "effects.\n\nNot recommended, only use if the other options give poor results.");
  static const char TR_SHADER_COMPILE_BEFORE_START_DESCRIPTION[] =
      QT_TR_NOOP("Waits for all shaders to finish compiling before starting a game. Enabling this "
                 "option may reduce stuttering or hitching for a short time after the game is "
                 "started, at the cost of a longer delay before the game starts. For systems with "
                 "two or fewer cores, it is recommended to enable this option, as a large shader "
                 "queue may reduce frame rates.\n\nOtherwise, if unsure, leave this unchecked.");

  AddDescription(m_backend_combo, TR_BACKEND_DESCRIPTION);
  AddDescription(m_adapter_combo, TR_ADAPTER_DESCRIPTION);
  AddDescription(m_aspect_combo, TR_ASPECT_RATIO_DESCRIPTION);
  AddDescription(m_enable_vsync, TR_VSYNC_DESCRIPTION);
  AddDescription(m_enable_fullscreen, TR_FULLSCREEN_DESCRIPTION);
  AddDescription(m_show_fps, TR_SHOW_FPS_DESCRIPTION);
  AddDescription(m_show_ping, TR_SHOW_NETPLAY_PING_DESCRIPTION);
  AddDescription(m_log_render_time, TR_LOG_RENDERTIME_DESCRIPTION);
  AddDescription(m_autoadjust_window_size, TR_AUTOSIZE_DESCRIPTION);
  AddDescription(m_show_messages, TR_SHOW_NETPLAY_MESSAGES_DESCRIPTION);
  AddDescription(m_render_main_window, TR_RENDER_TO_MAINWINDOW_DESCRIPTION);
  AddDescription(m_shader_compilation_mode[0], TR_SHADER_COMPILE_SYNC_DESCRIPTION);
  AddDescription(m_shader_compilation_mode[1], TR_SHADER_COMPILE_SYNC_UBER_DESCRIPTION);
  AddDescription(m_shader_compilation_mode[2], TR_SHADER_COMPILE_ASYNC_UBER_DESCRIPTION);
  AddDescription(m_shader_compilation_mode[3], TR_SHADER_COMPILE_ASYNC_SKIP_DESCRIPTION);
  AddDescription(m_wait_for_shaders, TR_SHADER_COMPILE_BEFORE_START_DESCRIPTION);
}

void GeneralWidget::OnBackendChanged(const QString& backend_name)
{
  m_backend_combo->setCurrentIndex(m_backend_combo->findData(QVariant(backend_name)));

  const QSignalBlocker blocker(m_adapter_combo);

  m_adapter_combo->clear();

  const auto& adapters = g_Config.backend_info.Adapters;

  for (const auto& adapter : adapters)
    m_adapter_combo->addItem(QString::fromStdString(adapter));

  const bool supports_adapters = !adapters.empty();

  m_adapter_combo->setCurrentIndex(g_Config.iAdapter);
  m_adapter_combo->setEnabled(supports_adapters && !Core::IsRunning());

  m_adapter_combo->setToolTip(supports_adapters ?
                                  QStringLiteral("") :
                                  tr("%1 doesn't support this feature.")
                                      .arg(tr(g_video_backend->GetDisplayName().c_str())));
}

// Copyright 2017 Dolphin Emulator Project5~5~5~
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/GeneralWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinQt2/Config/Graphics/GraphicsBool.h"
#include "DolphinQt2/Config/Graphics/GraphicsChoice.h"
#include "DolphinQt2/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt2/Settings.h"
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
}

void GeneralWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Basic Section
  auto* m_video_box = new QGroupBox(tr("Basic"));
  m_video_layout = new QGridLayout();

  m_backend_combo = new QComboBox();
  m_resolution_combo = new QComboBox();
  m_aspect_combo =
      new GraphicsChoice({tr("Auto"), tr("Force 16:9"), tr("Force 4:3"), tr("Stretch to Window")},
                         Config::GFX_ASPECT_RATIO);
  m_adapter_combo = new GraphicsChoice({}, Config::GFX_ADAPTER);
  m_enable_vsync = new GraphicsBool(tr("V-Sync"), Config::GFX_VSYNC);
  m_enable_fullscreen = new QCheckBox(tr("Use Fullscreen"));

  m_video_box->setLayout(m_video_layout);

  for (auto& backend : g_available_video_backends)
    m_backend_combo->addItem(tr(backend->GetDisplayName().c_str()));

#ifndef __APPLE__
  m_resolution_combo->addItem(tr("Auto"));

  for (const auto& res : VideoUtils::GetAvailableResolutions(m_xrr_config))
    m_resolution_combo->addItem(QString::fromStdString(res));
#endif

  m_video_layout->addWidget(new QLabel(tr("Backend:")), 0, 0);
  m_video_layout->addWidget(m_backend_combo, 0, 1);

#ifdef _WIN32
  m_video_layout->addWidget(new QLabel(tr("Adapter:")), 1, 0);
  m_video_layout->addWidget(m_adapter_combo, 1, 1);
#endif

#ifndef __APPLE__
  m_video_layout->addWidget(new QLabel(tr("Fullscreen Resolution:")), 2, 0);
  m_video_layout->addWidget(m_resolution_combo, 2, 1);
#endif

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
  m_autoadjust_window_size = new QCheckBox(tr("Auto-Adjust Window Size"));
  m_show_messages =
      new GraphicsBool(tr("Show NetPlay Messages"), Config::GFX_SHOW_NETPLAY_MESSAGES);
  m_keep_window_top = new QCheckBox(tr("Keep Window on Top"));
  m_hide_cursor = new QCheckBox(tr("Hide Mouse Cursor"));
  m_render_main_window = new QCheckBox(tr("Render to Main Window"));

  m_options_box->setLayout(m_options_layout);

  m_options_layout->addWidget(m_show_fps, 0, 0);
  m_options_layout->addWidget(m_show_ping, 0, 1);

  m_options_layout->addWidget(m_log_render_time, 1, 0);
  m_options_layout->addWidget(m_autoadjust_window_size, 1, 1);

  m_options_layout->addWidget(m_show_messages, 2, 0);
  m_options_layout->addWidget(m_keep_window_top, 2, 1);

  m_options_layout->addWidget(m_hide_cursor, 3, 0);
  m_options_layout->addWidget(m_render_main_window, 3, 1);

  main_layout->addWidget(m_video_box);
  main_layout->addWidget(m_options_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void GeneralWidget::ConnectWidgets()
{
  // Video Backend
  connect(m_backend_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  // Fullscreen Resolution
  connect(m_resolution_combo,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  // Enable Fullscreen
  for (QCheckBox* checkbox : {m_enable_fullscreen, m_hide_cursor, m_render_main_window})
    connect(checkbox, &QCheckBox::toggled, this, &GeneralWidget::SaveSettings);
}

void GeneralWidget::LoadSettings()
{
  // Video Backend
  for (const auto& backend : g_available_video_backends)
  {
    if (backend->GetName() == SConfig::GetInstance().m_strVideoBackend)
    {
      backend->InitBackendInfo();
      m_backend_combo->setCurrentIndex(
          m_backend_combo->findText(tr(backend->GetDisplayName().c_str())));
      break;
    }
  }

  // Fullscreen Resolution
  auto resolution = SConfig::GetInstance().strFullscreenResolution;
  m_resolution_combo->setCurrentIndex(
      resolution == "Auto" ? 0 : m_resolution_combo->findText(QString::fromStdString(resolution)));
  // Enable Fullscreen
  m_enable_fullscreen->setChecked(SConfig::GetInstance().bFullscreen);
  // Hide Cursor
  m_hide_cursor->setChecked(SConfig::GetInstance().bHideCursor);
  // Render to Main Window
  m_render_main_window->setChecked(SConfig::GetInstance().bRenderToMain);
  // Keep Window on Top
  m_keep_window_top->setChecked(SConfig::GetInstance().bKeepWindowOnTop);
  // Autoadjust Window size
  m_autoadjust_window_size->setChecked(SConfig::GetInstance().bRenderWindowAutoSize);
}

void GeneralWidget::SaveSettings()
{
  // Video Backend
  for (const auto& backend : g_available_video_backends)
  {
    if (backend->GetDisplayName() == m_backend_combo->currentText().toStdString())
    {
      const auto current_backend = backend->GetName();
      if (SConfig::GetInstance().m_strVideoBackend != current_backend)
      {
        SConfig::GetInstance().m_strVideoBackend = current_backend;

        if (backend->GetName() == "Software Renderer")
        {
          QMessageBox confirm_sw;

          confirm_sw.setIcon(QMessageBox::Warning);
          confirm_sw.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
          confirm_sw.setText(
              tr("Software rendering is an order of magnitude slower than using the "
                 "other backends.\nIt's only useful for debugging purposes.\nDo you "
                 "really want to enable software rendering? If unsure, select 'No'."));

          if (confirm_sw.exec() != QMessageBox::Yes)
          {
            for (const auto& prv_backend : g_available_video_backends)
            {
              if (prv_backend->GetName() == SConfig::GetInstance().m_strVideoBackend)
              {
                m_backend_combo->setCurrentIndex(
                    m_backend_combo->findText(tr(prv_backend->GetDisplayName().c_str())));
                break;
              }
            }
            return;
          }
        }
        SConfig::GetInstance().m_strVideoBackend = current_backend;
        backend->InitBackendInfo();
        emit BackendChanged(QString::fromStdString(current_backend));
        break;
      }
    }
  }

  // Fullscreen Resolution
  SConfig::GetInstance().strFullscreenResolution =
      m_resolution_combo->currentIndex() == 0 ? "Auto" :
                                                m_resolution_combo->currentText().toStdString();
  // Enable Fullscreen
  SConfig::GetInstance().bFullscreen = m_enable_fullscreen->isChecked();
  // Hide Cursor
  SConfig::GetInstance().bHideCursor = m_hide_cursor->isChecked();
  // Render to Main Window
  SConfig::GetInstance().bRenderToMain = m_render_main_window->isChecked();
  // Keep Window on Top
  SConfig::GetInstance().bKeepWindowOnTop = m_keep_window_top->isChecked();
  // Autoadjust windowsize
  SConfig::GetInstance().bRenderWindowAutoSize = m_autoadjust_window_size->isChecked();
}

void GeneralWidget::OnEmulationStateChanged(bool running)
{
  m_backend_combo->setEnabled(!running);
  m_render_main_window->setEnabled(!running);
#ifndef __APPLE__
  m_resolution_combo->setEnabled(!running);
#endif

#ifdef _WIN32
  m_adapter_combo->setEnabled(!running);
#endif
}

void GeneralWidget::AddDescriptions()
{
// We need QObject::tr
#if defined(_WIN32)
  static const char* TR_BACKEND_DESCRIPTION =
      QT_TR_NOOP("Selects what graphics API to use internally.\nThe software renderer is extremely "
                 "slow and only useful for debugging, so you'll want to use either Direct3D or "
                 "OpenGL. Different games and different GPUs will behave differently on each "
                 "backend, so for the best emulation experience it's recommended to try both and "
                 "choose the one that's less problematic.\n\nIf unsure, select OpenGL.");
  static const char* TR_ADAPTER_DESCRIPTION =
      QT_TR_NOOP("Selects a hardware adapter to use.\n\nIf unsure, use the first one.");
#else
  static const char* TR_BACKEND_DESCRIPTION =
      QT_TR_NOOP("Selects what graphics API to use internally.\nThe software renderer is extremely "
                 "slow and only useful for debugging, so unless you have a reason to use it you'll "
                 "want to select OpenGL here.\n\nIf unsure, select OpenGL.");
#endif
  static const char* TR_RESOLUTION_DESCRIPTION =
      QT_TR_NOOP("Selects the display resolution used in fullscreen mode.\nThis should always be "
                 "bigger than or equal to the internal resolution. Performance impact is "
                 "negligible.\n\nIf unsure, select auto.");
  static const char* TR_FULLSCREEN_DESCRIPTION = QT_TR_NOOP(
      "Enable this if you want the whole screen to be used for rendering.\nIf this is disabled, a "
      "render window will be created instead.\n\nIf unsure, leave this unchecked.");
  static const char* TR_AUTOSIZE_DESCRIPTION =
      QT_TR_NOOP("Automatically adjusts the window size to your internal resolution.\n\nIf unsure, "
                 "leave this unchecked.");
  static const char* TR_KEEP_WINDOW_ON_TOP_DESCRIPTION = QT_TR_NOOP(
      "Keep the game window on top of all other windows.\n\nIf unsure, leave this unchecked.");
  static const char* TR_HIDE_MOUSE_CURSOR_DESCRIPTION =
      QT_TR_NOOP("Hides the mouse cursor if it's on top of the emulation window.\n\nIf unsure, "
                 "leave this unchecked.");
  static const char* TR_RENDER_TO_MAINWINDOW_DESCRIPTION =
      QT_TR_NOOP("Enable this if you want to use the main Dolphin window for rendering rather than "
                 "a separate render window.\n\nIf unsure, leave this unchecked.");
  static const char* TR_ASPECT_RATIO_DESCRIPTION = QT_TR_NOOP(
      "Select what aspect ratio to use when rendering:\nAuto: Use the native aspect "
      "ratio\nForce 16:9: Mimic an analog TV with a widescreen aspect ratio.\nForce 4:3: "
      "Mimic a standard 4:3 analog TV.\nStretch to Window: Stretch the picture to the "
      "window size.\n\nIf unsure, select Auto.");
  static const char* TR_VSYNC_DESCRIPTION =
      QT_TR_NOOP("Wait for vertical blanks in order to reduce tearing.\nDecreases performance if "
                 "emulation speed is below 100%.\n\nIf unsure, leave this unchecked.");
  static const char* TR_SHOW_FPS_DESCRIPTION =
      QT_TR_NOOP("Show the number of frames rendered per second as a measure of "
                 "emulation speed.\n\nIf unsure, leave this unchecked.");
  static const char* TR_SHOW_NETPLAY_PING_DESCRIPTION =
      QT_TR_NOOP("Show the players' maximum Ping while playing on "
                 "NetPlay.\n\nIf unsure, leave this unchecked.");
  static const char* TR_LOG_RENDERTIME_DESCRIPTION =
      QT_TR_NOOP("Log the render time of every frame to User/Logs/render_time.txt. Use this "
                 "feature when you want to measure the performance of Dolphin.\n\nIf "
                 "unsure, leave this unchecked.");
  static const char* TR_SHOW_NETPLAY_MESSAGES_DESCRIPTION =
      QT_TR_NOOP("When playing on NetPlay, show chat messages, buffer changes and "
                 "desync alerts.\n\nIf unsure, leave this unchecked.");

  AddDescription(m_backend_combo, TR_BACKEND_DESCRIPTION);
#ifdef _WIN32
  AddDescription(m_adapter_combo, TR_ADAPTER_DESCRIPTION);
#endif
  AddDescription(m_resolution_combo, TR_RESOLUTION_DESCRIPTION);
  AddDescription(m_enable_fullscreen, TR_FULLSCREEN_DESCRIPTION);
  AddDescription(m_autoadjust_window_size, TR_AUTOSIZE_DESCRIPTION);
  AddDescription(m_hide_cursor, TR_HIDE_MOUSE_CURSOR_DESCRIPTION);
  AddDescription(m_render_main_window, TR_RENDER_TO_MAINWINDOW_DESCRIPTION);
  AddDescription(m_aspect_combo, TR_ASPECT_RATIO_DESCRIPTION);
  AddDescription(m_enable_vsync, TR_VSYNC_DESCRIPTION);
  AddDescription(m_show_fps, TR_SHOW_FPS_DESCRIPTION);
  AddDescription(m_show_ping, TR_SHOW_NETPLAY_PING_DESCRIPTION);
  AddDescription(m_log_render_time, TR_LOG_RENDERTIME_DESCRIPTION);
  AddDescription(m_show_messages, TR_SHOW_FPS_DESCRIPTION);
  AddDescription(m_keep_window_top, TR_KEEP_WINDOW_ON_TOP_DESCRIPTION);
  AddDescription(m_show_messages, TR_SHOW_NETPLAY_MESSAGES_DESCRIPTION);
}
void GeneralWidget::OnBackendChanged(const QString& backend_name)
{
  for (const auto& backend : g_available_video_backends)
  {
    if (QString::fromStdString(backend->GetName()) == backend_name)
    {
      m_backend_combo->setCurrentIndex(
          m_backend_combo->findText(tr(backend->GetDisplayName().c_str())));
      break;
    }
  }

#ifdef _WIN32
  m_adapter_combo->clear();

  for (const auto& adapter : g_Config.backend_info.Adapters)
    m_adapter_combo->addItem(QString::fromStdString(adapter));

  m_adapter_combo->setCurrentIndex(g_Config.iAdapter);
  m_adapter_combo->setEnabled(g_Config.backend_info.Adapters.size());
#endif
}

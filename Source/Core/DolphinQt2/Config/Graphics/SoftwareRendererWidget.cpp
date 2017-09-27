// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/SoftwareRendererWidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Config/Graphics/GraphicsBool.h"
#include "DolphinQt2/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt2/Settings.h"
#include "UICommon/VideoUtils.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

SoftwareRendererWidget::SoftwareRendererWidget(GraphicsWindow* parent) : GraphicsWidget(parent)
{
  CreateWidgets();

  connect(parent, &GraphicsWindow::BackendChanged, [this] { LoadSettings(); });

  LoadSettings();
  ConnectWidgets();
  AddDescriptions();
}

void SoftwareRendererWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  auto* rendering_box = new QGroupBox(tr("Rendering"));
  auto* rendering_layout = new QGridLayout();
  m_backend_combo = new QComboBox();
  m_bypass_xfb = new GraphicsBool(tr("Bypass XFB"), Config::GFX_USE_XFB, true);

  rendering_box->setLayout(rendering_layout);
  rendering_layout->addWidget(new QLabel(tr("Backend:")), 1, 1);
  rendering_layout->addWidget(m_backend_combo, 1, 2);
  rendering_layout->addWidget(m_bypass_xfb, 2, 1);

  for (const auto& backend : g_available_video_backends)
    m_backend_combo->addItem(tr(backend->GetDisplayName().c_str()));

  auto* overlay_box = new QGroupBox(tr("Overlay Information"));
  auto* overlay_layout = new QGridLayout();
  overlay_box->setLayout(overlay_layout);
  m_enable_statistics = new GraphicsBool(tr("Various Statistics"), Config::GFX_OVERLAY_STATS);

  overlay_layout->addWidget(m_enable_statistics);

  auto* utility_box = new QGroupBox(tr("Utility"));
  auto* utility_layout = new QGridLayout();
  m_dump_textures = new GraphicsBool(tr("Dump Textures"), Config::GFX_DUMP_TEXTURES);
  m_dump_objects = new GraphicsBool(tr("Dump Objects"), Config::GFX_SW_DUMP_OBJECTS);
  utility_box->setLayout(utility_layout);

  utility_layout->addWidget(m_dump_textures, 1, 1);
  utility_layout->addWidget(m_dump_objects, 1, 2);

  auto* debug_box = new QGroupBox(tr("Debug Only"));
  auto* debug_layout = new QGridLayout();
  m_dump_tev_stages = new GraphicsBool(tr("Dump TEV Stages"), Config::GFX_SW_DUMP_TEV_STAGES);
  m_dump_tev_fetches =
      new GraphicsBool(tr("Dump Texture Fetches"), Config::GFX_SW_DUMP_TEV_TEX_FETCHES);

  debug_layout->addWidget(m_dump_tev_stages, 1, 1);
  debug_layout->addWidget(m_dump_tev_fetches, 1, 2);

  debug_box->setLayout(debug_layout);

#ifdef _DEBUG
  utility_layout->addWidget(debug_box, 2, 1, 1, 2);
#endif

  auto* object_range_box = new QGroupBox(tr("Drawn Object Range"));
  auto* object_range_layout = new QGridLayout();
  m_object_range_min = new QSpinBox();
  m_object_range_max = new QSpinBox();

  for (auto* spin : {m_object_range_min, m_object_range_max})
  {
    spin->setMinimum(0);
    spin->setMaximum(100000);
  }

  object_range_box->setLayout(object_range_layout);

  object_range_layout->addWidget(m_object_range_min, 1, 1);
  object_range_layout->addWidget(m_object_range_max, 1, 2);

  main_layout->addWidget(rendering_box);
  main_layout->addWidget(overlay_box);
  main_layout->addWidget(utility_box);
  main_layout->addWidget(object_range_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void SoftwareRendererWidget::ConnectWidgets()
{
  connect(m_backend_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  connect(m_object_range_min, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          [this](int) { SaveSettings(); });
  connect(m_object_range_max, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          [this](int) { SaveSettings(); });
}

void SoftwareRendererWidget::LoadSettings()
{
  for (const auto& backend : g_available_video_backends)
  {
    if (backend->GetName() == SConfig::GetInstance().m_strVideoBackend)
      m_backend_combo->setCurrentIndex(
          m_backend_combo->findText(tr(backend->GetDisplayName().c_str())));
  }

  m_object_range_min->setValue(Config::Get(Config::GFX_SW_DRAW_START));
  m_object_range_max->setValue(Config::Get(Config::GFX_SW_DRAW_END));
}

void SoftwareRendererWidget::SaveSettings()
{
  for (const auto& backend : g_available_video_backends)
  {
    if (tr(backend->GetDisplayName().c_str()) == m_backend_combo->currentText())
    {
      const auto backend_name = backend->GetName();
      if (backend_name != SConfig::GetInstance().m_strVideoBackend)
      {
        SConfig::GetInstance().m_strVideoBackend = backend_name;
        emit BackendChanged(QString::fromStdString(backend_name));
      }
      break;
    }
  }

  Config::SetBaseOrCurrent(Config::GFX_SW_DRAW_START, m_object_range_min->value());
  Config::SetBaseOrCurrent(Config::GFX_SW_DRAW_END, m_object_range_max->value());
}

void SoftwareRendererWidget::AddDescriptions()
{
  static const char* TR_BACKEND_DESCRIPTION =
      QT_TR_NOOP("Selects what graphics API to use internally.\nThe software renderer is extremely "
                 "slow and only useful for debugging, so you'll want to use either Direct3D or "
                 "OpenGL. Different games and different GPUs will behave differently on each "
                 "backend, so for the best emulation experience it's recommended to try both and "
                 "choose the one that's less problematic.\n\nIf unsure, select OpenGL.");

  static const char* TR_BYPASS_XFB_DESCRIPTION = QT_TR_NOOP(
      "Disable any XFB emulation.\nSpeeds up emulation a lot but causes heavy glitches in many "
      "games "
      "which rely on them (especially homebrew applications).\n\nIf unsure, leave this checked.");

  static const char* TR_SHOW_STATISTICS_DESCRIPTION =
      QT_TR_NOOP("Show various rendering statistics.\n\nIf unsure, leave this unchecked.");

  static const char* TR_DUMP_TEXTURES_DESCRIPTION =
      QT_TR_NOOP("Dump decoded game textures to User/Dump/Textures/<game_id>/.\n\nIf unsure, leave "
                 "this unchecked.");

  AddDescription(m_backend_combo, TR_BACKEND_DESCRIPTION);
  AddDescription(m_bypass_xfb, TR_BYPASS_XFB_DESCRIPTION);
  AddDescription(m_enable_statistics, TR_SHOW_STATISTICS_DESCRIPTION);
  AddDescription(m_dump_textures, TR_DUMP_TEXTURES_DESCRIPTION);
}

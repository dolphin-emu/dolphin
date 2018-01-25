// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/AdvancedWidget.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Config/Graphics/GraphicsBool.h"
#include "DolphinQt2/Config/Graphics/GraphicsChoice.h"
#include "DolphinQt2/Config/Graphics/GraphicsWindow.h"
#include "VideoCommon/VideoConfig.h"

AdvancedWidget::AdvancedWidget(GraphicsWindow* parent) : GraphicsWidget(parent)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();

  connect(parent, &GraphicsWindow::BackendChanged, this, &AdvancedWidget::OnBackendChanged);
  connect(parent, &GraphicsWindow::EmulationStarted, [this] { OnEmulationStateChanged(true); });
  connect(parent, &GraphicsWindow::EmulationStopped, [this] { OnEmulationStateChanged(false); });

  OnBackendChanged();
}

void AdvancedWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Debugging
  auto* debugging_box = new QGroupBox(tr("Debugging"));
  auto* debugging_layout = new QGridLayout();
  debugging_box->setLayout(debugging_layout);

  m_enable_wireframe = new GraphicsBool(tr("Enable Wireframe"), Config::GFX_ENABLE_WIREFRAME);
  m_show_statistics = new GraphicsBool(tr("Show Statistics"), Config::GFX_OVERLAY_STATS);
  m_enable_format_overlay =
      new GraphicsBool(tr("Texture Format Overlay"), Config::GFX_TEXFMT_OVERLAY_ENABLE);
  m_enable_api_validation =
      new GraphicsBool(tr("Enable API Validation Layers"), Config::GFX_ENABLE_VALIDATION_LAYER);

  debugging_layout->addWidget(m_enable_wireframe, 0, 0);
  debugging_layout->addWidget(m_show_statistics, 0, 1);
  debugging_layout->addWidget(m_enable_format_overlay, 1, 0);
  debugging_layout->addWidget(m_enable_api_validation, 1, 1);

  // Utility
  auto* utility_box = new QGroupBox(tr("Utility"));
  auto* utility_layout = new QGridLayout();
  utility_box->setLayout(utility_layout);

  m_dump_textures = new GraphicsBool(tr("Dump Textures"), Config::GFX_DUMP_TEXTURES);
  m_load_custom_textures = new GraphicsBool(tr("Load Custom Textures"), Config::GFX_HIRES_TEXTURES);
  m_prefetch_custom_textures =
      new GraphicsBool(tr("Prefetch Custom Textures"), Config::GFX_CACHE_HIRES_TEXTURES);
  m_use_fullres_framedumps = new GraphicsBool(tr("Full Resolution Frame Dumps"),
                                              Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS);
  m_dump_efb_target = new GraphicsBool(tr("Dump EFB Target"), Config::GFX_DUMP_EFB_TARGET);
  m_enable_freelook = new GraphicsBool(tr("Free Look"), Config::GFX_FREE_LOOK);
  m_dump_use_ffv1 = new GraphicsBool(tr("Frame Dumps Use FFV1"), Config::GFX_USE_FFV1);

  utility_layout->addWidget(m_dump_textures, 0, 0);
  utility_layout->addWidget(m_load_custom_textures, 0, 1);
  utility_layout->addWidget(m_prefetch_custom_textures, 1, 0);
  utility_layout->addWidget(m_use_fullres_framedumps, 1, 1);
  utility_layout->addWidget(m_dump_efb_target, 2, 0);
  utility_layout->addWidget(m_enable_freelook, 2, 1);
#if defined(HAVE_FFMPEG)
  utility_layout->addWidget(m_dump_use_ffv1, 3, 0);
#endif

  // Misc.
  auto* misc_box = new QGroupBox(tr("Misc"));
  auto* misc_layout = new QGridLayout();
  misc_box->setLayout(misc_layout);

  m_enable_cropping = new GraphicsBool(tr("Crop"), Config::GFX_CROP);
  m_enable_prog_scan =
      new GraphicsBool(tr("Enable Progressive Scan"), Config::GFX_HACK_FORCE_PROGRESSIVE);

  misc_layout->addWidget(m_enable_cropping, 0, 0);
  misc_layout->addWidget(m_enable_prog_scan, 0, 1);

#ifdef _WIN32
  m_borderless_fullscreen =
      new GraphicsBool(tr("Borderless Fullscreen"), Config::GFX_BORDERLESS_FULLSCREEN);

  misc_layout->addWidget(m_borderless_fullscreen, 1, -1);
#endif

  main_layout->addWidget(debugging_box);
  main_layout->addWidget(utility_box);
  main_layout->addWidget(misc_box);

  setLayout(main_layout);
}

void AdvancedWidget::ConnectWidgets()
{
  connect(m_load_custom_textures, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
}

void AdvancedWidget::LoadSettings()
{
  m_prefetch_custom_textures->setEnabled(Config::Get(Config::GFX_HIRES_TEXTURES));
}

void AdvancedWidget::SaveSettings()
{
  const auto hires_enabled = Config::Get(Config::GFX_HIRES_TEXTURES);
  m_prefetch_custom_textures->setEnabled(hires_enabled);
}

void AdvancedWidget::OnBackendChanged()
{
  const auto supports_fr_framedumps = g_Config.backend_info.bSupportsInternalResolutionFrameDumps;
  m_use_fullres_framedumps->setEnabled(supports_fr_framedumps);
}

void AdvancedWidget::OnEmulationStateChanged(bool running)
{
  m_enable_prog_scan->setEnabled(!running);
}

void AdvancedWidget::AddDescriptions()
{
  static const char* TR_WIREFRAME_DESCRIPTION =
      QT_TR_NOOP("Render the scene as a wireframe.\n\nIf unsure, leave this unchecked.");
  static const char* TR_SHOW_STATS_DESCRIPTION =
      QT_TR_NOOP("Show various rendering statistics.\n\nIf unsure, leave this unchecked.");
  static const char* TR_TEXTURE_FORMAT_DECRIPTION =
      QT_TR_NOOP("Modify textures to show the format they're encoded in. Needs an emulation reset "
                 "in most cases.\n\nIf unsure, leave this unchecked.");
  static const char* TR_VALIDATION_LAYER_DESCRIPTION =
      QT_TR_NOOP("Enables validation of API calls made by the video backend, which may assist in "
                 "debugging graphical issues.\n\nIf unsure, leave this unchecked.");
  static const char* TR_DUMP_TEXTURE_DESCRIPTION =
      QT_TR_NOOP("Dump decoded game textures to User/Dump/Textures/<game_id>/.\n\nIf unsure, leave "
                 "this unchecked.");
  static const char* TR_LOAD_CUSTOM_TEXTURE_DESCRIPTION = QT_TR_NOOP(
      "Load custom textures from User/Load/Textures/<game_id>/.\n\nIf unsure, leave this "
      "unchecked.");
  static const char* TR_CACHE_CUSTOM_TEXTURE_DESCRIPTION =
      QT_TR_NOOP("Cache custom textures to system RAM on startup.\nThis can require exponentially "
                 "more RAM but fixes possible stuttering.\n\nIf unsure, leave this unchecked.");
  static const char* TR_DUMP_EFB_DESCRIPTION =
      QT_TR_NOOP("Dump the contents of EFB copies to User/Dump/Textures/.\n\nIf unsure, leave this "
                 "unchecked.");
  static const char* TR_INTERNAL_RESOLUTION_FRAME_DUMPING_DESCRIPTION = QT_TR_NOOP(
      "Create frame dumps and screenshots at the internal resolution of the renderer, rather than "
      "the size of the window it is displayed within. If the aspect ratio is widescreen, the "
      "output "
      "image will be scaled horizontally to preserve the vertical resolution.\n\nIf unsure, leave "
      "this unchecked.");
#if defined(HAVE_FFMPEG)
  static const char* TR_USE_FFV1_DESCRIPTION =
      QT_TR_NOOP("Encode frame dumps using the FFV1 codec.\n\nIf unsure, leave this unchecked.");
#endif
  static const char* TR_FREE_LOOK_DESCRIPTION = QT_TR_NOOP(
      "This feature allows you to change the game's camera.\nMove the mouse while holding the "
      "right "
      "mouse button to pan and while holding the middle button to move.\nHold SHIFT and press "
      "one of "
      "the WASD keys to move the camera by a certain step distance (SHIFT+2 to move faster and "
      "SHIFT+1 to move slower). Press SHIFT+R to reset the camera and SHIFT+F to reset the "
      "speed.\n\nIf unsure, leave this unchecked.");
  static const char* TR_CROPPING_DESCRIPTION =
      QT_TR_NOOP("Crop the picture from its native aspect ratio to 4:3 or "
                 "16:9.\n\nIf unsure, leave this unchecked.");
  static const char* TR_PROGRESSIVE_SCAN_DESCRIPTION = QT_TR_NOOP(
      "Enables progressive scan if supported by the emulated software.\nMost games don't "
      "care about this.\n\nIf unsure, leave this unchecked.");
#ifdef _WIN32
  static const char* TR_BORDERLESS_FULLSCREEN_DESCRIPTION = QT_TR_NOOP(
      "Implement fullscreen mode with a borderless window spanning the whole screen instead of "
      "using "
      "exclusive mode.\nAllows for faster transitions between fullscreen and windowed mode, but "
      "slightly increases input latency, makes movement less smooth and slightly decreases "
      "performance.\nExclusive mode is required for Nvidia 3D Vision to work in the Direct3D "
      "backend.\n\nIf unsure, leave this unchecked.");
#endif

  AddDescription(m_enable_wireframe, TR_WIREFRAME_DESCRIPTION);
  AddDescription(m_show_statistics, TR_SHOW_STATS_DESCRIPTION);
  AddDescription(m_enable_format_overlay, TR_TEXTURE_FORMAT_DECRIPTION);
  AddDescription(m_enable_api_validation, TR_VALIDATION_LAYER_DESCRIPTION);
  AddDescription(m_dump_textures, TR_DUMP_TEXTURE_DESCRIPTION);
  AddDescription(m_load_custom_textures, TR_LOAD_CUSTOM_TEXTURE_DESCRIPTION);
  AddDescription(m_prefetch_custom_textures, TR_CACHE_CUSTOM_TEXTURE_DESCRIPTION);
  AddDescription(m_dump_efb_target, TR_DUMP_EFB_DESCRIPTION);
  AddDescription(m_use_fullres_framedumps, TR_INTERNAL_RESOLUTION_FRAME_DUMPING_DESCRIPTION);
#ifdef HAVE_FFMPEG
  AddDescription(m_dump_use_ffv1, TR_USE_FFV1_DESCRIPTION);
#endif
  AddDescription(m_enable_cropping, TR_CROPPING_DESCRIPTION);
  AddDescription(m_enable_prog_scan, TR_PROGRESSIVE_SCAN_DESCRIPTION);
  AddDescription(m_enable_freelook, TR_FREE_LOOK_DESCRIPTION);
#ifdef _WIN32
  AddDescription(m_borderless_fullscreen, TR_BORDERLESS_FULLSCREEN_DESCRIPTION);
#endif
}

// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/AdvancedWidget.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsChoice.h"
#include "DolphinQt/Config/Graphics/GraphicsInteger.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoConfig.h"

AdvancedWidget::AdvancedWidget(GraphicsWindow* parent) : GraphicsWidget(parent)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();

  connect(parent, &GraphicsWindow::BackendChanged, this, &AdvancedWidget::OnBackendChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state != Core::State::Uninitialized); });

  OnBackendChanged();
  OnEmulationStateChanged(Core::GetState() != Core::State::Uninitialized);
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
  m_dump_efb_target = new GraphicsBool(tr("Dump EFB Target"), Config::GFX_DUMP_EFB_TARGET);
  m_disable_vram_copies =
      new GraphicsBool(tr("Disable EFB VRAM Copies"), Config::GFX_HACK_DISABLE_COPY_TO_VRAM);
  m_enable_freelook = new GraphicsBool(tr("Free Look"), Config::GFX_FREE_LOOK);

  utility_layout->addWidget(m_load_custom_textures, 0, 0);
  utility_layout->addWidget(m_prefetch_custom_textures, 0, 1);

  utility_layout->addWidget(m_enable_freelook, 1, 0);
  utility_layout->addWidget(m_disable_vram_copies, 1, 1);

  utility_layout->addWidget(m_dump_textures, 2, 0);
  utility_layout->addWidget(m_dump_efb_target, 2, 1);

  // Frame dumping
  auto* dump_box = new QGroupBox(tr("Frame Dumping"));
  auto* dump_layout = new QGridLayout();
  dump_box->setLayout(dump_layout);

  m_use_fullres_framedumps = new GraphicsBool(tr("Dump at Internal Resolution"),
                                              Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS);
  m_dump_use_ffv1 = new GraphicsBool(tr("Use Lossless Codec (FFV1)"), Config::GFX_USE_FFV1);
  m_dump_bitrate = new GraphicsInteger(0, 1000000, Config::GFX_BITRATE_KBPS, 1000);

  dump_layout->addWidget(m_use_fullres_framedumps, 0, 0);
#if defined(HAVE_FFMPEG)
  dump_layout->addWidget(m_dump_use_ffv1, 0, 1);
  dump_layout->addWidget(new QLabel(tr("Bitrate (kbps):")), 1, 0);
  dump_layout->addWidget(m_dump_bitrate, 1, 1);
#endif

  // Misc.
  auto* misc_box = new QGroupBox(tr("Misc"));
  auto* misc_layout = new QGridLayout();
  misc_box->setLayout(misc_layout);

  m_enable_cropping = new GraphicsBool(tr("Crop"), Config::GFX_CROP);
  m_enable_prog_scan = new QCheckBox(tr("Enable Progressive Scan"));
  m_backend_multithreading =
      new GraphicsBool(tr("Backend Multithreading"), Config::GFX_BACKEND_MULTITHREADING);

  misc_layout->addWidget(m_enable_cropping, 0, 0);
  misc_layout->addWidget(m_enable_prog_scan, 0, 1);
  misc_layout->addWidget(m_backend_multithreading, 1, 0);
#ifdef _WIN32
  m_borderless_fullscreen =
      new GraphicsBool(tr("Borderless Fullscreen"), Config::GFX_BORDERLESS_FULLSCREEN);

  misc_layout->addWidget(m_borderless_fullscreen, 1, 1);
#endif

  // Experimental.
  auto* experimental_box = new QGroupBox(tr("Experimental"));
  auto* experimental_layout = new QGridLayout();
  experimental_box->setLayout(experimental_layout);

  m_defer_efb_access_invalidation =
      new GraphicsBool(tr("Defer EFB Cache Invalidation"), Config::GFX_HACK_EFB_DEFER_INVALIDATION);

  experimental_layout->addWidget(m_defer_efb_access_invalidation, 0, 0);

  main_layout->addWidget(debugging_box);
  main_layout->addWidget(utility_box);
  main_layout->addWidget(dump_box);
  main_layout->addWidget(misc_box);
  main_layout->addWidget(experimental_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void AdvancedWidget::ConnectWidgets()
{
  connect(m_load_custom_textures, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
  connect(m_dump_use_ffv1, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
  connect(m_enable_prog_scan, &QCheckBox::toggled, this, &AdvancedWidget::SaveSettings);
}

void AdvancedWidget::LoadSettings()
{
  m_prefetch_custom_textures->setEnabled(Config::Get(Config::GFX_HIRES_TEXTURES));
  m_dump_bitrate->setEnabled(!Config::Get(Config::GFX_USE_FFV1));

  m_enable_prog_scan->setChecked(Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN));
}

void AdvancedWidget::SaveSettings()
{
  m_prefetch_custom_textures->setEnabled(Config::Get(Config::GFX_HIRES_TEXTURES));
  m_dump_bitrate->setEnabled(!Config::Get(Config::GFX_USE_FFV1));

  Config::SetBase(Config::SYSCONF_PROGRESSIVE_SCAN, m_enable_prog_scan->isChecked());
}

void AdvancedWidget::OnBackendChanged()
{
}

void AdvancedWidget::OnEmulationStateChanged(bool running)
{
  m_enable_prog_scan->setEnabled(!running);
}

void AdvancedWidget::AddDescriptions()
{
  static const char TR_WIREFRAME_DESCRIPTION[] =
      QT_TR_NOOP("Renders the scene as a wireframe.\n\nIf unsure, leave this unchecked.");
  static const char TR_SHOW_STATS_DESCRIPTION[] =
      QT_TR_NOOP("Shows various rendering statistics.\n\nIf unsure, leave this unchecked.");
  static const char TR_TEXTURE_FORMAT_DESCRIPTION[] = QT_TR_NOOP(
      "Modifies textures to show the format they're encoded in.\n\nMay require an emulation "
      "reset to apply.\n\nIf unsure, leave this unchecked.");
  static const char TR_VALIDATION_LAYER_DESCRIPTION[] =
      QT_TR_NOOP("Enables validation of API calls made by the video backend, which may assist in "
                 "debugging graphical issues.\n\nIf unsure, leave this unchecked.");
  static const char TR_DUMP_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Dumps decoded game textures to User/Dump/Textures/<game_id>/.\n\nIf unsure, leave "
      "this unchecked.");
  static const char TR_LOAD_CUSTOM_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Loads custom textures from User/Load/Textures/<game_id>/.\n\nIf unsure, leave this "
      "unchecked.");
  static const char TR_CACHE_CUSTOM_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Caches custom textures to system RAM on startup.\n\nThis can require exponentially "
      "more RAM but fixes possible stuttering.\n\nIf unsure, leave this unchecked.");
  static const char TR_DUMP_EFB_DESCRIPTION[] = QT_TR_NOOP(
      "Dumps the contents of EFB copies to User/Dump/Textures/.\n\nIf unsure, leave this "
      "unchecked.");
  static const char TR_DISABLE_VRAM_COPIES_DESCRIPTION[] =
      QT_TR_NOOP("Disables the VRAM copy of the EFB, forcing a round-trip to RAM. Inhibits all "
                 "upscaling.\n\nIf unsure, leave this unchecked.");
  static const char TR_INTERNAL_RESOLUTION_FRAME_DUMPING_DESCRIPTION[] = QT_TR_NOOP(
      "Creates frame dumps and screenshots at the internal resolution of the renderer, rather than "
      "the size of the window it is displayed within.\n\nIf the aspect ratio is widescreen, the "
      "output image will be scaled horizontally to preserve the vertical resolution.\n\nIf "
      "unsure, leave this unchecked.");
#if defined(HAVE_FFMPEG)
  static const char TR_USE_FFV1_DESCRIPTION[] =
      QT_TR_NOOP("Encodes frame dumps using the FFV1 codec.\n\nIf unsure, leave this unchecked.");
#endif
  static const char TR_FREE_LOOK_DESCRIPTION[] = QT_TR_NOOP(
      "Allows manipulation of the in-game camera. Move the mouse while holding the right button "
      "to pan or middle button to roll.\n\nUse the WASD keys while holding SHIFT to move the "
      "camera. Press SHIFT+2 to increase speed or SHIFT+1 to decrease speed. Press SHIFT+R "
      "to reset the camera or SHIFT+F to reset the speed.\n\nIf unsure, leave this unchecked. ");
  static const char TR_CROPPING_DESCRIPTION[] =
      QT_TR_NOOP("Crops the picture from its native aspect ratio to 4:3 or "
                 "16:9.\n\nIf unsure, leave this unchecked.");
  static const char TR_PROGRESSIVE_SCAN_DESCRIPTION[] = QT_TR_NOOP(
      "Enables progressive scan if supported by the emulated software. Most games don't have "
      "any issue with this.\n\nIf unsure, leave this unchecked.");
  static const char TR_BACKEND_MULTITHREADING_DESCRIPTION[] =
      QT_TR_NOOP("Enables multithreaded command submission in backends where supported. Enabling "
                 "this option may result in a performance improvement on systems with more than "
                 "two CPU cores. Currently, this is limited to the Vulkan backend.\n\nIf unsure, "
                 "leave this checked.");
  static const char TR_DEFER_EFB_ACCESS_INVALIDATION_DESCRIPTION[] = QT_TR_NOOP(
      "Defers invalidation of the EFB access cache until a GPU synchronization command "
      "is executed. If disabled, the cache will be invalidated with every draw call. "
      "\n\nMay improve performance in some games which rely on CPU EFB Access at the cost "
      "of stability.\n\nIf unsure, leave this unchecked.");

#ifdef _WIN32
  static const char TR_BORDERLESS_FULLSCREEN_DESCRIPTION[] = QT_TR_NOOP(
      "Implements fullscreen mode with a borderless window spanning the whole screen instead of "
      "using exclusive mode. Allows for faster transitions between fullscreen and windowed mode, "
      "but slightly increases input latency, makes movement less smooth and slightly decreases "
      "performance.\n\nExclusive mode is required for Nvidia 3D Vision to work in the Direct3D "
      "backend.\n\nIf unsure, leave this unchecked.");
#endif

  AddDescription(m_enable_wireframe, TR_WIREFRAME_DESCRIPTION);
  AddDescription(m_show_statistics, TR_SHOW_STATS_DESCRIPTION);
  AddDescription(m_enable_format_overlay, TR_TEXTURE_FORMAT_DESCRIPTION);
  AddDescription(m_enable_api_validation, TR_VALIDATION_LAYER_DESCRIPTION);
  AddDescription(m_dump_textures, TR_DUMP_TEXTURE_DESCRIPTION);
  AddDescription(m_load_custom_textures, TR_LOAD_CUSTOM_TEXTURE_DESCRIPTION);
  AddDescription(m_prefetch_custom_textures, TR_CACHE_CUSTOM_TEXTURE_DESCRIPTION);
  AddDescription(m_dump_efb_target, TR_DUMP_EFB_DESCRIPTION);
  AddDescription(m_disable_vram_copies, TR_DISABLE_VRAM_COPIES_DESCRIPTION);
  AddDescription(m_use_fullres_framedumps, TR_INTERNAL_RESOLUTION_FRAME_DUMPING_DESCRIPTION);
#ifdef HAVE_FFMPEG
  AddDescription(m_dump_use_ffv1, TR_USE_FFV1_DESCRIPTION);
#endif
  AddDescription(m_enable_cropping, TR_CROPPING_DESCRIPTION);
  AddDescription(m_enable_prog_scan, TR_PROGRESSIVE_SCAN_DESCRIPTION);
  AddDescription(m_enable_freelook, TR_FREE_LOOK_DESCRIPTION);
  AddDescription(m_backend_multithreading, TR_BACKEND_MULTITHREADING_DESCRIPTION);
#ifdef _WIN32
  AddDescription(m_borderless_fullscreen, TR_BORDERLESS_FULLSCREEN_DESCRIPTION);
#endif
  AddDescription(m_defer_efb_access_invalidation, TR_DEFER_EFB_ACCESS_INVALIDATION_DESCRIPTION);
}

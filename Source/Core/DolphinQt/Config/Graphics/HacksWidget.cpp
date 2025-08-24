// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/HacksWidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"
#include "DolphinQt/Config/GameConfigWidget.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"

#include "VideoCommon/VideoConfig.h"

HacksWidget::HacksWidget(GraphicsPane* gfx_pane) : m_game_layer{gfx_pane->GetConfigLayer()}
{
  CreateWidgets();
  ConnectWidgets();
  AddDescriptions();

  connect(gfx_pane, &GraphicsPane::BackendChanged, this, &HacksWidget::OnBackendChanged);
  OnBackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));
  connect(m_gpu_texture_decoding, &QCheckBox::toggled,
          [gfx_pane] { emit gfx_pane->UseGPUTextureDecodingChanged(); });
}

void HacksWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // EFB
  auto* efb_box = new QGroupBox(tr("Embedded Frame Buffer (EFB)"));
  auto* efb_layout = new QGridLayout();
  efb_box->setLayout(efb_layout);
  m_skip_efb_cpu = new ConfigBool(tr("Skip EFB Access from CPU"),
                                  Config::GFX_HACK_EFB_ACCESS_ENABLE, m_game_layer, true);
  m_ignore_format_changes = new ConfigBool(
      tr("Ignore Format Changes"), Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, m_game_layer, true);
  m_store_efb_copies = new ConfigBool(tr("Store EFB Copies to Texture Only"),
                                      Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, m_game_layer);
  m_defer_efb_copies = new ConfigBool(tr("Defer EFB Copies to RAM"),
                                      Config::GFX_HACK_DEFER_EFB_COPIES, m_game_layer);

  efb_layout->addWidget(m_skip_efb_cpu, 0, 0);
  efb_layout->addWidget(m_ignore_format_changes, 0, 1);
  efb_layout->addWidget(m_store_efb_copies, 1, 0);
  efb_layout->addWidget(m_defer_efb_copies, 1, 1);

  // Texture Cache
  auto* texture_cache_box = new QGroupBox(tr("Texture Cache"));
  auto* texture_cache_layout = new QGridLayout();
  texture_cache_box->setLayout(texture_cache_layout);

  m_accuracy =
      new ConfigSlider({0, 512, 128}, Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, m_game_layer);
  m_gpu_texture_decoding = new ConfigBool(tr("GPU Texture Decoding"),
                                          Config::GFX_ENABLE_GPU_TEXTURE_DECODING, m_game_layer);

  auto* safe_label = new QLabel(tr("Safe"));
  safe_label->setAlignment(Qt::AlignRight);

  m_accuracy_label = new ConfigSliderLabel(tr("Accuracy:"), m_accuracy);

  texture_cache_layout->addWidget(m_accuracy_label, 0, 0);
  texture_cache_layout->addWidget(safe_label, 0, 1);
  texture_cache_layout->addWidget(m_accuracy, 0, 2);
  texture_cache_layout->addWidget(new QLabel(tr("Fast")), 0, 3);
  texture_cache_layout->addWidget(m_gpu_texture_decoding, 1, 0);

  // XFB
  auto* xfb_box = new QGroupBox(tr("External Frame Buffer (XFB)"));
  auto* xfb_layout = new QVBoxLayout();
  xfb_box->setLayout(xfb_layout);

  m_store_xfb_copies = new ConfigBool(tr("Store XFB Copies to Texture Only"),
                                      Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, m_game_layer);
  m_immediate_xfb =
      new ConfigBool(tr("Immediately Present XFB"), Config::GFX_HACK_IMMEDIATE_XFB, m_game_layer);
  m_skip_duplicate_xfbs = new ConfigBool(tr("Skip Presenting Duplicate Frames"),
                                         Config::GFX_HACK_SKIP_DUPLICATE_XFBS, m_game_layer);

  xfb_layout->addWidget(m_store_xfb_copies);
  xfb_layout->addWidget(m_immediate_xfb);
  xfb_layout->addWidget(m_skip_duplicate_xfbs);

  // Other
  auto* other_box = new QGroupBox(tr("Other"));
  auto* other_layout = new QGridLayout();
  other_box->setLayout(other_layout);

  m_fast_depth_calculation =
      new ConfigBool(tr("Fast Depth Calculation"), Config::GFX_FAST_DEPTH_CALC, m_game_layer);
  m_disable_bounding_box =
      new ConfigBool(tr("Disable Bounding Box"), Config::GFX_HACK_BBOX_ENABLE, m_game_layer, true);
  m_vertex_rounding =
      new ConfigBool(tr("Vertex Rounding"), Config::GFX_HACK_VERTEX_ROUNDING, m_game_layer);
  m_save_texture_cache_state = new ConfigBool(
      tr("Save Texture Cache to State"), Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE, m_game_layer);
  m_vi_skip = new ConfigBool(tr("VBI Skip"), Config::GFX_HACK_VI_SKIP, m_game_layer);

  other_layout->addWidget(m_fast_depth_calculation, 0, 0);
  other_layout->addWidget(m_disable_bounding_box, 0, 1);
  other_layout->addWidget(m_vertex_rounding, 1, 0);
  other_layout->addWidget(m_save_texture_cache_state, 1, 1);
  other_layout->addWidget(m_vi_skip, 2, 0);

  main_layout->addWidget(efb_box);
  main_layout->addWidget(texture_cache_box);
  main_layout->addWidget(xfb_box);
  main_layout->addWidget(other_box);
  main_layout->addStretch();

  setLayout(main_layout);

  UpdateDeferEFBCopiesEnabled();
  UpdateSkipPresentingDuplicateFramesEnabled();
}

void HacksWidget::OnBackendChanged(const QString& backend_name)
{
  const bool bbox = g_backend_info.bSupportsBBox;
  const bool gpu_texture_decoding = g_backend_info.bSupportsGPUTextureDecoding;

  m_gpu_texture_decoding->setEnabled(gpu_texture_decoding);
  m_disable_bounding_box->setEnabled(bbox);

  const QString tooltip = tr("%1 doesn't support this feature on your system.")
                              .arg(tr(backend_name.toStdString().c_str()));

  m_gpu_texture_decoding->setToolTip(!gpu_texture_decoding ? tooltip : QString{});
  m_disable_bounding_box->setToolTip(!bbox ? tooltip : QString{});
}

void HacksWidget::ConnectWidgets()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(m_store_efb_copies, &QCheckBox::checkStateChanged,
          [this](Qt::CheckState) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_store_xfb_copies, &QCheckBox::checkStateChanged,
          [this](Qt::CheckState) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_immediate_xfb, &QCheckBox::checkStateChanged,
          [this](Qt::CheckState) { UpdateSkipPresentingDuplicateFramesEnabled(); });
  connect(m_vi_skip, &QCheckBox::checkStateChanged,
          [this](Qt::CheckState) { UpdateSkipPresentingDuplicateFramesEnabled(); });
#else
  connect(m_store_efb_copies, &QCheckBox::stateChanged,
          [this](int) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_store_xfb_copies, &QCheckBox::stateChanged,
          [this](int) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_immediate_xfb, &QCheckBox::stateChanged,
          [this](int) { UpdateSkipPresentingDuplicateFramesEnabled(); });
  connect(m_vi_skip, &QCheckBox::stateChanged,
          [this](int) { UpdateSkipPresentingDuplicateFramesEnabled(); });
#endif
}

void HacksWidget::AddDescriptions()
{
  static const char TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION[] = QT_TR_NOOP(
      "Ignores any requests from the CPU to read from or write to the EFB. "
      "<br><br>Improves performance in some games, but will disable all EFB-based "
      "graphical effects or gameplay-related features.<br><br><dolphin_emphasis>If unsure, "
      "leave this checked.</dolphin_emphasis>");
  static const char TR_IGNORE_FORMAT_CHANGE_DESCRIPTION[] = QT_TR_NOOP(
      "Ignores any changes to the EFB format.<br><br>Improves performance in many games "
      "without "
      "any negative effect. Causes graphical defects in a small number of other "
      "games.<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_STORE_EFB_TO_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Stores EFB copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
      "in a small number of games.<br><br>Enabled = EFB Copies to Texture<br>Disabled = EFB "
      "Copies to "
      "RAM (and Texture)<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_DEFER_EFB_COPIES_DESCRIPTION[] = QT_TR_NOOP(
      "Waits until the game synchronizes with the emulated GPU before writing the contents of EFB "
      "copies to RAM.<br><br>Reduces the overhead of EFB RAM copies, providing a performance "
      "boost in "
      "many games, at the risk of breaking those which do not safely synchronize with the "
      "emulated GPU.<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_ACCUARCY_DESCRIPTION[] = QT_TR_NOOP(
      "Adjusts the accuracy at which the GPU receives texture updates from RAM.<br><br>"
      "The \"Safe\" setting eliminates the likelihood of the GPU missing texture updates "
      "from RAM. Lower accuracies cause in-game text to appear garbled in certain "
      "games.<br><br><dolphin_emphasis>If unsure, select the rightmost "
      "value.</dolphin_emphasis>");
  static const char TR_STORE_XFB_TO_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Stores XFB copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
      "in a small number of games.<br><br>Enabled = XFB Copies to "
      "Texture<br>Disabled = XFB Copies to RAM (and Texture)<br><br><dolphin_emphasis>If "
      "unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_IMMEDIATE_XFB_DESCRIPTION[] = QT_TR_NOOP(
      "Displays XFB copies as soon as they are created, instead of waiting for "
      "scanout.<br><br>Can cause graphical defects in some games if the game doesn't "
      "expect all XFB copies to be displayed. However, turning this setting on reduces latency."
      "<br><br>Enabling this also forces an effect equivalent to the "
      "Skip Presenting Duplicate Frames setting."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_SKIP_DUPLICATE_XFBS_DESCRIPTION[] = QT_TR_NOOP(
      "Skips presentation of duplicate frames (XFB copies) in 25fps/30fps games. "
      "This may improve performance on low-end devices, while making frame pacing less consistent."
      "<br><br>Disable this option for optimal frame pacing."
      "<br><br>This setting is unavailable when Immediately Present XFB or VBI Skip is "
      "enabled. In those cases, duplicate frames are never presented."
      "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_GPU_DECODING_DESCRIPTION[] = QT_TR_NOOP(
      "Enables texture decoding using the GPU instead of the CPU.<br><br>This may result in "
      "performance gains in some scenarios, or on systems where the CPU is the "
      "bottleneck.<br><br>If this setting is enabled, Arbitrary Mipmap Detection will be "
      "disabled.<br><br>"
      "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_FAST_DEPTH_CALC_DESCRIPTION[] = QT_TR_NOOP(
      "Uses a less accurate algorithm to calculate depth values.<br><br>Causes issues in a few "
      "games, but can result in a decent speed increase depending on the game and/or "
      "GPU.<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_DISABLE_BOUNDINGBOX_DESCRIPTION[] =
      QT_TR_NOOP("Disables bounding box emulation.<br><br>This may improve GPU performance "
                 "significantly, but some games will break.<br><br><dolphin_emphasis>If "
                 "unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_SAVE_TEXTURE_CACHE_TO_STATE_DESCRIPTION[] =
      QT_TR_NOOP("Includes the contents of the embedded frame buffer (EFB) and upscaled EFB copies "
                 "in save states. Fixes missing and/or non-upscaled textures/objects when loading "
                 "states at the cost of additional save/load time.<br><br><dolphin_emphasis>If "
                 "unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_VERTEX_ROUNDING_DESCRIPTION[] = QT_TR_NOOP(
      "Rounds 2D vertices to whole pixels and rounds the viewport size to a whole number.<br><br>"
      "Fixes graphical problems in some games at higher internal resolutions. This setting has no "
      "effect when native internal resolution is used.<br><br>"
      "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_VI_SKIP_DESCRIPTION[] =
      QT_TR_NOOP("Skips Vertical Blank Interrupts when lag is detected, allowing for "
                 "smooth audio playback when emulation speed is not 100%. <br><br>"
                 "Enabling this also forces the effect of the"
                 " Skip Presenting Duplicate Frames setting.<br><br>"
                 "<dolphin_emphasis>WARNING: Can cause freezes and compatibility "
                 "issues.</dolphin_emphasis> <br><br>"
                 "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  m_skip_efb_cpu->SetDescription(tr(TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION));
  m_ignore_format_changes->SetDescription(tr(TR_IGNORE_FORMAT_CHANGE_DESCRIPTION));
  m_store_efb_copies->SetDescription(tr(TR_STORE_EFB_TO_TEXTURE_DESCRIPTION));
  m_defer_efb_copies->SetDescription(tr(TR_DEFER_EFB_COPIES_DESCRIPTION));
  m_accuracy->SetTitle(tr("Texture Cache Accuracy"));
  m_accuracy->SetDescription(tr(TR_ACCUARCY_DESCRIPTION));
  m_store_xfb_copies->SetDescription(tr(TR_STORE_XFB_TO_TEXTURE_DESCRIPTION));
  m_immediate_xfb->SetDescription(tr(TR_IMMEDIATE_XFB_DESCRIPTION));
  m_skip_duplicate_xfbs->SetDescription(tr(TR_SKIP_DUPLICATE_XFBS_DESCRIPTION));
  m_gpu_texture_decoding->SetDescription(tr(TR_GPU_DECODING_DESCRIPTION));
  m_fast_depth_calculation->SetDescription(tr(TR_FAST_DEPTH_CALC_DESCRIPTION));
  m_disable_bounding_box->SetDescription(tr(TR_DISABLE_BOUNDINGBOX_DESCRIPTION));
  m_save_texture_cache_state->SetDescription(tr(TR_SAVE_TEXTURE_CACHE_TO_STATE_DESCRIPTION));
  m_vertex_rounding->SetDescription(tr(TR_VERTEX_ROUNDING_DESCRIPTION));
  m_vi_skip->SetDescription(tr(TR_VI_SKIP_DESCRIPTION));
}

void HacksWidget::UpdateDeferEFBCopiesEnabled()
{
  // We disable the checkbox for defer EFB copies when both EFB and XFB copies to texture are
  // enabled.
  const bool can_defer = m_store_efb_copies->isChecked() && m_store_xfb_copies->isChecked();
  m_defer_efb_copies->setEnabled(!can_defer);
}

void HacksWidget::UpdateSkipPresentingDuplicateFramesEnabled()
{
  // If Immediate XFB is on, there's no point to skipping duplicate XFB copies as immediate presents
  // when the XFB is created, therefore all XFB copies will be unique.
  m_skip_duplicate_xfbs->setDisabled(m_immediate_xfb->isChecked() || m_vi_skip->isChecked());
}

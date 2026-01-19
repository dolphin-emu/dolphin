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
  ConnectWidgets(gfx_pane);
  AddDescriptions();

  connect(gfx_pane, &GraphicsPane::BackendChanged, this, &HacksWidget::OnBackendChanged);
  OnBackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));
  connect(m_fast_texture_sampling, &QCheckBox::toggled,
          [gfx_pane] { emit gfx_pane->UseFastTextureSamplingChanged(); });
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
  m_defer_efb_access_invalidation = new ConfigBool(
      tr("Defer EFB Cache Invalidation"), Config::GFX_HACK_EFB_DEFER_INVALIDATION, m_game_layer);
  m_store_efb_copies = new ConfigBool(tr("Store EFB Copies to Texture Only"),
                                      Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, m_game_layer);
  m_defer_efb_copies = new ConfigBool(tr("Defer EFB Copies to RAM"),
                                      Config::GFX_HACK_DEFER_EFB_COPIES, m_game_layer);
  m_ignore_format_changes = new ConfigBool(
      tr("Ignore Format Changes"), Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, m_game_layer, true);

  efb_layout->addWidget(m_skip_efb_cpu, 0, 0);
  efb_layout->addWidget(m_defer_efb_access_invalidation, 0, 1);
  efb_layout->addWidget(m_store_efb_copies, 1, 0);
  efb_layout->addWidget(m_defer_efb_copies, 1, 1);
  efb_layout->addWidget(m_ignore_format_changes, 2, 0);

  // XFB
  auto* xfb_box = new QGroupBox(tr("External Frame Buffer (XFB)"));
  auto* xfb_layout = new QVBoxLayout();
  xfb_box->setLayout(xfb_layout);

  m_store_xfb_copies = new ConfigBool(tr("Store XFB Copies to Texture Only"),
                                      Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, m_game_layer);
  m_immediate_xfb =
      new ConfigBool(tr("Immediately Present XFB"), Config::GFX_HACK_IMMEDIATE_XFB, m_game_layer);

  xfb_layout->addWidget(m_store_xfb_copies);
  xfb_layout->addWidget(m_immediate_xfb);

  // Other
  auto* other_box = new QGroupBox(tr("Other"));
  auto* other_layout = new QGridLayout();
  other_box->setLayout(other_layout);

  m_fast_depth_calculation =
      new ConfigBool(tr("Fast Depth Calculation"), Config::GFX_FAST_DEPTH_CALC, m_game_layer);
  m_fast_texture_sampling = new ConfigBool(tr("Fast Texture Sampling"),
                                           Config::GFX_HACK_FAST_TEXTURE_SAMPLING, m_game_layer);
  m_disable_bounding_box =
      new ConfigBool(tr("Disable Bounding Box"), Config::GFX_HACK_BBOX_ENABLE, m_game_layer, true);

  other_layout->addWidget(m_fast_depth_calculation, 0, 0);
  other_layout->addWidget(m_fast_texture_sampling, 0, 1);
  other_layout->addWidget(m_disable_bounding_box, 1, 0);

  // Texture Cache
  auto* texture_cache_layout = new QGridLayout();
  m_texture_accuracy =
      new ConfigSlider({0, 512, 128}, Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, m_game_layer);

  m_texture_accuracy_label =
      new ConfigSliderLabel(tr("Texture Cache Accuracy:"), m_texture_accuracy);
  auto* safe_label = new QLabel(tr("Safe"));
  safe_label->setAlignment(Qt::AlignRight);

  texture_cache_layout->addWidget(m_texture_accuracy_label, 0, 0);
  texture_cache_layout->setColumnMinimumWidth(1, 50);
  texture_cache_layout->addWidget(safe_label, 0, 2);
  texture_cache_layout->addWidget(m_texture_accuracy, 0, 3);
  texture_cache_layout->addWidget(new QLabel(tr("Fast")), 0, 4);
  other_layout->addLayout(texture_cache_layout, 2, 0, 1, 2);

  // Enhancements
  auto* graphics_box = new QGroupBox(tr("Graphics"));
  auto* enhancements_layout = new QGridLayout();
  graphics_box->setLayout(enhancements_layout);

  m_widescreen_hack =
      new ConfigBool(tr("Widescreen Hack"), Config::GFX_WIDESCREEN_HACK, m_game_layer);
  m_disable_fog = new ConfigBool(tr("Disable Fog"), Config::GFX_DISABLE_FOG, m_game_layer);

  int row = 0;
  enhancements_layout->addWidget(m_widescreen_hack, row, 0);
  enhancements_layout->addWidget(m_disable_fog, row, 1);
  ++row;

  main_layout->addWidget(efb_box);
  main_layout->addWidget(xfb_box);
  main_layout->addWidget(other_box);
  main_layout->addWidget(graphics_box);
  main_layout->addStretch();

  setLayout(main_layout);

  UpdateSkipEFBCPUEnabled();
  UpdateDeferEFBCopiesEnabled();
}

void HacksWidget::OnBackendChanged(const QString& backend_name)
{
  const bool bbox = g_backend_info.bSupportsBBox;
  m_disable_bounding_box->setEnabled(bbox);

  const QString tooltip = tr("%1 doesn't support this feature on your system.")
                              .arg(tr(backend_name.toStdString().c_str()));
  m_disable_bounding_box->setToolTip(!bbox ? tooltip : QString{});
}

void HacksWidget::ConnectWidgets(GraphicsPane* gfx_pane)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(m_skip_efb_cpu, &QCheckBox::checkStateChanged,
          [this](Qt::CheckState) { UpdateSkipEFBCPUEnabled(); });
  connect(m_store_efb_copies, &QCheckBox::checkStateChanged,
          [this](Qt::CheckState) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_store_xfb_copies, &QCheckBox::checkStateChanged,
          [this](Qt::CheckState) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_immediate_xfb, &QCheckBox::toggled,
          [gfx_pane] { emit gfx_pane->UpdateSkipPresentingDuplicateFramesEnabled(); });
#else
  connect(m_skip_efb_cpu, &QCheckBox::stateChanged, [this](int) { UpdateSkipEFBCPUEnabled(); });
  connect(m_store_efb_copies, &QCheckBox::stateChanged,
          [this](int) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_store_xfb_copies, &QCheckBox::stateChanged,
          [this](int) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_immediate_xfb, &QCheckBox::stateChanged,
          [gfx_pane] { emit gfx_pane->UpdateSkipPresentingDuplicateFramesEnabled(); });
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
  static const char TR_DEFER_EFB_ACCESS_INVALIDATION_DESCRIPTION[] = QT_TR_NOOP(
      "Defers invalidation of the EFB access cache until a GPU synchronization command "
      "is executed. If disabled, the cache will be invalidated with every draw call. "
      "<br><br>May improve performance in some games which rely on CPU EFB Access at the cost "
      "of stability.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
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
  static const char TR_FAST_DEPTH_CALC_DESCRIPTION[] = QT_TR_NOOP(
      "Uses a less accurate algorithm to calculate depth values.<br><br>Causes issues in a few "
      "games, but can result in a decent speed increase depending on the game and/or "
      "GPU.<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_FAST_TEXTURE_SAMPLING_DESCRIPTION[] = QT_TR_NOOP(
      "Use the graphics backend's built-in texture sampling instead of a manual "
      "implementation.<br><br>Disabling this setting can fix graphical issues in some games on "
      "certain GPUs, most commonly vertical lines on FMVs. In addition to this, disabling Fast "
      "Texture Sampling will allow for correct emulation of texture wrapping special cases (at 1x "
      "IR or when scaled EFB is disabled, and with custom textures disabled) and better emulates "
      "Level of Detail calculation.<br><br>Enabling this improves performance, especially at "
      "higher internal resolutions.<br><br>If this setting is disabled, the Texture Filtering "
      "setting will be disabled.<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_DISABLE_BOUNDINGBOX_DESCRIPTION[] =
      QT_TR_NOOP("Disables bounding box emulation.<br><br>This may improve GPU performance "
                 "significantly, but some games will break.<br><br><dolphin_emphasis>If "
                 "unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_WIDESCREEN_HACK_DESCRIPTION[] = QT_TR_NOOP(
      "Forces the game to output graphics at any aspect ratio by expanding the view frustum "
      "without stretching the image.<br>This is a hack, and its results will vary widely game "
      "to game (it often causes the UI to stretch).<br>"
      "Game-specific AR/Gecko-code aspect ratio patches are preferable over this if available."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_REMOVE_FOG_DESCRIPTION[] =
      QT_TR_NOOP("Makes distant objects more visible by removing fog, thus increasing the overall "
                 "detail.<br><br>Disabling fog will break some games which rely on proper fog "
                 "emulation.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");

  m_skip_efb_cpu->SetDescription(tr(TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION));
  m_defer_efb_access_invalidation->SetDescription(tr(TR_DEFER_EFB_ACCESS_INVALIDATION_DESCRIPTION));
  m_ignore_format_changes->SetDescription(tr(TR_IGNORE_FORMAT_CHANGE_DESCRIPTION));
  m_store_efb_copies->SetDescription(tr(TR_STORE_EFB_TO_TEXTURE_DESCRIPTION));
  m_defer_efb_copies->SetDescription(tr(TR_DEFER_EFB_COPIES_DESCRIPTION));
  m_texture_accuracy->SetTitle(tr("Texture Cache Accuracy"));
  m_texture_accuracy->SetDescription(tr(TR_ACCUARCY_DESCRIPTION));
  m_store_xfb_copies->SetDescription(tr(TR_STORE_XFB_TO_TEXTURE_DESCRIPTION));
  m_immediate_xfb->SetDescription(tr(TR_IMMEDIATE_XFB_DESCRIPTION));
  m_fast_depth_calculation->SetDescription(tr(TR_FAST_DEPTH_CALC_DESCRIPTION));
  m_fast_texture_sampling->SetDescription(tr(TR_FAST_TEXTURE_SAMPLING_DESCRIPTION));
  m_disable_bounding_box->SetDescription(tr(TR_DISABLE_BOUNDINGBOX_DESCRIPTION));
  m_widescreen_hack->SetDescription(tr(TR_WIDESCREEN_HACK_DESCRIPTION));
  m_disable_fog->SetDescription(tr(TR_REMOVE_FOG_DESCRIPTION));
}

void HacksWidget::UpdateSkipEFBCPUEnabled()
{
  // We disable the checkbox for defer EFB copies when both EFB and XFB copies to texture are
  // enabled.
  m_defer_efb_access_invalidation->setEnabled(!m_skip_efb_cpu->isChecked());
}

void HacksWidget::UpdateDeferEFBCopiesEnabled()
{
  // We disable the checkbox for defer EFB copies when both EFB and XFB copies to texture are
  // enabled.
  const bool can_defer = m_store_efb_copies->isChecked() && m_store_xfb_copies->isChecked();
  m_defer_efb_copies->setEnabled(!can_defer);
}

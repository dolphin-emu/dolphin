// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/HacksWidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Config/Graphics/GraphicsBool.h"
#include "DolphinQt2/Config/Graphics/GraphicsSlider.h"
#include "VideoCommon/VideoConfig.h"

HacksWidget::HacksWidget(GraphicsWindow* parent) : GraphicsWidget(parent)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  OnXFBToggled();
  AddDescriptions();
}

void HacksWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // EFB
  auto* efb_box = new QGroupBox(tr("Embedded Frame Buffer (EFB)"));
  auto* efb_layout = new QGridLayout();
  efb_box->setLayout(efb_layout);
  m_skip_efb_cpu =
      new GraphicsBool(tr("Skip EFB Access from CPU"), Config::GFX_HACK_EFB_ACCESS_ENABLE, true);
  m_ignore_format_changes = new GraphicsBool(tr("Ignore Format Changes"),
                                             Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, true);
  m_store_efb_copies = new GraphicsBool(tr("Store EFB Copies to Texture Only"),
                                        Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);

  efb_layout->addWidget(m_skip_efb_cpu, 0, 0);
  efb_layout->addWidget(m_ignore_format_changes, 0, 1);
  efb_layout->addWidget(m_store_efb_copies, 1, 0);

  // Texture Cache
  auto* texture_cache_box = new QGroupBox(tr("Texture Cache"));
  auto* texture_cache_layout = new QGridLayout();
  texture_cache_box->setLayout(texture_cache_layout);

  m_accuracy = new QSlider(Qt::Horizontal);
  m_accuracy->setMinimum(0);
  m_accuracy->setMaximum(2);
  m_gpu_texture_decoding =
      new GraphicsBool(tr("GPU Texture Decoding"), Config::GFX_ENABLE_GPU_TEXTURE_DECODING);

  auto* safe_label = new QLabel(tr("Safe"));
  safe_label->setAlignment(Qt::AlignRight);

  texture_cache_layout->addWidget(new QLabel(tr("Accuracy:")), 0, 0);
  texture_cache_layout->addWidget(safe_label, 0, 1);
  texture_cache_layout->addWidget(m_accuracy, 0, 2);
  texture_cache_layout->addWidget(new QLabel(tr("Fast")), 0, 3);
  texture_cache_layout->addWidget(m_gpu_texture_decoding, 1, 0);

  // XFB
  auto* xfb_box = new QGroupBox(tr("External Frame Buffer (XFB)"));
  auto* xfb_layout = new QGridLayout();
  xfb_box->setLayout(xfb_layout);

  m_disable_xfb = new GraphicsBool(tr("Disable"), Config::GFX_USE_XFB, true);
  m_real_xfb = new GraphicsBoolEx(tr("Real"), Config::GFX_USE_REAL_XFB, false);
  m_virtual_xfb = new GraphicsBoolEx(tr("Virtual"), Config::GFX_USE_REAL_XFB, true);

  xfb_layout->addWidget(m_disable_xfb, 0, 0);
  xfb_layout->addWidget(m_virtual_xfb, 0, 1);
  xfb_layout->addWidget(m_real_xfb, 0, 2);
  // Other
  auto* other_box = new QGroupBox(tr("Other"));
  auto* other_layout = new QGridLayout();
  other_box->setLayout(other_layout);

  m_fast_depth_calculation =
      new GraphicsBool(tr("Fast Depth Calculation"), Config::GFX_FAST_DEPTH_CALC);
  m_disable_bounding_box =
      new GraphicsBool(tr("Disable Bounding Box"), Config::GFX_HACK_BBOX_ENABLE, true);
  m_vertex_rounding = new GraphicsBool(tr("Vertex Rounding"), Config::GFX_HACK_VERTEX_ROUDING);

  other_layout->addWidget(m_fast_depth_calculation, 0, 0);
  other_layout->addWidget(m_disable_bounding_box, 0, 1);
  other_layout->addWidget(m_vertex_rounding, 1, 0);

  main_layout->addWidget(efb_box);
  main_layout->addWidget(texture_cache_box);
  main_layout->addWidget(xfb_box);
  main_layout->addWidget(other_box);

  setLayout(main_layout);
}

void HacksWidget::ConnectWidgets()
{
  connect(m_disable_xfb, &QCheckBox::toggled, this, &HacksWidget::OnXFBToggled);
  connect(m_accuracy, &QSlider::valueChanged, [this](int) { SaveSettings(); });
}

void HacksWidget::OnXFBToggled()
{
  m_real_xfb->setEnabled(!m_disable_xfb->isChecked());
  m_virtual_xfb->setEnabled(!m_disable_xfb->isChecked());
}

void HacksWidget::LoadSettings()
{
  auto samples = Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);

  int slider_pos = 0;

  switch (samples)
  {
  case 512:
    slider_pos = 1;
    break;
  case 128:
    slider_pos = 2;
    break;
  case 0:
    slider_pos = 0;
    break;
  // Custom values, ought not to be touched
  default:
    m_accuracy->setEnabled(false);
  }

  m_accuracy->setValue(slider_pos);
}

void HacksWidget::SaveSettings()
{
  int slider_pos = m_accuracy->value();

  if (m_accuracy->isEnabled())
  {
    int samples = 0;
    switch (slider_pos)
    {
    case 0:
      samples = 0;
      break;
    case 1:
      samples = 512;
      break;
    case 2:
      samples = 128;
    }

    Config::SetBaseOrCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, samples);
  }
}

void HacksWidget::AddDescriptions()
{
  static const char* TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION =
      QT_TR_NOOP("Ignore any requests from the CPU to read from or write to the EFB.\nImproves "
                 "performance in some games, but might disable some gameplay-related features or "
                 "graphical effects.\n\nIf unsure, leave this unchecked.");
  static const char* TR_IGNORE_FORMAT_CHANGE_DESCRIPTION = QT_TR_NOOP(
      "Ignore any changes to the EFB format.\nImproves performance in many games without "
      "any negative effect. Causes graphical defects in a small number of other "
      "games.\n\nIf unsure, leave this checked.");
  static const char* TR_STORE_EFB_TO_TEXTURE_DESCRIPTION = QT_TR_NOOP(
      "Stores EFB Copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
      "in a small number of games.\n\nEnabled = EFB Copies to Texture\nDisabled = EFB Copies to "
      "RAM "
      "(and Texture)\n\nIf unsure, leave this checked.");
  static const char* TR_ACCUARCY_DESCRIPTION = QT_TR_NOOP(
      "The \"Safe\" setting eliminates the likelihood of the GPU missing texture updates "
      "from RAM.\nLower accuracies cause in-game text to appear garbled in certain "
      "games.\n\nIf unsure, use the rightmost value.");

  static const char* TR_DISABLE_XFB_DESCRIPTION = QT_TR_NOOP(
      "Disable any XFB emulation.\nSpeeds up emulation a lot but causes heavy glitches in many "
      "games "
      "which rely on them (especially homebrew applications).\n\nIf unsure, leave this checked.");
  static const char* TR_VIRTUAL_XFB_DESCRIPTION = QT_TR_NOOP(
      "Emulate XFBs using GPU texture objects.\nFixes many games which don't work without XFB "
      "emulation while not being as slow as real XFB emulation. However, it may still fail for "
      "a lot "
      "of other games (especially homebrew applications).\n\nIf unsure, leave this checked.");

  static const char* TR_REAL_XFB_DESCRIPTION =
      QT_TR_NOOP("Emulate XFBs accurately.\nSlows down emulation a lot and prohibits "
                 "high-resolution rendering but is necessary to emulate a number of games "
                 "properly.\n\nIf unsure, check virtual XFB emulation instead.");

  static const char* TR_GPU_DECODING_DESCRIPTION =
      QT_TR_NOOP("Enables texture decoding using the GPU instead of the CPU. This may result in "
                 "performance gains in some scenarios, or on systems where the CPU is the "
                 "bottleneck.\n\nIf unsure, leave this unchecked.");

  static const char* TR_FAST_DEPTH_CALC_DESCRIPTION = QT_TR_NOOP(
      "Use a less accurate algorithm to calculate depth values.\nCauses issues in a few "
      "games, but can give a decent speedup depending on the game and/or your GPU.\n\nIf "
      "unsure, leave this checked.");
  static const char* TR_DISABLE_BOUNDINGBOX_DESCRIPTION =
      QT_TR_NOOP("Disable the bounding box emulation.\nThis may improve the GPU performance a lot, "
                 "but some games will break.\n\nIf unsure, leave this checked.");
  static const char* TR_VERTEX_ROUNDING_DESCRIPTION =
      QT_TR_NOOP("Rounds 2D vertices to whole pixels. Fixes graphical problems in some games at "
                 "higher internal resolutions. This setting has no effect when native internal "
                 "resolution is used.\n\nIf unsure, leave this unchecked.");

  AddDescription(m_skip_efb_cpu, TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION);
  AddDescription(m_ignore_format_changes, TR_IGNORE_FORMAT_CHANGE_DESCRIPTION);
  AddDescription(m_store_efb_copies, TR_STORE_EFB_TO_TEXTURE_DESCRIPTION);
  AddDescription(m_accuracy, TR_ACCUARCY_DESCRIPTION);
  AddDescription(m_disable_xfb, TR_DISABLE_XFB_DESCRIPTION);
  AddDescription(m_virtual_xfb, TR_VIRTUAL_XFB_DESCRIPTION);
  AddDescription(m_real_xfb, TR_REAL_XFB_DESCRIPTION);
  AddDescription(m_gpu_texture_decoding, TR_GPU_DECODING_DESCRIPTION);
  AddDescription(m_fast_depth_calculation, TR_FAST_DEPTH_CALC_DESCRIPTION);
  AddDescription(m_disable_bounding_box, TR_DISABLE_BOUNDINGBOX_DESCRIPTION);
  AddDescription(m_fast_depth_calculation, TR_FAST_DEPTH_CALC_DESCRIPTION);
  AddDescription(m_disable_bounding_box, TR_DISABLE_BOUNDINGBOX_DESCRIPTION);
  AddDescription(m_vertex_rounding, TR_VERTEX_ROUNDING_DESCRIPTION);
}

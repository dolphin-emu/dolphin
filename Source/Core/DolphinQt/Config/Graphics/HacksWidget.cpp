// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/HacksWidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsSlider.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoConfig.h"

HacksWidget::HacksWidget(GraphicsWindow* parent) : GraphicsWidget(parent)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();

  connect(parent, &GraphicsWindow::BackendChanged, this, &HacksWidget::OnBackendChanged);
  OnBackendChanged(QString::fromStdString(SConfig::GetInstance().m_strVideoBackend));
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &HacksWidget::LoadSettings);
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
  m_defer_efb_copies =
      new GraphicsBool(tr("Defer EFB Copies to RAM"), Config::GFX_HACK_DEFER_EFB_COPIES);

  efb_layout->addWidget(m_skip_efb_cpu, 0, 0);
  efb_layout->addWidget(m_ignore_format_changes, 0, 1);
  efb_layout->addWidget(m_store_efb_copies, 1, 0);
  efb_layout->addWidget(m_defer_efb_copies, 1, 1);

  // Texture Cache
  auto* texture_cache_box = new QGroupBox(tr("Texture Cache"));
  auto* texture_cache_layout = new QGridLayout();
  texture_cache_box->setLayout(texture_cache_layout);

  m_accuracy = new QSlider(Qt::Horizontal);
  m_accuracy->setMinimum(0);
  m_accuracy->setMaximum(2);
  m_accuracy->setPageStep(1);
  m_accuracy->setTickPosition(QSlider::TicksBelow);
  m_gpu_texture_decoding =
      new GraphicsBool(tr("GPU Texture Decoding"), Config::GFX_ENABLE_GPU_TEXTURE_DECODING);

  auto* safe_label = new QLabel(tr("Safe"));
  safe_label->setAlignment(Qt::AlignRight);

  m_accuracy_label = new QLabel(tr("Accuracy:"));

  texture_cache_layout->addWidget(m_accuracy_label, 0, 0);
  texture_cache_layout->addWidget(safe_label, 0, 1);
  texture_cache_layout->addWidget(m_accuracy, 0, 2);
  texture_cache_layout->addWidget(new QLabel(tr("Fast")), 0, 3);
  texture_cache_layout->addWidget(m_gpu_texture_decoding, 1, 0);

  // XFB
  auto* xfb_box = new QGroupBox(tr("External Frame Buffer (XFB)"));
  auto* xfb_layout = new QVBoxLayout();
  xfb_box->setLayout(xfb_layout);

  m_store_xfb_copies = new GraphicsBool(tr("Store XFB Copies to Texture Only"),
                                        Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM);
  m_immediate_xfb = new GraphicsBool(tr("Immediately Present XFB"), Config::GFX_HACK_IMMEDIATE_XFB);

  xfb_layout->addWidget(m_store_xfb_copies);
  xfb_layout->addWidget(m_immediate_xfb);

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
  main_layout->addStretch();

  setLayout(main_layout);

  UpdateDeferEFBCopiesEnabled();
}

void HacksWidget::OnBackendChanged(const QString& backend_name)
{
  const bool bbox = g_Config.backend_info.bSupportsBBox;
  const bool gpu_texture_decoding = g_Config.backend_info.bSupportsGPUTextureDecoding;

  m_gpu_texture_decoding->setEnabled(gpu_texture_decoding);
  m_disable_bounding_box->setEnabled(bbox);

  const QString tooltip = tr("%1 doesn't support this feature on your system.").arg(backend_name);

  m_gpu_texture_decoding->setToolTip(!gpu_texture_decoding ? tooltip : QStringLiteral(""));
  m_disable_bounding_box->setToolTip(!bbox ? tooltip : QStringLiteral(""));
}

void HacksWidget::ConnectWidgets()
{
  connect(m_accuracy, &QSlider::valueChanged, [this](int) { SaveSettings(); });
  connect(m_store_efb_copies, &QCheckBox::stateChanged,
          [this](int) { UpdateDeferEFBCopiesEnabled(); });
  connect(m_store_xfb_copies, &QCheckBox::stateChanged,
          [this](int) { UpdateDeferEFBCopiesEnabled(); });
}

void HacksWidget::LoadSettings()
{
  const QSignalBlocker blocker(m_accuracy);
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

  QFont bf = m_accuracy_label->font();

  bf.setBold(Config::GetActiveLayerForConfig(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES) !=
             Config::LayerType::Base);

  m_accuracy_label->setFont(bf);
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
  static const char TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION[] =
      QT_TR_NOOP("Ignores any requests from the CPU to read from or write to the EFB. "
                 "\n\nImproves performance in some games, but will disable all EFB-based "
                 "graphical effects or gameplay-related features.\n\nIf unsure, "
                 "leave this unchecked.");
  static const char TR_IGNORE_FORMAT_CHANGE_DESCRIPTION[] = QT_TR_NOOP(
      "Ignores any changes to the EFB format.\n\nImproves performance in many games without "
      "any negative effect. Causes graphical defects in a small number of other "
      "games.\n\nIf unsure, leave this checked.");
  static const char TR_STORE_EFB_TO_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Stores EFB copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
      "in a small number of games.\n\nEnabled = EFB Copies to Texture\nDisabled = EFB Copies to "
      "RAM (and Texture)\n\nIf unsure, leave this checked.");
  static const char TR_DEFER_EFB_COPIES_DESCRIPTION[] = QT_TR_NOOP(
      "Waits until the game synchronizes with the emulated GPU before writing the contents of EFB "
      "copies to RAM.\n\nReduces the overhead of EFB RAM copies, providing a performance boost in "
      "many games, at the risk of breaking those which do not safely synchronize with the "
      "emulated GPU.\n\nIf unsure, leave this checked.");
  static const char TR_ACCUARCY_DESCRIPTION[] = QT_TR_NOOP(
      "Adjusts the accuracy at which the GPU receives texture updates from RAM.\n\n"
      "The \"Safe\" setting eliminates the likelihood of the GPU missing texture updates "
      "from RAM. Lower accuracies cause in-game text to appear garbled in certain "
      "games.\n\nIf unsure, select the rightmost value.");

  static const char TR_STORE_XFB_TO_TEXTURE_DESCRIPTION[] = QT_TR_NOOP(
      "Stores XFB copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
      "in a small number of games.\n\nEnabled = XFB Copies to "
      "Texture\nDisabled = XFB Copies to RAM (and Texture)\n\nIf unsure, leave this checked.");

  static const char TR_IMMEDIATE_XFB_DESCRIPTION[] =
      QT_TR_NOOP("Displays XFB copies as soon as they are created, instead of waiting for "
                 "scanout.\n\nCan cause graphical defects in some games if the game doesn't "
                 "expect all XFB copies to be displayed. However, turning this setting on reduces "
                 "latency.\n\nIf unsure, leave this unchecked.");

  static const char TR_GPU_DECODING_DESCRIPTION[] =
      QT_TR_NOOP("Enables texture decoding using the GPU instead of the CPU.\n\nThis may result in "
                 "performance gains in some scenarios, or on systems where the CPU is the "
                 "bottleneck.\n\nIf unsure, leave this unchecked.");

  static const char TR_FAST_DEPTH_CALC_DESCRIPTION[] = QT_TR_NOOP(
      "Uses a less accurate algorithm to calculate depth values.\n\nCauses issues in a few "
      "games, but can result in a decent speed increase depending on the game and/or "
      "GPU.\n\nIf unsure, leave this checked.");
  static const char TR_DISABLE_BOUNDINGBOX_DESCRIPTION[] =
      QT_TR_NOOP("Disables bounding box emulation.\n\nThis may improve GPU performance "
                 "significantly, but some games will break.\n\nIf unsure, leave this checked.");
  static const char TR_VERTEX_ROUNDING_DESCRIPTION[] =
      QT_TR_NOOP("Rounds 2D vertices to whole pixels.\n\nFixes graphical problems in some games at "
                 "higher internal resolutions. This setting has no effect when native internal "
                 "resolution is used.\n\nIf unsure, leave this unchecked.");

  AddDescription(m_skip_efb_cpu, TR_SKIP_EFB_CPU_ACCESS_DESCRIPTION);
  AddDescription(m_ignore_format_changes, TR_IGNORE_FORMAT_CHANGE_DESCRIPTION);
  AddDescription(m_store_efb_copies, TR_STORE_EFB_TO_TEXTURE_DESCRIPTION);
  AddDescription(m_defer_efb_copies, TR_DEFER_EFB_COPIES_DESCRIPTION);
  AddDescription(m_accuracy, TR_ACCUARCY_DESCRIPTION);
  AddDescription(m_store_xfb_copies, TR_STORE_XFB_TO_TEXTURE_DESCRIPTION);
  AddDescription(m_immediate_xfb, TR_IMMEDIATE_XFB_DESCRIPTION);
  AddDescription(m_gpu_texture_decoding, TR_GPU_DECODING_DESCRIPTION);
  AddDescription(m_fast_depth_calculation, TR_FAST_DEPTH_CALC_DESCRIPTION);
  AddDescription(m_disable_bounding_box, TR_DISABLE_BOUNDINGBOX_DESCRIPTION);
  AddDescription(m_vertex_rounding, TR_VERTEX_ROUNDING_DESCRIPTION);
}

void HacksWidget::UpdateDeferEFBCopiesEnabled()
{
  // We disable the checkbox for defer EFB copies when both EFB and XFB copies to texture are
  // enabled.
  const bool can_defer = m_store_efb_copies->isChecked() && m_store_xfb_copies->isChecked();
  m_defer_efb_copies->setEnabled(!can_defer);
}

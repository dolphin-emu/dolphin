// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"

#include <cmath>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsChoice.h"
#include "DolphinQt/Config/Graphics/GraphicsSlider.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Config/Graphics/PostProcessingConfigWindow.h"
#include "DolphinQt/Settings.h"

#include "UICommon/VideoUtils.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

EnhancementsWidget::EnhancementsWidget(GraphicsWindow* parent)
    : GraphicsWidget(parent), m_block_save(false)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();
  connect(parent, &GraphicsWindow::BackendChanged,
          [this](const QString& backend) { LoadSettings(); });
}

void EnhancementsWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Enhancements
  auto* enhancements_box = new QGroupBox(tr("Enhancements"));
  auto* enhancements_layout = new QGridLayout();
  enhancements_box->setLayout(enhancements_layout);

  m_ir_combo = new GraphicsChoice({tr("Auto (Multiple of 640x528)"), tr("Native (640x528)"),
                                   tr("2x Native (1280x1056) for 720p"),
                                   tr("3x Native (1920x1584) for 1080p"),
                                   tr("4x Native (2560x2112) for 1440p"),
                                   tr("5x Native (3200x2640)"), tr("6x Native (3840x3168) for 4K"),
                                   tr("7x Native (4480x3696)"), tr("8x Native (5120x4224) for 5K")},
                                  Config::GFX_EFB_SCALE);

  if (g_Config.iEFBScale > 8)
  {
    m_ir_combo->addItem(tr("Custom"));
    m_ir_combo->setCurrentIndex(m_ir_combo->count() - 1);
  }

  m_ir_combo->setMaxVisibleItems(m_ir_combo->count());

  m_aa_combo = new QComboBox();
  m_af_combo = new GraphicsChoice({tr("1x"), tr("2x"), tr("4x"), tr("8x"), tr("16x")},
                                  Config::GFX_ENHANCE_MAX_ANISOTROPY);

  m_pp_effect = new QComboBox();
  m_configure_pp_effect = new QPushButton(tr("Configure"));
  m_scaled_efb_copy = new GraphicsBool(tr("Scaled EFB Copy"), Config::GFX_HACK_COPY_EFB_SCALED);
  m_per_pixel_lighting =
      new GraphicsBool(tr("Per-Pixel Lighting"), Config::GFX_ENABLE_PIXEL_LIGHTING);
  m_force_texture_filtering =
      new GraphicsBool(tr("Force Texture Filtering"), Config::GFX_ENHANCE_FORCE_FILTERING);
  m_widescreen_hack = new GraphicsBool(tr("Widescreen Hack"), Config::GFX_WIDESCREEN_HACK);
  m_disable_fog = new GraphicsBool(tr("Disable Fog"), Config::GFX_DISABLE_FOG);
  m_force_24bit_color =
      new GraphicsBool(tr("Force 24-Bit Color"), Config::GFX_ENHANCE_FORCE_TRUE_COLOR);
  m_disable_copy_filter =
      new GraphicsBool(tr("Disable Copy Filter"), Config::GFX_ENHANCE_DISABLE_COPY_FILTER);
  m_arbitrary_mipmap_detection = new GraphicsBool(tr("Arbitrary Mipmap Detection"),
                                                  Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION);

  enhancements_layout->addWidget(new QLabel(tr("Internal Resolution:")), 0, 0);
  enhancements_layout->addWidget(m_ir_combo, 0, 1, 1, -1);
  enhancements_layout->addWidget(new QLabel(tr("Anti-Aliasing:")), 1, 0);
  enhancements_layout->addWidget(m_aa_combo, 1, 1, 1, -1);
  enhancements_layout->addWidget(new QLabel(tr("Anisotropic Filtering:")), 2, 0);
  enhancements_layout->addWidget(m_af_combo, 2, 1, 1, -1);

  enhancements_layout->addWidget(new QLabel(tr("Post-Processing Effect:")), 4, 0);
  enhancements_layout->addWidget(m_pp_effect, 4, 1);
  enhancements_layout->addWidget(m_configure_pp_effect, 4, 2);

  enhancements_layout->addWidget(m_scaled_efb_copy, 5, 0);
  enhancements_layout->addWidget(m_per_pixel_lighting, 5, 1);
  enhancements_layout->addWidget(m_force_texture_filtering, 6, 0);
  enhancements_layout->addWidget(m_widescreen_hack, 6, 1);
  enhancements_layout->addWidget(m_disable_fog, 7, 0);
  enhancements_layout->addWidget(m_force_24bit_color, 7, 1);
  enhancements_layout->addWidget(m_disable_copy_filter, 8, 0);
  enhancements_layout->addWidget(m_arbitrary_mipmap_detection, 8, 1);

  main_layout->addWidget(enhancements_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void EnhancementsWidget::ConnectWidgets()
{
  connect(m_aa_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  connect(m_pp_effect, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  connect(m_configure_pp_effect, &QPushButton::pressed, this,
          &EnhancementsWidget::ConfigurePostProcessingShader);
}

void EnhancementsWidget::LoadPPShaders()
{
  std::vector<std::string> shaders = VideoCommon::PostProcessing::GetShaderList();

  m_pp_effect->clear();
  m_pp_effect->addItem(tr("(off)"));

  auto selected_shader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);

  for (const auto& shader : shaders)
  {
    m_pp_effect->addItem(QString::fromStdString(shader));
    if (selected_shader == shader)
    {
      m_pp_effect->setCurrentIndex(m_pp_effect->count() - 1);
    }
  }

  const bool supports_postprocessing = g_Config.backend_info.bSupportsPostProcessing;
  m_pp_effect->setEnabled(supports_postprocessing);

  m_pp_effect->setToolTip(supports_postprocessing ?
                              QStringLiteral("") :
                              tr("%1 doesn't support this feature.")
                                  .arg(tr(g_video_backend->GetDisplayName().c_str())));

  VideoCommon::PostProcessingConfiguration pp_shader;
  if (selected_shader != "(off)" && supports_postprocessing)
  {
    pp_shader.LoadShader(selected_shader);
    m_configure_pp_effect->setEnabled(pp_shader.HasOptions());
  }
  else
  {
    m_configure_pp_effect->setEnabled(false);
  }
}

void EnhancementsWidget::LoadSettings()
{
  m_block_save = true;
  // Anti-Aliasing

  int aa_selection = Config::Get(Config::GFX_MSAA);
  bool ssaa = Config::Get(Config::GFX_SSAA);

  m_aa_combo->clear();
  for (const auto& option : VideoUtils::GetAvailableAntialiasingModes(m_msaa_modes))
    m_aa_combo->addItem(option == "None" ? tr("None") : QString::fromStdString(option));

  m_aa_combo->setCurrentText(
      QString::fromStdString(std::to_string(aa_selection) + "x " + (ssaa ? "SSAA" : "MSAA")));
  m_aa_combo->setEnabled(m_aa_combo->count() > 1);

  // Post Processing Shader
  LoadPPShaders();

  m_block_save = false;
}

void EnhancementsWidget::SaveSettings()
{
  if (m_block_save)
    return;

  bool is_ssaa = m_aa_combo->currentText().endsWith(QStringLiteral("SSAA"));

  int aa_value = m_aa_combo->currentIndex();

  if (aa_value == 0)
  {
    aa_value = 1;
  }
  else
  {
    if (aa_value > m_msaa_modes)
      aa_value -= m_msaa_modes;
    aa_value = std::pow(2, aa_value);
  }
  Config::SetBaseOrCurrent(Config::GFX_MSAA, static_cast<unsigned int>(aa_value));

  Config::SetBaseOrCurrent(Config::GFX_SSAA, is_ssaa);

  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER,
    m_pp_effect->currentIndex() == 0 ? "(off)" : m_pp_effect->currentText().toStdString());

  VideoCommon::PostProcessingConfiguration pp_shader;
  if (Config::Get(Config::GFX_ENHANCE_POST_SHADER) != "(off)")
  {
    pp_shader.LoadShader(Config::Get(Config::GFX_ENHANCE_POST_SHADER));
    m_configure_pp_effect->setEnabled(pp_shader.HasOptions());
  }
  else
  {
    m_configure_pp_effect->setEnabled(false);
  }

  LoadSettings();
}

void EnhancementsWidget::AddDescriptions()
{
  static const char TR_INTERNAL_RESOLUTION_DESCRIPTION[] =
      QT_TR_NOOP("Controls the rendering resolution.\n\nA high resolution greatly improves "
                 "visual quality, but also greatly increases GPU load and can cause issues in "
                 "certain games. Generally speaking, the lower the internal resolution, the "
                 "better performance will be.\n\nIf unsure, select Native.");

  static const char TR_ANTIALIAS_DESCRIPTION[] = QT_TR_NOOP(
      "Reduces the amount of aliasing caused by rasterizing 3D graphics, resulting "
      "in smoother edges on objects. Increases GPU load and sometimes causes graphical "
      "issues.\n\nSSAA is significantly more demanding than MSAA, but provides top quality "
      "geometry anti-aliasing and also applies anti-aliasing to lighting, shader "
      "effects, and textures.\n\nIf unsure, select None.");

  static const char TR_ANISOTROPIC_FILTERING_DESCRIPTION[] = QT_TR_NOOP(
      "Enables anisotropic filtering, which enhances the visual quality of textures that "
      "are at oblique viewing angles.\n\nMight cause issues in a small "
      "number of games.\n\nIf unsure, select 1x.");

  static const char TR_POSTPROCESSING_DESCRIPTION[] = QT_TR_NOOP(
      "Applies a post-processing effect after rendering a frame.\n\nIf unsure, select (off).");

  static const char TR_SCALED_EFB_COPY_DESCRIPTION[] =
      QT_TR_NOOP("Greatly increases the quality of textures generated using render-to-texture "
                 "effects.\n\nSlightly increases GPU load and causes relatively few graphical "
                 "issues. Raising the internal resolution will improve the effect of this setting. "
                 "\n\nIf unsure, leave this checked.");
  static const char TR_PER_PIXEL_LIGHTING_DESCRIPTION[] = QT_TR_NOOP(
      "Calculates lighting of 3D objects per-pixel rather than per-vertex, smoothing out the "
      "appearance of lit polygons and making individual triangles less noticeable.\n\nRarely "
      "causes slowdowns or graphical issues.\n\nIf unsure, leave this unchecked.");
  static const char TR_WIDESCREEN_HACK_DESCRIPTION[] = QT_TR_NOOP(
      "Forces the game to output graphics for any aspect ratio. Use with \"Aspect Ratio\" set to "
      "\"Force 16:9\" to force 4:3-only games to run at 16:9.\n\nRarely produces good results and "
      "often partially breaks graphics and game UIs. Unnecessary (and detrimental) if using any "
      "AR/Gecko-code widescreen patches.\n\nIf unsure, leave this unchecked.");
  static const char TR_REMOVE_FOG_DESCRIPTION[] =
      QT_TR_NOOP("Makes distant objects more visible by removing fog, thus increasing the overall "
                 "detail.\n\nDisabling fog will break some games which rely on proper fog "
                 "emulation.\n\nIf unsure, leave this unchecked.");
  static const char TR_FORCE_24BIT_DESCRIPTION[] =
      QT_TR_NOOP("Forces the game to render the RGB color channels in 24-bit, thereby increasing "
                 "quality by reducing color banding.\n\nHas no impact on performance and causes "
                 "few graphical issues.\n\nIf unsure, leave this checked.");
  static const char TR_FORCE_TEXTURE_FILTERING_DESCRIPTION[] =
      QT_TR_NOOP("Filters all textures, including any that the game explicitly set as "
                 "unfiltered.\n\nMay improve quality of certain textures in some games, but "
                 "will cause issues in others.\n\nIf unsure, leave this unchecked.");
  static const char TR_DISABLE_COPY_FILTER_DESCRIPTION[] =
      QT_TR_NOOP("Disables the blending of adjacent rows when copying the EFB. This is known in "
                 "some games as \"deflickering\" or \"smoothing\". \n\nDisabling the filter has no "
                 "effect on performance, but may result in a sharper image. Causes few "
                 "graphical issues.\n\nIf unsure, leave this checked.");
  static const char TR_ARBITRARY_MIPMAP_DETECTION_DESCRIPTION[] = QT_TR_NOOP(
      "Enables detection of arbitrary mipmaps, which some games use for special distance-based "
      "effects.\n\nMay have false positives that result in blurry textures at increased internal "
      "resolution, such as in games that use very low resolution mipmaps. Disabling this can also "
      "reduce stutter in games that frequently load new textures. This feature is not compatible "
      "with GPU Texture Decoding.\n\nIf unsure, leave this checked.");

  AddDescription(m_ir_combo, TR_INTERNAL_RESOLUTION_DESCRIPTION);
  AddDescription(m_aa_combo, TR_ANTIALIAS_DESCRIPTION);
  AddDescription(m_af_combo, TR_ANISOTROPIC_FILTERING_DESCRIPTION);
  AddDescription(m_pp_effect, TR_POSTPROCESSING_DESCRIPTION);
  AddDescription(m_scaled_efb_copy, TR_SCALED_EFB_COPY_DESCRIPTION);
  AddDescription(m_per_pixel_lighting, TR_PER_PIXEL_LIGHTING_DESCRIPTION);
  AddDescription(m_widescreen_hack, TR_WIDESCREEN_HACK_DESCRIPTION);
  AddDescription(m_disable_fog, TR_REMOVE_FOG_DESCRIPTION);
  AddDescription(m_force_24bit_color, TR_FORCE_24BIT_DESCRIPTION);
  AddDescription(m_force_texture_filtering, TR_FORCE_TEXTURE_FILTERING_DESCRIPTION);
  AddDescription(m_disable_copy_filter, TR_DISABLE_COPY_FILTER_DESCRIPTION);
  AddDescription(m_arbitrary_mipmap_detection, TR_ARBITRARY_MIPMAP_DETECTION_DESCRIPTION);
}

void EnhancementsWidget::ConfigurePostProcessingShader()
{
  const std::string shader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  PostProcessingConfigWindow(this, shader).exec();
}

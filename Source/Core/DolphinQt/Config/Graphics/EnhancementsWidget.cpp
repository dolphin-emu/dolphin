// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"

#include <cmath>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigRadio.h"
#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"
#include "DolphinQt/Config/Graphics/ColorCorrectionConfigWindow.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Config/Graphics/PostProcessingConfigWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipPushButton.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

EnhancementsWidget::EnhancementsWidget(GraphicsWindow* parent) : m_block_save(false)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();
  connect(parent, &GraphicsWindow::BackendChanged,
          [this](const QString& backend) { LoadSettings(); });
  connect(parent, &GraphicsWindow::UseFastTextureSamplingChanged, this,
          &EnhancementsWidget::LoadSettings);
  connect(parent, &GraphicsWindow::UseGPUTextureDecodingChanged, this,
          &EnhancementsWidget::LoadSettings);
}

constexpr int TEXTURE_FILTERING_DEFAULT = 0;
constexpr int TEXTURE_FILTERING_ANISO_2X = 1;
constexpr int TEXTURE_FILTERING_ANISO_4X = 2;
constexpr int TEXTURE_FILTERING_ANISO_8X = 3;
constexpr int TEXTURE_FILTERING_ANISO_16X = 4;
constexpr int TEXTURE_FILTERING_FORCE_NEAREST = 5;
constexpr int TEXTURE_FILTERING_FORCE_LINEAR = 6;
constexpr int TEXTURE_FILTERING_FORCE_LINEAR_ANISO_2X = 7;
constexpr int TEXTURE_FILTERING_FORCE_LINEAR_ANISO_4X = 8;
constexpr int TEXTURE_FILTERING_FORCE_LINEAR_ANISO_8X = 9;
constexpr int TEXTURE_FILTERING_FORCE_LINEAR_ANISO_16X = 10;

void EnhancementsWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Enhancements
  auto* enhancements_box = new QGroupBox(tr("Enhancements"));
  auto* enhancements_layout = new QGridLayout();
  enhancements_box->setLayout(enhancements_layout);

  QStringList resolution_options{tr("Auto (Multiple of 640x528)"), tr("Native (640x528)")};
  // From 2x up.
  // To calculate the suggested internal resolution scale for each common output resolution,
  // we find the minimum multiplier that results in an equal or greater resolution than the
  // output one, on both width and height.
  // Note that often games don't render to the full resolution, but have some black bars
  // on the edges; this is not accounted for in the calculations.
  const QStringList resolution_extra_options{
      tr("720p"),         tr("1080p"),        tr("1440p"), QStringLiteral(""),
      tr("4K"),           QStringLiteral(""), tr("5K"),    QStringLiteral(""),
      QStringLiteral(""), QStringLiteral(""), tr("8K")};
  const int visible_resolution_option_count = static_cast<int>(resolution_options.size()) +
                                              static_cast<int>(resolution_extra_options.size());

  // If the current scale is greater than the max scale in the ini, add sufficient options so that
  // when the settings are saved we don't lose the user-modified value from the ini.
  const int max_efb_scale =
      std::max(Config::Get(Config::GFX_EFB_SCALE), Config::Get(Config::GFX_MAX_EFB_SCALE));
  for (int scale = static_cast<int>(resolution_options.size()); scale <= max_efb_scale; scale++)
  {
    const QString scale_text = QString::number(scale);
    const QString width_text = QString::number(static_cast<int>(EFB_WIDTH) * scale);
    const QString height_text = QString::number(static_cast<int>(EFB_HEIGHT) * scale);
    const int extra_index = resolution_options.size() - 2;
    const QString extra_text = resolution_extra_options.size() > extra_index ?
                                   resolution_extra_options[extra_index] :
                                   QStringLiteral("");
    if (extra_text.isEmpty())
    {
      resolution_options.append(tr("%1x Native (%2x%3)").arg(scale_text, width_text, height_text));
    }
    else
    {
      resolution_options.append(
          tr("%1x Native (%2x%3) for %4").arg(scale_text, width_text, height_text, extra_text));
    }
  }

  m_ir_combo = new ConfigChoice(resolution_options, Config::GFX_EFB_SCALE);
  m_ir_combo->setMaxVisibleItems(visible_resolution_option_count);

  m_aa_combo = new ToolTipComboBox();

  m_texture_filtering_combo = new ToolTipComboBox();
  m_texture_filtering_combo->addItem(tr("Default"), TEXTURE_FILTERING_DEFAULT);
  m_texture_filtering_combo->addItem(tr("2x Anisotropic"), TEXTURE_FILTERING_ANISO_2X);
  m_texture_filtering_combo->addItem(tr("4x Anisotropic"), TEXTURE_FILTERING_ANISO_4X);
  m_texture_filtering_combo->addItem(tr("8x Anisotropic"), TEXTURE_FILTERING_ANISO_8X);
  m_texture_filtering_combo->addItem(tr("16x Anisotropic"), TEXTURE_FILTERING_ANISO_16X);
  m_texture_filtering_combo->addItem(tr("Force Nearest"), TEXTURE_FILTERING_FORCE_NEAREST);
  m_texture_filtering_combo->addItem(tr("Force Linear"), TEXTURE_FILTERING_FORCE_LINEAR);
  m_texture_filtering_combo->addItem(tr("Force Linear and 2x Anisotropic"),
                                     TEXTURE_FILTERING_FORCE_LINEAR_ANISO_2X);
  m_texture_filtering_combo->addItem(tr("Force Linear and 4x Anisotropic"),
                                     TEXTURE_FILTERING_FORCE_LINEAR_ANISO_4X);
  m_texture_filtering_combo->addItem(tr("Force Linear and 8x Anisotropic"),
                                     TEXTURE_FILTERING_FORCE_LINEAR_ANISO_8X);
  m_texture_filtering_combo->addItem(tr("Force Linear and 16x Anisotropic"),
                                     TEXTURE_FILTERING_FORCE_LINEAR_ANISO_16X);

  m_output_resampling_combo = new ToolTipComboBox();
  m_output_resampling_combo->addItem(tr("Default"),
                                     static_cast<int>(OutputResamplingMode::Default));
  m_output_resampling_combo->addItem(tr("Bilinear"),
                                     static_cast<int>(OutputResamplingMode::Bilinear));
  m_output_resampling_combo->addItem(tr("Bicubic: B-Spline"),
                                     static_cast<int>(OutputResamplingMode::BSpline));
  m_output_resampling_combo->addItem(tr("Bicubic: Mitchell-Netravali"),
                                     static_cast<int>(OutputResamplingMode::MitchellNetravali));
  m_output_resampling_combo->addItem(tr("Bicubic: Catmull-Rom"),
                                     static_cast<int>(OutputResamplingMode::CatmullRom));
  m_output_resampling_combo->addItem(tr("Sharp Bilinear"),
                                     static_cast<int>(OutputResamplingMode::SharpBilinear));
  m_output_resampling_combo->addItem(tr("Area Sampling"),
                                     static_cast<int>(OutputResamplingMode::AreaSampling));

  m_configure_color_correction = new ToolTipPushButton(tr("Configure"));

  m_pp_effect = new ToolTipComboBox();
  m_configure_pp_effect = new NonDefaultQPushButton(tr("Configure"));
  m_scaled_efb_copy = new ConfigBool(tr("Scaled EFB Copy"), Config::GFX_HACK_COPY_EFB_SCALED);
  m_per_pixel_lighting =
      new ConfigBool(tr("Per-Pixel Lighting"), Config::GFX_ENABLE_PIXEL_LIGHTING);

  m_widescreen_hack = new ConfigBool(tr("Widescreen Hack"), Config::GFX_WIDESCREEN_HACK);
  m_disable_fog = new ConfigBool(tr("Disable Fog"), Config::GFX_DISABLE_FOG);
  m_force_24bit_color =
      new ConfigBool(tr("Force 24-Bit Color"), Config::GFX_ENHANCE_FORCE_TRUE_COLOR);
  m_disable_copy_filter =
      new ConfigBool(tr("Disable Copy Filter"), Config::GFX_ENHANCE_DISABLE_COPY_FILTER);
  m_arbitrary_mipmap_detection = new ConfigBool(tr("Arbitrary Mipmap Detection"),
                                                Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION);
  m_hdr = new ConfigBool(tr("HDR Post-Processing"), Config::GFX_ENHANCE_HDR_OUTPUT);

  int row = 0;
  enhancements_layout->addWidget(new QLabel(tr("Internal Resolution:")), row, 0);
  enhancements_layout->addWidget(m_ir_combo, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(new QLabel(tr("Anti-Aliasing:")), row, 0);
  enhancements_layout->addWidget(m_aa_combo, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(new QLabel(tr("Texture Filtering:")), row, 0);
  enhancements_layout->addWidget(m_texture_filtering_combo, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(new QLabel(tr("Output Resampling:")), row, 0);
  enhancements_layout->addWidget(m_output_resampling_combo, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(new QLabel(tr("Color Correction:")), row, 0);
  enhancements_layout->addWidget(m_configure_color_correction, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(new QLabel(tr("Post-Processing Effect:")), row, 0);
  enhancements_layout->addWidget(m_pp_effect, row, 1);
  enhancements_layout->addWidget(m_configure_pp_effect, row, 2);
  ++row;

  enhancements_layout->addWidget(m_scaled_efb_copy, row, 0);
  enhancements_layout->addWidget(m_per_pixel_lighting, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(m_widescreen_hack, row, 0);
  enhancements_layout->addWidget(m_force_24bit_color, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(m_disable_fog, row, 0);
  enhancements_layout->addWidget(m_arbitrary_mipmap_detection, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(m_disable_copy_filter, row, 0);
  enhancements_layout->addWidget(m_hdr, row, 1, 1, -1);
  ++row;

  // Stereoscopy
  auto* stereoscopy_box = new QGroupBox(tr("Stereoscopy"));
  auto* stereoscopy_layout = new QGridLayout();
  stereoscopy_box->setLayout(stereoscopy_layout);

  m_3d_mode = new ConfigChoice({tr("Off"), tr("Side-by-Side"), tr("Top-and-Bottom"), tr("Anaglyph"),
                                tr("HDMI 3D"), tr("Passive")},
                               Config::GFX_STEREO_MODE);
  m_3d_depth = new ConfigSlider(0, Config::GFX_STEREO_DEPTH_MAXIMUM, Config::GFX_STEREO_DEPTH);
  m_3d_convergence = new ConfigSlider(0, Config::GFX_STEREO_CONVERGENCE_MAXIMUM,
                                      Config::GFX_STEREO_CONVERGENCE, 100);
  m_3d_swap_eyes = new ConfigBool(tr("Swap Eyes"), Config::GFX_STEREO_SWAP_EYES);

  stereoscopy_layout->addWidget(new QLabel(tr("Stereoscopic 3D Mode:")), 0, 0);
  stereoscopy_layout->addWidget(m_3d_mode, 0, 1);
  stereoscopy_layout->addWidget(new QLabel(tr("Depth:")), 1, 0);
  stereoscopy_layout->addWidget(m_3d_depth, 1, 1);
  stereoscopy_layout->addWidget(new QLabel(tr("Convergence:")), 2, 0);
  stereoscopy_layout->addWidget(m_3d_convergence, 2, 1);
  stereoscopy_layout->addWidget(m_3d_swap_eyes, 3, 0);

  main_layout->addWidget(enhancements_box);
  main_layout->addWidget(stereoscopy_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void EnhancementsWidget::ConnectWidgets()
{
  connect(m_aa_combo, &QComboBox::currentIndexChanged, [this](int) { SaveSettings(); });
  connect(m_texture_filtering_combo, &QComboBox::currentIndexChanged,
          [this](int) { SaveSettings(); });
  connect(m_output_resampling_combo, &QComboBox::currentIndexChanged,
          [this](int) { SaveSettings(); });
  connect(m_pp_effect, &QComboBox::currentIndexChanged, [this](int) { SaveSettings(); });
  connect(m_3d_mode, &QComboBox::currentIndexChanged, [this] {
    m_block_save = true;
    m_configure_color_correction->setEnabled(g_Config.backend_info.bSupportsPostProcessing);
    LoadPPShaders();
    m_block_save = false;

    SaveSettings();
  });
  connect(m_configure_color_correction, &QPushButton::clicked, this,
          &EnhancementsWidget::ConfigureColorCorrection);
  connect(m_configure_pp_effect, &QPushButton::clicked, this,
          &EnhancementsWidget::ConfigurePostProcessingShader);
}

void EnhancementsWidget::LoadPPShaders()
{
  std::vector<std::string> shaders = VideoCommon::PostProcessing::GetShaderList();
  if (g_Config.stereo_mode == StereoMode::Anaglyph)
  {
    shaders = VideoCommon::PostProcessing::GetAnaglyphShaderList();
  }
  else if (g_Config.stereo_mode == StereoMode::Passive)
  {
    shaders = VideoCommon::PostProcessing::GetPassiveShaderList();
  }

  m_pp_effect->clear();

  if (g_Config.stereo_mode != StereoMode::Anaglyph && g_Config.stereo_mode != StereoMode::Passive)
    m_pp_effect->addItem(tr("(off)"));

  auto selected_shader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);

  bool found = false;

  for (const auto& shader : shaders)
  {
    m_pp_effect->addItem(QString::fromStdString(shader));
    if (selected_shader == shader)
    {
      m_pp_effect->setCurrentIndex(m_pp_effect->count() - 1);
      found = true;
    }
  }

  if (g_Config.stereo_mode == StereoMode::Anaglyph && !found)
    m_pp_effect->setCurrentIndex(m_pp_effect->findText(QStringLiteral("dubois")));
  else if (g_Config.stereo_mode == StereoMode::Passive && !found)
    m_pp_effect->setCurrentIndex(m_pp_effect->findText(QStringLiteral("horizontal")));

  const bool supports_postprocessing = g_Config.backend_info.bSupportsPostProcessing;
  m_pp_effect->setEnabled(supports_postprocessing);

  m_pp_effect->setToolTip(supports_postprocessing ?
                              QString{} :
                              tr("%1 doesn't support this feature.")
                                  .arg(tr(g_video_backend->GetDisplayName().c_str())));

  VideoCommon::PostProcessingConfiguration pp_shader;
  if (selected_shader != "" && supports_postprocessing)
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
  m_texture_filtering_combo->setEnabled(Config::Get(Config::GFX_HACK_FAST_TEXTURE_SAMPLING));
  m_arbitrary_mipmap_detection->setEnabled(!Config::Get(Config::GFX_ENABLE_GPU_TEXTURE_DECODING));

  // Anti-Aliasing

  const u32 aa_selection = Config::Get(Config::GFX_MSAA);
  const bool ssaa = Config::Get(Config::GFX_SSAA);
  const int aniso = Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY);
  const TextureFilteringMode tex_filter_mode =
      Config::Get(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING);

  m_aa_combo->clear();

  for (const u32 aa_mode : g_Config.backend_info.AAModes)
  {
    if (aa_mode == 1)
      m_aa_combo->addItem(tr("None"), 1);
    else
      m_aa_combo->addItem(tr("%1x MSAA").arg(aa_mode), static_cast<int>(aa_mode));

    if (aa_mode == aa_selection && !ssaa)
      m_aa_combo->setCurrentIndex(m_aa_combo->count() - 1);
  }
  if (g_Config.backend_info.bSupportsSSAA)
  {
    for (const u32 aa_mode : g_Config.backend_info.AAModes)
    {
      if (aa_mode != 1)  // don't show "None" twice
      {
        // Mark SSAA using negative values in the variant
        m_aa_combo->addItem(tr("%1x SSAA").arg(aa_mode), -static_cast<int>(aa_mode));
        if (aa_mode == aa_selection && ssaa)
          m_aa_combo->setCurrentIndex(m_aa_combo->count() - 1);
      }
    }
  }

  m_aa_combo->setEnabled(m_aa_combo->count() > 1);

  switch (tex_filter_mode)
  {
  case TextureFilteringMode::Default:
    if (aniso >= 0 && aniso <= 4)
      m_texture_filtering_combo->setCurrentIndex(aniso);
    else
      m_texture_filtering_combo->setCurrentIndex(TEXTURE_FILTERING_DEFAULT);
    break;
  case TextureFilteringMode::Nearest:
    m_texture_filtering_combo->setCurrentIndex(TEXTURE_FILTERING_FORCE_NEAREST);
    break;
  case TextureFilteringMode::Linear:
    if (aniso >= 0 && aniso <= 4)
      m_texture_filtering_combo->setCurrentIndex(TEXTURE_FILTERING_FORCE_LINEAR + aniso);
    else
      m_texture_filtering_combo->setCurrentIndex(TEXTURE_FILTERING_FORCE_LINEAR);
    break;
  }

  // Resampling
  const OutputResamplingMode output_resampling_mode =
      Config::Get(Config::GFX_ENHANCE_OUTPUT_RESAMPLING);
  m_output_resampling_combo->setCurrentIndex(static_cast<int>(output_resampling_mode));

  m_output_resampling_combo->setEnabled(g_Config.backend_info.bSupportsPostProcessing);

  // Color Correction
  m_configure_color_correction->setEnabled(g_Config.backend_info.bSupportsPostProcessing);

  // Post Processing Shader
  LoadPPShaders();

  // Stereoscopy
  const bool supports_stereoscopy = g_Config.backend_info.bSupportsGeometryShaders;
  m_3d_mode->setEnabled(supports_stereoscopy);
  m_3d_convergence->setEnabled(supports_stereoscopy);
  m_3d_depth->setEnabled(supports_stereoscopy);
  m_3d_swap_eyes->setEnabled(supports_stereoscopy);

  m_hdr->setEnabled(g_Config.backend_info.bSupportsHDROutput);

  m_block_save = false;
}

void EnhancementsWidget::SaveSettings()
{
  if (m_block_save)
    return;

  const u32 aa_value = static_cast<u32>(std::abs(m_aa_combo->currentData().toInt()));
  const bool is_ssaa = m_aa_combo->currentData().toInt() < 0;

  Config::SetBaseOrCurrent(Config::GFX_MSAA, aa_value);
  Config::SetBaseOrCurrent(Config::GFX_SSAA, is_ssaa);

  const int texture_filtering_selection = m_texture_filtering_combo->currentData().toInt();
  switch (texture_filtering_selection)
  {
  case TEXTURE_FILTERING_DEFAULT:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 0);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Default);
    break;
  case TEXTURE_FILTERING_ANISO_2X:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 1);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Default);
    break;
  case TEXTURE_FILTERING_ANISO_4X:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 2);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Default);
    break;
  case TEXTURE_FILTERING_ANISO_8X:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 3);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Default);
    break;
  case TEXTURE_FILTERING_ANISO_16X:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 4);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Default);
    break;
  case TEXTURE_FILTERING_FORCE_NEAREST:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 0);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Nearest);
    break;
  case TEXTURE_FILTERING_FORCE_LINEAR:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 0);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Linear);
    break;
  case TEXTURE_FILTERING_FORCE_LINEAR_ANISO_2X:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 1);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Linear);
    break;
  case TEXTURE_FILTERING_FORCE_LINEAR_ANISO_4X:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 2);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Linear);
    break;
  case TEXTURE_FILTERING_FORCE_LINEAR_ANISO_8X:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 3);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Linear);
    break;
  case TEXTURE_FILTERING_FORCE_LINEAR_ANISO_16X:
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, 4);
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                             TextureFilteringMode::Linear);
    break;
  }

  const int output_resampling_selection = m_output_resampling_combo->currentData().toInt();
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_OUTPUT_RESAMPLING,
                           static_cast<OutputResamplingMode>(output_resampling_selection));

  const bool anaglyph = g_Config.stereo_mode == StereoMode::Anaglyph;
  const bool passive = g_Config.stereo_mode == StereoMode::Passive;
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER,
                           (!anaglyph && !passive && m_pp_effect->currentIndex() == 0) ?
                               "" :
                               m_pp_effect->currentText().toStdString());

  VideoCommon::PostProcessingConfiguration pp_shader;
  if (Config::Get(Config::GFX_ENHANCE_POST_SHADER) != "")
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
      QT_TR_NOOP("Controls the rendering resolution.<br><br>A high resolution greatly improves "
                 "visual quality, but also greatly increases GPU load and can cause issues in "
                 "certain games. Generally speaking, the lower the internal resolution, the "
                 "better performance will be.<br><br><dolphin_emphasis>If unsure, "
                 "select Native.</dolphin_emphasis>");
  static const char TR_ANTIALIAS_DESCRIPTION[] = QT_TR_NOOP(
      "Reduces the amount of aliasing caused by rasterizing 3D graphics, resulting "
      "in smoother edges on objects. Increases GPU load and sometimes causes graphical "
      "issues.<br><br>SSAA is significantly more demanding than MSAA, but provides top quality "
      "geometry anti-aliasing and also applies anti-aliasing to lighting, shader "
      "effects, and textures.<br><br><dolphin_emphasis>If unsure, select "
      "None.</dolphin_emphasis>");
  static const char TR_FORCE_TEXTURE_FILTERING_DESCRIPTION[] = QT_TR_NOOP(
      "Adjust the texture filtering. Anisotropic filtering enhances the visual quality of textures "
      "that are at oblique viewing angles. Force Nearest and Force Linear override the texture "
      "scaling filter selected by the game.<br><br>Any option except 'Default' will alter the look "
      "of the game's textures and might cause issues in a small number of "
      "games.<br><br>This option is incompatible with Manual Texture Sampling.<br><br>"
      "<dolphin_emphasis>If unsure, select 'Default'.</dolphin_emphasis>");
  static const char TR_OUTPUT_RESAMPLING_DESCRIPTION[] =
      QT_TR_NOOP("Affects how the game output is scaled to the window resolution."
                 "<br>The performance mostly depends on the number of samples each method uses."
                 "<br>Compared to SSAA, resampling is useful in case the output window"
                 "<br>resolution isn't a multiplier of the native emulation resolution."

                 "<br><br><b>Default</b> - [fastest]"
                 "<br>Internal GPU bilinear sampler which is not gamma corrected."
                 "<br>This setting might be ignored if gamma correction is forced on."

                 "<br><br><b>Bilinear</b> - [4 samples]"
                 "<br>Gamma corrected linear interpolation between pixels."

                 "<br><br><b>Bicubic</b> - [16 samples]"
                 "<br>Gamma corrected cubic interpolation between pixels."
                 "<br>Good when rescaling between close resolutions. i.e 1080p and 1440p."
                 "<br>Comes in various flavors:"
                 "<br><b>B-Spline</b>: Blurry, but avoids all lobing artifacts"
                 "<br><b>Mitchell-Netravali</b>: Good middle ground between blurry and lobing"
                 "<br><b>Catmull-Rom</b>: Sharper, but can cause lobing artifacts"

                 "<br><br><b>Sharp Bilinear</b> - [1-4 samples]"
                 "<br>Similarly to \"Nearest Neighbor\", it maintains a sharp look,"
                 "<br>but also does some blending to avoid shimmering."
                 "<br>Works best with 2D games at low resolutions."

                 "<br><br><b>Area Sampling</b> - [up to 324 samples]"
                 "<br>Weights pixels by the percentage of area they occupy. Gamma corrected."
                 "<br>Best for down scaling by more than 2x."

                 "<br><br><dolphin_emphasis>If unsure, select 'Default'.</dolphin_emphasis>");
  static const char TR_COLOR_CORRECTION_DESCRIPTION[] =
      QT_TR_NOOP("A group of features to make the colors more accurate, matching the color space "
                 "Wii and GC games were meant for.");
  static const char TR_POSTPROCESSING_DESCRIPTION[] =
      QT_TR_NOOP("Applies a post-processing effect after rendering a frame.<br><br "
                 "/><dolphin_emphasis>If unsure, select (off).</dolphin_emphasis>");
  static const char TR_SCALED_EFB_COPY_DESCRIPTION[] =
      QT_TR_NOOP("Greatly increases the quality of textures generated using render-to-texture "
                 "effects.<br><br>Slightly increases GPU load and causes relatively few graphical "
                 "issues. Raising the internal resolution will improve the effect of this setting. "
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_PER_PIXEL_LIGHTING_DESCRIPTION[] = QT_TR_NOOP(
      "Calculates lighting of 3D objects per-pixel rather than per-vertex, smoothing out the "
      "appearance of lit polygons and making individual triangles less noticeable.<br><br "
      "/>Rarely "
      "causes slowdowns or graphical issues.<br><br><dolphin_emphasis>If unsure, leave "
      "this unchecked.</dolphin_emphasis>");
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
  static const char TR_3D_MODE_DESCRIPTION[] = QT_TR_NOOP(
      "Selects the stereoscopic 3D mode. Stereoscopy allows a better feeling "
      "of depth if the necessary hardware is present. Heavily decreases "
      "emulation speed and sometimes causes issues.<br><br>Side-by-Side and Top-and-Bottom are "
      "used by most 3D TVs.<br>Anaglyph is used for Red-Cyan colored glasses.<br>HDMI 3D is "
      "used when the monitor supports 3D display resolutions.<br>Passive is another type of 3D "
      "used by some TVs.<br><br><dolphin_emphasis>If unsure, select Off.</dolphin_emphasis>");
  static const char TR_3D_DEPTH_DESCRIPTION[] = QT_TR_NOOP(
      "Controls the separation distance between the virtual cameras.<br><br>A higher "
      "value creates a stronger feeling of depth while a lower value is more comfortable.");
  static const char TR_3D_CONVERGENCE_DESCRIPTION[] = QT_TR_NOOP(
      "Controls the distance of the convergence plane. This is the distance at which "
      "virtual objects will appear to be in front of the screen.<br><br>A higher value creates "
      "stronger out-of-screen effects while a lower value is more comfortable.");
  static const char TR_3D_SWAP_EYES_DESCRIPTION[] = QT_TR_NOOP(
      "Swaps the left and right eye. Most useful in side-by-side stereoscopy "
      "mode.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_FORCE_24BIT_DESCRIPTION[] = QT_TR_NOOP(
      "Forces the game to render the RGB color channels in 24-bit, thereby increasing "
      "quality by reducing color banding.<br><br>Has no impact on performance and causes "
      "few graphical issues.<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_DISABLE_COPY_FILTER_DESCRIPTION[] = QT_TR_NOOP(
      "Disables the blending of adjacent rows when copying the EFB. This is known in "
      "some games as \"deflickering\" or \"smoothing\".<br><br>Disabling the filter has no "
      "effect on performance, but may result in a sharper image. Causes few "
      "graphical issues.<br><br><dolphin_emphasis>If unsure, leave this "
      "checked.</dolphin_emphasis>");
  static const char TR_ARBITRARY_MIPMAP_DETECTION_DESCRIPTION[] = QT_TR_NOOP(
      "Enables detection of arbitrary mipmaps, which some games use for special distance-based "
      "effects.<br><br>May have false positives that result in blurry textures at increased "
      "internal "
      "resolution, such as in games that use very low resolution mipmaps. Disabling this can also "
      "reduce stutter in games that frequently load new textures. This feature is not compatible "
      "with GPU Texture Decoding.<br><br><dolphin_emphasis>If unsure, leave this "
      "unchecked.</dolphin_emphasis>");
  static const char TR_HDR_DESCRIPTION[] = QT_TR_NOOP(
      "Enables scRGB HDR output (if supported by your graphics backend and monitor)."
      " Fullscreen might be required."
      "<br><br>This gives post process shaders more room for accuracy, allows \"AutoHDR\" "
      "post-process shaders to work, and allows to fully display the PAL and NTSC-J color spaces."
      "<br><br>Note that games still render in SDR internally."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  m_ir_combo->SetTitle(tr("Internal Resolution"));
  m_ir_combo->SetDescription(tr(TR_INTERNAL_RESOLUTION_DESCRIPTION));

  m_aa_combo->SetTitle(tr("Anti-Aliasing"));
  m_aa_combo->SetDescription(tr(TR_ANTIALIAS_DESCRIPTION));

  m_texture_filtering_combo->SetTitle(tr("Texture Filtering"));
  m_texture_filtering_combo->SetDescription(tr(TR_FORCE_TEXTURE_FILTERING_DESCRIPTION));

  m_output_resampling_combo->SetTitle(tr("Output Resampling"));
  m_output_resampling_combo->SetDescription(tr(TR_OUTPUT_RESAMPLING_DESCRIPTION));

  m_configure_color_correction->SetTitle(tr("Color Correction"));
  m_configure_color_correction->SetDescription(tr(TR_COLOR_CORRECTION_DESCRIPTION));

  m_pp_effect->SetTitle(tr("Post-Processing Effect"));
  m_pp_effect->SetDescription(tr(TR_POSTPROCESSING_DESCRIPTION));

  m_scaled_efb_copy->SetDescription(tr(TR_SCALED_EFB_COPY_DESCRIPTION));

  m_per_pixel_lighting->SetDescription(tr(TR_PER_PIXEL_LIGHTING_DESCRIPTION));

  m_widescreen_hack->SetDescription(tr(TR_WIDESCREEN_HACK_DESCRIPTION));

  m_disable_fog->SetDescription(tr(TR_REMOVE_FOG_DESCRIPTION));

  m_force_24bit_color->SetDescription(tr(TR_FORCE_24BIT_DESCRIPTION));

  m_disable_copy_filter->SetDescription(tr(TR_DISABLE_COPY_FILTER_DESCRIPTION));

  m_arbitrary_mipmap_detection->SetDescription(tr(TR_ARBITRARY_MIPMAP_DETECTION_DESCRIPTION));

  m_hdr->SetDescription(tr(TR_HDR_DESCRIPTION));

  m_3d_mode->SetTitle(tr("Stereoscopic 3D Mode"));
  m_3d_mode->SetDescription(tr(TR_3D_MODE_DESCRIPTION));

  m_3d_depth->SetTitle(tr("Depth"));
  m_3d_depth->SetDescription(tr(TR_3D_DEPTH_DESCRIPTION));

  m_3d_convergence->SetTitle(tr("Convergence"));
  m_3d_convergence->SetDescription(tr(TR_3D_CONVERGENCE_DESCRIPTION));

  m_3d_swap_eyes->SetDescription(tr(TR_3D_SWAP_EYES_DESCRIPTION));
}

void EnhancementsWidget::ConfigureColorCorrection()
{
  ColorCorrectionConfigWindow dialog(this);
  SetQWidgetWindowDecorations(&dialog);
  dialog.exec();
}

void EnhancementsWidget::ConfigurePostProcessingShader()
{
  const std::string shader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  PostProcessingConfigWindow dialog(this, shader);
  SetQWidgetWindowDecorations(&dialog);
  dialog.exec();
}

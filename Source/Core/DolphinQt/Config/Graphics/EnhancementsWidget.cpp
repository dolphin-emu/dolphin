// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"

#include <utility>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/CommonTypes.h"

#include "Core/Config/GraphicsSettings.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigFloatSlider.h"
#include "DolphinQt/Config/GameConfigWidget.h"
#include "DolphinQt/Config/Graphics/ColorCorrectionConfigWindow.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"
#include "DolphinQt/Config/Graphics/PostProcessingConfigWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipPushButton.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

EnhancementsWidget::EnhancementsWidget(GraphicsPane* gfx_pane)
    : m_game_layer{gfx_pane->GetConfigLayer()}
{
  CreateWidgets();
  LoadPostProcessingShaders();
  ConnectWidgets();
  AddDescriptions();

  // BackendChanged is called by parent on window creation.
  connect(gfx_pane, &GraphicsPane::BackendChanged, this, &EnhancementsWidget::OnBackendChanged);
  connect(gfx_pane, &GraphicsPane::UseFastTextureSamplingChanged, this, [this] {
    m_texture_filtering_combo->setEnabled(ReadSetting(Config::GFX_HACK_FAST_TEXTURE_SAMPLING));
  });
  connect(gfx_pane, &GraphicsPane::UseGPUTextureDecodingChanged, this, [this] {
    m_arbitrary_mipmap_detection->setEnabled(!ReadSetting(Config::GFX_ENABLE_GPU_TEXTURE_DECODING));
  });
}

constexpr int ANISO_1x = std::to_underlying(AnisotropicFilteringMode::Force1x);
constexpr int ANISO_2X = std::to_underlying(AnisotropicFilteringMode::Force2x);
constexpr int ANISO_4X = std::to_underlying(AnisotropicFilteringMode::Force4x);
constexpr int ANISO_8X = std::to_underlying(AnisotropicFilteringMode::Force8x);
constexpr int ANISO_16X = std::to_underlying(AnisotropicFilteringMode::Force16x);
constexpr int FILTERING_DEFAULT = std::to_underlying(TextureFilteringMode::Default);
constexpr int FILTERING_NEAREST = std::to_underlying(TextureFilteringMode::Nearest);
constexpr int FILTERING_LINEAR = std::to_underlying(TextureFilteringMode::Linear);

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
      std::max(ReadSetting(Config::GFX_EFB_SCALE), ReadSetting(Config::GFX_MAX_EFB_SCALE));
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

  m_ir_combo = new ConfigChoice(resolution_options, Config::GFX_EFB_SCALE, m_game_layer);
  m_ir_combo->setMaxVisibleItems(visible_resolution_option_count);

  m_antialiasing_combo = new ConfigComplexChoice(Config::GFX_MSAA, Config::GFX_SSAA, m_game_layer);
  m_antialiasing_combo->Add(tr("None"), (u32)1, false);

  m_texture_filtering_combo =
      new ConfigComplexChoice(Config::GFX_ENHANCE_MAX_ANISOTROPY,
                              Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING, m_game_layer);

  m_texture_filtering_combo->Add(tr("Default"), Config::DefaultState{}, FILTERING_DEFAULT);
  m_texture_filtering_combo->Add(tr("1x Anisotropic"), ANISO_1x, FILTERING_DEFAULT);
  m_texture_filtering_combo->Add(tr("2x Anisotropic"), ANISO_2X, FILTERING_DEFAULT);
  m_texture_filtering_combo->Add(tr("4x Anisotropic"), ANISO_4X, FILTERING_DEFAULT);
  m_texture_filtering_combo->Add(tr("8x Anisotropic"), ANISO_8X, FILTERING_DEFAULT);
  m_texture_filtering_combo->Add(tr("16x Anisotropic"), ANISO_16X, FILTERING_DEFAULT);
  m_texture_filtering_combo->Add(tr("Force Nearest and 1x Anisotropic "), ANISO_1x,
                                 FILTERING_NEAREST);
  m_texture_filtering_combo->Add(tr("Force Linear and 1x Anisotropic"), ANISO_1x, FILTERING_LINEAR);
  m_texture_filtering_combo->Add(tr("Force Linear and 2x Anisotropic"), ANISO_2X, FILTERING_LINEAR);
  m_texture_filtering_combo->Add(tr("Force Linear and 4x Anisotropic"), ANISO_4X, FILTERING_LINEAR);
  m_texture_filtering_combo->Add(tr("Force Linear and 8x Anisotropic"), ANISO_8X, FILTERING_LINEAR);
  m_texture_filtering_combo->Add(tr("Force Linear and 16x Anisotropic"), ANISO_16X,
                                 FILTERING_LINEAR);
  m_texture_filtering_combo->Refresh();
  m_texture_filtering_combo->setEnabled(ReadSetting(Config::GFX_HACK_FAST_TEXTURE_SAMPLING));

  m_output_resampling_combo = new ConfigChoice(
      {tr("Default"), tr("Bilinear"), tr("Bicubic: B-Spline"), tr("Bicubic: Mitchell-Netravali"),
       tr("Bicubic: Catmull-Rom"), tr("Sharp Bilinear"), tr("Area Sampling")},
      Config::GFX_ENHANCE_OUTPUT_RESAMPLING, m_game_layer);

  m_configure_color_correction = new ToolTipPushButton(tr("Configure"));

  // The post-processing effect "(off)" has the config value "", so we need to use the constructor
  // that sets ConfigStringChoice's m_text_is_data to false. m_post_processing_effect is cleared in
  // LoadPostProcessingShaders so it's pointless to fill it with real data here.
  const std::vector<std::pair<QString, QString>> separate_data_and_text;
  m_post_processing_effect =
      new ConfigStringChoice(separate_data_and_text, Config::GFX_ENHANCE_POST_SHADER, m_game_layer);
  m_configure_post_processing_effect = new NonDefaultQPushButton(tr("Configure"));
  m_configure_post_processing_effect->setDisabled(true);

  m_scaled_efb_copy =
      new ConfigBool(tr("Scaled EFB Copy"), Config::GFX_HACK_COPY_EFB_SCALED, m_game_layer);
  m_per_pixel_lighting =
      new ConfigBool(tr("Per-Pixel Lighting"), Config::GFX_ENABLE_PIXEL_LIGHTING, m_game_layer);

  m_widescreen_hack =
      new ConfigBool(tr("Widescreen Hack"), Config::GFX_WIDESCREEN_HACK, m_game_layer);
  m_disable_fog = new ConfigBool(tr("Disable Fog"), Config::GFX_DISABLE_FOG, m_game_layer);
  m_force_24bit_color =
      new ConfigBool(tr("Force 24-Bit Color"), Config::GFX_ENHANCE_FORCE_TRUE_COLOR, m_game_layer);
  m_disable_copy_filter = new ConfigBool(tr("Disable Copy Filter"),
                                         Config::GFX_ENHANCE_DISABLE_COPY_FILTER, m_game_layer);
  m_arbitrary_mipmap_detection =
      new ConfigBool(tr("Arbitrary Mipmap Detection"),
                     Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, m_game_layer);
  m_arbitrary_mipmap_detection->setEnabled(!ReadSetting(Config::GFX_ENABLE_GPU_TEXTURE_DECODING));
  m_hdr = new ConfigBool(tr("HDR Post-Processing"), Config::GFX_ENHANCE_HDR_OUTPUT, m_game_layer);

  int row = 0;
  enhancements_layout->addWidget(new QLabel(tr("Internal Resolution:")), row, 0);
  enhancements_layout->addWidget(m_ir_combo, row, 1, 1, -1);
  ++row;

  enhancements_layout->addWidget(new QLabel(tr("Anti-Aliasing:")), row, 0);
  enhancements_layout->addWidget(m_antialiasing_combo, row, 1, 1, -1);
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
  enhancements_layout->addWidget(m_post_processing_effect, row, 1);
  enhancements_layout->addWidget(m_configure_post_processing_effect, row, 2);
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
                               Config::GFX_STEREO_MODE, m_game_layer);
  m_3d_depth = new ConfigFloatSlider(0, Config::GFX_STEREO_DEPTH_MAXIMUM, Config::GFX_STEREO_DEPTH,
                                     1.0f, m_game_layer);
  m_3d_convergence = new ConfigFloatSlider(0, Config::GFX_STEREO_CONVERGENCE_MAXIMUM,
                                           Config::GFX_STEREO_CONVERGENCE, 0.01f, m_game_layer);
  m_3d_depth_value = new QLabel();
  m_3d_convergence_value = new QLabel();

  m_3d_swap_eyes = new ConfigBool(tr("Swap Eyes"), Config::GFX_STEREO_SWAP_EYES, m_game_layer);

  m_3d_per_eye_resolution = new ConfigBool(
      tr("Use Full Resolution Per Eye"), Config::GFX_STEREO_PER_EYE_RESOLUTION_FULL, m_game_layer);

  stereoscopy_layout->addWidget(new QLabel(tr("Stereoscopic 3D Mode:")), 0, 0);
  stereoscopy_layout->addWidget(m_3d_mode, 0, 1);
  stereoscopy_layout->addWidget(new ConfigFloatLabel(tr("Depth:"), m_3d_depth), 1, 0);
  stereoscopy_layout->addWidget(m_3d_depth, 1, 1);
  stereoscopy_layout->addWidget(m_3d_depth_value, 1, 2);
  stereoscopy_layout->addWidget(new ConfigFloatLabel(tr("Convergence:"), m_3d_convergence), 2, 0);
  stereoscopy_layout->addWidget(m_3d_convergence, 2, 1);
  stereoscopy_layout->addWidget(m_3d_convergence_value, 2, 2);
  stereoscopy_layout->addWidget(m_3d_swap_eyes, 3, 0);
  stereoscopy_layout->addWidget(m_3d_per_eye_resolution, 4, 0);

  m_3d_depth_value->setText(QString::asprintf("%.0f", m_3d_depth->GetValue()));
  m_3d_convergence_value->setText(QString::asprintf("%.2f", m_3d_convergence->GetValue()));

  auto current_stereo_mode = ReadSetting(Config::GFX_STEREO_MODE);
  if (current_stereo_mode != StereoMode::SBS && current_stereo_mode != StereoMode::TAB)
    m_3d_per_eye_resolution->hide();

  main_layout->addWidget(enhancements_box);
  main_layout->addWidget(stereoscopy_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void EnhancementsWidget::ConnectWidgets()
{
  connect(m_3d_mode, &QComboBox::currentIndexChanged, [this] {
    auto current_stereo_mode = ReadSetting(Config::GFX_STEREO_MODE);
    LoadPostProcessingShaders();

    if (current_stereo_mode == StereoMode::SBS || current_stereo_mode == StereoMode::TAB)
      m_3d_per_eye_resolution->show();
    else
      m_3d_per_eye_resolution->hide();
  });

  connect(m_post_processing_effect, &QComboBox::currentIndexChanged, this,
          &EnhancementsWidget::ShaderChanged);

  connect(m_configure_color_correction, &QPushButton::clicked, this,
          &EnhancementsWidget::ConfigureColorCorrection);
  connect(m_configure_post_processing_effect, &QPushButton::clicked, this,
          &EnhancementsWidget::ConfigurePostProcessingShader);

  connect(m_3d_depth, &ConfigFloatSlider::valueChanged, this,
          [this] { m_3d_depth_value->setText(QString::asprintf("%.0f", m_3d_depth->GetValue())); });
  connect(m_3d_convergence, &ConfigFloatSlider::valueChanged, this, [this] {
    m_3d_convergence_value->setText(QString::asprintf("%.2f", m_3d_convergence->GetValue()));
  });
}

template <typename T>
T EnhancementsWidget::ReadSetting(const Config::Info<T>& setting) const
{
  if (m_game_layer != nullptr)
    return m_game_layer->Get(setting);
  else
    return Config::Get(setting);
}

void EnhancementsWidget::LoadPostProcessingShaders()
{
  auto stereo_mode = ReadSetting(Config::GFX_STEREO_MODE);

  const QSignalBlocker blocker(m_post_processing_effect);
  m_post_processing_effect->clear();

  // Get shader list
  std::vector<std::string> shaders = VideoCommon::PostProcessing::GetShaderList();

  if (stereo_mode == StereoMode::Anaglyph)
    shaders = VideoCommon::PostProcessing::GetAnaglyphShaderList();
  else if (stereo_mode == StereoMode::Passive)
    shaders = VideoCommon::PostProcessing::GetPassiveShaderList();

  // Populate widget
  if (stereo_mode != StereoMode::Anaglyph && stereo_mode != StereoMode::Passive)
    m_post_processing_effect->addItem(tr("(off)"), QStringLiteral(""));

  auto selected_shader = ReadSetting(Config::GFX_ENHANCE_POST_SHADER);

  bool found = false;

  for (const auto& shader : shaders)
  {
    const QString name = QString::fromStdString(shader);
    m_post_processing_effect->addItem(name, name);
    if (selected_shader == shader)
    {
      m_post_processing_effect->setCurrentIndex(m_post_processing_effect->count() - 1);
      found = true;
    }
  }

  // Force a shader for StereoModes that require it.
  if (!found)
  {
    if (stereo_mode == StereoMode::Anaglyph)
      selected_shader = "dubois";
    else if (stereo_mode == StereoMode::Passive)
      selected_shader = "horizontal";
    else
      selected_shader = "";

    const int index =
        std::max(0, m_post_processing_effect->findData(QString::fromStdString(selected_shader)));
    m_post_processing_effect->setCurrentIndex(index);

    // Save forced shader, but avoid forcing an option into a game ini layer.
    if (m_game_layer == nullptr && ReadSetting(Config::GFX_ENHANCE_POST_SHADER) != selected_shader)
      Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, selected_shader);
  }

  m_post_processing_effect->Load();
  ShaderChanged();
}

void EnhancementsWidget::OnBackendChanged()
{
  m_output_resampling_combo->setEnabled(g_backend_info.bSupportsPostProcessing);
  m_configure_color_correction->setEnabled(g_backend_info.bSupportsPostProcessing);
  m_hdr->setEnabled(g_backend_info.bSupportsHDROutput);

  // Stereoscopy
  const bool supports_stereoscopy = g_backend_info.bSupportsGeometryShaders ||
                                    g_backend_info.bSupportsVSLayerOutput;
  m_3d_mode->setEnabled(supports_stereoscopy);
  m_3d_convergence->setEnabled(supports_stereoscopy);
  m_3d_depth->setEnabled(supports_stereoscopy);
  m_3d_swap_eyes->setEnabled(supports_stereoscopy);

  // PostProcessing
  const bool supports_postprocessing = g_backend_info.bSupportsPostProcessing;
  if (!supports_postprocessing)
  {
    m_configure_post_processing_effect->setEnabled(false);
    m_post_processing_effect->setEnabled(false);
    m_post_processing_effect->setToolTip(
        tr("%1 doesn't support this feature.").arg(tr(g_video_backend->GetDisplayName().c_str())));
  }
  else if (!m_post_processing_effect->isEnabled() && supports_postprocessing)
  {
    m_configure_post_processing_effect->setEnabled(true);
    m_post_processing_effect->setEnabled(true);
    m_post_processing_effect->setToolTip(QString{});
    LoadPostProcessingShaders();
  }

  UpdateAntialiasingOptions();
}

void EnhancementsWidget::ShaderChanged()
{
  auto shader = ReadSetting(Config::GFX_ENHANCE_POST_SHADER);

  if (shader == "(off)" || shader == "")
  {
    shader = "";

    // Setting a shader to null in a game ini could be confusing, as it won't be bolded. Remove it
    // instead.
    if (m_game_layer != nullptr)
      m_game_layer->DeleteKey(Config::GFX_ENHANCE_POST_SHADER.GetLocation());
    else
      Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, shader);
  }

  if (shader != "" && m_post_processing_effect->isEnabled())
  {
    VideoCommon::PostProcessingConfiguration pp_shader;
    pp_shader.LoadShader(shader);
    m_configure_post_processing_effect->setEnabled(pp_shader.HasOptions());
  }
  else
  {
    m_configure_post_processing_effect->setEnabled(false);
  }
}

void EnhancementsWidget::OnConfigChanged()
{
  // Only used for the GameConfigWidget. Bypasses graphics window signals and backend info due to it
  // being global.
  m_texture_filtering_combo->setEnabled(ReadSetting(Config::GFX_HACK_FAST_TEXTURE_SAMPLING));
  m_arbitrary_mipmap_detection->setEnabled(!ReadSetting(Config::GFX_ENABLE_GPU_TEXTURE_DECODING));
  UpdateAntialiasingOptions();

  // Needs to update after deleting a key for 3d settings.
  LoadPostProcessingShaders();
}

void EnhancementsWidget::UpdateAntialiasingOptions()
{
  const QSignalBlocker blocker(m_antialiasing_combo);

  m_antialiasing_combo->Reset();
  m_antialiasing_combo->Add(tr("None"), (u32)1, false);

  const std::vector<u32>& aa_modes = g_backend_info.AAModes;
  for (const u32 aa_mode : aa_modes)
  {
    if (aa_mode > 1)
      m_antialiasing_combo->Add(tr("%1x MSAA").arg(aa_mode), aa_mode, false);
  }

  if (g_backend_info.bSupportsSSAA)
  {
    for (const u32 aa_mode : aa_modes)
    {
      if (aa_mode > 1)
        m_antialiasing_combo->Add(tr("%1x SSAA").arg(aa_mode), aa_mode, true);
    }
  }

  m_antialiasing_combo->Refresh();

  // Backend info can't be populated in the local game settings window. Only enable local game AA
  // edits when the backend info is correct - global and local have the same backend.
  const bool good_info =
      m_game_layer == nullptr || !m_game_layer->Exists(Config::MAIN_GFX_BACKEND.GetLocation()) ||
      Config::Get(Config::MAIN_GFX_BACKEND) == m_game_layer->Get(Config::MAIN_GFX_BACKEND);

  m_antialiasing_combo->setEnabled(m_antialiasing_combo->count() > 1 && good_info);
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
      "of the game's textures and might cause issues in a small number of games.<br><br>This "
      "setting is disabled when Manual Texture Sampling is enabled.<br><br>"
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
                 "<br>Good when rescaling between close resolutions, e.g. 1080p and 1440p."
                 "<br>Comes in various flavors:"
                 "<br><b>B-Spline</b>: Blurry, but avoids all lobing artifacts"
                 "<br><b>Mitchell-Netravali</b>: Good middle ground between blurry and lobing"
                 "<br><b>Catmull-Rom</b>: Sharper, but can cause lobing artifacts"

                 "<br><br><b>Sharp Bilinear</b> - [1-4 samples]"
                 "<br>Similar to \"Nearest Neighbor\", it maintains a sharp look,"
                 "<br>but also does some blending to avoid shimmering."
                 "<br>Works best with 2D games at low resolutions."

                 "<br><br><b>Area Sampling</b> - [up to 324 samples]"
                 "<br>Weighs pixels by the percentage of area they occupy. Gamma corrected."
                 "<br>Best for downscaling by more than 2x."

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
  static const char TR_3D_PER_EYE_RESOLUTION_DESCRIPTION[] =
      QT_TR_NOOP("Whether each eye gets full or half image resolution when using side-by-side "
                 "or above-and-below 3D."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
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
      "reduce stutter in games that frequently load new textures.<br><br>This setting is disabled "
      "when GPU Texture Decoding is enabled.<br><br><dolphin_emphasis>If unsure, leave this "
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

  m_antialiasing_combo->SetTitle(tr("Anti-Aliasing"));
  m_antialiasing_combo->SetDescription(tr(TR_ANTIALIAS_DESCRIPTION));

  m_texture_filtering_combo->SetTitle(tr("Texture Filtering"));
  m_texture_filtering_combo->SetDescription(tr(TR_FORCE_TEXTURE_FILTERING_DESCRIPTION));

  m_output_resampling_combo->SetTitle(tr("Output Resampling"));
  m_output_resampling_combo->SetDescription(tr(TR_OUTPUT_RESAMPLING_DESCRIPTION));

  m_configure_color_correction->SetTitle(tr("Color Correction"));
  m_configure_color_correction->SetDescription(tr(TR_COLOR_CORRECTION_DESCRIPTION));

  m_post_processing_effect->SetTitle(tr("Post-Processing Effect"));
  m_post_processing_effect->SetDescription(tr(TR_POSTPROCESSING_DESCRIPTION));

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

  m_3d_per_eye_resolution->SetDescription(tr(TR_3D_PER_EYE_RESOLUTION_DESCRIPTION));

  m_3d_swap_eyes->SetDescription(tr(TR_3D_SWAP_EYES_DESCRIPTION));
}

void EnhancementsWidget::ConfigureColorCorrection()
{
  ColorCorrectionConfigWindow dialog(this);
  dialog.exec();
}

void EnhancementsWidget::ConfigurePostProcessingShader()
{
  const std::string shader = ReadSetting(Config::GFX_ENHANCE_POST_SHADER);
  PostProcessingConfigWindow dialog(this, shader);
  dialog.exec();
}

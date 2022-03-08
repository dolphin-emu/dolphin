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

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsChoice.h"
#include "DolphinQt/Config/Graphics/GraphicsSlider.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Config/Graphics/PostProcessingConfigWindow.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/Settings.h"

#include "UICommon/VideoUtils.h"

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
}

void EnhancementsWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // Enhancements
  auto* enhancements_box = new QGroupBox(tr("Enhancements"));
  auto* enhancements_layout = new QGridLayout();
  enhancements_box->setLayout(enhancements_layout);

  // Only display the first 8 scales, which most users will not go beyond.
  QStringList resolution_options{
      tr("Auto (Multiple of 640x528)"),      tr("Native (640x528)"),
      tr("2x Native (1280x1056) for 720p"),  tr("3x Native (1920x1584) for 1080p"),
      tr("4x Native (2560x2112) for 1440p"), tr("5x Native (3200x2640)"),
      tr("6x Native (3840x3168) for 4K"),    tr("7x Native (4480x3696)"),
      tr("8x Native (5120x4224) for 5K")};
  const int visible_resolution_option_count = static_cast<int>(resolution_options.size());

  // If the current scale is greater than the max scale in the ini, add sufficient options so that
  // when the settings are saved we don't lose the user-modified value from the ini.
  const int max_efb_scale =
      std::max(Config::Get(Config::GFX_EFB_SCALE), Config::Get(Config::GFX_MAX_EFB_SCALE));
  for (int scale = static_cast<int>(resolution_options.size()); scale <= max_efb_scale; scale++)
  {
    resolution_options.append(tr("%1x Native (%2x%3)")
                                  .arg(QString::number(scale),
                                       QString::number(static_cast<int>(EFB_WIDTH) * scale),
                                       QString::number(static_cast<int>(EFB_HEIGHT) * scale)));
  }

  m_ir_combo = new GraphicsChoice(resolution_options, Config::GFX_EFB_SCALE);
  m_ir_combo->setMaxVisibleItems(visible_resolution_option_count);

  m_aa_combo = new ToolTipComboBox();
  m_af_combo = new GraphicsChoice({tr("1x"), tr("2x"), tr("4x"), tr("8x"), tr("16x")},
                                  Config::GFX_ENHANCE_MAX_ANISOTROPY);

  m_pp_effect = new ToolTipComboBox();
  m_configure_pp_effect = new NonDefaultQPushButton(tr("Configure"));
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

  // Stereoscopy
  auto* stereoscopy_box = new QGroupBox(tr("Stereoscopy"));
  auto* stereoscopy_layout = new QGridLayout();
  stereoscopy_box->setLayout(stereoscopy_layout);

  m_3d_mode = new GraphicsChoice({tr("Off"), tr("Side-by-Side"), tr("Top-and-Bottom"),
                                  tr("Anaglyph"), tr("HDMI 3D"), tr("Passive")},
                                 Config::GFX_STEREO_MODE);
  m_3d_depth = new GraphicsSlider(0, Config::GFX_STEREO_DEPTH_MAXIMUM, Config::GFX_STEREO_DEPTH);
  m_3d_convergence = new GraphicsSlider(0, Config::GFX_STEREO_CONVERGENCE_MAXIMUM,
                                        Config::GFX_STEREO_CONVERGENCE, 100);
  m_3d_swap_eyes = new GraphicsBool(tr("Swap Eyes"), Config::GFX_STEREO_SWAP_EYES);

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
  connect(m_aa_combo, qOverload<int>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  connect(m_pp_effect, qOverload<int>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  connect(m_3d_mode, qOverload<int>(&QComboBox::currentIndexChanged), [this] {
    m_block_save = true;
    LoadPPShaders();
    m_block_save = false;

    SaveSettings();
  });
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

  // Stereoscopy
  const bool supports_stereoscopy = g_Config.backend_info.bSupportsGeometryShaders;
  m_3d_mode->setEnabled(supports_stereoscopy);
  m_3d_convergence->setEnabled(supports_stereoscopy);
  m_3d_depth->setEnabled(supports_stereoscopy);
  m_3d_swap_eyes->setEnabled(supports_stereoscopy);
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

  const bool anaglyph = g_Config.stereo_mode == StereoMode::Anaglyph;
  const bool passive = g_Config.stereo_mode == StereoMode::Passive;
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER,
                           (!anaglyph && !passive && m_pp_effect->currentIndex() == 0) ?
                               "(off)" :
                               m_pp_effect->currentText().toStdString());

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
  static const char TR_ANISOTROPIC_FILTERING_DESCRIPTION[] = QT_TR_NOOP(
      "Enables anisotropic filtering, which enhances the visual quality of textures that "
      "are at oblique viewing angles.<br><br>Might cause issues in a small "
      "number of games.<br><br><dolphin_emphasis>If unsure, select 1x.</dolphin_emphasis>");
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
      "Forces the game to output graphics for any aspect ratio. Use with \"Aspect Ratio\" set to "
      "\"Force 16:9\" to force 4:3-only games to run at 16:9.<br><br>Rarely produces good "
      "results and "
      "often partially breaks graphics and game UIs. Unnecessary (and detrimental) if using any "
      "AR/Gecko-code widescreen patches.<br><br><dolphin_emphasis>If unsure, leave "
      "this unchecked.</dolphin_emphasis>");
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
  static const char TR_FORCE_TEXTURE_FILTERING_DESCRIPTION[] =
      QT_TR_NOOP("Filters all textures, including any that the game explicitly set as "
                 "unfiltered.<br><br>May improve quality of certain textures in some games, but "
                 "will cause issues in others.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
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
      "checked.</dolphin_emphasis>");

  m_ir_combo->SetTitle(tr("Internal Resolution"));
  m_ir_combo->SetDescription(tr(TR_INTERNAL_RESOLUTION_DESCRIPTION));

  m_aa_combo->SetTitle(tr("Anti-Aliasing"));
  m_aa_combo->SetDescription(tr(TR_ANTIALIAS_DESCRIPTION));

  m_af_combo->SetTitle(tr("Anisotropic Filtering"));
  m_af_combo->SetDescription(tr(TR_ANISOTROPIC_FILTERING_DESCRIPTION));

  m_pp_effect->SetTitle(tr("Post-Processing Effect"));
  m_pp_effect->SetDescription(tr(TR_POSTPROCESSING_DESCRIPTION));

  m_scaled_efb_copy->SetDescription(tr(TR_SCALED_EFB_COPY_DESCRIPTION));

  m_per_pixel_lighting->SetDescription(tr(TR_PER_PIXEL_LIGHTING_DESCRIPTION));

  m_widescreen_hack->SetDescription(tr(TR_WIDESCREEN_HACK_DESCRIPTION));

  m_disable_fog->SetDescription(tr(TR_REMOVE_FOG_DESCRIPTION));

  m_force_24bit_color->SetDescription(tr(TR_FORCE_24BIT_DESCRIPTION));

  m_force_texture_filtering->SetDescription(tr(TR_FORCE_TEXTURE_FILTERING_DESCRIPTION));

  m_disable_copy_filter->SetDescription(tr(TR_DISABLE_COPY_FILTER_DESCRIPTION));

  m_arbitrary_mipmap_detection->SetDescription(tr(TR_ARBITRARY_MIPMAP_DETECTION_DESCRIPTION));

  m_3d_mode->SetTitle(tr("Stereoscopic 3D Mode"));
  m_3d_mode->SetDescription(tr(TR_3D_MODE_DESCRIPTION));

  m_3d_depth->SetTitle(tr("Depth"));
  m_3d_depth->SetDescription(tr(TR_3D_DEPTH_DESCRIPTION));

  m_3d_convergence->SetTitle(tr("Convergence"));
  m_3d_convergence->SetDescription(tr(TR_3D_CONVERGENCE_DESCRIPTION));

  m_3d_swap_eyes->SetDescription(tr(TR_3D_SWAP_EYES_DESCRIPTION));
}

void EnhancementsWidget::ConfigurePostProcessingShader()
{
  const std::string shader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  PostProcessingConfigWindow(this, shader).exec();
}

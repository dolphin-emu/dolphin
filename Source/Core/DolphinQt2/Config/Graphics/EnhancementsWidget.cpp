// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/EnhancementsWidget.h"

#include <cmath>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Config/Graphics/GraphicsBool.h"
#include "DolphinQt2/Config/Graphics/GraphicsChoice.h"
#include "DolphinQt2/Config/Graphics/GraphicsSlider.h"
#include "DolphinQt2/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt2/Settings.h"
#include "UICommon/VideoUtils.h"
#include "VideoCommon/PostProcessing.h"
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

  m_ir_combo = new GraphicsChoice(
      {tr("Auto (Window Size)"), tr("Auto (Multiple of 640x528)"), tr("Native (640x528)"),
       tr("1.5x Native (960x792)"), tr("2x Native (1280x1056) for 720p"),
       tr("2.5x Native (1600x1320)"), tr("3x Native (1920x1584) for 1080p"),
       tr("4x Native (2560x2112) for 1440p"), tr("5x Native (3200x2640)"),
       tr("6x Native (3840x3168) for 4K"), tr("7x Native (4480x3696)"),
       tr("8x Native (5120x4224) for 5K")},
      Config::GFX_EFB_SCALE);

  if (g_Config.iEFBScale > 11)
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
  m_scaled_efb_copy = new GraphicsBool(tr("Scaled EFB Copy"), Config::GFX_HACK_COPY_EFB_ENABLED);
  m_per_pixel_lighting =
      new GraphicsBool(tr("Per-Pixel Lighting"), Config::GFX_ENABLE_PIXEL_LIGHTING);
  m_force_texture_filtering =
      new GraphicsBool(tr("Force Texture Filtering"), Config::GFX_ENHANCE_FORCE_FILTERING);
  m_widescreen_hack = new GraphicsBool(tr("Widescreen Hack"), Config::GFX_WIDESCREEN_HACK);
  m_disable_fog = new GraphicsBool(tr("Disable Fog"), Config::GFX_DISABLE_FOG);
  m_force_24bit_color =
      new GraphicsBool(tr("Force 24-bit Color"), Config::GFX_ENHANCE_FORCE_TRUE_COLOR);

  enhancements_layout->addWidget(new QLabel(tr("Internal Resolution:")), 0, 0);
  enhancements_layout->addWidget(m_ir_combo, 0, 1, 1, -1);
  enhancements_layout->addWidget(new QLabel(tr("Anti-Aliasing:")), 1, 0);
  enhancements_layout->addWidget(m_aa_combo, 1, 1, 1, -1);
  enhancements_layout->addWidget(new QLabel(tr("Anisotropic Filtering:")), 2, 0);
  enhancements_layout->addWidget(m_af_combo, 2, 1, 1, -1);
  enhancements_layout->addWidget(new QLabel(tr("Post-Processing Effect:")), 3, 0);
  enhancements_layout->addWidget(m_pp_effect, 3, 1);
  enhancements_layout->addWidget(m_configure_pp_effect, 3, 2);

  enhancements_layout->addWidget(m_scaled_efb_copy, 4, 0);
  enhancements_layout->addWidget(m_per_pixel_lighting, 4, 1);
  enhancements_layout->addWidget(m_force_texture_filtering, 5, 0);
  enhancements_layout->addWidget(m_widescreen_hack, 5, 1);
  enhancements_layout->addWidget(m_disable_fog, 6, 0);
  enhancements_layout->addWidget(m_force_24bit_color, 6, 1);

  // Stereoscopy
  auto* stereoscopy_box = new QGroupBox(tr("Stereoscopy"));
  auto* stereoscopy_layout = new QGridLayout();
  stereoscopy_box->setLayout(stereoscopy_layout);

  m_3d_mode =
      new GraphicsChoice({tr("Off"), tr("Side-by-Side"), tr("Top-and-Bottom"), tr("Anaglyph")},
                         Config::GFX_STEREO_MODE);
  m_3d_depth = new GraphicsSlider(0, 100, Config::GFX_STEREO_DEPTH);
  m_3d_convergence = new GraphicsSlider(0, 200, Config::GFX_STEREO_CONVERGENCE, 100);
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
  connect(m_aa_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  connect(m_pp_effect, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
  connect(m_3d_mode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
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
  std::vector<std::string> shaders =
      g_Config.iStereoMode == STEREO_ANAGLYPH ?
          PostProcessingShaderImplementation::GetAnaglyphShaderList(
              g_Config.backend_info.api_type) :
          PostProcessingShaderImplementation::GetShaderList(g_Config.backend_info.api_type);

  m_pp_effect->clear();
  m_pp_effect->addItem(tr("(off)"));

  const auto selected_shader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);

  for (const auto& shader : shaders)
  {
    m_pp_effect->addItem(QString::fromStdString(shader));
    if (selected_shader == shader)
      m_pp_effect->setCurrentIndex(m_pp_effect->count() - 1);
  }

  PostProcessingShaderConfiguration pp_shader;
  if (selected_shader != "(off)")
  {
    pp_shader.LoadShader(selected_shader);
    m_configure_pp_effect->setEnabled(pp_shader.HasOptions());
  }
  bool supports_stereoscopy = g_Config.backend_info.bSupportsGeometryShaders;

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

  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER,
                           m_pp_effect->currentText().toStdString());

  PostProcessingShaderConfiguration pp_shader;
  if (Config::Get(Config::GFX_ENHANCE_POST_SHADER) != "(off)")
  {
    pp_shader.LoadShader(Config::Get(Config::GFX_ENHANCE_POST_SHADER));
    m_configure_pp_effect->setEnabled(pp_shader.HasOptions());
  }
  else
  {
    m_configure_pp_effect->setEnabled(false);
  }
}

void EnhancementsWidget::AddDescriptions()
{
  static const char* TR_INTERNAL_RESOLUTION_DESCRIPTION =
      QT_TR_NOOP("Specifies the resolution used to render at. A high resolution greatly improves "
                 "visual quality, but also greatly increases GPU load and can cause issues in "
                 "certain games.\n\"Multiple of 640x528\" will result in a size slightly larger "
                 "than \"Window Size\" but yield fewer issues. Generally speaking, the lower the "
                 "internal resolution is, the better your performance will be. Auto (Window Size), "
                 "1.5x, and 2.5x may cause issues in some games.\n\nIf unsure, select Native.");

  static const char* TR_ANTIALIAS_DESCRIPTION =
      QT_TR_NOOP("Reduces the amount of aliasing caused by rasterizing 3D graphics. This smooths "
                 "out jagged edges on objects.\nIncreases GPU load and sometimes causes graphical "
                 "issues. SSAA is significantly more demanding than MSAA, but provides top quality "
                 "geometry anti-aliasing and also applies anti-aliasing to lighting, shader "
                 "effects, and textures.\n\nIf unsure, select None.");

  static const char* TR_ANISOTROPIC_FILTERING_DESCRIPTION = QT_TR_NOOP(
      "Enable anisotropic filtering.\nEnhances visual quality of textures that are at oblique "
      "viewing angles.\nMight cause issues in a small number of games.\n\nIf unsure, select 1x.");

  static const char* TR_POSTPROCESSING_DESCRIPTION = QT_TR_NOOP(
      "Apply a post-processing effect after finishing a frame.\n\nIf unsure, select (off).");

  static const char* TR_SCALED_EFB_COPY_DESCRIPTION = QT_TR_NOOP(
      "Greatly increases quality of textures generated using render-to-texture "
      "effects.\nRaising the "
      "internal resolution will improve the effect of this setting.\nSlightly increases GPU "
      "load and "
      "causes relatively few graphical issues.\n\nIf unsure, leave this checked.");
  static const char* TR_PER_PIXEL_LIGHTING_DESCRIPTION = QT_TR_NOOP(
      "Calculates lighting of 3D objects per-pixel rather than per-vertex, smoothing out the "
      "appearance of lit polygons and making individual triangles less noticeable.\nRarely causes "
      "slowdowns or graphical issues.\n\nIf unsure, leave this unchecked.");
  static const char* TR_WIDESCREEN_HACK_DESCRIPTION = QT_TR_NOOP(
      "Forces the game to output graphics for any aspect ratio.\nUse with \"Aspect Ratio\" set to "
      "\"Force 16:9\" to force 4:3-only games to run at 16:9.\nRarely produces good results and "
      "often partially breaks graphics and game UIs.\nUnnecessary (and detrimental) if using any "
      "AR/Gecko-code widescreen patches.\n\nIf unsure, leave this unchecked.");
  static const char* TR_REMOVE_FOG_DESCRIPTION =
      QT_TR_NOOP("Makes distant objects more visible by removing fog, thus increasing the overall "
                 "detail.\nDisabling fog will break some games which rely on proper fog "
                 "emulation.\n\nIf unsure, leave this unchecked.");

  static const char* TR_3D_MODE_DESCRIPTION = QT_TR_NOOP(
      "Selects the stereoscopic 3D mode. Stereoscopy allows you to get a better feeling "
      "of depth if you have the necessary hardware.\nSide-by-Side and Top-and-Bottom are "
      "used by most 3D TVs.\nAnaglyph is used for Red-Cyan colored glasses.\nHDMI 3D is "
      "used when your monitor supports 3D display resolutions.\nHeavily decreases "
      "emulation speed and sometimes causes issues.\n\nIf unsure, select Off.");
  static const char* TR_3D_DEPTH_DESCRIPTION =
      QT_TR_NOOP("Controls the separation distance between the virtual cameras.\nA higher value "
                 "creates a stronger feeling of depth while a lower value is more comfortable.");
  static const char* TR_3D_CONVERGENCE_DESCRIPTION = QT_TR_NOOP(
      "Controls the distance of the convergence plane. This is the distance at which "
      "virtual objects will appear to be in front of the screen.\nA higher value creates "
      "stronger out-of-screen effects while a lower value is more comfortable.");
  static const char* TR_3D_SWAP_EYES_DESCRIPTION =
      QT_TR_NOOP("Swaps the left and right eye. Mostly useful if you want to view side-by-side "
                 "cross-eyed.\n\nIf unsure, leave this unchecked.");
  static const char* TR_FORCE_24BIT_DESCRIPTION =
      QT_TR_NOOP("Forces the game to render the RGB color channels in 24-bit, thereby increasing "
                 "quality by reducing color banding.\nIt has no impact on performance and causes "
                 "few graphical issues.\n\n\nIf unsure, leave this checked.");
  static const char* TR_FORCE_TEXTURE_FILTERING_DESCRIPTION =
      QT_TR_NOOP("Filter all textures, including any that the game explicitly set as "
                 "unfiltered.\nMay improve quality of certain textures in some games, but will "
                 "cause issues in others.\n\nIf unsure, leave this unchecked.");

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
  AddDescription(m_3d_mode, TR_3D_MODE_DESCRIPTION);
  AddDescription(m_3d_depth, TR_3D_DEPTH_DESCRIPTION);
  AddDescription(m_3d_convergence, TR_3D_CONVERGENCE_DESCRIPTION);
  AddDescription(m_3d_swap_eyes, TR_3D_SWAP_EYES_DESCRIPTION);
}

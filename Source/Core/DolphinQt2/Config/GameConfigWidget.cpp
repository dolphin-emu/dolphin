// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/GameConfigWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Config/Graphics/GraphicsSlider.h"
#include "UICommon/GameFile.h"

constexpr int DETERMINISM_NOT_SET_INDEX = 0;
constexpr int DETERMINISM_AUTO_INDEX = 1;
constexpr int DETERMINISM_NONE_INDEX = 2;
constexpr int DETERMINISM_FAKE_COMPLETION_INDEX = 3;

constexpr const char* DETERMINISM_NOT_SET_STRING = "";
constexpr const char* DETERMINISM_AUTO_STRING = "auto";
constexpr const char* DETERMINISM_NONE_STRING = "none";
constexpr const char* DETERMINISM_FAKE_COMPLETION_STRING = "fake-completion";

GameConfigWidget::GameConfigWidget(const UICommon::GameFile& game) : m_game(game)
{
  m_game_id = m_game.GetGameID();
  m_gameini_local_path =
      QString::fromStdString(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
  m_gameini_local = SConfig::LoadLocalGameIni(m_game_id, m_game.GetRevision());
  m_gameini_default = SConfig::LoadDefaultGameIni(m_game_id, m_game.GetRevision());

  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
}

void GameConfigWidget::CreateWidgets()
{
  m_refresh_config = new QPushButton(tr("Refresh"));
  m_edit_user_config = new QPushButton(tr("Edit User Config"));
  m_view_default_config = new QPushButton(tr("View Default Config"));

  // Core
  auto* core_box = new QGroupBox(tr("Core"));
  auto* core_layout = new QGridLayout;
  core_box->setLayout(core_layout);

  m_enable_dual_core = new QCheckBox(tr("Enable Dual Core"));
  m_enable_mmu = new QCheckBox(tr("Enable MMU"));
  m_enable_fprf = new QCheckBox(tr("Enable FPRF"));
  m_sync_gpu = new QCheckBox(tr("Synchronize GPU thread"));
  m_enable_fast_disc = new QCheckBox(tr("Speed up Disc Transfer Rate"));
  m_use_dsp_hle = new QCheckBox(tr("DSP HLE Emulation (fast)"));
  m_deterministic_dual_core = new QComboBox;

  for (const auto& item : {tr("Not Set"), tr("auto"), tr("none"), tr("fake-completion")})
    m_deterministic_dual_core->addItem(item);

  m_enable_mmu->setToolTip(tr(
      "Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = Fast)"));

  m_enable_fprf->setToolTip(tr("Enables Floating Point Result Flag calculation, needed for a few "
                               "games. (ON = Compatible, OFF = Fast)"));
  m_sync_gpu->setToolTip(tr("Synchronizes the GPU and CPU threads to help prevent random freezes "
                            "in Dual core mode. (ON = Compatible, OFF = Fast)"));
  m_enable_fast_disc->setToolTip(tr("Enable fast disc access. This can cause crashes and other "
                                    "problems in some games. (ON = Fast, OFF = Compatible)"));

  core_layout->addWidget(m_enable_dual_core, 0, 0);
  core_layout->addWidget(m_enable_mmu, 1, 0);
  core_layout->addWidget(m_enable_fprf, 2, 0);
  core_layout->addWidget(m_sync_gpu, 3, 0);
  core_layout->addWidget(m_enable_fast_disc, 4, 0);
  core_layout->addWidget(m_use_dsp_hle, 5, 0);
  core_layout->addWidget(new QLabel(tr("Deterministic dual core:")), 6, 0);
  core_layout->addWidget(m_deterministic_dual_core, 6, 1);

  // Stereoscopy
  auto* stereoscopy_box = new QGroupBox(tr("Stereoscopy"));
  auto* stereoscopy_layout = new QGridLayout;
  stereoscopy_box->setLayout(stereoscopy_layout);

  m_depth_slider = new QSlider(Qt::Horizontal);

  m_depth_slider->setMinimum(100);
  m_depth_slider->setMaximum(200);

  m_convergence_spin = new QSpinBox;
  m_convergence_spin->setMinimum(0);
  m_convergence_spin->setMaximum(INT32_MAX);
  m_use_monoscopic_shadows = new QCheckBox(tr("Monoscopic Shadows"));

  m_depth_slider->setToolTip(
      tr("This value is multiplied with the depth set in the graphics configuration."));
  m_convergence_spin->setToolTip(
      tr("This value is added to the convergence value set in the graphics configuration."));
  m_use_monoscopic_shadows->setToolTip(
      tr("Use a single depth buffer for both eyes. Needed for a few games."));

  stereoscopy_layout->addWidget(new QLabel(tr("Depth Percentage:")), 0, 0);
  stereoscopy_layout->addWidget(m_depth_slider, 0, 1);
  stereoscopy_layout->addWidget(new QLabel(tr("Convergence:")), 1, 0);
  stereoscopy_layout->addWidget(m_convergence_spin, 1, 1);
  stereoscopy_layout->addWidget(m_use_monoscopic_shadows, 2, 0);

  auto* settings_box = new QGroupBox(tr("Game-Specific Settings"));
  auto* settings_layout = new QVBoxLayout;
  settings_box->setLayout(settings_layout);

  settings_layout->addWidget(
      new QLabel(tr("These settings override core Dolphin settings.\nUndetermined means the game "
                    "uses Dolphin's setting.")));
  settings_layout->addWidget(core_box);
  settings_layout->addWidget(stereoscopy_box);

  m_state_combo = new QComboBox;

  for (const auto& item :
       {tr("Not Set"), tr("Broken"), tr("Intro"), tr("In Game"), tr("Playable"), tr("Perfect")})
    m_state_combo->addItem(item);

  m_state_comment_edit = new QLineEdit;

  auto* layout = new QGridLayout;

  auto* emulation_state = new QLabel(tr("Emulation State:"));

  layout->addWidget(settings_box, 0, 0, 1, -1);
  layout->addWidget(emulation_state, 1, 0);
  layout->addWidget(m_state_combo, 1, 1);
  layout->addWidget(m_state_comment_edit, 1, 2, 1, -1);

  auto* button_layout = new QHBoxLayout;
  button_layout->setMargin(0);

  layout->addLayout(button_layout, 2, 0, 1, -1);

  button_layout->addWidget(m_refresh_config);
  button_layout->addWidget(m_edit_user_config);
  button_layout->addWidget(m_view_default_config);

  for (QCheckBox* item : {m_enable_dual_core, m_enable_mmu, m_enable_fprf, m_sync_gpu,
                          m_enable_fast_disc, m_use_dsp_hle, m_use_monoscopic_shadows})
    item->setTristate(true);

  emulation_state->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_state_combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  setLayout(layout);
}

void GameConfigWidget::ConnectWidgets()
{
  // Buttons
  connect(m_refresh_config, &QPushButton::pressed, this, &GameConfigWidget::LoadSettings);
  connect(m_edit_user_config, &QPushButton::pressed, this, &GameConfigWidget::EditUserConfig);
  connect(m_view_default_config, &QPushButton::pressed, this, &GameConfigWidget::ViewDefaultConfig);

  // Settings
  connect(m_state_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &GameConfigWidget::SaveSettings);
  connect(m_state_comment_edit, &QLineEdit::editingFinished, this, &GameConfigWidget::SaveSettings);

  for (QCheckBox* box : {m_enable_dual_core, m_enable_mmu, m_enable_fprf, m_sync_gpu,
                         m_enable_fast_disc, m_use_dsp_hle, m_use_monoscopic_shadows})
    connect(box, &QCheckBox::toggled, this, &GameConfigWidget::SaveSettings);

  connect(m_deterministic_dual_core,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &GameConfigWidget::SaveSettings);
  connect(m_depth_slider, static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged), this,
          &GameConfigWidget::SaveSettings);
  connect(m_convergence_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &GameConfigWidget::SaveSettings);
}

void GameConfigWidget::LoadCheckBox(QCheckBox* checkbox, const std::string& section,
                                    const std::string& key)
{
  bool checked;

  if (m_gameini_local.GetOrCreateSection(section)->Get(key, &checked))
    return checkbox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);

  if (m_gameini_default.GetOrCreateSection(section)->Get(key, &checked))
    return checkbox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);

  checkbox->setCheckState(Qt::PartiallyChecked);
}

void GameConfigWidget::SaveCheckBox(QCheckBox* checkbox, const std::string& section,
                                    const std::string& key)
{
  // Delete any existing entries from the local gameini if checkbox is undetermined.
  // Otherwise, write the current value to the local gameini if the value differs from the default
  // gameini values.
  // Delete any existing entry from the local gameini if the value does not differ from the default
  // gameini value.

  if (checkbox->checkState() == Qt::PartiallyChecked)
  {
    m_gameini_local.DeleteKey(section, key);
    return;
  }

  bool checked = checkbox->checkState() == Qt::Checked;

  if (m_gameini_default.Exists(section, key))
  {
    bool default_value;
    m_gameini_default.GetOrCreateSection(section)->Get(key, &default_value);

    if (default_value != checked)
      m_gameini_local.GetOrCreateSection(section)->Set(key, checked);
    else
      m_gameini_local.DeleteKey(section, key);

    return;
  }

  m_gameini_local.GetOrCreateSection(section)->Set(key, checked);
}

void GameConfigWidget::LoadSettings()
{
  // Load state information
  m_state_combo->setCurrentIndex(m_game.GetEmuState());
  m_state_comment_edit->setText(QString::fromStdString(m_game.GetIssues()));

  // Load game-specific settings

  // Core
  LoadCheckBox(m_enable_dual_core, "Core", "CPUThread");
  LoadCheckBox(m_enable_mmu, "Core", "MMU");
  LoadCheckBox(m_enable_fprf, "Core", "FPRF");
  LoadCheckBox(m_sync_gpu, "Core", "SyncGPU");
  LoadCheckBox(m_enable_fast_disc, "Core", "FastDiscSpeed");
  LoadCheckBox(m_use_dsp_hle, "Core", "DSPHLE");

  std::string determinism_mode;

  int determinism_index = DETERMINISM_NOT_SET_INDEX;

  m_gameini_default.GetIfExists("Core", "GPUDeterminismMode", &determinism_mode);
  m_gameini_local.GetIfExists("Core", "GPUDeterminismMode", &determinism_mode);

  if (determinism_mode == DETERMINISM_AUTO_STRING)
  {
    determinism_index = DETERMINISM_AUTO_INDEX;
  }
  else if (determinism_mode == DETERMINISM_NONE_STRING)
  {
    determinism_index = DETERMINISM_NONE_INDEX;
  }
  else if (determinism_mode == DETERMINISM_FAKE_COMPLETION_STRING)
  {
    determinism_index = DETERMINISM_FAKE_COMPLETION_INDEX;
  }

  m_deterministic_dual_core->setCurrentIndex(determinism_index);

  // Stereoscopy
  int depth_percentage = 100;

  m_gameini_default.GetIfExists("Video_Stereoscopy", "StereoDepthPercentage", &depth_percentage);
  m_gameini_local.GetIfExists("Video_Stereoscopy", "StereoDepthPercentage", &depth_percentage);

  m_depth_slider->setValue(depth_percentage);

  int convergence = 0;

  m_gameini_default.GetIfExists("Video_Stereoscopy", "StereoConvergence", &convergence);
  m_gameini_local.GetIfExists("Video_Stereoscopy", "StereoConvergence", &convergence);

  m_convergence_spin->setValue(convergence);

  LoadCheckBox(m_use_monoscopic_shadows, "Video_Stereoscopy", "StereoEFBMonoDepth");
}

void GameConfigWidget::SaveSettings()
{
  // Save state information
  QString comment = m_state_comment_edit->text();
  int state = m_state_combo->currentIndex();

  if (comment != QString::fromStdString(m_game.GetIssues()))
    m_gameini_local.GetOrCreateSection("EmuState")->Set("EmulationIssues", comment.toStdString());

  if (state != m_game.GetEmuState())
    m_gameini_local.GetOrCreateSection("EmuState")->Set("EmulationStateId", state);

  // Save game-specific settings

  // Core
  SaveCheckBox(m_enable_dual_core, "Core", "CPUThread");
  SaveCheckBox(m_enable_mmu, "Core", "MMU");
  SaveCheckBox(m_enable_fprf, "Core", "FPRF");
  SaveCheckBox(m_sync_gpu, "Core", "SyncGPU");
  SaveCheckBox(m_enable_fast_disc, "Core", "FastDiscSpeed");
  SaveCheckBox(m_use_dsp_hle, "Core", "DSPHLE");

  int determinism_num = m_deterministic_dual_core->currentIndex();

  std::string determinism_mode = DETERMINISM_NOT_SET_STRING;

  switch (determinism_num)
  {
  case DETERMINISM_AUTO_INDEX:
    determinism_mode = DETERMINISM_AUTO_STRING;
    break;
  case DETERMINISM_NONE_INDEX:
    determinism_mode = DETERMINISM_NONE_STRING;
    break;
  case DETERMINISM_FAKE_COMPLETION_INDEX:
    determinism_mode = DETERMINISM_FAKE_COMPLETION_STRING;
    break;
  }

  if (determinism_mode != DETERMINISM_NOT_SET_STRING)
  {
    std::string default_mode = DETERMINISM_NOT_SET_STRING;
    if (!(m_gameini_default.GetIfExists("Core", "GPUDeterminismMode", &default_mode) &&
          default_mode == determinism_mode))
    {
      m_gameini_local.GetOrCreateSection("Core")->Set("GPUDeterminismMode", determinism_mode);
    }
  }

  // Stereoscopy
  int depth_percentage = m_depth_slider->value();

  if (depth_percentage != 100)
  {
    int default_value = 0;
    if (!(m_gameini_default.GetIfExists("Video_Stereoscopy", "StereoDepthPercentage",
                                        &default_value) &&
          default_value == depth_percentage))
    {
      m_gameini_local.GetOrCreateSection("Video_Stereoscopy")
          ->Set("StereoDepthPercentage", depth_percentage);
    }
  }

  int convergence = m_convergence_spin->value();
  if (convergence != 0)
  {
    int default_value = 0;
    if (!(m_gameini_default.GetIfExists("Video_Stereoscopy", "StereoConvergence", &default_value) &&
          default_value == convergence))
    {
      m_gameini_local.GetOrCreateSection("Video_Stereoscopy")
          ->Set("StereoConvergence", convergence);
    }
  }

  SaveCheckBox(m_use_monoscopic_shadows, "Video_Stereoscopy", "StereoEFBMonoDepth");

  bool success = m_gameini_local.Save(m_gameini_local_path.toStdString());

  // If the resulting file is empty, delete it. Kind of a hack, but meh.
  if (success && File::GetSize(m_gameini_local_path.toStdString()) == 0)
    File::Delete(m_gameini_local_path.toStdString());
}

void GameConfigWidget::EditUserConfig()
{
  QFile file(m_gameini_local_path);

  if (!file.exists())
  {
    file.open(QIODevice::WriteOnly);
    file.close();
  }

  QDesktopServices::openUrl(QUrl::fromLocalFile(m_gameini_local_path));
}

void GameConfigWidget::ViewDefaultConfig()
{
  for (const std::string& filename :
       ConfigLoaders::GetGameIniFilenames(m_game_id, m_game.GetRevision()))
  {
    QString path =
        QString::fromStdString(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename);

    if (QFile(path).exists())
      QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  }
}

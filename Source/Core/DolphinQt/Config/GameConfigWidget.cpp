// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GameConfigWidget.h"

#include <QFont>
#include <QGroupBox>
#include <QIcon>
#include <QTimer>
#include <QVBoxLayout>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/FileUtil.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigManager.h"
#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"
#include "DolphinQt/Config/ConfigControls/ConfigRadio.h"
#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"
#include "DolphinQt/Config/GameConfigEdit.h"
#include "DolphinQt/Config/Graphics/AdvancedWidget.h"
#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"
#include "DolphinQt/Config/Graphics/GeneralWidget.h"
#include "DolphinQt/Config/Graphics/HacksWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include "UICommon/GameFile.h"

static void PopulateTab(QTabWidget* tab, const std::string& path, std::string& game_id,
                        u16 revision, bool read_only)
{
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(game_id, revision))
  {
    const std::string ini_path = path + filename;
    if (File::Exists(ini_path))
    {
      auto* edit = new GameConfigEdit(nullptr, QString::fromStdString(ini_path), read_only);
      tab->addTab(edit, QString::fromStdString(filename));
    }
  }
}

GameConfigWidget::GameConfigWidget(const UICommon::GameFile& game) : m_game(game)
{
  m_game_id = m_game.GetGameID();

  m_gameini_local_path =
      QString::fromStdString(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");

  m_layer = std::make_unique<Config::Layer>(
      ConfigLoaders::GenerateLocalGameConfigLoader(m_game_id, m_game.GetRevision()));
  m_global_layer = std::make_unique<Config::Layer>(
      ConfigLoaders::GenerateGlobalGameConfigLoader(m_game_id, m_game.GetRevision()));

  CreateWidgets();
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &GameConfigWidget::LoadSettings);

  PopulateTab(m_default_tab, File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP, m_game_id,
              m_game.GetRevision(), true);
  PopulateTab(m_local_tab, File::GetUserPath(D_GAMESETTINGS_IDX), m_game_id, m_game.GetRevision(),
              false);

  bool game_id_tab = false;
  for (int i = 0; i < m_local_tab->count(); i++)
  {
    if (m_local_tab->tabText(i).toStdString() == m_game_id + ".ini")
      game_id_tab = true;
  }

  if (game_id_tab == false)
  {
    // Create new local game ini tab if none exists.
    auto* edit = new GameConfigEdit(
        nullptr, QString::fromStdString(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini"),
        false);
    m_local_tab->addTab(edit, QString::fromStdString(m_game_id + ".ini"));
  }

  // Fails to change font if it's directly called at this time. Is there a better workaround?
  QTimer::singleShot(100, this, [this]() {
    SetItalics();
    Config::OnConfigChanged();
  });
}

void GameConfigWidget::CreateWidgets()
{
  Config::Layer* layer = m_layer.get();
  // Core
  auto* core_box = new QGroupBox(tr("Core"));
  auto* core_layout = new QGridLayout;
  core_box->setLayout(core_layout);

  m_enable_dual_core = new ConfigBool(tr("Enable Dual Core"), Config::MAIN_CPU_THREAD, layer);
  m_enable_mmu = new ConfigBool(tr("Enable MMU"), Config::MAIN_MMU, layer);
  m_enable_fprf = new ConfigBool(tr("Enable FPRF"), Config::MAIN_FPRF, layer);
  m_sync_gpu = new ConfigBool(tr("Synchronize GPU thread"), Config::MAIN_SYNC_GPU, layer);
  m_emulate_disc_speed =
      new ConfigBool(tr("Emulate Disc Speed"), Config::MAIN_FAST_DISC_SPEED, layer, true);
  m_use_dsp_hle = new ConfigBool(tr("DSP HLE (fast)"), Config::MAIN_DSP_HLE, layer);

  const std::vector<std::string> choice{tr("auto").toStdString(), tr("none").toStdString(),
                                        tr("fake-completion").toStdString()};
  m_deterministic_dual_core =
      new ConfigStringChoice(choice, Config::MAIN_GPU_DETERMINISM_MODE, layer);

  m_enable_mmu->setToolTip(tr(
      "Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = Fast)"));

  m_enable_fprf->setToolTip(tr("Enables Floating Point Result Flag calculation, needed for a few "
                               "games. (ON = Compatible, OFF = Fast)"));
  m_sync_gpu->setToolTip(tr("Synchronizes the GPU and CPU threads to help prevent random freezes "
                            "in Dual core mode. (ON = Compatible, OFF = Fast)"));
  m_emulate_disc_speed->setToolTip(
      tr("Enable emulated disc speed. Disabling this can cause crashes "
         "and other problems in some games. "
         "(ON = Compatible, OFF = Unlocked)"));

  core_layout->addWidget(m_enable_dual_core, 0, 0);
  core_layout->addWidget(m_enable_mmu, 1, 0);
  core_layout->addWidget(m_enable_fprf, 2, 0);
  core_layout->addWidget(m_sync_gpu, 3, 0);
  core_layout->addWidget(m_emulate_disc_speed, 4, 0);
  core_layout->addWidget(m_use_dsp_hle, 5, 0);
  core_layout->addWidget(new QLabel(tr("Deterministic dual core:")), 6, 0);
  core_layout->addWidget(m_deterministic_dual_core, 6, 1);

  // Stereoscopy
  auto* stereoscopy_box = new QGroupBox(tr("Stereoscopy"));
  auto* stereoscopy_layout = new QGridLayout;
  stereoscopy_box->setLayout(stereoscopy_layout);

  m_depth_slider = new ConfigSlider(100, 200, Config::GFX_STEREO_DEPTH_PERCENTAGE, layer);
  m_convergence_spin = new ConfigInteger(0, INT32_MAX, Config::GFX_STEREO_CONVERGENCE, layer);
  m_use_monoscopic_shadows =
      new ConfigBool(tr("Monoscopic Shadows"), Config::GFX_STEREO_EFB_MONO_DEPTH, layer);

  m_depth_slider->setToolTip(
      tr("This value is multiplied with the depth set in the graphics configuration."));
  m_convergence_spin->setToolTip(
      tr("This value is added to the convergence value set in the graphics configuration."));
  m_use_monoscopic_shadows->setToolTip(
      tr("Use a single depth buffer for both eyes. Needed for a few games."));

  stereoscopy_layout->addWidget(new ConfigSliderLabel(tr("Depth Percentage:"), m_depth_slider), 0,
                                0);
  stereoscopy_layout->addWidget(m_depth_slider, 0, 1);
  stereoscopy_layout->addWidget(new ConfigIntegerLabel(tr("Convergence:"), m_convergence_spin), 1,
                                0);
  stereoscopy_layout->addWidget(m_convergence_spin, 1, 1);
  stereoscopy_layout->addWidget(m_use_monoscopic_shadows, 2, 0);

  auto* general_layout = new QVBoxLayout;
  general_layout->addWidget(core_box);
  general_layout->addWidget(stereoscopy_box);
  general_layout->addStretch();

  auto* general_widget = new QWidget;
  general_widget->setLayout(general_layout);

  // Editor tab
  auto* advanced_layout = new QVBoxLayout;

  auto* default_group = new QGroupBox(tr("Default Config (Read Only)"));
  auto* default_layout = new QVBoxLayout;
  m_default_tab = new QTabWidget;

  default_group->setLayout(default_layout);
  default_layout->addWidget(m_default_tab);

  auto* local_group = new QGroupBox(tr("User Config"));
  auto* local_layout = new QVBoxLayout;
  m_local_tab = new QTabWidget;

  local_group->setLayout(local_layout);
  local_layout->addWidget(m_local_tab);

  advanced_layout->addWidget(default_group);
  advanced_layout->addWidget(local_group);

  auto* advanced_widget = new QWidget;

  advanced_widget->setLayout(advanced_layout);

  auto* layout = new QVBoxLayout;
  auto* tab_widget = new QTabWidget;
  tab_widget->addTab(general_widget, tr("General"));

  // GFX settings tabs. Placed in a QWidget for consistent margins.
  auto* gfx_tab_holder = new QWidget;
  auto* gfx_layout = new QVBoxLayout;
  gfx_tab_holder->setLayout(gfx_layout);
  tab_widget->addTab(gfx_tab_holder, tr("Graphics"));

  auto* gfx_tabs = new QTabWidget;

  gfx_tabs->addTab(GetWrappedWidget(new GeneralWidget(this, m_layer.get()), this, 125, 100),
                   tr("General"));
  gfx_tabs->addTab(GetWrappedWidget(new EnhancementsWidget(this, m_layer.get()), this, 125, 100),
                   tr("Enhancements"));
  gfx_tabs->addTab(GetWrappedWidget(new HacksWidget(this, m_layer.get()), this, 125, 100),
                   tr("Hacks"));
  gfx_tabs->addTab(GetWrappedWidget(new AdvancedWidget(this, m_layer.get()), this, 125, 100),
                   tr("Advanced"));
  const int editor_index = tab_widget->addTab(advanced_widget, tr("Editor"));
  gfx_layout->addWidget(gfx_tabs);

  connect(tab_widget, &QTabWidget::currentChanged, this, [this, editor_index](int index) {
    // Update the ini editor after editing other tabs.
    if (index == editor_index)
    {
      // Layer only auto-saves when it is destroyed.
      m_layer->Save();

      // There can be multiple ini loaded for a game, only replace the one related to the game
      // ini being edited.
      for (int i = 0; i < m_local_tab->count(); i++)
      {
        if (m_local_tab->tabText(i).toStdString() == m_game_id + ".ini")
        {
          m_local_tab->removeTab(i);

          auto* edit = new GameConfigEdit(
              nullptr,
              QString::fromStdString(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini"),
              false);

          m_local_tab->insertTab(i, edit, QString::fromStdString(m_game_id + ".ini"));
          break;
        }
      }
    }

    // Update other tabs after using ini editor.
    if (m_prev_tab_index == editor_index)
    {
      // Load won't clear deleted keys, so everything is wiped before loading.
      m_layer->DeleteAllKeys();
      m_layer->Load();
      Config::OnConfigChanged();
    }

    m_prev_tab_index = index;
  });

  const QString help_msg = tr(
      "Italics mark default game settings, bold marks user settings.\nRight-click to remove user "
      "settings.\nGraphics tabs don't display the value of a default game setting.\nAnti-Aliasing "
      "settings are disabled when the global graphics backend doesn't "
      "match the game setting.");

  auto help_icon = style()->standardIcon(QStyle::SP_MessageBoxQuestion);
  auto* help_label = new QLabel(tr("These settings override core Dolphin settings."));
  help_label->setToolTip(help_msg);
  auto help_label_icon = new QLabel();
  help_label_icon->setPixmap(help_icon.pixmap(12, 12));
  help_label_icon->setToolTip(help_msg);
  auto* help_layout = new QHBoxLayout();
  help_layout->addWidget(help_label);
  help_layout->addWidget(help_label_icon);
  help_layout->addStretch();

  layout->addLayout(help_layout);
  layout->addWidget(tab_widget);
  setLayout(layout);
}

GameConfigWidget::~GameConfigWidget()
{
  // Destructor saves the layer to file.
  m_layer.reset();

  // If a game is running and the game properties window is closed, update local game layer with
  // any new changes. Not sure if doing it more frequently is safe.
  auto local_layer = Config::GetLayer(Config::LayerType::LocalGame);
  if (local_layer && SConfig::GetInstance().GetGameID() == m_game_id)
  {
    local_layer->DeleteAllKeys();
    local_layer->Load();
    Config::OnConfigChanged();
  }

  // Delete empty configs
  if (File::GetSize(m_gameini_local_path.toStdString()) == 0)
    File::Delete(m_gameini_local_path.toStdString());
}

void GameConfigWidget::LoadSettings()
{
  // Load globals
  auto update_bool = [this](auto config, bool reverse = false) {
    const Config::Location& setting = config->GetLocation();

    // Don't overwrite local with global
    if (m_layer->Exists(setting) || !m_global_layer->Exists(setting))
      return;

    std::optional<bool> value = m_global_layer->Get<bool>(config->GetLocation());

    if (value.has_value())
    {
      const QSignalBlocker blocker(config);
      config->setChecked(value.value() ^ reverse);
    }
  };

  auto update_int = [this](auto config) {
    const Config::Location& setting = config->GetLocation();

    if (m_layer->Exists(setting) || !m_global_layer->Exists(setting))
      return;

    std::optional<int> value = m_global_layer->Get<int>(setting);

    if (value.has_value())
    {
      const QSignalBlocker blocker(config);
      config->setValue(value.value());
    }
  };

  for (ConfigBool* config : {m_enable_dual_core, m_enable_mmu, m_enable_fprf, m_sync_gpu,
                             m_use_dsp_hle, m_use_monoscopic_shadows})
  {
    update_bool(config);
  }

  update_bool(m_emulate_disc_speed, true);

  update_int(m_depth_slider);
  update_int(m_convergence_spin);
}

void GameConfigWidget::SetItalics()
{
  // Mark system game settings with italics. Called once because it should never change.
  auto italics = [this](auto config) {
    if (!m_global_layer->Exists(config->GetLocation()))
      return;

    QFont ifont = config->font();
    ifont.setItalic(true);
    config->setFont(ifont);
  };

  for (auto* config : findChildren<ConfigBool*>())
    italics(config);
  for (auto* config : findChildren<ConfigSlider*>())
    italics(config);
  for (auto* config : findChildren<ConfigInteger*>())
    italics(config);
  for (auto* config : findChildren<ConfigRadioInt*>())
    italics(config);
  for (auto* config : findChildren<ConfigChoice*>())
    italics(config);
  for (auto* config : findChildren<ConfigStringChoice*>())
    italics(config);

  for (auto* config : findChildren<ConfigComplexChoice*>())
  {
    std::pair<Config::Location, Config::Location> location = config->GetLocation();
    if (m_global_layer->Exists(location.first) || m_global_layer->Exists(location.second))
    {
      QFont ifont = config->font();
      ifont.setItalic(true);
      config->setFont(ifont);
    }
  }
}

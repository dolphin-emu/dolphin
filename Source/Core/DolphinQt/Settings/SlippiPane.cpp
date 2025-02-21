#include "DolphinQt/Settings/SlippiPane.h"

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QSizePolicy>

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"

#ifndef IS_PLAYBACK
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_DeviceSlippi.h"
#include "SlippiPane.h"
#endif

SlippiPane::SlippiPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadConfig();
  ConnectLayout();
}

void SlippiPane::CreateLayout()
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  m_main_layout = new QVBoxLayout();
  setLayout(m_main_layout);

#ifndef IS_PLAYBACK
  // Replay Settings
  auto* replay_settings = new QGroupBox(tr("Replay Settings"));
  auto* replay_settings_layout = new QVBoxLayout();
  replay_settings->setLayout(replay_settings_layout);
  m_main_layout->addWidget(replay_settings);

  m_save_replays = new QCheckBox(tr("Save Slippi Replays"));
  m_save_replays->setToolTip(
      tr("Enable this to make Slippi automatically save .slp recordings of your games."));
  replay_settings_layout->addWidget(m_save_replays);

  m_monthly_replay_folders = new QCheckBox(tr("Save Replays to Monthly Subfolders"));
  m_monthly_replay_folders->setToolTip(
      tr("Enable this to save your replays into subfolders by month (YYYY-MM)."));
  replay_settings_layout->addWidget(m_monthly_replay_folders);

  auto* replay_folder_layout = new QGridLayout();
  m_replay_folder_edit = new QLineEdit();
  m_replay_folder_edit->setToolTip(tr("Choose where your Slippi replay files are saved."));
  connect(m_replay_folder_edit, &QLineEdit::editingFinished, [this] {
    Config::SetBase(Config::SLIPPI_REPLAY_DIR, m_replay_folder_edit->text().toStdString());
  });

  m_replay_folder_open = new NonDefaultQPushButton(QStringLiteral("..."));
  replay_folder_layout->addWidget(new QLabel(tr("Replay Folder:")), 0, 0);
  replay_folder_layout->addWidget(m_replay_folder_edit, 0, 1);
  replay_folder_layout->addWidget(m_replay_folder_open, 0, 2);
  replay_settings_layout->addLayout(replay_folder_layout);

  // Online Settings
  auto* online_settings = new QGroupBox(tr("Online Settings"));
  auto* online_settings_layout = new QFormLayout();
  online_settings->setLayout(online_settings_layout);
  m_main_layout->addWidget(online_settings);

  m_delay_spin = new QSpinBox();
  m_delay_spin->setFixedSize(45, 25);
  m_delay_spin->setRange(1, 9);
  m_delay_spin->setToolTip(tr("Leave this at 2 unless consistently playing on 120+ ping. "
                              "Increasing this can cause unplayable input delay, and lowering it "
                              "can cause visual artifacts/lag."));
  online_settings_layout->addRow(tr("Delay Frames:"), m_delay_spin);

  m_netplay_quick_chat_combo = new QComboBox();
  for (const auto& item : {tr("Enabled"), tr("Direct Only"), tr("Off")})
  {
    m_netplay_quick_chat_combo->addItem(item);
  }
  online_settings_layout->addRow(tr("Quick Chat:"), m_netplay_quick_chat_combo);

  m_netplay_port = new QSpinBox();
  m_netplay_port->setFixedSize(60, 25);
  QSizePolicy sp_retain = m_netplay_port->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_netplay_port->setSizePolicy(sp_retain);
  m_netplay_port->setRange(1000, 65535);
  m_force_netplay_port = new QCheckBox(tr("Force Netplay Port"));
  m_force_netplay_port->setToolTip(
      tr("Enable this to force Slippi to use a specific network port for online peer-to-peer "
         "connections."));

  auto* netplay_port_layout = new QGridLayout();
  netplay_port_layout->setColumnStretch(1, 1);
  netplay_port_layout->addWidget(m_force_netplay_port, 0, 0);
  netplay_port_layout->addWidget(m_netplay_port, 0, 1, Qt::AlignLeft);

  online_settings_layout->addRow(netplay_port_layout);

  // // i'd like to note that I hate everything about how this is organized for the next two
  // sections
  // // and a lot of the Qstring bullshit drives me up the wall.
  // auto* netplay_ip_edit = new QLineEdit();
  // netplay_ip_edit->setFixedSize(100, 25);
  // sp_retain = netplay_ip_edit->sizePolicy();
  // sp_retain.setRetainSizeWhenHidden(true);
  // netplay_ip_edit->setSizePolicy(sp_retain);
  // std::string ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
  //// You may want to use QRegularExpression for new code with Qt 5 (not mandatory).
  // QRegularExpression ipRegex(QString::fromStdString(
  //     "^" + ipRange + "(\\." + ipRange + ")" + "(\\." + ipRange + ")" + "(\\." + ipRange +
  //     ")$"));
  // QRegularExpressionValidator* ipValidator = new QRegularExpressionValidator(ipRegex, this);
  // netplay_ip_edit->setValidator(ipValidator);
  // auto lan_ip = SConfig::GetInstance().m_slippiLanIp;
  // netplay_ip_edit->setText(QString::fromStdString(lan_ip));
  // if (!SConfig::GetInstance().m_slippiForceLanIp)
  //{
  //   netplay_ip_edit->hide();
  // }
  // auto* enable_force_netplay_ip_checkbox = new QCheckBox(tr("Force Netplay IP"));
  // enable_force_netplay_ip_checkbox->setToolTip(
  //     tr("Enable this to force Slippi to use a specific LAN IP when connecting to users with a "
  //        "matching WAN IP. Should not be required for most users."));

  // enable_force_netplay_ip_checkbox->setChecked(SConfig::GetInstance().m_slippiForceLanIp);
  // connect(enable_force_netplay_ip_checkbox, &QCheckBox::toggled, this,
  //         [netplay_ip_edit](bool checked) {
  //           SConfig::GetInstance().m_slippiForceLanIp = checked;
  //           checked ? netplay_ip_edit->show() : netplay_ip_edit->hide();
  //         });
  // auto* netplay_ip_layout = new QGridLayout();
  // netplay_ip_layout->setColumnStretch(1, 1);
  // netplay_ip_layout->addWidget(enable_force_netplay_ip_checkbox, 0, 0);
  // netplay_ip_layout->addWidget(netplay_ip_edit, 0, 1, Qt::AlignLeft);

  // online_settings_layout->addRow(netplay_ip_layout);

  // Jukebox Settings
  auto* jukebox_settings = new QGroupBox(tr("Jukebox Settings (Beta)"));
  auto* jukebox_settings_layout = new QVBoxLayout();
  jukebox_settings->setLayout(jukebox_settings_layout);
  m_main_layout->addWidget(jukebox_settings);

  m_enable_jukebox = new QCheckBox(tr("Enable Music"));
  m_enable_jukebox->setToolTip(
      tr("Toggle in-game music for stages and menus. Changing this does not affect "
         "other audio like character hits or effects. Music will not play when "
         "using the Exclusive WASAPI audio backend."));
  jukebox_settings_layout->addWidget(m_enable_jukebox);

  auto* sfx_music_slider_layout = new QGridLayout;
  m_music_volume_slider = new QSlider(Qt::Horizontal);
  m_music_volume_slider->setRange(0, 100);
  auto* music_volume_label = new QLabel(tr("Music Volume:"));
  m_music_volume_percent = new QLabel(tr(""));
  m_music_volume_percent->setFixedWidth(40);

  sfx_music_slider_layout->addWidget(music_volume_label, 1, 0);
  sfx_music_slider_layout->addWidget(m_music_volume_slider, 1, 1, Qt::AlignVCenter);
  sfx_music_slider_layout->addWidget(m_music_volume_percent, 1, 2);

  jukebox_settings_layout->addLayout(sfx_music_slider_layout);

#else
  // Playback Settings
  auto* playback_settings = new QGroupBox(tr("Playback Settings"));
  auto* playback_settings_layout = new QVBoxLayout();
  playback_settings->setLayout(playback_settings_layout);
  m_main_layout->addWidget(playback_settings);

  auto* enable_playback_seek_checkbox = new QCheckBox(tr("Enable Seekbar"));
  char seekbarTooltip[] = "<html><head/><body><p>Enables video player style controls while "
                          "watching Slippi replays. Uses more cpu resources and can be stuttery. "
                          "Space: Pause/Play "
                          "Left/Right: Jump 5 seconds back/forward"
                          "Shift + Left/Right: Jump 20 seconds back/forward"
                          "Period (while paused): Advance one frame";
  enable_playback_seek_checkbox->setToolTip(tr(seekbarTooltip));
  playback_settings_layout->addWidget(enable_playback_seek_checkbox);
  enable_playback_seek_checkbox->setChecked(Config::Get(Config::SLIPPI_ENABLE_SEEK));
  connect(enable_playback_seek_checkbox, &QCheckBox::toggled, this,
          [](bool checked) { Config::SetBaseOrCurrent(Config::SLIPPI_ENABLE_SEEK, checked); });
#endif
}

void SlippiPane::LoadConfig()
{
#ifndef IS_PLAYBACK
  // Replay Settings
  auto save_replays = Config::Get(Config::SLIPPI_SAVE_REPLAYS);
  m_save_replays->setChecked(save_replays);
  m_monthly_replay_folders->setChecked(Config::Get(Config::SLIPPI_REPLAY_MONTHLY_FOLDERS));
  m_replay_folder_edit->setText(QString::fromStdString(Config::Get(Config::SLIPPI_REPLAY_DIR)));

  m_monthly_replay_folders->setDisabled(!save_replays);
  m_replay_folder_edit->setDisabled(!save_replays);
  m_replay_folder_open->setDisabled(!save_replays);

  // Online Settings
  m_delay_spin->setValue(Config::Get(Config::SLIPPI_ONLINE_DELAY));
  m_netplay_quick_chat_combo->setCurrentIndex(
      static_cast<u8>(Config::Get(Config::SLIPPI_ENABLE_QUICK_CHAT)));

  auto force_netplay_port = Config::Get(Config::SLIPPI_FORCE_NETPLAY_PORT);
  m_force_netplay_port->setChecked(force_netplay_port);
  m_netplay_port->setValue(Config::Get(Config::SLIPPI_NETPLAY_PORT));

  m_netplay_port->setDisabled(!force_netplay_port);

  // Jukebox Settings
  auto enable_jukebox = Config::Get(Config::SLIPPI_ENABLE_JUKEBOX);
  auto jukebox_volume = Config::Get(Config::SLIPPI_JUKEBOX_VOLUME);
  m_enable_jukebox->setChecked(enable_jukebox);
  m_music_volume_slider->setValue(jukebox_volume);
  m_music_volume_percent->setText(tr(" %1%").arg(jukebox_volume));

  m_music_volume_slider->setDisabled(!enable_jukebox);
#else
  // HOOKUP PLAYBACK STUFF
#endif
}

void SlippiPane::ConnectLayout()
{
#ifndef IS_PLAYBACK
  // Replay Settings
  connect(m_save_replays, &QCheckBox::toggled, this, &SlippiPane::SetSaveReplays);
  connect(m_monthly_replay_folders, &QCheckBox::toggled, this,
          [](bool checked) { Config::SetBase(Config::SLIPPI_REPLAY_MONTHLY_FOLDERS, checked); });
  connect(m_replay_folder_open, &QPushButton::clicked, this, &SlippiPane::BrowseReplayFolder);

  // Online Settings
  connect(m_delay_spin, qOverload<int>(&QSpinBox::valueChanged), this,
          [](int delay) { Config::SetBase(Config::SLIPPI_ONLINE_DELAY, delay); });
  connect(m_netplay_quick_chat_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [](int index) {
            Config::SetBase(Config::SLIPPI_ENABLE_QUICK_CHAT, static_cast<Slippi::Chat>(index));
          });
  connect(m_force_netplay_port, &QCheckBox::toggled, this, &SlippiPane::SetForceNetplayPort);
  connect(m_netplay_port, qOverload<int>(&QSpinBox::valueChanged), this,
          [](int port) { Config::SetBase(Config::SLIPPI_NETPLAY_PORT, port); });

  // Jukebox Settings
  connect(m_enable_jukebox, &QCheckBox::toggled, this, &SlippiPane::ToggleJukebox);
  connect(m_music_volume_slider, qOverload<int>(&QSlider::valueChanged), this,
          &SlippiPane::OnMusicVolumeUpdate);
#else
  // HOOKUP PLAYBACK STUFF
#endif
}

void SlippiPane::SetSaveReplays(bool checked)
{
  Config::SetBase(Config::SLIPPI_SAVE_REPLAYS, checked);
  m_monthly_replay_folders->setDisabled(!checked);
  m_replay_folder_edit->setDisabled(!checked);
  m_replay_folder_open->setDisabled(!checked);
}

void SlippiPane::BrowseReplayFolder()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
      this, tr("Select Replay Folder"),
      QString::fromStdString(Config::Get(Config::SLIPPI_REPLAY_DIR))));
  if (!dir.isEmpty())
  {
    m_replay_folder_edit->setText(dir);
    Config::SetBase(Config::SLIPPI_REPLAY_DIR, dir.toStdString());
  }
}

void SlippiPane::SetForceNetplayPort(bool checked)
{
  Config::SetBase(Config::SLIPPI_FORCE_NETPLAY_PORT, checked);
  m_netplay_port->setDisabled(!checked);
}

void SlippiPane::ToggleJukebox(bool checked)
{
  Config::SetBase(Config::SLIPPI_ENABLE_JUKEBOX, checked);
  m_music_volume_slider->setDisabled(!checked);

  if (Core::GetState(Core::System::GetInstance()) == Core::State::Running)
  {
    auto& system = Core::System::GetInstance();
    auto& exi_manager = system.GetExpansionInterface();
    ExpansionInterface::CEXISlippi* slippi_exi = static_cast<ExpansionInterface::CEXISlippi*>(
        exi_manager.GetDevice(ExpansionInterface::Slot::B));

    if (slippi_exi != nullptr)
      slippi_exi->ConfigureJukebox();
  }
}

void SlippiPane::OnMusicVolumeUpdate(int volume)
{
  Config::SetBase(Config::SLIPPI_JUKEBOX_VOLUME, volume);
  m_music_volume_percent->setText(tr(" %1%").arg(volume));
  if (Core::GetState(Core::System::GetInstance()) == Core::State::Running)
  {
    auto& system = Core::System::GetInstance();
    auto& exi_manager = system.GetExpansionInterface();
    ExpansionInterface::CEXISlippi* slippi_exi = static_cast<ExpansionInterface::CEXISlippi*>(
        exi_manager.GetDevice(ExpansionInterface::Slot::B));

    if (slippi_exi != nullptr)
      slippi_exi->UpdateJukeboxDolphinMusicVolume(volume);
  }
}

void SlippiPane::OnSaveConfig()
{
  Config::ConfigChangeCallbackGuard config_guard;

  auto& settings = SConfig::GetInstance();

  // Config::SetBase(Config::MAIN_AUTO_DISC_CHANGE, m_checkbox_auto_disc_change->isChecked());
  // Config::SetBaseOrCurrent(Config::MAIN_ENABLE_CHEATS, m_checkbox_cheats->isChecked());
  // Settings::Instance().SetFallbackRegion(
  //   UpdateFallbackRegionFromIndex(m_combobox_fallback_region->currentIndex()));

  settings.SaveSettings();
}

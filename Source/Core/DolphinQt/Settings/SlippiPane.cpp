#include "DolphinQt/Settings/SlippiPane.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

SlippiPane::SlippiPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadConfig();
  // ConnectLayout();
}

void SlippiPane::LoadConfig()
{
}
void SlippiPane::CreateLayout()
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  m_main_layout = new QVBoxLayout();
  setLayout(m_main_layout);

#ifndef IS_PLAYBACK
  // Replay Settings
  m_replay_settings = new QGroupBox(tr("Replay Settings"));
  m_replay_settings_layout = new QVBoxLayout();
  m_replay_settings->setLayout(m_replay_settings_layout);
  m_main_layout->addWidget(m_replay_settings);

  m_enable_replay_save = new QCheckBox(tr("Save Slippi Replays"));
  m_enable_replay_save->setToolTip(
      tr("Enable this to make Slippi automatically save .slp recordings of your games."));
  m_replay_settings_layout->addWidget(m_enable_replay_save);
  m_enable_replay_save->setChecked(Config::Get(Config::SLIPPI_SAVE_REPLAYS));
  // connect(enable_replay_save_checkbox, &QCheckBox::toggled, this,
  //   [](bool checked) { Config::SetBase(Config::SLIPPI_SAVE_REPLAYS, checked); });

  // auto* enable_monthly_replay_folders_checkbox =
  //     new QCheckBox(tr("Save Replays to Monthly Subfolders"));
  // enable_monthly_replay_folders_checkbox->setToolTip(
  //     tr("Enable this to save your replays into subfolders by month (YYYY-MM)."));
  // replay_settings_layout->addWidget(enable_monthly_replay_folders_checkbox);
  // enable_monthly_replay_folders_checkbox->setChecked(
  //     SConfig::GetInstance().m_slippiReplayMonthFolders);
  // connect(enable_monthly_replay_folders_checkbox, &QCheckBox::toggled, this,
  //         [](bool checked) { SConfig::GetInstance().m_slippiReplayMonthFolders = checked; });

  // auto* replay_folder_layout = new QGridLayout();
  // m_replay_folder_edit =
  //     new QLineEdit(QString::fromStdString(SConfig::GetInstance().m_strSlippiReplayDir));
  // m_replay_folder_edit->setToolTip(tr("Choose where your Slippi replay files are saved."));
  // connect(m_replay_folder_edit, &QLineEdit::editingFinished, [this] {
  //   SConfig::GetInstance().m_strSlippiReplayDir = m_replay_folder_edit->text().toStdString();
  // });
  // QPushButton* replay_folder_open = new QPushButton(QStringLiteral("..."));
  // connect(replay_folder_open, &QPushButton::clicked, this, &SlippiPane::BrowseReplayFolder);
  // replay_folder_layout->addWidget(new QLabel(tr("Replay Folder:")), 0, 0);
  // replay_folder_layout->addWidget(m_replay_folder_edit, 0, 1);
  // replay_folder_layout->addWidget(replay_folder_open, 0, 2);
  // replay_settings_layout->addLayout(replay_folder_layout);

  //// Online Settings
  // auto* online_settings = new QGroupBox(tr("Online Settings"));
  // auto* online_settings_layout = new QFormLayout();
  // online_settings->setLayout(online_settings_layout);
  // layout->addWidget(online_settings);

  // auto* delay_spin = new QSpinBox();
  // delay_spin->setFixedSize(30, 25);
  // delay_spin->setRange(1, 9);
  // delay_spin->setToolTip(tr("Leave this at 2 unless consistently playing on 120+ ping. "
  //                           "Increasing this can cause unplayable input delay, and lowering it "
  //                           "can cause visual artifacts/lag."));
  // online_settings_layout->addRow(tr("Delay Frames:"), delay_spin);
  // delay_spin->setValue(SConfig::GetInstance().m_slippiOnlineDelay);
  // connect(delay_spin, qOverload<int>(&QSpinBox::valueChanged), this,
  //         [](int delay) { SConfig::GetInstance().m_slippiOnlineDelay = delay; });

  // auto* netplay_quick_chat_combo = new QComboBox();
  // for (const auto& item : {tr("Enabled"), tr("Direct Only"), tr("Off")})
  //{
  //   netplay_quick_chat_combo->addItem(item);
  // }
  // connect(netplay_quick_chat_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
  //         [](int index) {
  //           SConfig::GetInstance().m_slippiEnableQuickChat = static_cast<Slippi::Chat>(index);
  //         });
  // netplay_quick_chat_combo->setCurrentIndex(
  //     static_cast<u32>(SConfig::GetInstance().m_slippiEnableQuickChat));

  // online_settings_layout->addRow(tr("Quick Chat:"), netplay_quick_chat_combo);

  //// i'd like to note that I hate everything about how this is organized for the next two sections
  //// and a lot of the Qstring bullshit drives me up the wall.
  // auto* netplay_port_spin = new QSpinBox();
  // netplay_port_spin->setFixedSize(60, 25);
  // QSizePolicy sp_retain = netplay_port_spin->sizePolicy();
  // sp_retain.setRetainSizeWhenHidden(true);
  // netplay_port_spin->setSizePolicy(sp_retain);
  // netplay_port_spin->setRange(1000, 65535);
  // netplay_port_spin->setValue(SConfig::GetInstance().m_slippiNetplayPort);
  // if (!SConfig::GetInstance().m_slippiForceNetplayPort)
  //{
  //   netplay_port_spin->hide();
  // }
  // auto* enable_force_netplay_port_checkbox = new QCheckBox(tr("Force Netplay Port"));
  // enable_force_netplay_port_checkbox->setToolTip(
  //     tr("Enable this to force Slippi to use a specific network port for online peer-to-peer "
  //        "connections."));

  // enable_force_netplay_port_checkbox->setChecked(SConfig::GetInstance().m_slippiForceNetplayPort);
  // connect(enable_force_netplay_port_checkbox, &QCheckBox::toggled, this,
  //         [netplay_port_spin](bool checked) {
  //           SConfig::GetInstance().m_slippiForceNetplayPort = checked;
  //           checked ? netplay_port_spin->show() : netplay_port_spin->hide();
  //         });
  // auto* netplay_port_layout = new QGridLayout();
  // netplay_port_layout->setColumnStretch(1, 1);
  // netplay_port_layout->addWidget(enable_force_netplay_port_checkbox, 0, 0);
  // netplay_port_layout->addWidget(netplay_port_spin, 0, 1, Qt::AlignLeft);

  // online_settings_layout->addRow(netplay_port_layout);

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

void SlippiPane::BrowseReplayFolder()
{
  // QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
  //     this, tr("Select Replay Folder"),
  //     QString::fromStdString(SConfig::GetInstance().m_strSlippiReplayDir)));
  // if (!dir.isEmpty())
  //{
  //   m_replay_folder_edit->setText(dir);
  //   SConfig::GetInstance().m_strSlippiReplayDir = dir.toStdString();
  // }
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

// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/NetPlay/NetPlayDialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QToolButton>

#include <sstream>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/TraversalClient.h"

#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/NetPlayServer.h"

#include "DolphinQt/GameList/GameList.h"
#include "DolphinQt/NetPlay/GameListDialog.h"
#include "DolphinQt/NetPlay/MD5Dialog.h"
#include "DolphinQt/NetPlay/PadMappingDialog.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VideoConfig.h"

NetPlayDialog::NetPlayDialog(QWidget* parent)
    : QDialog(parent), m_game_list_model(Settings::Instance().GetGameListModel())
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  setWindowTitle(tr("NetPlay"));
  setWindowIcon(Resources::GetAppIcon());

  m_pad_mapping = new PadMappingDialog(this);
  m_md5_dialog = new MD5Dialog(this);

  CreateChatLayout();
  CreatePlayersLayout();
  CreateMainLayout();
  ConnectWidgets();

  auto& settings = Settings::Instance().GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("netplaydialog/geometry")).toByteArray());
  m_splitter->restoreState(settings.value(QStringLiteral("netplaydialog/splitter")).toByteArray());
}

NetPlayDialog::~NetPlayDialog()
{
  auto& settings = Settings::Instance().GetQSettings();

  settings.setValue(QStringLiteral("netplaydialog/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("netplaydialog/splitter"), m_splitter->saveState());
}

void NetPlayDialog::CreateMainLayout()
{
  m_main_layout = new QGridLayout;
  m_game_button = new QPushButton;
  m_md5_button = new QToolButton;
  m_start_button = new QPushButton(tr("Start"));
  m_buffer_size_box = new QSpinBox;
  m_save_sd_box = new QCheckBox(tr("Write save/SD data"));
  m_load_wii_box = new QCheckBox(tr("Load Wii Save"));
  m_record_input_box = new QCheckBox(tr("Record inputs"));
  m_reduce_polling_rate_box = new QCheckBox(tr("Reduce Polling Rate"));
  m_buffer_label = new QLabel(tr("Buffer:"));
  m_quit_button = new QPushButton(tr("Quit"));
  m_splitter = new QSplitter(Qt::Horizontal);

  m_game_button->setDefault(false);
  m_game_button->setAutoDefault(false);

  auto* default_button = new QAction(tr("Calculate MD5 hash"), m_md5_button);

  auto* menu = new QMenu(this);

  auto* other_game_button = new QAction(tr("Other game"), this);
  auto* sdcard_button = new QAction(tr("SD Card"), this);

  menu->addAction(other_game_button);
  menu->addAction(sdcard_button);

  connect(default_button, &QAction::triggered,
          [this] { Settings::Instance().GetNetPlayServer()->ComputeMD5(m_current_game); });
  connect(other_game_button, &QAction::triggered, [this] {
    GameListDialog gld(this);

    if (gld.exec() == QDialog::Accepted)
    {
      Settings::Instance().GetNetPlayServer()->ComputeMD5(gld.GetSelectedUniqueID().toStdString());
    }
  });
  connect(sdcard_button, &QAction::triggered,
          [] { Settings::Instance().GetNetPlayServer()->ComputeMD5(WII_SDCARD); });

  m_md5_button->setDefaultAction(default_button);
  m_md5_button->setPopupMode(QToolButton::MenuButtonPopup);
  m_md5_button->setMenu(menu);

  m_reduce_polling_rate_box->setToolTip(
      tr("This will reduce bandwidth usage by polling GameCube controllers only twice per frame. "
         "Does not affect Wii Remotes."));

  m_main_layout->addWidget(m_game_button, 0, 0);
  m_main_layout->addWidget(m_md5_button, 0, 1);
  m_main_layout->addWidget(m_splitter, 1, 0, 1, -1);

  m_splitter->addWidget(m_chat_box);
  m_splitter->addWidget(m_players_box);

  auto* options_widget = new QHBoxLayout;

  options_widget->addWidget(m_start_button);
  options_widget->addWidget(m_buffer_label);
  options_widget->addWidget(m_buffer_size_box);
  options_widget->addWidget(m_save_sd_box);
  options_widget->addWidget(m_load_wii_box);
  options_widget->addWidget(m_record_input_box);
  options_widget->addWidget(m_reduce_polling_rate_box);
  options_widget->addWidget(m_quit_button);
  m_main_layout->addLayout(options_widget, 2, 0, 1, -1, Qt::AlignRight);

  setLayout(m_main_layout);
}

void NetPlayDialog::CreateChatLayout()
{
  m_chat_box = new QGroupBox(tr("Chat"));
  m_chat_edit = new QTextBrowser;
  m_chat_type_edit = new QLineEdit;
  m_chat_send_button = new QPushButton(tr("Send"));

  m_chat_send_button->setDefault(false);
  m_chat_send_button->setAutoDefault(false);

  m_chat_edit->setReadOnly(true);

  auto* layout = new QGridLayout;

  layout->addWidget(m_chat_edit, 0, 0, 1, -1);
  layout->addWidget(m_chat_type_edit, 1, 0);
  layout->addWidget(m_chat_send_button, 1, 1);

  m_chat_box->setLayout(layout);
}

void NetPlayDialog::CreatePlayersLayout()
{
  m_players_box = new QGroupBox(tr("Players"));
  m_room_box = new QComboBox;
  m_hostcode_label = new QLabel;
  m_hostcode_action_button = new QPushButton(tr("Copy"));
  m_players_list = new QTableWidget;
  m_kick_button = new QPushButton(tr("Kick Player"));
  m_assign_ports_button = new QPushButton(tr("Assign Controller Ports"));

  m_players_list->setColumnCount(5);
  m_players_list->verticalHeader()->hide();
  m_players_list->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_players_list->horizontalHeader()->setStretchLastSection(true);

  for (int i = 0; i < 4; i++)
    m_players_list->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

  auto* layout = new QGridLayout;

  layout->addWidget(m_room_box, 0, 0);
  layout->addWidget(m_hostcode_label, 0, 1);
  layout->addWidget(m_hostcode_action_button, 0, 2);
  layout->addWidget(m_players_list, 1, 0, 1, -1);
  layout->addWidget(m_kick_button, 2, 0, 1, -1);
  layout->addWidget(m_assign_ports_button, 3, 0, 1, -1);

  m_players_box->setLayout(layout);
}

void NetPlayDialog::ConnectWidgets()
{
  // Players
  connect(m_room_box, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &NetPlayDialog::UpdateGUI);
  connect(m_hostcode_action_button, &QPushButton::clicked, [this] {
    if (m_is_copy_button_retry && m_room_box->currentIndex() == 0)
      g_TraversalClient->ReconnectToServer();
    else
      QApplication::clipboard()->setText(m_hostcode_label->text());
  });
  connect(m_players_list, &QTableWidget::itemSelectionChanged, [this] {
    int row = m_players_list->currentRow();
    m_kick_button->setEnabled(row > 0 &&
                              !m_players_list->currentItem()->data(Qt::UserRole).isNull());
  });
  connect(m_kick_button, &QPushButton::clicked, [this] {
    auto id = m_players_list->currentItem()->data(Qt::UserRole).toInt();
    Settings::Instance().GetNetPlayServer()->KickPlayer(id);
  });
  connect(m_assign_ports_button, &QPushButton::clicked, [this] {
    m_pad_mapping->exec();

    Settings::Instance().GetNetPlayServer()->SetPadMapping(m_pad_mapping->GetGCPadArray());
    Settings::Instance().GetNetPlayServer()->SetWiimoteMapping(m_pad_mapping->GetWiimoteArray());
  });

  // Chat
  connect(m_chat_send_button, &QPushButton::clicked, this, &NetPlayDialog::OnChat);
  connect(m_chat_type_edit, &QLineEdit::returnPressed, this, &NetPlayDialog::OnChat);

  // Other
  connect(m_buffer_size_box, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          [this](int value) {
            if (value == m_buffer_size)
              return;

            if (Settings::Instance().GetNetPlayServer() != nullptr)
              Settings::Instance().GetNetPlayServer()->AdjustPadBufferSize(value);
          });

  connect(m_start_button, &QPushButton::clicked, this, &NetPlayDialog::OnStart);
  connect(m_quit_button, &QPushButton::clicked, this, &NetPlayDialog::reject);

  connect(m_game_button, &QPushButton::clicked, [this] {
    GameListDialog gld(this);
    if (gld.exec() == QDialog::Accepted)
    {
      auto unique_id = gld.GetSelectedUniqueID();
      Settings::Instance().GetNetPlayServer()->ChangeGame(unique_id.toStdString());
    }
  });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [=](Core::State state) {
    if (isVisible())
    {
      GameStatusChanged(state != Core::State::Uninitialized);
      if (state == Core::State::Uninitialized)
        DisplayMessage(tr("Stopped game"), "red");
    }
  });
}

void NetPlayDialog::OnChat()
{
  QueueOnObject(this, [this] {
    auto msg = m_chat_type_edit->text().toStdString();
    Settings::Instance().GetNetPlayClient()->SendChatMessage(msg);
    m_chat_type_edit->clear();

    DisplayMessage(QStringLiteral("%1: %2").arg(QString::fromStdString(m_nickname).toHtmlEscaped(),
                                                QString::fromStdString(msg).toHtmlEscaped()),
                   "blue");
  });
}

void NetPlayDialog::OnStart()
{
  if (!Settings::Instance().GetNetPlayClient()->DoAllPlayersHaveGame())
  {
    if (QMessageBox::question(this, tr("Warning"),
                              tr("Not all players have the game. Do you really want to start?")) ==
        QMessageBox::No)
      return;
  }

  NetSettings settings;

  // Copy all relevant settings
  SConfig& instance = SConfig::GetInstance();
  settings.m_CPUthread = instance.bCPUThread;
  settings.m_CPUcore = instance.cpu_core;
  settings.m_EnableCheats = instance.bEnableCheats;
  settings.m_SelectedLanguage = instance.SelectedLanguage;
  settings.m_OverrideGCLanguage = instance.bOverrideGCLanguage;
  settings.m_ProgressiveScan = Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN);
  settings.m_PAL60 = Config::Get(Config::SYSCONF_PAL60);
  settings.m_DSPHLE = instance.bDSPHLE;
  settings.m_DSPEnableJIT = instance.m_DSPEnableJIT;
  settings.m_WriteToMemcard = m_save_sd_box->isChecked();
  settings.m_CopyWiiSave = m_load_wii_box->isChecked();
  settings.m_OCEnable = instance.m_OCEnable;
  settings.m_OCFactor = instance.m_OCFactor;
  settings.m_ReducePollingRate = m_reduce_polling_rate_box->isChecked();
  settings.m_EXIDevice[0] = instance.m_EXIDevice[0];
  settings.m_EXIDevice[1] = instance.m_EXIDevice[1];

  Settings::Instance().GetNetPlayServer()->SetNetSettings(settings);
  Settings::Instance().GetNetPlayServer()->StartGame();
}

void NetPlayDialog::reject()
{
  if (QMessageBox::question(this, tr("Confirmation"),
                            tr("Are you sure you want to quit NetPlay?")) == QMessageBox::Yes)
  {
    QDialog::reject();
  }
}

void NetPlayDialog::show(std::string nickname, bool use_traversal)
{
  m_nickname = nickname;
  m_use_traversal = use_traversal;
  m_buffer_size = 0;

  m_room_box->clear();
  m_chat_edit->clear();
  m_chat_type_edit->clear();

  bool is_hosting = Settings::Instance().GetNetPlayServer() != nullptr;

  if (is_hosting)
  {
    if (use_traversal)
      m_room_box->addItem(tr("Room ID"));

    for (const auto& iface : Settings::Instance().GetNetPlayServer()->GetInterfaceSet())
    {
      const auto interface = QString::fromStdString(iface);
      m_room_box->addItem(iface == "!local!" ? tr("Local") : interface, interface);
    }
  }

  m_start_button->setHidden(!is_hosting);
  m_save_sd_box->setHidden(!is_hosting);
  m_load_wii_box->setHidden(!is_hosting);
  m_reduce_polling_rate_box->setHidden(!is_hosting);
  m_buffer_size_box->setHidden(!is_hosting);
  m_buffer_label->setHidden(!is_hosting);
  m_kick_button->setHidden(!is_hosting);
  m_assign_ports_button->setHidden(!is_hosting);
  m_md5_button->setHidden(!is_hosting);
  m_room_box->setHidden(!is_hosting);
  m_hostcode_label->setHidden(!is_hosting);
  m_hostcode_action_button->setHidden(!is_hosting);
  m_game_button->setEnabled(is_hosting);
  m_kick_button->setEnabled(false);

  QDialog::show();
  UpdateGUI();
}

void NetPlayDialog::UpdateGUI()
{
  auto* client = Settings::Instance().GetNetPlayClient();

  // Update Player List
  const auto players = client->GetPlayers();
  const auto player_count = static_cast<int>(players.size());

  int selection_pid = m_players_list->currentItem() ?
                          m_players_list->currentItem()->data(Qt::UserRole).toInt() :
                          -1;

  m_players_list->clear();
  m_players_list->setHorizontalHeaderLabels(
      {tr("Player"), tr("Game Status"), tr("Ping"), tr("Mapping"), tr("Revision")});
  m_players_list->setRowCount(player_count);

  const auto get_mapping_string = [](const Player* player, const PadMappingArray& array) {
    std::string str;
    for (size_t i = 0; i < array.size(); i++)
    {
      if (player->pid == array[i])
        str += std::to_string(i + 1);
      else
        str += '-';
    }

    return '|' + str + '|';
  };

  static const std::map<PlayerGameStatus, QString> player_status{
      {PlayerGameStatus::Ok, tr("OK")}, {PlayerGameStatus::NotFound, tr("Not Found")}};

  for (int i = 0; i < player_count; i++)
  {
    const auto* p = players[i];

    auto* name_item = new QTableWidgetItem(QString::fromStdString(p->name));
    auto* status_item = new QTableWidgetItem(player_status.count(p->game_status) ?
                                                 player_status.at(p->game_status) :
                                                 QStringLiteral("?"));
    auto* ping_item = new QTableWidgetItem(QStringLiteral("%1 ms").arg(p->ping));
    auto* mapping_item = new QTableWidgetItem(
        QString::fromStdString(get_mapping_string(p, client->GetPadMapping()) +
                               get_mapping_string(p, client->GetWiimoteMapping())));
    auto* revision_item = new QTableWidgetItem(QString::fromStdString(p->revision));

    for (auto* item : {name_item, status_item, ping_item, mapping_item, revision_item})
    {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setData(Qt::UserRole, static_cast<int>(p->pid));
    }

    m_players_list->setItem(i, 0, name_item);
    m_players_list->setItem(i, 1, status_item);
    m_players_list->setItem(i, 2, ping_item);
    m_players_list->setItem(i, 3, mapping_item);
    m_players_list->setItem(i, 4, revision_item);

    if (p->pid == selection_pid)
      m_players_list->selectRow(i);
  }

  // Update Room ID / IP label
  if (m_use_traversal && m_room_box->currentIndex() == 0)
  {
    switch (g_TraversalClient->GetState())
    {
    case TraversalClient::Connecting:
      m_hostcode_label->setText(tr("..."));
      m_hostcode_action_button->setEnabled(false);
      break;
    case TraversalClient::Connected:
    {
      const auto host_id = g_TraversalClient->GetHostID();
      m_hostcode_label->setText(
          QString::fromStdString(std::string(host_id.begin(), host_id.end())));
      m_hostcode_action_button->setEnabled(true);
      m_hostcode_action_button->setText(tr("Copy"));
      m_is_copy_button_retry = false;
      break;
    }
    case TraversalClient::Failure:
      m_hostcode_label->setText(tr("Error"));
      m_hostcode_action_button->setText(tr("Retry"));
      m_hostcode_action_button->setEnabled(true);
      m_is_copy_button_retry = true;
      break;
    }
  }
  else if (Settings::Instance().GetNetPlayServer())
  {
    m_hostcode_label->setText(
        QString::fromStdString(Settings::Instance().GetNetPlayServer()->GetInterfaceHost(
            m_room_box->currentData().toString().toStdString())));
    m_hostcode_action_button->setText(tr("Copy"));
    m_hostcode_action_button->setEnabled(true);
  }
}

// NetPlayUI methods

void NetPlayDialog::BootGame(const std::string& filename)
{
  m_got_stop_request = false;
  emit Boot(QString::fromStdString(filename));
}

void NetPlayDialog::StopGame()
{
  if (m_got_stop_request)
    return;

  m_got_stop_request = true;
  emit Stop();
}

void NetPlayDialog::Update()
{
  QueueOnObject(this, &NetPlayDialog::UpdateGUI);
}

void NetPlayDialog::DisplayMessage(const QString& msg, const std::string& color, int duration)
{
  QueueOnObject(m_chat_edit, [this, color, msg] {
    m_chat_edit->append(
        QStringLiteral("<font color='%1'>%2</font>").arg(QString::fromStdString(color), msg));
  });

  if (g_ActiveConfig.bShowNetPlayMessages && Core::IsRunning())
  {
    u32 osd_color;

    // Convert the color string to a OSD color
    if (color == "red")
      osd_color = OSD::Color::RED;
    else if (color == "cyan")
      osd_color = OSD::Color::CYAN;
    else if (color == "green")
      osd_color = OSD::Color::GREEN;
    else
      osd_color = OSD::Color::YELLOW;

    OSD::AddTypedMessage(OSD::MessageType::NetPlayBuffer, msg.toStdString(), OSD::Duration::NORMAL,
                         osd_color);
  }
}

void NetPlayDialog::AppendChat(const std::string& msg)
{
  DisplayMessage(QString::fromStdString(msg).toHtmlEscaped(), "");
}

void NetPlayDialog::OnMsgChangeGame(const std::string& title)
{
  QString qtitle = QString::fromStdString(title);
  QueueOnObject(this, [this, qtitle, title] {
    m_game_button->setText(qtitle);
    m_current_game = title;
  });
  DisplayMessage(tr("Game changed to \"%1\"").arg(qtitle), "magenta");
}

void NetPlayDialog::GameStatusChanged(bool running)
{
  if (!running && !m_got_stop_request)
    Settings::Instance().GetNetPlayClient()->RequestStopGame();

  QueueOnObject(this, [this, running] {
    if (Settings::Instance().GetNetPlayServer() != nullptr)
    {
      m_start_button->setEnabled(!running);
      m_game_button->setEnabled(!running);
      m_load_wii_box->setEnabled(!running);
      m_save_sd_box->setEnabled(!running);
      m_assign_ports_button->setEnabled(!running);
      m_reduce_polling_rate_box->setEnabled(!running);
    }

    m_record_input_box->setEnabled(!running);
  });
}

void NetPlayDialog::OnMsgStartGame()
{
  DisplayMessage(tr("Started game"), "green");

  QueueOnObject(this, [this] {
    Settings::Instance().GetNetPlayClient()->StartGame(FindGame(m_current_game));
  });
}

void NetPlayDialog::OnMsgStopGame()
{
}

void NetPlayDialog::OnPadBufferChanged(u32 buffer)
{
  QueueOnObject(this, [this, buffer] { m_buffer_size_box->setValue(buffer); });
  DisplayMessage(tr("Buffer size changed to %1").arg(buffer), "");

  m_buffer_size = static_cast<int>(buffer);
}

void NetPlayDialog::OnDesync(u32 frame, const std::string& player)
{
  DisplayMessage(tr("Possible desync detected: %1 might have desynced at frame %2")
                     .arg(QString::fromStdString(player), QString::number(frame)),
                 "red", OSD::Duration::VERY_LONG);
}

void NetPlayDialog::OnConnectionLost()
{
  DisplayMessage(tr("Lost connection to NetPlay server..."), "red");
}

void NetPlayDialog::OnConnectionError(const std::string& message)
{
  QueueOnObject(this, [this, message] {
    QMessageBox::critical(this, tr("Error"),
                          tr("Failed to connect to server: %1").arg(tr(message.c_str())));
  });
}

void NetPlayDialog::OnTraversalError(TraversalClient::FailureReason error)
{
  QueueOnObject(this, [this, error] {
    switch (error)
    {
    case TraversalClient::FailureReason::BadHost:
      QMessageBox::critical(this, tr("Traversal Error"), tr("Couldn't look up central server"));
      QDialog::reject();
      break;
    case TraversalClient::FailureReason::VersionTooOld:
      QMessageBox::critical(this, tr("Traversal Error"),
                            tr("Dolphin is too old for traversal server"));
      QDialog::reject();
      break;
    case TraversalClient::FailureReason::ServerForgotAboutUs:
    case TraversalClient::FailureReason::SocketSendError:
    case TraversalClient::FailureReason::ResendTimeout:
      UpdateGUI();
      break;
    }
  });
}

bool NetPlayDialog::IsRecording()
{
  std::optional<bool> is_recording = RunOnObject(m_record_input_box, &QCheckBox::isChecked);
  if (is_recording)
    return *is_recording;
  return false;
}

std::string NetPlayDialog::FindGame(const std::string& game)
{
  std::optional<std::string> path = RunOnObject(this, [this, game] {
    for (int i = 0; i < m_game_list_model->rowCount(QModelIndex()); i++)
    {
      if (m_game_list_model->GetUniqueIdentifier(i).toStdString() == game)
        return m_game_list_model->GetPath(i).toStdString();
    }
    return std::string("");
  });
  if (path)
    return *path;
  return std::string("");
}

void NetPlayDialog::ShowMD5Dialog(const std::string& file_identifier)
{
  QueueOnObject(this, [this, file_identifier] {
    m_md5_button->setEnabled(false);

    if (m_md5_dialog->isVisible())
      m_md5_dialog->close();

    m_md5_dialog->show(QString::fromStdString(file_identifier));
  });
}

void NetPlayDialog::SetMD5Progress(int pid, int progress)
{
  QueueOnObject(this, [this, pid, progress] {
    if (m_md5_dialog->isVisible())
      m_md5_dialog->SetProgress(pid, progress);
  });
}

void NetPlayDialog::SetMD5Result(int pid, const std::string& result)
{
  QueueOnObject(this, [this, pid, result] {
    m_md5_dialog->SetResult(pid, result);
    m_md5_button->setEnabled(true);
  });
}

void NetPlayDialog::AbortMD5()
{
  QueueOnObject(this, [this] {
    m_md5_dialog->close();
    m_md5_button->setEnabled(true);
  });
}

// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/NetPlaySetupDialog.h"

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "Core/Config/NetplaySettings.h"
#include "Core/NetPlayProto.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/UTF8CodePointCountValidator.h"
#include "DolphinQt/Settings.h"

#include "Common/Version.h"
#include "DolphinQt/NetPlay/NetPlayBrowser.h"
#include "UICommon/GameFile.h"
#include "UICommon/NetPlayIndex.h"

NetPlaySetupDialog::NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent)
    : QDialog(parent), m_game_list_model(game_list_model)
{
  setWindowTitle(tr("NetPlay Setup"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateMainLayout();

  bool use_index = Config::Get(Config::NETPLAY_USE_INDEX);
  std::string index_name = Config::Get(Config::NETPLAY_INDEX_NAME);
  std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);
  std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  int connect_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  int host_port = Config::Get(Config::NETPLAY_HOST_PORT);
#ifdef USE_UPNP
  bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);

  m_host_upnp->setChecked(use_upnp);
#endif

  m_nickname_edit->setText(QString::fromStdString(nickname));
  m_connection_type->setCurrentIndex(traversal_choice == "direct" ? 0 : 1);
  m_connect_port_box->setValue(connect_port);
  m_host_port_box->setValue(host_port);

  m_host_server_name->setEnabled(use_index);
  m_host_server_name->setText(QString::fromStdString(index_name));

  // Browser Stuff
  const auto& settings = Settings::Instance().GetQSettings();

  const QByteArray geometry =
      settings.value(QStringLiteral("netplaybrowser/geometry")).toByteArray();
  if (!geometry.isEmpty())
    restoreGeometry(geometry);

  const QString region = settings.value(QStringLiteral("netplaybrowser/region")).toString();
  const bool valid_region = m_region_combo->findText(region) != -1;
  if (valid_region)
    m_region_combo->setCurrentText(region);

  m_edit_name->setText(settings.value(QStringLiteral("netplaybrowser/name")).toString());

  const QString visibility = settings.value(QStringLiteral("netplaybrowser/visibility")).toString();
  if (visibility == QStringLiteral("public"))
    m_radio_public->setChecked(true);
  else if (visibility == QStringLiteral("private"))
    m_radio_private->setChecked(true);

  m_check_hide_ingame->setChecked(true);

  OnConnectionTypeChanged(m_connection_type->currentIndex());

  ConnectWidgets();

  m_refresh_run.Set(true);
  m_refresh_thread = std::thread([this] { RefreshLoopBrowser(); });

  UpdateListBrowser();
  RefreshBrowser();
}

void NetPlaySetupDialog::CreateMainLayout()
{
  m_main_layout = new QGridLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Cancel);
  m_nickname_edit = new QLineEdit;
  m_connection_type = new QComboBox;

  m_connection_type->setCurrentIndex(1);
  m_reset_traversal_button = new NonDefaultQPushButton(tr("Reset Traversal Settings"));

  m_tab_widget = new QTabWidget;

  m_nickname_edit->setValidator(
      new UTF8CodePointCountValidator(NetPlay::MAX_NAME_LENGTH, m_nickname_edit));

  // Connection widget
  auto* connection_widget = new QWidget;
  auto* connection_layout = new QGridLayout;

  // NetPlay Browser
  auto* browser_widget = new QWidget;
  auto* layout = new QVBoxLayout;

  m_table_widget = new QTableWidget;
  m_table_widget->setTabKeyNavigation(false);

  m_table_widget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table_widget->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table_widget->setWordWrap(false);

  m_region_combo = new QComboBox;

  m_region_combo->addItem(tr("Any Region"));

  for (const auto& region : NetPlayIndex::GetRegions())
  {
    m_region_combo->addItem(
        tr("%1 (%2)").arg(tr(region.second.c_str())).arg(QString::fromStdString(region.first)),
        QString::fromStdString(region.first));
  }

  m_region_combo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

  m_status_label = new QLabel;
  m_b_button_box = new QDialogButtonBox;
  m_button_refresh = new QPushButton(tr("Refresh"));
  m_edit_name = new QLineEdit;
  m_check_hide_ingame = new QCheckBox(tr("Hide In-Game Sessions"));

  m_radio_all = new QRadioButton(tr("Private and Public"));
  m_radio_private = new QRadioButton(tr("Private"));
  m_radio_public = new QRadioButton(tr("Public"));

  m_radio_all->setChecked(true);

  layout->addWidget(m_table_widget);
  layout->addWidget(m_status_label);
  layout->addWidget(m_b_button_box);

  m_b_button_box->addButton(m_button_refresh, QDialogButtonBox::ResetRole);

  browser_widget->setLayout(layout);

  m_ip_label = new QLabel;
  m_ip_edit = new QLineEdit;
  m_connect_port_label = new QLabel(tr("Port:"));
  m_connect_port_box = new QSpinBox;
  m_connect_button = new NonDefaultQPushButton(tr("Connect"));

  m_connect_port_box->setMaximum(65535);

  connection_layout->addWidget(m_ip_label, 0, 0);
  connection_layout->addWidget(m_ip_edit, 0, 1);
  connection_layout->addWidget(m_connect_port_label, 0, 2);
  connection_layout->addWidget(m_connect_port_box, 0, 3);
  connection_layout->addWidget(
      new QLabel(tr(
          "ALERT:\n\n"
          "All players must use the same Dolphin version, unless Dolphin-MPN is used to host\n"
          "If enabled, SD cards must be identical between players.\n"
          "If DSP LLE is used, DSP ROMs must be identical between players.\n"
          "If a game is hanging on boot, it may not support Dual Core Netplay."
          " Disable Dual Core.\n"
          "If connecting directly, the host must have the chosen UDP port open/forwarded!\n"
          "\n"
          "Wii Remote support in netplay is experimental and may not work correctly.\n"
          "Use at your own risk.\n")),
      1, 0, -1, -1);
  connection_layout->addWidget(m_connect_button, 3, 3, Qt::AlignRight);

  connection_widget->setLayout(connection_layout);

  // Host widget
  auto* host_widget = new QWidget;
  auto* host_layout = new QGridLayout;
  m_host_port_box = new QSpinBox;
  m_host_chunked_upload_limit_check = new QCheckBox(tr("Limit Chunked Upload Speed:"));
  m_host_chunked_upload_limit_box = new QSpinBox;
  m_host_server_name = new QLineEdit;

#ifdef USE_UPNP
  m_host_upnp = new QCheckBox(tr("Forward port (UPnP)"));
#endif
  m_host_games = new QListWidget;
  m_host_button = new NonDefaultQPushButton(tr("Host"));

  m_host_port_box->setMaximum(65535);

  m_host_server_name->setToolTip(tr("Name of your session shown in the server browser"));
  m_host_server_name->setPlaceholderText(tr("Name"));

  host_layout->addWidget(m_host_port_box, 0, 0, Qt::AlignLeft);
#ifdef USE_UPNP
  host_layout->addWidget(m_host_upnp, 0, 5, Qt::AlignRight);
#endif
  host_layout->addWidget(m_host_games, 2, 0, 1, -1);
  host_layout->addWidget(m_host_button, 4, 5, Qt::AlignRight);

  host_widget->setLayout(host_layout);

  m_connection_type->addItem(tr("Direct Connection"));
  m_connection_type->addItem(tr("Traversal Server"));

  m_main_layout->addWidget(new QLabel(tr("Connection Type:")), 0, 0);
  m_main_layout->addWidget(m_connection_type, 0, 1);
  m_main_layout->addWidget(m_reset_traversal_button, 0, 2);
  m_main_layout->addWidget(new QLabel(tr("Nickname:")), 1, 0);
  m_main_layout->addWidget(m_nickname_edit, 1, 1);
  m_main_layout->addWidget(m_tab_widget, 2, 0, 1, -1);
  m_main_layout->addWidget(m_button_box, 3, 0, 1, -1);

  // Tabs
  m_tab_widget->addTab(connection_widget, tr("Connect"));
  m_tab_widget->addTab(host_widget, tr("Host"));
  m_tab_widget->addTab(browser_widget, tr("Browser"));

  setLayout(m_main_layout);
}

NetPlaySetupDialog::~NetPlaySetupDialog()
{
  m_refresh_run.Set(false);
  m_refresh_event.Set();
  if (m_refresh_thread.joinable())
    m_refresh_thread.join();

  SaveSettings();
}

void NetPlaySetupDialog::ConnectWidgets()
{
  connect(m_connection_type, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::OnConnectionTypeChanged);
  connect(m_nickname_edit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);

  // Connect widget
  connect(m_ip_edit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_connect_port_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &NetPlaySetupDialog::SaveSettings);
  // Host widget
  connect(m_host_port_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &NetPlaySetupDialog::SaveSettings);
  connect(m_host_games, qOverload<int>(&QListWidget::currentRowChanged), [this](int index) {
    Settings::GetQSettings().setValue(QStringLiteral("netplay/hostgame"),
                                      m_host_games->item(index)->text());
  });

  // refresh browser on tab changed
  connect(m_tab_widget, &QTabWidget::currentChanged, this, &NetPlaySetupDialog::RefreshBrowser);

  connect(m_host_games, &QListWidget::itemDoubleClicked, this, &NetPlaySetupDialog::accept);

  connect(m_host_server_name, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);

#ifdef USE_UPNP
  connect(m_host_upnp, &QCheckBox::stateChanged, this, &NetPlaySetupDialog::SaveSettings);
#endif

  connect(m_connect_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_host_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_reset_traversal_button, &QPushButton::clicked, this,
          &NetPlaySetupDialog::ResetTraversalHost);

  // Browser Stuff
  connect(m_region_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::RefreshBrowser);

  connect(m_button_refresh, &QPushButton::clicked, this, &NetPlaySetupDialog::RefreshBrowser);

  connect(m_radio_all, &QRadioButton::toggled, this, &NetPlaySetupDialog::RefreshBrowser);
  connect(m_radio_private, &QRadioButton::toggled, this, &NetPlaySetupDialog::RefreshBrowser);
  connect(m_check_hide_ingame, &QRadioButton::toggled, this, &NetPlaySetupDialog::RefreshBrowser);

  connect(m_edit_name, &QLineEdit::textChanged, this, &NetPlaySetupDialog::RefreshBrowser);

  connect(m_table_widget, &QTableWidget::itemDoubleClicked, this,
          &NetPlaySetupDialog::acceptBrowser);

  connect(this, &NetPlaySetupDialog::UpdateStatusRequestedBrowser, this,
          &NetPlaySetupDialog::OnUpdateStatusRequestedBrowser, Qt::QueuedConnection);
  connect(this, &NetPlaySetupDialog::UpdateListRequestedBrowser, this,
          &NetPlaySetupDialog::OnUpdateListRequestedBrowser, Qt::QueuedConnection);
}

void NetPlaySetupDialog::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  Config::SetBaseOrCurrent(Config::NETPLAY_NICKNAME, m_nickname_edit->text().toStdString());
  Config::SetBaseOrCurrent(m_connection_type->currentIndex() == 0 ? Config::NETPLAY_ADDRESS :
                                                                    Config::NETPLAY_HOST_CODE,
                           m_ip_edit->text().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_CONNECT_PORT,
                           static_cast<u16>(m_connect_port_box->value()));
  Config::SetBaseOrCurrent(Config::NETPLAY_HOST_PORT, static_cast<u16>(m_host_port_box->value()));
#ifdef USE_UPNP
  Config::SetBaseOrCurrent(Config::NETPLAY_USE_UPNP, m_host_upnp->isChecked());
#endif

  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_REGION, "NA");
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_NAME, m_nickname_edit->text().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_PASSWORD, "");

  // Browser Stuff
  auto& settings = Settings::Instance().GetQSettings();

  settings.setValue(QStringLiteral("netplaybrowser/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("netplaybrowser/region"), m_region_combo->currentText());
  settings.setValue(QStringLiteral("netplaybrowser/name"), m_edit_name->text());

  QString visibility(QStringLiteral("all"));
  if (m_radio_public->isChecked())
    visibility = QStringLiteral("public");
  else if (m_radio_private->isChecked())
    visibility = QStringLiteral("private");
  settings.setValue(QStringLiteral("netplaybrowser/visibility"), visibility);

  settings.setValue(QStringLiteral("netplaybrowser/hide_incompatible"), true);
  settings.setValue(QStringLiteral("netplaybrowser/hide_ingame"), m_check_hide_ingame->isChecked());
}

void NetPlaySetupDialog::OnConnectionTypeChanged(int index)
{
  m_connect_port_box->setHidden(index != 0);
  m_connect_port_label->setHidden(index != 0);

  m_host_port_box->setHidden(index != 0);
#ifdef USE_UPNP
  m_host_upnp->setHidden(index != 0);
#endif
  m_reset_traversal_button->setHidden(index == 0);

  std::string address =
      index == 0 ? Config::Get(Config::NETPLAY_ADDRESS) : Config::Get(Config::NETPLAY_HOST_CODE);

  m_ip_label->setText(index == 0 ? tr("IP Address:") : tr("Host Code:"));
  m_ip_edit->setText(QString::fromStdString(address));

  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE,
                           std::string(index == 0 ? "direct" : "traversal"));
}

void NetPlaySetupDialog::show()
{
  // Here i'm setting the lobby name if it's empty to make
  // NetPlay sessions start more easily for first time players
  if (m_host_server_name->text().isEmpty())
  {
    std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);
    m_host_server_name->setText(QString::fromStdString(nickname));
  }
  m_connection_type->setCurrentIndex(1);
  m_tab_widget->setCurrentIndex(2);  // start on browser

  PopulateGameList();
  QDialog::show();
}

void NetPlaySetupDialog::accept()
{
  SaveSettings();
  if (m_tab_widget->currentIndex() == 0)
  {
    emit Join();
  }
  else
  {
    auto items = m_host_games->selectedItems();
    if (items.empty())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must select a game to host!"));
      return;
    }

    emit Host(*items[0]->data(Qt::UserRole).value<std::shared_ptr<const UICommon::GameFile>>());
  }
}

void NetPlaySetupDialog::PopulateGameList()
{
  QSignalBlocker blocker(m_host_games);

  m_host_games->clear();
  for (int i = 0; i < m_game_list_model.rowCount(QModelIndex()); i++)
  {
    std::shared_ptr<const UICommon::GameFile> game = m_game_list_model.GetGameFile(i);

    auto* item =
        new QListWidgetItem(QString::fromStdString(m_game_list_model.GetNetPlayName(*game)));
    item->setData(Qt::UserRole, QVariant::fromValue(std::move(game)));
    m_host_games->addItem(item);
  }

  m_host_games->sortItems();

  const QString selected_game =
      Settings::GetQSettings().value(QStringLiteral("netplay/hostgame"), QString{}).toString();
  const auto find_list = m_host_games->findItems(selected_game, Qt::MatchFlag::MatchExactly);

  if (find_list.count() > 0)
    m_host_games->setCurrentItem(find_list[0]);
}

void NetPlaySetupDialog::ResetTraversalHost()
{
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_SERVER,
                           Config::NETPLAY_TRAVERSAL_SERVER.GetDefaultValue());
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_PORT,
                           Config::NETPLAY_TRAVERSAL_PORT.GetDefaultValue());

  ModalMessageBox::information(
      this, tr("Reset Traversal Server"),
      tr("Reset Traversal Server to %1:%2")
          .arg(QString::fromStdString(Config::NETPLAY_TRAVERSAL_SERVER.GetDefaultValue()),
               QString::number(Config::NETPLAY_TRAVERSAL_PORT.GetDefaultValue())));
}

void NetPlaySetupDialog::RefreshBrowser()
{
  std::map<std::string, std::string> filters;

  if (!m_edit_name->text().isEmpty())
    filters["name"] = m_edit_name->text().toStdString();

  if (true)
    filters["version"] = Common::GetScmDescStr();

  if (!m_radio_all->isChecked())
    filters["password"] = std::to_string(m_radio_private->isChecked());

  if (m_region_combo->currentIndex() != 0)
    filters["region"] = m_region_combo->currentData().toString().toStdString();

  if (m_check_hide_ingame->isChecked())
    filters["in_game"] = "0";

  std::unique_lock<std::mutex> lock(m_refresh_filters_mutex);
  m_refresh_filters = std::move(filters);
  m_refresh_event.Set();
  SaveSettings();
}

void NetPlaySetupDialog::RefreshLoopBrowser()
{
  while (m_refresh_run.IsSet())
  {
    m_refresh_event.Wait();

    std::unique_lock<std::mutex> lock(m_refresh_filters_mutex);
    if (m_refresh_filters)
    {
      auto filters = std::move(*m_refresh_filters);
      m_refresh_filters.reset();

      lock.unlock();

      emit UpdateStatusRequestedBrowser(tr("Refreshing..."));

      NetPlayIndex client;

      auto entries = client.List(filters);

      if (entries)
      {
        emit UpdateListRequestedBrowser(std::move(*entries));
      }
      else
      {
        emit UpdateStatusRequestedBrowser(tr("Error obtaining session list: %1")
                                              .arg(QString::fromStdString(client.GetLastError())));
      }
    }
  }
}

void NetPlaySetupDialog::UpdateListBrowser()
{
  const int session_count = static_cast<int>(m_sessions.size());

  m_table_widget->clear();
  m_table_widget->setColumnCount(4);
  m_table_widget->setHorizontalHeaderLabels({
      tr("Name"),
      tr("Game"),
      tr("Players"),
      tr("In-Game"),
  });

  auto* hor_header = m_table_widget->horizontalHeader();
  hor_header->setSectionResizeMode(0, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(0, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(1, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  hor_header->setHighlightSections(false);

  m_table_widget->setRowCount(session_count);

  for (int i = 0; i < session_count; i++)
  {
    const auto& entry = m_sessions[i];

    auto* name = new QTableWidgetItem(QString::fromStdString(entry.name));
    auto* in_game = new QTableWidgetItem(entry.in_game ? tr("Yes") : tr("No"));
    auto* game_id = new QTableWidgetItem(QString::fromStdString(entry.game_id));
    auto* player_count = new QTableWidgetItem(QStringLiteral("%1").arg(entry.player_count));

    const bool enabled = Common::GetScmDescStr() == entry.version;

    for (const auto& item : {name, game_id, player_count, in_game})
      item->setFlags(enabled ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags);

    m_table_widget->setItem(i, 0, name);
    m_table_widget->setItem(i, 1, game_id);
    m_table_widget->setItem(i, 2, player_count);
    m_table_widget->setItem(i, 3, in_game);
  }

  m_status_label->setText(
      (session_count == 1 ? tr("%1 session found") : tr("%1 sessions found")).arg(session_count));
}

void NetPlaySetupDialog::OnSelectionChangedBrowser()
{
  return;
}

void NetPlaySetupDialog::OnUpdateStatusRequestedBrowser(const QString& status)
{
  m_status_label->setText(status);
}

void NetPlaySetupDialog::OnUpdateListRequestedBrowser(std::vector<NetPlaySession> sessions)
{
  m_sessions = std::move(sessions);
  UpdateListBrowser();
}

void NetPlaySetupDialog::acceptBrowser()
{
  if (m_table_widget->selectedItems().isEmpty())
    return;

  const int index = m_table_widget->selectedItems()[0]->row();

  NetPlaySession& session = m_sessions[index];

  std::string server_id = session.server_id;

  if (m_sessions[index].has_password)
  {
    auto* dialog = new QInputDialog(this);

    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->setWindowTitle(tr("Enter password"));
    dialog->setLabelText(tr("This session requires a password:"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setTextEchoMode(QLineEdit::Password);

    if (dialog->exec() != QDialog::Accepted)
      return;

    const std::string password = dialog->textValue().toStdString();

    auto decrypted_id = session.DecryptID(password);

    if (!decrypted_id)
    {
      ModalMessageBox::warning(this, tr("Error"), tr("Invalid password provided."));
      return;
    }

    server_id = decrypted_id.value();
  }

  QDialog::accept();

  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE, session.method);

  Config::SetBaseOrCurrent(Config::NETPLAY_CONNECT_PORT, session.port);

  if (session.method == "traversal")
    Config::SetBaseOrCurrent(Config::NETPLAY_HOST_CODE, server_id);
  else
    Config::SetBaseOrCurrent(Config::NETPLAY_ADDRESS, server_id);

  emit JoinBrowser();
}

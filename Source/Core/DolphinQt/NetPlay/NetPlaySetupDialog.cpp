// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/NetPlaySetupDialog.h"

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>

#include "Core/Config/NetplaySettings.h"
#include "Core/NetPlayProto.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/UTF8CodePointCountValidator.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameFile.h"
#include "UICommon/NetPlayIndex.h"

NetPlaySetupDialog::NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent)
    : QDialog(parent), m_game_list_model(game_list_model)
{
  setWindowTitle(tr("NetPlay Setup"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateMainLayout();

  bool use_index = Config::Get(Config::NETPLAY_USE_INDEX);
  std::string index_region = Config::Get(Config::NETPLAY_INDEX_REGION);
  std::string index_name = Config::Get(Config::NETPLAY_INDEX_NAME);
  std::string index_password = Config::Get(Config::NETPLAY_INDEX_PASSWORD);
  std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);
  std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  int connect_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  int host_port = Config::Get(Config::NETPLAY_HOST_PORT);
  int host_listen_port = Config::Get(Config::NETPLAY_LISTEN_PORT);
  bool enable_chunked_upload_limit = Config::Get(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT);
  u32 chunked_upload_limit = Config::Get(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT);
#ifdef USE_UPNP
  bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);

  m_host_upnp->setChecked(use_upnp);
#endif

  m_nickname_edit->setText(QString::fromStdString(nickname));
  m_connection_type->setCurrentIndex(traversal_choice == "direct" ? 0 : 1);
  m_connect_port_box->setValue(connect_port);
  m_host_port_box->setValue(host_port);

  m_host_force_port_box->setValue(host_listen_port);
  m_host_force_port_box->setEnabled(false);

  m_host_server_browser->setChecked(use_index);

  m_host_server_region->setEnabled(use_index);
  m_host_server_region->setCurrentIndex(
      m_host_server_region->findData(QString::fromStdString(index_region)));

  m_host_server_name->setEnabled(use_index);
  m_host_server_name->setText(QString::fromStdString(index_name));

  m_host_server_password->setEnabled(use_index);
  m_host_server_password->setText(QString::fromStdString(index_password));

  m_host_chunked_upload_limit_check->setChecked(enable_chunked_upload_limit);
  m_host_chunked_upload_limit_box->setValue(chunked_upload_limit);
  m_host_chunked_upload_limit_box->setEnabled(enable_chunked_upload_limit);

  OnConnectionTypeChanged(m_connection_type->currentIndex());

  ConnectWidgets();
}

void NetPlaySetupDialog::CreateMainLayout()
{
  m_main_layout = new QGridLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Cancel);
  m_nickname_edit = new QLineEdit;
  m_connection_type = new QComboBox;
  m_reset_traversal_button = new NonDefaultQPushButton(tr("Reset Traversal Settings"));
  m_tab_widget = new QTabWidget;

  m_nickname_edit->setValidator(
      new UTF8CodePointCountValidator(NetPlay::MAX_NAME_LENGTH, m_nickname_edit));

  // Connection widget
  auto* connection_widget = new QWidget;
  auto* connection_layout = new QGridLayout;

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
  auto* const alert_label = new QLabel(
      tr("ALERT:\n\n"
         "All players must use the same Dolphin version.\n"
         "If enabled, SD cards must be identical between players.\n"
         "If DSP LLE is used, DSP ROMs must be identical between players.\n"
         "If a game is hanging on boot, it may not support Dual Core Netplay."
         " Disable Dual Core.\n"
         "If connecting directly, the host must have the chosen UDP port open/forwarded!\n"
         "\n"
         "Wii Remote support in netplay is experimental and may not work correctly.\n"
         "Use at your own risk.\n"));

  // Prevent the label from stretching vertically so the spacer gets all the extra space
  alert_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

  connection_layout->addWidget(alert_label, 1, 0, 1, -1);
  connection_layout->addItem(new QSpacerItem(1, 1), 2, 0, -1, -1);
  connection_layout->addWidget(m_connect_button, 3, 3, Qt::AlignRight);

  connection_widget->setLayout(connection_layout);

  // Host widget
  auto* host_widget = new QWidget;
  auto* host_layout = new QGridLayout;
  m_host_port_label = new QLabel(tr("Port:"));
  m_host_port_box = new QSpinBox;
  m_host_force_port_check = new QCheckBox(tr("Force Listen Port:"));
  m_host_force_port_box = new QSpinBox;
  m_host_chunked_upload_limit_check = new QCheckBox(tr("Limit Chunked Upload Speed:"));
  m_host_chunked_upload_limit_box = new QSpinBox;
  m_host_server_browser = new QCheckBox(tr("Show in server browser"));
  m_host_server_name = new QLineEdit;
  m_host_server_password = new QLineEdit;
  m_host_server_region = new QComboBox;

#ifdef USE_UPNP
  m_host_upnp = new QCheckBox(tr("Forward port (UPnP)"));
#endif
  m_host_games = new QListWidget;
  m_host_button = new NonDefaultQPushButton(tr("Host"));

  m_host_port_box->setMaximum(65535);
  m_host_force_port_box->setMaximum(65535);
  m_host_chunked_upload_limit_box->setRange(1, 1000000);
  m_host_chunked_upload_limit_box->setSingleStep(100);
  m_host_chunked_upload_limit_box->setSuffix(QStringLiteral(" kbps"));

  m_host_chunked_upload_limit_check->setToolTip(tr(
      "This will limit the speed of chunked uploading per client, which is used for save sync."));

  m_host_server_name->setToolTip(tr("Name of your session shown in the server browser"));
  m_host_server_name->setPlaceholderText(tr("Name"));
  m_host_server_password->setToolTip(tr("Password for joining your game (leave empty for none)"));
  m_host_server_password->setPlaceholderText(tr("Password"));

  for (const auto& region : NetPlayIndex::GetRegions())
  {
    m_host_server_region->addItem(
        tr("%1 (%2)").arg(tr(region.second.c_str())).arg(QString::fromStdString(region.first)),
        QString::fromStdString(region.first));
  }

  host_layout->addWidget(m_host_port_label, 0, 0);
  host_layout->addWidget(m_host_port_box, 0, 1);
#ifdef USE_UPNP
  host_layout->addWidget(m_host_upnp, 0, 2);
#endif
  host_layout->addWidget(m_host_server_browser, 1, 0);
  host_layout->addWidget(m_host_server_region, 1, 1);
  host_layout->addWidget(m_host_server_name, 1, 2);
  host_layout->addWidget(m_host_server_password, 1, 3);
  host_layout->addWidget(m_host_games, 2, 0, 1, -1);
  host_layout->addWidget(m_host_force_port_check, 3, 0);
  host_layout->addWidget(m_host_force_port_box, 3, 1, Qt::AlignLeft);
  host_layout->addWidget(m_host_chunked_upload_limit_check, 4, 0);
  host_layout->addWidget(m_host_chunked_upload_limit_box, 4, 1, Qt::AlignLeft);
  host_layout->addWidget(m_host_button, 4, 3, 2, 1, Qt::AlignRight);

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

  setLayout(m_main_layout);
}

void NetPlaySetupDialog::ConnectWidgets()
{
  connect(m_connection_type, &QComboBox::currentIndexChanged, this,
          &NetPlaySetupDialog::OnConnectionTypeChanged);
  connect(m_nickname_edit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);

  // Connect widget
  connect(m_ip_edit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_connect_port_box, &QSpinBox::valueChanged, this, &NetPlaySetupDialog::SaveSettings);
  // Host widget
  connect(m_host_port_box, &QSpinBox::valueChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_games, &QListWidget::currentRowChanged, [this](int index) {
    Settings::GetQSettings().setValue(QStringLiteral("netplay/hostgame"),
                                      m_host_games->item(index)->text());
  });

  connect(m_host_games, &QListWidget::itemDoubleClicked, this, &NetPlaySetupDialog::accept);

  connect(m_host_force_port_check, &QCheckBox::toggled,
          [this](bool value) { m_host_force_port_box->setEnabled(value); });
  connect(m_host_chunked_upload_limit_check, &QCheckBox::toggled, this, [this](bool value) {
    m_host_chunked_upload_limit_box->setEnabled(value);
    SaveSettings();
  });
  connect(m_host_chunked_upload_limit_box, &QSpinBox::valueChanged, this,
          &NetPlaySetupDialog::SaveSettings);

  connect(m_host_server_browser, &QCheckBox::toggled, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_name, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_password, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_region,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::SaveSettings);

#ifdef USE_UPNP
  connect(m_host_upnp, &QCheckBox::stateChanged, this, &NetPlaySetupDialog::SaveSettings);
#endif

  connect(m_connect_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_host_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_reset_traversal_button, &QPushButton::clicked, this,
          &NetPlaySetupDialog::ResetTraversalHost);
  connect(m_host_server_browser, &QCheckBox::toggled, this, [this](bool value) {
    m_host_server_region->setEnabled(value);
    m_host_server_name->setEnabled(value);
    m_host_server_password->setEnabled(value);
  });
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

  if (m_host_force_port_check->isChecked())
    Config::SetBaseOrCurrent(Config::NETPLAY_LISTEN_PORT,
                             static_cast<u16>(m_host_force_port_box->value()));

  Config::SetBaseOrCurrent(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT,
                           m_host_chunked_upload_limit_check->isChecked());
  Config::SetBaseOrCurrent(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT,
                           m_host_chunked_upload_limit_box->value());

  Config::SetBaseOrCurrent(Config::NETPLAY_USE_INDEX, m_host_server_browser->isChecked());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_REGION,
                           m_host_server_region->currentData().toString().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_NAME, m_host_server_name->text().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_PASSWORD,
                           m_host_server_password->text().toStdString());
}

void NetPlaySetupDialog::OnConnectionTypeChanged(int index)
{
  m_connect_port_box->setHidden(index != 0);
  m_connect_port_label->setHidden(index != 0);

  m_host_port_label->setHidden(index != 0);
  m_host_port_box->setHidden(index != 0);
#ifdef USE_UPNP
  m_host_upnp->setHidden(index != 0);
#endif
  m_host_force_port_check->setHidden(index == 0);
  m_host_force_port_box->setHidden(index == 0);

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

    if (m_host_server_browser->isChecked() && m_host_server_name->text().isEmpty())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must provide a name for your session!"));
      return;
    }

    if (m_host_server_browser->isChecked() &&
        m_host_server_region->currentData().toString().isEmpty())
    {
      ModalMessageBox::critical(this, tr("Error"),
                                tr("You must provide a region for your session!"));
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

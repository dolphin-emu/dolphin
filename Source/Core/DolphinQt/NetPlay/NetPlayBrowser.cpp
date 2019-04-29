// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/NetPlay/NetPlayBrowser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpacerItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "Common/Version.h"

#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/RunOnObject.h"

NetPlayBrowser::NetPlayBrowser(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("NetPlay Session Browser"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateWidgets();
  ConnectWidgets();

  resize(750, 500);

  m_table_widget->verticalHeader()->setHidden(true);
  m_table_widget->setAlternatingRowColors(true);

  m_refresh_run.Set(true);
  m_refresh_thread = std::thread([this] { RefreshLoop(); });

  UpdateList();
  Refresh();
}

NetPlayBrowser::~NetPlayBrowser()
{
  m_refresh_run.Set(false);
  m_refresh_event.Set();
  if (m_refresh_thread.joinable())
    m_refresh_thread.join();
}

void NetPlayBrowser::CreateWidgets()
{
  auto* layout = new QVBoxLayout;

  m_table_widget = new QTableWidget;

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
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  m_button_refresh = new QPushButton(tr("Refresh"));
  m_edit_name = new QLineEdit;
  m_edit_game_id = new QLineEdit;
  m_check_hide_incompatible = new QCheckBox(tr("Hide Incompatible Sessions"));

  m_check_hide_incompatible->setChecked(true);

  m_radio_all = new QRadioButton(tr("Private and Public"));
  m_radio_private = new QRadioButton(tr("Private"));
  m_radio_public = new QRadioButton(tr("Public"));

  m_radio_all->setChecked(true);

  auto* filter_box = new QGroupBox(tr("Filters"));
  auto* filter_layout = new QGridLayout;
  filter_box->setLayout(filter_layout);

  filter_layout->addWidget(new QLabel(tr("Region:")), 0, 0);
  filter_layout->addWidget(m_region_combo, 0, 1, 1, -1);
  filter_layout->addWidget(new QLabel(tr("Name:")), 1, 0);
  filter_layout->addWidget(m_edit_name, 1, 1, 1, -1);
  filter_layout->addWidget(new QLabel(tr("Game ID:")), 2, 0);
  filter_layout->addWidget(m_edit_game_id, 2, 1, 1, -1);
  filter_layout->addWidget(m_radio_all, 3, 1);
  filter_layout->addWidget(m_radio_public, 3, 2);
  filter_layout->addWidget(m_radio_private, 3, 3);
  filter_layout->addItem(new QSpacerItem(4, 1, QSizePolicy::Expanding), 3, 4);
  filter_layout->addWidget(m_check_hide_incompatible, 4, 1, 1, -1);

  layout->addWidget(m_table_widget);
  layout->addWidget(filter_box);
  layout->addWidget(m_status_label);
  layout->addWidget(m_button_box);

  m_button_box->addButton(m_button_refresh, QDialogButtonBox::ResetRole);
  m_button_box->button(QDialogButtonBox::Ok)->setEnabled(false);

  setLayout(layout);
}

void NetPlayBrowser::ConnectWidgets()
{
  connect(m_region_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &NetPlayBrowser::Refresh);

  connect(m_button_box, &QDialogButtonBox::accepted, this, &NetPlayBrowser::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &NetPlayBrowser::reject);
  connect(m_button_refresh, &QPushButton::pressed, this, &NetPlayBrowser::Refresh);

  connect(m_radio_all, &QRadioButton::toggled, this, &NetPlayBrowser::Refresh);
  connect(m_radio_private, &QRadioButton::toggled, this, &NetPlayBrowser::Refresh);
  connect(m_check_hide_incompatible, &QRadioButton::toggled, this, &NetPlayBrowser::Refresh);

  connect(m_edit_name, &QLineEdit::textChanged, this, &NetPlayBrowser::Refresh);
  connect(m_edit_game_id, &QLineEdit::textChanged, this, &NetPlayBrowser::Refresh);

  connect(m_table_widget, &QTableWidget::itemSelectionChanged, this,
          &NetPlayBrowser::OnSelectionChanged);
  connect(m_table_widget, &QTableWidget::itemDoubleClicked, this, &NetPlayBrowser::accept);
}

void NetPlayBrowser::Refresh()
{
  std::map<std::string, std::string> filters;

  if (m_check_hide_incompatible->isChecked())
    filters["version"] = Common::scm_desc_str;

  if (!m_edit_name->text().isEmpty())
    filters["name"] = m_edit_name->text().toStdString();

  if (!m_edit_game_id->text().isEmpty())
    filters["game"] = m_edit_game_id->text().toStdString();

  if (!m_radio_all->isChecked())
    filters["password"] = std::to_string(m_radio_private->isChecked());

  if (m_region_combo->currentIndex() != 0)
    filters["region"] = m_region_combo->currentData().toString().toStdString();

  std::unique_lock<std::mutex> lock(m_refresh_filters_mutex);
  m_refresh_filters = std::move(filters);
  m_refresh_event.Set();
}

void NetPlayBrowser::RefreshLoop()
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

      RunOnObject(this, [this] {
        m_status_label->setText(tr("Refreshing..."));
        return nullptr;
      });

      NetPlayIndex client;

      auto entries = client.List(filters);

      if (entries)
      {
        RunOnObject(this, [this, &entries] {
          m_sessions = *entries;
          UpdateList();
          return nullptr;
        });
      }
      else
      {
        RunOnObject(this, [this, &client] {
          m_status_label->setText(tr("Error obtaining session list: %1")
                                      .arg(QString::fromStdString(client.GetLastError())));
          return nullptr;
        });
      }
    }
  }
}

void NetPlayBrowser::UpdateList()
{
  const int session_count = static_cast<int>(m_sessions.size());

  m_table_widget->clear();
  m_table_widget->setColumnCount(7);
  m_table_widget->setHorizontalHeaderLabels({tr("Region"), tr("Name"), tr("Password?"),
                                             tr("In-Game?"), tr("Game"), tr("Players"),
                                             tr("Version")});

  auto* hor_header = m_table_widget->horizontalHeader();

  hor_header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(1, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(4, QHeaderView::Stretch);
  hor_header->setHighlightSections(false);

  m_table_widget->setRowCount(session_count);

  for (int i = 0; i < session_count; i++)
  {
    const auto& entry = m_sessions[i];

    auto* region = new QTableWidgetItem(QString::fromStdString(entry.region));
    auto* name = new QTableWidgetItem(QString::fromStdString(entry.name));
    auto* password = new QTableWidgetItem(entry.has_password ? tr("Yes") : tr("No"));
    auto* in_game = new QTableWidgetItem(entry.in_game ? tr("Yes") : tr("No"));
    auto* game_id = new QTableWidgetItem(QString::fromStdString(entry.game_id));
    auto* player_count = new QTableWidgetItem(QStringLiteral("%1").arg(entry.player_count));
    auto* version = new QTableWidgetItem(QString::fromStdString(entry.version));

    const bool enabled = Common::scm_desc_str == entry.version;

    for (const auto& item : {region, name, password, in_game, game_id, player_count, version})
      item->setFlags(enabled ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags);

    m_table_widget->setItem(i, 0, region);
    m_table_widget->setItem(i, 1, name);
    m_table_widget->setItem(i, 2, password);
    m_table_widget->setItem(i, 3, in_game);
    m_table_widget->setItem(i, 4, game_id);
    m_table_widget->setItem(i, 5, player_count);
    m_table_widget->setItem(i, 6, version);
  }

  m_status_label->setText(
      (session_count == 1 ? tr("%1 session found") : tr("%1 sessions found")).arg(session_count));
}

void NetPlayBrowser::OnSelectionChanged()
{
  m_button_box->button(QDialogButtonBox::Ok)
      ->setEnabled(!m_table_widget->selectedItems().isEmpty());
}

void NetPlayBrowser::accept()
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

  emit Join();
}

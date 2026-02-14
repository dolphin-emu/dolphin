// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/TriforcePane.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/Logging/Log.h"

#include "Core/Config/MainSettings.h"
#include "Core/HW/DVD/AMMediaboard.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QtUtils.h"

namespace
{

constexpr int COLUMN_EMULATED = 0;
constexpr int COLUMN_REAL = 1;
constexpr int COLUMN_DESCRIPTION = 2;

class IPRedirectionsDialog final : public QDialog
{
public:
  explicit IPRedirectionsDialog(QWidget* parent);

private:
  void LoadConfig();
  void SaveConfig() const;

  void LoadDefault();
  void OnClear();

  QTableWidget* m_ip_redirections_table;
};

}  // namespace

TriforcePane::TriforcePane()
{
  auto* const main_layout = new QVBoxLayout{this};

  auto* const controllers_group = new QGroupBox{tr("Controllers")};
  main_layout->addWidget(controllers_group);

  auto* const controllers_layout = new QVBoxLayout{controllers_group};

  auto* const configure_controllers_button = new NonDefaultQPushButton{tr("Configure")};
  controllers_layout->addWidget(configure_controllers_button);

  connect(configure_controllers_button, &QPushButton::clicked, this, [this] {
    auto* const window = new MappingWindow(this, MappingWindow::Type::MAPPING_AM_BASEBOARD, 0);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->setWindowModality(Qt::WindowModality::WindowModal);
    window->show();
  });

  auto* const ip_redirection_group = new QGroupBox{tr("IP Address Redirections")};
  main_layout->addWidget(ip_redirection_group);

  auto* const ip_redirection_layout = new QVBoxLayout{ip_redirection_group};

  auto* const configure_ip_redirections_button = new NonDefaultQPushButton{tr("Configure")};
  ip_redirection_layout->addWidget(configure_ip_redirections_button);

  connect(configure_ip_redirections_button, &QPushButton::clicked, this, [this] {
    auto* const ip_redirections = new IPRedirectionsDialog{this};
    ip_redirections->setAttribute(Qt::WA_DeleteOnClose);
    ip_redirections->open();
  });

  main_layout->addStretch(1);
}

IPRedirectionsDialog::IPRedirectionsDialog(QWidget* parent) : QDialog{parent}
{
  setWindowTitle(tr("IP Address Redirections"));

  auto* const main_layout = new QVBoxLayout{this};

  auto* const load_default_button = new QPushButton(tr("Default"));
  connect(load_default_button, &QPushButton::clicked, this, &IPRedirectionsDialog::LoadDefault);

  auto* const clear_button = new QPushButton(tr("Clear"));
  connect(clear_button, &QPushButton::clicked, this, &IPRedirectionsDialog::OnClear);

  m_ip_redirections_table = new QTableWidget;
  main_layout->addWidget(m_ip_redirections_table);

  m_ip_redirections_table->setColumnCount(3);
  m_ip_redirections_table->setHorizontalHeaderLabels(
      {tr("Emulated"), tr("Real"), tr("Description")});
  m_ip_redirections_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_ip_redirections_table->setSizeAdjustPolicy(QTableWidget::AdjustToContents);

  m_ip_redirections_table->setEditTriggers(QAbstractItemView::DoubleClicked |
                                           QAbstractItemView::EditKeyPressed);

  auto* const button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  main_layout->addWidget(button_box);

  button_box->addButton(load_default_button, QDialogButtonBox::ActionRole);
  button_box->addButton(clear_button, QDialogButtonBox::ActionRole);

  connect(button_box, &QDialogButtonBox::accepted, this, &IPRedirectionsDialog::accept);
  connect(button_box, &QDialogButtonBox::rejected, this, &IPRedirectionsDialog::reject);

  connect(this, &IPRedirectionsDialog::accepted, this, &IPRedirectionsDialog::SaveConfig);

  connect(m_ip_redirections_table, &QTableWidget::itemChanged, this,
          [this](QTableWidgetItem* item) {
            const int row_count = m_ip_redirections_table->rowCount();
            const bool is_last_row = item->row() == row_count - 1;
            if (is_last_row)
            {
              if (!item->text().isEmpty())
                m_ip_redirections_table->insertRow(row_count);
            }
            else if (item->column() != COLUMN_DESCRIPTION && item->text().isEmpty())
            {
              // Just remove inner rows that have text erased.
              // This UX could be less weird, but it's good enough for now.
              m_ip_redirections_table->removeRow(item->row());
            }
          });

  LoadConfig();

  QtUtils::AdjustSizeWithinScreen(this);
}

void IPRedirectionsDialog::LoadConfig()
{
  m_ip_redirections_table->setRowCount(0);

  // Add the final empty row.
  m_ip_redirections_table->insertRow(0);

  int row = 0;

  const auto ip_redirections_str = Config::Get(Config::MAIN_TRIFORCE_IP_REDIRECTIONS);
  for (auto&& ip_pair : ip_redirections_str | std::views::split(','))
  {
    const auto ip_pair_str = std::string_view{ip_pair};
    const auto parsed = AMMediaboard::ParseIPRedirection(ip_pair_str);
    if (!parsed.has_value())
    {
      ERROR_LOG_FMT(COMMON, "Bad IP redirection string: {}", ip_pair_str);
      continue;
    }

    m_ip_redirections_table->insertRow(row);

    m_ip_redirections_table->setItem(row, COLUMN_EMULATED,
                                     new QTableWidgetItem(QString::fromUtf8(parsed->original)));
    m_ip_redirections_table->setItem(row, COLUMN_REAL,
                                     new QTableWidgetItem(QString::fromUtf8(parsed->replacement)));
    m_ip_redirections_table->setItem(row, COLUMN_DESCRIPTION,
                                     new QTableWidgetItem(QString::fromUtf8(parsed->description)));

    ++row;
  }
}

void IPRedirectionsDialog::SaveConfig() const
{
  // Ignore our empty row.
  const int row_count = m_ip_redirections_table->rowCount() - 1;

  std::vector<std::string> replacements(row_count);
  for (int row = 0; row < row_count; ++row)
  {
    auto* const original_item = m_ip_redirections_table->item(row, COLUMN_EMULATED);
    auto* const replacement_item = m_ip_redirections_table->item(row, COLUMN_REAL);

    // Skip incomplete rows.
    if (original_item == nullptr || replacement_item == nullptr)
      continue;

    replacements[row] = fmt::format("{}={}", StripWhitespace(original_item->text().toStdString()),
                                    replacement_item->text().toStdString());

    auto* const description_item = m_ip_redirections_table->item(row, COLUMN_DESCRIPTION);
    if (description_item == nullptr)
      continue;

    const auto description = description_item->text().toStdString();

    if (!description.empty())
      (replacements[row] += ' ') += description;
  }

  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_IP_REDIRECTIONS,
                           fmt::format("{}", fmt::join(replacements, ",")));
}

void IPRedirectionsDialog::LoadDefault()
{
  // This alters the config before "OK" is pressed. Bad UX..

  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_IP_REDIRECTIONS, Config::DefaultState{});

  LoadConfig();
}

void IPRedirectionsDialog::OnClear()
{
  m_ip_redirections_table->setRowCount(0);
  // Add the final empty row.
  m_ip_redirections_table->insertRow(0);
}

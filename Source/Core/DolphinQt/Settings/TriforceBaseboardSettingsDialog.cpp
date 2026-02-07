// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/TriforceBaseboardSettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/Logging/Log.h"

#include "Core/Config/MainSettings.h"
#include "Core/HW/DVD/AMMediaboard.h"

#include "DolphinQt/QtUtils/QtUtils.h"

TriforceBaseboardSettingsDialog::TriforceBaseboardSettingsDialog(QWidget* parent) : QDialog{parent}
{
  setWindowTitle(tr("Triforce Baseboard"));

  connect(this, &TriforceBaseboardSettingsDialog::accepted, this,
          &TriforceBaseboardSettingsDialog::SaveConfig);

  auto* const dialog_layout = new QVBoxLayout{this};

  auto* const ip_override_group = new QGroupBox{tr("IP Address Overrides")};
  dialog_layout->addWidget(ip_override_group);

  auto* const ip_override_layout = new QFormLayout{ip_override_group};

  m_bind_ip_edit = new QLineEdit;
  ip_override_layout->addRow(tr("Bind IP: "), m_bind_ip_edit);

  m_ip_overrides_table = new QTableWidget;
  ip_override_layout->addRow(tr("IP Overrides: "), m_ip_overrides_table);

  m_ip_overrides_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  m_ip_overrides_table->setColumnCount(2);
  m_ip_overrides_table->setHorizontalHeaderLabels({tr("Match"), tr("Replacement")});

  m_ip_overrides_table->setEditTriggers(QAbstractItemView::DoubleClicked |
                                        QAbstractItemView::EditKeyPressed);

  auto* const button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  dialog_layout->addWidget(button_box);

  auto* const load_default_button = new QPushButton(tr("Default"));
  connect(load_default_button, &QPushButton::clicked, this,
          &TriforceBaseboardSettingsDialog::LoadDefault);

  button_box->addButton(load_default_button, QDialogButtonBox::ActionRole);

  connect(button_box, &QDialogButtonBox::accepted, this, &TriforceBaseboardSettingsDialog::accept);
  connect(button_box, &QDialogButtonBox::rejected, this, &TriforceBaseboardSettingsDialog::reject);

  connect(m_ip_overrides_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
    const int row_count = m_ip_overrides_table->rowCount();
    const bool is_last_row = item->row() == row_count - 1;
    if (is_last_row)
    {
      if (!item->text().isEmpty())
        m_ip_overrides_table->insertRow(row_count);
    }
    else
    {
      // Just remove inner rows that have text erased.
      // This UX could be less weird, but it's good enough for now.
      if (item->text().isEmpty())
        m_ip_overrides_table->removeRow(item->row());
    }
  });

  LoadConfig();

  QtUtils::AdjustSizeWithinScreen(this);
}

void TriforceBaseboardSettingsDialog::LoadConfig()
{
  m_bind_ip_edit->setText(QString::fromUtf8(Config::Get(Config::MAIN_TRIFORCE_BIND_IP)));

  m_ip_overrides_table->setRowCount(0);

  // Add the final empty row.
  m_ip_overrides_table->insertRow(0);

  int row = 0;

  const auto ip_overrides_str = Config::Get(Config::MAIN_TRIFORCE_IP_OVERRIDES);
  for (auto&& ip_pair : ip_overrides_str | std::views::split(','))
  {
    const auto ip_pair_str = std::string_view{ip_pair};
    const auto parts = AMMediaboard::ParseIPOverride(ip_pair_str);
    if (!parts.has_value())
    {
      ERROR_LOG_FMT(COMMON, "Bad IP pair string: {}", ip_pair_str);
      continue;
    }

    m_ip_overrides_table->insertRow(row);

    m_ip_overrides_table->setItem(row, 0, new QTableWidgetItem(QString::fromUtf8(parts->first)));
    m_ip_overrides_table->setItem(row, 1, new QTableWidgetItem(QString::fromUtf8(parts->second)));

    ++row;
  }
}

void TriforceBaseboardSettingsDialog::SaveConfig()
{
  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_BIND_IP, m_bind_ip_edit->text().toStdString());

  // Ignore our empty row.
  const int row_count = m_ip_overrides_table->rowCount() - 1;

  std::vector<std::string> replacements(row_count);
  for (int row = 0; row < row_count; ++row)
  {
    // Skip incomplete rows.
    if (m_ip_overrides_table->item(row, 0) == nullptr ||
        m_ip_overrides_table->item(row, 1) == nullptr)
    {
      continue;
    }

    replacements[row] =
        std::string(StripWhitespace(m_ip_overrides_table->item(row, 0)->text().toStdString())) +
        "=" +
        std::string(StripWhitespace(m_ip_overrides_table->item(row, 1)->text().toStdString()));
  }

  // TODO: Ideally we'd remove duplicates based on the first IP.

  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_IP_OVERRIDES,
                           fmt::format("{}", fmt::join(replacements, ",")));
}

void TriforceBaseboardSettingsDialog::LoadDefault()
{
  // This alters the config before "OK" is pressed. Bad UX..
  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_BIND_IP, Config::DefaultState{});
  Config::SetBaseOrCurrent(Config::MAIN_TRIFORCE_IP_OVERRIDES, Config::DefaultState{});

  LoadConfig();
}

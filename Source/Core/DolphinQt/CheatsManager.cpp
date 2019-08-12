// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/CheatsManager.h"

#include <algorithm>
#include <cstring>
#include <optional>

#include <QByteArray>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Core/ActionReplay.h"
#include "Core/CheatSearch.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

#include "UICommon/GameFile.h"

#include "DolphinQt/Config/ARCodeWidget.h"
#include "DolphinQt/Config/GeckoCodeWidget.h"
#include "DolphinQt/GameList/GameListModel.h"
#include "DolphinQt/Settings.h"

constexpr u32 MAX_RESULTS = 50;

constexpr int INDEX_ROLE = Qt::UserRole;
constexpr int COLUMN_ROLE = Qt::UserRole + 1;

constexpr int AR_SET_BYTE_CMD = 0x00;
constexpr int AR_SET_SHORT_CMD = 0x02;
constexpr int AR_SET_INT_CMD = 0x04;

struct Result
{
  Cheats::SearchResult m_result;
  QString m_name;
  bool m_locked = false;
};

Q_DECLARE_METATYPE(Cheats::DataType);
Q_DECLARE_METATYPE(Cheats::CompareType);

static void UpdatePatch(const Result& result)
{
  PowerPC::debug_interface.UnsetPatch(result.m_result.m_address);
  if (result.m_locked)
  {
    PowerPC::debug_interface.SetPatch(result.m_result.m_address,
                                      Cheats::GetValueAsByteVector(result.m_result.m_value));
  }
}

static std::vector<ActionReplay::AREntry> ResultToAREntries(const Cheats::SearchResult& result)
{
  std::vector<ActionReplay::AREntry> codes;
  std::vector<u8> data = Cheats::GetValueAsByteVector(result.m_value);

  // TODO: use 2byte/4byte AR codes to reduce AR code count
  // on that note, do AR codes writes need to be 2byte/4byte aligned?
  for (size_t i = 0; i < data.size(); ++i)
  {
    u32 address = (result.m_address + i) & 0xff'ff'ff;
    u8 cmd = AR_SET_BYTE_CMD;
    u32 val = data[i];
    codes.emplace_back((cmd << 24) | address, val);
  }

  return codes;
}

CheatsManager::CheatsManager(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Cheats Manager"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &CheatsManager::OnStateChanged);

  OnStateChanged(Core::GetState());

  CreateWidgets();
  ConnectWidgets();
  Reset();
  UpdateGUI();
}

CheatsManager::~CheatsManager() = default;

void CheatsManager::OnStateChanged(Core::State state)
{
  if (state != Core::State::Running && state != Core::State::Paused)
    return;

  auto* model = Settings::Instance().GetGameListModel();

  for (int i = 0; i < model->rowCount(QModelIndex()); i++)
  {
    auto file = model->GetGameFile(i);

    if (file->GetGameID() == SConfig::GetInstance().GetGameID())
    {
      m_game_file = file;
      if (m_tab_widget->count() == 3)
      {
        m_tab_widget->removeTab(0);
        m_tab_widget->removeTab(0);
      }

      if (m_tab_widget->count() == 1)
      {
        if (m_ar_code)
          m_ar_code->deleteLater();

        m_ar_code = new ARCodeWidget(*m_game_file, false);
        m_tab_widget->insertTab(0, m_ar_code, tr("AR Code"));
        m_tab_widget->insertTab(1, new GeckoCodeWidget(*m_game_file, false), tr("Gecko Codes"));
      }
    }
  }
}

void CheatsManager::CreateWidgets()
{
  m_tab_widget = new QTabWidget;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  m_cheat_search = CreateCheatSearch();

  m_tab_widget->addTab(m_cheat_search, tr("Cheat Search"));

  auto* layout = new QVBoxLayout;
  layout->addWidget(m_tab_widget);
  layout->addWidget(m_button_box);

  setLayout(layout);
}

void CheatsManager::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_match_new, &QPushButton::clicked, this, &CheatsManager::NewSearch);
  connect(m_match_next, &QPushButton::clicked, this, &CheatsManager::NextSearch);
  connect(m_match_refresh, &QPushButton::clicked, this, &CheatsManager::UpdateResultsAndGUI);
  connect(m_match_reset, &QPushButton::clicked, this, &CheatsManager::Reset);

  m_match_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_watch_table->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(m_match_table, &QTableWidget::customContextMenuRequested, this,
          &CheatsManager::OnMatchContextMenu);
  connect(m_watch_table, &QTableWidget::customContextMenuRequested, this,
          &CheatsManager::OnWatchContextMenu);
  connect(m_watch_table, &QTableWidget::itemChanged, this, &CheatsManager::OnWatchItemChanged);
}

void CheatsManager::OnWatchContextMenu()
{
  if (m_watch_table->selectedItems().isEmpty())
    return;

  QMenu* menu = new QMenu(this);

  menu->addAction(tr("Remove from Watch"), this, [this] {
    auto* item = m_watch_table->selectedItems()[0];
    int index = item->data(INDEX_ROLE).toInt();
    m_watch.erase(m_watch.begin() + index);
    UpdateGUI();
  });

  menu->addSeparator();

  menu->addAction(tr("Generate Action Replay Code"), this, &CheatsManager::GenerateARCode);

  menu->exec(QCursor::pos());
}

void CheatsManager::OnMatchContextMenu()
{
  if (m_match_table->selectedItems().isEmpty())
    return;

  QMenu* menu = new QMenu(this);

  menu->addAction(tr("Add to Watch"), this, [this] {
    auto* item = m_match_table->selectedItems()[0];
    int index = item->data(INDEX_ROLE).toInt();
    m_watch.emplace_back();
    m_watch.back().m_result = m_results[index];
    UpdateGUI();
  });

  menu->exec(QCursor::pos());
}

void CheatsManager::GenerateARCode()
{
  if (!m_ar_code)
    return;

  auto* item = m_watch_table->selectedItems()[0];

  int index = item->data(INDEX_ROLE).toInt();
  ActionReplay::ARCode ar_code;

  ar_code.active = true;
  ar_code.user_defined = true;
  ar_code.name = tr("Generated by search (Address %1)")
                     .arg(m_watch[index].m_result.m_address, 8, 16, QLatin1Char('0'))
                     .toStdString();

  ar_code.ops = ResultToAREntries(m_watch[index].m_result);

  m_ar_code->AddCode(ar_code);
}

void CheatsManager::OnWatchItemChanged(QTableWidgetItem* item)
{
  if (m_updating)
    return;

  int index = item->data(INDEX_ROLE).toInt();
  int column = item->data(COLUMN_ROLE).toInt();

  switch (column)
  {
  case 0:
    m_watch[index].m_name = item->text();
    break;
  case 2:
    m_watch[index].m_locked = item->checkState() == Qt::Checked;
    UpdatePatch(m_watch[index]);
    break;
  case 3:
  {
    const auto text = item->text();
    switch (Cheats::GetDataType(m_watch[index].m_result.m_value))
    {
    case Cheats::DataType::U8:
      m_watch[index].m_result.m_value.m_value = static_cast<u8>(text.toUShort(nullptr, 16) & 0xFF);
      break;
    case Cheats::DataType::U16:
      m_watch[index].m_result.m_value.m_value = static_cast<u16>(text.toUShort(nullptr, 16));
      break;
    case Cheats::DataType::U32:
      m_watch[index].m_result.m_value.m_value = static_cast<u32>(text.toUInt(nullptr, 16));
      break;
    case Cheats::DataType::U64:
      m_watch[index].m_result.m_value.m_value = static_cast<u64>(text.toULongLong(nullptr, 16));
      break;
    case Cheats::DataType::F32:
      m_watch[index].m_result.m_value.m_value = text.toFloat();
      break;
    case Cheats::DataType::F64:
      m_watch[index].m_result.m_value.m_value = text.toDouble();
      break;
    default:
      break;
    }

    UpdatePatch(m_watch[index]);
    break;
  }
  }

  UpdateResultsAndGUI();
}

QWidget* CheatsManager::CreateCheatSearch()
{
  m_match_table = new QTableWidget;
  m_watch_table = new QTableWidget;

  m_match_table->verticalHeader()->hide();
  m_watch_table->verticalHeader()->hide();

  m_match_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_watch_table->setSelectionBehavior(QAbstractItemView::SelectRows);

  // Options
  m_result_label = new QLabel;
  m_match_length = new QComboBox;
  m_match_operation = new QComboBox;
  m_match_value = new QLineEdit;
  m_match_new = new QPushButton(tr("New Search"));
  m_match_next = new QPushButton(tr("Next Search"));
  m_match_refresh = new QPushButton(tr("Refresh"));
  m_match_reset = new QPushButton(tr("Reset"));

  auto* options = new QWidget;
  auto* layout = new QVBoxLayout;
  options->setLayout(layout);

  m_match_length->addItem(tr("8-bit Integer"), QVariant::fromValue(Cheats::DataType::U8));
  m_match_length->addItem(tr("16-bit Integer"), QVariant::fromValue(Cheats::DataType::U16));
  m_match_length->addItem(tr("32-bit Integer"), QVariant::fromValue(Cheats::DataType::U32));
  m_match_length->addItem(tr("64-bit Integer"), QVariant::fromValue(Cheats::DataType::U64));
  m_match_length->addItem(tr("Float"), QVariant::fromValue(Cheats::DataType::F32));
  m_match_length->addItem(tr("Double"), QVariant::fromValue(Cheats::DataType::F64));
  m_match_length->addItem(tr("String"), QVariant::fromValue(Cheats::DataType::ByteArray));

  m_match_operation->addItem(tr("Equals to"), QVariant::fromValue(Cheats::CompareType::Equal));
  m_match_operation->addItem(tr("Not equals to"),
                             QVariant::fromValue(Cheats::CompareType::NotEqual));
  m_match_operation->addItem(tr("Less than"), QVariant::fromValue(Cheats::CompareType::Less));
  m_match_operation->addItem(tr("Less or equal to"),
                             QVariant::fromValue(Cheats::CompareType::LessEqual));
  m_match_operation->addItem(tr("More than"), QVariant::fromValue(Cheats::CompareType::More));
  m_match_operation->addItem(tr("More or equal to"),
                             QVariant::fromValue(Cheats::CompareType::MoreEqual));

  auto* group_box = new QGroupBox(tr("Type"));
  auto* group_layout = new QHBoxLayout;
  group_box->setLayout(group_layout);

  // i18n: The base 10 numeral system. Not related to non-integer numbers
  m_match_decimal = new QRadioButton(tr("Decimal"));
  m_match_hexadecimal = new QRadioButton(tr("Hexadecimal"));
  m_match_octal = new QRadioButton(tr("Octal"));

  group_layout->addWidget(m_match_decimal);
  group_layout->addWidget(m_match_hexadecimal);
  group_layout->addWidget(m_match_octal);

  layout->addWidget(m_result_label);
  layout->addWidget(m_match_length);
  layout->addWidget(m_match_operation);
  layout->addWidget(m_match_value);
  layout->addWidget(group_box);
  layout->addWidget(m_match_new);
  layout->addWidget(m_match_next);
  layout->addWidget(m_match_refresh);
  layout->addWidget(m_match_reset);

  // Splitters
  m_option_splitter = new QSplitter(Qt::Horizontal);
  m_table_splitter = new QSplitter(Qt::Vertical);

  m_table_splitter->addWidget(m_match_table);
  m_table_splitter->addWidget(m_watch_table);

  m_option_splitter->addWidget(m_table_splitter);
  m_option_splitter->addWidget(options);

  return m_option_splitter;
}

std::optional<Cheats::SearchValue> CheatsManager::ParseValue()
{
  const QString text = m_match_value->text();

  if (text.isEmpty())
  {
    m_result_label->setText(tr("No search value entered."));
    return std::nullopt;
  }

  const int base =
      (m_match_decimal->isChecked() ? 10 : (m_match_hexadecimal->isChecked() ? 16 : 8));

  bool conversion_succeeded = false;
  Cheats::SearchValue value;
  switch (m_match_length->currentData().value<Cheats::DataType>())
  {
  case Cheats::DataType::U8:
    value.m_value = static_cast<u8>(text.toUShort(&conversion_succeeded, base) & 0xFF);
    break;
  case Cheats::DataType::U16:
    value.m_value = static_cast<u16>(text.toUShort(&conversion_succeeded, base));
    break;
  case Cheats::DataType::U32:
    value.m_value = static_cast<u32>(text.toUInt(&conversion_succeeded, base));
    break;
  case Cheats::DataType::U64:
    value.m_value = static_cast<u64>(text.toULongLong(&conversion_succeeded, base));
    break;
  case Cheats::DataType::F32:
    value.m_value = text.toFloat(&conversion_succeeded);
    break;
  case Cheats::DataType::F64:
    value.m_value = text.toDouble(&conversion_succeeded);
    break;
  case Cheats::DataType::ByteArray:
  {
    conversion_succeeded = true;
    const QByteArray utf8_bytes = text.toUtf8();
    value.m_value = std::vector<u8>(utf8_bytes.begin(), utf8_bytes.end());
    break;
  }
  }

  if (conversion_succeeded)
    return value;

  m_result_label->setText(tr("Cannot interpret the given value.\nHave you chosen the right type?"));
  return std::nullopt;
}

void CheatsManager::NewSearch()
{
  Cheats::CompareType op = m_match_operation->currentData().value<Cheats::CompareType>();
  std::optional<Cheats::SearchValue> value = ParseValue();
  if (!value)
    return;

  auto new_results = Cheats::NewSearch(op, *value);
  if (!new_results)
  {
    m_result_label->setText(tr("Search failed. Please try again."));
    return;
  }

  m_results = std::move(*new_results);

  m_match_next->setEnabled(true);
  UpdateGUI();
}

void CheatsManager::NextSearch()
{
  Cheats::CompareType op = m_match_operation->currentData().value<Cheats::CompareType>();
  std::optional<Cheats::SearchValue> value = ParseValue();
  if (!value)
    return;

  auto new_results = Cheats::NextSearch(m_results, op, *value);
  if (!new_results)
  {
    m_result_label->setText(tr("Search failed. Please try again."));
    return;
  }

  m_results = std::move(*new_results);

  UpdateGUI();
}

static QString GetValueString(const Cheats::SearchValue& value)
{
  switch (Cheats::GetDataType(value))
  {
  case Cheats::DataType::U8:
    return QStringLiteral("%1").arg(std::get<u8>(value.m_value), 2, 16, QLatin1Char('0'));
  case Cheats::DataType::U16:
    return QStringLiteral("%1").arg(std::get<u16>(value.m_value), 4, 16, QLatin1Char('0'));
  case Cheats::DataType::U32:
    return QStringLiteral("%1").arg(std::get<u32>(value.m_value), 8, 16, QLatin1Char('0'));
  case Cheats::DataType::U64:
    return QStringLiteral("%1").arg(std::get<u64>(value.m_value), 8, 16, QLatin1Char('0'));
  case Cheats::DataType::F32:
    return QString::number(std::get<float>(value.m_value));
  case Cheats::DataType::F64:
    return QString::number(std::get<double>(value.m_value));
  case Cheats::DataType::ByteArray:
  {
    const std::vector<u8>& data = std::get<std::vector<u8>>(value.m_value);
    return QStringLiteral("0x%1").arg(QString::fromLatin1(
        QByteArray(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()))
            .toHex()));
  }
  default:
    return {};
  }
}

void CheatsManager::UpdateResults()
{
  if (!m_results.empty())
  {
    auto new_results = Cheats::UpdateValues(m_results);
    if (new_results)
      m_results = std::move(*new_results);
  }

  if (!m_watch.empty())
  {
    std::vector<Cheats::SearchResult> watch_results;
    watch_results.reserve(m_watch.size());
    for (const Result& r : m_watch)
      watch_results.push_back(r.m_result);

    auto new_watch_results = Cheats::UpdateValues(watch_results);
    if (new_watch_results && new_watch_results->size() == m_watch.size())
    {
      for (size_t i = 0; i < m_watch.size(); ++i)
        m_watch[i].m_result = (*new_watch_results)[i];
    }
  }
}

void CheatsManager::UpdateGUI()
{
  m_updating = true;

  m_match_table->clear();
  m_watch_table->clear();
  m_match_table->setColumnCount(2);
  m_watch_table->setColumnCount(4);

  m_match_table->setHorizontalHeaderLabels({tr("Address"), tr("Value")});
  m_watch_table->setHorizontalHeaderLabels({tr("Name"), tr("Address"), tr("Lock"), tr("Value")});

  const bool too_many_results = m_results.size() > MAX_RESULTS;
  if (too_many_results)
    m_result_label->setText(tr("Too many matches to display (%1)").arg(m_results.size()));
  else
    m_result_label->setText(tr("%1 Match(es)").arg(m_results.size()));

  size_t result_count_to_display = too_many_results ? 0 : m_results.size();
  m_match_table->setRowCount(static_cast<int>(result_count_to_display));

  for (size_t i = 0; i < result_count_to_display; i++)
  {
    auto* address_item = new QTableWidgetItem(
        QStringLiteral("%1").arg(m_results[i].m_address, 8, 16, QLatin1Char('0')));
    auto* value_item = new QTableWidgetItem;

    address_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    value_item->setText(GetValueString(m_results[i].m_value));

    address_item->setData(INDEX_ROLE, static_cast<int>(i));
    value_item->setData(INDEX_ROLE, static_cast<int>(i));

    m_match_table->setItem(static_cast<int>(i), 0, address_item);
    m_match_table->setItem(static_cast<int>(i), 1, value_item);
  }

  m_watch_table->setRowCount(static_cast<int>(m_watch.size()));

  for (size_t i = 0; i < m_watch.size(); i++)
  {
    auto* name_item = new QTableWidgetItem(m_watch[i].m_name);
    auto* address_item = new QTableWidgetItem(
        QStringLiteral("%1").arg(m_watch[i].m_result.m_address, 8, 16, QLatin1Char('0')));
    auto* lock_item = new QTableWidgetItem;
    auto* value_item = new QTableWidgetItem;

    value_item->setText(GetValueString(m_results[i].m_value));

    name_item->setData(INDEX_ROLE, static_cast<int>(i));
    name_item->setData(COLUMN_ROLE, 0);

    address_item->setData(INDEX_ROLE, static_cast<int>(i));
    address_item->setData(COLUMN_ROLE, 1);

    lock_item->setData(INDEX_ROLE, static_cast<int>(i));
    lock_item->setData(COLUMN_ROLE, 2);

    value_item->setData(INDEX_ROLE, static_cast<int>(i));
    value_item->setData(COLUMN_ROLE, 3);

    name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    address_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    lock_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);

    lock_item->setCheckState(m_watch[i].m_locked ? Qt::Checked : Qt::Unchecked);

    m_watch_table->setItem(static_cast<int>(i), 0, name_item);
    m_watch_table->setItem(static_cast<int>(i), 1, address_item);
    m_watch_table->setItem(static_cast<int>(i), 2, lock_item);
    m_watch_table->setItem(static_cast<int>(i), 3, value_item);
  }

  m_updating = false;
}

void CheatsManager::UpdateResultsAndGUI()
{
  UpdateResults();
  UpdateGUI();
}

void CheatsManager::Reset()
{
  m_results.clear();
  m_watch.clear();
  m_match_next->setEnabled(false);
  m_match_table->clear();
  m_watch_table->clear();
  m_match_decimal->setChecked(true);
  m_result_label->clear();

  UpdateGUI();
}

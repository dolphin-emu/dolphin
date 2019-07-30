// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/CheatsManager.h"

#include <algorithm>
#include <cstring>

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

enum class CompareType : int
{
  Equal = 0,
  NotEqual = 1,
  Less = 2,
  LessEqual = 3,
  More = 4,
  MoreEqual = 5
};

enum class DataType : int
{
  Byte = 0,
  Short = 1,
  Int = 2,
  Float = 3,
  Double = 4,
  String = 5
};

struct Result
{
  u32 address;
  DataType type;
  QString name;
  bool locked = false;
  u32 locked_value;
};

static u32 GetResultValue(Result result)
{
  switch (result.type)
  {
  case DataType::Byte:
    return PowerPC::HostRead_U8(result.address);
  case DataType::Short:
    return PowerPC::HostRead_U16(result.address);
  case DataType::Int:
    return PowerPC::HostRead_U32(result.address);
  default:
    return 0;
  }
}

static void UpdatePatch(Result result)
{
  PowerPC::debug_interface.UnsetPatch(result.address);
  if (result.locked)
  {
    switch (result.type)
    {
    case DataType::Byte:
      PowerPC::debug_interface.SetPatch(result.address,
                                        std::vector<u8>{static_cast<u8>(result.locked_value)});
      break;
    default:
      PowerPC::debug_interface.SetPatch(result.address, result.locked_value);
      break;
    }
  }
}

static ActionReplay::AREntry ResultToAREntry(Result result)
{
  u8 cmd;

  switch (result.type)
  {
  case DataType::Byte:
    cmd = AR_SET_BYTE_CMD;
    break;
  case DataType::Short:
    cmd = AR_SET_SHORT_CMD;
    break;
  default:
  case DataType::Int:
    cmd = AR_SET_INT_CMD;
    break;
  }

  u32 address = result.address & 0xffffff;

  return ActionReplay::AREntry(cmd << 24 | address, result.locked_value);
}

template <typename T>
static bool Compare(T mem_value, T value, CompareType op)
{
  switch (op)
  {
  case CompareType::Equal:
    return mem_value == value;
  case CompareType::NotEqual:
    return mem_value != value;
  case CompareType::Less:
    return mem_value < value;
  case CompareType::LessEqual:
    return mem_value <= value;
  case CompareType::More:
    return mem_value > value;
  case CompareType::MoreEqual:
    return mem_value >= value;
  default:
    return false;
  }
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
  Update();
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

  connect(m_match_new, &QPushButton::pressed, this, &CheatsManager::NewSearch);
  connect(m_match_next, &QPushButton::pressed, this, &CheatsManager::NextSearch);
  connect(m_match_refresh, &QPushButton::pressed, this, &CheatsManager::Update);
  connect(m_match_reset, &QPushButton::pressed, this, &CheatsManager::Reset);

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

    Update();
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

    m_results[index].locked_value = GetResultValue(m_results[index]);

    m_watch.push_back(m_results[index]);

    Update();
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
                     .arg(m_watch[index].address, 8, 16, QLatin1Char('0'))
                     .toStdString();

  ar_code.ops.push_back(ResultToAREntry(m_watch[index]));

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
    m_watch[index].name = item->text();
    break;
  case 2:
    m_watch[index].locked = item->checkState() == Qt::Checked;

    if (m_watch[index].locked)
      m_watch[index].locked_value = GetResultValue(m_results[index]);

    UpdatePatch(m_watch[index]);
    break;
  case 3:
  {
    const auto text = item->text();
    u32 value = 0;

    switch (m_watch[index].type)
    {
    case DataType::Byte:
      value = text.toUShort(nullptr, 16) & 0xFF;
      break;
    case DataType::Short:
      value = text.toUShort(nullptr, 16);
      break;
    case DataType::Int:
      value = text.toUInt(nullptr, 16);
      break;
    case DataType::Float:
    {
      float f = text.toFloat();
      std::memcpy(&value, &f, sizeof(float));
      break;
    }
    default:
      break;
    }

    m_watch[index].locked_value = value;

    UpdatePatch(m_watch[index]);

    break;
  }
  }

  Update();
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

  for (const auto& option : {tr("8-bit Integer"), tr("16-bit Integer"), tr("32-bit Integer"),
                             tr("Float"), tr("Double"), tr("String")})
  {
    m_match_length->addItem(option);
  }

  for (const auto& option : {tr("Equals to"), tr("Not equals to"), tr("Less than"),
                             tr("Less or equal to"), tr("More than"), tr("More or equal to")})
  {
    m_match_operation->addItem(option);
  }

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

size_t CheatsManager::GetTypeSize() const
{
  switch (static_cast<DataType>(m_match_length->currentIndex()))
  {
  case DataType::Byte:
    return sizeof(u8);
  case DataType::Short:
    return sizeof(u16);
  case DataType::Int:
    return sizeof(u32);
  case DataType::Float:
    return sizeof(float);
  case DataType::Double:
    return sizeof(double);
  default:
    return m_match_value->text().toStdString().size();
  }
}

std::function<bool(u32)> CheatsManager::CreateMatchFunction()
{
  const QString text = m_match_value->text();

  if (text.isEmpty())
  {
    m_result_label->setText(tr("No search value entered."));
    return nullptr;
  }

  const CompareType op = static_cast<CompareType>(m_match_operation->currentIndex());

  const int base =
      (m_match_decimal->isChecked() ? 10 : (m_match_hexadecimal->isChecked() ? 16 : 8));

  bool conversion_succeeded = false;
  std::function<bool(u32)> matches_func;
  switch (static_cast<DataType>(m_match_length->currentIndex()))
  {
  case DataType::Byte:
  {
    u8 comparison_value = text.toUShort(&conversion_succeeded, base) & 0xFF;
    matches_func = [=](u32 addr) {
      return Compare<u8>(PowerPC::HostRead_U8(addr), comparison_value, op);
    };
    break;
  }
  case DataType::Short:
  {
    u16 comparison_value = text.toUShort(&conversion_succeeded, base);
    matches_func = [=](u32 addr) {
      return Compare(PowerPC::HostRead_U16(addr), comparison_value, op);
    };
    break;
  }
  case DataType::Int:
  {
    u32 comparison_value = text.toUInt(&conversion_succeeded, base);
    matches_func = [=](u32 addr) {
      return Compare(PowerPC::HostRead_U32(addr), comparison_value, op);
    };
    break;
  }
  case DataType::Float:
  {
    float comparison_value = text.toFloat(&conversion_succeeded);
    matches_func = [=](u32 addr) {
      return Compare(PowerPC::HostRead_F32(addr), comparison_value, op);
    };
    break;
  }
  case DataType::Double:
  {
    double comparison_value = text.toDouble(&conversion_succeeded);
    matches_func = [=](u32 addr) {
      return Compare(PowerPC::HostRead_F64(addr), comparison_value, op);
    };
    break;
  }
  case DataType::String:
  {
    if (op != CompareType::Equal && op != CompareType::NotEqual)
    {
      m_result_label->setText(tr("String values can only be compared using equality."));
      return nullptr;
    }

    conversion_succeeded = true;

    const QString lambda_text = m_match_value->text();
    const QByteArray utf8_bytes = lambda_text.toUtf8();

    matches_func = [op, utf8_bytes](u32 addr) {
      bool is_equal = std::equal(utf8_bytes.cbegin(), utf8_bytes.cend(),
                                 reinterpret_cast<char*>(Memory::m_pRAM + addr - 0x80000000));
      switch (op)
      {
      case CompareType::Equal:
        return is_equal;
      case CompareType::NotEqual:
        return !is_equal;
      default:
        // This should never occur since we've already checked the type of op
        return false;
      }
    };
    break;
  }
  }

  if (conversion_succeeded)
    return matches_func;

  m_result_label->setText(tr("Cannot interpret the given value.\nHave you chosen the right type?"));
  return nullptr;
}

void CheatsManager::NewSearch()
{
  m_results.clear();
  const u32 base_address = 0x80000000;

  if (!Memory::m_pRAM)
  {
    m_result_label->setText(tr("Memory Not Ready"));
    return;
  }

  std::function<bool(u32)> matches_func = CreateMatchFunction();
  if (matches_func == nullptr)
    return;

  Core::RunAsCPUThread([&] {
    for (u32 i = 0; i < Memory::REALRAM_SIZE - GetTypeSize(); i++)
    {
      if (PowerPC::HostIsRAMAddress(base_address + i) && matches_func(base_address + i))
        m_results.push_back(
            {base_address + i, static_cast<DataType>(m_match_length->currentIndex())});
    }
  });

  m_match_next->setEnabled(true);

  Update();
}

void CheatsManager::NextSearch()
{
  if (!Memory::m_pRAM)
  {
    m_result_label->setText(tr("Memory Not Ready"));
    return;
  }

  std::function<bool(u32)> matches_func = CreateMatchFunction();
  if (matches_func == nullptr)
    return;

  Core::RunAsCPUThread([this, matches_func] {
    m_results.erase(std::remove_if(m_results.begin(), m_results.end(),
                                   [matches_func](Result r) {
                                     return !PowerPC::HostIsRAMAddress(r.address) ||
                                            !matches_func(r.address);
                                   }),
                    m_results.end());
  });

  Update();
}

static QString GetResultString(const Result& result)
{
  if (!PowerPC::HostIsRAMAddress(result.address))
  {
    return QStringLiteral("---");
  }
  switch (result.type)
  {
  case DataType::Byte:
    return QStringLiteral("%1").arg(PowerPC::HostRead_U8(result.address), 2, 16, QLatin1Char('0'));
  case DataType::Short:
    return QStringLiteral("%1").arg(PowerPC::HostRead_U16(result.address), 4, 16, QLatin1Char('0'));
  case DataType::Int:
    return QStringLiteral("%1").arg(PowerPC::HostRead_U32(result.address), 8, 16, QLatin1Char('0'));
  case DataType::Float:
    return QString::number(PowerPC::HostRead_F32(result.address));
  case DataType::Double:
    return QString::number(PowerPC::HostRead_F64(result.address));
  case DataType::String:
    return QObject::tr("String Match");
  default:
    // Make MSVC happy
    return QStringLiteral("");
  }
}

void CheatsManager::Update()
{
  m_match_table->clear();
  m_watch_table->clear();
  m_match_table->setColumnCount(2);
  m_watch_table->setColumnCount(4);

  m_match_table->setHorizontalHeaderLabels({tr("Address"), tr("Value")});
  m_watch_table->setHorizontalHeaderLabels({tr("Name"), tr("Address"), tr("Lock"), tr("Value")});

  if (m_results.size() > MAX_RESULTS)
  {
    m_result_label->setText(tr("Too many matches to display (%1)").arg(m_results.size()));
    return;
  }

  m_result_label->setText(tr("%1 Match(es)").arg(m_results.size()));
  m_match_table->setRowCount(static_cast<int>(m_results.size()));

  if (m_results.empty())
    return;

  m_updating = true;

  Core::RunAsCPUThread([this] {
    for (size_t i = 0; i < m_results.size(); i++)
    {
      auto* address_item = new QTableWidgetItem(
          QStringLiteral("%1").arg(m_results[i].address, 8, 16, QLatin1Char('0')));
      auto* value_item = new QTableWidgetItem;

      address_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      value_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

      value_item->setText(GetResultString(m_results[i]));

      address_item->setData(INDEX_ROLE, static_cast<int>(i));
      value_item->setData(INDEX_ROLE, static_cast<int>(i));

      m_match_table->setItem(static_cast<int>(i), 0, address_item);
      m_match_table->setItem(static_cast<int>(i), 1, value_item);
    }

    m_watch_table->setRowCount(static_cast<int>(m_watch.size()));

    for (size_t i = 0; i < m_watch.size(); i++)
    {
      auto* name_item = new QTableWidgetItem(m_watch[i].name);
      auto* address_item = new QTableWidgetItem(
          QStringLiteral("%1").arg(m_watch[i].address, 8, 16, QLatin1Char('0')));
      auto* lock_item = new QTableWidgetItem;
      auto* value_item = new QTableWidgetItem;

      value_item->setText(GetResultString(m_results[i]));

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

      lock_item->setCheckState(m_watch[i].locked ? Qt::Checked : Qt::Unchecked);

      m_watch_table->setItem(static_cast<int>(i), 0, name_item);
      m_watch_table->setItem(static_cast<int>(i), 1, address_item);
      m_watch_table->setItem(static_cast<int>(i), 2, lock_item);
      m_watch_table->setItem(static_cast<int>(i), 3, value_item);
    }
  });

  m_updating = false;
}

void CheatsManager::Reset()
{
  m_results.clear();
  m_watch.clear();
  m_match_next->setEnabled(false);
  m_match_table->clear();
  m_watch_table->clear();
  m_match_decimal->setChecked(true);
  m_result_label->setText(QStringLiteral(""));

  Update();
}

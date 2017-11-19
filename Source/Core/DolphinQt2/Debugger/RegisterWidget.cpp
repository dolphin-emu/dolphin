// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Debugger/RegisterWidget.h"

#include "Core/Core.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt2/QtUtils/ActionHelper.h"
#include "DolphinQt2/Settings.h"

#include <QHeaderView>
#include <QMenu>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

RegisterWidget::RegisterWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Registers"));
  setAllowedAreas(Qt::AllDockWidgetAreas);

  QSettings settings;

  restoreGeometry(settings.value(QStringLiteral("registerwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("registerwidget/floating")).toBool());

  CreateWidgets();
  PopulateTable();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, [this](Core::State state) {
    if (Settings::Instance().IsDebugModeEnabled() && Core::GetState() == Core::State::Paused)
      emit RequestTableUpdate();
  });

  connect(this, &RegisterWidget::RequestTableUpdate, [this] {
    m_updating = true;
    emit UpdateTable();
    m_updating = false;
  });

  connect(&Settings::Instance(), &Settings::RegistersVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsRegistersVisible());
  });

  setHidden(!Settings::Instance().IsRegistersVisible() ||
            !Settings::Instance().IsDebugModeEnabled());
}

RegisterWidget::~RegisterWidget()
{
  QSettings settings;

  settings.setValue(QStringLiteral("registerwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("registerwidget/floating"), isFloating());
}

void RegisterWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetRegistersVisible(false);
}

void RegisterWidget::CreateWidgets()
{
  m_table = new QTableWidget;

  m_table->setColumnCount(9);

  m_table->verticalHeader()->setVisible(false);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);

  QStringList empty_list;

  for (auto i = 0; i < 9; i++)
    empty_list << QStringLiteral("");

  m_table->setHorizontalHeaderLabels(empty_list);

  QWidget* widget = new QWidget;
  auto* layout = new QVBoxLayout;
  layout->addWidget(m_table);
  widget->setLayout(layout);

  setWidget(widget);
}

void RegisterWidget::ConnectWidgets()
{
  connect(m_table, &QTableWidget::customContextMenuRequested, this,
          &RegisterWidget::ShowContextMenu);
  connect(m_table, &QTableWidget::itemChanged, this, &RegisterWidget::OnItemChanged);
}

void RegisterWidget::OnItemChanged(QTableWidgetItem* item)
{
  if (!item->data(DATA_TYPE).isNull() && !m_updating)
    static_cast<RegisterColumn*>(item)->SetValue();
}

void RegisterWidget::ShowContextMenu()
{
  QMenu* menu = new QMenu(this);

  if (m_table->selectedItems().size())
  {
    auto variant = m_table->selectedItems()[0]->data(DATA_TYPE);

    if (!variant.isNull())
    {
      auto* item = reinterpret_cast<RegisterColumn*>(m_table->selectedItems()[0]);
      auto type = static_cast<RegisterType>(item->data(DATA_TYPE).toInt());
      auto display = item->GetDisplay();

      AddAction(menu, tr("Add to &watch"), this,
                [this, item] { emit RequestMemoryBreakpoint(item->GetValue()); });
      menu->addAction(tr("View &memory"));
      menu->addAction(tr("View &code"));

      menu->addSeparator();

      QActionGroup* group = new QActionGroup(menu);
      group->setExclusive(true);

      auto* view_hex = menu->addAction(tr("Hexadecimal"));
      auto* view_int = menu->addAction(tr("Signed Integer"));
      auto* view_uint = menu->addAction(tr("Unsigned Integer"));
      auto* view_float = menu->addAction(tr("Float"));

      for (auto* action : {view_hex, view_int, view_uint, view_float})
      {
        action->setCheckable(true);
        action->setVisible(false);
        action->setActionGroup(group);
      }

      switch (display)
      {
      case RegisterDisplay::Hex:
        view_hex->setChecked(true);
        break;
      case RegisterDisplay::SInt32:
        view_int->setChecked(true);
        break;
      case RegisterDisplay::UInt32:
        view_uint->setChecked(true);
        break;
      case RegisterDisplay::Float:
        view_float->setChecked(true);
        break;
      }

      switch (type)
      {
      case RegisterType::gpr:
        view_hex->setVisible(true);
        view_int->setVisible(true);
        view_uint->setVisible(true);
        view_float->setVisible(true);
        break;
      case RegisterType::fpr:
        view_hex->setVisible(true);
        view_float->setVisible(true);
        break;
      default:
        break;
      }

      connect(view_hex, &QAction::triggered, [this, item] {
        m_updating = true;
        item->SetDisplay(RegisterDisplay::Hex);
        m_updating = false;
      });

      connect(view_int, &QAction::triggered, [this, item] {
        m_updating = true;
        item->SetDisplay(RegisterDisplay::SInt32);
        m_updating = false;
      });

      connect(view_uint, &QAction::triggered, [this, item] {
        m_updating = true;
        item->SetDisplay(RegisterDisplay::UInt32);
        m_updating = false;
      });

      connect(view_float, &QAction::triggered, [this, item] {
        m_updating = true;
        item->SetDisplay(RegisterDisplay::Float);
        m_updating = false;
      });

      menu->addSeparator();
    }
  }

  AddAction(menu, tr("Update"), this, [this] { emit RequestTableUpdate(); });

  menu->exec(QCursor::pos());
}

void RegisterWidget::PopulateTable()
{
  for (int i = 0; i < 32; i++)
  {
    // General purpose registers (int)
    AddRegister(i, 0, RegisterType::gpr, "r" + std::to_string(i), [i] { return GPR(i); },
                [i](u64 value) { GPR(i) = value; });

    // General purpose registers (float)
    AddRegister(i, 2, RegisterType::fpr, "f" + std::to_string(i), [i] { return riPS0(i); },
                [i](u64 value) { riPS0(i) = value; });

    AddRegister(i, 4, RegisterType::fpr, "", [i] { return riPS1(i); },
                [i](u64 value) { riPS1(i) = value; });
  }

  for (int i = 0; i < 8; i++)
  {
    // IBAT registers
    AddRegister(i, 5, RegisterType::ibat, "IBAT" + std::to_string(i),
                [i] {
                  return (static_cast<u64>(PowerPC::ppcState.spr[SPR_IBAT0U + i * 2]) << 32) +
                         PowerPC::ppcState.spr[SPR_IBAT0L + i * 2];
                },
                nullptr);
    // DBAT registers
    AddRegister(i + 8, 5, RegisterType::dbat, "DBAT" + std::to_string(i),
                [i] {
                  return (static_cast<u64>(PowerPC::ppcState.spr[SPR_DBAT0U + i * 2]) << 32) +
                         PowerPC::ppcState.spr[SPR_DBAT0L + i * 2];
                },
                nullptr);
  }

  for (int i = 0; i < 16; i++)
  {
    // SR registers
    AddRegister(i, 7, RegisterType::sr, "SR" + std::to_string(i),
                [i] { return PowerPC::ppcState.sr[i]; },
                [i](u64 value) { PowerPC::ppcState.sr[i] = value; });
  }

  // Special registers
  // PC
  AddRegister(16, 5, RegisterType::pc, "PC", [] { return PowerPC::ppcState.pc; },
              [](u64 value) { PowerPC::ppcState.pc = value; });

  // LR
  AddRegister(17, 5, RegisterType::fpscr, "LR", [] { return PowerPC::ppcState.spr[SPR_LR]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_LR] = value; });

  // CTR
  AddRegister(18, 5, RegisterType::fpscr, "FPSCR", [] { return PowerPC::ppcState.spr[SPR_CTR]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_CTR] = value; });

  // CR
  AddRegister(19, 5, RegisterType::cr, "CR", [] { return GetCR(); },
              [](u64 value) { SetCR(value); });

  // FPSCR
  AddRegister(20, 5, RegisterType::fpscr, "FPSCR", [] { return PowerPC::ppcState.fpscr; },
              [](u64 value) { PowerPC::ppcState.fpscr = value; });

  // MSR
  AddRegister(21, 5, RegisterType::msr, "MSR", [] { return PowerPC::ppcState.msr; },
              [](u64 value) { PowerPC::ppcState.msr = value; });

  // SRR 0-1
  AddRegister(22, 5, RegisterType::srr, "SRR0", [] { return PowerPC::ppcState.spr[SPR_SRR0]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_SRR0] = value; });
  AddRegister(23, 5, RegisterType::srr, "SRR1", [] { return PowerPC::ppcState.spr[SPR_SRR1]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_SRR1] = value; });

  // Exceptions
  AddRegister(24, 5, RegisterType::exceptions, "Exceptions",
              [] { return PowerPC::ppcState.Exceptions; },
              [](u64 value) { PowerPC::ppcState.Exceptions = value; });

  // Int Mask
  AddRegister(25, 5, RegisterType::int_mask, "Int Mask",
              [] { return ProcessorInterface::GetMask(); }, nullptr);

  // Int Cause
  AddRegister(26, 5, RegisterType::int_cause, "Int Cause",
              [] { return ProcessorInterface::GetCause(); }, nullptr);

  // DSISR
  AddRegister(27, 5, RegisterType::dsisr, "DSISR", [] { return PowerPC::ppcState.spr[SPR_DSISR]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_DSISR] = value; });
  // DAR
  AddRegister(28, 5, RegisterType::dar, "DAR", [] { return PowerPC::ppcState.spr[SPR_DAR]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_DAR] = value; });

  // Hash Mask
  AddRegister(
      29, 5, RegisterType::pt_hashmask, "Hash Mask",
      [] { return (PowerPC::ppcState.pagetable_hashmask << 6) | PowerPC::ppcState.pagetable_base; },
      nullptr);

  emit RequestTableUpdate();
  m_table->resizeColumnsToContents();
}

void RegisterWidget::AddRegister(int row, int column, RegisterType type, std::string register_name,
                                 std::function<u32()> get_reg, std::function<void(u32)> set_reg)
{
  auto* value = new RegisterColumn(type, get_reg, set_reg);

  if (m_table->rowCount() <= row)
    m_table->setRowCount(row + 1);

  bool has_label = !register_name.empty();

  if (has_label)
  {
    auto* label = new QTableWidgetItem(QString::fromStdString(register_name));
    label->setFlags(Qt::ItemIsEnabled);

    QFont label_font = label->font();
    label_font.setBold(true);
    label->setFont(label_font);

    m_table->setItem(row, column, label);
    m_table->setItem(row, column + 1, value);
  }
  else
  {
    m_table->setItem(row, column, value);
  }

  connect(this, &RegisterWidget::UpdateTable, [value] { value->RefreshValue(); });
}

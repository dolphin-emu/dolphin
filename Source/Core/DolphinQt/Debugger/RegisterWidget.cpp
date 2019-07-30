// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/RegisterWidget.h"

#include <utility>

#include <QHeaderView>
#include <QMenu>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Core/Core.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

RegisterWidget::RegisterWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Registers"));
  setObjectName(QStringLiteral("registers"));

  setHidden(!Settings::Instance().IsRegistersVisible() ||
            !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("registerwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("registerwidget/floating")).toBool());

  CreateWidgets();
  PopulateTable();
  ConnectWidgets();

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &RegisterWidget::Update);

  connect(&Settings::Instance(), &Settings::RegistersVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsRegistersVisible());
  });
}

RegisterWidget::~RegisterWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("registerwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("registerwidget/floating"), isFloating());
}

void RegisterWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetRegistersVisible(false);
}

void RegisterWidget::showEvent(QShowEvent* event)
{
  Update();
}

void RegisterWidget::CreateWidgets()
{
  m_table = new QTableWidget;

  m_table->setColumnCount(9);

  m_table->verticalHeader()->setVisible(false);
  m_table->verticalHeader()->setDefaultSectionSize(24);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_table->setFont(Settings::Instance().GetDebugFont());

  QStringList empty_list;

  for (auto i = 0; i < 9; i++)
    empty_list << QStringLiteral("");

  m_table->setHorizontalHeaderLabels(empty_list);

  QWidget* widget = new QWidget;
  auto* layout = new QVBoxLayout;
  layout->addWidget(m_table);
  layout->setContentsMargins(2, 2, 2, 2);
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

  auto variant = m_table->currentItem()->data(DATA_TYPE);

  if (!variant.isNull())
  {
    auto* item = static_cast<RegisterColumn*>(m_table->currentItem());
    auto type = static_cast<RegisterType>(item->data(DATA_TYPE).toInt());
    auto display = item->GetDisplay();

    // i18n: This kind of "watch" is used for watching emulated memory.
    // It's not related to timekeeping devices.
    menu->addAction(tr("Add to &watch"), this,
                    [this, item] { emit RequestMemoryBreakpoint(item->GetValue()); });
    menu->addAction(tr("View &memory"));
    menu->addAction(tr("View &code"));

    menu->addSeparator();

    QActionGroup* group = new QActionGroup(menu);
    group->setExclusive(true);

    auto* view_hex = menu->addAction(tr("Hexadecimal"));
    auto* view_int = menu->addAction(tr("Signed Integer"));
    auto* view_uint = menu->addAction(tr("Unsigned Integer"));
    // i18n: A floating point number
    auto* view_float = menu->addAction(tr("Float"));
    // i18n: A double precision floating point number
    auto* view_double = menu->addAction(tr("Double"));

    for (auto* action : {view_hex, view_int, view_uint, view_float, view_double})
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
    case RegisterDisplay::Double:
      view_double->setChecked(true);
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
      view_double->setVisible(true);
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

    connect(view_double, &QAction::triggered, [this, item] {
      m_updating = true;
      item->SetDisplay(RegisterDisplay::Double);
      m_updating = false;
    });

    menu->addSeparator();
  }

  menu->addAction(tr("Update"), this, [this] { emit RequestTableUpdate(); });

  menu->exec(QCursor::pos());
}

void RegisterWidget::PopulateTable()
{
  for (int i = 0; i < 32; i++)
  {
    // General purpose registers (int)
    AddRegister(i, 0, RegisterType::gpr, "r" + std::to_string(i), [i] { return GPR(i); },
                [i](u64 value) { GPR(i) = value; });

    // Floating point registers (double)
    AddRegister(i, 2, RegisterType::fpr, "f" + std::to_string(i), [i] { return rPS(i).PS0AsU64(); },
                [i](u64 value) { rPS(i).SetPS0(value); });

    AddRegister(i, 4, RegisterType::fpr, "", [i] { return rPS(i).PS1AsU64(); },
                [i](u64 value) { rPS(i).SetPS1(value); });
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
    // Graphics quantization registers
    AddRegister(i + 16, 7, RegisterType::gqr, "GQR" + std::to_string(i),
                [i] { return PowerPC::ppcState.spr[SPR_GQR0 + i]; }, nullptr);
  }

  // HID registers
  AddRegister(24, 7, RegisterType::hid, "HID0", [] { return PowerPC::ppcState.spr[SPR_HID0]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_HID0] = static_cast<u32>(value); });
  AddRegister(25, 7, RegisterType::hid, "HID1", [] { return PowerPC::ppcState.spr[SPR_HID1]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_HID1] = static_cast<u32>(value); });
  AddRegister(26, 7, RegisterType::hid, "HID2", [] { return PowerPC::ppcState.spr[SPR_HID2]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_HID2] = static_cast<u32>(value); });
  AddRegister(27, 7, RegisterType::hid, "HID4", [] { return PowerPC::ppcState.spr[SPR_HID4]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_HID4] = static_cast<u32>(value); });

  for (int i = 0; i < 16; i++)
  {
    // SR registers
    AddRegister(i, 7, RegisterType::sr, "SR" + std::to_string(i),
                [i] { return PowerPC::ppcState.sr[i]; },
                [i](u64 value) { PowerPC::ppcState.sr[i] = value; });
  }

  // Special registers
  // TB
  AddRegister(16, 5, RegisterType::tb, "TB", PowerPC::ReadFullTimeBaseValue, nullptr);

  // PC
  AddRegister(17, 5, RegisterType::pc, "PC", [] { return PowerPC::ppcState.pc; },
              [](u64 value) { PowerPC::ppcState.pc = value; });

  // LR
  AddRegister(18, 5, RegisterType::lr, "LR", [] { return PowerPC::ppcState.spr[SPR_LR]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_LR] = value; });

  // CTR
  AddRegister(19, 5, RegisterType::ctr, "CTR", [] { return PowerPC::ppcState.spr[SPR_CTR]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_CTR] = value; });

  // CR
  AddRegister(20, 5, RegisterType::cr, "CR", [] { return PowerPC::ppcState.cr.Get(); },
              [](u64 value) { PowerPC::ppcState.cr.Set(value); });

  // XER
  AddRegister(21, 5, RegisterType::xer, "XER", [] { return PowerPC::GetXER().Hex; },
              [](u64 value) { PowerPC::SetXER(UReg_XER(value)); });

  // FPSCR
  AddRegister(22, 5, RegisterType::fpscr, "FPSCR", [] { return PowerPC::ppcState.fpscr.Hex; },
              [](u64 value) { PowerPC::ppcState.fpscr = static_cast<u32>(value); });

  // MSR
  AddRegister(23, 5, RegisterType::msr, "MSR", [] { return PowerPC::ppcState.msr.Hex; },
              [](u64 value) { PowerPC::ppcState.msr.Hex = value; });

  // SRR 0-1
  AddRegister(24, 5, RegisterType::srr, "SRR0", [] { return PowerPC::ppcState.spr[SPR_SRR0]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_SRR0] = value; });
  AddRegister(25, 5, RegisterType::srr, "SRR1", [] { return PowerPC::ppcState.spr[SPR_SRR1]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_SRR1] = value; });

  // Exceptions
  AddRegister(26, 5, RegisterType::exceptions, "Exceptions",
              [] { return PowerPC::ppcState.Exceptions; },
              [](u64 value) { PowerPC::ppcState.Exceptions = value; });

  // Int Mask
  AddRegister(27, 5, RegisterType::int_mask, "Int Mask",
              [] { return ProcessorInterface::GetMask(); }, nullptr);

  // Int Cause
  AddRegister(28, 5, RegisterType::int_cause, "Int Cause",
              [] { return ProcessorInterface::GetCause(); }, nullptr);

  // DSISR
  AddRegister(29, 5, RegisterType::dsisr, "DSISR", [] { return PowerPC::ppcState.spr[SPR_DSISR]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_DSISR] = value; });
  // DAR
  AddRegister(30, 5, RegisterType::dar, "DAR", [] { return PowerPC::ppcState.spr[SPR_DAR]; },
              [](u64 value) { PowerPC::ppcState.spr[SPR_DAR] = value; });

  // Hash Mask
  AddRegister(
      31, 5, RegisterType::pt_hashmask, "Hash Mask",
      [] { return (PowerPC::ppcState.pagetable_hashmask << 6) | PowerPC::ppcState.pagetable_base; },
      nullptr);

  emit RequestTableUpdate();
  m_table->resizeColumnsToContents();
}

void RegisterWidget::AddRegister(int row, int column, RegisterType type, std::string register_name,
                                 std::function<u64()> get_reg, std::function<void(u64)> set_reg)
{
  auto* value = new RegisterColumn(type, std::move(get_reg), std::move(set_reg));

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
    m_table->item(row, column + 1)->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);
  }
  else
  {
    m_table->setItem(row, column, value);
    m_table->item(row, column)->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);
  }

  connect(this, &RegisterWidget::UpdateTable, [value] { value->RefreshValue(); });
}

void RegisterWidget::Update()
{
  if (isVisible() && Core::GetState() == Core::State::Paused)
  {
    m_updating = true;
    emit UpdateTable();
    m_updating = false;
  }
}

// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/RegisterWidget.h"

#include <utility>

#include <QActionGroup>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Core/Core.h"
#include "Core/Debugger/CodeTrace.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/Settings.h"

RegisterWidget::RegisterWidget(QWidget* parent)
    : QDockWidget(parent), m_system(Core::System::GetInstance())
{
  setWindowTitle(tr("Registers"));
  setObjectName(QStringLiteral("registers"));

  setHidden(!Settings::Instance().IsRegistersVisible() ||
            !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("registerwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("registerwidget/floating")).toBool());

  PopulateTable();
  ConnectWidgets();

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &RegisterWidget::Update);

  connect(&Settings::Instance(), &Settings::RegistersVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, [this](bool enabled) {
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
  m_table->setTabKeyNavigation(false);

  m_table->setColumnCount(9);

  m_table->verticalHeader()->setVisible(false);
  m_table->verticalHeader()->setDefaultSectionSize(24);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_table->setFont(Settings::Instance().GetDebugFont());

  QStringList empty_list;

  for (auto i = 0; i < 9; i++)
    empty_list << QString{};

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
  connect(&Settings::Instance(), &Settings::DebugFontChanged, m_table, &QWidget::setFont);
}

void RegisterWidget::OnItemChanged(QTableWidgetItem* item)
{
  if (!item->data(DATA_TYPE).isNull() && !m_updating)
    static_cast<RegisterColumn*>(item)->SetValue();
}

void RegisterWidget::ShowContextMenu()
{
  QMenu* menu = new QMenu(this);

  auto* raw_item = m_table->currentItem();

  if (raw_item != nullptr && !raw_item->data(DATA_TYPE).isNull())
  {
    auto* item = static_cast<RegisterColumn*>(raw_item);
    auto type = static_cast<RegisterType>(item->data(DATA_TYPE).toInt());
    auto display = item->GetDisplay();

    // i18n: This kind of "watch" is used for watching emulated memory.
    // It's not related to timekeeping devices.
    menu->addAction(tr("Add to &watch"), this, [this, item] {
      const u32 address = item->GetValue();
      const QString name = QStringLiteral("reg_%1").arg(address, 8, 16, QLatin1Char('0'));
      emit RequestWatch(name, address);
    });
    menu->addAction(tr("Add memory &breakpoint"), this,
                    [this, item] { emit RequestMemoryBreakpoint(item->GetValue()); });
    menu->addAction(tr("View &memory"), this,
                    [this, item] { emit RequestViewInMemory(item->GetValue()); });
    menu->addAction(tr("View &code"), this,
                    [this, item] { emit RequestViewInCode(item->GetValue()); });

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

    menu->addSeparator();

    auto* view_hex_column = menu->addAction(tr("All Hexadecimal"));
    view_hex_column->setData(static_cast<int>(RegisterDisplay::Hex));
    auto* view_int_column = menu->addAction(tr("All Signed Integer"));
    view_int_column->setData(static_cast<int>(RegisterDisplay::SInt32));
    auto* view_uint_column = menu->addAction(tr("All Unsigned Integer"));
    view_uint_column->setData(static_cast<int>(RegisterDisplay::UInt32));
    // i18n: A floating point number
    auto* view_float_column = menu->addAction(tr("All Float"));
    view_float_column->setData(static_cast<int>(RegisterDisplay::Float));
    // i18n: A double precision floating point number
    auto* view_double_column = menu->addAction(tr("All Double"));
    view_double_column->setData(static_cast<int>(RegisterDisplay::Double));

    if (type == RegisterType::gpr || type == RegisterType::fpr)
    {
      menu->addSeparator();

      const std::string type_string =
          fmt::format("{}{}", type == RegisterType::gpr ? "r" : "f", m_table->currentItem()->row());
      menu->addAction(tr("Run until hit (ignoring breakpoints)"),
                      [this, type_string]() { AutoStep(type_string); });
    }

    for (auto* action : {view_hex, view_int, view_uint, view_float, view_double})
    {
      action->setCheckable(true);
      action->setVisible(false);
      action->setActionGroup(group);
    }

    for (auto* action : {view_hex_column, view_int_column, view_uint_column, view_float_column,
                         view_double_column})
    {
      action->setVisible(false);
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
      view_hex_column->setVisible(true);
      view_int_column->setVisible(true);
      view_uint_column->setVisible(true);
      view_float_column->setVisible(true);
      break;
    case RegisterType::fpr:
      view_hex->setVisible(true);
      view_double->setVisible(true);
      view_hex_column->setVisible(true);
      view_double_column->setVisible(true);
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

    for (auto* action : {view_hex_column, view_int_column, view_uint_column, view_float_column,
                         view_double_column})
    {
      connect(action, &QAction::triggered, [this, action] {
        auto col = m_table->currentItem()->column();
        for (int i = 0; i < 32; i++)
        {
          auto* update_item = static_cast<RegisterColumn*>(m_table->item(i, col));
          update_item->SetDisplay(static_cast<RegisterDisplay>(action->data().toInt()));
        }
      });
    }

    menu->addSeparator();
  }

  menu->addAction(tr("Update"), this, [this] { emit RequestTableUpdate(); });

  menu->exec(QCursor::pos());
}

void RegisterWidget::AutoStep(const std::string& reg) const
{
  CodeTrace trace;
  trace.SetRegTracked(reg);

  QMessageBox msgbox(
      QMessageBox::NoIcon, tr("Timed Out"),
      tr("<font color='#ff0000'>AutoStepping timed out. Current instruction is irrelevant."),
      QMessageBox::Cancel);
  QPushButton* run_button = msgbox.addButton(tr("Keep Running"), QMessageBox::AcceptRole);

  while (true)
  {
    const AutoStepResults results = [this, &trace] {
      Core::CPUThreadGuard guard(m_system);
      return trace.AutoStepping(guard, true);
    }();

    emit Host::GetInstance()->UpdateDisasmDialog();

    if (!results.timed_out)
      break;

    // Can keep running and try again after a time out.
    SetQWidgetWindowDecorations(&msgbox);
    msgbox.exec();
    if (msgbox.clickedButton() != (QAbstractButton*)run_button)
      break;
  }
}

void RegisterWidget::PopulateTable()
{
  for (int i = 0; i < 32; i++)
  {
    // General purpose registers (int)
    AddRegister(
        i, 0, RegisterType::gpr, "r" + std::to_string(i),
        [this, i] { return m_system.GetPPCState().gpr[i]; },
        [this, i](u64 value) { m_system.GetPPCState().gpr[i] = value; });

    // Floating point registers (double)
    AddRegister(
        i, 2, RegisterType::fpr, "f" + std::to_string(i),
        [this, i] { return m_system.GetPPCState().ps[i].PS0AsU64(); },
        [this, i](u64 value) { m_system.GetPPCState().ps[i].SetPS0(value); });

    AddRegister(
        i, 4, RegisterType::fpr, "", [this, i] { return m_system.GetPPCState().ps[i].PS1AsU64(); },
        [this, i](u64 value) { m_system.GetPPCState().ps[i].SetPS1(value); });
  }

  // The IBAT and DBAT registers have a large gap between
  // registers 3 and 4 so we can't just use SPR_IBAT0U or
  // SPR_DBAT0U as low-index the entire way
  for (int i = 0; i < 4; i++)
  {
    // IBAT registers
    AddRegister(
        i, 5, RegisterType::ibat, "IBAT" + std::to_string(i),
        [this, i] {
          const auto& ppc_state = m_system.GetPPCState();
          return (static_cast<u64>(ppc_state.spr[SPR_IBAT0U + i * 2]) << 32) +
                 ppc_state.spr[SPR_IBAT0L + i * 2];
        },
        nullptr);
    AddRegister(
        i + 4, 5, RegisterType::ibat, "IBAT" + std::to_string(4 + i),
        [this, i] {
          const auto& ppc_state = m_system.GetPPCState();
          return (static_cast<u64>(ppc_state.spr[SPR_IBAT4U + i * 2]) << 32) +
                 ppc_state.spr[SPR_IBAT4L + i * 2];
        },
        nullptr);

    // DBAT registers
    AddRegister(
        i + 8, 5, RegisterType::dbat, "DBAT" + std::to_string(i),
        [this, i] {
          const auto& ppc_state = m_system.GetPPCState();
          return (static_cast<u64>(ppc_state.spr[SPR_DBAT0U + i * 2]) << 32) +
                 ppc_state.spr[SPR_DBAT0L + i * 2];
        },
        nullptr);
    AddRegister(
        i + 12, 5, RegisterType::dbat, "DBAT" + std::to_string(4 + i),
        [this, i] {
          const auto& ppc_state = m_system.GetPPCState();
          return (static_cast<u64>(ppc_state.spr[SPR_DBAT4U + i * 2]) << 32) +
                 ppc_state.spr[SPR_DBAT4L + i * 2];
        },
        nullptr);
  }

  for (int i = 0; i < 8; i++)
  {
    // Graphics quantization registers
    AddRegister(
        i + 16, 7, RegisterType::gqr, "GQR" + std::to_string(i),
        [this, i] { return m_system.GetPPCState().spr[SPR_GQR0 + i]; }, nullptr);
  }

  // HID registers
  AddRegister(
      24, 7, RegisterType::hid, "HID0", [this] { return m_system.GetPPCState().spr[SPR_HID0]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_HID0] = static_cast<u32>(value); });
  AddRegister(
      25, 7, RegisterType::hid, "HID1", [this] { return m_system.GetPPCState().spr[SPR_HID1]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_HID1] = static_cast<u32>(value); });
  AddRegister(
      26, 7, RegisterType::hid, "HID2", [this] { return m_system.GetPPCState().spr[SPR_HID2]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_HID2] = static_cast<u32>(value); });
  AddRegister(
      27, 7, RegisterType::hid, "HID4", [this] { return m_system.GetPPCState().spr[SPR_HID4]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_HID4] = static_cast<u32>(value); });

  for (int i = 0; i < 16; i++)
  {
    // SR registers
    AddRegister(
        i, 7, RegisterType::sr, "SR" + std::to_string(i),
        [this, i] { return m_system.GetPPCState().sr[i]; },
        [this, i](u64 value) { m_system.GetPPCState().sr[i] = value; });
  }

  // Special registers
  // TB
  AddRegister(
      16, 5, RegisterType::tb, "TB",
      [this] { return m_system.GetPowerPC().ReadFullTimeBaseValue(); }, nullptr);

  // PC
  AddRegister(
      17, 5, RegisterType::pc, "PC", [this] { return m_system.GetPPCState().pc; },
      [this](u64 value) { m_system.GetPPCState().pc = value; });

  // LR
  AddRegister(
      18, 5, RegisterType::lr, "LR", [this] { return m_system.GetPPCState().spr[SPR_LR]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_LR] = value; });

  // CTR
  AddRegister(
      19, 5, RegisterType::ctr, "CTR", [this] { return m_system.GetPPCState().spr[SPR_CTR]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_CTR] = value; });

  // CR
  AddRegister(
      20, 5, RegisterType::cr, "CR", [this] { return m_system.GetPPCState().cr.Get(); },
      [this](u64 value) { m_system.GetPPCState().cr.Set(value); });

  // XER
  AddRegister(
      21, 5, RegisterType::xer, "XER", [this] { return m_system.GetPPCState().GetXER().Hex; },
      [this](u64 value) { m_system.GetPPCState().SetXER(UReg_XER(value)); });

  // FPSCR
  AddRegister(
      22, 5, RegisterType::fpscr, "FPSCR", [this] { return m_system.GetPPCState().fpscr.Hex; },
      [this](u64 value) { m_system.GetPPCState().fpscr = static_cast<u32>(value); });

  // MSR
  AddRegister(
      23, 5, RegisterType::msr, "MSR", [this] { return m_system.GetPPCState().msr.Hex; },
      [this](u64 value) {
        m_system.GetPPCState().msr.Hex = value;
        PowerPC::MSRUpdated(m_system.GetPPCState());
      });

  // SRR 0-1
  AddRegister(
      24, 5, RegisterType::srr, "SRR0", [this] { return m_system.GetPPCState().spr[SPR_SRR0]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_SRR0] = value; });
  AddRegister(
      25, 5, RegisterType::srr, "SRR1", [this] { return m_system.GetPPCState().spr[SPR_SRR1]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_SRR1] = value; });

  // Exceptions
  AddRegister(
      26, 5, RegisterType::exceptions, "Exceptions",
      [this] { return m_system.GetPPCState().Exceptions; },
      [this](u64 value) { m_system.GetPPCState().Exceptions = value; });

  // Int Mask
  AddRegister(
      27, 5, RegisterType::int_mask, "Int Mask",
      [this] { return m_system.GetProcessorInterface().GetMask(); }, nullptr);

  // Int Cause
  AddRegister(
      28, 5, RegisterType::int_cause, "Int Cause",
      [this] { return m_system.GetProcessorInterface().GetCause(); }, nullptr);

  // DSISR
  AddRegister(
      29, 5, RegisterType::dsisr, "DSISR", [this] { return m_system.GetPPCState().spr[SPR_DSISR]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_DSISR] = value; });
  // DAR
  AddRegister(
      30, 5, RegisterType::dar, "DAR", [this] { return m_system.GetPPCState().spr[SPR_DAR]; },
      [this](u64 value) { m_system.GetPPCState().spr[SPR_DAR] = value; });

  // Hash Mask
  AddRegister(
      31, 5, RegisterType::pt_hashmask, "Hash Mask",
      [this] {
        const auto& ppc_state = m_system.GetPPCState();
        return (ppc_state.pagetable_hashmask << 6) | ppc_state.pagetable_base;
      },
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

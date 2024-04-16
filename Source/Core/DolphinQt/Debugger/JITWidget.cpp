// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/JITWidget.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <utility>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "Common/CommonFuncs.h"
#include "Common/GekkoDisassembler.h"
#include "Core/Core.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/System.h"

#include "DolphinQt/Debugger/JitBlockTableModel.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/ClickableStatusBar.h"
#include "DolphinQt/QtUtils/FromStdString.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"
#include "UICommon/UICommon.h"

class JitBlockProxyModel final : public QSortFilterProxyModel
{
  friend JITWidget;

public:
  explicit JitBlockProxyModel(QObject* parent = nullptr);
  ~JitBlockProxyModel() override;

  JitBlockProxyModel(const JitBlockProxyModel&) = delete;
  JitBlockProxyModel(JitBlockProxyModel&&) = delete;
  JitBlockProxyModel& operator=(const JitBlockProxyModel&) = delete;
  JitBlockProxyModel& operator=(JitBlockProxyModel&&) = delete;

  bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
  [[noreturn]] void setSourceModel(QAbstractItemModel* source_model) override;
  void setSourceModel(JitBlockTableModel* source_model);
  JitBlockTableModel* sourceModel() const;

  const JitBlock& GetJitBlock(const QModelIndex& index);

  // Always connected slots (external signals)
  void OnSymbolTextChanged(const QString& text);
  template <std::optional<u32> JitBlockProxyModel::*member>
  void OnAddressTextChanged(const QString& text);

private:
  std::optional<u32> m_em_address_min, m_em_address_max, m_pm_address_covered;
  QString m_symbol_name = {};
};

const JitBlock& JitBlockProxyModel::GetJitBlock(const QModelIndex& index)
{
  return sourceModel()->GetJitBlock(mapToSource(index));
}

void JitBlockProxyModel::OnSymbolTextChanged(const QString& text)
{
  m_symbol_name = text;
  invalidateRowsFilter();
}

template <std::optional<u32> JitBlockProxyModel::*member>
void JitBlockProxyModel::OnAddressTextChanged(const QString& text)
{
  bool ok = false;
  if (const u32 value = text.toUInt(&ok, 16); ok)
    this->*member = value;
  else
    this->*member = std::nullopt;
  invalidateRowsFilter();
}

bool JitBlockProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  if (source_parent.isValid()) [[unlikely]]
    return false;
  if (!m_symbol_name.isEmpty())
  {
    if (const QVariant& symbol_name_v = *sourceModel()->GetSymbolList()[source_row];
        !symbol_name_v.isValid() || !static_cast<const QString*>(symbol_name_v.data())
                                         ->contains(m_symbol_name, Qt::CaseInsensitive))
    {
      return false;
    }
  }
  const JitBlock& block = sourceModel()->GetJitBlockRefs()[source_row];
  if (m_em_address_min.has_value())
  {
    if (block.effectiveAddress < m_em_address_min.value())
      return false;
  }
  if (m_em_address_max.has_value())
  {
    if (block.effectiveAddress > m_em_address_max.value())
      return false;
  }
  if (m_pm_address_covered.has_value())
  {
    if (!block.physical_addresses.contains(m_pm_address_covered.value()))
      return false;
  }
  return true;
}

// Virtual setSourceModel is forbidden for type-safety reasons.
void JitBlockProxyModel::setSourceModel(QAbstractItemModel* source_model)
{
  Crash();
}

void JitBlockProxyModel::setSourceModel(JitBlockTableModel* source_model)
{
  QSortFilterProxyModel::setSourceModel(source_model);
}

JitBlockTableModel* JitBlockProxyModel::sourceModel() const
{
  return static_cast<JitBlockTableModel*>(QSortFilterProxyModel::sourceModel());
}

JitBlockProxyModel::JitBlockProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
}

JitBlockProxyModel::~JitBlockProxyModel() = default;

void JITWidget::UpdateProfilingButton()
{
  const QSignalBlocker blocker(m_toggle_profiling_button);
  const bool enabled = Config::Get(Config::MAIN_DEBUG_JIT_ENABLE_PROFILING);
  m_toggle_profiling_button->setText(enabled ? tr("Stop Profiling") : tr("Start Profiling"));
  m_toggle_profiling_button->setChecked(enabled);
}

void JITWidget::UpdateOtherButtons(Core::State state)
{
  const bool jit_exists = m_system.GetJitInterface().GetCore() != nullptr;
  m_clear_cache_button->setEnabled(jit_exists);
  m_wipe_profiling_button->setEnabled(jit_exists);
}

void JITWidget::UpdateDebugFont(const QFont& font)
{
  m_table_view->setFont(font);
  m_ppc_asm_widget->setFont(font);
  m_host_near_asm_widget->setFont(font);
  m_host_far_asm_widget->setFont(font);
}

void JITWidget::ClearDisassembly()
{
  m_ppc_asm_widget->clear();
  m_host_near_asm_widget->clear();
  m_host_far_asm_widget->clear();
  m_status_bar->clearMessage();
}

void JITWidget::ShowFreeMemoryStatus()
{
  const std::vector memory_stats = m_system.GetJitInterface().GetMemoryStats();
  QString message = tr("Free memory:");
  for (const auto& [name, stats] : memory_stats)
  {
    const auto& [free_size, fragmentation_ratio] = stats;
    // i18n: Of each memory region, %1 is its remaining size displayed in an appropriate scale
    // of bytes (e.g. MiB), %2 is its untranslated name, and %3 is its fragmentation percentage.
    message.append(tr(" %1 %2 (%3% fragmented)")
                       .arg(QString::fromStdString(UICommon::FormatSize(free_size, 2)))
                       .arg(QtUtils::FromStdString(name))
                       .arg(fragmentation_ratio * 100.0, 0, 'f', 2));
  }
  m_status_bar->showMessage(message);
}

void JITWidget::UpdateContent(Core::State state)
{
  ClearDisassembly();
  if (state == Core::State::Paused)
    ShowFreeMemoryStatus();
}

static void DisassembleCodeBuffer(const JitBlock& block, PPCSymbolDB& ppc_symbol_db,
                                  std::ostream& stream)
{
  // Instructions are 4 byte aligned, so next_address = 1 will never produce a false-negative.
  for (u32 next_address = 1; const auto& [address, inst] : block.original_buffer)
  {
    if (address != next_address)
    {
      stream << ppc_symbol_db.GetDescription(address) << '\n';
      next_address = address;
    }
    fmt::print(stream, "0x{:08x}\t{}\n", address,
               Common::GekkoDisassembler::Disassemble(inst.hex, address));
    next_address += sizeof(UGeckoInstruction);
  }
}

void JITWidget::CrossDisassemble(const JitBlock& block)
{
  // TODO C++20: std::ostringstream::view() + QtUtils::FromStdString + std::ostream::seekp(0) would
  // save a lot of wasted allocation here, but compiler support for the first thing isn't here yet.
  std::ostringstream stream;
  DisassembleCodeBuffer(block, m_system.GetPPCSymbolDB(), stream);
  m_ppc_asm_widget->setPlainText(QString::fromStdString(std::move(stream).str()));

  auto& jit_interface = m_system.GetJitInterface();

  const auto host_near_instruction_count = jit_interface.DisassembleNearCode(block, stream);
  m_host_near_asm_widget->setPlainText(QString::fromStdString(std::move(stream).str()));

  const auto host_far_instruction_count = jit_interface.DisassembleFarCode(block, stream);
  m_host_far_asm_widget->setPlainText(QString::fromStdString(std::move(stream).str()));

  // i18n: "near" and "far" refer to the near code cache and far code cache of Dolphin's JITs.
  // %1 and %2 are instruction counts from the near and far code caches, respectively. %3 is a
  // percentage calculated from how inefficient (in other words, "blown-up") a given JIT block's
  // recompilation was when considering the host instruction count vs the PPC instruction count.
  m_status_bar->showMessage(tr("Host instruction count: %1 near %2 far (%3% blowup)")
                                .arg(host_near_instruction_count)
                                .arg(host_far_instruction_count)
                                .arg(static_cast<double>(100 * (host_near_instruction_count +
                                                                host_far_instruction_count)) /
                                             block.originalSize -
                                         100.0,
                                     0, 'f', 2));
}

void JITWidget::CrossDisassemble(const QModelIndex& index)
{
  if (index.isValid())
  {
    CrossDisassemble(m_table_proxy->GetJitBlock(index));
    return;
  }
  UpdateContent(Core::GetState(m_system));
}

void JITWidget::CrossDisassemble()
{
  CrossDisassemble(m_table_view->currentIndex());
}

void JITWidget::TableEraseBlocks()
{
  auto* const selection_model = m_table_view->selectionModel();
  QModelIndexList index_list = selection_model->selectedRows();
  selection_model->clear();  // Side effect: currentChanged will be emitted (this is intended).

  std::ranges::transform(index_list, index_list.begin(), [this](const QModelIndex& index) {
    return m_table_proxy->mapToSource(index);
  });
  std::ranges::sort(index_list, std::less{});  // QModelIndex is incompatible with std::ranges::less
  for (const QModelIndex& index : std::ranges::reverse_view{index_list})
  {
    if (!index.isValid())
      continue;
    m_table_model->removeRow(index.row());
  }
}

void JITWidget::LoadQSettings()
{
  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("jitwidget/geometry")).toByteArray());
  setHidden(!Settings::Instance().IsJITVisible() || !Settings::Instance().IsDebugModeEnabled());
  // macOS: setFloating() needs to be after setHidden() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("jitwidget/floating")).toBool());
  m_table_view->horizontalHeader()->restoreState(
      settings.value(QStringLiteral("jitwidget/tableheader/state")).toByteArray());
  m_table_splitter->restoreState(
      settings.value(QStringLiteral("jitwidget/tablesplitter")).toByteArray());
  m_disasm_splitter->restoreState(
      settings.value(QStringLiteral("jitwidget/disasmsplitter")).toByteArray());
}

void JITWidget::SaveQSettings() const
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("jitwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("jitwidget/floating"), isFloating());
  settings.setValue(QStringLiteral("jitwidget/tableheader/state"),
                    m_table_view->horizontalHeader()->saveState());
  settings.setValue(QStringLiteral("jitwidget/tablesplitter"), m_table_splitter->saveState());
  settings.setValue(QStringLiteral("jitwidget/disasmsplitter"), m_disasm_splitter->saveState());
}

void JITWidget::ConnectSlots()
{
  auto* const host = Host::GetInstance();
  connect(host, &Host::JitCacheCleared, this, &JITWidget::OnJitCacheCleared);
  connect(host, &Host::UpdateDisasmDialog, this, &JITWidget::OnUpdateDisasmDialog);
  connect(host, &Host::PPCSymbolsChanged, this, &JITWidget::OnPPCSymbolsUpdated);
  connect(host, &Host::PPCBreakpointsChanged, this, &JITWidget::OnPPCBreakpointsChanged);
  auto* const settings = &Settings::Instance();
  connect(settings, &Settings::ConfigChanged, this, &JITWidget::OnConfigChanged);
  connect(settings, &Settings::DebugFontChanged, this, &JITWidget::OnDebugFontChanged);
  connect(settings, &Settings::EmulationStateChanged, this, &JITWidget::OnEmulationStateChanged);
}

void JITWidget::DisconnectSlots()
{
  auto* const host = Host::GetInstance();
  disconnect(host, &Host::JitCacheCleared, this, &JITWidget::OnJitCacheCleared);
  disconnect(host, &Host::UpdateDisasmDialog, this, &JITWidget::OnUpdateDisasmDialog);
  disconnect(host, &Host::PPCSymbolsChanged, this, &JITWidget::OnPPCSymbolsUpdated);
  disconnect(host, &Host::PPCBreakpointsChanged, this, &JITWidget::OnPPCBreakpointsChanged);
  auto* const settings = &Settings::Instance();
  disconnect(settings, &Settings::ConfigChanged, this, &JITWidget::OnConfigChanged);
  disconnect(settings, &Settings::DebugFontChanged, this, &JITWidget::OnDebugFontChanged);
  disconnect(settings, &Settings::EmulationStateChanged, this, &JITWidget::OnEmulationStateChanged);
}

void JITWidget::Show()
{
  ConnectSlots();
  // Handle every slot that may have missed a signal while this widget was hidden.
  // OnJitCacheCleared() can be skipped.
  // OnUpdateDisasmDialog() can be skipped.
  // OnPPCSymbolsUpdated() can be skipped.
  // OnPPCBreakpointsChanged() can be skipped.
  OnConfigChanged();
  OnDebugFontChanged(Settings::Instance().GetDebugFont());
  OnEmulationStateChanged(Core::GetState(m_system));
}

void JITWidget::Hide()
{
  DisconnectSlots();
  ClearDisassembly();
}

void JITWidget::OnRequestPPCComparison(u32 address, bool translate_address)
{
  Settings::Instance().SetJITVisible(true);
  raise();

  if (translate_address)
  {
    const std::optional<u32> pm_address = m_system.GetMMU().GetTranslatedAddress(address);
    if (!pm_address.has_value())
    {
      ModalMessageBox::warning(
          this, tr("Error"),
          tr("Effective address %1 has no physical address translation.").arg(address, 0, 16));
      return;
    }
    address = pm_address.value();
  }
  m_pm_address_covered_line_edit->setText(QString::number(address, 16));
}

void JITWidget::OnVisibilityToggled(bool visible)
{
  setHidden(!visible);
}

void JITWidget::OnDebugModeToggled(bool enabled)
{
  setHidden(!enabled || !Settings::Instance().IsJITVisible());
}

void JITWidget::OnToggleProfiling(bool enabled)
{
  Config::SetBaseOrCurrent(Config::MAIN_DEBUG_JIT_ENABLE_PROFILING, enabled);
}

void JITWidget::OnClearCache()
{
  m_system.GetJitInterface().ClearCache(Core::CPUThreadGuard{m_system});
}

void JITWidget::OnWipeProfiling()
{
  m_system.GetJitInterface().WipeBlockProfilingData(Core::CPUThreadGuard{m_system});
}

void JITWidget::OnTableCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
  CrossDisassemble(current);
}

void JITWidget::OnTableDoubleClicked(const QModelIndex& index)
{
  emit SetCodeAddress(m_table_proxy->GetJitBlock(index).effectiveAddress);
}

void JITWidget::OnTableContextMenu(const QPoint& pos)
{
  // There needs to be an option somewhere for a user to recover from hiding every column.
  if (m_table_view->horizontalHeader()->hiddenSectionCount() == Column::NumberOfColumns)
  {
    m_column_visibility_menu->exec(m_table_view->viewport()->mapToGlobal(pos));
    return;
  }
  m_table_context_menu->exec(m_table_view->viewport()->mapToGlobal(pos));
}

void JITWidget::OnTableHeaderContextMenu(const QPoint& pos)
{
  m_column_visibility_menu->exec(m_table_view->horizontalHeader()->mapToGlobal(pos));
}

void JITWidget::OnTableMenuViewCode()
{
  // TODO: CodeWidget doesn't support it yet, but eventually signal if the address should be
  // translated with ((block.feature_flags & CPUEmuFeatureFlags::FEATURE_FLAG_MSR_IR) != 0).
  if (const QModelIndex& index = m_table_view->currentIndex(); index.isValid())
    emit SetCodeAddress(m_table_proxy->GetJitBlock(index).effectiveAddress);
}

void JITWidget::OnTableMenuEraseBlocks()
{
  TableEraseBlocks();  // Side effect: currentChanged will be emitted (this is intended).
  // Because currentChanged has been emitted, OnTableCurrentChanged has already handled the rest.
}

void JITWidget::OnStatusBarPressed()
{
  if (Core::GetState(m_system) == Core::State::Paused)
    ShowFreeMemoryStatus();
}

void JITWidget::OnJitCacheCleared()
{
  if (Core::GetState(m_system) != Core::State::Paused)
    return;
  ClearDisassembly();
  ShowFreeMemoryStatus();
}

void JITWidget::OnUpdateDisasmDialog()
{
  if (Core::GetState(m_system) != Core::State::Paused)
    return;
  CrossDisassemble();
}

void JITWidget::OnPPCSymbolsUpdated()
{
  if (Core::GetState(m_system) != Core::State::Paused)
    return;
  CrossDisassemble();
}

void JITWidget::OnPPCBreakpointsChanged()
{
  // Whatever row(s) might have been selected could no longer exist, because adding or removing
  // breakpoints can invalidate JIT blocks. We must clear the selection to avoid stale indices.
  auto* const selection_model = m_table_view->selectionModel();
  selection_model->clear();  // Side effect: currentChanged will be emitted (this is intended).
  // Because currentChanged has been emitted, OnTableCurrentChanged has already handled the rest.
}

void JITWidget::OnConfigChanged()
{
  UpdateProfilingButton();
}

void JITWidget::OnDebugFontChanged(const QFont& font)
{
  UpdateDebugFont(font);
}

void JITWidget::OnEmulationStateChanged(Core::State state)
{
  UpdateOtherButtons(state);
  UpdateContent(state);
}

void JITWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetJITVisible(false);
}

void JITWidget::showEvent(QShowEvent*)
{
  emit ShowSignal();
  Show();
}

void JITWidget::hideEvent(QHideEvent*)
{
  emit HideSignal();
  Hide();
}

JITWidget::JITWidget(Core::System& system, QWidget* parent) : QDockWidget(parent), m_system(system)
{
  setWindowTitle(tr("JIT Blocks"));
  setObjectName(QStringLiteral("jitwidget"));
  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto* const settings = &Settings::Instance();
  connect(settings, &Settings::JITVisibilityChanged, this, &JITWidget::OnVisibilityToggled);
  connect(settings, &Settings::DebugModeToggled, this, &JITWidget::OnDebugModeToggled);

  m_table_view = new QTableView(nullptr);
  m_table_proxy = new JitBlockProxyModel(m_table_view);
  m_table_model = new JitBlockTableModel(m_system, m_system.GetJitInterface(),
                                         m_system.GetPPCSymbolDB(), m_table_proxy);

  connect(this, &JITWidget::HideSignal, m_table_model, &JitBlockTableModel::OnHideSignal);
  connect(this, &JITWidget::ShowSignal, m_table_model, &JitBlockTableModel::OnShowSignal);

  m_table_proxy->setSourceModel(m_table_model);
  m_table_proxy->setSortRole(UserRole::SortRole);
  m_table_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

  m_table_view->setModel(m_table_proxy);
  m_table_view->setSortingEnabled(true);
  m_table_view->sortByColumn(Column::EffectiveAddress, Qt::AscendingOrder);
  m_table_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_table_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table_view->setCornerButtonEnabled(false);
  m_table_view->verticalHeader()->hide();
  connect(m_table_view, &QTableView::doubleClicked, this, &JITWidget::OnTableDoubleClicked);
  connect(m_table_view, &QTableView::customContextMenuRequested, this,
          &JITWidget::OnTableContextMenu);

  auto* const horizontal_header = m_table_view->horizontalHeader();
  horizontal_header->setContextMenuPolicy(Qt::CustomContextMenu);
  horizontal_header->setStretchLastSection(true);
  horizontal_header->setSectionsMovable(true);
  horizontal_header->setFirstSectionMovable(true);
  connect(horizontal_header, &QHeaderView::sortIndicatorChanged, m_table_model,
          &JitBlockTableModel::OnSortIndicatorChanged);
  connect(horizontal_header, &QHeaderView::customContextMenuRequested, this,
          &JITWidget::OnTableHeaderContextMenu);

  auto* const selection_model = m_table_view->selectionModel();
  connect(selection_model, &QItemSelectionModel::currentChanged, this,
          &JITWidget::OnTableCurrentChanged);

  auto* const controls_layout = new QHBoxLayout(nullptr);
  const auto address_filter_routine = [&](QLineEdit* line_edit, const QString& placeholder_text,
                                          void (JitBlockProxyModel::*slot)(const QString&)) {
    line_edit->setPlaceholderText(placeholder_text);
    connect(line_edit, &QLineEdit::textChanged, m_table_proxy, slot);
    controls_layout->addWidget(line_edit);
  };
  address_filter_routine(
      new QLineEdit(nullptr), tr("Min Effective Address"),
      &JitBlockProxyModel::OnAddressTextChanged<&JitBlockProxyModel::m_em_address_min>);
  address_filter_routine(
      new QLineEdit(nullptr), tr("Max Effective Address"),
      &JitBlockProxyModel::OnAddressTextChanged<&JitBlockProxyModel::m_em_address_max>);
  address_filter_routine(
      m_pm_address_covered_line_edit = new QLineEdit(nullptr), tr("Recompiles Physical Address"),
      &JitBlockProxyModel::OnAddressTextChanged<&JitBlockProxyModel::m_pm_address_covered>);

  auto* const symbol_name_line_edit = new QLineEdit(nullptr);
  symbol_name_line_edit->setPlaceholderText(tr("Symbol Name"));
  connect(symbol_name_line_edit, &QLineEdit::textChanged, m_table_model,
          &JitBlockTableModel::OnFilterSymbolTextChanged);
  connect(symbol_name_line_edit, &QLineEdit::textChanged, m_table_proxy,
          &JitBlockProxyModel::OnSymbolTextChanged);
  controls_layout->addWidget(symbol_name_line_edit);

  m_toggle_profiling_button = new QPushButton(nullptr);
  m_toggle_profiling_button->setToolTip(
      tr("Toggle software JIT block profiling (will clear the JIT cache)."));
  m_toggle_profiling_button->setCheckable(true);
  connect(m_toggle_profiling_button, &QPushButton::toggled, this, &JITWidget::OnToggleProfiling);
  controls_layout->addWidget(m_toggle_profiling_button);

  m_clear_cache_button = new QPushButton(tr("Clear Cache"), nullptr);
  connect(m_clear_cache_button, &QPushButton::clicked, this, &JITWidget::OnClearCache);
  controls_layout->addWidget(m_clear_cache_button);

  m_wipe_profiling_button = new QPushButton(tr("Wipe Profiling"), nullptr);
  m_wipe_profiling_button->setToolTip(tr("Re-initialize software JIT block profiling data."));
  connect(m_wipe_profiling_button, &QPushButton::clicked, this, &JITWidget::OnWipeProfiling);
  controls_layout->addWidget(m_wipe_profiling_button);

  m_disasm_splitter = new QSplitter(Qt::Horizontal, nullptr);
  const auto text_box_routine = [&](QPlainTextEdit* text_edit, const QString& placeholder_text) {
    text_edit->setWordWrapMode(QTextOption::NoWrap);
    text_edit->setPlaceholderText(placeholder_text);
    text_edit->setReadOnly(true);
    m_disasm_splitter->addWidget(text_edit);
  };
  text_box_routine(m_ppc_asm_widget = new QPlainTextEdit(nullptr), tr("PPC Instruction Coverage"));
  text_box_routine(m_host_near_asm_widget = new QPlainTextEdit(nullptr),
                   tr("Host Near Code Cache"));
  text_box_routine(m_host_far_asm_widget = new QPlainTextEdit(nullptr), tr("Host Far Code Cache"));

  m_table_splitter = new QSplitter(Qt::Vertical, nullptr);
  m_table_splitter->addWidget(m_table_view);
  m_table_splitter->addWidget(m_disasm_splitter);

  m_status_bar = new ClickableStatusBar(nullptr);
  m_status_bar->setSizeGripEnabled(false);
  connect(m_status_bar, &ClickableStatusBar::pressed, this, &JITWidget::OnStatusBarPressed);

  m_table_context_menu = new QMenu(this);
  m_table_context_menu->addAction(tr("View &Code"), this, &JITWidget::OnTableMenuViewCode);
  m_table_context_menu->addAction(tr("&Erase Block(s)"), this, &JITWidget::OnTableMenuEraseBlocks);

  LoadQSettings();

  m_column_visibility_menu = new QMenu(this);
  // These table header display names have abbreviated counterparts in JitBlockTableModel.cpp
  static constexpr std::array<const char*, Column::NumberOfColumns> headers = {
      QT_TR_NOOP("PPC Feature Flags"),
      // i18n: "Effective" means this memory address might be translated within the MMU.
      QT_TR_NOOP("Effective Address"),
      QT_TR_NOOP("Code Buffer Size"),
      // i18n: This means to say it is a count of PPC instructions recompiled more than once.
      QT_TR_NOOP("Repeat Instructions"),
      // i18n: "Near Code" refers to the near code cache of Dolphin's JITs.
      QT_TR_NOOP("Host Near Code Size"),
      // i18n: "Far Code" refers to the far code cache of Dolphin's JITs.
      QT_TR_NOOP("Host Far Code Size"),
      QT_TR_NOOP("Run Count"),
      // i18n: "Cycles" means instruction cycles.
      QT_TR_NOOP("Cycles Spent"),
      // i18n: "Cycles" means instruction cycles.
      QT_TR_NOOP("Cycles Average"),
      // i18n: "Cycles" means instruction cycles.
      QT_TR_NOOP("Cycles Percent"),
      // i18n: "ns" is an abbreviation of nanoseconds.
      QT_TR_NOOP("Time Spent (ns)"),
      // i18n: "ns" is an abbreviation of nanoseconds.
      QT_TR_NOOP("Time Average (ns)"),
      QT_TR_NOOP("Time Percent"),
      // i18n: "Symbol" means debugging symbol (its name in particular).
      QT_TR_NOOP("Symbol"),
  };
  for (int column = 0; column < Column::NumberOfColumns; ++column)
  {
    auto* const action =
        m_column_visibility_menu->addAction(tr(headers[column]), [this, column](bool enabled) {
          m_table_view->setColumnHidden(column, !enabled);
        });
    action->setChecked(!m_table_view->isColumnHidden(column));
    action->setCheckable(true);
  }

  auto* const main_layout = new QVBoxLayout(nullptr);
  main_layout->setContentsMargins(2, 2, 2, 2);
  main_layout->setSpacing(0);
  main_layout->addLayout(controls_layout);
  main_layout->addWidget(m_table_splitter);
  main_layout->addWidget(m_status_bar);

  auto* const main_widget = new QWidget(nullptr);
  main_widget->setLayout(main_layout);
  setWidget(main_widget);
}

JITWidget::~JITWidget()
{
  SaveQSettings();
}

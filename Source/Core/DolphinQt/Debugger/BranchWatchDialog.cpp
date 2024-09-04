// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BranchWatchDialog.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QString>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariant>
#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Unreachable.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "DolphinQt/Debugger/BranchWatchTableModel.h"
#include "DolphinQt/Debugger/CodeWidget.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

class BranchWatchProxyModel final : public QSortFilterProxyModel
{
  friend BranchWatchDialog;

public:
  explicit BranchWatchProxyModel(const Core::BranchWatch& branch_watch, QObject* parent = nullptr)
      : QSortFilterProxyModel(parent), m_branch_watch(branch_watch)
  {
  }
  ~BranchWatchProxyModel() override = default;

  BranchWatchProxyModel(const BranchWatchProxyModel&) = delete;
  BranchWatchProxyModel(BranchWatchProxyModel&&) = delete;
  BranchWatchProxyModel& operator=(const BranchWatchProxyModel&) = delete;
  BranchWatchProxyModel& operator=(BranchWatchProxyModel&&) = delete;

  BranchWatchTableModel* sourceModel() const
  {
    return static_cast<BranchWatchTableModel*>(QSortFilterProxyModel::sourceModel());
  }
  void setSourceModel(BranchWatchTableModel* source_model)
  {
    QSortFilterProxyModel::setSourceModel(source_model);
  }

  // Virtual setSourceModel is forbidden for type-safety reasons. See sourceModel().
  [[noreturn]] void setSourceModel(QAbstractItemModel* source_model) override { Crash(); }
  bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  template <bool BranchWatchProxyModel::*member>
  void OnToggled(bool enabled)
  {
    this->*member = enabled;
    invalidateRowsFilter();
  }
  template <QString BranchWatchProxyModel::*member>
  void OnSymbolTextChanged(const QString& text)
  {
    this->*member = text;
    invalidateRowsFilter();
  }
  template <std::optional<u32> BranchWatchProxyModel::*member>
  void OnAddressTextChanged(const QString& text)
  {
    bool ok = false;
    if (const u32 value = text.toUInt(&ok, 16); ok)
      this->*member = value;
    else
      this->*member = std::nullopt;
    invalidateRowsFilter();
  }

  bool IsBranchTypeAllowed(UGeckoInstruction inst) const;
  void SetInspected(const QModelIndex& index) const;
  const Core::BranchWatchSelectionValueType&
  GetBranchWatchSelection(const QModelIndex& index) const;

private:
  const Core::BranchWatch& m_branch_watch;

  QString m_origin_symbol_name = {}, m_destin_symbol_name = {};
  std::optional<u32> m_origin_min, m_origin_max, m_destin_min, m_destin_max;
  bool m_b = {}, m_bl = {}, m_bc = {}, m_bcl = {}, m_blr = {}, m_blrl = {}, m_bclr = {},
       m_bclrl = {}, m_bctr = {}, m_bctrl = {}, m_bcctr = {}, m_bcctrl = {};
  bool m_cond_true = {}, m_cond_false = {};
};

bool BranchWatchProxyModel::filterAcceptsRow(int source_row, const QModelIndex&) const
{
  const Core::BranchWatch::Selection::value_type& value = m_branch_watch.GetSelection()[source_row];
  if (value.condition)
  {
    if (!m_cond_true)
      return false;
  }
  else if (!m_cond_false)
    return false;

  const Core::BranchWatchCollectionKey& k = value.collection_ptr->first;
  if (!IsBranchTypeAllowed(k.original_inst))
    return false;

  if (m_origin_min.has_value() && k.origin_addr < m_origin_min.value())
    return false;
  if (m_origin_max.has_value() && k.origin_addr > m_origin_max.value())
    return false;
  if (m_destin_min.has_value() && k.destin_addr < m_destin_min.value())
    return false;
  if (m_destin_max.has_value() && k.destin_addr > m_destin_max.value())
    return false;

  if (!m_origin_symbol_name.isEmpty())
  {
    if (const QVariant& symbol_name_v = sourceModel()->GetSymbolList()[source_row].origin_name;
        !symbol_name_v.isValid() || !static_cast<const QString*>(symbol_name_v.data())
                                         ->contains(m_origin_symbol_name, Qt::CaseInsensitive))
      return false;
  }
  if (!m_destin_symbol_name.isEmpty())
  {
    if (const QVariant& symbol_name_v = sourceModel()->GetSymbolList()[source_row].destin_name;
        !symbol_name_v.isValid() || !static_cast<const QString*>(symbol_name_v.data())
                                         ->contains(m_destin_symbol_name, Qt::CaseInsensitive))
      return false;
  }
  return true;
}

bool BranchWatchProxyModel::IsBranchTypeAllowed(UGeckoInstruction inst) const
{
  switch (inst.OPCD)
  {
  case 18:
    return inst.LK ? m_bl : m_b;
  case 16:
    return inst.LK ? m_bcl : m_bc;
  case 19:
    switch (inst.SUBOP10)
    {
    case 16:
      if ((inst.BO & 0b10100) == 0b10100)  // 1z1zz - Branch always
        return inst.LK ? m_blrl : m_blr;
      return inst.LK ? m_bclrl : m_bclr;
    case 528:
      if ((inst.BO & 0b10100) == 0b10100)  // 1z1zz - Branch always
        return inst.LK ? m_bctrl : m_bctr;
      return inst.LK ? m_bcctrl : m_bcctr;
    }
  }
  return false;
}

void BranchWatchProxyModel::SetInspected(const QModelIndex& index) const
{
  sourceModel()->SetInspected(mapToSource(index));
}

const Core::BranchWatchSelectionValueType&
BranchWatchProxyModel::GetBranchWatchSelection(const QModelIndex& index) const
{
  return sourceModel()->GetBranchWatchSelection(mapToSource(index));
}

BranchWatchDialog::BranchWatchDialog(Core::System& system, Core::BranchWatch& branch_watch,
                                     PPCSymbolDB& ppc_symbol_db, CodeWidget* code_widget,
                                     QWidget* parent)
    : QDialog(parent), m_system(system), m_branch_watch(branch_watch), m_code_widget(code_widget)
{
  setWindowTitle(tr("Branch Watch Tool"));
  setWindowFlags((windowFlags() | Qt::WindowMinMaxButtonsHint) & ~Qt::WindowContextHelpButtonHint);

  // Branch Watch Table
  m_table_view = new QTableView(nullptr);
  m_table_proxy = new BranchWatchProxyModel(m_branch_watch, m_table_view);
  m_table_model = new BranchWatchTableModel(m_system, m_branch_watch, ppc_symbol_db, m_table_proxy);

  m_table_proxy->setSourceModel(m_table_model);
  m_table_proxy->setSortRole(UserRole::SortRole);
  m_table_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

  m_table_view->setModel(m_table_proxy);
  m_table_view->setSortingEnabled(true);
  m_table_view->sortByColumn(Column::Origin, Qt::AscendingOrder);
  m_table_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_table_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table_view->setCornerButtonEnabled(false);
  m_table_view->verticalHeader()->hide();
  m_table_view->setColumnWidth(Column::Instruction, 50);
  m_table_view->setColumnWidth(Column::Condition, 50);
  m_table_view->setColumnWidth(Column::OriginSymbol, 250);
  m_table_view->setColumnWidth(Column::DestinSymbol, 250);
  // The default column width (100 units) is fine for the rest.

  auto* const horizontal_header = m_table_view->horizontalHeader();
  horizontal_header->setContextMenuPolicy(Qt::CustomContextMenu);
  horizontal_header->setStretchLastSection(true);
  horizontal_header->setSectionsMovable(true);
  horizontal_header->setFirstSectionMovable(true);

  connect(m_table_view, &QTableView::clicked, this, &BranchWatchDialog::OnTableClicked);
  connect(m_table_view, &QTableView::customContextMenuRequested, this,
          &BranchWatchDialog::OnTableContextMenu);
  connect(horizontal_header, &QHeaderView::customContextMenuRequested, this,
          &BranchWatchDialog::OnTableHeaderContextMenu);
  connect(new QShortcut(QKeySequence(Qt::Key_Delete), this), &QShortcut::activated, this,
          &BranchWatchDialog::OnTableDeleteKeypress);

  // Status Bar
  m_status_bar = new QStatusBar(nullptr);
  m_status_bar->setSizeGripEnabled(false);

  // Controls Toolbar
  m_control_toolbar = new QToolBar(nullptr);
  {
    // Tool Controls
    m_btn_start_pause = new QPushButton(tr("Start Branch Watch"), nullptr);
    connect(m_btn_start_pause, &QPushButton::toggled, this, &BranchWatchDialog::OnStartPause);
    m_btn_start_pause->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_btn_start_pause->setCheckable(true);

    m_btn_clear_watch = new QPushButton(tr("Clear Branch Watch"), nullptr);
    connect(m_btn_clear_watch, &QPushButton::clicked, this, &BranchWatchDialog::OnClearBranchWatch);
    m_btn_clear_watch->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_btn_path_was_taken = new QPushButton(tr("Code Path Was Taken"), nullptr);
    connect(m_btn_path_was_taken, &QPushButton::clicked, this,
            &BranchWatchDialog::OnCodePathWasTaken);
    m_btn_path_was_taken->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_btn_path_not_taken = new QPushButton(tr("Code Path Not Taken"), nullptr);
    connect(m_btn_path_not_taken, &QPushButton::clicked, this,
            &BranchWatchDialog::OnCodePathNotTaken);
    m_btn_path_not_taken->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* const layout = new QGridLayout(nullptr);
    layout->addWidget(m_btn_start_pause, 0, 0);
    layout->addWidget(m_btn_clear_watch, 1, 0);
    layout->addWidget(m_btn_path_was_taken, 0, 1);
    layout->addWidget(m_btn_path_not_taken, 1, 1);

    auto* const group_box = new QGroupBox(tr("Tool Controls"), nullptr);
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    m_control_toolbar->addWidget(group_box);
  }
  {
    // Spacer
    auto* const widget = new QWidget(nullptr);
    widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_control_toolbar->addWidget(widget);
  }
  {
    // Branch Type Filter Options
    auto* const layout = new QGridLayout(nullptr);

    const auto routine = [this, layout](const QString& text, const QString& tooltip, int row,
                                        int column, void (BranchWatchProxyModel::*slot)(bool)) {
      auto* const check_box = new QCheckBox(text, nullptr);
      check_box->setToolTip(tooltip);
      layout->addWidget(check_box, row, column);
      connect(check_box, &QCheckBox::toggled, [this, slot](bool checked) {
        (m_table_proxy->*slot)(checked);
        UpdateStatus();
      });
      check_box->setChecked(true);
    };

    // clang-format off
    routine(QStringLiteral("b"     ), tr("Branch"                                         ), 0, 0, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_b     >);
    routine(QStringLiteral("bl"    ), tr("Branch (LR saved)"                              ), 0, 1, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bl    >);
    routine(QStringLiteral("bc"    ), tr("Branch Conditional"                             ), 0, 2, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bc    >);
    routine(QStringLiteral("bcl"   ), tr("Branch Conditional (LR saved)"                  ), 0, 3, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bcl   >);
    routine(QStringLiteral("blr"   ), tr("Branch to Link Register"                        ), 1, 0, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_blr   >);
    routine(QStringLiteral("blrl"  ), tr("Branch to Link Register (LR saved)"             ), 1, 1, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_blrl  >);
    routine(QStringLiteral("bclr"  ), tr("Branch Conditional to Link Register"            ), 1, 2, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bclr  >);
    routine(QStringLiteral("bclrl" ), tr("Branch Conditional to Link Register (LR saved)" ), 1, 3, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bclrl >);
    routine(QStringLiteral("bctr"  ), tr("Branch to Count Register"                       ), 2, 0, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bctr  >);
    routine(QStringLiteral("bctrl" ), tr("Branch to Count Register (LR saved)"            ), 2, 1, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bctrl >);
    routine(QStringLiteral("bcctr" ), tr("Branch Conditional to Count Register"           ), 2, 2, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bcctr >);
    routine(QStringLiteral("bcctrl"), tr("Branch Conditional to Count Register (LR saved)"), 2, 3, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bcctrl>);
    // clang-format on

    auto* const group_box = new QGroupBox(tr("Branch Type"), nullptr);
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    m_act_branch_type_filters = m_control_toolbar->addWidget(group_box);
  }
  {
    // Origin and Destination Filter Options
    auto* const layout = new QGridLayout(nullptr);

    const auto routine = [this, layout](const QString& placeholder_text, int row, int column,
                                        int width,
                                        void (BranchWatchProxyModel::*slot)(const QString&)) {
      auto* const line_edit = new QLineEdit(nullptr);
      layout->addWidget(line_edit, row, column, 1, width);
      connect(line_edit, &QLineEdit::textChanged, [this, slot](const QString& text) {
        (m_table_proxy->*slot)(text);
        UpdateStatus();
      });
      line_edit->setPlaceholderText(placeholder_text);
      return line_edit;
    };

    // clang-format off
    routine(tr("Origin Symbol"     ), 0, 0, 1, &BranchWatchProxyModel::OnSymbolTextChanged<&BranchWatchProxyModel::m_origin_symbol_name>);
    routine(tr("Origin Min"        ), 1, 0, 1, &BranchWatchProxyModel::OnAddressTextChanged<&BranchWatchProxyModel::m_origin_min>)->setMaxLength(8);
    routine(tr("Origin Max"        ), 2, 0, 1, &BranchWatchProxyModel::OnAddressTextChanged<&BranchWatchProxyModel::m_origin_max>)->setMaxLength(8);
    routine(tr("Destination Symbol"), 0, 1, 1, &BranchWatchProxyModel::OnSymbolTextChanged<&BranchWatchProxyModel::m_destin_symbol_name>);
    routine(tr("Destination Min"   ), 1, 1, 1, &BranchWatchProxyModel::OnAddressTextChanged<&BranchWatchProxyModel::m_destin_min>)->setMaxLength(8);
    routine(tr("Destination Max"   ), 2, 1, 1, &BranchWatchProxyModel::OnAddressTextChanged<&BranchWatchProxyModel::m_destin_max>)->setMaxLength(8);
    // clang-format on

    auto* const group_box = new QGroupBox(tr("Origin and Destination"), nullptr);
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    m_act_origin_destin_filters = m_control_toolbar->addWidget(group_box);
  }
  {
    // Condition Filter Options
    auto* const layout = new QVBoxLayout(nullptr);
    layout->setAlignment(Qt::AlignHCenter);

    const auto routine = [this, layout](const QString& text,
                                        void (BranchWatchProxyModel::*slot)(bool)) {
      auto* const check_box = new QCheckBox(text, nullptr);
      layout->addWidget(check_box);
      connect(check_box, &QCheckBox::toggled, [this, slot](bool checked) {
        (m_table_proxy->*slot)(checked);
        UpdateStatus();
      });
      check_box->setChecked(true);
      return check_box;
    };

    routine(tr("true"), &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_cond_true>)
        ->setToolTip(tr("This will also filter unconditional branches.\n"
                        "To filter for or against unconditional branches,\n"
                        "use the Branch Type filter options."));
    routine(tr("false"), &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_cond_false>);

    auto* const group_box = new QGroupBox(tr("Condition"), nullptr);
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    m_act_condition_filters = m_control_toolbar->addWidget(group_box);
  }
  {
    // Misc. Controls
    m_btn_was_overwritten = new QPushButton(tr("Branch Was Overwritten"), nullptr);
    connect(m_btn_was_overwritten, &QPushButton::clicked, this,
            &BranchWatchDialog::OnBranchWasOverwritten);
    m_btn_was_overwritten->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_btn_not_overwritten = new QPushButton(tr("Branch Not Overwritten"), nullptr);
    connect(m_btn_not_overwritten, &QPushButton::clicked, this,
            &BranchWatchDialog::OnBranchNotOverwritten);
    m_btn_not_overwritten->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_btn_wipe_recent_hits = new QPushButton(tr("Wipe Recent Hits"), nullptr);
    connect(m_btn_wipe_recent_hits, &QPushButton::clicked, this,
            &BranchWatchDialog::OnWipeRecentHits);
    m_btn_wipe_recent_hits->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_btn_wipe_recent_hits->setEnabled(false);

    auto* const layout = new QVBoxLayout(nullptr);
    layout->addWidget(m_btn_was_overwritten);
    layout->addWidget(m_btn_not_overwritten);
    layout->addWidget(m_btn_wipe_recent_hits);

    auto* const group_box = new QGroupBox(tr("Misc. Controls"), nullptr);
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    m_act_misc_controls = m_control_toolbar->addWidget(group_box);
  }

  // Table Context Menus
  auto* const delete_action = new QAction(tr("&Delete"), this);
  connect(delete_action, &QAction::triggered, this, &BranchWatchDialog::OnTableDelete);

  m_act_invert_condition = new QAction(tr("Invert &Condition"), this);
  connect(m_act_invert_condition, &QAction::triggered, this,
          &BranchWatchDialog::OnTableInvertCondition);

  m_act_invert_decrement_check = new QAction(tr("Invert &Decrement Check"), this);
  connect(m_act_invert_decrement_check, &QAction::triggered, this,
          &BranchWatchDialog::OnTableInvertDecrementCheck);

  m_act_make_unconditional = new QAction(tr("Make &Unconditional"), this);
  connect(m_act_make_unconditional, &QAction::triggered, this,
          &BranchWatchDialog::OnTableMakeUnconditional);

  m_act_copy_address = new QAction(tr("&Copy Address"), this);
  connect(m_act_copy_address, &QAction::triggered, this, &BranchWatchDialog::OnTableCopyAddress);

  m_act_insert_nop = new QAction(tr("Insert &NOP"), this);
  connect(m_act_insert_nop, &QAction::triggered, this, &BranchWatchDialog::OnTableSetNOP);

  m_act_insert_blr = new QAction(tr("Insert &BLR"), this);
  connect(m_act_insert_blr, &QAction::triggered, this, &BranchWatchDialog::OnTableSetBLR);

  m_mnu_set_breakpoint = new QMenu(tr("Set Brea&kpoint"), this);
  m_act_break_on_hit = m_mnu_set_breakpoint->addAction(
      tr("&Break on Hit"), this, &BranchWatchDialog::OnTableSetBreakpointBreak);
  m_act_log_on_hit = m_mnu_set_breakpoint->addAction(tr("&Log on Hit"), this,
                                                     &BranchWatchDialog::OnTableSetBreakpointLog);
  m_act_both_on_hit = m_mnu_set_breakpoint->addAction(tr("Break &and Log on Hit"), this,
                                                      &BranchWatchDialog::OnTableSetBreakpointBoth);

  m_mnu_table_context_instruction = new QMenu(this);
  m_mnu_table_context_instruction->addActions(
      {delete_action, m_act_invert_condition, m_act_invert_decrement_check});

  m_mnu_table_context_condition = new QMenu(this);
  m_mnu_table_context_condition->addActions({delete_action, m_act_make_unconditional});

  m_mnu_table_context_origin = new QMenu(this);
  m_mnu_table_context_origin->addActions(
      {delete_action, m_act_insert_nop, m_act_copy_address, m_mnu_set_breakpoint->menuAction()});

  m_mnu_table_context_destin_or_symbol = new QMenu(this);
  m_mnu_table_context_destin_or_symbol->addActions(
      {delete_action, m_act_insert_blr, m_act_copy_address, m_mnu_set_breakpoint->menuAction()});

  m_mnu_table_context_other = new QMenu(this);
  m_mnu_table_context_other->addAction(delete_action);

  LoadQSettings();

  // Column Visibility Menu
  m_mnu_column_visibility = new QMenu(this);
  {
    static constexpr std::array<const char*, Column::NumberOfColumns> headers = {
        QT_TR_NOOP("Instruction"),   QT_TR_NOOP("Condition"),         QT_TR_NOOP("Origin"),
        QT_TR_NOOP("Destination"),   QT_TR_NOOP("Recent Hits"),       QT_TR_NOOP("Total Hits"),
        QT_TR_NOOP("Origin Symbol"), QT_TR_NOOP("Destination Symbol")};

    for (int column = 0; column < Column::NumberOfColumns; ++column)
    {
      auto* const action =
          m_mnu_column_visibility->addAction(tr(headers[column]), [this, column](bool enabled) {
            m_table_view->setColumnHidden(column, !enabled);
          });
      action->setChecked(!m_table_view->isColumnHidden(column));
      action->setCheckable(true);
    }
  }

  // Toolbar Visibility Menu
  auto* const toolbar_visibility_menu = new QMenu(this);
  {
    const auto routine = [toolbar_visibility_menu](const QString& text, QAction* toolbar_action) {
      auto* const menu_action =
          toolbar_visibility_menu->addAction(text, toolbar_action, &QAction::setVisible);
      menu_action->setChecked(toolbar_action->isVisible());
      menu_action->setCheckable(true);
    };
    routine(tr("&Branch Type"), m_act_branch_type_filters);
    routine(tr("&Origin and Destination"), m_act_origin_destin_filters);
    routine(tr("&Condition"), m_act_condition_filters);
    routine(tr("&Misc. Controls"), m_act_misc_controls);
  }

  // Menu Bar
  auto* const menu_bar = new QMenuBar(nullptr);
  menu_bar->setNativeMenuBar(false);
  {
    auto* const menu = menu_bar->addMenu(tr("&File"));
    menu->addAction(tr("&Save Branch Watch"), this, &BranchWatchDialog::OnSave);
    menu->addAction(tr("Save Branch Watch &As..."), this, &BranchWatchDialog::OnSaveAs);
    menu->addAction(tr("&Load Branch Watch"), this, &BranchWatchDialog::OnLoad);
    menu->addAction(tr("Load Branch Watch &From..."), this, &BranchWatchDialog::OnLoadFrom);
    m_act_autosave = menu->addAction(tr("A&uto Save"));
    m_act_autosave->setCheckable(true);
    connect(m_act_autosave, &QAction::toggled, this, &BranchWatchDialog::OnToggleAutoSave);
  }
  {
    auto* const menu = menu_bar->addMenu(tr("&Tool"));
    menu->setToolTipsVisible(true);
    menu->addAction(tr("Hide &Controls"), this, &BranchWatchDialog::OnHideShowControls)
        ->setCheckable(true);
    auto* const act_ignore_apploader = menu->addAction(tr("Ignore &Apploader Branch Hits"));
    act_ignore_apploader->setToolTip(
        tr("This only applies to the initial boot of the emulated software."));
    act_ignore_apploader->setChecked(m_system.IsBranchWatchIgnoreApploader());
    act_ignore_apploader->setCheckable(true);
    connect(act_ignore_apploader, &QAction::toggled, this,
            &BranchWatchDialog::OnToggleIgnoreApploader);
    menu->addMenu(m_mnu_column_visibility)->setText(tr("Column &Visibility"));
    menu->addMenu(toolbar_visibility_menu)->setText(tr("&Toolbar Visibility"));
    menu->addAction(tr("Wipe &Inspection Data"), this, &BranchWatchDialog::OnWipeInspection);
    menu->addAction(tr("&Help"), this, &BranchWatchDialog::OnHelp);
  }

  connect(m_timer = new QTimer(this), &QTimer::timeout, this, &BranchWatchDialog::OnTimeout);
  connect(m_table_proxy, &BranchWatchProxyModel::layoutChanged, this,
          &BranchWatchDialog::UpdateStatus);

  auto* const main_layout = new QVBoxLayout(nullptr);
  main_layout->setMenuBar(menu_bar);
  main_layout->addWidget(m_control_toolbar);
  main_layout->addWidget(m_table_view);
  main_layout->addWidget(m_status_bar);
  setLayout(main_layout);
}

BranchWatchDialog::~BranchWatchDialog()
{
  SaveQSettings();
}

static constexpr int BRANCH_WATCH_TOOL_TIMER_DELAY_MS = 100;

static bool TimerCondition(const Core::BranchWatch& branch_watch, Core::State state)
{
  return branch_watch.GetRecordingActive() && state > Core::State::Paused;
}

void BranchWatchDialog::hideEvent(QHideEvent* event)
{
  Hide();
  QDialog::hideEvent(event);
}

void BranchWatchDialog::showEvent(QShowEvent* event)
{
  Show();
  QDialog::showEvent(event);
}

void BranchWatchDialog::OnStartPause(bool checked) const
{
  m_branch_watch.SetRecordingActive(Core::CPUThreadGuard{m_system}, checked);
  if (checked)
  {
    m_btn_start_pause->setText(tr("Pause Branch Watch"));
    if (Core::GetState(m_system) > Core::State::Paused)
      m_timer->start(BRANCH_WATCH_TOOL_TIMER_DELAY_MS);
  }
  else
  {
    m_btn_start_pause->setText(tr("Start Branch Watch"));
    if (m_timer->isActive())
      m_timer->stop();
  }
  Update();
}

void BranchWatchDialog::OnClearBranchWatch()
{
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnClearBranchWatch(guard);
    AutoSave(guard);
  }
  m_btn_wipe_recent_hits->setEnabled(false);
  UpdateStatus();
}

static std::string GetSnapshotDefaultFilepath()
{
  return fmt::format("{}{}.txt", File::GetUserPath(D_DUMPDEBUG_BRANCHWATCH_IDX),
                     SConfig::GetInstance().GetGameID());
}

void BranchWatchDialog::OnSave()
{
  if (!m_branch_watch.CanSave())
  {
    ModalMessageBox::warning(this, tr("Error"), tr("There is nothing to save!"));
    return;
  }

  Save(Core::CPUThreadGuard{m_system}, GetSnapshotDefaultFilepath());
}

void BranchWatchDialog::OnSaveAs()
{
  if (!m_branch_watch.CanSave())
  {
    ModalMessageBox::warning(this, tr("Error"), tr("There is nothing to save!"));
    return;
  }

  const QString filepath = DolphinFileDialog::getSaveFileName(
      this, tr("Save Branch Watch Snapshot"),
      QString::fromStdString(File::GetUserPath(D_DUMPDEBUG_BRANCHWATCH_IDX)),
      tr("Text file (*.txt);;All Files (*)"));
  if (filepath.isEmpty())
    return;

  Save(Core::CPUThreadGuard{m_system}, filepath.toStdString());
}

void BranchWatchDialog::OnLoad()
{
  Load(Core::CPUThreadGuard{m_system}, GetSnapshotDefaultFilepath());
}

void BranchWatchDialog::OnLoadFrom()
{
  const QString filepath = DolphinFileDialog::getOpenFileName(
      this, tr("Load Branch Watch Snapshot"),
      QString::fromStdString(File::GetUserPath(D_DUMPDEBUG_BRANCHWATCH_IDX)),
      tr("Text file (*.txt);;All Files (*)"), nullptr, QFileDialog::Option::ReadOnly);
  if (filepath.isEmpty())
    return;

  Load(Core::CPUThreadGuard{m_system}, filepath.toStdString());
}

void BranchWatchDialog::OnCodePathWasTaken()
{
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnCodePathWasTaken(guard);
    AutoSave(guard);
  }
  m_btn_wipe_recent_hits->setEnabled(true);
  UpdateStatus();
}

void BranchWatchDialog::OnCodePathNotTaken()
{
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnCodePathNotTaken(guard);
    AutoSave(guard);
  }
  UpdateStatus();
}

void BranchWatchDialog::OnBranchWasOverwritten()
{
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnBranchWasOverwritten(guard);
    AutoSave(guard);
  }
  UpdateStatus();
}

void BranchWatchDialog::OnBranchNotOverwritten()
{
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnBranchNotOverwritten(guard);
    AutoSave(guard);
  }
  UpdateStatus();
}

void BranchWatchDialog::OnWipeRecentHits() const
{
  m_table_model->OnWipeRecentHits();
}

void BranchWatchDialog::OnWipeInspection() const
{
  m_table_model->OnWipeInspection();
}

void BranchWatchDialog::OnTimeout() const
{
  Update();
}

void BranchWatchDialog::OnEmulationStateChanged(Core::State new_state) const
{
  m_btn_was_overwritten->setEnabled(new_state != Core::State::Uninitialized);
  m_btn_not_overwritten->setEnabled(new_state != Core::State::Uninitialized);
  if (TimerCondition(m_branch_watch, new_state))
    m_timer->start(BRANCH_WATCH_TOOL_TIMER_DELAY_MS);
  else if (m_timer->isActive())
    m_timer->stop();
  Update();
}

void BranchWatchDialog::OnThemeChanged()
{
  UpdateIcons();
}

void BranchWatchDialog::OnHelp()
{
  ModalMessageBox::information(
      this, tr("Branch Watch Tool Help (1/4)"),
      tr("Branch Watch is a code-searching tool that can isolate branches tracked by the emulated "
         "CPU by testing candidate branches with simple criteria. If you are familiar with Cheat "
         "Engine's Ultimap, Branch Watch is similar to that."
         "\n\n"
         "Press the \"Start Branch Watch\" button to activate Branch Watch. Branch Watch persists "
         "across emulation sessions, and a snapshot of your progress can be saved to and loaded "
         "from the User Directory to persist after Dolphin Emulator is closed. \"Save As...\" and "
         "\"Load From...\" actions are also available, and auto-saving can be enabled to save a "
         "snapshot at every step of a search. The \"Pause Branch Watch\" button will halt Branch "
         "Watch from tracking further branch hits until it is told to resume. Press the \"Clear "
         "Branch Watch\" button to clear all candidates and return to the blacklist phase."));
  ModalMessageBox::information(
      this, tr("Branch Watch Tool Help (2/4)"),
      tr("Branch Watch starts in the blacklist phase, meaning no candidates have been chosen yet, "
         "but candidates found so far can be excluded from the candidacy by pressing the \"Code "
         "Path Not Taken\", \"Branch Was Overwritten\", and \"Branch Not Overwritten\" buttons. "
         "Once the \"Code Path Was Taken\" button is pressed for the first time, Branch Watch will "
         "switch to the reduction phase, and the table will populate with all eligible "
         "candidates."));
  ModalMessageBox::information(
      this, tr("Branch Watch Tool Help (3/4)"),
      tr("Once in the reduction phase, it is time to start narrowing down the candidates shown in "
         "the table. Further reduce the candidates by checking whether a code path was or was not "
         "taken since the last time it was checked. It is also possible to reduce the candidates "
         "by determining whether a branch instruction has or has not been overwritten since it was "
         "first hit. Filter the candidates by branch kind, branch condition, origin or destination "
         "address, and origin or destination symbol name."
         "\n\n"
         "After enough passes and experimentation, you may be able to find function calls and "
         "conditional code paths that are only taken when an action is performed in the emulated "
         "software."));
  ModalMessageBox::information(
      this, tr("Branch Watch Tool Help (4/4)"),
      tr("Rows in the table can be left-clicked on the origin, destination, and symbol columns to "
         "view the associated address in Code View. Right-clicking the selected row(s) will bring "
         "up a context menu."
         "\n\n"
         "If the origin, destination, or symbol columns are right-clicked, an action copy the "
         "relevant address(es) to the clipboard will be available, and an action to set a "
         "breakpoint at the relevant address(es) will be available. Note that, for the origin / "
         "destination symbol columns, these actions will only be enabled if every row in the "
         "selection has a symbol."
         "\n\n"
         "If the instruction column of a row selection is right-clicked, an action to invert the "
         "branch instruction's condition and an action to invert the branch instruction's "
         "decrement check will be available, but only if the branch instruction is a conditional "
         "one."
         "\n\n"
         "If the condition column of a row selection is right-clicked, an action to make the "
         "branch instruction unconditional will be available, but only if the branch instruction "
         "is a conditional one."
         "\n\n"
         "If the origin column of a row selection is right-clicked, an action to replace the "
         "branch instruction at the origin(s) with a NOP instruction (No Operation) will be "
         "available."
         "\n\n"
         "If the destination column of a row selection is right-clicked, an action to replace the "
         "instruction at the destination(s) with a BLR instruction (Branch to Link Register) will "
         "be available, but will only be enabled if the branch instruction at every origin updates "
         "the link register."
         "\n\n"
         "If the origin / destination symbol column of a row selection is right-clicked, an action "
         "to replace the instruction at the start of the symbol(s) with a BLR instruction will be "
         "available, but will only be enabled if every row in the selection has a symbol."
         "\n\n"
         "All context menus have the action to delete the selected row(s) from the candidates."));
}

void BranchWatchDialog::OnToggleAutoSave(bool checked)
{
  if (!checked)
    return;

  const QString filepath = DolphinFileDialog::getSaveFileName(
      // i18n: If the user selects a file, Branch Watch will save to that file.
      // If the user presses Cancel, Branch Watch will save to a file in the user folder.
      this, tr("Select Branch Watch Snapshot Auto-Save File (for user folder location, cancel)"),
      QString::fromStdString(File::GetUserPath(D_DUMPDEBUG_BRANCHWATCH_IDX)),
      tr("Text file (*.txt);;All Files (*)"));
  if (filepath.isEmpty())
    m_autosave_filepath = std::nullopt;
  else
    m_autosave_filepath = filepath.toStdString();
}

void BranchWatchDialog::OnHideShowControls(bool checked) const
{
  if (checked)
    m_control_toolbar->hide();
  else
    m_control_toolbar->show();
}

void BranchWatchDialog::OnToggleIgnoreApploader(bool checked) const
{
  m_system.SetIsBranchWatchIgnoreApploader(checked);
}

void BranchWatchDialog::OnTableClicked(const QModelIndex& index) const
{
  const QVariant v = m_table_proxy->data(index, UserRole::ClickRole);
  switch (index.column())
  {
  case Column::OriginSymbol:
  case Column::DestinSymbol:
    if (!v.isValid())
      return;
    [[fallthrough]];
  case Column::Origin:
  case Column::Destination:
    m_code_widget->SetAddress(v.value<u32>(), CodeViewWidget::SetAddressUpdate::WithDetailedUpdate);
    return;
  }
}

void BranchWatchDialog::OnTableContextMenu(const QPoint& pos) const
{
  if (m_table_view->horizontalHeader()->hiddenSectionCount() == Column::NumberOfColumns)
  {
    m_mnu_column_visibility->exec(m_table_view->viewport()->mapToGlobal(pos));
    return;
  }
  const QModelIndex index = m_table_view->indexAt(pos);
  if (!index.isValid())
    return;
  m_index_list_temp = m_table_view->selectionModel()->selectedRows(index.column());
  GetTableContextMenu(index)->exec(m_table_view->viewport()->mapToGlobal(pos));
  m_index_list_temp.clear();
  m_index_list_temp.shrink_to_fit();
}

void BranchWatchDialog::OnTableHeaderContextMenu(const QPoint& pos) const
{
  m_mnu_column_visibility->exec(m_table_view->horizontalHeader()->mapToGlobal(pos));
}

void BranchWatchDialog::OnTableDelete() const
{
  std::ranges::transform(
      m_index_list_temp, m_index_list_temp.begin(),
      [this](const QModelIndex& index) { return m_table_proxy->mapToSource(index); });
  std::ranges::sort(m_index_list_temp, std::less{});
  for (const auto& index : std::ranges::reverse_view{m_index_list_temp})
  {
    if (!index.isValid())
      continue;
    m_table_model->removeRow(index.row());
  }
  UpdateStatus();
}

void BranchWatchDialog::OnTableDeleteKeypress() const
{
  m_index_list_temp = m_table_view->selectionModel()->selectedRows();
  OnTableDelete();
  m_index_list_temp.clear();
  m_index_list_temp.shrink_to_fit();
}

void BranchWatchDialog::OnTableSetBLR() const
{
  SetStubPatches(0x4e800020);
}

void BranchWatchDialog::OnTableSetNOP() const
{
  SetStubPatches(0x60000000);
}

void BranchWatchDialog::OnTableInvertCondition() const
{
  SetEditPatches([](u32 hex) {
    UGeckoInstruction inst = hex;
    inst.BO ^= 0b01000;
    return inst.hex;
  });
}

void BranchWatchDialog::OnTableInvertDecrementCheck() const
{
  SetEditPatches([](u32 hex) {
    UGeckoInstruction inst = hex;
    inst.BO ^= 0b00010;
    return inst.hex;
  });
}

void BranchWatchDialog::OnTableMakeUnconditional() const
{
  SetEditPatches([](u32 hex) {
    UGeckoInstruction inst = hex;
    inst.BO = 0b10100;  // 1z1zz - Branch always
    return inst.hex;
  });
}

void BranchWatchDialog::OnTableCopyAddress() const
{
  auto iter = m_index_list_temp.begin();
  if (iter == m_index_list_temp.end())
    return;

  QString text;
  text.reserve(m_index_list_temp.size() * 9 - 1);
  while (true)
  {
    text.append(QString::number(m_table_proxy->data(*iter, UserRole::ClickRole).value<u32>(), 16));
    if (++iter == m_index_list_temp.end())
      break;
    text.append(QChar::fromLatin1('\n'));
  }
  QApplication::clipboard()->setText(text);
}

void BranchWatchDialog::OnTableSetBreakpointBreak() const
{
  SetBreakpoints(true, false);
}

void BranchWatchDialog::OnTableSetBreakpointLog() const
{
  SetBreakpoints(false, true);
}

void BranchWatchDialog::OnTableSetBreakpointBoth() const
{
  SetBreakpoints(true, true);
}

void BranchWatchDialog::ConnectSlots()
{
  const auto* const settings = &Settings::Instance();
  connect(settings, &Settings::EmulationStateChanged, this,
          &BranchWatchDialog::OnEmulationStateChanged);
  connect(settings, &Settings::ThemeChanged, this, &BranchWatchDialog::OnThemeChanged);
  connect(settings, &Settings::DebugFontChanged, m_table_model, &BranchWatchTableModel::setFont);
  const auto* const host = Host::GetInstance();
  connect(host, &Host::PPCSymbolsChanged, m_table_model, &BranchWatchTableModel::UpdateSymbols);
}

void BranchWatchDialog::DisconnectSlots()
{
  const auto* const settings = &Settings::Instance();
  disconnect(settings, &Settings::EmulationStateChanged, this,
             &BranchWatchDialog::OnEmulationStateChanged);
  disconnect(settings, &Settings::ThemeChanged, this, &BranchWatchDialog::OnThemeChanged);
  disconnect(settings, &Settings::DebugFontChanged, m_table_model,
             &BranchWatchTableModel::OnDebugFontChanged);
  const auto* const host = Host::GetInstance();
  disconnect(host, &Host::PPCSymbolsChanged, m_table_model,
             &BranchWatchTableModel::OnPPCSymbolsChanged);
}

void BranchWatchDialog::Show()
{
  ConnectSlots();
  // Hit every slot that may have missed a signal while this widget was hidden.
  OnEmulationStateChanged(Core::GetState(m_system));
  OnThemeChanged();
  m_table_model->OnDebugFontChanged(Settings::Instance().GetDebugFont());
  m_table_model->OnPPCSymbolsChanged();
}

void BranchWatchDialog::Hide()
{
  DisconnectSlots();
  if (m_timer->isActive())
    m_timer->stop();
}

void BranchWatchDialog::LoadQSettings()
{
  const auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("branchwatchdialog/geometry")).toByteArray());
  m_table_view->horizontalHeader()->restoreState(  // Restore column visibility state.
      settings.value(QStringLiteral("branchwatchdialog/tableheader/state")).toByteArray());
  m_act_branch_type_filters->setVisible(
      !settings.value(QStringLiteral("branchwatchdialog/toolbar/branch_type_hidden")).toBool());
  m_act_origin_destin_filters->setVisible(
      !settings.value(QStringLiteral("branchwatchdialog/toolbar/origin_destin_hidden")).toBool());
  m_act_condition_filters->setVisible(
      !settings.value(QStringLiteral("branchwatchdialog/toolbar/condition_hidden")).toBool());
  m_act_misc_controls->setVisible(
      !settings.value(QStringLiteral("branchwatchdialog/toolbar/misc_controls_hidden")).toBool());
}

void BranchWatchDialog::SaveQSettings() const
{
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("branchwatchdialog/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("branchwatchdialog/tableheader/state"),
                    m_table_view->horizontalHeader()->saveState());
  settings.setValue(QStringLiteral("branchwatchdialog/toolbar/branch_type_hidden"),
                    !m_act_branch_type_filters->isVisible());
  settings.setValue(QStringLiteral("branchwatchdialog/toolbar/origin_destin_hidden"),
                    !m_act_origin_destin_filters->isVisible());
  settings.setValue(QStringLiteral("branchwatchdialog/toolbar/condition_hidden"),
                    !m_act_condition_filters->isVisible());
  settings.setValue(QStringLiteral("branchwatchdialog/toolbar/misc_controls_hidden"),
                    !m_act_misc_controls->isVisible());
}

void BranchWatchDialog::Update() const
{
  if (m_branch_watch.GetRecordingPhase() == Core::BranchWatch::Phase::Blacklist)
    UpdateStatus();
  m_table_model->UpdateHits();
}

void BranchWatchDialog::UpdateStatus() const
{
  switch (m_branch_watch.GetRecordingPhase())
  {
  case Core::BranchWatch::Phase::Blacklist:
  {
    const std::size_t candidate_size = m_branch_watch.GetCollectionSize();
    const std::size_t blacklist_size = m_branch_watch.GetBlacklistSize();
    if (blacklist_size == 0)
    {
      m_status_bar->showMessage(tr("Candidates: %1").arg(candidate_size));
      return;
    }
    m_status_bar->showMessage(tr("Candidates: %1 | Excluded: %2 | Remaining: %3")
                                  .arg(candidate_size)
                                  .arg(blacklist_size)
                                  .arg(candidate_size - blacklist_size));
    return;
  }
  case Core::BranchWatch::Phase::Reduction:
  {
    const std::size_t candidate_size = m_branch_watch.GetSelection().size();
    if (candidate_size == 0)
    {
      m_status_bar->showMessage(tr("Zero candidates remaining."));
      return;
    }
    const std::size_t remaining_size = m_table_proxy->rowCount();
    m_status_bar->showMessage(tr("Candidates: %1 | Filtered: %2 | Remaining: %3")
                                  .arg(candidate_size)
                                  .arg(candidate_size - remaining_size)
                                  .arg(remaining_size));
    return;
  }
  }
}

void BranchWatchDialog::UpdateIcons()
{
  m_icn_full = Resources::GetThemeIcon("debugger_breakpoint");
  m_icn_partial = Resources::GetThemeIcon("stop");
}

void BranchWatchDialog::Save(const Core::CPUThreadGuard& guard, const std::string& filepath)
{
  File::IOFile file(filepath, "w");
  if (!file.IsOpen())
  {
    ModalMessageBox::warning(
        this, tr("Error"),
        tr("Failed to save Branch Watch snapshot \"%1\"").arg(QString::fromStdString(filepath)));
    return;
  }

  m_table_model->Save(guard, file.GetHandle());
}

void BranchWatchDialog::Load(const Core::CPUThreadGuard& guard, const std::string& filepath)
{
  File::IOFile file(filepath, "r");
  if (!file.IsOpen())
  {
    ModalMessageBox::warning(
        this, tr("Error"),
        tr("Failed to open Branch Watch snapshot \"%1\"").arg(QString::fromStdString(filepath)));
    return;
  }

  m_table_model->Load(guard, file.GetHandle());
  m_btn_wipe_recent_hits->setEnabled(m_branch_watch.GetRecordingPhase() ==
                                     Core::BranchWatch::Phase::Reduction);
}

void BranchWatchDialog::AutoSave(const Core::CPUThreadGuard& guard)
{
  if (!m_act_autosave->isChecked() || !m_branch_watch.CanSave())
    return;
  Save(guard, m_autosave_filepath ? m_autosave_filepath.value() : GetSnapshotDefaultFilepath());
}

void BranchWatchDialog::SetStubPatches(u32 value) const
{
  auto& debug_interface = m_system.GetPowerPC().GetDebugInterface();
  for (const Core::CPUThreadGuard guard(m_system); const QModelIndex& index : m_index_list_temp)
  {
    debug_interface.SetPatch(guard, m_table_proxy->data(index, UserRole::ClickRole).value<u32>(),
                             value);
    m_table_proxy->SetInspected(index);
  }
  // TODO: This is not ideal. What I need is a signal for when memory has been changed by the GUI,
  // but I cannot find one. UpdateDisasmDialog comes close, but does too much in one signal. For
  // example, CodeViewWidget will scroll to the current PC when UpdateDisasmDialog is signaled. This
  // seems like a pervasive issue. For example, modifying an instruction in the CodeViewWidget will
  // not reflect in the MemoryViewWidget, and vice versa. Neither of these widgets changing memory
  // will reflect in the JITWidget, either. At the very least, we can make sure the CodeWidget
  // is updated in an acceptable way.
  m_code_widget->Update();
}

void BranchWatchDialog::SetEditPatches(u32 (*transform)(u32)) const
{
  auto& debug_interface = m_system.GetPowerPC().GetDebugInterface();
  for (const Core::CPUThreadGuard guard(m_system); const QModelIndex& index : m_index_list_temp)
  {
    const Core::BranchWatchCollectionKey& k =
        m_table_proxy->GetBranchWatchSelection(index).collection_ptr->first;
    // This function assumes patches apply to the origin address, unlike SetStubPatches.
    debug_interface.SetPatch(guard, k.origin_addr, transform(k.original_inst.hex));
    m_table_proxy->SetInspected(index);
  }
  // TODO: Same issue as SetStubPatches.
  m_code_widget->Update();
}

void BranchWatchDialog::SetBreakpoints(bool break_on_hit, bool log_on_hit) const
{
  auto& breakpoints = m_system.GetPowerPC().GetBreakPoints();
  for (const QModelIndex& index : m_index_list_temp)
  {
    const u32 address = m_table_proxy->data(index, UserRole::ClickRole).value<u32>();
    breakpoints.Add(address, break_on_hit, log_on_hit, {});
  }
  emit m_code_widget->BreakpointsChanged();
  m_code_widget->Update();
}

void BranchWatchDialog::SetBreakpointMenuActionsIcons() const
{
  qsizetype bp_break_count = 0, bp_log_count = 0, bp_both_count = 0;
  for (auto& breakpoints = m_system.GetPowerPC().GetBreakPoints();
       const QModelIndex& index : m_index_list_temp)
  {
    if (const TBreakPoint* bp = breakpoints.GetRegularBreakpoint(
            m_table_proxy->data(index, UserRole::ClickRole).value<u32>()))
    {
      if (bp->break_on_hit && bp->log_on_hit)
      {
        bp_both_count += 1;
        continue;
      }
      bp_break_count += bp->break_on_hit;
      bp_log_count += bp->log_on_hit;
    }
  }
  const qsizetype selected_row_count = m_index_list_temp.size();
  m_act_break_on_hit->setIconVisibleInMenu(bp_break_count != 0);
  m_act_break_on_hit->setIcon(bp_break_count == selected_row_count ? m_icn_full : m_icn_partial);
  m_act_log_on_hit->setIconVisibleInMenu(bp_log_count != 0);
  m_act_log_on_hit->setIcon(bp_log_count == selected_row_count ? m_icn_full : m_icn_partial);
  m_act_both_on_hit->setIconVisibleInMenu(bp_both_count != 0);
  m_act_both_on_hit->setIcon(bp_both_count == selected_row_count ? m_icn_full : m_icn_partial);
}

QMenu* BranchWatchDialog::GetTableContextMenu(const QModelIndex& index) const
{
  const bool core_initialized = Core::GetState(m_system) != Core::State::Uninitialized;
  switch (index.column())
  {
  case Column::Instruction:
    return GetTableContextMenu_Instruction(core_initialized);
  case Column::Condition:
    return GetTableContextMenu_Condition(core_initialized);
  case Column::Origin:
    return GetTableContextMenu_Origin(core_initialized);
  case Column::Destination:
    return GetTableContextMenu_Destin(core_initialized);
  case Column::RecentHits:
  case Column::TotalHits:
    return m_mnu_table_context_other;
  case Column::OriginSymbol:
  case Column::DestinSymbol:
    return GetTableContextMenu_Symbol(core_initialized);
  }
  static_assert(Column::NumberOfColumns == 8);
  Common::Unreachable();
}

QMenu* BranchWatchDialog::GetTableContextMenu_Instruction(bool core_initialized) const
{
  const bool all_branches_conditional =  // Taking advantage of short-circuit evaluation here.
      core_initialized && std::ranges::all_of(m_index_list_temp, [this](const QModelIndex& index) {
        return BranchIsConditional(
            m_table_proxy->GetBranchWatchSelection(index).collection_ptr->first.original_inst);
      });
  m_act_invert_condition->setEnabled(all_branches_conditional);
  m_act_invert_decrement_check->setEnabled(all_branches_conditional);
  return m_mnu_table_context_instruction;
}

QMenu* BranchWatchDialog::GetTableContextMenu_Condition(bool core_initialized) const
{
  const bool all_branches_conditional =  // Taking advantage of short-circuit evaluation here.
      core_initialized && std::ranges::all_of(m_index_list_temp, [this](const QModelIndex& index) {
        return BranchIsConditional(
            m_table_proxy->GetBranchWatchSelection(index).collection_ptr->first.original_inst);
      });
  m_act_make_unconditional->setEnabled(all_branches_conditional);
  return m_mnu_table_context_condition;
}

QMenu* BranchWatchDialog::GetTableContextMenu_Origin(bool core_initialized) const
{
  SetBreakpointMenuActionsIcons();
  m_act_insert_nop->setEnabled(core_initialized);
  m_act_copy_address->setEnabled(true);
  m_mnu_set_breakpoint->setEnabled(true);
  return m_mnu_table_context_origin;
}

QMenu* BranchWatchDialog::GetTableContextMenu_Destin(bool core_initialized) const
{
  SetBreakpointMenuActionsIcons();
  const bool all_branches_save_lr =  // Taking advantage of short-circuit evaluation here.
      core_initialized && std::ranges::all_of(m_index_list_temp, [this](const QModelIndex& index) {
        return m_table_proxy->GetBranchWatchSelection(index).collection_ptr->first.original_inst.LK;
      });
  m_act_insert_blr->setEnabled(all_branches_save_lr);
  m_act_copy_address->setEnabled(true);
  m_mnu_set_breakpoint->setEnabled(true);
  return m_mnu_table_context_destin_or_symbol;
}

QMenu* BranchWatchDialog::GetTableContextMenu_Symbol(bool core_initialized) const
{
  SetBreakpointMenuActionsIcons();
  const bool all_symbols_valid =
      std::ranges::all_of(m_index_list_temp, [this](const QModelIndex& index) {
        return m_table_proxy->data(index, UserRole::ClickRole).isValid();
      });
  m_act_insert_blr->setEnabled(core_initialized && all_symbols_valid);
  m_act_copy_address->setEnabled(all_symbols_valid);
  m_mnu_set_breakpoint->setEnabled(all_symbols_valid);
  return m_mnu_table_context_destin_or_symbol;
}

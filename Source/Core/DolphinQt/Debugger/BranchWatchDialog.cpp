// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/BranchWatchDialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/BranchWatch.h"

#include "DolphinQt/Debugger/BranchWatchProxyModel.h"
#include "DolphinQt/Debugger/BranchWatchTableModel.h"
#include "DolphinQt/Debugger/BranchWatchTableView.h"
#include "DolphinQt/Debugger/CodeWidget.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

static constexpr int CODE_DIFF_TIMER_DELAY_MS = 100;
static constexpr int CODE_DIFF_TIMER_PAUSE_ONESHOT_MS = 500;

static bool TimerCondition(const Core::BranchWatch& branch_watch, Core::State state)
{
  return branch_watch.IsRecordingActive() && state > Core::State::Paused;
}

BranchWatchDialog::BranchWatchDialog(Core::System& system, Core::BranchWatch& branch_watch,
                                     CodeWidget* code_widget, QWidget* parent)
    : QDialog(parent), m_system(system), m_branch_watch(branch_watch), m_code_widget(code_widget)
{
  setWindowTitle(tr("Branch Watch Tool"));
  setLayout(CreateLayout());
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("branchwatchdialog/geometry")).toByteArray());
}

void BranchWatchDialog::done(int r)
{
  if (m_timer->isActive())
    m_timer->stop();
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("branchwatchdialog/geometry"), saveGeometry());
  QDialog::done(r);
}

int BranchWatchDialog::exec()
{
  if (TimerCondition(m_branch_watch, Core::GetState()))
    m_timer->start(CODE_DIFF_TIMER_DELAY_MS);
  return QDialog::exec();
}

void BranchWatchDialog::open()
{
  if (TimerCondition(m_branch_watch, Core::GetState()))
    m_timer->start(CODE_DIFF_TIMER_DELAY_MS);
  QDialog::open();
}
QLayout* BranchWatchDialog::CreateLayout()
{
  auto* layout = new QVBoxLayout;

  // Menu Bar
  layout->setMenuBar([this]() {
    QMenuBar* menu_bar = new QMenuBar;
    menu_bar->setNativeMenuBar(false);

    QMenu* menu_file = new QMenu(tr("&File"), menu_bar);
    menu_file->addAction(tr("&Save Branch Watch"), this, &BranchWatchDialog::OnSave);
    menu_file->addAction(tr("Save Branch Watch &As..."), this, &BranchWatchDialog::OnSaveAs);
    menu_file->addAction(tr("&Load Branch Watch"), this, &BranchWatchDialog::OnLoad);
    menu_file->addAction(tr("Load Branch Watch &From..."), this, &BranchWatchDialog::OnLoadFrom);
    m_act_autosave = menu_file->addAction(tr("A&uto Save"));
    m_act_autosave->setCheckable(true);
    connect(m_act_autosave, &QAction::toggled, this, &BranchWatchDialog::OnToggleAutoSave);
    menu_bar->addMenu(menu_file);

    QMenu* menu_tool = new QMenu(tr("&Tool"), menu_bar);
    menu_tool->addAction(tr("Hide &Controls"), this, &BranchWatchDialog::OnHideShowControls)
        ->setCheckable(true);
    m_act_autopause = menu_tool->addAction(tr("&Pause On Cold Load"));
    m_act_autopause->setCheckable(true);
    m_act_autopause->setChecked(true);
    menu_tool->addAction(tr("&Help"), this, &BranchWatchDialog::OnHelp);

    menu_bar->addMenu(menu_tool);

    return menu_bar;
  }());

  // Controls Toolbar (widgets are added later)
  layout->addWidget(m_control_toolbar = new QToolBar);

  // Branch Watch Table
  layout->addWidget(m_table_view = [this]() {
    auto& settings = Settings::Instance();

    m_table_proxy = new BranchWatchProxyModel(m_branch_watch);
    m_table_proxy->setSourceModel(m_table_model =
                                      new BranchWatchTableModel(m_system, m_branch_watch));
    m_table_proxy->setSortRole(BranchWatchTableModel::UserRole::SortRole);

    auto* table_view = new BranchWatchTableView(m_system, m_code_widget);
    table_view->setModel(m_table_proxy);
    table_view->setSortingEnabled(true);
    table_view->sortByColumn(BranchWatchTableModel::Column::Origin, Qt::AscendingOrder);
    table_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table_view->setContextMenuPolicy(Qt::CustomContextMenu);
    table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_view->setColumnWidth(BranchWatchTableModel::Column::Instruction, 50);
    // The default column width of 100 units is fine for the rest.
    table_view->horizontalHeader()->setStretchLastSection(true);
    table_view->verticalHeader()->hide();
    table_view->setCornerButtonEnabled(false);
    table_view->setFont(settings.GetDebugFont());

    connect(&settings, &Settings::DebugFontChanged, table_view, &BranchWatchTableView::setFont);
    connect(table_view, &BranchWatchTableView::clicked, table_view,
            &BranchWatchTableView::OnClicked);
    connect(table_view, &BranchWatchTableView::customContextMenuRequested, table_view,
            &BranchWatchTableView::OnContextMenu);

    auto* delete_key_seq = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(delete_key_seq, &QShortcut::activated, table_view,
            &BranchWatchTableView::OnDeleteKeypress);

    return table_view;
  }());

  // Status Bar
  layout->addWidget(m_status_bar = []() {
    auto* status_bar = new QStatusBar;
    status_bar->setSizeGripEnabled(false);
    return status_bar;
  }());

  // Branch Watch Controls
  m_control_toolbar->addWidget([this]() {
    auto* layout = new QGridLayout;

    layout->addWidget(m_btn_start_stop = new QPushButton(tr("Start Recording")), 0, 0);
    connect(m_btn_start_stop, &QPushButton::toggled, this, &BranchWatchDialog::OnStartStop);
    m_btn_start_stop->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_btn_start_stop->setCheckable(true);

    layout->addWidget(m_btn_pause_resume = new QPushButton(tr("Pause Recording")), 1, 0);
    connect(m_btn_pause_resume, &QPushButton::toggled, this, &BranchWatchDialog::OnPauseResume);
    m_btn_pause_resume->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_btn_pause_resume->setCheckable(true);
    m_btn_pause_resume->setEnabled(false);

    layout->addWidget(m_btn_has_executed = new QPushButton(tr("Branch Has Executed")), 0, 1);
    connect(m_btn_has_executed, &QPushButton::pressed, this,
            &BranchWatchDialog::OnBranchHasExecuted);
    m_btn_has_executed->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_btn_has_executed->setEnabled(false);

    layout->addWidget(m_btn_not_executed = new QPushButton(tr("Branch Not Executed")), 1, 1);
    connect(m_btn_not_executed, &QPushButton::pressed, this,
            &BranchWatchDialog::OnBranchNotExecuted);
    m_btn_not_executed->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_btn_not_executed->setEnabled(false);

    auto* group_box = new QGroupBox(tr("Branch Watch Controls"));
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    return group_box;
  }());

  // Spacer
  m_control_toolbar->addWidget([]() {
    auto* widget = new QWidget;
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return widget;
  }());

  // Branch Filter Options
  m_control_toolbar->addWidget([this]() {
    auto* layout = new QGridLayout;

    const auto InstallCheckBox = [this, layout](const QString& text, int row, int column,
                                                void (BranchWatchProxyModel::*slot)(bool)) {
      QCheckBox* check_box = new QCheckBox(text);
      layout->addWidget(check_box, row, column);
      connect(check_box, &QCheckBox::toggled, [this, slot](bool checked) {
        (m_table_proxy->*slot)(checked);
        UpdateStatus();
      });
      check_box->setChecked(true);
    };

    // clang-format off
    InstallCheckBox(QStringLiteral("b"     ), 0, 0, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_b     >);
    InstallCheckBox(QStringLiteral("bl"    ), 0, 1, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bl    >);
    InstallCheckBox(QStringLiteral("bc"    ), 0, 2, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bc    >);
    InstallCheckBox(QStringLiteral("bcl"   ), 0, 3, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bcl   >);
    InstallCheckBox(QStringLiteral("blr"   ), 1, 0, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_blr   >);
    InstallCheckBox(QStringLiteral("blrl"  ), 1, 1, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_blrl  >);
    InstallCheckBox(QStringLiteral("bclr"  ), 1, 2, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bclr  >);
    InstallCheckBox(QStringLiteral("bclrl" ), 1, 3, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bclrl >);
    InstallCheckBox(QStringLiteral("bctr"  ), 2, 0, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bctr  >);
    InstallCheckBox(QStringLiteral("bctrl" ), 2, 1, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bctrl >);
    InstallCheckBox(QStringLiteral("bcctr" ), 2, 2, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bcctr >);
    InstallCheckBox(QStringLiteral("bcctrl"), 2, 3, &BranchWatchProxyModel::OnToggled<&BranchWatchProxyModel::m_bcctrl>);
    // clang-format on

    auto* group_box = new QGroupBox(tr("Branch Filter Options"));
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    return group_box;
  }());

  // Other Filter Options
  m_control_toolbar->addWidget([this]() {
    auto* layout = new QGridLayout;

    const auto InstallLineEdit = [this,
                                  layout](const QString& text, int row, int column, int width,
                                          void (BranchWatchProxyModel::*slot)(const QString&)) {
      QLineEdit* line_edit = new QLineEdit;
      layout->addWidget(line_edit, row, column, 1, width);
      connect(line_edit, &QLineEdit::textChanged, [this, slot](const QString& text) {
        (m_table_proxy->*slot)(text);
        UpdateStatus();
      });
      line_edit->setPlaceholderText(text);
      return line_edit;
    };

    // clang-format off
    InstallLineEdit(tr("Symbol Name"    ), 0, 0, 2, &BranchWatchProxyModel::OnSymbolTextChanged);
    InstallLineEdit(tr("Origin Min"     ), 1, 0, 1, &BranchWatchProxyModel::OnAddressTextChanged<&BranchWatchProxyModel::m_origin_min>)->setMaxLength(8);
    InstallLineEdit(tr("Origin Max"     ), 1, 1, 1, &BranchWatchProxyModel::OnAddressTextChanged<&BranchWatchProxyModel::m_origin_max>)->setMaxLength(8);
    InstallLineEdit(tr("Destination Min"), 2, 0, 1, &BranchWatchProxyModel::OnAddressTextChanged<&BranchWatchProxyModel::m_destin_min>)->setMaxLength(8);
    InstallLineEdit(tr("Destination Max"), 2, 1, 1, &BranchWatchProxyModel::OnAddressTextChanged<&BranchWatchProxyModel::m_destin_max>)->setMaxLength(8);
    // clang-format on

    auto* group_box = new QGroupBox(tr("Other Filter Options"));
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    return group_box;
  }());

  // Misc. Branch Watch Controls
  m_control_toolbar->addWidget([this]() {
    auto* layout = new QVBoxLayout;

    layout->addWidget(m_btn_was_overwritten = new QPushButton(tr("Branch Was Overwritten")));
    connect(m_btn_was_overwritten, &QPushButton::pressed, this,
            &BranchWatchDialog::OnBranchWasOverwritten);
    m_btn_was_overwritten->setEnabled(false);

    layout->addWidget(m_btn_not_overwritten = new QPushButton(tr("Branch Not Overwritten")));
    connect(m_btn_not_overwritten, &QPushButton::pressed, this,
            &BranchWatchDialog::OnBranchNotOverwritten);
    m_btn_not_overwritten->setEnabled(false);

    layout->addWidget(m_btn_clear_selection = new QPushButton(tr("Restart Without Clear")));
    connect(m_btn_clear_selection, &QPushButton::pressed, this,
            &BranchWatchDialog::OnRestartSelection);
    m_btn_clear_selection->setEnabled(false);

    auto* group_box = new QGroupBox(tr("Misc. Branch Watch Controls"));
    group_box->setLayout(layout);
    group_box->setAlignment(Qt::AlignHCenter);

    return group_box;
  }());

  connect(m_timer = new QTimer, &QTimer::timeout, this, &BranchWatchDialog::OnTimeout);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &BranchWatchDialog::OnEmulationStateChanged);
  connect(m_table_proxy, &BranchWatchProxyModel::layoutChanged, this,
          &BranchWatchDialog::UpdateStatus);

  return layout;
}

void BranchWatchDialog::OnStartStop(bool checked)
{
  if (checked)
    StartRecording();
  else
    StopRecording();
}

void BranchWatchDialog::OnPauseResume(bool checked)
{
  if (checked)
    PauseRecording();
  else
    ResumeRecording();
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
      this, tr("Save Branch Watch snapshot"),
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
      this, tr("Load Branch Watch snapshot"),
      QString::fromStdString(File::GetUserPath(D_DUMPDEBUG_BRANCHWATCH_IDX)),
      tr("Text file (*.txt);;All Files (*)"), nullptr, QFileDialog::Option::ReadOnly);
  if (filepath.isEmpty())
    return;

  Load(Core::CPUThreadGuard{m_system}, filepath.toStdString());
}

void BranchWatchDialog::OnBranchHasExecuted()
{
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnBranchHasExecuted(guard);
    AutoSave(guard);
  }
  UpdateStatus();
}

void BranchWatchDialog::OnBranchNotExecuted()
{
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnBranchNotExecuted(guard);
    AutoSave(guard);
  }
  UpdateStatus();
}

void BranchWatchDialog::OnBranchWasOverwritten()
{
  if (Core::GetState() == Core::State::Uninitialized)
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Core is uninitialized."));
    return;
  }
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnBranchWasOverwritten(guard);
    AutoSave(guard);
  }
  UpdateStatus();
}

void BranchWatchDialog::OnBranchNotOverwritten()
{
  if (Core::GetState() == Core::State::Uninitialized)
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Core is uninitialized."));
    return;
  }
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnBranchNotOverwritten(guard);
    AutoSave(guard);
  }
  UpdateStatus();
}

void BranchWatchDialog::OnRestartSelection()
{
  {
    const Core::CPUThreadGuard guard{m_system};
    m_table_model->OnRestartSelection(guard);
    AutoSave(guard);
  }
  UpdateStatus();
}

void BranchWatchDialog::OnTimeout()
{
  Update();
}

void BranchWatchDialog::OnEmulationStateChanged(Core::State new_state)
{
  if (!isVisible())
    return;

  if (TimerCondition(m_branch_watch, new_state))
    m_timer->start(CODE_DIFF_TIMER_DELAY_MS);
  else if (m_timer->isActive())
    m_timer->stop();
  Update();
}

void BranchWatchDialog::OnHelp()
{
  ModalMessageBox::information(
      this, tr("Branch Watch Tool Help (1/4)"),
      tr("Branch Watch is a code-searching tool that can isolate branches tracked by the emulated "
         "CPU by testing candidates with simple criteria. If you are familiar with Cheat Engine's "
         "Ultimap, Branch Watch is similar to that. Press the \"Start Recording\" button to turn "
         "on Branch Watch. Branch Watch persists across emulation sessions, and a snapshot of your "
         "progress can be saved to and loaded from the User Directory to persist after Dolphin "
         "Emulator is closed. \"Save As...\" and \"Load From...\" actions are also available, and "
         "auto-saving can be enabled to save a snapshot at every step of the search. The \"Pause "
         "Recording\" and \"Resume Recording\" buttons will pause Branch Watch from tracking "
         "further branch hits until it is told to resume. Press the \"Stop Recording\" button to "
         "start over."));
  ModalMessageBox::information(
      this, tr("Branch Watch Tool Help (2/4)"),
      tr("Branch Watch starts in the blacklist phase, meaning no candidates have been chosen yet, "
         "but all candidates found so far can be excluded from the candidacy by pressing the "
         "\"Branch Not Executed\", \"Branch Was Overwritten\", and \"Branch Not Overwritten\" "
         "buttons. Once the \"Branch Has Executed\" button is pressed for the first time, Branch "
         "Watch will switch to the reduction phase, and the table will populate with all eligible "
         "candidates."));
  ModalMessageBox::information(
      this, tr("Branch Watch Tool Help (3/4)"),
      tr("Once in the reduction phase, it is time to start narrowing down the candidates shown in "
         "the table. Further reduce the candidates by checking whether a branch has or has not "
         "executed since the last time it was checked. It is also possible to reduce the "
         "candidates by determining whether a branch instruction has or has not been overwritten "
         "since it was first hit. Filter the candidates by branch kind, origin or destination "
         "address, and symbol name. After enough passes and experimentation, you may be able to "
         "find function calls and conditional branches that only execute when an action is "
         "performed in the emulated software."));
  ModalMessageBox::information(
      this, tr("Branch Watch Tool Help (4/4)"),
      tr("Rows in the table can be left-clicked on the origin, destination, and symbol columns to "
         "view the associated address in Code View. Right-clicking a row will bring up a context "
         "menu. If the origin column of a row is right-clicked, an action to replace the branch "
         "instruction with a NOP instruction (no operation), and an action to copy the address to "
         "the clipboard will be available. If the destination column of a row is right-clicked, an "
         "action to replace the instruction at the destination with a BLR instruction (branch to "
         "link register) will be available, but only if the branch instruction at the origin "
         "updates the link register, and an action to copy the address to the clipboard will be "
         "available. If the symbol column of a row is right-clicked, the action to replace the "
         "instruction at the start of the symbol with a BLR instruction will be available, but "
         "only if a symbol enveloping the origin address is found. All context menus have the "
         "action to delete that row from the candidates. If multiple rows are selected, the only "
         "action in any context menu will be to delete all selected rows from the candidates."));
}

void BranchWatchDialog::OnToggleAutoSave(bool checked)
{
  if (!checked)
    return;

  const QString filepath = DolphinFileDialog::getSaveFileName(
      this, tr("Select Branch Watch snapshot auto-save file (for user folder location, cancel)"),
      QString::fromStdString(File::GetUserPath(D_DUMPDEBUG_BRANCHWATCH_IDX)),
      tr("Text file (*.txt);;All Files (*)"));
  if (filepath.isEmpty())
    m_autosave_filepath = std::nullopt;
  else
    m_autosave_filepath = filepath.toStdString();
}

void BranchWatchDialog::OnHideShowControls(bool checked)
{
  if (checked)
    m_control_toolbar->hide();
  else
    m_control_toolbar->show();
}

void BranchWatchDialog::StartRecording()
{
  if (!m_branch_watch.Start())
  {
    ModalMessageBox::information(this, tr("Branch Watch Tool"), tr("Failed to start recording."));
    m_btn_start_stop->setChecked(false);
    return;
  }
  m_btn_pause_resume->setEnabled(true);
  m_btn_clear_selection->setEnabled(true);
  m_btn_has_executed->setEnabled(true);
  m_btn_not_executed->setEnabled(true);
  m_btn_was_overwritten->setEnabled(true);
  m_btn_not_overwritten->setEnabled(true);
  m_btn_start_stop->setText(tr("Stop Recording"));
  if (Core::GetState() > Core::State::Paused)
    m_timer->start(CODE_DIFF_TIMER_DELAY_MS);
  Update();
}

void BranchWatchDialog::StopRecording()
{
  m_table_model->layoutAboutToBeChanged();
  m_branch_watch.Stop();
  m_table_model->layoutChanged();
  m_btn_pause_resume->setChecked(false);
  m_btn_pause_resume->setEnabled(false);
  m_btn_clear_selection->setEnabled(false);
  m_btn_has_executed->setEnabled(false);
  m_btn_not_executed->setEnabled(false);
  m_btn_was_overwritten->setEnabled(false);
  m_btn_not_overwritten->setEnabled(false);
  m_btn_start_stop->setText(tr("Start Recording"));
  if (m_timer->isActive())
    m_timer->stop();
  Update();
}

void BranchWatchDialog::PauseRecording()
{
  m_branch_watch.Pause();
  m_btn_pause_resume->setText(tr("Resume Recording"));
  if (Core::GetState() > Core::State::Paused)
  {
    // Schedule one last update in case Branch Watch in the middle of a hit.
    m_timer->setInterval(CODE_DIFF_TIMER_PAUSE_ONESHOT_MS);
    m_timer->setSingleShot(true);
  }
  Update();
}

void BranchWatchDialog::ResumeRecording()
{
  m_branch_watch.Resume();
  m_btn_pause_resume->setText(tr("Pause Recording"));
  if (Core::GetState() > Core::State::Paused)
  {
    m_timer->setSingleShot(false);
    m_timer->start(CODE_DIFF_TIMER_DELAY_MS);
  }
  Update();
}

void BranchWatchDialog::Update()
{
  if (m_branch_watch.GetRecordingPhase() == Core::BranchWatch::Phase::Blacklist)
    UpdateStatus();
  m_table_model->UpdateHits();
}

void BranchWatchDialog::UpdateSymbols()
{
  m_table_model->UpdateSymbols();
}

void BranchWatchDialog::UpdateStatus()
{
  switch (m_branch_watch.GetRecordingPhase())
  {
  case Core::BranchWatch::Phase::Disabled:
    m_status_bar->showMessage(tr("Ready"));
    return;
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

  m_table_model->Save(guard, file);
}

void BranchWatchDialog::Load(const Core::CPUThreadGuard& guard, const std::string& filepath)
{
  File::IOFile file(filepath, "r");
  if (!file.IsOpen())
  {
    ModalMessageBox::warning(
        this, tr("Error"),
        tr("Failed to load Branch Watch snapshot \"%1\"").arg(QString::fromStdString(filepath)));
    return;
  }

  if (!m_btn_start_stop->isChecked())
  {
    m_btn_start_stop->setChecked(true);
    if (m_act_autopause->isChecked())
      m_btn_pause_resume->setChecked(true);
  }
  m_table_model->Load(guard, file);
}

void BranchWatchDialog::AutoSave(const Core::CPUThreadGuard& guard)
{
  if (!m_act_autosave->isChecked() || !m_branch_watch.CanSave())
    return;
  Save(guard, m_autosave_filepath ? m_autosave_filepath.value() : GetSnapshotDefaultFilepath());
}

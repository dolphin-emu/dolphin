// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/CodeDiffDialog.h"

#include <chrono>
#include <regex>
#include <vector>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

#include "DolphinQt/Debugger/CodeWidget.h"

CodeDiffDialog::CodeDiffDialog(CodeWidget* parent) : QDialog(parent), m_code_widget(parent)
{
  setWindowTitle(tr("Diff"));
  CreateWidgets();
  ConnectWidgets();
  InfoDisp();
}

void CodeDiffDialog::reject()
{
  // Triggered on window close. Make sure to free memory and reset info message.
  ClearData();
  InfoDisp(); 
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("diffdialog/geometry"), saveGeometry());
  QDialog::reject();
}

void CodeDiffDialog::CreateWidgets()
{
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("diffdialog/geometry")).toByteArray());
  auto* btns_layout = new QGridLayout;
  m_exclude_btn = new QPushButton(tr("Code did not get executed"));
  m_include_btn = new QPushButton(tr("Code has been executed"));
  m_record_btn = new QPushButton(tr("Record functions"));
  m_record_btn->setCheckable(true);
  m_record_btn->setStyleSheet(
      QStringLiteral("QPushButton:checked { background-color: rgb(150, 0, 0); border-style: solid; "
                     "border-width: 3px; border-color: rgb(150,0,0); color: rgb(255, 255, 255);}"));

  btns_layout->addWidget(m_exclude_btn, 0, 0);
  btns_layout->addWidget(m_include_btn, 0, 1);
  btns_layout->addWidget(m_record_btn, 0, 2);

  auto* labels_layout = new QHBoxLayout;
  m_exclude_size_label = new QLabel(tr("Excluded"));
  m_include_size_label = new QLabel(tr("Included"));

  btns_layout->addWidget(m_exclude_size_label, 1, 0);
  btns_layout->addWidget(m_include_size_label, 1, 1);

  m_output_list = new QListWidget();
  m_output_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_output_list->setContextMenuPolicy(Qt::CustomContextMenu);
  m_reset_btn = new QPushButton(tr("Reset All"));
  m_reset_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  auto* layout = new QVBoxLayout();
  layout->addLayout(btns_layout);
  layout->addLayout(labels_layout);
  layout->addWidget(m_output_list);
  layout->addWidget(m_reset_btn);

  setLayout(layout);
}

void CodeDiffDialog::ConnectWidgets()
{
  connect(m_record_btn, &QPushButton::toggled, this, &CodeDiffDialog::OnRecord);
  connect(m_include_btn, &QPushButton::pressed, [this]() { Update(true); });
  connect(m_exclude_btn, &QPushButton::pressed, [this]() { Update(false); });
  connect(m_output_list, &QListWidget::itemClicked, [this](QListWidgetItem* item) {
    m_code_widget->SetAddress(item->data(Qt::UserRole).toUInt(),
                              CodeViewWidget::SetAddressUpdate::WithUpdate);
  });
  connect(m_reset_btn, &QPushButton::pressed, this, &CodeDiffDialog::ClearData);
  connect(m_output_list, &CodeDiffDialog::customContextMenuRequested, this,
          &CodeDiffDialog::OnContextMenu);
}

void CodeDiffDialog::ClearData()
{
  if (m_record_btn->isChecked())
    m_record_btn->toggle();
  ClearBlockCache();
  m_output_list->clear();
  std::vector<Diff>().swap(m_include);
  std::vector<Diff>().swap(m_exclude);
  JitInterface::SetProfilingState(JitInterface::ProfilingState::Disabled);
}

void CodeDiffDialog::ClearBlockCache()
{
  Core::State old_state = Core::GetState();

  if (old_state == Core::State::Running)
    Core::SetState(Core::State::Paused);

  JitInterface::ClearCache();

  if (old_state == Core::State::Running)
    Core::SetState(Core::State::Running);
}

void CodeDiffDialog::OnRecord(bool enabled)
{
  JitInterface::ProfilingState state;

  if (enabled)
  {
    ClearBlockCache();
    m_record_btn->setText(tr("Recording..."));
    state = JitInterface::ProfilingState::Enabled;
  }
  else
  {
    ClearBlockCache();
    m_record_btn->setText(tr("Start Recording"));
    state = JitInterface::ProfilingState::Disabled;
  }

  m_record_btn->update();
  JitInterface::SetProfilingState(state);
}

void CodeDiffDialog::OnIncludeExclude(bool include)
{
  bool isize = m_include.size() != 0;
  bool xsize = m_exclude.size() != 0;
  std::vector<Diff> current_diff;
  Profiler::ProfileStats prof_stats;
  auto& blockstats = prof_stats.block_stats;
  JitInterface::GetProfileResults(&prof_stats);
  std::vector<Diff> current;
  current.reserve(20000);

  // Convert blockstats to smaller struct Diff. Exclude repeat functions via symbols.
  for (auto& iter : blockstats)
  {
    Diff tmp_diff;
    std::string symbol = g_symbolDB.GetDescription(iter.addr);
    if (!std::any_of(current.begin(), current.end(),
                     [&symbol](Diff& v) { return v.symbol == symbol; }))
    {
      tmp_diff.symbol = symbol;
      tmp_diff.addr = iter.addr;
      tmp_diff.hits = iter.run_count;
      current.push_back(tmp_diff);
    }
  }

  // Could add address based difference instead of symbols. Probably need second function.
  // Sort for lower_bound.
  sort(current.begin(), current.end(),
       [](const Diff& v1, const Diff& v2) { return (v1.symbol < v2.symbol); });

  // If both lists are empty, write and skip.
  if (!isize && !xsize)
  {
    if (include)
      m_include = current;
    else
      m_exclude = current;
    return;
  }

  // Update exclude list. Compare to exclude list if it exists and make current_diff to check
  // against include.
  if (!xsize && !include)
  {
    m_exclude = current;
  }
  else if (xsize)
  {
    for (auto& iter : current)
    {
      auto pos = lower_bound(m_exclude.begin(), m_exclude.end(), iter.symbol, AddrOP);

      if (pos->symbol != iter.symbol)
      {
        current_diff.push_back(iter);

        if (!include)
          m_exclude.insert(pos, iter);
      }
    }
    // If there is no include list, we're done.
    if (!include && !isize)
      return;
  }
  else
  {
    current_diff = current;
  }

  // Update include list.
  if (include && isize)
  {
    // Compare include with current and remove items that aren't in both.
    std::vector<Diff> tmp_swap;
    for (auto& iter : m_include)
    {
      if (std::any_of(current_diff.begin(), current_diff.end(),
                      [&](Diff& v) { return v.symbol == iter.symbol; }))
        tmp_swap.push_back(iter);
    }
    m_include.swap(tmp_swap);
  }
  else if ((!isize && include) || (!xsize && !include))
  {
    // If both lists are about to be filled for the first time, compare full lists and remove from
    // include list.
    if (include)
      m_include = current;

    for (auto& list : m_exclude)
    {
      m_include.erase(std::remove_if(m_include.begin(), m_include.end(),
                                     [&](Diff const& v) { return v.symbol == list.symbol; }),
                      m_include.end());
    }
  }
  else
  {
    // If exclude pressed, remove new exclude items from include list that match.
    for (auto& list : current_diff)
    {
      m_include.erase(std::remove_if(m_include.begin(), m_include.end(),
                                     [&](Diff& v) { return v.symbol == list.symbol; }),
                      m_include.end());
    }
  }
}

void CodeDiffDialog::Update(bool include)
{
  // Wrap everything in a pause
  Core::State old_state = Core::GetState();
  if (old_state == Core::State::Running)
    Core::SetState(Core::State::Paused);

  // Main process
  OnIncludeExclude(include);

  m_output_list->clear();

  new QListWidgetItem(tr("Address\tHits\tSymbol"), m_output_list);

  for (auto& iter : m_include)
  {
    QString fix_sym = QString::fromStdString(iter.symbol);
    fix_sym.replace(QStringLiteral("\t"), QStringLiteral("  "));

    QString tmp_out =
        QString::fromStdString(StringFromFormat("%08x\t%i\t", iter.addr, iter.hits)) + fix_sym;

    auto* item = new QListWidgetItem(tmp_out, m_output_list);
    item->setData(Qt::UserRole, iter.addr);

    m_output_list->addItem(item);
  }
  m_exclude_size_label->setText(QString::number(m_exclude.size()));
  m_include_size_label->setText(QString::number(m_include.size()));

  JitInterface::ClearCache();
  if (old_state == Core::State::Running)
    Core::SetState(Core::State::Running);
}

void CodeDiffDialog::InfoDisp()
{
  new QListWidgetItem(
      QStringLiteral(
          "Used to find functions based on when they should be running.\n\nRecord Functions: will "
          "keep track of what functions run. Stop and restart to reset current "
          "recording.\nExclude: will add recorded functions to an excluded "
          "list, then reset the recording list.\nInclude: will add"
          "recorded function to an include list, then reset the recording list.\nAfter you use "
          "both "
          "exclude and include once, the "
          "exclude list will be subtracted\n   from the include list and any includes left over "
          "will "
          "be displayed.\nYou can continue to use include/exclude to narrow down the "
          "results.\n\nExample: "
          "You want to find a function that runs when HP is modified.\n1.  Start recording and "
          "play "
          "the game without letting HP be modified, then press exclude.\n2.  Immediately "
          "gain/lose HP and hit include.\n3.  Repeat 1 or 2 to narrow down the results.\nIncludes "
          "should "
          "have short recordings focusing on what you want.\nAlso note, pressing include multiple "
          "times will never increase the include list's size.\n   Pressing include twice will only "
          "keep functions that ran for both recordings.\n\nRight click -> blr will attempt to skip "
          "the function by placing a blr at the top of the symbol."),
      m_output_list);
}

void CodeDiffDialog::OnContextMenu()
{
  QMenu* menu = new QMenu(this);
  menu->addAction(tr("&Go to start of function"), this, &CodeDiffDialog::OnGoTop);
  menu->addAction(tr("Toggle &blr"), this, &CodeDiffDialog::OnToggleBLR);
  menu->addAction(tr("&Delete"), this, &CodeDiffDialog::OnDelete);
  menu->exec(QCursor::pos());
}

void CodeDiffDialog::OnGoTop()
{
  auto item = m_output_list->currentItem();
  Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(item->data(Qt::UserRole).toUInt());
  m_code_widget->SetAddress(symbol->address, CodeViewWidget::SetAddressUpdate::WithUpdate);
}

void CodeDiffDialog::OnDelete()
{
  // Delete from include and listwidget.
  int remove_item = m_output_list->row(m_output_list->currentItem());
  m_include.erase(m_include.begin() + remove_item - 1);
  m_output_list->takeItem(remove_item);
}

void CodeDiffDialog::OnToggleBLR()
{
  // Get address at top of function (hopefully) and blr it.
  auto item = m_output_list->currentItem();
  Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(item->data(Qt::UserRole).toUInt());

  PowerPC::debug_interface.UnsetPatch(symbol->address);

  if (PowerPC::HostRead_U32(symbol->address) != 0x4e800020)
  {
    PowerPC::debug_interface.SetPatch(symbol->address, 0x4e800020);
    item->setForeground(QBrush(Qt::red));
    m_code_widget->Update();
  }
  else
  {
    item->setForeground(QBrush(Qt::black));
  }
}

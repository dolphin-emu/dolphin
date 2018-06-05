// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Debugger/MemoryPatchWidget.h"

#include <iomanip>
#include <sstream>

#include <QHeaderView>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt2/QtUtils/ActionHelper.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"

MemoryPatchWidget::MemoryPatchWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Patches"));
  setObjectName(QStringLiteral("patches"));

  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("memorypatchwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("memorypatchwidget/floating")).toBool());

  CreateWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, [this](Core::State state) {
    if (!Settings::Instance().IsDebugModeEnabled())
      return;

    if (state == Core::State::Uninitialized)
    {
      PowerPC::debug_interface.ClearPatches();
      Update();
    }
  });

  connect(&Settings::Instance(), &Settings::MemoryPatchesVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsMemoryPatchesVisible());
  });

  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &MemoryPatchWidget::UpdateIcons);
  UpdateIcons();

  setHidden(!Settings::Instance().IsMemoryPatchesVisible() ||
            !Settings::Instance().IsDebugModeEnabled());

  Update();
}

MemoryPatchWidget::~MemoryPatchWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("memorypatchwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("memorypatchwidget/floating"), isFloating());
}

void MemoryPatchWidget::CreateWidgets()
{
  m_toolbar = new QToolBar;
  m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  m_table = new QTableWidget;
  m_table->setColumnCount(3);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->verticalHeader()->hide();

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_toolbar);
  layout->addWidget(m_table);

  m_delete = AddAction(m_toolbar, tr("Delete"), this, &MemoryPatchWidget::OnDelete);
  m_clear = AddAction(m_toolbar, tr("Clear"), this, &MemoryPatchWidget::OnClear);
  m_toggle_on_off = AddAction(m_toolbar, tr("On/Off"), this, &MemoryPatchWidget::OnToggleOnOff);

  QWidget* widget = new QWidget;
  widget->setLayout(layout);

  setWidget(widget);
}

void MemoryPatchWidget::UpdateIcons()
{
  m_delete->setIcon(Resources::GetScaledThemeIcon("debugger_delete"));
  m_clear->setIcon(Resources::GetScaledThemeIcon("debugger_clear"));
  m_toggle_on_off->setIcon(Resources::GetScaledThemeIcon("debugger_breakpoint"));
}

void MemoryPatchWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetMemoryPatchesVisible(false);
}

void MemoryPatchWidget::Update()
{
  m_table->clear();

  m_table->setHorizontalHeaderLabels({tr("Active"), tr("Address"), tr("Value")});

  int patch_id = 0;
  m_table->setRowCount(patch_id);

  auto create_item = [](const QString& string = QStringLiteral("")) {
    QTableWidgetItem* item = new QTableWidgetItem(string);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
  };

  for (const auto& patch : PowerPC::debug_interface.GetPatches())
  {
    m_table->setRowCount(patch_id + 1);

    const QString is_enabled =
        (patch.is_enabled == Common::Debug::MemoryPatch::State::Enabled) ? tr("on") : tr("off");
    auto* active = create_item(is_enabled);
    active->setData(Qt::UserRole, patch_id);
    m_table->setItem(patch_id, 0, active);

    const QString address = QStringLiteral("%1").arg(patch.address, 8, 16, QLatin1Char('0'));
    m_table->setItem(patch_id, 1, create_item(address));

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (u8 b : patch.value)
    {
      oss << std::setw(2) << static_cast<u32>(b);
    }
    m_table->setItem(patch_id, 2, create_item(QString::fromStdString(oss.str())));

    patch_id += 1;
  }
}

void MemoryPatchWidget::OnDelete()
{
  if (m_table->selectedItems().empty())
    return;

  const auto patch_id = m_table->selectedItems()[0]->data(Qt::UserRole).toUInt();

  PowerPC::debug_interface.RemovePatch(patch_id);

  emit MemoryPatchesChanged();
  Update();
}

void MemoryPatchWidget::OnClear()
{
  PowerPC::debug_interface.ClearPatches();

  m_table->setRowCount(0);

  emit MemoryPatchesChanged();
  Update();
}

void MemoryPatchWidget::OnToggleOnOff()
{
  if (m_table->selectedItems().empty())
    return;

  const auto patch_id = m_table->selectedItems()[0]->data(Qt::UserRole).toUInt();
  const auto& patch = PowerPC::debug_interface.GetPatch(patch_id);
  if (patch.is_enabled == Common::Debug::MemoryPatch::State::Enabled)
    PowerPC::debug_interface.DisablePatch(patch_id);
  else
    PowerPC::debug_interface.EnablePatch(patch_id);

  emit MemoryPatchesChanged();
  Update();
}

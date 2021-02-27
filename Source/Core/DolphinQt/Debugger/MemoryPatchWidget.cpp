// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/MemoryPatchWidget.h"

#include <iomanip>
#include <sstream>

#include <QFileDialog>
#include <QHeaderView>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

MemoryPatchWidget::MemoryPatchWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Patches"));
  setObjectName(QStringLiteral("patches"));

  setAllowedAreas(Qt::AllDockWidgetAreas);

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("memorypatchwidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("memorypatchwidget/floating")).toBool());

  CreateWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    if (!Settings::Instance().IsDebugModeEnabled())
      return;

    UpdateButtonsEnabled();

    if (state == Core::State::Uninitialized)
    {
      PowerPC::debug_interface.ClearPatches();
      Update();
    }
  });

  connect(&Settings::Instance(), &Settings::MemoryPatchesVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, [this](bool enabled) {
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
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

  const QStringList header{{tr("Active"), tr("Address"), tr("Patch value"), tr("Original value")}};
  m_table->setColumnCount(header.size());
  m_table->setHorizontalHeaderLabels(header);
  m_table->verticalHeader()->hide();

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_toolbar);
  layout->addWidget(m_table);

  m_toggle_on_off = m_toolbar->addAction(tr("On/Off"), this, &MemoryPatchWidget::OnToggleOnOff);
  m_delete = m_toolbar->addAction(tr("Delete"), this, &MemoryPatchWidget::OnDelete);
  m_clear = m_toolbar->addAction(tr("Clear"), this, &MemoryPatchWidget::OnClear);
  m_load = m_toolbar->addAction(tr("Load from file"), this, &MemoryPatchWidget::OnLoadFromFile);
  m_save = m_toolbar->addAction(tr("Save to file"), this, &MemoryPatchWidget::OnSaveToFile);

  QWidget* widget = new QWidget;
  widget->setLayout(layout);

  setWidget(widget);
}

void MemoryPatchWidget::UpdateIcons()
{
  // TODO: Create a "debugger_toggle_on_off" icon
  m_toggle_on_off->setIcon(Resources::GetScaledThemeIcon("debugger_breakpoint"));
  m_delete->setIcon(Resources::GetScaledThemeIcon("debugger_delete"));
  m_clear->setIcon(Resources::GetScaledThemeIcon("debugger_clear"));
  m_load->setIcon(Resources::GetScaledThemeIcon("debugger_load"));
  m_save->setIcon(Resources::GetScaledThemeIcon("debugger_save"));
}

void MemoryPatchWidget::UpdateButtonsEnabled()
{
  if (!isVisible())
    return;

  const bool is_enabled = Core::IsRunning();
  m_toggle_on_off->setEnabled(is_enabled);
  m_delete->setEnabled(is_enabled);
  m_clear->setEnabled(is_enabled);
  m_load->setEnabled(is_enabled);
  m_save->setEnabled(is_enabled);
}

void MemoryPatchWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetMemoryPatchesVisible(false);
}

void MemoryPatchWidget::showEvent(QShowEvent* event)
{
  UpdateButtonsEnabled();
}

void MemoryPatchWidget::Update()
{
  m_table->clearContents();

  int patch_id = 0;
  m_table->setRowCount(patch_id);

  auto create_item = [](const QString& string = QString()) {
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
    for (u8 b : patch.patch_value)
    {
      oss << std::setw(2) << static_cast<u32>(b);
    }
    m_table->setItem(patch_id, 2, create_item(QString::fromStdString(oss.str())));

    oss.str("");
    for (u8 b : patch.original_value)
    {
      oss << std::setw(2) << static_cast<u32>(b);
    }
    m_table->setItem(patch_id, 3, create_item(QString::fromStdString(oss.str())));

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

void MemoryPatchWidget::OnLoadFromFile()
{
  const QString path = QFileDialog::getOpenFileName(this, tr("Load patch file"), QString(),
                                                    tr("Dolphin Patch File (*.patch)"));

  if (path.isEmpty())
    return;

  std::ifstream ifs;
  File::OpenFStream(ifs, path.toStdString(), std::ios_base::in);
  if (!ifs)
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Failed to load patch file '%1'").arg(path));
    return;
  }

  std::string line;
  std::vector<std::string> patches;
  while (std::getline(ifs, line))
    patches.push_back(line);
  ifs.close();

  PowerPC::debug_interface.ClearPatches();
  PowerPC::debug_interface.LoadPatchesFromStrings(patches);

  emit MemoryPatchesChanged();
  Update();
}

void MemoryPatchWidget::OnSaveToFile()
{
  const QString path = QFileDialog::getSaveFileName(this, tr("Save patch file"), QString(),
                                                    tr("Dolphin Patch File (*.patch)"));

  if (path.isEmpty())
    return;

  std::ofstream out;
  File::OpenFStream(out, path.toStdString(), std::ios::out);
  if (!out)
  {
    ModalMessageBox::warning(this, tr("Error"),
                             tr("Failed to save patch file to path '%1'").arg(path));
  }

  for (auto& patch : PowerPC::debug_interface.SavePatchesToStrings())
    out << patch << std::endl;
  out.close();
}

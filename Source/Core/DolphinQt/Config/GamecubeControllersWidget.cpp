// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GamecubeControllersWidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <optional>
#include <utility>
#include <vector>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"

#include "DolphinQt/Config/Mapping/GCPadWiiUConfigDialog.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/GCAdapter.h"

static const std::vector<std::pair<SerialInterface::SIDevices, const char*>> s_gc_types = {
    {SerialInterface::SIDEVICE_NONE, _trans("None")},
    {SerialInterface::SIDEVICE_GC_CONTROLLER, _trans("Standard Controller")},
    {SerialInterface::SIDEVICE_WIIU_ADAPTER, _trans("GameCube Adapter for Wii U")},
    {SerialInterface::SIDEVICE_GC_STEERING, _trans("Steering Wheel")},
    {SerialInterface::SIDEVICE_DANCEMAT, _trans("Dance Mat")},
    {SerialInterface::SIDEVICE_GC_TARUKONGA, _trans("DK Bongos")},
#ifdef HAS_LIBMGBA
    {SerialInterface::SIDEVICE_GC_GBA_EMULATED, _trans("GBA (Integrated)")},
#endif
    {SerialInterface::SIDEVICE_GC_GBA, _trans("GBA (TCP)")},
    {SerialInterface::SIDEVICE_GC_KEYBOARD, _trans("Keyboard")}};

static std::optional<int> ToGCMenuIndex(const SerialInterface::SIDevices sidevice)
{
  for (size_t i = 0; i < s_gc_types.size(); ++i)
  {
    if (s_gc_types[i].first == sidevice)
      return static_cast<int>(i);
  }
  return {};
}

static SerialInterface::SIDevices FromGCMenuIndex(const int menudevice)
{
  return s_gc_types[menudevice].first;
}

static bool IsConfigurable(SerialInterface::SIDevices sidevice)
{
  return sidevice != SerialInterface::SIDEVICE_NONE && sidevice != SerialInterface::SIDEVICE_GC_GBA;
}

GamecubeControllersWidget::GamecubeControllersWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();
}

void GamecubeControllersWidget::CreateLayout()
{
  m_gc_box = new QGroupBox(tr("GameCube Controllers"));
  m_gc_layout = new QGridLayout();
  m_gc_layout->setVerticalSpacing(7);
  m_gc_layout->setColumnStretch(1, 1);

  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    auto* gc_label = new QLabel(tr("Port %1").arg(i + 1));
    auto* gc_box = m_gc_controller_boxes[i] = new QComboBox();
    auto* gc_button = m_gc_buttons[i] = new QPushButton(tr("Configure"));

    for (const auto& item : s_gc_types)
    {
      gc_box->addItem(tr(item.second));
    }

    int controller_row = m_gc_layout->rowCount();
    m_gc_layout->addWidget(gc_label, controller_row, 0);
    m_gc_layout->addWidget(gc_box, controller_row, 1);
    m_gc_layout->addWidget(gc_button, controller_row, 2);
  }
  m_gc_box->setLayout(m_gc_layout);

  auto* layout = new QVBoxLayout;
  layout->setMargin(0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_gc_box);
  setLayout(layout);
}

void GamecubeControllersWidget::ConnectWidgets()
{
  for (size_t i = 0; i < m_gc_controller_boxes.size(); i++)
  {
    connect(m_gc_controller_boxes[i], qOverload<int>(&QComboBox::currentIndexChanged), this,
            &GamecubeControllersWidget::SaveSettings);
    connect(m_gc_controller_boxes[i], qOverload<int>(&QComboBox::currentIndexChanged), this,
            &GamecubeControllersWidget::OnGCTypeChanged);
    connect(m_gc_buttons[i], &QPushButton::clicked, this,
            &GamecubeControllersWidget::OnGCPadConfigure);
  }
}

void GamecubeControllersWidget::OnGCTypeChanged(int type)
{
  const auto* box = static_cast<QComboBox*>(QObject::sender());

  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    if (m_gc_controller_boxes[i] == box)
    {
      const SerialInterface::SIDevices si_device = FromGCMenuIndex(box->currentIndex());
      m_gc_buttons[i]->setEnabled(IsConfigurable(si_device));
      return;
    }
  }

  SaveSettings();
}

void GamecubeControllersWidget::OnGCPadConfigure()
{
  size_t index;
  for (index = 0; index < m_gc_groups.size(); index++)
  {
    if (m_gc_buttons[index] == QObject::sender())
      break;
  }

  MappingWindow::Type type;

  switch (FromGCMenuIndex(m_gc_controller_boxes[index]->currentIndex()))
  {
  case SerialInterface::SIDEVICE_NONE:
  case SerialInterface::SIDEVICE_GC_GBA:
    return;
  case SerialInterface::SIDEVICE_GC_CONTROLLER:
    type = MappingWindow::Type::MAPPING_GCPAD;
    break;
  case SerialInterface::SIDEVICE_WIIU_ADAPTER:
    GCPadWiiUConfigDialog(static_cast<int>(index), this).exec();
    return;
  case SerialInterface::SIDEVICE_GC_STEERING:
    type = MappingWindow::Type::MAPPING_GC_STEERINGWHEEL;
    break;
  case SerialInterface::SIDEVICE_DANCEMAT:
    type = MappingWindow::Type::MAPPING_GC_DANCEMAT;
    break;
  case SerialInterface::SIDEVICE_GC_TARUKONGA:
    type = MappingWindow::Type::MAPPING_GC_BONGOS;
    break;
  case SerialInterface::SIDEVICE_GC_GBA_EMULATED:
    type = MappingWindow::Type::MAPPING_GC_GBA;
    break;
  case SerialInterface::SIDEVICE_GC_KEYBOARD:
    type = MappingWindow::Type::MAPPING_GC_KEYBOARD;
    break;
  default:
    return;
  }

  MappingWindow* window = new MappingWindow(this, type, static_cast<int>(index));
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void GamecubeControllersWidget::LoadSettings()
{
  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    const SerialInterface::SIDevices si_device =
        Config::Get(Config::GetInfoForSIDevice(static_cast<int>(i)));
    const std::optional<int> gc_index = ToGCMenuIndex(si_device);
    if (gc_index)
    {
      m_gc_controller_boxes[i]->setCurrentIndex(*gc_index);
      m_gc_buttons[i]->setEnabled(IsConfigurable(si_device));
    }
  }
}

void GamecubeControllersWidget::SaveSettings()
{
  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    const int index = m_gc_controller_boxes[i]->currentIndex();
    const SerialInterface::SIDevices si_device = FromGCMenuIndex(index);
    Config::SetBaseOrCurrent(Config::GetInfoForSIDevice(static_cast<int>(i)), si_device);

    if (Core::IsRunning())
      SerialInterface::ChangeDevice(si_device, static_cast<s32>(i));

    m_gc_buttons[i]->setEnabled(IsConfigurable(si_device));
  }

  if (GCAdapter::UseAdapter())
    GCAdapter::StartScanThread();
  else
    GCAdapter::StopScanThread();

  SConfig::GetInstance().SaveSettings();
}

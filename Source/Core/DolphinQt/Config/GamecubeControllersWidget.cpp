// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/GamecubeControllersWidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <map>
#include <optional>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"

#include "DolphinQt/Config/Mapping/GCPadWiiUConfigDialog.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/GCAdapter.h"

static const std::map<SerialInterface::SIDevices, int> s_gc_types = {
    {SerialInterface::SIDEVICE_NONE, 0},         {SerialInterface::SIDEVICE_GC_CONTROLLER, 1},
    {SerialInterface::SIDEVICE_WIIU_ADAPTER, 2}, {SerialInterface::SIDEVICE_GC_STEERING, 3},
    {SerialInterface::SIDEVICE_DANCEMAT, 4},     {SerialInterface::SIDEVICE_GC_TARUKONGA, 5},
    {SerialInterface::SIDEVICE_GC_GBA, 6},       {SerialInterface::SIDEVICE_GC_KEYBOARD, 7}};

static std::optional<int> ToGCMenuIndex(const SerialInterface::SIDevices sidevice)
{
  auto it = s_gc_types.find(sidevice);
  return it != s_gc_types.end() ? it->second : std::optional<int>();
}

static std::optional<SerialInterface::SIDevices> FromGCMenuIndex(const int menudevice)
{
  auto it = std::find_if(s_gc_types.begin(), s_gc_types.end(),
                         [=](auto pair) { return pair.second == menudevice; });
  return it != s_gc_types.end() ? it->first : std::optional<SerialInterface::SIDevices>();
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

    for (const auto& item :
         {tr("None"), tr("Standard Controller"), tr("GameCube Adapter for Wii U"),
          tr("Steering Wheel"), tr("Dance Mat"), tr("DK Bongos"), tr("GBA"), tr("Keyboard")})
    {
      gc_box->addItem(item);
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
      const int index = box->currentIndex();
      m_gc_buttons[i]->setEnabled(index != 0 && index != 6);
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

  switch (m_gc_controller_boxes[index]->currentIndex())
  {
  case 0:  // None
  case 6:  // GBA
    return;
  case 1:  // Standard Controller
    type = MappingWindow::Type::MAPPING_GCPAD;
    break;
  case 2:  // GameCube Adapter for Wii U
    GCPadWiiUConfigDialog(static_cast<int>(index), this).exec();
    return;
  case 3:  // Steering Wheel
    type = MappingWindow::Type::MAPPING_GC_STEERINGWHEEL;
    break;
  case 4:  // Dance Mat
    type = MappingWindow::Type::MAPPING_GC_DANCEMAT;
    break;
  case 5:  // DK Bongos
    type = MappingWindow::Type::MAPPING_GC_BONGOS;
    break;
  case 7:  // Keyboard
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
    const std::optional<int> gc_index = ToGCMenuIndex(SConfig::GetInstance().m_SIDevice[i]);
    if (gc_index)
    {
      m_gc_controller_boxes[i]->setCurrentIndex(*gc_index);
      m_gc_buttons[i]->setEnabled(*gc_index != 0 && *gc_index != 6);
    }
  }
}

void GamecubeControllersWidget::SaveSettings()
{
  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    const int index = m_gc_controller_boxes[i]->currentIndex();
    const std::optional<SerialInterface::SIDevices> si_device = FromGCMenuIndex(index);
    if (si_device)
    {
      SConfig::GetInstance().m_SIDevice[i] = *si_device;

      if (Core::IsRunning())
        SerialInterface::ChangeDevice(*si_device, static_cast<s32>(i));
    }

    m_gc_buttons[i]->setEnabled(index != 0 && index != 6);
  }

  if (GCAdapter::UseAdapter())
    GCAdapter::StartScanThread();
  else
    GCAdapter::StopScanThread();

  SConfig::GetInstance().SaveSettings();
}

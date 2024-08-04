// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatSearchFactoryWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include <string>
#include <vector>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "Common/StringUtil.h"
#include "Core/CheatSearch.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"

CheatSearchFactoryWidget::CheatSearchFactoryWidget()
{
  CreateWidgets();
  ConnectWidgets();
  RefreshGui();
}

CheatSearchFactoryWidget::~CheatSearchFactoryWidget() = default;

Q_DECLARE_METATYPE(Cheats::DataType);

void CheatSearchFactoryWidget::CreateWidgets()
{
  auto* layout = new QVBoxLayout();

  auto* address_space_group = new QGroupBox(tr("Address Space"));
  auto* address_space_layout = new QVBoxLayout();
  address_space_group->setLayout(address_space_layout);

  m_standard_address_space = new QRadioButton(tr("Typical GameCube/Wii Address Space"));
  m_standard_address_space->setChecked(true);
  m_custom_address_space = new QRadioButton(tr("Custom Address Space"));

  QLabel* label_standard_address_space =
      new QLabel(tr("Sets up the search using standard MEM1 and (on Wii) MEM2 mappings in virtual "
                    "address space. This will work for the vast majority of games."));
  label_standard_address_space->setWordWrap(true);

  auto* custom_address_space_layout = new QVBoxLayout();
  custom_address_space_layout->setContentsMargins(6, 6, 6, 6);
  auto* custom_address_space_button_group = new QButtonGroup(this);
  m_custom_virtual_address_space = new QRadioButton(tr("Use virtual addresses when possible"));
  m_custom_virtual_address_space->setChecked(true);
  m_custom_physical_address_space = new QRadioButton(tr("Use physical addresses"));
  m_custom_effective_address_space =
      new QRadioButton(tr("Use memory mapper configuration at time of scan"));
  custom_address_space_button_group->addButton(m_custom_virtual_address_space);
  custom_address_space_button_group->addButton(m_custom_physical_address_space);
  custom_address_space_button_group->addButton(m_custom_effective_address_space);
  custom_address_space_layout->addWidget(m_custom_virtual_address_space);
  custom_address_space_layout->addWidget(m_custom_physical_address_space);
  custom_address_space_layout->addWidget(m_custom_effective_address_space);

  QLabel* label_range_start = new QLabel(tr("Range Start: "));
  m_custom_address_start = new QLineEdit(QStringLiteral("0x80000000"));
  QLabel* label_range_end = new QLabel(tr("Range End: "));
  m_custom_address_end = new QLineEdit(QStringLiteral("0x81800000"));
  custom_address_space_layout->addWidget(label_range_start);
  custom_address_space_layout->addWidget(m_custom_address_start);
  custom_address_space_layout->addWidget(label_range_end);
  custom_address_space_layout->addWidget(m_custom_address_end);

  address_space_layout->addWidget(m_standard_address_space);
  address_space_layout->addWidget(label_standard_address_space);
  address_space_layout->addWidget(m_custom_address_space);
  address_space_layout->addLayout(custom_address_space_layout);

  layout->addWidget(address_space_group);

  auto* data_type_group = new QGroupBox(tr("Data Type"));
  auto* data_type_layout = new QVBoxLayout();
  data_type_group->setLayout(data_type_layout);

  m_data_type_dropdown = new QComboBox();
  m_data_type_dropdown->addItem(tr("8-bit Unsigned Integer"),
                                QVariant::fromValue(Cheats::DataType::U8));
  m_data_type_dropdown->addItem(tr("16-bit Unsigned Integer"),
                                QVariant::fromValue(Cheats::DataType::U16));
  m_data_type_dropdown->addItem(tr("32-bit Unsigned Integer"),
                                QVariant::fromValue(Cheats::DataType::U32));
  m_data_type_dropdown->addItem(tr("64-bit Unsigned Integer"),
                                QVariant::fromValue(Cheats::DataType::U64));
  m_data_type_dropdown->addItem(tr("8-bit Signed Integer"),
                                QVariant::fromValue(Cheats::DataType::S8));
  m_data_type_dropdown->addItem(tr("16-bit Signed Integer"),
                                QVariant::fromValue(Cheats::DataType::S16));
  m_data_type_dropdown->addItem(tr("32-bit Signed Integer"),
                                QVariant::fromValue(Cheats::DataType::S32));
  m_data_type_dropdown->addItem(tr("64-bit Signed Integer"),
                                QVariant::fromValue(Cheats::DataType::S64));
  m_data_type_dropdown->addItem(tr("32-bit Float"), QVariant::fromValue(Cheats::DataType::F32));
  m_data_type_dropdown->addItem(tr("64-bit Float"), QVariant::fromValue(Cheats::DataType::F64));
  m_data_type_dropdown->setCurrentIndex(6);  // select 32bit signed int by default

  data_type_layout->addWidget(m_data_type_dropdown);

  m_data_type_aligned = new QCheckBox(tr("Aligned to data type length"));
  m_data_type_aligned->setChecked(true);

  data_type_layout->addWidget(m_data_type_aligned);

  layout->addWidget(data_type_group);

  m_new_search = new NonDefaultQPushButton(tr("New Search"));
  layout->addWidget(m_new_search);

  layout->addStretch();

  WrapInScrollArea(this, layout);
}

void CheatSearchFactoryWidget::ConnectWidgets()
{
  connect(m_new_search, &QPushButton::clicked, this, &CheatSearchFactoryWidget::OnNewSearchClicked);
  connect(m_standard_address_space, &QPushButton::toggled, this,
          &CheatSearchFactoryWidget::OnAddressSpaceRadioChanged);
  connect(m_custom_address_space, &QRadioButton::toggled, this,
          &CheatSearchFactoryWidget::OnAddressSpaceRadioChanged);
}

void CheatSearchFactoryWidget::RefreshGui()
{
  bool enable_custom = m_custom_address_space->isChecked();
  m_custom_virtual_address_space->setEnabled(enable_custom);
  m_custom_physical_address_space->setEnabled(enable_custom);
  m_custom_effective_address_space->setEnabled(enable_custom);
  m_custom_address_start->setEnabled(enable_custom);
  m_custom_address_end->setEnabled(enable_custom);
}

void CheatSearchFactoryWidget::OnAddressSpaceRadioChanged()
{
  RefreshGui();
}

void CheatSearchFactoryWidget::OnNewSearchClicked()
{
  std::vector<Cheats::MemoryRange> memory_ranges;
  PowerPC::RequestedAddressSpace address_space;
  if (m_standard_address_space->isChecked())
  {
    auto& system = Core::System::GetInstance();
    if (!Core::IsRunning(system))
    {
      ModalMessageBox::warning(
          this, tr("No game running."),
          tr("Please start a game before starting a search with standard memory regions."));
      return;
    }

    auto& memory = system.GetMemory();
    memory_ranges.emplace_back(0x80000000, memory.GetRamSizeReal());
    if (system.IsWii())
      memory_ranges.emplace_back(0x90000000, memory.GetExRamSizeReal());
    address_space = PowerPC::RequestedAddressSpace::Virtual;
  }
  else
  {
    const std::string address_start_str = m_custom_address_start->text().toStdString();
    const std::string address_end_str = m_custom_address_end->text().toStdString();

    u64 address_start;
    u64 address_end;
    if (!TryParse(address_start_str, &address_start) || !TryParse(address_end_str, &address_end))
      return;
    if (address_end <= address_start || address_end > 0x1'0000'0000)
      return;

    memory_ranges.emplace_back(static_cast<u32>(address_start), address_end - address_start);

    if (m_custom_virtual_address_space->isChecked())
      address_space = PowerPC::RequestedAddressSpace::Virtual;
    else if (m_custom_physical_address_space->isChecked())
      address_space = PowerPC::RequestedAddressSpace::Physical;
    else
      address_space = PowerPC::RequestedAddressSpace::Effective;
  }

  bool aligned = m_data_type_aligned->isChecked();
  auto data_type = m_data_type_dropdown->currentData().value<Cheats::DataType>();
  auto session = Cheats::MakeSession(std::move(memory_ranges), address_space, aligned, data_type);
  if (session)
    emit NewSessionCreated(*session);
}

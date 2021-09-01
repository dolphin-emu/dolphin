// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/MemoryWidget.h"

#include <optional>
#include <string>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpacerItem>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/AddressSpace.h"
#include "DolphinQt/Debugger/MemoryViewWidget.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

MemoryWidget::MemoryWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Memory"));
  setObjectName(QStringLiteral("memory"));

  setHidden(!Settings::Instance().IsMemoryVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  QSettings& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("memorywidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("memorywidget/floating")).toBool());
  m_splitter->restoreState(settings.value(QStringLiteral("codewidget/splitter")).toByteArray());

  connect(&Settings::Instance(), &Settings::MemoryVisibilityChanged, this,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsMemoryVisible()); });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &MemoryWidget::Update);
  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &MemoryWidget::Update);

  LoadSettings();

  ConnectWidgets();
  OnAddressSpaceChanged();
  OnTypeChanged();
}

MemoryWidget::~MemoryWidget()
{
  QSettings& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("memorywidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("memorywidget/floating"), isFloating());
  settings.setValue(QStringLiteral("memorywidget/splitter"), m_splitter->saveState());

  SaveSettings();
}

void MemoryWidget::CreateWidgets()
{
  auto* layout = new QHBoxLayout;

  layout->setContentsMargins(2, 2, 2, 2);
  layout->setSpacing(0);

  //// Sidebar

  // Search
  auto* m_address_splitter = new QSplitter(Qt::Horizontal);

  m_search_address = new QLineEdit;
  m_search_offset = new QLineEdit;
  m_data_edit = new QLineEdit;
  m_set_value = new QPushButton(tr("Set &Value"));

  m_search_address->setPlaceholderText(tr("Search Address"));
  m_search_offset->setPlaceholderText(tr("Offset"));
  m_data_edit->setPlaceholderText(tr("Value"));

  m_address_splitter->addWidget(m_search_address);
  m_address_splitter->addWidget(m_search_offset);
  m_address_splitter->setHandleWidth(1);
  m_address_splitter->setCollapsible(0, false);
  m_address_splitter->setStretchFactor(1, 2);

  // Dump
  m_dump_mram = new QPushButton(tr("Dump &MRAM"));
  m_dump_exram = new QPushButton(tr("Dump &ExRAM"));
  m_dump_aram = new QPushButton(tr("Dump &ARAM"));
  m_dump_fake_vmem = new QPushButton(tr("Dump &FakeVMEM"));

  // Search Options
  auto* search_group = new QGroupBox(tr("Search"));
  auto* search_layout = new QVBoxLayout;
  search_group->setLayout(search_layout);

  m_find_next = new QPushButton(tr("Find &Next"));
  m_find_previous = new QPushButton(tr("Find &Previous"));
  m_find_ascii = new QRadioButton(tr("ASCII"));
  m_find_hex = new QRadioButton(tr("Hex string"));
  m_result_label = new QLabel;

  search_layout->addWidget(m_find_next);
  search_layout->addWidget(m_find_previous);
  search_layout->addWidget(m_result_label);
  search_layout->setSpacing(1);

  // Address Space
  auto* address_space_group = new QGroupBox(tr("Address Space"));
  auto* address_space_layout = new QVBoxLayout;
  address_space_group->setLayout(address_space_layout);

  // i18n: "Effective" addresses are the addresses used directly by the CPU and may be subject to
  // translation via the MMU to physical addresses.
  m_address_space_effective = new QRadioButton(tr("Effective"));
  // i18n: The "Auxiliary" address space is the address space of ARAM (Auxiliary RAM).
  m_address_space_auxiliary = new QRadioButton(tr("Auxiliary"));
  // i18n: The "Physical" address space is the address space that reflects how devices (e.g. RAM) is
  // physically wired up.
  m_address_space_physical = new QRadioButton(tr("Physical"));

  address_space_layout->addWidget(m_address_space_effective);
  address_space_layout->addWidget(m_address_space_auxiliary);
  address_space_layout->addWidget(m_address_space_physical);
  address_space_layout->setSpacing(1);

  // Data Type
  auto* datatype_group = new QGroupBox(tr("Data Type"));
  auto* datatype_layout = new QVBoxLayout;
  datatype_group->setLayout(datatype_layout);

  m_type_u8 = new QRadioButton(tr("U&8"));
  m_type_u16 = new QRadioButton(tr("U&16"));
  m_type_u32 = new QRadioButton(tr("U&32"));
  m_type_ascii = new QRadioButton(tr("ASCII"));
  m_type_float = new QRadioButton(tr("Float"));

  datatype_layout->addWidget(m_type_u8);
  datatype_layout->addWidget(m_type_u16);
  datatype_layout->addWidget(m_type_u32);
  datatype_layout->addWidget(m_type_ascii);
  datatype_layout->addWidget(m_type_float);
  datatype_layout->setSpacing(1);

  // MBP options
  auto* bp_group = new QGroupBox(tr("Memory breakpoint options"));
  auto* bp_layout = new QVBoxLayout;
  bp_group->setLayout(bp_layout);

  // i18n: This string is used for a radio button that represents the type of
  // memory breakpoint that gets triggered when a read operation or write operation occurs.
  // The string is not a command to read and write something or to allow reading and writing.
  m_bp_read_write = new QRadioButton(tr("Read and write"));
  // i18n: This string is used for a radio button that represents the type of
  // memory breakpoint that gets triggered when a read operation occurs.
  // The string does not mean "read-only" in the sense that something cannot be written to.
  m_bp_read_only = new QRadioButton(tr("Read only"));
  // i18n: This string is used for a radio button that represents the type of
  // memory breakpoint that gets triggered when a write operation occurs.
  // The string does not mean "write-only" in the sense that something cannot be read from.
  m_bp_write_only = new QRadioButton(tr("Write only"));
  m_bp_log_check = new QCheckBox(tr("Log"));

  bp_layout->addWidget(m_bp_read_write);
  bp_layout->addWidget(m_bp_read_only);
  bp_layout->addWidget(m_bp_write_only);
  bp_layout->addWidget(m_bp_log_check);
  bp_layout->setSpacing(1);

  // Sidebar
  auto* sidebar = new QWidget;
  auto* sidebar_layout = new QVBoxLayout;
  sidebar_layout->setSpacing(1);

  sidebar->setLayout(sidebar_layout);

  sidebar_layout->addWidget(m_address_splitter);
  sidebar_layout->addWidget(m_data_edit);

  auto* types_layout = new QHBoxLayout;
  types_layout->addWidget(m_find_ascii);
  types_layout->addItem(new QSpacerItem(20, 1));
  types_layout->addWidget(m_find_hex);
  types_layout->setAlignment(Qt::AlignCenter);
  sidebar_layout->addLayout(types_layout);

  sidebar_layout->addWidget(m_set_value);
  sidebar_layout->addItem(new QSpacerItem(1, 20));
  sidebar_layout->addWidget(m_dump_mram);
  sidebar_layout->addWidget(m_dump_exram);
  sidebar_layout->addWidget(m_dump_aram);
  sidebar_layout->addWidget(m_dump_fake_vmem);
  sidebar_layout->addItem(new QSpacerItem(1, 15));
  sidebar_layout->addWidget(search_group);
  sidebar_layout->addWidget(address_space_group);
  sidebar_layout->addWidget(datatype_group);
  sidebar_layout->addWidget(bp_group);
  sidebar_layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));

  // Splitter
  m_splitter = new QSplitter(Qt::Horizontal);

  auto* sidebar_scroll = new QScrollArea;
  sidebar_scroll->setWidget(sidebar);
  sidebar_scroll->setWidgetResizable(true);
  sidebar_scroll->setFixedWidth(190);

  m_memory_view = new MemoryViewWidget(this);

  m_splitter->addWidget(m_memory_view);
  m_splitter->addWidget(sidebar_scroll);

  layout->addWidget(m_splitter);

  auto* widget = new QWidget;
  widget->setLayout(layout);
  setWidget(widget);
}

void MemoryWidget::ConnectWidgets()
{
  connect(m_search_address, &QLineEdit::textChanged, this, &MemoryWidget::OnSearchAddress);
  connect(m_search_offset, &QLineEdit::textChanged, this, &MemoryWidget::OnSearchAddress);

  connect(m_data_edit, &QLineEdit::textChanged, this, &MemoryWidget::ValidateSearchValue);
  connect(m_find_ascii, &QRadioButton::toggled, this, &MemoryWidget::ValidateSearchValue);
  connect(m_find_hex, &QRadioButton::toggled, this, &MemoryWidget::ValidateSearchValue);

  connect(m_set_value, &QPushButton::clicked, this, &MemoryWidget::OnSetValue);

  connect(m_dump_mram, &QPushButton::clicked, this, &MemoryWidget::OnDumpMRAM);
  connect(m_dump_exram, &QPushButton::clicked, this, &MemoryWidget::OnDumpExRAM);
  connect(m_dump_aram, &QPushButton::clicked, this, &MemoryWidget::OnDumpARAM);
  connect(m_dump_fake_vmem, &QPushButton::clicked, this, &MemoryWidget::OnDumpFakeVMEM);

  connect(m_find_next, &QPushButton::clicked, this, &MemoryWidget::OnFindNextValue);
  connect(m_find_previous, &QPushButton::clicked, this, &MemoryWidget::OnFindPreviousValue);

  for (auto* radio :
       {m_address_space_effective, m_address_space_auxiliary, m_address_space_physical})
  {
    connect(radio, &QRadioButton::toggled, this, &MemoryWidget::OnAddressSpaceChanged);
  }

  for (auto* radio : {m_type_u8, m_type_u16, m_type_u32, m_type_ascii, m_type_float})
    connect(radio, &QRadioButton::toggled, this, &MemoryWidget::OnTypeChanged);

  for (auto* radio : {m_bp_read_write, m_bp_read_only, m_bp_write_only})
    connect(radio, &QRadioButton::toggled, this, &MemoryWidget::OnBPTypeChanged);

  connect(m_bp_log_check, &QCheckBox::toggled, this, &MemoryWidget::OnBPLogChanged);
  connect(m_memory_view, &MemoryViewWidget::BreakpointsChanged, this,
          &MemoryWidget::BreakpointsChanged);
  connect(m_memory_view, &MemoryViewWidget::ShowCode, this, &MemoryWidget::ShowCode);
  connect(m_memory_view, &MemoryViewWidget::RequestWatch, this, &MemoryWidget::RequestWatch);
}

void MemoryWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetMemoryVisible(false);
}

void MemoryWidget::showEvent(QShowEvent* event)
{
  Update();
}

void MemoryWidget::Update()
{
  if (!isVisible())
    return;

  m_memory_view->Update();
  update();
}

void MemoryWidget::LoadSettings()
{
  QSettings& settings = Settings::GetQSettings();

  const bool search_ascii =
      settings.value(QStringLiteral("memorywidget/searchascii"), true).toBool();
  const bool search_hex = settings.value(QStringLiteral("memorywidget/searchhex"), false).toBool();

  m_find_ascii->setChecked(search_ascii);
  m_find_hex->setChecked(search_hex);

  const bool address_space_effective =
      settings.value(QStringLiteral("memorywidget/addrspace_effective"), true).toBool();
  const bool address_space_auxiliary =
      settings.value(QStringLiteral("memorywidget/addrspace_auxiliary"), false).toBool();
  const bool address_space_physical =
      settings.value(QStringLiteral("memorywidget/addrspace_physical"), false).toBool();

  m_address_space_effective->setChecked(address_space_effective);
  m_address_space_auxiliary->setChecked(address_space_auxiliary);
  m_address_space_physical->setChecked(address_space_physical);

  const bool type_u8 = settings.value(QStringLiteral("memorywidget/typeu8"), true).toBool();
  const bool type_u16 = settings.value(QStringLiteral("memorywidget/typeu16"), false).toBool();
  const bool type_u32 = settings.value(QStringLiteral("memorywidget/typeu32"), false).toBool();
  const bool type_float = settings.value(QStringLiteral("memorywidget/typefloat"), false).toBool();
  const bool type_ascii = settings.value(QStringLiteral("memorywidget/typeascii"), false).toBool();

  m_type_u8->setChecked(type_u8);
  m_type_u16->setChecked(type_u16);
  m_type_u32->setChecked(type_u32);
  m_type_float->setChecked(type_float);
  m_type_ascii->setChecked(type_ascii);

  bool bp_rw = settings.value(QStringLiteral("memorywidget/bpreadwrite"), true).toBool();
  bool bp_r = settings.value(QStringLiteral("memorywidget/bpread"), false).toBool();
  bool bp_w = settings.value(QStringLiteral("memorywidget/bpwrite"), false).toBool();
  bool bp_log = settings.value(QStringLiteral("memorywidget/bplog"), true).toBool();

  if (bp_rw)
    m_memory_view->SetBPType(MemoryViewWidget::BPType::ReadWrite);
  else if (bp_r)
    m_memory_view->SetBPType(MemoryViewWidget::BPType::ReadOnly);
  else
    m_memory_view->SetBPType(MemoryViewWidget::BPType::WriteOnly);

  m_bp_read_write->setChecked(bp_rw);
  m_bp_read_only->setChecked(bp_r);
  m_bp_write_only->setChecked(bp_w);
  m_bp_log_check->setChecked(bp_log);
}

void MemoryWidget::SaveSettings()
{
  QSettings& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("memorywidget/searchascii"), m_find_ascii->isChecked());
  settings.setValue(QStringLiteral("memorywidget/searchhex"), m_find_hex->isChecked());

  settings.setValue(QStringLiteral("memorywidget/addrspace_effective"),
                    m_address_space_effective->isChecked());
  settings.setValue(QStringLiteral("memorywidget/addrspace_auxiliary"),
                    m_address_space_auxiliary->isChecked());
  settings.setValue(QStringLiteral("memorywidget/addrspace_physical"),
                    m_address_space_physical->isChecked());

  settings.setValue(QStringLiteral("memorywidget/typeu8"), m_type_u8->isChecked());
  settings.setValue(QStringLiteral("memorywidget/typeu16"), m_type_u16->isChecked());
  settings.setValue(QStringLiteral("memorywidget/typeu32"), m_type_u32->isChecked());
  settings.setValue(QStringLiteral("memorywidget/typeascii"), m_type_ascii->isChecked());
  settings.setValue(QStringLiteral("memorywidget/typefloat"), m_type_float->isChecked());

  settings.setValue(QStringLiteral("memorywidget/bpreadwrite"), m_bp_read_write->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bpread"), m_bp_read_only->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bpwrite"), m_bp_write_only->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bplog"), m_bp_log_check->isChecked());
}

void MemoryWidget::OnAddressSpaceChanged()
{
  AddressSpace::Type space;

  if (m_address_space_effective->isChecked())
    space = AddressSpace::Type::Effective;
  else if (m_address_space_auxiliary->isChecked())
    space = AddressSpace::Type::Auxiliary;
  else
    space = AddressSpace::Type::Physical;

  m_memory_view->SetAddressSpace(space);

  SaveSettings();
}

void MemoryWidget::OnTypeChanged()
{
  MemoryViewWidget::Type type;

  if (m_type_u8->isChecked())
    type = MemoryViewWidget::Type::U8;
  else if (m_type_u16->isChecked())
    type = MemoryViewWidget::Type::U16;
  else if (m_type_u32->isChecked())
    type = MemoryViewWidget::Type::U32;
  else if (m_type_ascii->isChecked())
    type = MemoryViewWidget::Type::ASCII;
  else
    type = MemoryViewWidget::Type::Float32;

  m_memory_view->SetType(type);

  SaveSettings();
}

void MemoryWidget::OnBPLogChanged()
{
  m_memory_view->SetBPLoggingEnabled(m_bp_log_check->isChecked());
  SaveSettings();
}

void MemoryWidget::OnBPTypeChanged()
{
  bool read_write = m_bp_read_write->isChecked();
  bool read_only = m_bp_read_only->isChecked();

  MemoryViewWidget::BPType type;

  if (read_write)
    type = MemoryViewWidget::BPType::ReadWrite;
  else if (read_only)
    type = MemoryViewWidget::BPType::ReadOnly;
  else
    type = MemoryViewWidget::BPType::WriteOnly;

  m_memory_view->SetBPType(type);

  SaveSettings();
}

void MemoryWidget::SetAddress(u32 address)
{
  m_memory_view->SetAddress(address);
  Settings::Instance().SetMemoryVisible(true);
  raise();

  m_memory_view->setFocus();
}

void MemoryWidget::OnSearchAddress()
{
  bool good_addr, good_offset;
  // Returns 0 if conversion fails
  const u32 addr = m_search_address->text().toUInt(&good_addr, 16);
  const u32 offset = m_search_offset->text().toUInt(&good_offset, 16);

  QFont addr_font, offset_font;
  QPalette addr_palette, offset_palette;

  if (good_addr || m_search_address->text().isEmpty())
  {
    if (good_addr)
      m_memory_view->SetAddress(addr + offset);
  }
  else
  {
    addr_font.setBold(true);
    addr_palette.setColor(QPalette::Text, Qt::red);
  }

  if (!good_offset && !m_search_offset->text().isEmpty())
  {
    offset_font.setBold(true);
    offset_palette.setColor(QPalette::Text, Qt::red);
  }

  m_search_address->setFont(addr_font);
  m_search_address->setPalette(addr_palette);
  m_search_offset->setFont(offset_font);
  m_search_offset->setPalette(offset_palette);
}

void MemoryWidget::ValidateSearchValue()
{
  QFont font;
  QPalette palette;

  if (!IsValueValid())
  {
    font.setBold(true);
    palette.setColor(QPalette::Text, Qt::red);
  }

  m_data_edit->setFont(font);
  m_data_edit->setPalette(palette);
}

void MemoryWidget::OnSetValue()
{
  if (!Core::IsRunning())
    return;

  bool good_address, good_offset;
  // Returns 0 if conversion fails
  u32 addr = m_search_address->text().toUInt(&good_address, 16);
  addr += m_search_offset->text().toUInt(&good_offset, 16);

  if (!good_address)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad address provided."));
    return;
  }

  if (!good_offset)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad offset provided."));
    return;
  }

  if (m_data_edit->text().isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("No value provided."));
    return;
  }

  if (!IsValueValid())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad value provided."));
    return;
  }

  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_memory_view->GetAddressSpace());

  const QByteArray bytes = GetValueData();
  for (const char c : bytes)
    accessors->WriteU8(addr++, static_cast<u8>(c));

  Update();
}

static void DumpArray(const std::string& filename, const u8* data, size_t length)
{
  if (!data)
    return;

  File::IOFile f(filename, "wb");

  if (!f)
  {
    ModalMessageBox::critical(
        nullptr, QObject::tr("Error"),
        QObject::tr("Failed to dump %1: Can't open file").arg(QString::fromStdString(filename)));
    return;
  }

  if (!f.WriteBytes(data, length))
  {
    ModalMessageBox::critical(nullptr, QObject::tr("Error"),
                              QObject::tr("Failed to dump %1: Failed to write to file")
                                  .arg(QString::fromStdString(filename)));
  }
}

void MemoryWidget::OnDumpMRAM()
{
  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(AddressSpace::Type::Mem1);
  DumpArray(File::GetUserPath(F_MEM1DUMP_IDX), accessors->begin(),
            std::distance(accessors->begin(), accessors->end()));
}

void MemoryWidget::OnDumpExRAM()
{
  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(AddressSpace::Type::Mem2);
  DumpArray(File::GetUserPath(F_MEM2DUMP_IDX), accessors->begin(),
            std::distance(accessors->begin(), accessors->end()));
}

void MemoryWidget::OnDumpARAM()
{
  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(AddressSpace::Type::Auxiliary);
  DumpArray(File::GetUserPath(F_ARAMDUMP_IDX), accessors->begin(),
            std::distance(accessors->begin(), accessors->end()));
}

void MemoryWidget::OnDumpFakeVMEM()
{
  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(AddressSpace::Type::Fake);
  DumpArray(File::GetUserPath(F_FAKEVMEMDUMP_IDX), accessors->begin(),
            std::distance(accessors->begin(), accessors->end()));
}

bool MemoryWidget::IsValueValid() const
{
  if (m_find_ascii->isChecked())
    return true;
  const QRegularExpression is_hex(QStringLiteral("^([0-9A-F]{2})*$"),
                                  QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch match = is_hex.match(m_data_edit->text());
  return match.hasMatch();
}

QByteArray MemoryWidget::GetValueData() const
{
  if (!IsValueValid())
    return QByteArray();

  const QByteArray value = m_data_edit->text().toUtf8();

  if (m_find_ascii->isChecked())
    return value;

  return QByteArray::fromHex(value);
}

void MemoryWidget::FindValue(bool next)
{
  if (!IsValueValid())
  {
    m_result_label->setText(tr("Bad value provided."));
    return;
  }

  const QByteArray search_for = GetValueData();

  if (search_for.isEmpty())
  {
    m_result_label->setText(tr("No Value Given"));
    return;
  }
  u32 addr = 0;

  if (!m_search_address->text().isEmpty())
  {
    // skip the quoted address so we don't potentially refind the last result
    addr = m_search_address->text().toUInt(nullptr, 16) +
           m_search_offset->text().toUInt(nullptr, 16) + (next ? 1 : -1);
  }

  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_memory_view->GetAddressSpace());

  const auto found_addr = accessors->Search(addr, reinterpret_cast<const u8*>(search_for.data()),
                                            static_cast<u32>(search_for.size()), next);

  if (found_addr.has_value())
  {
    m_result_label->setText(tr("Match Found"));

    u32 offset = *found_addr;

    m_search_address->setText(QStringLiteral("%1").arg(offset, 8, 16, QLatin1Char('0')));
    m_search_offset->clear();

    m_memory_view->SetAddress(offset);

    return;
  }

  m_result_label->setText(tr("No Match"));
}

void MemoryWidget::OnFindNextValue()
{
  FindValue(true);
}

void MemoryWidget::OnFindPreviousValue()
{
  FindValue(false);
}

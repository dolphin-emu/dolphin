// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include <QScrollArea>
#include <QSpacerItem>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/HW/AddressSpace.h"
#include "DolphinQt/Debugger/MemoryViewWidget.h"
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

  connect(&Settings::Instance(), &Settings::MemoryVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsMemoryVisible()); });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &MemoryWidget::Update);

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
  m_search_address = new QLineEdit;
  m_data_edit = new QLineEdit;
  m_set_value = new QPushButton(tr("Set &Value"));

  m_search_address->setPlaceholderText(tr("Search Address"));
  m_data_edit->setPlaceholderText(tr("Value"));

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
  m_find_hex = new QRadioButton(tr("Hex"));
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

  sidebar_layout->addWidget(m_search_address);
  sidebar_layout->addWidget(m_data_edit);
  sidebar_layout->addWidget(m_find_ascii);
  sidebar_layout->addWidget(m_find_hex);
  sidebar_layout->addWidget(m_set_value);
  sidebar_layout->addItem(new QSpacerItem(1, 32));
  sidebar_layout->addWidget(m_dump_mram);
  sidebar_layout->addWidget(m_dump_exram);
  sidebar_layout->addWidget(m_dump_aram);
  sidebar_layout->addWidget(m_dump_fake_vmem);
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

  connect(m_data_edit, &QLineEdit::textChanged, this, &MemoryWidget::ValidateSearchValue);
  connect(m_find_ascii, &QRadioButton::toggled, this, &MemoryWidget::ValidateSearchValue);
  connect(m_find_hex, &QRadioButton::toggled, this, &MemoryWidget::ValidateSearchValue);

  connect(m_set_value, &QPushButton::pressed, this, &MemoryWidget::OnSetValue);

  connect(m_dump_mram, &QPushButton::pressed, this, &MemoryWidget::OnDumpMRAM);
  connect(m_dump_exram, &QPushButton::pressed, this, &MemoryWidget::OnDumpExRAM);
  connect(m_dump_aram, &QPushButton::pressed, this, &MemoryWidget::OnDumpARAM);
  connect(m_dump_fake_vmem, &QPushButton::pressed, this, &MemoryWidget::OnDumpFakeVMEM);

  connect(m_find_next, &QPushButton::pressed, this, &MemoryWidget::OnFindNextValue);
  connect(m_find_previous, &QPushButton::pressed, this, &MemoryWidget::OnFindPreviousValue);

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
  bool good;
  u32 addr = m_search_address->text().toUInt(&good, 16);

  QFont font;
  QPalette palette;

  if (good || m_search_address->text().isEmpty())
  {
    if (good)
      m_memory_view->SetAddress(addr);
  }
  else
  {
    font.setBold(true);
    palette.setColor(QPalette::Text, Qt::red);
  }

  m_search_address->setFont(font);
  m_search_address->setPalette(palette);
}

void MemoryWidget::ValidateSearchValue()
{
  QFont font;
  QPalette palette;

  if (m_find_hex->isChecked() && !m_data_edit->text().isEmpty())
  {
    bool good;
    m_data_edit->text().toULongLong(&good, 16);

    if (!good)
    {
      font.setBold(true);
      palette.setColor(QPalette::Text, Qt::red);
    }
  }

  m_data_edit->setFont(font);
  m_data_edit->setPalette(palette);
}

void MemoryWidget::OnSetValue()
{
  bool good_address;
  u32 addr = m_search_address->text().toUInt(&good_address, 16);

  if (!good_address)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Bad address provided."));
    return;
  }

  if (m_data_edit->text().isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("No value provided."));
    return;
  }

  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_memory_view->GetAddressSpace());

  if (m_find_ascii->isChecked())
  {
    const QByteArray bytes = m_data_edit->text().toUtf8();

    for (char c : bytes)
      accessors->WriteU8(static_cast<u8>(c), addr++);
  }
  else
  {
    bool good_value;
    u64 value = m_data_edit->text().toULongLong(&good_value, 16);

    if (!good_value)
    {
      ModalMessageBox::critical(this, tr("Error"), tr("Bad value provided."));
      return;
    }

    if (value == static_cast<u8>(value))
    {
      accessors->WriteU8(addr, static_cast<u8>(value));
    }
    else if (value == static_cast<u16>(value))
    {
      accessors->WriteU16(addr, static_cast<u16>(value));
    }
    else if (value == static_cast<u32>(value))
    {
      accessors->WriteU32(addr, static_cast<u32>(value));
    }
    else
      accessors->WriteU64(addr, value);
  }

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

std::vector<u8> MemoryWidget::GetValueData() const
{
  std::vector<u8> search_for;  // Series of bytes we want to look for

  if (m_find_ascii->isChecked())
  {
    const QByteArray bytes = m_data_edit->text().toUtf8();
    search_for.assign(bytes.begin(), bytes.end());
  }
  else
  {
    bool good;
    u64 value = m_data_edit->text().toULongLong(&good, 16);

    if (!good)
      return {};

    int size;

    if (value == static_cast<u8>(value))
      size = sizeof(u8);
    else if (value == static_cast<u16>(value))
      size = sizeof(u16);
    else if (value == static_cast<u32>(value))
      size = sizeof(u32);
    else
      size = sizeof(u64);

    for (int i = size - 1; i >= 0; i--)
      search_for.push_back((value >> (i * 8)) & 0xFF);
  }

  return search_for;
}

void MemoryWidget::FindValue(bool next)
{
  std::vector<u8> search_for = GetValueData();

  if (search_for.empty())
  {
    m_result_label->setText(tr("No Value Given"));
    return;
  }
  u32 addr = 0;

  if (!m_search_address->text().isEmpty())
  {
    // skip the quoted address so we don't potentially refind the last result
    addr = m_search_address->text().toUInt(nullptr, 16) + (next ? 1 : -1);
  }

  AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(m_memory_view->GetAddressSpace());

  auto found_addr =
      accessors->Search(addr, search_for.data(), static_cast<u32>(search_for.size()), next);

  if (found_addr.has_value())
  {
    m_result_label->setText(tr("Match Found"));

    u32 offset = *found_addr;

    m_search_address->setText(QStringLiteral("%1").arg(offset, 8, 16, QLatin1Char('0')));

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

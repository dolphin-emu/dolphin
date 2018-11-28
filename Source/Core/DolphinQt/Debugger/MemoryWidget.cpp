// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/MemoryWidget.h"

#include <ctype.h>
#include <string>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
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
#include "Core/HW/DSP.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Debugger/MemoryViewWidget.h"
#include "DolphinQt/Settings.h"

MemoryWidget::MemoryWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Memory"));
  setObjectName(QStringLiteral("memory"));

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  QSettings& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("memorywidget/geometry")).toByteArray());
  setFloating(settings.value(QStringLiteral("memorywidget/floating")).toBool());
  m_splitter->restoreState(settings.value(QStringLiteral("codewidget/splitter")).toByteArray());

  connect(&Settings::Instance(), &Settings::MemoryVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled,
          [this](bool enabled) { setHidden(!enabled || !Settings::Instance().IsMemoryVisible()); });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, &MemoryWidget::Update);

  setHidden(!Settings::Instance().IsCodeVisible() || !Settings::Instance().IsDebugModeEnabled());

  LoadSettings();

  ConnectWidgets();
  Update();
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
  m_search_address_offset = new QLineEdit;
  m_data_edit = new QLineEdit;
  m_set_value = new QPushButton(tr("Set &Value"));
  m_data_preview = new QLabel;

  m_search_address->setMaxLength(8);
  m_data_preview->setBackgroundRole(QPalette::Base);
  m_data_preview->setAutoFillBackground(true);

  m_search_address->setPlaceholderText(tr("Search Address"));
  m_search_address_offset->setPlaceholderText(tr("Offset"));
  m_data_edit->setPlaceholderText(tr("Value"));
  // Dump
  m_dump_mram = new QPushButton(tr("Dump &MRAM"));
  m_dump_exram = new QPushButton(tr("Dump &ExRAM"));
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

  // Data Type
  auto* datatype_group = new QGroupBox(tr("Data Type"));
  auto* datatype_layout = new QVBoxLayout;
  datatype_group->setLayout(datatype_layout);

  m_type_u8 = new QRadioButton(tr("U&8"));
  m_type_u16 = new QRadioButton(tr("U&16"));
  m_type_u32 = new QRadioButton(tr("U&32"));
  m_type_ascii = new QRadioButton(tr("ASCII"));
  m_type_float = new QRadioButton(tr("Float"));
  m_mem_view_style = new QCheckBox(tr("Alternate View"));

  datatype_layout->addWidget(m_type_u8);
  datatype_layout->addWidget(m_type_u16);
  datatype_layout->addWidget(m_type_u32);
  datatype_layout->addWidget(m_type_ascii);
  datatype_layout->addWidget(m_type_float);
  datatype_layout->addWidget(m_mem_view_style);
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

  // Search Address
  auto* searchaddr_layout = new QHBoxLayout;
  searchaddr_layout->addWidget(m_search_address);
  searchaddr_layout->addWidget(m_search_address_offset);

  // Sidebar
  auto* sidebar = new QWidget;
  auto* sidebar_layout = new QVBoxLayout;
  sidebar_layout->setSpacing(1);

  sidebar->setLayout(sidebar_layout);

  sidebar_layout->addLayout(searchaddr_layout);
  sidebar_layout->addWidget(m_data_edit);
  sidebar_layout->addWidget(m_data_preview);
  sidebar_layout->addWidget(m_find_ascii);
  sidebar_layout->addWidget(m_find_hex);
  sidebar_layout->addWidget(m_set_value);
  sidebar_layout->addItem(new QSpacerItem(1, 32));
  sidebar_layout->addWidget(m_dump_mram);
  sidebar_layout->addWidget(m_dump_exram);
  sidebar_layout->addWidget(m_dump_fake_vmem);
  sidebar_layout->addWidget(search_group);
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
  connect(m_search_address_offset, &QLineEdit::textChanged, this, &MemoryWidget::OnSearchAddress);

  connect(m_data_edit, &QLineEdit::textChanged, this, &MemoryWidget::ValidateSearchValue);
  connect(m_find_ascii, &QRadioButton::toggled, this, &MemoryWidget::ValidateSearchValue);
  connect(m_find_hex, &QRadioButton::toggled, this, &MemoryWidget::ValidateSearchValue);

  connect(m_set_value, &QPushButton::pressed, this, &MemoryWidget::OnSetValue);

  connect(m_dump_mram, &QPushButton::pressed, this, &MemoryWidget::OnDumpMRAM);
  connect(m_dump_exram, &QPushButton::pressed, this, &MemoryWidget::OnDumpExRAM);
  connect(m_dump_fake_vmem, &QPushButton::pressed, this, &MemoryWidget::OnDumpFakeVMEM);

  connect(m_find_next, &QPushButton::pressed, this, &MemoryWidget::OnFindNextValue);
  connect(m_find_previous, &QPushButton::pressed, this, &MemoryWidget::OnFindPreviousValue);

  for (auto* radio : {m_type_u8, m_type_u16, m_type_u32, m_type_ascii, m_type_float})
    connect(radio, &QRadioButton::toggled, this, &MemoryWidget::OnTypeChanged);

  connect(m_mem_view_style, &QCheckBox::toggled, this, &MemoryWidget::OnTypeChanged);

  for (auto* radio : {m_bp_read_write, m_bp_read_only, m_bp_write_only})
    connect(radio, &QRadioButton::toggled, this, &MemoryWidget::OnBPTypeChanged);

  connect(m_bp_log_check, &QCheckBox::toggled, this, &MemoryWidget::OnBPLogChanged);
  connect(m_memory_view, &MemoryViewWidget::BreakpointsChanged, this,
          &MemoryWidget::BreakpointsChanged);
  connect(m_memory_view, &MemoryViewWidget::SendSearchValue, m_search_address, &QLineEdit::setText);
  connect(m_memory_view, &MemoryViewWidget::SendSearchValue, m_search_address_offset,
          &QLineEdit::clear);
  connect(m_memory_view, &MemoryViewWidget::SendDataValue, m_data_edit, &QLineEdit::setText);
}

void MemoryWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetMemoryVisible(false);
}

void MemoryWidget::Update()
{
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

  const bool type_u8 = settings.value(QStringLiteral("memorywidget/typeu8"), true).toBool();
  const bool type_u16 = settings.value(QStringLiteral("memorywidget/typeu16"), false).toBool();
  const bool type_u32 = settings.value(QStringLiteral("memorywidget/typeu32"), false).toBool();
  const bool type_float = settings.value(QStringLiteral("memorywidget/typefloat"), false).toBool();
  const bool type_ascii = settings.value(QStringLiteral("memorywidget/typeascii"), false).toBool();
  const bool mem_view_style =
      settings.value(QStringLiteral("memorywidget/memviewstyle"), false).toBool();

  m_type_u8->setChecked(type_u8);
  m_type_u16->setChecked(type_u16);
  m_type_u32->setChecked(type_u32);
  m_type_float->setChecked(type_float);
  m_type_ascii->setChecked(type_ascii);
  m_mem_view_style->setChecked(mem_view_style);

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

  settings.setValue(QStringLiteral("memorywidget/typeu8"), m_type_u8->isChecked());
  settings.setValue(QStringLiteral("memorywidget/typeu16"), m_type_u16->isChecked());
  settings.setValue(QStringLiteral("memorywidget/typeu32"), m_type_u32->isChecked());
  settings.setValue(QStringLiteral("memorywidget/typeascii"), m_type_ascii->isChecked());
  settings.setValue(QStringLiteral("memorywidget/typefloat"), m_type_float->isChecked());
  settings.setValue(QStringLiteral("memorywidget/memviewstyle"), m_mem_view_style->isChecked());

  settings.setValue(QStringLiteral("memorywidget/bpreadwrite"), m_bp_read_write->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bpread"), m_bp_read_only->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bpwrite"), m_bp_write_only->isChecked());
  settings.setValue(QStringLiteral("memorywidget/bplog"), m_bp_log_check->isChecked());
}

void MemoryWidget::OnTypeChanged()
{
  MemoryViewWidget::Type type;
  if (m_mem_view_style->isChecked() && m_type_ascii->isChecked())
    type = MemoryViewWidget::Type::U32xASCII;
  else if (m_mem_view_style->isChecked() && m_type_float->isChecked())
    type = MemoryViewWidget::Type::U32xFloat32;
  else if (m_type_u8->isChecked())
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

void MemoryWidget::OnSearchAddress()
{
  bool good;
  u32 addr =
      m_search_address->text().toUInt(&good, 16) + m_search_address_offset->text().toInt(&good, 16);

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
  m_data_preview->clear();

  if (m_find_hex->isChecked() && !m_data_edit->text().isEmpty())
  {
    bool good;
    u64 value = m_data_edit->text().toULongLong(&good, 16);

    if (!good)
    {
      font.setBold(true);
      palette.setColor(QPalette::Text, Qt::red);
    }
    else
    {
      const int data_length = m_data_edit->text().size();
      int field_width = 8;

      if (data_length > 8)
      {
        field_width = 16;
      }
      else if ((m_type_u8->isChecked() || m_type_ascii->isChecked()) && data_length < 5)
      {
        field_width = data_length > 2 ? 4 : 2;
      }
      else if (m_type_u16->isChecked() && data_length <= 4)
      {
        field_width = 4;
      }

      QString hex_string =
          QStringLiteral("%1").arg(value, field_width, 16, QLatin1Char('0')).toUpper();
      int hsize = hex_string.size();
      for (int i = 2; i < hsize; i += 2)
        hex_string.insert(hsize - i, QLatin1Char{' '});
      m_data_preview->setText(hex_string);
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
    QMessageBox::critical(this, tr("Error"), tr("Bad address provided."));
    return;
  }

  if (m_data_edit->text().isEmpty())
  {
    QMessageBox::critical(this, tr("Error"), tr("No value provided."));
    return;
  }

  if (m_find_ascii->isChecked())
  {
    const QByteArray bytes = m_data_edit->text().toUtf8();

    for (char c : bytes)
      PowerPC::HostWrite_U8(static_cast<u8>(c), addr++);
  }
  else
  {
    bool good_value;
    u64 value = m_data_edit->text().toULongLong(&good_value, 16);

    if (!good_value)
    {
      QMessageBox::critical(this, tr("Error"), tr("Bad value provided."));
      return;
    }

    const int data_length = m_data_edit->text().size();

    if (data_length > 8)
    {
      PowerPC::HostWrite_U64(value, addr);
    }
    else if ((m_type_u8->isChecked() || m_type_ascii->isChecked()) && data_length < 5)
    {
      if (data_length > 2)
        PowerPC::HostWrite_U16(static_cast<u16>(value), addr);
      else
        PowerPC::HostWrite_U8(static_cast<u8>(value), addr);
    }
    else if (m_type_u16->isChecked() && data_length <= 4)
    {
      PowerPC::HostWrite_U16(static_cast<u16>(value), addr);
    }
    else
    {
      PowerPC::HostWrite_U32(static_cast<u32>(value), addr);
    }
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
    QMessageBox::critical(
        nullptr, QObject::tr("Error"),
        QObject::tr("Failed to dump %1: Can't open file").arg(QString::fromStdString(filename)));
    return;
  }

  if (!f.WriteBytes(data, length))
  {
    QMessageBox::critical(nullptr, QObject::tr("Error"),
                          QObject::tr("Failed to dump %1: Failed to write to file")
                              .arg(QString::fromStdString(filename)));
  }
}

void MemoryWidget::OnDumpMRAM()
{
  DumpArray(File::GetUserPath(F_RAMDUMP_IDX), Memory::m_pRAM, Memory::REALRAM_SIZE);
}

void MemoryWidget::OnDumpExRAM()
{
  if (SConfig::GetInstance().bWii)
  {
    DumpArray(File::GetUserPath(F_ARAMDUMP_IDX), Memory::m_pEXRAM, Memory::EXRAM_SIZE);
  }
  else
  {
    DumpArray(File::GetUserPath(F_ARAMDUMP_IDX), DSP::GetARAMPtr(), DSP::ARAM_SIZE);
  }
}

void MemoryWidget::OnDumpFakeVMEM()
{
  DumpArray(File::GetUserPath(F_FAKEVMEMDUMP_IDX), Memory::m_pFakeVMEM, Memory::FAKEVMEM_SIZE);
}

std::vector<u8> MemoryWidget::GetValueData() const
{
  // Series of bytes we want to look for.
  std::vector<u8> search_for;

  if (m_find_ascii->isChecked())
  {
    const QByteArray bytes = m_data_edit->text().toUtf8();
    search_for.assign(bytes.begin(), bytes.end());
  }
  else
  {
    // Accepts any amount of bytes.
    std::string search_prep = m_data_edit->text().toStdString();
    for (size_t i = 0; i <= search_prep.length() - 2; i = i + 2)
    {
      if (!isxdigit(search_prep[i]) || !isxdigit(search_prep[i + 1]))
        return {};
      search_for.push_back(std::stoi((search_prep.substr(i, 2)), nullptr, 16));
    }
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

  const u8* ram_ptr = nullptr;
  std::size_t ram_size = 0;
  u32 base_address = 0;

  if (m_type_u16->isChecked())
  {
    ram_ptr = DSP::GetARAMPtr();
    ram_size = DSP::ARAM_SIZE;
    base_address = 0x0c005000;
  }
  else if (Memory::m_pRAM)
  {
    ram_ptr = Memory::m_pRAM;
    ram_size = Memory::REALRAM_SIZE;
    base_address = 0x80000000;
  }
  else
  {
    m_result_label->setText(tr("Memory Not Ready"));
    return;
  }

  u32 addr = 0;

  if (!m_search_address->text().isEmpty())
    addr = m_search_address->text().toUInt(nullptr, 16) + 1;

  if (addr >= base_address)
    addr -= base_address;

  if (addr >= ram_size - search_for.size())
  {
    m_result_label->setText(tr("Address Out of Range"));
    return;
  }

  const u8* ptr;
  const u8* end;

  if (next)
  {
    end = &ram_ptr[ram_size - search_for.size() + 1];
    ptr = std::search(&ram_ptr[addr], end, search_for.begin(), search_for.end());
  }
  else
  {
    end = &ram_ptr[addr - 1];
    ptr = std::find_end(ram_ptr, end, search_for.begin(), search_for.end());
  }

  if (ptr != end)
  {
    m_result_label->setText(tr("Match Found"));

    u32 offset = static_cast<u32>(ptr - ram_ptr) + base_address;

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

void MemoryWidget::OnSetAddress(u32 addr)
{
  const QString address = QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0'));
  m_search_address->setText(address);
}

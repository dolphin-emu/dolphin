// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/FIFO/FIFOAnalyzer.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "Common/Assert.h"
#include "Common/Swap.h"
#include "Core/FifoPlayer/FifoPlayer.h"

#include "DolphinQt/Settings.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/XFStructs.h"

constexpr int FRAME_ROLE = Qt::UserRole;
constexpr int OBJECT_ROLE = Qt::UserRole + 1;

FIFOAnalyzer::FIFOAnalyzer()
{
  CreateWidgets();
  ConnectWidgets();

  UpdateTree();

  auto& settings = Settings::GetQSettings();

  m_object_splitter->restoreState(
      settings.value(QStringLiteral("fifoanalyzer/objectsplitter")).toByteArray());
  m_search_splitter->restoreState(
      settings.value(QStringLiteral("fifoanalyzer/searchsplitter")).toByteArray());

  m_detail_list->setFont(Settings::Instance().GetDebugFont());
  m_entry_detail_browser->setFont(Settings::Instance().GetDebugFont());

  connect(&Settings::Instance(), &Settings::DebugFontChanged, this, [this] {
    m_detail_list->setFont(Settings::Instance().GetDebugFont());
    m_entry_detail_browser->setFont(Settings::Instance().GetDebugFont());
  });
}

FIFOAnalyzer::~FIFOAnalyzer()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("fifoanalyzer/objectsplitter"), m_object_splitter->saveState());
  settings.setValue(QStringLiteral("fifoanalyzer/searchsplitter"), m_search_splitter->saveState());
}

void FIFOAnalyzer::CreateWidgets()
{
  m_tree_widget = new QTreeWidget;
  m_detail_list = new QListWidget;
  m_entry_detail_browser = new QTextBrowser;

  m_object_splitter = new QSplitter(Qt::Horizontal);

  m_object_splitter->addWidget(m_tree_widget);
  m_object_splitter->addWidget(m_detail_list);

  m_tree_widget->header()->hide();

  m_search_box = new QGroupBox(tr("Search Current Object"));
  m_search_edit = new QLineEdit;
  m_search_new = new QPushButton(tr("Search"));
  m_search_next = new QPushButton(tr("Next Match"));
  m_search_previous = new QPushButton(tr("Previous Match"));
  m_search_label = new QLabel;

  m_search_next->setEnabled(false);
  m_search_previous->setEnabled(false);

  auto* box_layout = new QHBoxLayout;

  box_layout->addWidget(m_search_edit);
  box_layout->addWidget(m_search_new);
  box_layout->addWidget(m_search_next);
  box_layout->addWidget(m_search_previous);
  box_layout->addWidget(m_search_label);

  m_search_box->setLayout(box_layout);

  m_search_box->setMaximumHeight(m_search_box->minimumSizeHint().height());

  m_search_splitter = new QSplitter(Qt::Vertical);

  m_search_splitter->addWidget(m_object_splitter);
  m_search_splitter->addWidget(m_entry_detail_browser);
  m_search_splitter->addWidget(m_search_box);

  auto* layout = new QHBoxLayout;
  layout->addWidget(m_search_splitter);

  setLayout(layout);
}

void FIFOAnalyzer::ConnectWidgets()
{
  connect(m_tree_widget, &QTreeWidget::itemSelectionChanged, this, &FIFOAnalyzer::UpdateDetails);
  connect(m_detail_list, &QListWidget::itemSelectionChanged, this,
          &FIFOAnalyzer::UpdateDescription);

  connect(m_search_edit, &QLineEdit::returnPressed, this, &FIFOAnalyzer::BeginSearch);
  connect(m_search_new, &QPushButton::clicked, this, &FIFOAnalyzer::BeginSearch);
  connect(m_search_next, &QPushButton::clicked, this, &FIFOAnalyzer::FindNext);
  connect(m_search_previous, &QPushButton::clicked, this, &FIFOAnalyzer::FindPrevious);
}

void FIFOAnalyzer::Update()
{
  UpdateTree();
  UpdateDetails();
  UpdateDescription();
}

void FIFOAnalyzer::UpdateTree()
{
  m_tree_widget->clear();

  if (!FifoPlayer::GetInstance().IsPlaying())
  {
    m_tree_widget->addTopLevelItem(new QTreeWidgetItem({tr("No recording loaded.")}));
    return;
  }

  auto* recording_item = new QTreeWidgetItem({tr("Recording")});

  m_tree_widget->addTopLevelItem(recording_item);

  auto* file = FifoPlayer::GetInstance().GetFile();

  const u32 frame_count = file->GetFrameCount();
  for (u32 frame = 0; frame < frame_count; frame++)
  {
    auto* frame_item = new QTreeWidgetItem({tr("Frame %1").arg(frame)});

    recording_item->addChild(frame_item);

    const u32 object_count = FifoPlayer::GetInstance().GetFrameObjectCount(frame);
    for (u32 object = 0; object < object_count; object++)
    {
      auto* object_item = new QTreeWidgetItem({tr("Object %1").arg(object)});

      frame_item->addChild(object_item);

      object_item->setData(0, FRAME_ROLE, frame);
      object_item->setData(0, OBJECT_ROLE, object);
    }
  }
}

static std::string GetPrimitiveName(u8 cmd)
{
  if ((cmd & 0xC0) != 0x80)
  {
    PanicAlertFmt("Not a primitive command: {:#04x}", cmd);
    return "";
  }
  const u8 vat = cmd & OpcodeDecoder::GX_VAT_MASK;  // Vertex loader index (0 - 7)
  const u8 primitive =
      (cmd & OpcodeDecoder::GX_PRIMITIVE_MASK) >> OpcodeDecoder::GX_PRIMITIVE_SHIFT;
  static constexpr std::array<const char*, 8> names = {
      "GX_DRAW_QUADS",        "GX_DRAW_QUADS_2 (nonstandard)",
      "GX_DRAW_TRIANGLES",    "GX_DRAW_TRIANGLE_STRIP",
      "GX_DRAW_TRIANGLE_FAN", "GX_DRAW_LINES",
      "GX_DRAW_LINE_STRIP",   "GX_DRAW_POINTS",
  };
  return fmt::format("{} VAT {}", names[primitive], vat);
}

void FIFOAnalyzer::UpdateDetails()
{
  // Clearing the detail list can update the selection, which causes UpdateDescription to be called
  // immediately.  However, the object data offsets have not been recalculated yet, which can cause
  // the wrong data to be used, potentially leading to out of bounds data or other bad things.
  // Clear m_object_data_offsets first, so that UpdateDescription exits immediately.
  m_object_data_offsets.clear();
  m_detail_list->clear();
  m_search_results.clear();
  m_search_next->setEnabled(false);
  m_search_previous->setEnabled(false);
  m_search_label->clear();

  if (!FifoPlayer::GetInstance().IsPlaying())
    return;

  const auto items = m_tree_widget->selectedItems();

  if (items.isEmpty() || items[0]->data(0, OBJECT_ROLE).isNull())
    return;

  const u32 frame_nr = items[0]->data(0, FRAME_ROLE).toUInt();
  const u32 object_nr = items[0]->data(0, OBJECT_ROLE).toUInt();

  const auto& frame_info = FifoPlayer::GetInstance().GetAnalyzedFrameInfo(frame_nr);
  const auto& fifo_frame = FifoPlayer::GetInstance().GetFile()->GetFrame(frame_nr);

  // Note that frame_info.objectStarts[object_nr] is the start of the primitive data,
  // but we want to start with the register updates which happen before that.
  const u32 object_start = (object_nr == 0 ? 0 : frame_info.objectEnds[object_nr - 1]);
  const u32 object_size = frame_info.objectEnds[object_nr] - object_start;

  const u8* const object = &fifo_frame.fifoData[object_start];

  u32 object_offset = 0;
  while (object_offset < object_size)
  {
    QString new_label;
    const u32 start_offset = object_offset;
    m_object_data_offsets.push_back(start_offset);

    const u8 command = object[object_offset++];
    switch (command)
    {
    case OpcodeDecoder::GX_NOP:
      if (object[object_offset] == OpcodeDecoder::GX_NOP)
      {
        u32 nop_count = 2;
        while (object[++object_offset] == OpcodeDecoder::GX_NOP)
          nop_count++;

        new_label = QStringLiteral("NOP (%1x)").arg(nop_count);
      }
      else
      {
        new_label = QStringLiteral("NOP");
      }
      break;

    case OpcodeDecoder::GX_CMD_UNKNOWN_METRICS:
      new_label = QStringLiteral("GX_CMD_UNKNOWN_METRICS");
      break;

    case OpcodeDecoder::GX_CMD_INVL_VC:
      new_label = QStringLiteral("GX_CMD_INVL_VC");
      break;

    case OpcodeDecoder::GX_LOAD_CP_REG:
    {
      const u8 cmd2 = object[object_offset++];
      const u32 value = Common::swap32(&object[object_offset]);
      object_offset += 4;

      const auto [name, desc] = GetCPRegInfo(cmd2, value);
      ASSERT(!name.empty());

      new_label = QStringLiteral("CP  %1  %2  %3")
                      .arg(cmd2, 2, 16, QLatin1Char('0'))
                      .arg(value, 8, 16, QLatin1Char('0'))
                      .arg(QString::fromStdString(name));
    }
    break;

    case OpcodeDecoder::GX_LOAD_XF_REG:
    {
      const auto [name, desc] = GetXFTransferInfo(&object[object_offset]);
      const u32 cmd2 = Common::swap32(&object[object_offset]);
      object_offset += 4;
      ASSERT(!name.empty());

      const u8 stream_size = ((cmd2 >> 16) & 15) + 1;

      new_label = QStringLiteral("XF  %1  ").arg(cmd2, 8, 16, QLatin1Char('0'));

      for (u8 i = 0; i < stream_size; i++)
      {
        const u32 value = Common::swap32(&object[object_offset]);
        object_offset += 4;

        new_label += QStringLiteral("%1 ").arg(value, 8, 16, QLatin1Char('0'));
      }

      new_label += QStringLiteral("  ") + QString::fromStdString(name);
    }
    break;

    case OpcodeDecoder::GX_LOAD_INDX_A:
    {
      const auto [desc, written] =
          GetXFIndexedLoadInfo(ARRAY_XF_A, Common::swap32(&object[object_offset]));
      object_offset += 4;
      new_label = QStringLiteral("LOAD INDX A   %1").arg(QString::fromStdString(desc));
    }
    break;
    case OpcodeDecoder::GX_LOAD_INDX_B:
    {
      const auto [desc, written] =
          GetXFIndexedLoadInfo(ARRAY_XF_B, Common::swap32(&object[object_offset]));
      object_offset += 4;
      new_label = QStringLiteral("LOAD INDX B   %1").arg(QString::fromStdString(desc));
    }
    break;
    case OpcodeDecoder::GX_LOAD_INDX_C:
    {
      const auto [desc, written] =
          GetXFIndexedLoadInfo(ARRAY_XF_C, Common::swap32(&object[object_offset]));
      object_offset += 4;
      new_label = QStringLiteral("LOAD INDX C   %1").arg(QString::fromStdString(desc));
    }
    break;
    case OpcodeDecoder::GX_LOAD_INDX_D:
    {
      const auto [desc, written] =
          GetXFIndexedLoadInfo(ARRAY_XF_D, Common::swap32(&object[object_offset]));
      object_offset += 4;
      new_label = QStringLiteral("LOAD INDX D   %1").arg(QString::fromStdString(desc));
    }
    break;

    case OpcodeDecoder::GX_CMD_CALL_DL:
      // The recorder should have expanded display lists into the fifo stream and skipped the
      // call to start them
      // That is done to make it easier to track where memory is updated
      ASSERT(false);
      object_offset += 8;
      new_label = QStringLiteral("CALL DL");
      break;

    case OpcodeDecoder::GX_LOAD_BP_REG:
    {
      const u8 cmd2 = object[object_offset++];
      const u32 cmddata = Common::swap24(&object[object_offset]);
      object_offset += 3;

      const auto [name, desc] = GetBPRegInfo(cmd2, cmddata);
      ASSERT(!name.empty());

      new_label = QStringLiteral("BP  %1  %2  %3")
                      .arg(cmd2, 2, 16, QLatin1Char('0'))
                      .arg(cmddata, 6, 16, QLatin1Char('0'))
                      .arg(QString::fromStdString(name));
    }
    break;

    default:
      if ((command & 0xC0) == 0x80)
      {
        // Object primitive data

        const u8 vat = command & OpcodeDecoder::GX_VAT_MASK;
        const auto& vtx_desc = frame_info.objectCPStates[object_nr].vtxDesc;
        const auto& vtx_attr = frame_info.objectCPStates[object_nr].vtxAttr[vat];

        const auto name = GetPrimitiveName(command);

        const u16 vertex_count = Common::swap16(&object[object_offset]);
        object_offset += 2;
        const u32 vertex_size = VertexLoaderBase::GetVertexSize(vtx_desc, vtx_attr);

        // Note that vertex_count is allowed to be 0, with no special treatment
        // (another command just comes right after the current command, with no vertices in between)
        const u32 object_prim_size = vertex_count * vertex_size;

        new_label = QStringLiteral("PRIMITIVE %1 (%2)  %3 vertices %4 bytes/vertex %5 total bytes")
                        .arg(QString::fromStdString(name))
                        .arg(command, 2, 16, QLatin1Char('0'))
                        .arg(vertex_count)
                        .arg(vertex_size)
                        .arg(object_prim_size);

        // It's not really useful to have a massive unreadable hex string for the object primitives.
        // Put it in the description instead.

// #define INCLUDE_HEX_IN_PRIMITIVES
#ifdef INCLUDE_HEX_IN_PRIMITIVES
        new_label += QStringLiteral("   ");
        for (u32 i = 0; i < object_prim_size; i++)
        {
          new_label += QStringLiteral("%1").arg(object[object_offset++], 2, 16, QLatin1Char('0'));
        }
#else
        object_offset += object_prim_size;
#endif
      }
      else
      {
        new_label = QStringLiteral("Unknown opcode %1").arg(command, 2, 16);
      }
      break;
    }
    new_label = QStringLiteral("%1:  ").arg(object_start + start_offset, 8, 16, QLatin1Char('0')) +
                new_label;
    m_detail_list->addItem(new_label);
  }

  ASSERT(object_offset == object_size);

  // Needed to ensure the description updates when changing objects
  m_detail_list->setCurrentRow(0);
}

void FIFOAnalyzer::BeginSearch()
{
  const QString search_str = m_search_edit->text();

  if (!FifoPlayer::GetInstance().IsPlaying())
    return;

  const auto items = m_tree_widget->selectedItems();

  if (items.isEmpty() || items[0]->data(0, FRAME_ROLE).isNull() ||
      items[0]->data(0, OBJECT_ROLE).isNull())
  {
    m_search_label->setText(tr("Invalid search parameters (no object selected)"));
    return;
  }

  // TODO: Remove even string length limit
  if (search_str.length() % 2)
  {
    m_search_label->setText(tr("Invalid search string (only even string lengths supported)"));
    return;
  }

  const size_t length = search_str.length() / 2;

  std::vector<u8> search_val;

  for (size_t i = 0; i < length; i++)
  {
    const QString byte_str = search_str.mid(static_cast<int>(i * 2), 2);

    bool good;
    u8 value = byte_str.toUInt(&good, 16);

    if (!good)
    {
      m_search_label->setText(tr("Invalid search string (couldn't convert to number)"));
      return;
    }

    search_val.push_back(value);
  }

  m_search_results.clear();

  const u32 frame_nr = items[0]->data(0, FRAME_ROLE).toUInt();
  const u32 object_nr = items[0]->data(0, OBJECT_ROLE).toUInt();

  const AnalyzedFrameInfo& frame_info = FifoPlayer::GetInstance().GetAnalyzedFrameInfo(frame_nr);
  const FifoFrameInfo& fifo_frame = FifoPlayer::GetInstance().GetFile()->GetFrame(frame_nr);

  const u32 object_start = (object_nr == 0 ? 0 : frame_info.objectEnds[object_nr - 1]);
  const u32 object_size = frame_info.objectEnds[object_nr] - object_start;

  const u8* const object = &fifo_frame.fifoData[object_start];

  // TODO: Support searching for bit patterns
  for (u32 cmd_nr = 0; cmd_nr < m_object_data_offsets.size(); cmd_nr++)
  {
    const u32 cmd_start = m_object_data_offsets[cmd_nr];
    const u32 cmd_end = (cmd_nr + 1 == m_object_data_offsets.size()) ?
                            object_size :
                            m_object_data_offsets[cmd_nr + 1];

    const u8* const cmd_start_ptr = &object[cmd_start];
    const u8* const cmd_end_ptr = &object[cmd_end];

    for (const u8* ptr = cmd_start_ptr; ptr < cmd_end_ptr - length + 1; ptr++)
    {
      if (std::equal(search_val.begin(), search_val.end(), ptr))
      {
        m_search_results.emplace_back(frame_nr, object_nr, cmd_nr);
        break;
      }
    }
  }

  ShowSearchResult(0);

  m_search_label->setText(
      tr("Found %1 results for \"%2\"").arg(m_search_results.size()).arg(search_str));
}

void FIFOAnalyzer::FindNext()
{
  const int index = m_detail_list->currentRow();
  ASSERT(index >= 0);

  auto next_result =
      std::find_if(m_search_results.begin(), m_search_results.end(),
                   [index](auto& result) { return result.m_cmd > static_cast<u32>(index); });
  if (next_result != m_search_results.end())
  {
    ShowSearchResult(next_result - m_search_results.begin());
  }
}

void FIFOAnalyzer::FindPrevious()
{
  const int index = m_detail_list->currentRow();
  ASSERT(index >= 0);

  auto prev_result =
      std::find_if(m_search_results.rbegin(), m_search_results.rend(),
                   [index](auto& result) { return result.m_cmd < static_cast<u32>(index); });
  if (prev_result != m_search_results.rend())
  {
    ShowSearchResult((m_search_results.rend() - prev_result) - 1);
  }
}

void FIFOAnalyzer::ShowSearchResult(size_t index)
{
  if (m_search_results.empty())
    return;

  if (index >= m_search_results.size())
  {
    ShowSearchResult(m_search_results.size() - 1);
    return;
  }

  const auto& result = m_search_results[index];

  QTreeWidgetItem* object_item =
      m_tree_widget->topLevelItem(0)->child(result.m_frame)->child(result.m_object);

  m_tree_widget->setCurrentItem(object_item);
  m_detail_list->setCurrentRow(result.m_cmd);

  m_search_next->setEnabled(index + 1 < m_search_results.size());
  m_search_previous->setEnabled(index > 0);
}

void FIFOAnalyzer::UpdateDescription()
{
  m_entry_detail_browser->clear();

  if (!FifoPlayer::GetInstance().IsPlaying())
    return;

  const auto items = m_tree_widget->selectedItems();

  if (items.isEmpty() || m_object_data_offsets.empty())
    return;

  if (items[0]->data(0, FRAME_ROLE).isNull() || items[0]->data(0, OBJECT_ROLE).isNull())
    return;

  const u32 frame_nr = items[0]->data(0, FRAME_ROLE).toUInt();
  const u32 object_nr = items[0]->data(0, OBJECT_ROLE).toUInt();
  const u32 entry_nr = m_detail_list->currentRow();

  const AnalyzedFrameInfo& frame_info = FifoPlayer::GetInstance().GetAnalyzedFrameInfo(frame_nr);
  const FifoFrameInfo& fifo_frame = FifoPlayer::GetInstance().GetFile()->GetFrame(frame_nr);

  const u32 object_start = (object_nr == 0 ? 0 : frame_info.objectEnds[object_nr - 1]);
  const u32 entry_start = m_object_data_offsets[entry_nr];

  const u8* cmddata = &fifo_frame.fifoData[object_start + entry_start];

  // TODO: Not sure whether we should bother translating the descriptions

  QString text;
  if (*cmddata == OpcodeDecoder::GX_LOAD_BP_REG)
  {
    const u8 cmd = *(cmddata + 1);
    const u32 value = Common::swap24(cmddata + 2);

    const auto [name, desc] = GetBPRegInfo(cmd, value);
    ASSERT(!name.empty());

    text = tr("BP register ");
    text += QString::fromStdString(name);
    text += QLatin1Char{'\n'};

    if (desc.empty())
      text += tr("No description available");
    else
      text += QString::fromStdString(desc);
  }
  else if (*cmddata == OpcodeDecoder::GX_LOAD_CP_REG)
  {
    const u8 cmd = *(cmddata + 1);
    const u32 value = Common::swap32(cmddata + 2);

    const auto [name, desc] = GetCPRegInfo(cmd, value);
    ASSERT(!name.empty());

    text = tr("CP register ");
    text += QString::fromStdString(name);
    text += QLatin1Char{'\n'};

    if (desc.empty())
      text += tr("No description available");
    else
      text += QString::fromStdString(desc);
  }
  else if (*cmddata == OpcodeDecoder::GX_LOAD_XF_REG)
  {
    const auto [name, desc] = GetXFTransferInfo(cmddata + 1);
    ASSERT(!name.empty());

    text = tr("XF register ");
    text += QString::fromStdString(name);
    text += QLatin1Char{'\n'};

    if (desc.empty())
      text += tr("No description available");
    else
      text += QString::fromStdString(desc);
  }
  else if (*cmddata == OpcodeDecoder::GX_LOAD_INDX_A)
  {
    const auto [desc, written] = GetXFIndexedLoadInfo(ARRAY_XF_A, Common::swap32(cmddata + 1));

    text = QString::fromStdString(desc);
    text += QLatin1Char{'\n'};
    text += tr("Usually used for position matrices");
    text += QLatin1Char{'\n'};
    text += QString::fromStdString(written);
  }
  else if (*cmddata == OpcodeDecoder::GX_LOAD_INDX_B)
  {
    const auto [desc, written] = GetXFIndexedLoadInfo(ARRAY_XF_B, Common::swap32(cmddata + 1));

    text = QString::fromStdString(desc);
    text += QLatin1Char{'\n'};
    // i18n: A normal matrix is a matrix used for transforming normal vectors. The word "normal"
    // does not have its usual meaning here, but rather the meaning of "perpendicular to a surface".
    text += tr("Usually used for normal matrices");
    text += QLatin1Char{'\n'};
    text += QString::fromStdString(written);
  }
  else if (*cmddata == OpcodeDecoder::GX_LOAD_INDX_C)
  {
    const auto [desc, written] = GetXFIndexedLoadInfo(ARRAY_XF_C, Common::swap32(cmddata + 1));

    text = QString::fromStdString(desc);
    text += QLatin1Char{'\n'};
    // i18n: Tex coord is short for texture coordinate
    text += tr("Usually used for tex coord matrices");
    text += QLatin1Char{'\n'};
    text += QString::fromStdString(written);
  }
  else if (*cmddata == OpcodeDecoder::GX_LOAD_INDX_D)
  {
    const auto [desc, written] = GetXFIndexedLoadInfo(ARRAY_XF_D, Common::swap32(cmddata + 1));

    text = QString::fromStdString(desc);
    text += QLatin1Char{'\n'};
    text += tr("Usually used for light objects");
    text += QLatin1Char{'\n'};
    text += QString::fromStdString(written);
  }
  else if ((*cmddata & 0xC0) == 0x80)
  {
    const u8 vat = *cmddata & OpcodeDecoder::GX_VAT_MASK;
    const QString name = QString::fromStdString(GetPrimitiveName(*cmddata));
    const u16 vertex_count = Common::swap16(cmddata + 1);

    // i18n: In this context, a primitive means a point, line, triangle or rectangle.
    // Do not translate the word primitive as if it was an adjective.
    text = tr("Primitive %1").arg(name);
    text += QLatin1Char{'\n'};

    const auto& vtx_desc = frame_info.objectCPStates[object_nr].vtxDesc;
    const auto& vtx_attr = frame_info.objectCPStates[object_nr].vtxAttr[vat];
    const auto component_sizes = VertexLoaderBase::GetVertexComponentSizes(vtx_desc, vtx_attr);

    u32 i = 3;
    for (u32 vertex_num = 0; vertex_num < vertex_count; vertex_num++)
    {
      text += QLatin1Char{'\n'};
      for (u32 comp_size : component_sizes)
      {
        for (u32 comp_off = 0; comp_off < comp_size; comp_off++)
        {
          text += QStringLiteral("%1").arg(cmddata[i++], 2, 16, QLatin1Char('0'));
        }
        text += QLatin1Char{' '};
      }
    }
  }
  else
  {
    text = tr("No description available");
  }

  m_entry_detail_browser->setText(text);
}

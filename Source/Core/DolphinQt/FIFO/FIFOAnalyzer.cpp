// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include "VideoCommon/OpcodeDecoding.h"

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

  connect(&Settings::Instance(), &Settings::DebugFontChanged, [this] {
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

  connect(m_search_new, &QPushButton::pressed, this, &FIFOAnalyzer::BeginSearch);
  connect(m_search_next, &QPushButton::pressed, this, &FIFOAnalyzer::FindNext);
  connect(m_search_previous, &QPushButton::pressed, this, &FIFOAnalyzer::FindPrevious);
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

  int object_count = FifoPlayer::GetInstance().GetFrameObjectCount();
  int frame_count = file->GetFrameCount();

  for (int i = 0; i < frame_count; i++)
  {
    auto* frame_item = new QTreeWidgetItem({tr("Frame %1").arg(i)});

    recording_item->addChild(frame_item);

    for (int j = 0; j < object_count; j++)
    {
      auto* object_item = new QTreeWidgetItem({tr("Object %1").arg(j)});

      frame_item->addChild(object_item);

      object_item->setData(0, FRAME_ROLE, i);
      object_item->setData(0, OBJECT_ROLE, j);
    }
  }
}

void FIFOAnalyzer::UpdateDetails()
{
  m_detail_list->clear();
  m_object_data_offsets.clear();

  auto items = m_tree_widget->selectedItems();

  if (items.isEmpty() || items[0]->data(0, OBJECT_ROLE).isNull())
    return;

  int frame_nr = items[0]->data(0, FRAME_ROLE).toInt();
  int object_nr = items[0]->data(0, OBJECT_ROLE).toInt();

  const auto& frame_info = FifoPlayer::GetInstance().GetAnalyzedFrameInfo(frame_nr);
  const auto& fifo_frame = FifoPlayer::GetInstance().GetFile()->GetFrame(frame_nr);

  const u8* objectdata_start = &fifo_frame.fifoData[frame_info.objectStarts[object_nr]];
  const u8* objectdata_end = &fifo_frame.fifoData[frame_info.objectEnds[object_nr]];
  const u8* objectdata = objectdata_start;
  const std::ptrdiff_t obj_offset =
      objectdata_start - &fifo_frame.fifoData[frame_info.objectStarts[0]];

  int cmd = *objectdata++;
  int stream_size = Common::swap16(objectdata);
  objectdata += 2;
  QString new_label = QStringLiteral("%1:  %2 %3  ")
                          .arg(obj_offset, 8, 16, QLatin1Char('0'))
                          .arg(cmd, 2, 16, QLatin1Char('0'))
                          .arg(stream_size, 4, 16, QLatin1Char('0'));
  if (stream_size && ((objectdata_end - objectdata) % stream_size))
    new_label += tr("NOTE: Stream size doesn't match actual data length\n");

  while (objectdata < objectdata_end)
    new_label += QStringLiteral("%1").arg(*objectdata++, 2, 16, QLatin1Char('0'));

  m_detail_list->addItem(new_label);
  m_object_data_offsets.push_back(0);

  // Between objectdata_end and next_objdata_start, there are register setting commands
  if (object_nr + 1 < static_cast<int>(frame_info.objectStarts.size()))
  {
    const u8* next_objdata_start = &fifo_frame.fifoData[frame_info.objectStarts[object_nr + 1]];
    while (objectdata < next_objdata_start)
    {
      m_object_data_offsets.push_back(objectdata - objectdata_start);
      int new_offset = objectdata - &fifo_frame.fifoData[frame_info.objectStarts[0]];
      int command = *objectdata++;
      switch (command)
      {
      case OpcodeDecoder::GX_NOP:
        new_label = QStringLiteral("NOP");
        break;

      case 0x44:
        new_label = QStringLiteral("0x44");
        break;

      case OpcodeDecoder::GX_CMD_INVL_VC:
        new_label = QStringLiteral("GX_CMD_INVL_VC");
        break;

      case OpcodeDecoder::GX_LOAD_CP_REG:
      {
        u32 cmd2 = *objectdata++;
        u32 value = Common::swap32(objectdata);
        objectdata += 4;

        new_label = QStringLiteral("CP  %1  %2")
                        .arg(cmd2, 2, 16, QLatin1Char('0'))
                        .arg(value, 8, 16, QLatin1Char('0'));
      }
      break;

      case OpcodeDecoder::GX_LOAD_XF_REG:
      {
        u32 cmd2 = Common::swap32(objectdata);
        objectdata += 4;

        u8 streamSize = ((cmd2 >> 16) & 15) + 1;

        const u8* stream_start = objectdata;
        const u8* stream_end = stream_start + streamSize * 4;

        new_label = QStringLiteral("XF  %1  ").arg(cmd2, 8, 16, QLatin1Char('0'));
        while (objectdata < stream_end)
        {
          new_label += QStringLiteral("%1").arg(*objectdata++, 2, 16, QLatin1Char('0'));

          if (((objectdata - stream_start) % 4) == 0)
            new_label += QLatin1Char(' ');
        }
      }
      break;

      case OpcodeDecoder::GX_LOAD_INDX_A:
      case OpcodeDecoder::GX_LOAD_INDX_B:
      case OpcodeDecoder::GX_LOAD_INDX_C:
      case OpcodeDecoder::GX_LOAD_INDX_D:
      {
        objectdata += 4;
        new_label = (command == OpcodeDecoder::GX_LOAD_INDX_A) ?
                        QStringLiteral("LOAD INDX A") :
                        (command == OpcodeDecoder::GX_LOAD_INDX_B) ?
                        QStringLiteral("LOAD INDX B") :
                        (command == OpcodeDecoder::GX_LOAD_INDX_C) ? QStringLiteral("LOAD INDX C") :
                                                                     QStringLiteral("LOAD INDX D");
      }
      break;

      case OpcodeDecoder::GX_CMD_CALL_DL:
        // The recorder should have expanded display lists into the fifo stream and skipped the
        // call to start them
        // That is done to make it easier to track where memory is updated
        ASSERT(false);
        objectdata += 8;
        new_label = QStringLiteral("CALL DL");
        break;

      case OpcodeDecoder::GX_LOAD_BP_REG:
      {
        u32 cmd2 = Common::swap32(objectdata);
        objectdata += 4;
        new_label = QStringLiteral("BP  %1 %2")
                        .arg(cmd2 >> 24, 2, 16, QLatin1Char('0'))
                        .arg(cmd2 & 0xFFFFFF, 6, 16, QLatin1Char('0'));
      }
      break;

      default:
        new_label = tr("Unexpected 0x80 call? Aborting...");
        objectdata = static_cast<const u8*>(next_objdata_start);
        break;
      }
      new_label = QStringLiteral("%1:  ").arg(new_offset, 8, 16, QLatin1Char('0')) + new_label;
      m_detail_list->addItem(new_label);
    }
  }
}

void FIFOAnalyzer::BeginSearch()
{
  QString search_str = m_search_edit->text();

  auto items = m_tree_widget->selectedItems();

  if (items.isEmpty() || items[0]->data(0, FRAME_ROLE).isNull())
    return;

  if (items[0]->data(0, OBJECT_ROLE).isNull())
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

  int frame_nr = items[0]->data(0, FRAME_ROLE).toInt();
  int object_nr = items[0]->data(0, OBJECT_ROLE).toInt();

  const AnalyzedFrameInfo& frame_info = FifoPlayer::GetInstance().GetAnalyzedFrameInfo(frame_nr);
  const FifoFrameInfo& fifo_frame = FifoPlayer::GetInstance().GetFile()->GetFrame(frame_nr);

  // TODO: Support searching through the last object...how do we know where the cmd data ends?
  // TODO: Support searching for bit patterns

  const auto* start_ptr = &fifo_frame.fifoData[frame_info.objectStarts[object_nr]];
  const auto* end_ptr = &fifo_frame.fifoData[frame_info.objectStarts[object_nr + 1]];

  for (const u8* ptr = start_ptr; ptr < end_ptr - length + 1; ++ptr)
  {
    if (std::equal(search_val.begin(), search_val.end(), ptr))
    {
      SearchResult result;
      result.frame = frame_nr;

      result.object = object_nr;
      result.cmd = 0;
      for (unsigned int cmd_nr = 1; cmd_nr < m_object_data_offsets.size(); ++cmd_nr)
      {
        if (ptr < start_ptr + m_object_data_offsets[cmd_nr])
        {
          result.cmd = cmd_nr - 1;
          break;
        }
      }
      m_search_results.push_back(result);
    }
  }

  ShowSearchResult(0);

  m_search_label->setText(
      tr("Found %1 results for \"%2\"").arg(m_search_results.size()).arg(search_str));
}

void FIFOAnalyzer::FindNext()
{
  int index = m_detail_list->currentRow();

  if (index == -1)
  {
    ShowSearchResult(0);
    return;
  }

  for (auto it = m_search_results.begin(); it != m_search_results.end(); ++it)
  {
    if (it->cmd > index)
    {
      ShowSearchResult(it - m_search_results.begin());
      return;
    }
  }
}

void FIFOAnalyzer::FindPrevious()
{
  int index = m_detail_list->currentRow();

  if (index == -1)
  {
    ShowSearchResult(m_search_results.size() - 1);
    return;
  }

  for (auto it = m_search_results.rbegin(); it != m_search_results.rend(); ++it)
  {
    if (it->cmd < index)
    {
      ShowSearchResult(m_search_results.size() - 1 - (it - m_search_results.rbegin()));
      return;
    }
  }
}

void FIFOAnalyzer::ShowSearchResult(size_t index)
{
  if (m_search_results.empty())
    return;

  if (index > m_search_results.size())
  {
    ShowSearchResult(m_search_results.size() - 1);
    return;
  }

  const auto& result = m_search_results[index];

  QTreeWidgetItem* object_item =
      m_tree_widget->topLevelItem(0)->child(result.frame)->child(result.object);

  m_tree_widget->setCurrentItem(object_item);
  m_detail_list->setCurrentRow(result.cmd);

  m_search_next->setEnabled(index + 1 < m_search_results.size());
  m_search_previous->setEnabled(index > 0);
}

void FIFOAnalyzer::UpdateDescription()
{
  m_entry_detail_browser->clear();

  auto items = m_tree_widget->selectedItems();

  if (items.isEmpty())
    return;

  int frame_nr = items[0]->data(0, FRAME_ROLE).toInt();
  int object_nr = items[0]->data(0, OBJECT_ROLE).toInt();
  int entry_nr = m_detail_list->currentRow();

  const AnalyzedFrameInfo& frame = FifoPlayer::GetInstance().GetAnalyzedFrameInfo(frame_nr);
  const FifoFrameInfo& fifo_frame = FifoPlayer::GetInstance().GetFile()->GetFrame(frame_nr);

  const u8* cmddata =
      &fifo_frame.fifoData[frame.objectStarts[object_nr]] + m_object_data_offsets[entry_nr];

  // TODO: Not sure whether we should bother translating the descriptions

  QString text;
  if (*cmddata == OpcodeDecoder::GX_LOAD_BP_REG)
  {
    std::string name;
    std::string desc;
    GetBPRegInfo(cmddata + 1, &name, &desc);

    text = tr("BP register ");
    text += name.empty() ?
                QStringLiteral("UNKNOWN_%1").arg(*(cmddata + 1), 2, 16, QLatin1Char('0')) :
                QString::fromStdString(name);
    text += QStringLiteral("\n");

    if (desc.empty())
      text += tr("No description available");
    else
      text += QString::fromStdString(desc);
  }
  else if (*cmddata == OpcodeDecoder::GX_LOAD_CP_REG)
  {
    text = tr("CP register ");
  }
  else if (*cmddata == OpcodeDecoder::GX_LOAD_XF_REG)
  {
    text = tr("XF register ");
  }
  else
  {
    text = tr("No description available");
  }

  m_entry_detail_browser->setText(text);
}

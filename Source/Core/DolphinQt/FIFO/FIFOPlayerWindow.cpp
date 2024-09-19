// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/FIFO/FIFOPlayerWindow.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

#include <algorithm>

#include "Core/Core.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/System.h"

#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/FIFO/FIFOAnalyzer.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

FIFOPlayerWindow::FIFOPlayerWindow(FifoPlayer& fifo_player, FifoRecorder& fifo_recorder,
                                   QWidget* parent)
    : QWidget(parent), m_fifo_player(fifo_player), m_fifo_recorder(fifo_recorder)
{
  setWindowTitle(tr("FIFO Player"));
  setWindowIcon(Resources::GetAppIcon());

  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();

  UpdateInfo();

  UpdateControls();

  m_fifo_player.SetFileLoadedCallback(
      [this] { QueueOnObject(this, &FIFOPlayerWindow::OnFIFOLoaded); });
  m_fifo_player.SetFrameWrittenCallback([this] {
    QueueOnObject(this, [this] {
      UpdateInfo();
      UpdateControls();
    });
  });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    if (state == Core::State::Running && m_emu_state != Core::State::Paused)
      OnEmulationStarted();
    else if (state == Core::State::Uninitialized)
      OnEmulationStopped();
    m_emu_state = state;
  });

  installEventFilter(this);
}

FIFOPlayerWindow::~FIFOPlayerWindow()
{
  m_fifo_player.SetFileLoadedCallback({});
  m_fifo_player.SetFrameWrittenCallback({});
}

void FIFOPlayerWindow::CreateWidgets()
{
  auto* layout = new QVBoxLayout;

  // Info
  auto* info_group = new QGroupBox(tr("File Info"));
  auto* info_layout = new QHBoxLayout;

  m_info_label = new QLabel;
  info_layout->addWidget(m_info_label);
  info_group->setLayout(info_layout);

  m_info_label->setFixedHeight(QFontMetrics(font()).lineSpacing() * 3);

  // Object Range
  auto* object_range_group = new QGroupBox(tr("Object Range"));
  auto* object_range_layout = new QHBoxLayout;

  m_object_range_from = new QSpinBox;
  m_object_range_from_label = new QLabel(tr("From:"));
  m_object_range_to = new QSpinBox;
  m_object_range_to_label = new QLabel(tr("To:"));

  object_range_layout->addWidget(m_object_range_from_label);
  object_range_layout->addWidget(m_object_range_from);
  object_range_layout->addWidget(m_object_range_to_label);
  object_range_layout->addWidget(m_object_range_to);
  object_range_group->setLayout(object_range_layout);

  // Frame Range
  auto* frame_range_group = new QGroupBox(tr("Frame Range"));
  auto* frame_range_layout = new QHBoxLayout;

  m_frame_range_from = new QSpinBox;
  m_frame_range_from_label = new QLabel(tr("From:"));
  m_frame_range_to = new QSpinBox;
  m_frame_range_to_label = new QLabel(tr("To:"));

  frame_range_layout->addWidget(m_frame_range_from_label);
  frame_range_layout->addWidget(m_frame_range_from);
  frame_range_layout->addWidget(m_frame_range_to_label);
  frame_range_layout->addWidget(m_frame_range_to);
  frame_range_group->setLayout(frame_range_layout);

  // Playback Options
  auto* playback_group = new QGroupBox(tr("Playback Options"));
  auto* playback_layout = new QGridLayout;
  m_early_memory_updates = new ToolTipCheckBox(tr("Early Memory Updates"));
  m_loop = new ToolTipCheckBox(tr("Loop"));

  playback_layout->addWidget(object_range_group, 0, 0);
  playback_layout->addWidget(frame_range_group, 0, 1);
  playback_layout->addWidget(m_early_memory_updates, 1, 0);
  playback_layout->addWidget(m_loop, 1, 1);
  playback_group->setLayout(playback_layout);

  // Recording Options
  auto* recording_group = new QGroupBox(tr("Recording Options"));
  auto* recording_layout = new QHBoxLayout;
  m_frame_record_count = new QSpinBox;
  m_frame_record_count_label = new QLabel(tr("Frames to Record:"));

  m_frame_record_count->setMinimum(1);
  m_frame_record_count->setMaximum(3600);
  m_frame_record_count->setValue(3);

  recording_layout->addWidget(m_frame_record_count_label);
  recording_layout->addWidget(m_frame_record_count);
  recording_group->setLayout(recording_layout);

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  // Action Buttons
  m_load = m_button_box->addButton(tr("Load..."), QDialogButtonBox::ActionRole);
  m_save = m_button_box->addButton(tr("Save..."), QDialogButtonBox::ActionRole);
  m_record = m_button_box->addButton(tr("Record"), QDialogButtonBox::ActionRole);
  m_stop = m_button_box->addButton(tr("Stop"), QDialogButtonBox::ActionRole);

  layout->addWidget(info_group);
  layout->addWidget(playback_group);
  layout->addWidget(recording_group);
  layout->addWidget(m_button_box);

  m_main_widget = new QWidget(this);
  m_main_widget->setLayout(layout);

  m_tab_widget = new QTabWidget(this);

  m_analyzer = new FIFOAnalyzer(m_fifo_player);

  m_tab_widget->addTab(m_main_widget, tr("Play / Record"));
  m_tab_widget->addTab(m_analyzer, tr("Analyze"));

  auto* tab_layout = new QVBoxLayout;
  tab_layout->addWidget(m_tab_widget);

  setLayout(tab_layout);
}

void FIFOPlayerWindow::LoadSettings()
{
  m_early_memory_updates->setChecked(Config::Get(Config::MAIN_FIFOPLAYER_EARLY_MEMORY_UPDATES));
  m_loop->setChecked(Config::Get(Config::MAIN_FIFOPLAYER_LOOP_REPLAY));
}

void FIFOPlayerWindow::ConnectWidgets()
{
  connect(m_load, &QPushButton::clicked, this, &FIFOPlayerWindow::LoadRecording);
  connect(m_save, &QPushButton::clicked, this, &FIFOPlayerWindow::SaveRecording);
  connect(m_record, &QPushButton::clicked, this, &FIFOPlayerWindow::StartRecording);
  connect(m_stop, &QPushButton::clicked, this, &FIFOPlayerWindow::StopRecording);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &FIFOPlayerWindow::hide);
  connect(m_early_memory_updates, &QCheckBox::toggled, this, &FIFOPlayerWindow::OnConfigChanged);
  connect(m_loop, &QCheckBox::toggled, this, &FIFOPlayerWindow::OnConfigChanged);

  connect(m_frame_range_from, &QSpinBox::valueChanged, this, &FIFOPlayerWindow::OnLimitsChanged);
  connect(m_frame_range_to, &QSpinBox::valueChanged, this, &FIFOPlayerWindow::OnLimitsChanged);

  connect(m_object_range_from, &QSpinBox::valueChanged, this, &FIFOPlayerWindow::OnLimitsChanged);
  connect(m_object_range_to, &QSpinBox::valueChanged, this, &FIFOPlayerWindow::OnLimitsChanged);
}

void FIFOPlayerWindow::AddDescriptions()
{
  static const char TR_MEMORY_UPDATES_DESCRIPTION[] = QT_TR_NOOP(
      "If enabled, then all memory updates happen at once before the first frame.<br><br>"
      "Causes issues with many fifologs, but can be useful for testing.<br><br>"
      "<dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_LOOP_DESCRIPTION[] =
      QT_TR_NOOP("If unchecked, then playback of the fifolog stops after the final frame.<br><br>"
                 "This is generally only useful when a frame-dumping option is enabled.<br><br>"
                 "<dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");

  m_early_memory_updates->SetDescription(tr(TR_MEMORY_UPDATES_DESCRIPTION));
  m_loop->SetDescription(tr(TR_LOOP_DESCRIPTION));
}

void FIFOPlayerWindow::LoadRecording()
{
  QString path = DolphinFileDialog::getOpenFileName(this, tr("Open FIFO Log"), QString(),
                                                    tr("Dolphin FIFO Log (*.dff)"));

  if (path.isEmpty())
    return;

  emit LoadFIFORequested(path);
}

void FIFOPlayerWindow::SaveRecording()
{
  QString path = DolphinFileDialog::getSaveFileName(this, tr("Save FIFO Log"), QString(),
                                                    tr("Dolphin FIFO Log (*.dff)"));

  if (path.isEmpty())
    return;

  FifoDataFile* file = m_fifo_recorder.GetRecordedFile();

  bool result = file->Save(path.toStdString());

  if (!result)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Failed to save FIFO log."));
  }
}

void FIFOPlayerWindow::StartRecording()
{
  // Start recording
  m_fifo_recorder.StartRecording(m_frame_record_count->value(),
                                 [this] { QueueOnObject(this, [this] { OnRecordingDone(); }); });

  UpdateControls();

  UpdateInfo();
}

void FIFOPlayerWindow::StopRecording()
{
  m_fifo_recorder.StopRecording();

  UpdateControls();
  UpdateInfo();
}

void FIFOPlayerWindow::OnEmulationStarted()
{
  UpdateControls();

  if (m_fifo_player.GetFile())
    OnFIFOLoaded();
}

void FIFOPlayerWindow::OnEmulationStopped()
{
  // If we have previously been recording, stop now.
  if (m_fifo_recorder.IsRecording())
    StopRecording();

  UpdateControls();
  // When emulation stops, switch away from the analyzer tab, as it no longer shows anything useful
  m_tab_widget->setCurrentWidget(m_main_widget);
  m_analyzer->Update();
}

void FIFOPlayerWindow::OnRecordingDone()
{
  UpdateInfo();
  UpdateControls();
}

void FIFOPlayerWindow::UpdateInfo()
{
  if (m_fifo_player.IsPlaying())
  {
    FifoDataFile* file = m_fifo_player.GetFile();
    m_info_label->setText(tr("%1 frame(s)\n%2 object(s)\nCurrent Frame: %3")
                              .arg(QString::number(file->GetFrameCount()),
                                   QString::number(m_fifo_player.GetCurrentFrameObjectCount()),
                                   QString::number(m_fifo_player.GetCurrentFrameNum())));
    return;
  }

  if (m_fifo_recorder.IsRecordingDone())
  {
    FifoDataFile* file = m_fifo_recorder.GetRecordedFile();
    size_t fifo_bytes = 0;
    size_t mem_bytes = 0;

    for (u32 i = 0; i < file->GetFrameCount(); ++i)
    {
      fifo_bytes += file->GetFrame(i).fifoData.size();
      for (const auto& mem_update : file->GetFrame(i).memoryUpdates)
        mem_bytes += mem_update.data.size();
    }

    m_info_label->setText(tr("%1 FIFO bytes\n%2 memory bytes\n%3 frames")
                              .arg(QString::number(fifo_bytes), QString::number(mem_bytes),
                                   QString::number(file->GetFrameCount())));
    return;
  }

  if (Core::IsRunning(Core::System::GetInstance()) && m_fifo_recorder.IsRecording())
  {
    m_info_label->setText(tr("Recording..."));
    return;
  }

  m_info_label->setText(tr("No file loaded / recorded."));
}

void FIFOPlayerWindow::OnFIFOLoaded()
{
  FifoDataFile* file = m_fifo_player.GetFile();

  auto object_count = m_fifo_player.GetMaxObjectCount();
  auto frame_count = file->GetFrameCount();

  m_frame_range_to->setMaximum(frame_count - 1);
  m_object_range_to->setMaximum(object_count - 1);

  m_frame_range_from->setValue(0);
  m_object_range_from->setValue(0);
  m_frame_range_to->setValue(frame_count - 1);
  m_object_range_to->setValue(object_count - 1);

  UpdateInfo();
  UpdateLimits();
  UpdateControls();

  m_analyzer->Update();
}

void FIFOPlayerWindow::OnConfigChanged()
{
  Config::SetBase(Config::MAIN_FIFOPLAYER_EARLY_MEMORY_UPDATES,
                  m_early_memory_updates->isChecked());
  Config::SetBase(Config::MAIN_FIFOPLAYER_LOOP_REPLAY, m_loop->isChecked());
}

void FIFOPlayerWindow::OnLimitsChanged()
{
  FifoPlayer& player = m_fifo_player;

  player.SetFrameRangeStart(m_frame_range_from->value());
  player.SetFrameRangeEnd(m_frame_range_to->value());
  player.SetObjectRangeStart(m_object_range_from->value());
  player.SetObjectRangeEnd(m_object_range_to->value());
  UpdateLimits();
}

void FIFOPlayerWindow::UpdateLimits()
{
  m_frame_range_from->setMaximum(m_frame_range_to->value());
  m_frame_range_to->setMinimum(m_frame_range_from->value());
  m_object_range_from->setMaximum(m_object_range_to->value());
  m_object_range_to->setMinimum(m_object_range_from->value());
}

void FIFOPlayerWindow::UpdateControls()
{
  bool running = Core::IsRunning(Core::System::GetInstance());
  bool is_recording = m_fifo_recorder.IsRecording();
  bool is_playing = m_fifo_player.IsPlaying();

  m_frame_range_from->setEnabled(is_playing);
  m_frame_range_from_label->setEnabled(is_playing);
  m_frame_range_to->setEnabled(is_playing);
  m_frame_range_to_label->setEnabled(is_playing);
  m_object_range_from->setEnabled(is_playing);
  m_object_range_from_label->setEnabled(is_playing);
  m_object_range_to->setEnabled(is_playing);
  m_object_range_to_label->setEnabled(is_playing);

  bool enable_frame_record_count = !is_playing && !is_recording;

  m_frame_record_count_label->setEnabled(enable_frame_record_count);
  m_frame_record_count->setEnabled(enable_frame_record_count);

  m_load->setEnabled(!running);
  m_record->setEnabled(running && !is_playing);

  m_stop->setVisible(running && is_recording);
  m_record->setVisible(!m_stop->isVisible());

  m_save->setEnabled(m_fifo_recorder.IsRecordingDone());
}

bool FIFOPlayerWindow::eventFilter(QObject* object, QEvent* event)
{
  // Close when escape is pressed
  if (event->type() == QEvent::KeyPress)
  {
    if (static_cast<QKeyEvent*>(event)->matches(QKeySequence::Cancel))
      hide();
  }

  return false;
}

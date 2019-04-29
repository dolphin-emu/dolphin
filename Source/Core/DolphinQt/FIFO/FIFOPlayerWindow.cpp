// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/FIFO/FIFOPlayerWindow.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "Core/Core.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/FifoPlayer/FifoPlaybackAnalyzer.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"

#include "DolphinQt/FIFO/FIFOAnalyzer.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

FIFOPlayerWindow::FIFOPlayerWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("FIFO Player"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateWidgets();
  ConnectWidgets();

  UpdateInfo();

  UpdateControls();

  FifoPlayer::GetInstance().SetFileLoadedCallback(
      [this] { QueueOnObject(this, &FIFOPlayerWindow::OnFIFOLoaded); });
  FifoPlayer::GetInstance().SetFrameWrittenCallback([this] {
    QueueOnObject(this, [this] {
      UpdateInfo();
      UpdateControls();
    });
  });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    if (state == Core::State::Running)
      OnEmulationStarted();
    else if (state == Core::State::Uninitialized)
      OnEmulationStopped();
  });
}

FIFOPlayerWindow::~FIFOPlayerWindow()
{
  FifoPlayer::GetInstance().SetFileLoadedCallback({});
  FifoPlayer::GetInstance().SetFrameWrittenCallback({});
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
  m_early_memory_updates = new QCheckBox(tr("Early Memory Updates"));

  playback_layout->addWidget(object_range_group, 0, 0);
  playback_layout->addWidget(frame_range_group, 0, 1);
  playback_layout->addWidget(m_early_memory_updates, 1, 0, 1, -1);
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

  QWidget* main_widget = new QWidget(this);
  main_widget->setLayout(layout);

  auto* tab_widget = new QTabWidget(this);

  m_analyzer = new FIFOAnalyzer;

  tab_widget->addTab(main_widget, tr("Play / Record"));
  tab_widget->addTab(m_analyzer, tr("Analyze"));

  auto* tab_layout = new QVBoxLayout;
  tab_layout->addWidget(tab_widget);

  setLayout(tab_layout);
}

void FIFOPlayerWindow::ConnectWidgets()
{
  connect(m_load, &QPushButton::clicked, this, &FIFOPlayerWindow::LoadRecording);
  connect(m_save, &QPushButton::pressed, this, &FIFOPlayerWindow::SaveRecording);
  connect(m_record, &QPushButton::pressed, this, &FIFOPlayerWindow::StartRecording);
  connect(m_stop, &QPushButton::pressed, this, &FIFOPlayerWindow::StopRecording);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &FIFOPlayerWindow::reject);
  connect(m_early_memory_updates, &QCheckBox::toggled, this,
          &FIFOPlayerWindow::OnEarlyMemoryUpdatesChanged);
  connect(m_frame_range_from, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &FIFOPlayerWindow::OnLimitsChanged);
  connect(m_frame_range_to, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &FIFOPlayerWindow::OnLimitsChanged);

  connect(m_object_range_from, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &FIFOPlayerWindow::OnLimitsChanged);
  connect(m_object_range_to, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &FIFOPlayerWindow::OnLimitsChanged);
}

void FIFOPlayerWindow::LoadRecording()
{
  QString path = QFileDialog::getOpenFileName(this, tr("Open FIFO log"), QString(),
                                              tr("Dolphin FIFO Log (*.dff)"));

  if (path.isEmpty())
    return;

  emit LoadFIFORequested(path);
}

void FIFOPlayerWindow::SaveRecording()
{
  QString path = QFileDialog::getSaveFileName(this, tr("Save FIFO log"), QString(),
                                              tr("Dolphin FIFO Log (*.dff)"));

  if (path.isEmpty())
    return;

  FifoDataFile* file = FifoRecorder::GetInstance().GetRecordedFile();

  bool result = file->Save(path.toStdString());

  if (!result)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Failed to save FIFO log."));
  }
}

void FIFOPlayerWindow::StartRecording()
{
  // Start recording
  FifoRecorder::GetInstance().StartRecording(m_frame_record_count->value(), [this] {
    QueueOnObject(this, [this] { OnRecordingDone(); });
  });

  UpdateControls();

  UpdateInfo();
}

void FIFOPlayerWindow::StopRecording()
{
  FifoRecorder::GetInstance().StopRecording();

  UpdateControls();
  UpdateInfo();
}

void FIFOPlayerWindow::OnEmulationStarted()
{
  UpdateControls();

  if (FifoPlayer::GetInstance().GetFile())
    OnFIFOLoaded();
}

void FIFOPlayerWindow::OnEmulationStopped()
{
  // If we have previously been recording, stop now.
  if (FifoRecorder::GetInstance().IsRecording())
    StopRecording();

  UpdateControls();
}

void FIFOPlayerWindow::OnRecordingDone()
{
  UpdateInfo();
  UpdateControls();
}

void FIFOPlayerWindow::UpdateInfo()
{
  if (FifoPlayer::GetInstance().IsPlaying())
  {
    FifoDataFile* file = FifoPlayer::GetInstance().GetFile();
    m_info_label->setText(
        tr("%1 frame(s)\n%2 object(s)\nCurrent Frame: %3")
            .arg(QString::number(file->GetFrameCount()),
                 QString::number(FifoPlayer::GetInstance().GetFrameObjectCount()),
                 QString::number(FifoPlayer::GetInstance().GetCurrentFrameNum())));
    return;
  }

  if (FifoRecorder::GetInstance().IsRecordingDone())
  {
    FifoDataFile* file = FifoRecorder::GetInstance().GetRecordedFile();
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

  if (Core::IsRunning() && FifoRecorder::GetInstance().IsRecording())
  {
    m_info_label->setText(tr("Recording..."));
    return;
  }

  m_info_label->setText(tr("No file loaded / recorded."));
}

void FIFOPlayerWindow::OnFIFOLoaded()
{
  FifoDataFile* file = FifoPlayer::GetInstance().GetFile();

  auto object_count = FifoPlayer::GetInstance().GetFrameObjectCount();
  auto frame_count = file->GetFrameCount();

  m_frame_range_to->setMaximum(frame_count);
  m_object_range_to->setMaximum(object_count);

  m_frame_range_to->setValue(frame_count);
  m_object_range_to->setValue(object_count);

  UpdateInfo();
  UpdateLimits();
  UpdateControls();

  m_analyzer->Update();
}

void FIFOPlayerWindow::OnEarlyMemoryUpdatesChanged(bool enabled)
{
  FifoPlayer::GetInstance().SetEarlyMemoryUpdates(enabled);
}

void FIFOPlayerWindow::OnLimitsChanged()
{
  FifoPlayer& player = FifoPlayer::GetInstance();

  player.SetFrameRangeStart(m_frame_range_from->value());
  player.SetFrameRangeEnd(m_frame_range_to->value());
  player.SetObjectRangeStart(m_object_range_from->value());
  player.SetObjectRangeEnd(m_object_range_to->value());
  UpdateLimits();
}

void FIFOPlayerWindow::UpdateLimits()
{
  m_frame_range_from->setMaximum(std::max(m_frame_range_to->value() - 1, 0));
  m_frame_range_to->setMinimum(m_frame_range_from->value() + 1);
  m_object_range_from->setMaximum(std::max(m_object_range_to->value() - 1, 0));
  m_object_range_to->setMinimum(m_object_range_from->value() + 1);
}

void FIFOPlayerWindow::UpdateControls()
{
  bool running = Core::IsRunning();
  bool is_recording = FifoRecorder::GetInstance().IsRecording();
  bool is_playing = FifoPlayer::GetInstance().IsPlaying();

  m_frame_range_from->setEnabled(is_playing);
  m_frame_range_from_label->setEnabled(is_playing);
  m_frame_range_to->setEnabled(is_playing);
  m_frame_range_to_label->setEnabled(is_playing);
  m_object_range_from->setEnabled(is_playing);
  m_object_range_from_label->setEnabled(is_playing);
  m_object_range_to->setEnabled(is_playing);
  m_object_range_to_label->setEnabled(is_playing);

  m_early_memory_updates->setEnabled(is_playing);

  bool enable_frame_record_count = !is_playing && !is_recording;

  m_frame_record_count_label->setEnabled(enable_frame_record_count);
  m_frame_record_count->setEnabled(enable_frame_record_count);

  m_load->setEnabled(!running);
  m_record->setEnabled(running && !is_playing);

  m_stop->setVisible(running && is_recording);
  m_record->setVisible(!m_stop->isVisible());

  m_save->setEnabled(FifoRecorder::GetInstance().IsRecordingDone());
}

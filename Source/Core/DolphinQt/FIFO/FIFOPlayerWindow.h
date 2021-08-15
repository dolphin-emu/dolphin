// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include "Core/Core.h"

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSpinBox;
class FIFOAnalyzer;

class FIFOPlayerWindow : public QWidget
{
  Q_OBJECT
public:
  explicit FIFOPlayerWindow(QWidget* parent = nullptr);
  ~FIFOPlayerWindow();

signals:
  void LoadFIFORequested(const QString& path);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadRecording();
  void SaveRecording();
  void StartRecording();
  void StopRecording();

  void OnEmulationStarted();
  void OnEmulationStopped();
  void OnLimitsChanged();
  void OnEarlyMemoryUpdatesChanged(bool enabled);
  void OnRecordingDone();
  void OnFIFOLoaded();

  void UpdateControls();
  void UpdateInfo();
  void UpdateLimits();

  bool eventFilter(QObject* object, QEvent* event) final override;

  QLabel* m_info_label;
  QPushButton* m_load;
  QPushButton* m_save;
  QPushButton* m_record;
  QPushButton* m_stop;
  QSpinBox* m_frame_range_from;
  QLabel* m_frame_range_from_label;
  QSpinBox* m_frame_range_to;
  QLabel* m_frame_range_to_label;
  QSpinBox* m_frame_record_count;
  QLabel* m_frame_record_count_label;
  QSpinBox* m_object_range_from;
  QLabel* m_object_range_from_label;
  QSpinBox* m_object_range_to;
  QLabel* m_object_range_to_label;
  QCheckBox* m_early_memory_updates;
  QDialogButtonBox* m_button_box;

  FIFOAnalyzer* m_analyzer;
  Core::State m_emu_state = Core::State::Uninitialized;
};

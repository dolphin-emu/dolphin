// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSpinBox;
class FIFOAnalyzer;

class FIFOPlayerWindow : public QDialog
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
};

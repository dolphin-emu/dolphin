// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include "Core/Core.h"

class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class ToolTipCheckBox;
class FifoPlayer;
class FifoRecorder;
class FIFOAnalyzer;

class FIFOPlayerWindow : public QWidget
{
  Q_OBJECT
public:
  explicit FIFOPlayerWindow(FifoPlayer& fifo_player, FifoRecorder& fifo_recorder,
                            QWidget* parent = nullptr);
  ~FIFOPlayerWindow();

signals:
  void LoadFIFORequested(const QString& path);

private:
  void CreateWidgets();
  void LoadSettings();
  void ConnectWidgets();
  void AddDescriptions();

  void LoadRecording();
  void SaveRecording();
  void StartRecording();
  void StopRecording();

  void OnEmulationStarted();
  void OnEmulationStopped();
  void OnLimitsChanged();
  void OnRecordingDone();
  void OnFIFOLoaded();
  void OnConfigChanged();

  void UpdateControls();
  void UpdateInfo();
  void UpdateLimits();

  bool eventFilter(QObject* object, QEvent* event) final override;

  FifoPlayer& m_fifo_player;
  FifoRecorder& m_fifo_recorder;

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
  ToolTipCheckBox* m_early_memory_updates;
  ToolTipCheckBox* m_loop;
  QDialogButtonBox* m_button_box;

  QWidget* m_main_widget;
  QTabWidget* m_tab_widget;

  FIFOAnalyzer* m_analyzer;
  Core::State m_emu_state = Core::State::Uninitialized;
};

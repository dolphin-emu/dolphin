#pragma once

#include <vector>

#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

class QLineEdit;

namespace Core
{
enum class State;
}

class SlippiPane final : public QWidget
{
  Q_OBJECT
public:
  explicit SlippiPane(QWidget* parent = nullptr);

private:
  void BrowseReplayFolder();
  void CreateLayout();
  void LoadConfig();
  void ConnectLayout();
  void OnSaveConfig();

  QVBoxLayout* m_main_layout;

  // Playback settings
  QGroupBox* m_replay_settings;
  QVBoxLayout* m_replay_settings_layout;
  QCheckBox* m_enable_replay_save;
  QLineEdit* m_replay_folder_edit;
};

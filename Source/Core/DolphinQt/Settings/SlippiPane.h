#pragma once

#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
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
  void SetSaveReplays(bool checked);
  void BrowseReplayFolder();
  void ToggleJukebox(bool checked);
  void SetForceNetplayPort(bool checked);
  void OnMusicVolumeUpdate(int volume);
  void CreateLayout();
  void LoadConfig();
  void ConnectLayout();
  void OnSaveConfig();

  QVBoxLayout* m_main_layout;

  // Replay Settings
  QCheckBox* m_save_replays;
  QCheckBox* m_monthly_replay_folders;
  QLineEdit* m_replay_folder_edit;
  QPushButton* m_replay_folder_open;

  // Online Settings
  QSpinBox* m_delay_spin;
  QComboBox* m_netplay_quick_chat_combo;
  QCheckBox* m_force_netplay_port;
  QSpinBox* m_netplay_port;

  // Jukebox Settings
  QCheckBox* m_enable_jukebox;
  QSlider* m_music_volume_slider;
  QLabel* m_music_volume_percent;
};

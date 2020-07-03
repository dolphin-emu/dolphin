#pragma once

#include <vector>

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;

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

  QCheckBox* m_enable_replay_save_checkbox;
  QCheckBox* m_enable_monthly_replay_folders_checkbox;
  QLineEdit* m_replay_folder_edit;
  QSpinBox* m_delay_spin;
};

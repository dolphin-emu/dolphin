#pragma once

#include <vector>

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

  QLineEdit* m_replay_folder_edit;
};

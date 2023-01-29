// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QFileInfo>
#include <QPushButton>

class GraphicsImage final : public QPushButton
{
  Q_OBJECT
public:
  explicit GraphicsImage(QWidget* parent = nullptr);

  std::string GetPath() const { return m_file_info.filePath().toStdString(); }

signals:
  void PathIsUpdated();
public slots:
  void UpdateImage();
  void SelectImage();

private:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  QFileInfo m_file_info;
};

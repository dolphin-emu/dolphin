// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GraphicsImage.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>

GraphicsImage::GraphicsImage(QWidget* parent)
    : QPushButton(QStringLiteral("Select image..."), parent)
{
  connect(this, SIGNAL(clicked()), this, SLOT(SelectImage()));
  setAcceptDrops(true);
}

void GraphicsImage::UpdateImage()
{
  setText(m_file_info.fileName());
  emit PathIsUpdated();
}

void GraphicsImage::SelectImage()
{
  QString path =
      QFileDialog::getOpenFileName(this, tr("Select image"), QString(), tr("PNG files (*.png)"));

  if (path.isEmpty())
    return;

  m_file_info = QFileInfo(path);
  UpdateImage();
}

void GraphicsImage::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
    event->acceptProposedAction();
}

void GraphicsImage::dropEvent(QDropEvent* event)
{
  const auto& urls = event->mimeData()->urls();
  if (urls.empty())
    return;

  const auto& url = urls[0];
  QFileInfo file_info(url.toLocalFile());

  if (!file_info.exists() || !file_info.isReadable())
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("Failed to open '%1'").arg(file_info.filePath()));
    return;
  }

  if (!file_info.isFile())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Not a file '%1'").arg(file_info.filePath()));
    return;
  }

  if (file_info.completeSuffix().toLower() != QStringLiteral("png"))
  {
    ModalMessageBox::critical(this, tr("Error"),
                              tr("Not a png file '%1'").arg(file_info.filePath()));
    return;
  }
  m_file_info = file_info;
  UpdateImage();
}

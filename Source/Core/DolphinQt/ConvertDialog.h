// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QDialog>
#include <QList>

#include "DiscIO/Blob.h"

class QCheckBox;
class QComboBox;

namespace UICommon
{
class GameFile;
}

class ConvertDialog final : public QDialog
{
  Q_OBJECT

public:
  explicit ConvertDialog(QList<std::shared_ptr<const UICommon::GameFile>> files,
                         QWidget* parent = nullptr);

private slots:
  void OnFormatChanged();
  void Convert();

private:
  void AddToFormatComboBox(const QString& name, DiscIO::BlobType format);
  void AddToBlockSizeComboBox(int size);

  bool ShowAreYouSureDialog(const QString& text);

  QComboBox* m_format;
  QComboBox* m_block_size;
  QCheckBox* m_scrub;
  QList<std::shared_ptr<const UICommon::GameFile>> m_files;
};

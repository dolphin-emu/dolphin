// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>
#include <QList>

#include "DiscIO/Blob.h"

class QCheckBox;
class QComboBox;

namespace DiscIO
{
enum class WIARVZCompressionType : u32;
}

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
  void OnCompressionChanged();
  void Convert();

private:
  void AddToBlockSizeComboBox(int size);
  void AddToCompressionComboBox(const QString& name, DiscIO::WIARVZCompressionType type);
  void AddToCompressionLevelComboBox(int level);

  bool ShowAreYouSureDialog(const QString& text);

  QComboBox* m_format;
  QComboBox* m_block_size;
  QComboBox* m_compression;
  QComboBox* m_compression_level;
  QCheckBox* m_scrub;
  QList<std::shared_ptr<const UICommon::GameFile>> m_files;
};

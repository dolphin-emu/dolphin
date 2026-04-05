// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QMap>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>

class QCompleter;
class QMenu;
class QTextEdit;

class GameConfigEdit : public QWidget
{
public:
  explicit GameConfigEdit(QWidget* parent, QString path, bool read_only);

protected:
  void keyPressEvent(QKeyEvent* e) override;
  void focusInEvent(QFocusEvent* e) override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadFile();
  void SaveFile();

  void OnSelectionChanged();
  void OnAutoComplete(const QString& completion);
  void OpenExternalEditor();

  QString GetTextUnderCursor();

  void AddDescription(const QString& keyword, const QString& description);

  QCompleter* m_completer;
  QStringList m_completions;
  QPushButton* m_refresh_button;
  QPushButton* m_external_editor_button;
  QTextEdit* m_edit;

  const QString m_path;

  bool m_read_only;

  QMap<QString, QString> m_keyword_map;
};

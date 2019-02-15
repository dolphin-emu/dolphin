// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QCompleter;
class QMenu;
class QTextEdit;

class GameConfigEdit : public QWidget
{
public:
  explicit GameConfigEdit(QWidget* parent, const QString& path, bool read_only);

protected:
  void keyPressEvent(QKeyEvent* e) override;
  void focusInEvent(QFocusEvent* e) override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void AddMenubarOptions();

  void LoadFile();
  void SaveFile();

  void OnSelectionChanged();
  void OnAutoComplete(const QString& completion);
  void OpenExternalEditor();

  void SetReadOnly(bool read_only);

  QString GetTextUnderCursor();

  void AddBoolOption(QMenu* menu, const QString& name, const QString& section, const QString& key);

  void SetOption(const QString& section, const QString& key, const QString& value);

  void AddDescription(const QString& keyword, const QString& description);

  QCompleter* m_completer;
  QStringList m_completions;
  QMenu* m_menu;
  QTextEdit* m_edit;

  const QString m_path;

  bool m_read_only;

  QMap<QString, QString> m_keyword_map;
};

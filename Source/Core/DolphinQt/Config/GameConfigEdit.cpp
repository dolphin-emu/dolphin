// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GameConfigEdit.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDesktopServices>
#include <QFile>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWhatsThis>

#include "DolphinQt/Config/GameConfigHighlighter.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

GameConfigEdit::GameConfigEdit(QWidget* parent, QString path, bool read_only)
    : QWidget{parent}, m_path(std::move(path)), m_read_only(read_only)
{
  CreateWidgets();

  LoadFile();

  new GameConfigHighlighter(m_edit->document());

  AddDescription(QStringLiteral("Core"),
                 tr("Section that contains most CPU and Hardware related settings."));

  AddDescription(QStringLiteral("CPUThread"), tr("Controls whether or not Dual Core should be "
                                                 "enabled. Can improve performance but can also "
                                                 "cause issues. Defaults to <b>True</b>"));

  AddDescription(QStringLiteral("FastDiscSpeed"),
                 tr("Emulate the disc speed of real hardware. Disabling can cause instability. "
                    "Defaults to <b>True</b>"));

  AddDescription(QStringLiteral("MMU"), tr("Controls whether or not the Memory Management Unit "
                                           "should be emulated fully. Few games require it."));

  AddDescription(
      QStringLiteral("DSPHLE"),
      tr("Controls whether to use high or low-level DSP emulation. Defaults to <b>True</b>"));

  AddDescription(
      QStringLiteral("JITFollowBranch"),
      tr("Tries to translate branches ahead of time, improving performance in most cases. Defaults "
         "to <b>True</b>"));

  AddDescription(QStringLiteral("Gecko"), tr("Section that contains all Gecko cheat codes."));

  AddDescription(QStringLiteral("ActionReplay"),
                 tr("Section that contains all Action Replay cheat codes."));

  AddDescription(QStringLiteral("Video_Settings"),
                 tr("Section that contains all graphics related settings."));

  m_completer = new QCompleter(m_edit);

  auto* completion_model = new QStringListModel(m_completer);
  completion_model->setStringList(m_completions);

  m_completer->setModel(completion_model);
  m_completer->setModelSorting(QCompleter::UnsortedModel);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  m_completer->setWidget(m_edit);

  ConnectWidgets();
}

void GameConfigEdit::CreateWidgets()
{
  m_edit = new QTextEdit;
  m_edit->setReadOnly(m_read_only);
  m_edit->setAcceptRichText(false);

  m_refresh_button = new QPushButton(tr("Refresh"));
  m_external_editor_button = new QPushButton(tr("Open in External Editor"));

  if (m_read_only)
  {
    m_refresh_button->hide();
    m_external_editor_button->hide();
  }

  auto* button_layout = new QHBoxLayout;
  button_layout->addWidget(m_refresh_button);
  button_layout->addWidget(m_external_editor_button);

  auto* layout = new QVBoxLayout;
  layout->addLayout(button_layout);
  layout->addWidget(m_edit);

  setLayout(layout);
}

void GameConfigEdit::AddDescription(const QString& keyword, const QString& description)
{
  m_keyword_map[keyword] = description;
  m_completions << keyword;
}

void GameConfigEdit::LoadFile()
{
  QFile file(m_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  m_edit->setPlainText(QString::fromStdString(file.readAll().toStdString()));
}

void GameConfigEdit::SaveFile()
{
  if (!isVisible() || m_read_only)
    return;

  QFile file(m_path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    ModalMessageBox::warning(this, tr("Warning"), tr("Failed to open config file!"));
    return;
  }

  const QByteArray contents = m_edit->toPlainText().toUtf8();

  if (file.write(contents) == -1)
    ModalMessageBox::warning(this, tr("Warning"), tr("Failed to write config file!"));
}

void GameConfigEdit::ConnectWidgets()
{
  connect(m_edit, &QTextEdit::textChanged, this, &GameConfigEdit::SaveFile);
  connect(m_edit, &QTextEdit::selectionChanged, this, &GameConfigEdit::OnSelectionChanged);
  connect(m_completer, qOverload<const QString&>(&QCompleter::activated), this,
          &GameConfigEdit::OnAutoComplete);

  connect(m_refresh_button, &QPushButton::clicked, this, &GameConfigEdit::LoadFile);
  connect(m_external_editor_button, &QPushButton::clicked, this,
          &GameConfigEdit::OpenExternalEditor);
}

void GameConfigEdit::OnSelectionChanged()
{
  const QString& keyword = m_edit->textCursor().selectedText();

  if (m_keyword_map.contains(keyword))
    QWhatsThis::showText(QCursor::pos(), m_keyword_map[keyword], this);
}

QString GameConfigEdit::GetTextUnderCursor()
{
  QTextCursor tc = m_edit->textCursor();
  tc.select(QTextCursor::WordUnderCursor);
  return tc.selectedText();
}

void GameConfigEdit::OnAutoComplete(const QString& completion)
{
  QTextCursor cursor = m_edit->textCursor();
  int extra = completion.length() - m_completer->completionPrefix().length();
  cursor.movePosition(QTextCursor::Left);
  cursor.movePosition(QTextCursor::EndOfWord);
  cursor.insertText(completion.right(extra));
  m_edit->setTextCursor(cursor);
}

void GameConfigEdit::OpenExternalEditor()
{
  QFile file(m_path);

  if (!file.exists())
  {
    if (m_read_only)
      return;

    if (!file.open(QIODevice::WriteOnly))
    {
      ModalMessageBox::warning(this, tr("Error"),
                               tr("Failed to create the configuration file:\n%1").arg(m_path));
      return;
    }

    file.close();
  }

  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_path)))
  {
    ModalMessageBox::warning(this, tr("Error"),
                             tr("Failed to open file in external editor.\nMake sure there's an "
                                "application assigned to open INI files."));
  }
}

void GameConfigEdit::keyPressEvent(QKeyEvent* e)
{
  if (m_completer->popup()->isVisible())
  {
    // The following keys are forwarded by the completer to the widget
    switch (e->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Escape:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
      e->ignore();
      return;  // let the completer do default behavior
    default:
      break;
    }
  }

  QWidget::keyPressEvent(e);

  const static QString end_of_word = QStringLiteral("~!@#$%^&*()_+{}|:\"<>?,./;'\\-=");

  QString completion_prefix = GetTextUnderCursor();

  if (e->text().isEmpty() || completion_prefix.length() < 2 ||
      end_of_word.contains(e->text().right(1)))
  {
    m_completer->popup()->hide();
    return;
  }

  if (completion_prefix != m_completer->completionPrefix())
  {
    m_completer->setCompletionPrefix(completion_prefix);
    m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
  }
  QRect cr = m_edit->cursorRect();
  cr.setWidth(m_completer->popup()->sizeHintForColumn(0) +
              m_completer->popup()->verticalScrollBar()->sizeHint().width());
  m_completer->complete(cr);  // popup it up!
}

void GameConfigEdit::focusInEvent(QFocusEvent* e)
{
  m_completer->setWidget(m_edit);
  QWidget::focusInEvent(e);
}

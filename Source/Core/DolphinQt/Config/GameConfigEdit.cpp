// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GameConfigEdit.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDesktopServices>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
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

  AddMenubarOptions();
  ConnectWidgets();
}

void GameConfigEdit::CreateWidgets()
{
  m_edit = new QTextEdit;
  m_edit->setReadOnly(m_read_only);
  m_edit->setAcceptRichText(false);

  auto* layout = new QVBoxLayout;

  auto* menu_button = new QPushButton;

  menu_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  menu_button->setText(tr("Presets"));

  m_menu = new QMenu(menu_button);
  menu_button->setMenu(m_menu);

  layout->addWidget(menu_button);
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
}

void GameConfigEdit::OnSelectionChanged()
{
  const QString& keyword = m_edit->textCursor().selectedText();

  if (m_keyword_map.count(keyword))
    QWhatsThis::showText(QCursor::pos(), m_keyword_map[keyword], this);
}

void GameConfigEdit::AddBoolOption(QMenu* menu, const QString& name, const QString& section,
                                   const QString& key)
{
  auto* option = menu->addMenu(name);

  option->addAction(tr("On"), this,
                    [this, section, key] { SetOption(section, key, QStringLiteral("True")); });
  option->addAction(tr("Off"), this,
                    [this, section, key] { SetOption(section, key, QStringLiteral("False")); });
}

void GameConfigEdit::SetOption(const QString& section, const QString& key, const QString& value)
{
  auto section_cursor =
      m_edit->document()->find(QRegularExpression(QStringLiteral("^\\[%1\\]").arg(section)), 0);

  // Check if the section this belongs in can be found
  if (section_cursor.isNull())
  {
    m_edit->append(QStringLiteral("[%1]\n\n%2 = %3\n").arg(section).arg(key).arg(value));
  }
  else
  {
    auto value_cursor = m_edit->document()->find(
        QRegularExpression(QStringLiteral("^%1 = .*").arg(key)), section_cursor);

    const QString new_line = QStringLiteral("%1 = %2").arg(key).arg(value);

    // Check if the value that has to be set already exists
    if (value_cursor.isNull())
    {
      section_cursor.clearSelection();
      section_cursor.insertText(QLatin1Char{'\n'} + new_line);
    }
    else
    {
      value_cursor.insertText(new_line);
    }
  }
}

QString GameConfigEdit::GetTextUnderCursor()
{
  QTextCursor tc = m_edit->textCursor();
  tc.select(QTextCursor::WordUnderCursor);
  return tc.selectedText();
}

void GameConfigEdit::AddMenubarOptions()
{
  auto* editor = m_menu->addMenu(tr("Editor"));

  editor->addAction(tr("Refresh"), this, &GameConfigEdit::LoadFile);
  editor->addAction(tr("Open in External Editor"), this, &GameConfigEdit::OpenExternalEditor);

  if (!m_read_only)
  {
    m_menu->addSeparator();
    auto* core_menubar = m_menu->addMenu(tr("Core"));

    AddBoolOption(core_menubar, tr("Dual Core"), QStringLiteral("Core"),
                  QStringLiteral("CPUThread"));
    AddBoolOption(core_menubar, tr("MMU"), QStringLiteral("Core"), QStringLiteral("MMU"));

    auto* video_menubar = m_menu->addMenu(tr("Video"));

    AddBoolOption(video_menubar, tr("Store EFB Copies to Texture Only"),
                  QStringLiteral("Video_Hacks"), QStringLiteral("EFBToTextureEnable"));

    AddBoolOption(video_menubar, tr("Store XFB Copies to Texture Only"),
                  QStringLiteral("Video_Hacks"), QStringLiteral("XFBToTextureEnable"));

    {
      auto* texture_cache = video_menubar->addMenu(tr("Texture Cache"));
      texture_cache->addAction(tr("Safe"), this, [this] {
        SetOption(QStringLiteral("Video_Settings"), QStringLiteral("SafeTextureCacheColorSamples"),
                  QStringLiteral("0"));
      });
      texture_cache->addAction(tr("Medium"), this, [this] {
        SetOption(QStringLiteral("Video_Settings"), QStringLiteral("SafeTextureCacheColorSamples"),
                  QStringLiteral("512"));
      });
      texture_cache->addAction(tr("Fast"), this, [this] {
        SetOption(QStringLiteral("Video_Settings"), QStringLiteral("SafeTextureCacheColorSamples"),
                  QStringLiteral("128"));
      });
    }
  }
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

    file.open(QIODevice::WriteOnly);
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

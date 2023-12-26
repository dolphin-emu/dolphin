// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Debugger/AssemblyEditor.h"

#include <QFile>
#include <QPainter>
#include <QTextBlock>
#include <QToolTip>

#include <filesystem>

#include "Common/Assembler/GekkoParser.h"
#include "Common/StringUtil.h"
#include "DolphinQt/Debugger/GekkoSyntaxHighlight.h"

QSize AsmEditor::LineNumberArea::sizeHint() const
{
  return QSize(asm_editor->LineNumberAreaWidth(), 0);
}

void AsmEditor::LineNumberArea::paintEvent(QPaintEvent* event)
{
  asm_editor->LineNumberAreaPaintEvent(event);
}

AsmEditor::AsmEditor(const QString& path, int editor_num, bool dark_scheme, QWidget* parent)
    : QPlainTextEdit(parent), m_path(path), m_base_address(QStringLiteral("0")),
      m_editor_num(editor_num), m_dirty(false), m_dark_scheme(dark_scheme)
{
  if (!m_path.isEmpty())
  {
    m_filename =
        QString::fromStdString(std::filesystem::path(m_path.toStdString()).filename().string());
  }

  m_line_number_area = new LineNumberArea(this);
  m_highlighter = new GekkoSyntaxHighlight(document(), currentCharFormat(), dark_scheme);
  m_last_block = textCursor().block();

  QFont mono_font(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
  mono_font.setPointSize(12);
  setFont(mono_font);
  m_line_number_area->setFont(mono_font);

  UpdateLineNumberAreaWidth(0);
  HighlightCurrentLine();
  setMouseTracking(true);

  connect(this, &AsmEditor::blockCountChanged, this, &AsmEditor::UpdateLineNumberAreaWidth);
  connect(this, &AsmEditor::updateRequest, this, &AsmEditor::UpdateLineNumberArea);
  connect(this, &AsmEditor::cursorPositionChanged, this, &AsmEditor::HighlightCurrentLine);
  connect(this, &AsmEditor::textChanged, this, [this] {
    m_dirty = true;
    emit DirtyChanged();
  });
}

int AsmEditor::LineNumberAreaWidth()
{
  int num_digits = 1;
  for (int max = qMax(1, blockCount()); max >= 10; max /= 10, ++num_digits)
  {
  }

  return 3 + CharWidth() * qMax(2, num_digits);
}

void AsmEditor::SetBaseAddress(const QString& ba)
{
  if (ba != m_base_address)
  {
    m_base_address = ba;
    m_dirty = true;
    emit DirtyChanged();
  }
}

bool AsmEditor::LoadFromPath()
{
  QFile file(m_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    return false;
  }

  const std::string base_addr_line = file.readLine().toStdString();
  std::string base_address = "";
  for (size_t i = 0; i < base_addr_line.length(); i++)
  {
    if (std::isspace(base_addr_line[i]))
    {
      continue;
    }
    else if (base_addr_line[i] == '#')
    {
      base_address = base_addr_line.substr(i + 1);
      break;
    }
    else
    {
      break;
    }
  }

  if (base_address.empty())
  {
    file.seek(0);
  }
  else
  {
    StringPopBackIf(&base_address, '\n');
    if (base_address.empty())
    {
      base_address = "0";
    }
    m_base_address = QString::fromStdString(base_address);
  }

  const bool old_block = blockSignals(true);
  setPlainText(QString::fromStdString(file.readAll().toStdString()));
  blockSignals(old_block);
  return true;
}

bool AsmEditor::PathsMatch(const QString& path) const
{
  if (m_path.isEmpty() || path.isEmpty())
  {
    return false;
  }
  return std::filesystem::path(m_path.toStdString()) == std::filesystem::path(path.toStdString());
}

void AsmEditor::Zoom(int amount)
{
  if (amount > 0)
  {
    zoomIn(amount);
  }
  else
  {
    zoomOut(-amount);
  }
  m_line_number_area->setFont(font());
}

bool AsmEditor::SaveFile(const QString& save_path)
{
  QFile file(save_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
  {
    return false;
  }

  if (m_path != save_path)
  {
    m_path = save_path;
    m_filename =
        QString::fromStdString(std::filesystem::path(m_path.toStdString()).filename().string());
    emit PathChanged();
  }

  if (file.write(QStringLiteral("#%1\n").arg(m_base_address).toUtf8()) == -1)
  {
    return false;
  }

  if (file.write(toPlainText().toUtf8()) == -1)
  {
    return false;
  }

  m_dirty = false;
  emit DirtyChanged();
  return true;
}

void AsmEditor::UpdateLineNumberAreaWidth(int)
{
  setViewportMargins(LineNumberAreaWidth(), 0, 0, 0);
}

void AsmEditor::UpdateLineNumberArea(const QRect& rect, int dy)
{
  if (dy != 0)
  {
    m_line_number_area->scroll(0, dy);
  }
  else
  {
    m_line_number_area->update(0, rect.y(), m_line_number_area->width(), rect.height());
  }

  if (rect.contains(viewport()->rect()))
  {
    UpdateLineNumberAreaWidth(0);
  }
}

int AsmEditor::CharWidth() const
{
  return fontMetrics().horizontalAdvance(QLatin1Char(' '));
}

void AsmEditor::resizeEvent(QResizeEvent* e)
{
  QPlainTextEdit::resizeEvent(e);

  const QRect cr = contentsRect();
  m_line_number_area->setGeometry(QRect(cr.left(), cr.top(), LineNumberAreaWidth(), cr.height()));
}

void AsmEditor::paintEvent(QPaintEvent* event)
{
  QPlainTextEdit::paintEvent(event);

  QPainter painter(viewport());
  QTextCursor tc(document());

  QPen p = QPen(Qt::red);
  p.setStyle(Qt::PenStyle::SolidLine);
  p.setWidth(1);
  painter.setPen(p);
  const int width = CharWidth();

  for (QTextBlock blk = firstVisibleBlock(); blk.isVisible() && blk.isValid(); blk = blk.next())
  {
    if (blk.userData() == nullptr)
    {
      continue;
    }

    BlockInfo* info = static_cast<BlockInfo*>(blk.userData());
    if (info->error_at_eol)
    {
      tc.setPosition(blk.position() + blk.length() - 1);
      tc.clearSelection();
      const QRect qr = cursorRect(tc);
      painter.drawLine(qr.x(), qr.y() + qr.height(), qr.x() + width, qr.y() + qr.height());
    }
  }
}

bool AsmEditor::event(QEvent* e)
{
  if (e->type() == QEvent::ToolTip)
  {
    QHelpEvent* he = static_cast<QHelpEvent*>(e);
    QTextCursor hover_cursor = cursorForPosition(he->pos());
    QTextBlock hover_block = hover_cursor.block();

    BlockInfo* info = static_cast<BlockInfo*>(hover_block.userData());
    if (info == nullptr || !info->error)
    {
      QToolTip::hideText();
      return true;
    }

    QRect check_rect;
    if (info->error_at_eol)
    {
      hover_cursor.setPosition(hover_block.position() +
                               static_cast<int>(info->error->col + info->error->len));
      const QRect cursor_left = cursorRect(hover_cursor);
      const int area_width = CharWidth();
      check_rect = QRect(cursor_left.x() + LineNumberAreaWidth(), cursor_left.y(),
                         cursor_left.x() + area_width, cursor_left.height());
    }
    else
    {
      hover_cursor.setPosition(hover_block.position() + static_cast<int>(info->error->col));
      const QRect cursor_left = cursorRect(hover_cursor);
      hover_cursor.setPosition(hover_block.position() +
                               static_cast<int>(info->error->col + info->error->len));
      const QRect cursor_right = cursorRect(hover_cursor);
      check_rect = QRect(cursor_left.x() + LineNumberAreaWidth(), cursor_left.y(),
                         cursor_right.x() - cursor_left.x(), cursor_left.height());
    }
    if (check_rect.contains(he->pos()))
    {
      QToolTip::showText(he->globalPos(), QString::fromStdString(info->error->message));
    }
    else
    {
      QToolTip::hideText();
    }
    return true;
  }
  return QPlainTextEdit::event(e);
}

void AsmEditor::keyPressEvent(QKeyEvent* event)
{
  // HACK: Change shift+enter to enter to keep lines as blocks
  if (event->modifiers() & Qt::ShiftModifier &&
      (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return))
  {
    event->setModifiers(event->modifiers() & ~Qt::ShiftModifier);
  }
  QPlainTextEdit::keyPressEvent(event);
}

void AsmEditor::wheelEvent(QWheelEvent* event)
{
  QPlainTextEdit::wheelEvent(event);

  if (event->modifiers() & Qt::ControlModifier)
  {
    auto delta = static_cast<int>(std::round((event->angleDelta().y() / 120.0)));
    if (delta != 0)
    {
      emit ZoomRequested(delta);
    }
  }
}

void AsmEditor::HighlightCurrentLine()
{
  const bool old_state = blockSignals(true);

  if (m_last_block.blockNumber() != textCursor().blockNumber())
  {
    m_highlighter->SetMode(2);
    m_highlighter->rehighlightBlock(m_last_block);

    m_last_block = textCursor().block();
  }

  m_highlighter->SetCursorLoc(textCursor().positionInBlock());
  m_highlighter->SetMode(1);
  m_highlighter->rehighlightBlock(textCursor().block());
  m_highlighter->SetMode(0);

  blockSignals(old_state);
}

void AsmEditor::LineNumberAreaPaintEvent(QPaintEvent* event)
{
  QPainter painter(m_line_number_area);
  if (m_dark_scheme)
  {
    painter.fillRect(event->rect(), QColor::fromRgb(76, 76, 76));
  }
  else
  {
    painter.fillRect(event->rect(), QColor::fromRgb(180, 180, 180));
  }

  QTextBlock block = firstVisibleBlock();
  int block_num = block.blockNumber();
  int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  while (block.isValid() && top <= event->rect().bottom())
  {
    if (block.isVisible() && bottom >= event->rect().top())
    {
      const QString num = QString::number(block_num + 1);
      painter.drawText(0, top, m_line_number_area->width(), fontMetrics().height(), Qt::AlignRight,
                       num);
    }

    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++block_num;
  }
}

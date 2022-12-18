#pragma once

#include <QPlainTextEdit>
#include <QTextBlock>

class QWidget;
class QPaintEvent;
class QResizeEvent;
class QRect;
class GekkoSyntaxHighlight;

class AsmEditor : public QPlainTextEdit
{
  Q_OBJECT;

public:
  AsmEditor(const QString& file_path, int editor_num, QWidget* parent = nullptr);
  void LineNumberAreaPaintEvent(QPaintEvent* event);
  int LineNumberAreaWidth();
  const QString& Path() const { return m_path; }
  const QString& FileName() const { return m_filename; }
  const QString& BaseAddress() const { return m_base_address; }
  void SetBaseAddress(const QString& ba);
  int EditorNum() const { return m_editor_num; }
  bool LoadFromPath();
  bool IsDirty() const { return m_dirty; }
  bool PathsMatch(const QString& path) const;

public slots:
  bool SaveFile(const QString& save_path);

signals:
  void PathChanged();
  void DirtyChanged();

protected:
  void resizeEvent(QResizeEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  bool event(QEvent* e) override;
  void keyPressEvent(QKeyEvent* event) override;

private:
  void UpdateLineNumberAreaWidth(int new_block_count);
  void HighlightCurrentLine();
  void UpdateLineNumberArea(const QRect& rect, int dy);
  int CharWidth() const;

private:
  class LineNumberArea : public QWidget
  {
  public:
    LineNumberArea(AsmEditor* editor) : QWidget(editor), asm_editor(editor) {}
    QSize sizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    AsmEditor* asm_editor;
  };

  QWidget* m_line_number_area;
  GekkoSyntaxHighlight* m_highlighter;
  QString m_path;
  QString m_filename;
  QString m_base_address;
  const int m_editor_num;
  bool m_dirty;
  QTextBlock m_last_block;
};

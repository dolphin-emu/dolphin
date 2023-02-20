#pragma once

#include <QAbstractScrollArea>
#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QRect>
#include <QShortcut>
#include <QTimer>
#include <QWidget>

#include "MemoryEngine/Common/CommonTypes.h"
#include "MemoryEngine/Common/MemoryCommon.h"
#include "MemoryEngine/MemoryWatch/MemWatchEntry.h"

class MemViewer : public QAbstractScrollArea
{
  Q_OBJECT

public:
  MemViewer(QWidget* parent);
  ~MemViewer();
  QSize sizeHint() const override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void scrollContentsBy(int dx, int dy) override;
  u32 getCurrentFirstAddress() const;
  void jumpToAddress(const u32 address);
  void updateViewer();
  void memoryValidityChanged(const bool valid);

signals:
  void memErrorOccured();
  void addWatch(MemWatchEntry* entry);

private:
  enum class SelectionType
  {
    upward,
    downward,
    single
  };

  enum class MemoryRegion
  {
    MEM1,
    MEM2,
    ARAM
  };

  struct bytePosFromMouse
  {
    int x = 0;
    int y = 0;
    bool isInViewer = false;
  };

  void initialise();

  void updateFontSize(int newSize);
  bytePosFromMouse mousePosToBytePos(QPoint pos);
  void scrollToSelection();
  void copySelection(Common::MemType type);
  void editSelection();
  void addSelectionAsArrayOfBytes();
  void addByteIndexAsWatch(int index);
  bool handleNaviguationKey(const int key, bool shiftIsHeld);
  void writeCharacterToSelectedMemory(char byteToWrite);
  void updateMemoryData();
  void changeMemoryRegion(const MemoryRegion region);
  void renderColumnsHeaderText(QPainter& painter);
  void renderRowHeaderText(QPainter& painter, const int rowIndex);
  void renderSeparatorLines(QPainter& painter);
  void renderMemory(QPainter& painter, const int rowIndex, const int columnIndex);
  void renderHexByte(QPainter& painter, const int rowIndex, const int columnIndex, QColor& bgColor,
                     QColor& fgColor, bool drawCarret);
  void renderASCIIText(QPainter& painter, const int rowIndex, const int columnIndex,
                       QColor& bgColor, QColor& fgColor);
  void renderCarret(QPainter& painter, const int rowIndex, const int columnIndex);
  void determineMemoryTextRenderProperties(const int rowIndex, const int columnIndex,
                                           bool& drawCarret, QColor& bgColor, QColor& fgColor);

  const int m_numRows = 16;
  const int m_numColumns = 16;  // Should be a multiple of 16, or the header doesn't make much sense
  const int m_numCells = m_numRows * m_numColumns;
  int m_memoryFontSize = 15;
  int m_StartBytesSelectionPosX = 0;
  int m_StartBytesSelectionPosY = 0;
  int m_EndBytesSelectionPosX = 0;
  int m_EndBytesSelectionPosY = 0;
  SelectionType m_selectionType = SelectionType::single;
  int m_charWidthEm = 0;
  int m_charHeight = 0;
  int m_hexAreaWidth = 0;
  int m_hexAreaHeight = 0;
  int m_rowHeaderWidth = 0;
  int m_columnHeaderHeight = 0;
  int m_hexAsciiSeparatorPosX = 0;
  char* m_updatedRawMemoryData = nullptr;
  char* m_lastRawMemoryData = nullptr;
  int* m_memoryMsElapsedLastChange = nullptr;
  bool m_editingHex = false;
  bool m_carretBetweenHex = false;
  bool m_disableScrollContentEvent = false;
  bool m_validMemory = false;
  u32 m_currentFirstAddress = 0;
  u32 m_memViewStart = 0;
  u32 m_memViewEnd = 0;
  QRect* m_curosrRect;
  QShortcut* m_copyShortcut;
  QElapsedTimer m_elapsedTimer;
};

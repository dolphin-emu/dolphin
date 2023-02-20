#include "MemViewer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

#include <QApplication>
#include <QClipboard>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>

#include "../../../MemoryEngine/Common/MemoryCommon.h"
#include "../MemWatcher/Dialogs/DlgAddWatchEntry.h"
#include "../Settings/DMEConfig.h"

MemViewer::MemViewer(QWidget* parent) : QAbstractScrollArea(parent)
{
  initialise();

  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  changeMemoryRegion(MemoryRegion::MEM1);
  verticalScrollBar()->setPageStep(m_numRows);

  m_elapsedTimer.start();

  m_copyShortcut = new QShortcut(QKeySequence(Qt::Modifier::CTRL + Qt::Key::Key_C), parent);
  connect(m_copyShortcut, &QShortcut::activated, this,
          [=]() { copySelection(Common::MemType::type_byteArray); });

  // The viewport is implicitly updated at the constructor's end
}

MemViewer::~MemViewer()
{
  delete[] m_updatedRawMemoryData;
  delete[] m_lastRawMemoryData;
  delete[] m_memoryMsElapsedLastChange;
  delete m_curosrRect;
}

void MemViewer::initialise()
{
  updateFontSize(m_memoryFontSize);
  m_curosrRect = new QRect();
  m_updatedRawMemoryData = new char[m_numCells];
  m_lastRawMemoryData = new char[m_numCells];
  m_memoryMsElapsedLastChange = new int[m_numCells];
  m_memViewStart = (size_t)Common::getMEM1();
  m_memViewEnd = (size_t)Common::getMEM1End();
  m_currentFirstAddress = m_memViewStart;

  std::fill(m_memoryMsElapsedLastChange, m_memoryMsElapsedLastChange + m_numCells, 0);
  updateMemoryData();
  std::memcpy(m_lastRawMemoryData, m_updatedRawMemoryData, m_numCells);
}

QSize MemViewer::sizeHint() const
{
  return QSize(m_rowHeaderWidth + m_hexAreaWidth + 17 * m_charWidthEm +
                   verticalScrollBar()->width(),
               m_columnHeaderHeight + m_hexAreaHeight + m_charHeight / 2);
}

void MemViewer::memoryValidityChanged(const bool valid)
{
  m_validMemory = valid;
  viewport()->update();
}

void MemViewer::updateMemoryData()
{
  std::swap(m_updatedRawMemoryData, m_lastRawMemoryData);
  m_validMemory = Common::isValidAddress(m_currentFirstAddress);
  if (m_validMemory)
    Common::readFromRAM(m_updatedRawMemoryData, m_currentFirstAddress, m_numCells);
  if (!m_validMemory)
    emit memErrorOccured();
}

void MemViewer::updateViewer()
{
  updateMemoryData();
  viewport()->update();
  if (!m_validMemory)
    emit memErrorOccured();
}

u32 MemViewer::getCurrentFirstAddress() const
{
  return m_currentFirstAddress;
}

void MemViewer::jumpToAddress(const u32 address)
{
  if (Common::isValidAddress(address))
  {
    m_currentFirstAddress = address;
    if (address >= (size_t)Common::getARAM() && address < (size_t)Common::getARAMEnd() &&
        m_memViewStart != (size_t)Common::getARAM())
      changeMemoryRegion(MemoryRegion::ARAM);
    else if (address >= (size_t)Common::getMEM1() && address < (size_t)Common::getMEM1End() &&
             m_memViewStart != (size_t)Common::getMEM1())
      changeMemoryRegion(MemoryRegion::MEM1);
    else if (address >= (size_t)Common::getMEM2() && address < (size_t)Common::getMEM2End() &&
             m_memViewStart != (size_t)Common::getMEM2())
      changeMemoryRegion(MemoryRegion::MEM2);

    if (m_currentFirstAddress > m_memViewEnd - m_numCells)
      m_currentFirstAddress = m_memViewEnd - m_numCells;
    std::fill(m_memoryMsElapsedLastChange, m_memoryMsElapsedLastChange + m_numCells, 0);
    updateMemoryData();
    std::memcpy(m_lastRawMemoryData, m_updatedRawMemoryData, m_numCells);
    m_carretBetweenHex = false;

    m_disableScrollContentEvent = true;
    verticalScrollBar()->setValue(((address & 0xFFFFFFF0) - m_memViewStart) / m_numColumns);
    m_disableScrollContentEvent = false;

    viewport()->update();
  }
}

void MemViewer::changeMemoryRegion(const MemoryRegion region)
{
  switch (region)
  {
  case MemoryRegion::ARAM:
    m_memViewStart = (size_t)Common::getARAM();
    m_memViewEnd = (size_t)Common::getARAMEnd();
    break;
  case MemoryRegion::MEM1:
    m_memViewStart = (size_t)Common::getMEM1();
    m_memViewEnd = (size_t)Common::getMEM1End();
    break;
  case MemoryRegion::MEM2:
    m_memViewStart = (size_t)Common::getMEM2();
    m_memViewEnd = (size_t)Common::getMEM2End();
    break;
  }
  verticalScrollBar()->setRange(0, ((m_memViewEnd - m_memViewStart) / m_numColumns) - m_numRows);
}

MemViewer::bytePosFromMouse MemViewer::mousePosToBytePos(QPoint pos)
{
  int x = pos.x();
  int y = pos.y();

  const int spacing = m_charWidthEm / 2;
  const int hexCellWidth = m_charWidthEm * 2 + spacing;
  const int hexAreaLeft = m_rowHeaderWidth - spacing / 2;
  const int asciiAreaLeft = m_hexAsciiSeparatorPosX + spacing;
  const int areaTop = m_columnHeaderHeight + m_charHeight - fontMetrics().overlinePos();
  QRect hexArea(hexAreaLeft, areaTop, m_hexAreaWidth, m_charHeight * m_numRows);
  QRect asciiArea(asciiAreaLeft, areaTop, m_charWidthEm * m_numColumns, m_charHeight * m_numRows);

  bytePosFromMouse bytePos;

  // Transform x and y to indices for column and row
  if (hexArea.contains(x, y, false))
  {
    bytePos.x = (x - hexAreaLeft) / hexCellWidth;
    m_editingHex = true;
  }
  else if (asciiArea.contains(x, y, false))
  {
    bytePos.x = (x - asciiAreaLeft) / m_charWidthEm;
    m_editingHex = false;
  }
  else
  {
    bytePos.isInViewer = false;
    return bytePos;
  }
  bytePos.y = (y - areaTop) / m_charHeight;
  bytePos.isInViewer = true;
  return bytePos;
}

void MemViewer::mousePressEvent(QMouseEvent* event)
{
  // Only handle left-click events
  if (event->button() != Qt::MouseButton::LeftButton)
    return;

  bytePosFromMouse bytePos = mousePosToBytePos(event->pos());

  if (!bytePos.isInViewer)
    return;

  const bool wasEditingHex = m_editingHex;

  // Toggle carrot-between-hex when the same byte is clicked twice from the hex table
  m_carretBetweenHex =
      (m_editingHex && wasEditingHex && !m_carretBetweenHex &&
       m_StartBytesSelectionPosX == bytePos.x && m_StartBytesSelectionPosY == bytePos.y &&
       m_EndBytesSelectionPosX == bytePos.x && m_EndBytesSelectionPosY == bytePos.y);

  m_StartBytesSelectionPosX = bytePos.x;
  m_StartBytesSelectionPosY = bytePos.y;
  m_EndBytesSelectionPosX = bytePos.x;
  m_EndBytesSelectionPosY = bytePos.y;
  m_selectionType = SelectionType::single;

  viewport()->update();
}

void MemViewer::mouseMoveEvent(QMouseEvent* event)
{
  if (!(event->buttons() & Qt::LeftButton))
    return;

  bytePosFromMouse bytePos = mousePosToBytePos(event->pos());

  if (!bytePos.isInViewer)
    return;

  int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
  int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
  int indexDrag = bytePos.y * m_numColumns + bytePos.x;

  // The selection is getting retracted, but still goes the same direction as before
  if (indexDrag > indexStart && indexDrag < indexEnd)
  {
    if (m_selectionType == SelectionType::upward)
    {
      m_StartBytesSelectionPosX = bytePos.x;
      m_StartBytesSelectionPosY = bytePos.y;
    }
    else if (m_selectionType == SelectionType::downward)
    {
      m_EndBytesSelectionPosX = bytePos.x;
      m_EndBytesSelectionPosY = bytePos.y;
    }
  }
  // The selection either expands upwards OR it switches to upward
  else if (indexDrag < indexStart && indexDrag < indexEnd)
  {
    if (m_selectionType == SelectionType::downward)
    {
      m_EndBytesSelectionPosX = m_StartBytesSelectionPosX;
      m_EndBytesSelectionPosY = m_StartBytesSelectionPosY;
    }
    m_StartBytesSelectionPosX = bytePos.x;
    m_StartBytesSelectionPosY = bytePos.y;
    m_selectionType = SelectionType::upward;
  }
  // The selection either expands downward OR it switches to downward
  else if (indexDrag > indexStart && indexDrag > indexEnd)
  {
    if (m_selectionType == SelectionType::upward)
    {
      m_StartBytesSelectionPosX = m_EndBytesSelectionPosX;
      m_StartBytesSelectionPosY = m_EndBytesSelectionPosY;
    }
    m_EndBytesSelectionPosX = bytePos.x;
    m_EndBytesSelectionPosY = bytePos.y;
    m_selectionType = SelectionType::downward;
  }
  // The selection is just one byte
  else if ((indexDrag == indexStart && m_selectionType == SelectionType::downward) ||
           (indexDrag == indexEnd && m_selectionType == SelectionType::upward))
  {
    m_StartBytesSelectionPosX = bytePos.x;
    m_StartBytesSelectionPosY = bytePos.y;
    m_EndBytesSelectionPosX = bytePos.x;
    m_EndBytesSelectionPosY = bytePos.y;
    m_selectionType = SelectionType::single;
  }

  viewport()->update();
}

void MemViewer::contextMenuEvent(QContextMenuEvent* event)
{
  if (event->reason() == QContextMenuEvent::Reason::Mouse)
  {
    bytePosFromMouse bytePos = mousePosToBytePos(event->pos());
    if (!bytePos.isInViewer)
    {
      event->ignore();
      return;
    }

    int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
    int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
    int indexMouse = bytePos.y * m_numColumns + bytePos.x;

    QMenu* contextMenu = new QMenu(this);

    if (indexMouse >= indexStart && indexMouse <= indexEnd)
    {
      QAction* copyBytesAction = new QAction(tr("&Copy selection as bytes"));
      connect(copyBytesAction, &QAction::triggered, this,
              [=]() { copySelection(Common::MemType::type_byteArray); });
      contextMenu->addAction(copyBytesAction);

      QAction* copyStringAction = new QAction(tr("&Copy selection as ASCII string"));
      connect(copyStringAction, &QAction::triggered, this,
              [=]() { copySelection(Common::MemType::type_string); });
      contextMenu->addAction(copyStringAction);

      QAction* editAction = new QAction(tr("&Edit all selection..."));
      connect(editAction, &QAction::triggered, this, &MemViewer::editSelection);
      contextMenu->addAction(editAction);

      QAction* addBytesArrayAction = new QAction(tr("&Add selection as array of bytes..."));
      connect(addBytesArrayAction, &QAction::triggered, this,
              &MemViewer::addSelectionAsArrayOfBytes);
      contextMenu->addAction(addBytesArrayAction);
    }

    QAction* addSingleWatchAction = new QAction(tr("&Add a watch to this address..."));
    connect(addSingleWatchAction, &QAction::triggered, this,
            [=]() { addByteIndexAsWatch(indexMouse); });
    contextMenu->addAction(addSingleWatchAction);

    contextMenu->popup(viewport()->mapToGlobal(event->pos()));
  }
}

void MemViewer::wheelEvent(QWheelEvent* event)
{
  if (event->modifiers().testFlag(Qt::ControlModifier))
  {
    if (event->delta() < 0 && m_memoryFontSize > 5)
      updateFontSize(m_memoryFontSize - 1);
    else if (event->delta() > 0)
      updateFontSize(m_memoryFontSize + 1);

    viewport()->update();
  }
  else
  {
    QAbstractScrollArea::wheelEvent(event);
  }
}

void MemViewer::updateFontSize(int newSize)
{
  m_memoryFontSize = newSize;

#ifdef __linux__
  setFont(QFont(tr("Monospace"), m_memoryFontSize));
#elif _WIN32
  setFont(QFont(tr("Courier New"), m_memoryFontSize));
#endif

  m_charWidthEm = fontMetrics().width(QLatin1Char('M'));
  m_charHeight = fontMetrics().height();
  m_hexAreaWidth = m_numColumns * (m_charWidthEm * 2 + m_charWidthEm / 2);
  m_hexAreaHeight = m_numRows * m_charHeight;
  m_rowHeaderWidth = m_charWidthEm * (sizeof(u32) * 2 + 1) + m_charWidthEm / 2;
  m_hexAsciiSeparatorPosX = m_rowHeaderWidth + m_hexAreaWidth;
  m_columnHeaderHeight = m_charHeight + m_charWidthEm / 2;
}

void MemViewer::scrollToSelection()
{
  if (m_selectionType != SelectionType::downward)
  {
    if (m_StartBytesSelectionPosY < 0)
      scrollContentsBy(0, -m_StartBytesSelectionPosY);
    else if (m_StartBytesSelectionPosY >= m_numRows)
      scrollContentsBy(0, m_numRows - m_StartBytesSelectionPosY - 1);
  }
  else
  {
    if (m_EndBytesSelectionPosY < 0)
      scrollContentsBy(0, -m_EndBytesSelectionPosY);
    else if (m_EndBytesSelectionPosY >= m_numRows)
      scrollContentsBy(0, m_numRows - m_EndBytesSelectionPosY - 1);
  }

  viewport()->update();
}

void MemViewer::copySelection(Common::MemType type)
{
  int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
  int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
  size_t selectionLength = static_cast<size_t>(indexEnd - indexStart + 1);

  char* selectedMem = new char[selectionLength];
  if (Common::isValidAddress(m_currentFirstAddress))
  {
    Common::readFromRAM(selectedMem, m_currentFirstAddress + indexStart, selectionLength);
    std::string bytes = Common::formatMemoryToString(selectedMem, type, selectionLength,
                                                     Common::MemBase::base_none, true);
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(QString::fromStdString(bytes));
  }
  delete[] selectedMem;
}

void MemViewer::editSelection()
{
  QInputDialog* dlg = new QInputDialog(this);

  int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
  int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
  size_t selectionLength = static_cast<size_t>(indexEnd - indexStart + 1);

  QString strByte = dlg->getText(this, tr("Enter the new byte"), tr("Byte (in hex)"));
  if (!strByte.isEmpty())
  {
    QRegularExpression hexMatcher(tr("^[0-9A-F]{1,2}$"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = hexMatcher.match(strByte);
    if (!match.hasMatch())
    {
      QMessageBox* errorBox =
          new QMessageBox(QMessageBox::Critical, tr("Invalid byte"),
                          QString::fromStdString("The byte is an invalid hexadecimal number"),
                          QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }

    std::stringstream ss(strByte.toStdString());
    ss >> std::hex;
    int byte = 0;
    ss >> byte;

    char* newMem = new char[selectionLength];
    std::memset(newMem, byte, selectionLength);
    if (Common::isValidAddress(m_currentFirstAddress + indexStart))
      Common::writeToRAM(newMem, m_currentFirstAddress + indexStart, selectionLength, false);
    delete[] newMem;
  }
}

void MemViewer::addSelectionAsArrayOfBytes()
{
  QInputDialog* dlg = new QInputDialog(this);

  int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
  int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
  size_t selectionLength = static_cast<size_t>(indexEnd - indexStart + 1);

  QString strLabel = dlg->getText(this, tr("Enter the label of the new watch"), tr("label"));
  if (!strLabel.isEmpty())
  {
    MemWatchEntry* newEntry = new MemWatchEntry(strLabel, m_currentFirstAddress + indexStart,
                                                Common::MemType::type_byteArray,
                                                Common::MemBase::base_none, true, selectionLength);
    emit addWatch(newEntry);
  }
}

void MemViewer::addByteIndexAsWatch(int index)
{
  MemWatchEntry* entry = new MemWatchEntry();
  entry->setConsoleAddress(m_currentFirstAddress + index);
  DlgAddWatchEntry* dlg = new DlgAddWatchEntry(entry);
  if (dlg->exec() == QDialog::Accepted)
    emit addWatch(dlg->getEntry());
}

bool MemViewer::handleNaviguationKey(const int key, bool shiftIsHeld)
{
  switch (key)
  {
  case Qt::Key_Up:
    if (verticalScrollBar()->value() + m_StartBytesSelectionPosY <= verticalScrollBar()->minimum())
    {
      QApplication::beep();
    }
    else
    {
      if (m_selectionType == SelectionType::downward && shiftIsHeld)
      {
        m_EndBytesSelectionPosY--;
        int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
        int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
        if (indexEnd <= indexStart)
        {
          std::swap(m_StartBytesSelectionPosX, m_EndBytesSelectionPosX);
          std::swap(m_StartBytesSelectionPosY, m_EndBytesSelectionPosY);
          if (indexEnd == indexStart)
            m_selectionType = SelectionType::single;
          else
            m_selectionType = SelectionType::upward;
        }
      }
      else
      {
        m_StartBytesSelectionPosY--;
        if (!shiftIsHeld)
        {
          m_EndBytesSelectionPosX = m_StartBytesSelectionPosX;
          m_EndBytesSelectionPosY = m_StartBytesSelectionPosY;
          m_selectionType = SelectionType::single;
        }
        else
        {
          m_selectionType = SelectionType::upward;
        }
      }
    }

    scrollToSelection();
    break;
  case Qt::Key_Down:
    if (verticalScrollBar()->value() + m_StartBytesSelectionPosY >= verticalScrollBar()->maximum())
    {
      QApplication::beep();
    }
    else
    {
      if (m_selectionType == SelectionType::upward && shiftIsHeld)
      {
        m_StartBytesSelectionPosY++;
        int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
        int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
        if (indexEnd <= indexStart)
        {
          std::swap(m_StartBytesSelectionPosX, m_EndBytesSelectionPosX);
          std::swap(m_StartBytesSelectionPosY, m_EndBytesSelectionPosY);
          if (indexEnd == indexStart)
            m_selectionType = SelectionType::single;
          else
            m_selectionType = SelectionType::downward;
        }
      }
      else
      {
        m_EndBytesSelectionPosY++;
        if (!shiftIsHeld)
        {
          m_StartBytesSelectionPosX = m_EndBytesSelectionPosX;
          m_StartBytesSelectionPosY = m_EndBytesSelectionPosY;
          m_selectionType = SelectionType::single;
        }
        else
        {
          m_selectionType = SelectionType::downward;
        }
      }
    }

    scrollToSelection();
    break;
  case Qt::Key_Left:
    if (m_StartBytesSelectionPosX <= 0 && m_StartBytesSelectionPosY <= 0 &&
        verticalScrollBar()->value() == verticalScrollBar()->minimum())
    {
      QApplication::beep();
    }
    else
    {
      if (m_selectionType == SelectionType::downward && shiftIsHeld)
      {
        m_EndBytesSelectionPosX--;
        if (m_EndBytesSelectionPosX < 0)
        {
          m_EndBytesSelectionPosX += m_numColumns;
          m_EndBytesSelectionPosY--;
        }
        int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
        int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
        if (indexEnd == indexStart)
          m_selectionType = SelectionType::single;
      }
      else
      {
        m_StartBytesSelectionPosX--;
        if (m_StartBytesSelectionPosX < 0)
        {
          m_StartBytesSelectionPosX += m_numColumns;
          m_StartBytesSelectionPosY--;
        }
        if (!shiftIsHeld)
        {
          m_EndBytesSelectionPosX = m_StartBytesSelectionPosX;
          m_EndBytesSelectionPosY = m_StartBytesSelectionPosY;
          m_selectionType = SelectionType::single;
        }
        else
        {
          m_selectionType = SelectionType::upward;
        }
      }

      scrollToSelection();
    }
    break;
  case Qt::Key_Right:
    if (m_StartBytesSelectionPosX >= m_numColumns - 1 &&
        m_StartBytesSelectionPosY >= m_numRows - 1 &&
        verticalScrollBar()->value() == verticalScrollBar()->maximum())
    {
      QApplication::beep();
    }
    else
    {
      if (m_selectionType == SelectionType::upward && shiftIsHeld)
      {
        m_StartBytesSelectionPosX++;
        if (m_StartBytesSelectionPosX >= m_numColumns)
        {
          m_StartBytesSelectionPosX -= m_numColumns;
          m_StartBytesSelectionPosY++;
        }
        int indexStart = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
        int indexEnd = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;
        if (indexEnd == indexStart)
          m_selectionType = SelectionType::single;
      }
      else
      {
        m_EndBytesSelectionPosX++;
        if (m_EndBytesSelectionPosX >= m_numColumns)
        {
          m_EndBytesSelectionPosX -= m_numColumns;
          m_EndBytesSelectionPosY++;
        }
        if (!shiftIsHeld)
        {
          m_StartBytesSelectionPosX = m_EndBytesSelectionPosX;
          m_StartBytesSelectionPosY = m_EndBytesSelectionPosY;
          m_selectionType = SelectionType::single;
        }
        else
        {
          m_selectionType = SelectionType::downward;
        }
      }

      scrollToSelection();
    }
    break;
  case Qt::Key_PageUp:
    scrollContentsBy(0, std::min(verticalScrollBar()->pageStep(), verticalScrollBar()->value()));
    break;
  case Qt::Key_PageDown:
    scrollContentsBy(0, std::max(-verticalScrollBar()->pageStep(),
                                 verticalScrollBar()->value() - verticalScrollBar()->maximum()));
    break;
  case Qt::Key_Home:
    verticalScrollBar()->setValue(0);
    m_StartBytesSelectionPosX = 0;
    m_StartBytesSelectionPosY = 0;
    m_EndBytesSelectionPosX = 0;
    m_EndBytesSelectionPosY = 0;
    m_selectionType = SelectionType::single;
    viewport()->update();
    break;
  case Qt::Key_End:
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    m_StartBytesSelectionPosX = m_numColumns - 1;
    m_StartBytesSelectionPosY = m_numRows - 1;
    m_EndBytesSelectionPosX = m_numColumns - 1;
    m_EndBytesSelectionPosY = m_numRows - 1;
    m_selectionType = SelectionType::single;
    viewport()->update();
    break;
  default:
    return false;
  }

  // Always set the carrot at the start of a byte after navigating
  m_carretBetweenHex = false;
  return true;
}

void MemViewer::writeCharacterToSelectedMemory(char byteToWrite)
{
  const size_t memoryOffset =
      ((m_StartBytesSelectionPosY * m_numColumns) + m_StartBytesSelectionPosX);
  if (m_editingHex)
  {
    // Convert ascii to actual value
    if (byteToWrite >= 'A')
      byteToWrite -= 'A' - 10;
    else if (byteToWrite >= '0')
      byteToWrite -= '0';

    const char selectedMemoryValue = *(m_updatedRawMemoryData + memoryOffset);
    if (m_carretBetweenHex)
      byteToWrite = (selectedMemoryValue & 0xF0) | byteToWrite;
    else
      byteToWrite = (selectedMemoryValue & 0x0F) | (byteToWrite << 4);
  }

  Common::writeToRAM(&byteToWrite, m_currentFirstAddress + (u32)memoryOffset, 1, false);
}

void MemViewer::keyPressEvent(QKeyEvent* event)
{
  if (!m_validMemory)
  {
    event->ignore();
    return;
  }

  QString text = event->text();

  if (text.isEmpty())
  {
    if (!handleNaviguationKey(event->key(), event->modifiers() & Qt::ShiftModifier))
      event->ignore();
    return;
  }

  if (m_editingHex)
  {
    // Beep when entering a non-valid value
    const std::string hexChar = event->text().toUpper().toStdString();
    const char value = hexChar[0];
    if (!(value >= '0' && value <= '9') && !(value >= 'A' && value <= 'F'))
    {
      QApplication::beep();
      return;
    }

    writeCharacterToSelectedMemory(value);
    m_carretBetweenHex = !m_carretBetweenHex;
  }
  else
  {
    std::string str = text.toStdString();
    writeCharacterToSelectedMemory(str[0]);
  }

  if (!m_carretBetweenHex || !m_editingHex)
    handleNaviguationKey(Qt::Key::Key_Right, false);

  updateMemoryData();
  viewport()->update();
}

void MemViewer::scrollContentsBy(int dx, int dy)
{
  if (!m_disableScrollContentEvent && m_validMemory)
  {
    u32 newAddress = m_currentFirstAddress + m_numColumns * (-dy);

    // Move selection
    m_StartBytesSelectionPosY += dy;
    m_EndBytesSelectionPosY += dy;

    if (newAddress < m_memViewStart)
      newAddress = m_memViewStart;
    else if (newAddress > m_memViewEnd - m_numCells)
      newAddress = m_memViewEnd - m_numCells;

    if (newAddress != m_currentFirstAddress)
      jumpToAddress(newAddress);
  }
}

void MemViewer::renderSeparatorLines(QPainter& painter)
{
  QColor oldPenColor = painter.pen().color();
  painter.setPen(QGuiApplication::palette().color(QPalette::WindowText));

  painter.drawLine(m_hexAsciiSeparatorPosX, 0, m_hexAsciiSeparatorPosX,
                   m_columnHeaderHeight + m_hexAreaHeight);
  painter.drawLine(m_rowHeaderWidth - m_charWidthEm / 2, 0, m_rowHeaderWidth - m_charWidthEm / 2,
                   m_columnHeaderHeight + m_hexAreaHeight);
  painter.drawLine(0, m_columnHeaderHeight, m_hexAsciiSeparatorPosX + 17 * m_charWidthEm,
                   m_columnHeaderHeight);

  if (DMEConfig::getInstance().getViewerNbrBytesSeparator() != 0)
  {
    int bytesSeparatorXPos = m_rowHeaderWidth - m_charWidthEm / 2 + m_charWidthEm / 4;
    for (int i = 0; i < m_numColumns / DMEConfig::getInstance().getViewerNbrBytesSeparator() - 1;
         i++)
    {
      bytesSeparatorXPos +=
          (m_charWidthEm * 2) * DMEConfig::getInstance().getViewerNbrBytesSeparator() +
          (m_charWidthEm / 2) * DMEConfig::getInstance().getViewerNbrBytesSeparator();
      painter.drawLine(bytesSeparatorXPos, 0, bytesSeparatorXPos,
                       m_columnHeaderHeight + m_hexAreaHeight);
    }
  }
  painter.setPen(oldPenColor);
}

void MemViewer::renderColumnsHeaderText(QPainter& painter)
{
  QColor oldPenColor = painter.pen().color();
  painter.setPen(QGuiApplication::palette().color(QPalette::WindowText));
  painter.drawText(m_charWidthEm * 1.5f, m_charHeight, tr("Address"));
  int posXHeaderText = m_rowHeaderWidth;
  for (int i = 0; i < m_numColumns; i++)
  {
    std::stringstream ss;
    int byte = (m_currentFirstAddress + i) & 0xF;
    ss << std::hex << std::uppercase << byte;
    std::string headerText = "." + ss.str();
    painter.drawText(posXHeaderText, m_charHeight, QString::fromStdString(headerText));
    posXHeaderText += m_charWidthEm * 2 + m_charWidthEm / 2;
  }

  painter.drawText(m_hexAsciiSeparatorPosX + m_charWidthEm * 2.5f, m_charHeight,
                   tr("Text (ASCII)"));
  painter.drawText(0, 0, 0, 0, 0, QString());
  painter.setPen(oldPenColor);
}

void MemViewer::renderRowHeaderText(QPainter& painter, const int rowIndex)
{
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(sizeof(u32) * 2) << std::hex << std::uppercase
     << m_currentFirstAddress + m_numColumns * rowIndex;
  int x = m_charWidthEm / 2;
  int y = (rowIndex + 1) * m_charHeight + m_columnHeaderHeight;
  QColor oldPenColor = painter.pen().color();
  painter.setPen(QGuiApplication::palette().color(QPalette::WindowText));
  painter.drawText(x, y, QString::fromStdString(ss.str()));
  painter.setPen(oldPenColor);
}

void MemViewer::renderCarret(QPainter& painter, const int rowIndex, const int columnIndex)
{
  int posXHex = m_rowHeaderWidth + (m_charWidthEm * 2 + m_charWidthEm / 2) * columnIndex;
  QColor oldPenColor = painter.pen().color();
  int carretPosX = posXHex + (m_carretBetweenHex ? m_charWidthEm : 0);
  painter.setPen(QColor(Qt::red));
  painter.drawLine(carretPosX,
                   rowIndex * m_charHeight + (m_charHeight - fontMetrics().overlinePos()) +
                       m_columnHeaderHeight,
                   carretPosX,
                   rowIndex * m_charHeight + (m_charHeight - fontMetrics().overlinePos()) +
                       m_columnHeaderHeight + m_charHeight - 1);
  painter.setPen(oldPenColor);
}

void MemViewer::determineMemoryTextRenderProperties(const int rowIndex, const int columnIndex,
                                                    bool& drawCarret, QColor& bgColor,
                                                    QColor& fgColor)
{
  int currentIndex = rowIndex * m_numColumns + columnIndex;
  int selectionStartIndex = m_StartBytesSelectionPosY * m_numColumns + m_StartBytesSelectionPosX;
  int selectionEndIndex = m_EndBytesSelectionPosY * m_numColumns + m_EndBytesSelectionPosX;

  if (currentIndex >= selectionStartIndex && currentIndex <= selectionEndIndex)
  {
    bgColor = QGuiApplication::palette().color(QPalette::Highlight);
    fgColor = QGuiApplication::palette().color(QPalette::HighlightedText);
    if (selectionStartIndex == selectionEndIndex)
      drawCarret = true;
  }
  // If the byte changed since the last data update
  else if (m_lastRawMemoryData[rowIndex * m_numColumns + columnIndex] !=
           m_updatedRawMemoryData[rowIndex * m_numColumns + columnIndex])
  {
    m_memoryMsElapsedLastChange[rowIndex * m_numColumns + columnIndex] = m_elapsedTimer.elapsed();
    bgColor = QColor(Qt::red);
  }
  // If the last changes is less than a second old
  else if (m_memoryMsElapsedLastChange[rowIndex * m_numColumns + columnIndex] != 0 &&
           m_elapsedTimer.elapsed() -
                   m_memoryMsElapsedLastChange[rowIndex * m_numColumns + columnIndex] <
               1000)
  {
    QColor baseColor = QColor(Qt::red);
    float alphaPercentage =
        (1000 - (m_elapsedTimer.elapsed() -
                 m_memoryMsElapsedLastChange[rowIndex * m_numColumns + columnIndex])) /
        (1000 / 100);
    int newAlpha = std::trunc(baseColor.alpha() * (alphaPercentage / 100));
    bgColor = QColor(baseColor.red(), baseColor.green(), baseColor.blue(), newAlpha);
  }
}

void MemViewer::renderHexByte(QPainter& painter, const int rowIndex, const int columnIndex,
                              QColor& bgColor, QColor& fgColor, bool drawCarret)
{
  int posXHex = m_rowHeaderWidth + (m_charWidthEm * 2 + m_charWidthEm / 2) * columnIndex;
  std::string hexByte = Common::formatMemoryToString(
      m_updatedRawMemoryData + ((rowIndex * m_numColumns) + columnIndex),
      Common::MemType::type_byteArray, 1, Common::MemBase::base_none, true);
  QRect* currentByteRect = new QRect(posXHex,
                                     m_columnHeaderHeight + rowIndex * m_charHeight +
                                         (m_charHeight - fontMetrics().overlinePos()),
                                     m_charWidthEm * 2, m_charHeight);

  painter.fillRect(*currentByteRect, bgColor);
  if (drawCarret)
    renderCarret(painter, rowIndex, columnIndex);

  painter.setPen(fgColor);
  painter.drawText(posXHex, (rowIndex + 1) * m_charHeight + m_columnHeaderHeight,
                   QString::fromStdString(hexByte));
  delete currentByteRect;
}

void MemViewer::renderASCIIText(QPainter& painter, const int rowIndex, const int columnIndex,
                                QColor& bgColor, QColor& fgColor)
{
  std::string asciiStr = Common::formatMemoryToString(
      m_updatedRawMemoryData + ((rowIndex * m_numColumns) + columnIndex),
      Common::MemType::type_string, 1, Common::MemBase::base_none, true);
  int asciiByte = (int)asciiStr[0];
  if (asciiByte > 0x7E || asciiByte < 0x20)
    asciiStr = ".";
  QRect* currentCharRect = new QRect(
      (columnIndex * m_charWidthEm) + m_hexAsciiSeparatorPosX + m_charWidthEm / 2,
      rowIndex * m_charHeight + (m_charHeight - fontMetrics().overlinePos()) + m_columnHeaderHeight,
      m_charWidthEm, m_charHeight);
  painter.fillRect(*currentCharRect, bgColor);
  painter.setPen(fgColor);
  painter.drawText((columnIndex * m_charWidthEm) + m_hexAsciiSeparatorPosX + m_charWidthEm / 2,
                   (rowIndex + 1) * m_charHeight + m_columnHeaderHeight,
                   QString::fromStdString(asciiStr));
  delete currentCharRect;
}

void MemViewer::renderMemory(QPainter& painter, const int rowIndex, const int columnIndex)
{
  QColor oldPenColor = painter.pen().color();
  QColor fgColor = QGuiApplication::palette().color(QPalette::WindowText);
  int posXHex = m_rowHeaderWidth + (m_charWidthEm * 2 + m_charWidthEm / 2) * columnIndex;
  if (!(m_currentFirstAddress + (m_numColumns * rowIndex + columnIndex) >= m_memViewStart &&
        m_currentFirstAddress + (m_numColumns * rowIndex + columnIndex) < m_memViewEnd) ||
      !Common::isValidAddress(m_currentFirstAddress + (m_numColumns * rowIndex + columnIndex)) ||
      !m_validMemory)
  {
    painter.setPen(fgColor);
    painter.drawText(posXHex, (rowIndex + 1) * m_charHeight + m_columnHeaderHeight, tr("??"));
    painter.drawText((columnIndex * m_charWidthEm) + m_hexAsciiSeparatorPosX + m_charWidthEm / 2,
                     (rowIndex + 1) * m_charHeight + m_columnHeaderHeight, tr("?"));
  }
  else
  {
    QColor bgColor = QColor(Qt::transparent);
    bool drawCarret = false;

    determineMemoryTextRenderProperties(rowIndex, columnIndex, drawCarret, bgColor, fgColor);

    renderHexByte(painter, rowIndex, columnIndex, bgColor, fgColor, drawCarret);
    renderASCIIText(painter, rowIndex, columnIndex, bgColor, fgColor);
  }
  painter.setPen(oldPenColor);
}

void MemViewer::paintEvent(QPaintEvent* event)
{
  QPainter painter(viewport());
  painter.setPen(QColor(Qt::black));

  renderSeparatorLines(painter);
  renderColumnsHeaderText(painter);

  for (int i = 0; i < m_numRows; ++i)
  {
    renderRowHeaderText(painter, i);
    for (int j = 0; j < m_numColumns; ++j)
      renderMemory(painter, i, j);
  }
}

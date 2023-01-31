#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

#include "MemViewer.h"

class MemViewerWidget : public QWidget
{
  Q_OBJECT

public:
  MemViewerWidget(QWidget* parent, u32 consoleAddress);
  ~MemViewerWidget();

  void onJumpToAddressTextChanged();
  void onGoToMEM1Start();
  void onGoToSecondaryRAMStart();
  QTimer* getUpdateTimer() const;
  void hookStatusChanged(bool hook);
  void onMEM2StatusChanged(bool enabled);
  void goToAddress(u32 address);

signals:
  void mustUnhook();
  void addWatchRequested(MemWatchEntry* entry);

private:
  void initialiseWidgets();
  void makeLayouts();

  QLineEdit* m_txtJumpAddress;
  QPushButton* m_btnGoToMEM1Start;
  QPushButton* m_btnGoToSecondaryRAMStart;
  QTimer* m_updateMemoryTimer;
  MemViewer* m_memViewer;
};

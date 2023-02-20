#pragma once

#include <QCloseEvent>
#include <QDialog>

#include "MemScanner/MemScanWidget.h"
#include "MemViewer/MemViewerWidget.h"
#include "MemWatcher/MemWatchWidget.h"
#include "MemoryEngine/Common/CommonTypes.h"
#include "MemoryEngine/Common/MemoryCommon.h"

class MemoryEngine : public QDialog
{
  Q_OBJECT

public:
  MemoryEngine(QWidget* parent = nullptr);
  ~MemoryEngine();
  void closeEvent(QCloseEvent* event) override;
  void forceQuit();
  void addWatchRequested(u32 address, Common::MemType type, size_t length, bool isUnsigned,
                         Common::MemBase base);
  void addSelectedResultsToWatchList(Common::MemType type, size_t length, bool isUnsigned,
                                     Common::MemBase base);
  void addAllResultsToWatchList(Common::MemType type, size_t length, bool isUnsigned,
                                Common::MemBase base);
  void onOpenMemViewer();
  void onOpenMemViewerWithAddress(u32 address);

  void onOpenWatchFile();
  void onSaveWatchFile();
  void onSaveAsWatchFile();
  void onClearWatchList();
  void onOpenSettings();
  void onImportFromCT();
  void onExportAsCSV();
  void onShowScanner(bool show);
  void onQuit();
  void onEmulationStateChanged(bool running);

signals:
  void sigQuit();

private:
  // void makeMenus();
  void initialiseWidgets();
  void makeLayouts();
  void makeMemViewer();
  void start();

  MemWatchWidget* m_watcher;
  MemScanWidget* m_scanner;
  MemViewerWidget* m_viewer;

  QLabel* m_lblMem2Status;
  QPushButton* m_btnOpenMemViewer;
};

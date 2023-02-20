#include "MemoryEngine.h"

#include <QHBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QString>
#include <QVBoxLayout>
#include <string>

#include "MemoryEngine/Common/MemoryCommon.h"
#include "MemoryEngine/MemoryWatch/MemWatchEntry.h"
#include "Settings/DMEConfig.h"
#include "Settings/DlgSettings.h"

MemoryEngine::MemoryEngine(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Memory Engine"));
  initialiseWidgets();
  makeLayouts();
  // makeMenus();
  makeMemViewer();
}

MemoryEngine::~MemoryEngine()
{
  delete m_viewer;
  delete m_watcher;
}

/*void MemoryEngine::makeMenus()
{
  m_actOpenWatchList = new QAction(tr("&Open..."), this);
  m_actSaveWatchList = new QAction(tr("&Save"), this);
  m_actSaveAsWatchList = new QAction(tr("&Save as..."), this);
  m_actClearWatchList = new QAction(tr("&Clear the watch list"), this);
  m_actImportFromCT = new QAction(tr("&Import from Cheat Engine's CT file..."), this);
  m_actExportAsCSV = new QAction(tr("&Export as CSV..."), this);

  m_actOpenWatchList->setShortcut(Qt::Modifier::CTRL + Qt::Key::Key_O);
  m_actSaveWatchList->setShortcut(Qt::Modifier::CTRL + Qt::Key::Key_S);
  m_actSaveAsWatchList->setShortcut(Qt::Modifier::CTRL + Qt::Modifier::SHIFT + Qt::Key::Key_S);
  m_actImportFromCT->setShortcut(Qt::Modifier::CTRL + Qt::Key::Key_I);

  m_actSettings = new QAction(tr("&Settings"), this);

  m_actViewScanner = new QAction(tr("&Scanner"), this);
  m_actViewScanner->setCheckable(true);
  m_actViewScanner->setChecked(true);

  m_actQuit = new QAction(tr("&Quit"), this);
  m_actAbout = new QAction(tr("&About"), this);
  connect(m_actOpenWatchList, &QAction::triggered, this, &MemoryEngine::onOpenWatchFile);
  connect(m_actSaveWatchList, &QAction::triggered, this, &MemoryEngine::onSaveWatchFile);
  connect(m_actSaveAsWatchList, &QAction::triggered, this, &MemoryEngine::onSaveAsWatchFile);
  connect(m_actClearWatchList, &QAction::triggered, this, &MemoryEngine::onClearWatchList);
  connect(m_actImportFromCT, &QAction::triggered, this, &MemoryEngine::onImportFromCT);
  connect(m_actExportAsCSV, &QAction::triggered, this, &MemoryEngine::onExportAsCSV);

  connect(m_actSettings, &QAction::triggered, this, &MemoryEngine::onOpenSettings);

  connect(m_actViewScanner, &QAction::toggled, this,
          [=]
          {
            if (m_actViewScanner->isChecked())
              m_scanner->show();
            else
              m_scanner->hide();
          });

  connect(m_actQuit, &QAction::triggered, this, &MemoryEngine::onQuit);
  connect(m_actAbout, &QAction::triggered, this, &MemoryEngine::onAbout);

  m_menuFile = menuBar()->addMenu(tr("&File"));
  m_menuFile->addAction(m_actOpenWatchList);
  m_menuFile->addAction(m_actSaveWatchList);
  m_menuFile->addAction(m_actSaveAsWatchList);
  m_menuFile->addAction(m_actClearWatchList);
  m_menuFile->addAction(m_actImportFromCT);
  m_menuFile->addAction(m_actExportAsCSV);
  m_menuFile->addAction(m_actQuit);

  m_menuEdit = menuBar()->addMenu(tr("&Edit"));
  m_menuEdit->addAction(m_actSettings);

  m_menuView = menuBar()->addMenu(tr("&View"));
  m_menuView->addAction(m_actViewScanner);

  m_menuHelp = menuBar()->addMenu(tr("&Help"));
  m_menuHelp->addAction(m_actAbout);
}*/

void MemoryEngine::initialiseWidgets()
{
  m_scanner = new MemScanWidget();
  connect(m_scanner, &MemScanWidget::requestAddWatchEntry, this, &MemoryEngine::addWatchRequested);
  connect(m_scanner, &MemScanWidget::requestAddAllResultsToWatchList, this,
          &MemoryEngine::addAllResultsToWatchList);
  connect(m_scanner, &MemScanWidget::requestAddSelectedResultsToWatchList, this,
          &MemoryEngine::addSelectedResultsToWatchList);

  m_watcher = new MemWatchWidget(this);

  m_lblMem2Status = new QLabel(tr(""));
  m_lblMem2Status->setAlignment(Qt::AlignHCenter);

  m_btnOpenMemViewer = new QPushButton(tr("Open memory viewer"));
  connect(m_btnOpenMemViewer, &QPushButton::clicked, this, &MemoryEngine::onOpenMemViewer);
}

void MemoryEngine::makeLayouts()
{
  QFrame* separatorline = new QFrame();
  separatorline->setFrameShape(QFrame::HLine);

  QVBoxLayout* mainLayout = new QVBoxLayout;
  mainLayout->addWidget(m_lblMem2Status);
  mainLayout->addWidget(separatorline);
  mainLayout->addWidget(m_scanner);
  mainLayout->addSpacing(5);
  mainLayout->addWidget(m_btnOpenMemViewer);
  mainLayout->addSpacing(5);
  mainLayout->addWidget(m_watcher);

  setLayout(mainLayout);
}

void MemoryEngine::makeMemViewer()
{
  m_viewer = new MemViewerWidget(nullptr, (size_t)Common::getMEM1());
  connect(m_viewer, &MemViewerWidget::addWatchRequested, m_watcher, &MemWatchWidget::addWatchEntry);
  connect(m_watcher, &MemWatchWidget::goToAddressInViewer, this,
          &MemoryEngine::onOpenMemViewerWithAddress);
}

void MemoryEngine::addSelectedResultsToWatchList(Common::MemType type, size_t length,
                                                 bool isUnsigned, Common::MemBase base)
{
  QModelIndexList selection = m_scanner->getSelectedResults();
  for (int i = 0; i < selection.count(); i++)
  {
    u32 address = m_scanner->getResultListModel()->getResultAddress(selection.at(i).row());
    MemWatchEntry* newEntry =
        new MemWatchEntry(tr("No label"), address, type, base, isUnsigned, length);
    m_watcher->addWatchEntry(newEntry);
  }
}

void MemoryEngine::addAllResultsToWatchList(Common::MemType type, size_t length, bool isUnsigned,
                                            Common::MemBase base)
{
  for (auto item : m_scanner->getAllResults())
  {
    MemWatchEntry* newEntry =
        new MemWatchEntry(tr("No label"), item, type, base, isUnsigned, length);
    m_watcher->addWatchEntry(newEntry);
  }
}

void MemoryEngine::addWatchRequested(u32 address, Common::MemType type, size_t length,
                                     bool isUnsigned, Common::MemBase base)
{
  MemWatchEntry* newEntry =
      new MemWatchEntry(tr("No label"), address, type, base, isUnsigned, length);
  m_watcher->addWatchEntry(newEntry);
}

void MemoryEngine::onOpenMemViewer()
{
  m_viewer->show();
  m_viewer->raise();
}

void MemoryEngine::onOpenMemViewerWithAddress(u32 address)
{
  m_viewer->goToAddress(address);
  m_viewer->show();
}

void MemoryEngine::onOpenWatchFile()
{
  if (m_watcher->warnIfUnsavedChanges())
    m_watcher->openWatchFile();
}

void MemoryEngine::onSaveWatchFile()
{
  m_watcher->saveWatchFile();
}

void MemoryEngine::onSaveAsWatchFile()
{
  m_watcher->saveAsWatchFile();
}

void MemoryEngine::onClearWatchList()
{
  m_watcher->clearWatchList();
}

void MemoryEngine::onImportFromCT()
{
  m_watcher->importFromCTFile();
}

void MemoryEngine::onExportAsCSV()
{
  m_watcher->exportWatchListAsCSV();
}

void MemoryEngine::onShowScanner(bool show)
{
  if (show)
    m_scanner->show();
  else
    m_scanner->hide();
}

void MemoryEngine::onOpenSettings()
{
  DlgSettings* dlg = new DlgSettings(this);
  int dlgResult = dlg->exec();
  delete dlg;
  if (dlgResult == QDialog::Accepted)
  {
    m_watcher->getUpdateTimer()->stop();
    m_watcher->getFreezeTimer()->stop();
    m_viewer->getUpdateTimer()->stop();
    if (Common::hasMemory())
    {
      m_watcher->getUpdateTimer()->start(DMEConfig::getInstance().getWatcherUpdateTimerMs());
      m_watcher->getFreezeTimer()->start(DMEConfig::getInstance().getFreezeTimerMs());
      m_viewer->getUpdateTimer()->start(DMEConfig::getInstance().getViewerUpdateTimerMs());
    }
  }
}

void MemoryEngine::onQuit()
{
  close();
}

void MemoryEngine::onEmulationStateChanged(bool running)
{
  if (running)
  {
    m_watcher->getUpdateTimer()->start(DMEConfig::getInstance().getWatcherUpdateTimerMs());
    m_watcher->getFreezeTimer()->start(DMEConfig::getInstance().getFreezeTimerMs());
    m_viewer->getUpdateTimer()->start(DMEConfig::getInstance().getViewerUpdateTimerMs());
  }
  else
  {
    m_watcher->getUpdateTimer()->stop();
    m_watcher->getFreezeTimer()->stop();
    m_viewer->getUpdateTimer()->stop();
  }
}

void MemoryEngine::closeEvent(QCloseEvent* event)
{
  if (m_watcher->warnIfUnsavedChanges())
  {
    m_viewer->close();
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

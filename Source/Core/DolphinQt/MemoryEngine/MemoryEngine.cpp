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
  makeMemViewer();
}

MemoryEngine::~MemoryEngine()
{
  delete m_viewer;
  delete m_watcher;
}

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

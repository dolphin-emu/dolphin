#include "MemWatchWidget.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIODevice>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QSignalMapper>
#include <QString>
#include <QTextStream>
#include <QVBoxLayout>
#include <string>

#include "../../../MemoryEngine/Common/MemoryCommon.h"
#include "../../../MemoryEngine/MemoryWatch/MemWatchEntry.h"
#include "../GUICommon.h"
#include "Dialogs/DlgAddWatchEntry.h"
#include "Dialogs/DlgChangeType.h"
#include "Dialogs/DlgImportCTFile.h"

MemWatchWidget::MemWatchWidget(QWidget* parent) : QWidget(parent)
{
  initialiseWidgets();
  makeLayouts();
}

MemWatchWidget::~MemWatchWidget()
{
  delete m_watchDelegate;
  delete m_watchModel;
}

void MemWatchWidget::initialiseWidgets()
{
  m_watchModel = new MemWatchModel(this);
  connect(m_watchModel, &MemWatchModel::writeFailed, this, &MemWatchWidget::onValueWriteError);
  connect(m_watchModel, &MemWatchModel::dropSucceeded, this, &MemWatchWidget::onDropSucceeded);
  connect(m_watchModel, &MemWatchModel::readFailed, this, &MemWatchWidget::mustUnhook);

  m_watchDelegate = new MemWatchDelegate();

  m_watchView = new QTreeView;
  m_watchView->setDragEnabled(true);
  m_watchView->setAcceptDrops(true);
  m_watchView->setDragDropMode(QAbstractItemView::InternalMove);
  m_watchView->setDropIndicatorShown(true);
  m_watchView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_watchView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_watchView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_watchView, &QWidget::customContextMenuRequested, this,
          &MemWatchWidget::onMemWatchContextMenuRequested);
  connect(m_watchView, &QAbstractItemView::doubleClicked, this,
          &MemWatchWidget::onWatchDoubleClicked);
  m_watchView->setItemDelegate(m_watchDelegate);
  m_watchView->setSortingEnabled(true);
  m_watchView->setModel(m_watchModel);

  m_watchView->header()->resizeSection(MemWatchModel::WATCH_COL_LOCK, 50);
  m_watchView->header()->resizeSection(MemWatchModel::WATCH_COL_LABEL, 225);
  m_watchView->header()->resizeSection(MemWatchModel::WATCH_COL_TYPE, 130);
  m_watchView->header()->resizeSection(MemWatchModel::WATCH_COL_ADDRESS, 120);

  QShortcut* deleteWatchShortcut = new QShortcut(QKeySequence::Delete, m_watchView);
  connect(deleteWatchShortcut, &QShortcut::activated, this, &MemWatchWidget::onDeleteSelection);

  QShortcut* copyWatchShortcut =
      new QShortcut(QKeySequence(Qt::Modifier::CTRL + Qt::Key::Key_C), m_watchView);
  connect(copyWatchShortcut, &QShortcut::activated, this,
          &MemWatchWidget::copySelectedWatchesToClipBoard);

  QShortcut* cutWatchShortcut =
      new QShortcut(QKeySequence(Qt::Modifier::CTRL + Qt::Key::Key_X), m_watchView);
  connect(cutWatchShortcut, &QShortcut::activated, this,
          &MemWatchWidget::cutSelectedWatchesToClipBoard);

  QShortcut* pasteWatchShortcut =
      new QShortcut(QKeySequence(Qt::Modifier::CTRL + Qt::Key::Key_V), m_watchView);
  connect(pasteWatchShortcut, &QShortcut::activated, this, [=] {
    pasteWatchFromClipBoard(
        m_watchModel->getTreeNodeFromIndex(m_watchView->selectionModel()->currentIndex()),
        m_watchView->selectionModel()->currentIndex().row() + 1);
  });

  m_btnAddGroup = new QPushButton(tr("Add group"), this);
  connect(m_btnAddGroup, &QPushButton::clicked, this, &MemWatchWidget::onAddGroup);

  m_btnAddWatchEntry = new QPushButton(tr("Add watch"), this);
  connect(m_btnAddWatchEntry, &QPushButton::clicked, this, &MemWatchWidget::onAddWatchEntry);

  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, m_watchModel, &MemWatchModel::onUpdateTimer);

  m_freezeTimer = new QTimer(this);
  connect(m_freezeTimer, &QTimer::timeout, m_watchModel, &MemWatchModel::onFreezeTimer);
}

void MemWatchWidget::makeLayouts()
{
  QWidget* buttons = new QWidget;
  QHBoxLayout* buttons_layout = new QHBoxLayout;

  buttons_layout->addWidget(m_btnAddGroup);
  buttons_layout->addWidget(m_btnAddWatchEntry);
  buttons_layout->setContentsMargins(0, 0, 0, 0);
  buttons->setLayout(buttons_layout);

  QVBoxLayout* layout = new QVBoxLayout;

  layout->addWidget(buttons);
  layout->addWidget(m_watchView);
  layout->setContentsMargins(3, 0, 3, 0);
  setLayout(layout);
}

void MemWatchWidget::onMemWatchContextMenuRequested(const QPoint& pos)
{
  QModelIndex index = m_watchView->indexAt(pos);
  QMenu* contextMenu = new QMenu(this);
  MemWatchTreeNode* node = nullptr;
  if (index != QModelIndex())
  {
    node = m_watchModel->getTreeNodeFromIndex(index);
    if (!node->isGroup())
    {
      MemWatchEntry* entry = m_watchModel->getEntryFromIndex(index);
      int typeIndex = static_cast<int>(entry->getType());
      Common::MemType theType = static_cast<Common::MemType>(typeIndex);

      if (entry->isBoundToPointer())
      {
        QMenu* memViewerSubMenu = contextMenu->addMenu(tr("Browse &memory at"));
        QAction* showPointerInViewer = new QAction(tr("The &pointer address..."), this);
        connect(showPointerInViewer, &QAction::triggered, this,
                [=] { emit goToAddressInViewer(entry->getConsoleAddress()); });
        memViewerSubMenu->addAction(showPointerInViewer);
        for (int i = 0; i < entry->getPointerLevel(); ++i)
        {
          std::string strAddressOfPath = entry->getAddressStringForPointerLevel(i + 1);
          if (strAddressOfPath == "???")
            break;
          QAction* showAddressOfPathInViewer = new QAction(
              tr("The pointed address at &level %1...").arg(QString::number(i + 1)), this);
          connect(showAddressOfPathInViewer, &QAction::triggered, this,
                  [=] { emit goToAddressInViewer(entry->getAddressForPointerLevel(i + 1)); });
          memViewerSubMenu->addAction(showAddressOfPathInViewer);
        }

        QAction* showInViewer = new QAction(tr("Browse memory at this &address..."), this);
        connect(showInViewer, &QAction::triggered, this,
                [=] { emit goToAddressInViewer(entry->getConsoleAddress()); });
      }
      else
      {
        QAction* showInViewer = new QAction(tr("Browse memory at this &address..."), this);
        connect(showInViewer, &QAction::triggered, this,
                [=] { emit goToAddressInViewer(entry->getConsoleAddress()); });

        contextMenu->addAction(showInViewer);
      }
      if (theType != Common::MemType::type_string)
      {
        contextMenu->addSeparator();

        QAction* viewDec = new QAction(tr("View as &Decimal"), this);
        QAction* viewHex = new QAction(tr("View as &Hexadecimal"), this);
        QAction* viewOct = new QAction(tr("View as &Octal"), this);
        QAction* viewBin = new QAction(tr("View as &Binary"), this);

        connect(viewDec, &QAction::triggered, m_watchModel, [=] {
          entry->setBase(Common::MemBase::base_decimal);
          m_hasUnsavedChanges = true;
        });
        connect(viewHex, &QAction::triggered, m_watchModel, [=] {
          entry->setBase(Common::MemBase::base_hexadecimal);
          m_hasUnsavedChanges = true;
        });
        connect(viewOct, &QAction::triggered, m_watchModel, [=] {
          entry->setBase(Common::MemBase::base_octal);
          m_hasUnsavedChanges = true;
        });
        connect(viewBin, &QAction::triggered, m_watchModel, [=] {
          entry->setBase(Common::MemBase::base_binary);
          m_hasUnsavedChanges = true;
        });

        contextMenu->addAction(viewDec);
        contextMenu->addAction(viewHex);
        contextMenu->addAction(viewOct);
        contextMenu->addAction(viewBin);
        contextMenu->addSeparator();

        int baseIndex = static_cast<int>(entry->getBase());
        Common::MemBase theBase = static_cast<Common::MemBase>(baseIndex);
        contextMenu->actions().at(baseIndex + 2)->setEnabled(false);

        if (theBase == Common::MemBase::base_decimal)
        {
          QAction* viewSigned = new QAction(tr("View as &Signed"), this);
          QAction* viewUnsigned = new QAction(tr("View as &Unsigned"), this);

          connect(viewSigned, &QAction::triggered, m_watchModel, [=] {
            entry->setSignedUnsigned(false);
            m_hasUnsavedChanges = true;
          });
          connect(viewUnsigned, &QAction::triggered, m_watchModel, [=] {
            entry->setSignedUnsigned(true);
            m_hasUnsavedChanges = true;
          });

          contextMenu->addAction(viewSigned);
          contextMenu->addAction(viewUnsigned);

          if (entry->isUnsigned())
          {
            contextMenu->actions()
                .at(static_cast<int>(Common::MemBase::base_none) + 4)
                ->setEnabled(false);
          }
          else
          {
            contextMenu->actions()
                .at(static_cast<int>(Common::MemBase::base_none) + 3)
                ->setEnabled(false);
          }
        }
      }
      contextMenu->addSeparator();
    }
  }
  else
  {
    node = m_watchModel->getRootNode();
  }

  QAction* cut = new QAction(tr("Cu&t"), this);
  connect(cut, &QAction::triggered, this, [=] { cutSelectedWatchesToClipBoard(); });
  contextMenu->addAction(cut);
  QAction* copy = new QAction(tr("&Copy"), this);
  connect(copy, &QAction::triggered, this, [=] { copySelectedWatchesToClipBoard(); });
  contextMenu->addAction(copy);

  QAction* paste = new QAction(tr("&Paste"), this);
  connect(paste, &QAction::triggered, this, [=] {
    pasteWatchFromClipBoard(node, m_watchView->selectionModel()->currentIndex().row() + 1);
  });
  contextMenu->addAction(paste);

  contextMenu->addSeparator();
  QAction* deleteSelection = new QAction(tr("&Delete"), this);
  connect(deleteSelection, &QAction::triggered, this, [=] { onDeleteSelection(); });
  contextMenu->addAction(deleteSelection);

  QModelIndexList selection = m_watchView->selectionModel()->selectedRows();
  if (selection.count() == 0)
  {
    copy->setEnabled(false);
    cut->setEnabled(false);
    deleteSelection->setEnabled(false);
  }

  contextMenu->popup(m_watchView->viewport()->mapToGlobal(pos));
}

void MemWatchWidget::cutSelectedWatchesToClipBoard()
{
  copySelectedWatchesToClipBoard();

  QModelIndexList* cutList = simplifySelection();

  if (cutList->count() > 0)
  {
    for (auto i : *cutList)
      m_watchModel->removeNode(i);

    m_hasUnsavedChanges = true;
  }
}

void MemWatchWidget::copySelectedWatchesToClipBoard()
{
  QModelIndexList selection = m_watchView->selectionModel()->selectedRows();
  if (selection.count() == 0)
    return;

  QModelIndexList* toCopyList = simplifySelection();

  MemWatchTreeNode* rootNodeCopy = new MemWatchTreeNode(nullptr, nullptr, false, QString(tr("")));
  for (auto i : *toCopyList)
  {
    MemWatchTreeNode* theNode = new MemWatchTreeNode(*(m_watchModel->getTreeNodeFromIndex(i)));
    rootNodeCopy->appendChild(theNode);
  }

  QJsonObject jsonNode;
  rootNodeCopy->writeToJson(jsonNode);
  QJsonDocument doc(jsonNode);
  QString nodeJsonStr(QString::fromStdString(doc.toJson().toStdString()));

  QClipboard* clipboard = QApplication::clipboard();
  clipboard->setText(nodeJsonStr);
}

void MemWatchWidget::pasteWatchFromClipBoard(MemWatchTreeNode* node, int row)
{
  QClipboard* clipboard = QApplication::clipboard();
  QString nodeStr = clipboard->text();

  QJsonDocument loadDoc(QJsonDocument::fromJson(nodeStr.toUtf8()));
  MemWatchTreeNode* copiedRootNode = new MemWatchTreeNode(nullptr);
  copiedRootNode->readFromJson(loadDoc.object(), nullptr);
  if (copiedRootNode->hasChildren())
  {
    int numberIterated = 0;
    for (auto i : copiedRootNode->getChildren())
    {
      if (node == nullptr)
        node = m_watchModel->getRootNode();
      if (node->isGroup() || (node->getParent() == nullptr))
      {
        node->appendChild(i);
      }
      else
      {
        node->getParent()->insertChild(row + numberIterated, i);
      }
      numberIterated++;
    }
    emit m_watchModel->layoutChanged();

    m_hasUnsavedChanges = true;
  }
}

void MemWatchWidget::onWatchDoubleClicked(const QModelIndex& index)
{
  if (index != QVariant())
  {
    MemWatchTreeNode* node = static_cast<MemWatchTreeNode*>(index.internalPointer());
    if (index.column() == MemWatchModel::WATCH_COL_TYPE && !node->isGroup())
    {
      MemWatchEntry* entry = node->getEntry();
      int typeIndex = static_cast<int>(entry->getType());
      DlgChangeType* dlg = new DlgChangeType(this, typeIndex, entry->getLength());
      if (dlg->exec() == QDialog::Accepted)
      {
        Common::MemType theType = static_cast<Common::MemType>(dlg->getTypeIndex());
        m_watchModel->changeType(index, theType, dlg->getLength());
        m_hasUnsavedChanges = true;
      }
    }
    else if (index.column() == MemWatchModel::WATCH_COL_ADDRESS && !node->isGroup())
    {
      MemWatchEntry* entryCopy = new MemWatchEntry(node->getEntry());
      DlgAddWatchEntry* dlg = new DlgAddWatchEntry(entryCopy);
      if (dlg->exec() == QDialog::Accepted)
      {
        m_watchModel->editEntry(entryCopy, index);
        m_hasUnsavedChanges = true;
      }
    }
  }
}

void MemWatchWidget::onValueWriteError(const QModelIndex& index,
                                       Common::MemOperationReturnCode writeReturn)
{
  switch (writeReturn)
  {
  case Common::MemOperationReturnCode::invalidInput:
  {
    MemWatchTreeNode* node = m_watchModel->getTreeNodeFromIndex(index);
    MemWatchEntry* entry = node->getEntry();
    int typeIndex = static_cast<int>(entry->getType());
    int baseIndex = static_cast<int>(entry->getBase());

    QString errorMsg;
    if (entry->getType() == Common::MemType::type_byteArray)
      errorMsg = tr("The value you entered for the type %1 is invalid, you must enter the bytes in "
                    "hexadecimal with one space between each byte")
                     .arg(GUICommon::g_memTypeNames.at(typeIndex));
    else
      errorMsg = tr("The value you entered for the type %1 in the base %2 is invalid")
                     .arg(GUICommon::g_memTypeNames.at(typeIndex))
                     .arg(GUICommon::g_memBaseNames.at(baseIndex));
    QMessageBox* errorBox = new QMessageBox(QMessageBox::Critical, tr("Invalid value"), errorMsg,
                                            QMessageBox::Ok, this);
    errorBox->exec();
    break;
  }
  case Common::MemOperationReturnCode::inputTooLong:
  {
    MemWatchTreeNode* node = static_cast<MemWatchTreeNode*>(index.internalPointer());
    MemWatchEntry* entry = node->getEntry();
    int typeIndex = static_cast<int>(entry->getType());

    QString errorMsg(tr("The value you entered for the type ") +
                     GUICommon::g_memTypeNames.at(typeIndex) + tr(" for the length ") +
                     QString::fromStdString(std::to_string(entry->getLength())) +
                     tr(" is too long for the given length"));
    if (entry->getType() == Common::MemType::type_byteArray)
      errorMsg += tr(", you must enter the bytes in hexadecimal with one space between each byte");
    QMessageBox* errorBox = new QMessageBox(QMessageBox::Critical, QString(tr("Invalid value")),
                                            errorMsg, QMessageBox::Ok, this);
    errorBox->exec();
    break;
  }
  case Common::MemOperationReturnCode::invalidPointer:
  {
    QMessageBox* errorBox = new QMessageBox(QMessageBox::Critical, QString(tr("Invalid pointer")),
                                            tr("This pointer points to invalid memory, therefore, you "
                                            "cannot write to the value of this watch"),
                                            QMessageBox::Ok, this);
    errorBox->exec();
    break;
  }
  case Common::MemOperationReturnCode::operationFailed:
  {
    emit mustUnhook();
    break;
  }
  }
}

void MemWatchWidget::onAddGroup()
{
  bool ok = false;
  QString text = QInputDialog::getText(this, tr("Add a group"),
                                       tr("Enter the group name:"), QLineEdit::Normal, tr(""), &ok);
  if (ok && !text.isEmpty())
  {
    m_watchModel->addGroup(text);
    m_hasUnsavedChanges = true;
  }
}

void MemWatchWidget::onAddWatchEntry()
{
  DlgAddWatchEntry* dlg = new DlgAddWatchEntry(nullptr);
  if (dlg->exec() == QDialog::Accepted)
    addWatchEntry(dlg->getEntry());
}

void MemWatchWidget::addWatchEntry(MemWatchEntry* entry)
{
  m_watchModel->addEntry(entry);
  m_hasUnsavedChanges = true;
}

QModelIndexList* MemWatchWidget::simplifySelection() const
{
  QModelIndexList* simplifiedSelection = new QModelIndexList();
  QModelIndexList selection = m_watchView->selectionModel()->selectedRows();

  // Discard all indexes whose parent is selected already
  for (int i = 0; i < selection.count(); ++i)
  {
    const QModelIndex index = selection.at(i);
    if (!isAnyAncestorSelected(index))
      simplifiedSelection->append(index);
  }
  return simplifiedSelection;
}

bool MemWatchWidget::isAnyAncestorSelected(const QModelIndex index) const
{
  if (m_watchModel->parent(index) == QModelIndex())
    return false;
  else if (m_watchView->selectionModel()->isSelected(index.parent()))
    return true;
  else
    return isAnyAncestorSelected(index.parent());
}

void MemWatchWidget::onDeleteSelection()
{
  QModelIndexList selection = m_watchView->selectionModel()->selectedRows();
  if (selection.count() == 0)
    return;

  bool hasGroupWithChild = false;
  for (int i = 0; i < selection.count(); i++)
  {
    const QModelIndex index = selection.at(i);
    MemWatchTreeNode* node = static_cast<MemWatchTreeNode*>(index.internalPointer());
    if (node->isGroup() && node->hasChildren())
    {
      hasGroupWithChild = true;
      break;
    }
  }

  QString confirmationMsg = tr("Are you sure you want to delete these watches and/or groups?");
  if (hasGroupWithChild)
    confirmationMsg +=
        tr("\n\nThe current selection contains one or more groups with watches in them, deleting "
        "the "
        "groups will also delete their watches, if you want to avoid this, move the watches out "
        "of "
        "the groups.");

  QMessageBox* confirmationBox =
      new QMessageBox(QMessageBox::Question, QString(tr("Deleting confirmation")), confirmationMsg,
                      QMessageBox::Yes | QMessageBox::No, this);
  confirmationBox->setDefaultButton(QMessageBox::Yes);
  if (confirmationBox->exec() == QMessageBox::Yes)
  {
    QModelIndexList* toDeleteList = simplifySelection();

    for (auto i : *toDeleteList)
      m_watchModel->removeNode(i);

    m_hasUnsavedChanges = true;
  }
}

void MemWatchWidget::onDropSucceeded()
{
  m_hasUnsavedChanges = true;
}

QTimer* MemWatchWidget::getUpdateTimer() const
{
  return m_updateTimer;
}

QTimer* MemWatchWidget::getFreezeTimer() const
{
  return m_freezeTimer;
}

void MemWatchWidget::openWatchFile()
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Open watch list"), m_watchListFile,
                                                  tr("Dolphin memory watches file (*.dmw)"));
  if (fileName != tr(""))
  {
    if (m_watchModel->hasAnyNodes())
    {
      QMessageBox* questionBox = new QMessageBox(
          QMessageBox::Question, tr("Asking to merge lists"),
          tr("The current watch list has entries in it, do you want to merge it with this one?"),
          QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes, this);
      int answer = questionBox->exec();
      if (answer == QMessageBox::Cancel)
        return;
      else if (answer == QMessageBox::No)
        m_watchModel->clearRoot();
    }

    QFile watchFile(fileName);
    if (!watchFile.exists())
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Critical, QString(tr("Error while opening file")),
          QString(tr("The watch list file ") + fileName + tr(" does not exist")), QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }
    if (!watchFile.open(QIODevice::ReadOnly))
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Critical, tr("Error while opening file"),
          tr("An error occured while opening the watch list file for reading"), QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }
    QByteArray bytes = watchFile.readAll();
    watchFile.close();
    QJsonDocument loadDoc(QJsonDocument::fromJson(bytes));
    m_watchModel->loadRootFromJsonRecursive(loadDoc.object());
    m_watchListFile = fileName;
    m_hasUnsavedChanges = false;
  }
}

void MemWatchWidget::saveWatchFile()
{
  if (QFile::exists(m_watchListFile))
  {
    QFile watchFile(m_watchListFile);
    if (!watchFile.open(QIODevice::WriteOnly))
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Critical, tr("Error while creating file"),
          tr("An error occured while creating and opening the watch list file for writting"),
          QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }
    QJsonObject root;
    m_watchModel->writeRootToJsonRecursive(root);
    QJsonDocument saveDoc(root);
    watchFile.write(saveDoc.toJson());
    watchFile.close();
    m_hasUnsavedChanges = false;
  }
  else
  {
    saveAsWatchFile();
  }
}

void MemWatchWidget::saveAsWatchFile()
{
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save watch list"), m_watchListFile,
                                                  tr("Dolphin memory watches file (*.dmw)"));
  if (fileName != tr(""))
  {
    if (!fileName.endsWith(tr(".dmw")))
      fileName.append(tr(".dmw"));
    QFile watchFile(fileName);
    if (!watchFile.open(QIODevice::WriteOnly))
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Critical, tr("Error while creating file"),
          tr("An error occured while creating and opening the watch list file for writting"),
          QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }
    QJsonObject root;
    m_watchModel->writeRootToJsonRecursive(root);
    QJsonDocument saveDoc(root);
    watchFile.write(saveDoc.toJson());
    watchFile.close();
    m_watchListFile = fileName;
    m_hasUnsavedChanges = false;
  }
}

void MemWatchWidget::clearWatchList()
{
  if (!m_watchModel->hasAnyNodes())
    return;

  if (!warnIfUnsavedChanges())
    return;

  m_watchModel->clearRoot();
}

void MemWatchWidget::importFromCTFile()
{
  if (!warnIfUnsavedChanges())
    return;

  DlgImportCTFile* dlg = new DlgImportCTFile(this);
  if (dlg->exec() == QDialog::Accepted)
  {
    QFile* CTFile = new QFile(dlg->getFileName(), this);
    if (!CTFile->exists())
    {
      QMessageBox* errorBox =
          new QMessageBox(QMessageBox::Critical, QString(tr("Error while opening file")),
                          QString(tr("The cheat table file ") + CTFile->fileName() + tr(" does not exist")),
                          QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }
    if (!CTFile->open(QIODevice::ReadOnly))
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Critical, tr("Error while opening file"),
          tr("An error occured while opening the cheat table file for reading"), QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }

    bool useDolphinPointers = dlg->willUseDolphinPointers();
    MemWatchModel::CTParsingErrors parsingErrors;
    if (useDolphinPointers)
      parsingErrors = m_watchModel->importRootFromCTFile(CTFile, useDolphinPointers);
    else
      parsingErrors =
          m_watchModel->importRootFromCTFile(CTFile, useDolphinPointers, dlg->getCommonBase());
    CTFile->close();

    if (parsingErrors.errorStr.isEmpty())
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Information, tr("Import sucessfull"),
          tr("The Cheat Table was imported sucessfully without errors."), QMessageBox::Ok, this);
      errorBox->exec();
    }
    else if (parsingErrors.isCritical)
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Critical, tr("Import failed"),
          tr("The Cheat Table could not have been imported, here are the details of the error:\n\n") +
              parsingErrors.errorStr,
          QMessageBox::Ok, this);
      errorBox->exec();
    }
    else
    {
      QMessageBox* errorBox =
          new QMessageBox(QMessageBox::Warning, tr("Cheat table imported with errors"),
                          tr("The Cheat Table was imported with error(s), click \"Show Details...\" "
                          "to see the error(s) detail(s) (you can right-click to select all the "
                          "details and copy it to the clipboard)"),
                          QMessageBox::Ok, this);
      errorBox->setDetailedText(parsingErrors.errorStr);
      errorBox->exec();
    }
  }
}

void MemWatchWidget::exportWatchListAsCSV()
{
  QString fileName = QFileDialog::getSaveFileName(this, tr("Export Watch List"), m_watchListFile,
                                                  tr("Column separated values file (*.csv)"));
  if (fileName != tr(""))
  {
    if (!fileName.endsWith(tr(".csv")))
      fileName.append(tr(".csv"));
    QFile csvFile(fileName);
    if (!csvFile.open(QIODevice::WriteOnly))
    {
      QMessageBox* errorBox =
          new QMessageBox(QMessageBox::Critical, tr("Error while creating file"),
                          tr("An error occured while creating and opening the csv file for writting"),
                          QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }
    QTextStream outputStream(&csvFile);
    QString csvContents = m_watchModel->writeRootToCSVStringRecursive();
    outputStream << csvContents;
    csvFile.close();
  }
}

bool MemWatchWidget::warnIfUnsavedChanges()
{
  if (m_hasUnsavedChanges && m_watchModel->getRootNode()->hasChildren())
  {
    QMessageBox* questionBox = new QMessageBox(
        QMessageBox::Question, tr("Unsaved changes"),
        tr("You have unsaved changes in the current watch list, do you want to save them?"),
        QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes);
    switch (questionBox->exec())
    {
    case QMessageBox::Cancel:
      return false;
    case QMessageBox::No:
      return true;
    case QMessageBox::Yes:
      saveWatchFile();
      return true;
    }
  }
  return true;
}

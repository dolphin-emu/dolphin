#include "MemWatchModel.h"

#include <QDataStream>
#include <QMimeData>
#include <cstring>
#include <limits>
#include <sstream>

#include "../../../MemoryEngine/CheatEngineParser/CheatEngineParser.h"
#include "../GUICommon.h"

MemWatchModel::MemWatchModel(QObject* parent) : QAbstractItemModel(parent)
{
  m_rootNode = new MemWatchTreeNode(nullptr);
}

MemWatchModel::~MemWatchModel()
{
  delete m_rootNode;
}

void MemWatchModel::onUpdateTimer()
{
  if (!updateNodeValueRecursive(m_rootNode))
    emit readFailed();
}

void MemWatchModel::onFreezeTimer()
{
  if (!freezeNodeValueRecursive(m_rootNode))
    emit writeFailed(QModelIndex(), Common::MemOperationReturnCode::operationFailed);
}

bool MemWatchModel::updateNodeValueRecursive(MemWatchTreeNode* node, const QModelIndex& parent,
                                             bool readSucess)
{
  QVector<MemWatchTreeNode*> children = node->getChildren();
  if (children.count() > 0)
  {
    for (auto i : children)
    {
      QModelIndex theIndex = index(i->getRow(), WATCH_COL_VALUE, parent);
      readSucess = updateNodeValueRecursive(i, theIndex, readSucess);
      if (!readSucess)
        return false;
      if (!GUICommon::g_valueEditing && !i->isGroup())
        emit dataChanged(theIndex, theIndex);
    }
  }

  MemWatchEntry* entry = node->getEntry();
  if (entry != nullptr)
    if (entry->readMemoryFromRAM() == Common::MemOperationReturnCode::operationFailed)
      return false;
  return true;
}

bool MemWatchModel::freezeNodeValueRecursive(MemWatchTreeNode* node, const QModelIndex& parent,
                                             bool writeSucess)
{
  QVector<MemWatchTreeNode*> children = node->getChildren();
  if (children.count() > 0)
  {
    for (auto i : children)
    {
      QModelIndex theIndex = index(i->getRow(), WATCH_COL_VALUE, parent);
      writeSucess = freezeNodeValueRecursive(i, theIndex, writeSucess);
      if (!writeSucess)
        return false;
    }
  }

  MemWatchEntry* entry = node->getEntry();
  if (entry != nullptr)
  {
    if (entry->isLocked())
    {
      Common::MemOperationReturnCode writeReturn = entry->freeze();
      // Here we want to not care about invalid pointers, it won't write anyway
      if (writeReturn == Common::MemOperationReturnCode::OK ||
          writeReturn == Common::MemOperationReturnCode::invalidPointer)
        return true;
    }
  }
  return true;
}

void MemWatchModel::changeType(const QModelIndex& index, Common::MemType type, size_t length)
{
  MemWatchEntry* entry = getEntryFromIndex(index);
  entry->setTypeAndLength(type, length);
  emit dataChanged(index, index);
}

MemWatchEntry* MemWatchModel::getEntryFromIndex(const QModelIndex& index) const
{
  MemWatchTreeNode* node = static_cast<MemWatchTreeNode*>(index.internalPointer());
  return node->getEntry();
}

void MemWatchModel::addGroup(const QString& name)
{
  const QModelIndex rootIndex = index(0, 0);
  MemWatchTreeNode* node = new MemWatchTreeNode(nullptr, m_rootNode, true, name);
  beginInsertRows(rootIndex, rowCount(rootIndex), rowCount(rootIndex));
  m_rootNode->appendChild(node);
  endInsertRows();
  emit layoutChanged();
}

void MemWatchModel::addEntry(MemWatchEntry* entry)
{
  MemWatchTreeNode* newNode = new MemWatchTreeNode(entry);
  QModelIndex idx = index(0, 0);
  beginInsertRows(idx, rowCount(QModelIndex()), rowCount(QModelIndex()));
  m_rootNode->appendChild(newNode);
  endInsertRows();
  emit layoutChanged();
}

void MemWatchModel::editEntry(MemWatchEntry* entry, const QModelIndex& index)
{
  MemWatchTreeNode* node = static_cast<MemWatchTreeNode*>(index.internalPointer());
  node->setEntry(entry);
  emit layoutChanged();
}

void MemWatchModel::clearRoot()
{
  m_rootNode->clearAllChild();
  emit layoutChanged();
}

void MemWatchModel::removeNode(const QModelIndex& index)
{
  if (index.isValid())
  {
    MemWatchTreeNode* toDelete = static_cast<MemWatchTreeNode*>(index.internalPointer());
    MemWatchTreeNode* parent = toDelete->getParent();

    int toDeleteRow = toDelete->getRow();

    beginRemoveRows(index.parent(), toDeleteRow, toDeleteRow);
    bool removeChildren = (toDelete->isGroup() && toDelete->hasChildren());
    if (removeChildren)
      beginRemoveRows(index, 0, toDelete->childrenCount());
    toDelete->getParent()->removeChild(toDeleteRow);
    delete toDelete;
    if (removeChildren)
      endRemoveRows();
    endRemoveRows();
  }
}

int MemWatchModel::columnCount(const QModelIndex& parent) const
{
  return WATCH_COL_NUM;
}

int MemWatchModel::rowCount(const QModelIndex& parent) const
{
  MemWatchTreeNode* parentItem;
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
    parentItem = m_rootNode;
  else
    parentItem = static_cast<MemWatchTreeNode*>(parent.internalPointer());

  return parentItem->getChildren().size();
}

QVariant MemWatchModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  MemWatchTreeNode* item = static_cast<MemWatchTreeNode*>(index.internalPointer());

  if (!item->isGroup())
  {
    if (role == Qt::EditRole && index.column() == WATCH_COL_TYPE)
      return QVariant(static_cast<int>(item->getEntry()->getType()));

    MemWatchEntry* entry = item->getEntry();
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
      switch (index.column())
      {
      case WATCH_COL_LABEL:
      {
        return entry->getLabel();
      }
      case WATCH_COL_TYPE:
      {
        Common::MemType type = entry->getType();
        size_t length = entry->getLength();
        return GUICommon::getStringFromType(type, length);
      }
      case WATCH_COL_ADDRESS:
      {
        u32 address = entry->getConsoleAddress();
        bool isPointer = entry->isBoundToPointer();
        return getAddressString(address, isPointer);
      }
      case WATCH_COL_VALUE:
      {
        return QString::fromStdString(entry->getStringFromMemory());
      }
      }
    }
    else if (role == Qt::CheckStateRole && index.column() == WATCH_COL_LOCK)
    {
      if (entry->isLocked())
        return Qt::Checked;
      return Qt::Unchecked;
    }
  }
  else
  {
    if (index.column() == 0 && (role == Qt::DisplayRole || role == Qt::EditRole))
      return item->getGroupName();
  }
  return QVariant();
}

bool MemWatchModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (!index.isValid())
    return false;

  MemWatchTreeNode* node = static_cast<MemWatchTreeNode*>(index.internalPointer());
  if (node == m_rootNode)
    return false;

  if (!node->isGroup())
  {
    MemWatchEntry* entry = node->getEntry();
    if (role == Qt::EditRole)
    {
      switch (index.column())
      {
      case WATCH_COL_LABEL:
      {
        entry->setLabel(value.toString());
        emit dataChanged(index, index);
        return true;
      }
      case WATCH_COL_VALUE:
      {
        Common::MemOperationReturnCode returnCode =
            entry->writeMemoryFromString((value.toString().toStdString()));
        if (returnCode != Common::MemOperationReturnCode::OK)
        {
          emit writeFailed(index, returnCode);
          return false;
        }
        emit dataChanged(index, index);
        return true;
      }
      default:
      {
        return false;
      }
      }
    }
    else if (role == Qt::CheckStateRole && index.column() == WATCH_COL_LOCK)
    {
      value == Qt::Checked ? entry->setLock(true) : entry->setLock(false);
      emit dataChanged(index, index);
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
    node->setGroupName(value.toString());
    return true;
  }
}

Qt::ItemFlags MemWatchModel::flags(const QModelIndex& index) const
{
  if (!index.isValid())
    return Qt::ItemIsDropEnabled;

  MemWatchTreeNode* node = static_cast<MemWatchTreeNode*>(index.internalPointer());
  if (node == m_rootNode)
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;

  // These flags are common to every node
  Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;

  if (node->isGroup())
  {
    flags |= Qt::ItemIsDropEnabled;
    if (index.column() == WATCH_COL_LABEL)
      flags |= Qt::ItemIsEditable;
    return flags;
  }

  if (index.column() == WATCH_COL_LOCK)
    return flags |= Qt::ItemIsUserCheckable;

  if (index.column() != WATCH_COL_ADDRESS && index.column() != WATCH_COL_TYPE)
    flags |= Qt::ItemIsEditable;
  return flags;
}

QModelIndex MemWatchModel::index(int row, int column, const QModelIndex& parent) const
{
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  MemWatchTreeNode* parentNode;

  if (!parent.isValid())
    parentNode = m_rootNode;
  else
    parentNode = static_cast<MemWatchTreeNode*>(parent.internalPointer());

  MemWatchTreeNode* childNode = parentNode->getChildren().at(row);
  if (childNode)
    return createIndex(row, column, childNode);
  else
    return QModelIndex();
}

QModelIndex MemWatchModel::parent(const QModelIndex& index) const
{
  if (!index.isValid())
    return QModelIndex();

  MemWatchTreeNode* childNode = static_cast<MemWatchTreeNode*>(index.internalPointer());
  MemWatchTreeNode* parentNode = childNode->getParent();

  if (parentNode == m_rootNode || parentNode == nullptr)
    return QModelIndex();

  return createIndex(parentNode->getRow(), 0, parentNode);
}

QVariant MemWatchModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    switch (section)
    {
    case WATCH_COL_LABEL:
      return tr("Name");
    case WATCH_COL_TYPE:
      return tr("Type");
    case WATCH_COL_ADDRESS:
      return tr("Address");
    case WATCH_COL_VALUE:
      return tr("Value");
    case WATCH_COL_LOCK:
      return tr("Lock");
    }
  }
  return QVariant();
}

Qt::DropActions MemWatchModel::supportedDropActions() const
{
  return Qt::MoveAction;
}

Qt::DropActions MemWatchModel::supportedDragActions() const
{
  return Qt::MoveAction;
}

QStringList MemWatchModel::mimeTypes() const
{
  return QStringList() << tr("application/x-memwatchtreenode");
}

int MemWatchModel::getNodeDeepness(const MemWatchTreeNode* node) const
{
  if (node == m_rootNode)
    return 0;
  else if (node->getParent() == m_rootNode)
    return 1;
  else
    return getNodeDeepness(node->getParent()) + 1;
}

MemWatchTreeNode*
MemWatchModel::getLeastDeepNodeFromList(const QList<MemWatchTreeNode*> nodes) const
{
  int leastLevelFound = std::numeric_limits<int>::max();
  MemWatchTreeNode* returnNode = new MemWatchTreeNode(nullptr);
  for (auto i : nodes)
  {
    int deepness = getNodeDeepness(i);
    if (deepness < leastLevelFound)
    {
      returnNode = i;
      leastLevelFound = deepness;
    }
  }
  return returnNode;
}

QMimeData* MemWatchModel::mimeData(const QModelIndexList& indexes) const
{
  QMimeData* mimeData = new QMimeData;
  QByteArray data;

  QDataStream stream(&data, QIODevice::WriteOnly);
  QList<MemWatchTreeNode*> nodes;

  foreach (const QModelIndex& index, indexes)
  {
    MemWatchTreeNode* node = static_cast<MemWatchTreeNode*>(index.internalPointer());
    if (!nodes.contains(node))
      nodes << node;
  }
  qulonglong leastDeepPointer = 0;
  MemWatchTreeNode* leastDeepNode = getLeastDeepNodeFromList(nodes);
  std::memcpy(&leastDeepPointer, &leastDeepNode, sizeof(MemWatchTreeNode*));
  stream << leastDeepPointer;
  stream << nodes.count();
  foreach (MemWatchTreeNode* node, nodes)
  {
    qulonglong pointer = 0;
    std::memcpy(&pointer, &node, sizeof(node));
    stream << pointer;
  }
  mimeData->setData(tr("application/x-memwatchtreenode"), data);
  return mimeData;
}

bool MemWatchModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                 const QModelIndex& parent)
{
  if (!data->hasFormat(tr("application/x-memwatchtreenode")))
    return false;

  QByteArray bytes = data->data(tr("application/x-memwatchtreenode"));
  QDataStream stream(&bytes, QIODevice::ReadOnly);
  MemWatchTreeNode* destParentNode = nullptr;
  if (!parent.isValid())
    destParentNode = m_rootNode;
  else
    destParentNode = static_cast<MemWatchTreeNode*>(parent.internalPointer());

  qlonglong leastDeepNodePtr;
  stream >> leastDeepNodePtr;
  MemWatchTreeNode* leastDeepNode = nullptr;
  std::memcpy(&leastDeepNode, &leastDeepNodePtr, sizeof(leastDeepNodePtr));

  if (row == -1)
  {
    if (parent.isValid() && destParentNode->isGroup())
      row = rowCount(parent);
    else if (!parent.isValid())
      return false;
  }

  // beginMoveRows will cause a segfault if it ends up with doing nothing (so moving one row to the
  // same place), but there's also a discrepancy of 1 in the row received / the row given to
  // beginMoveRows and the actuall row of the node.  This discrepancy has to be reversed before
  // checking if we are trying to move the source (using the least deep one to accomodate
  // multi-select) to the same place.
  int trueRow = (leastDeepNode->getRow() < row) ? (row - 1) : (row);
  if (destParentNode == leastDeepNode->getParent() && leastDeepNode->getRow() == trueRow)
    return false;

  int count;
  stream >> count;

  for (int i = 0; i < count; ++i)
  {
    qlonglong nodePtr;
    stream >> nodePtr;
    MemWatchTreeNode* srcNode = nullptr;
    std::memcpy(&srcNode, &nodePtr, sizeof(nodePtr));

    // Since beginMoveRows uses the same row format then the one received, we want to keep that, but
    // still use the correct row number for inserting.
    int destMoveRow = row;
    if (srcNode->getRow() < row && destParentNode == srcNode->getParent())
      --row;

    const int srcNodeRow = srcNode->getRow();
    const QModelIndex idx = createIndex(srcNodeRow, 0, srcNode);

    // A move is imperative here to not have the view collapse the source on its own.
    beginMoveRows(idx.parent(), srcNodeRow, srcNodeRow, parent, destMoveRow);
    srcNode->getParent()->removeChild(srcNodeRow);
    destParentNode->insertChild(row, srcNode);
    endMoveRows();

    ++row;
  }
  emit dropSucceeded();
  return true;
}

void MemWatchModel::sort(int column, Qt::SortOrder order)
{
  sortRecursive(column, order, m_rootNode);
  emit layoutChanged();
}

void MemWatchModel::sortRecursive(int column, Qt::SortOrder order, MemWatchTreeNode* parent)
{
  if (!parent->hasChildren())
    return;

  QVector<MemWatchTreeNode*> children = parent->getChildren();

  switch (column)
  {
  case WATCH_COL_LABEL:
  {
    std::sort(
        children.begin(), children.end(), [=](MemWatchTreeNode* left, MemWatchTreeNode* right) {
          if (left->isGroup() && right->isGroup())
          {
            int compareResult =
                QString::compare(left->getGroupName(), right->getGroupName(), Qt::CaseInsensitive);
            return order == Qt::AscendingOrder ? compareResult < 0 : compareResult > 0;
          }
          else if (left->isGroup())
            return true;
          else if (right->isGroup())
            return false;

          int compareResult = QString::compare(left->getEntry()->getLabel(),
                                               right->getEntry()->getLabel(), Qt::CaseInsensitive);
          return order == Qt::AscendingOrder ? compareResult < 0 : compareResult > 0;
        });
    break;
  }
  case WATCH_COL_TYPE:
  {
    std::sort(children.begin(), children.end(),
              [=](MemWatchTreeNode* left, MemWatchTreeNode* right) {
                if (left->isGroup())
                  return true;
                else if (right->isGroup())
                  return false;

                int compareResult = static_cast<int>(left->getEntry()->getType()) -
                                    static_cast<int>(right->getEntry()->getType());
                return order == Qt::AscendingOrder ? compareResult < 0 : compareResult > 0;
              });
    break;
  }
  case WATCH_COL_ADDRESS:
  {
    std::sort(children.begin(), children.end(),
              [=](MemWatchTreeNode* left, MemWatchTreeNode* right) {
                if (left->isGroup())
                  return true;
                else if (right->isGroup())
                  return false;

                u32 leftAddress = left->getEntry()->getConsoleAddress();
                u32 rightAddress = right->getEntry()->getConsoleAddress();

                return order == Qt::AscendingOrder ? leftAddress < rightAddress
                                                   : leftAddress > rightAddress;
              });
    break;
  }
  case WATCH_COL_LOCK:
  {
    std::sort(children.begin(), children.end(),
              [=](MemWatchTreeNode* left, MemWatchTreeNode* right) {
                if (left->isGroup())
                  return true;
                else if (right->isGroup())
                  return false;

                bool lessThan = !left->getEntry()->isLocked() && right->getEntry()->isLocked();
                bool equal = left->getEntry()->isLocked() == right->getEntry()->isLocked();
                return order == Qt::AscendingOrder ? lessThan : !lessThan && !equal;
              });
    break;
  }
  case WATCH_COL_VALUE:
  {
    std::sort(
        children.begin(), children.end(), [=](MemWatchTreeNode* left, MemWatchTreeNode* right) {
          if (left->isGroup() && right->isGroup())
            return false;
          else if (left->isGroup())
            return true;
          else if (right->isGroup())
            return false;

          int compareResult =
              QString::compare(QString::fromStdString(left->getEntry()->getStringFromMemory()),
                               QString::fromStdString(right->getEntry()->getStringFromMemory()),
                               Qt::CaseInsensitive);
          return order == Qt::AscendingOrder ? compareResult < 0 : compareResult > 0;
        });
    break;
  }
  }

  parent->setChildren(children);

  for (auto i : parent->getChildren())
  {
    if (i->isGroup())
      sortRecursive(column, order, i);
  }
}

QString MemWatchModel::getAddressString(u32 address, bool isPointer) const
{
  std::stringstream ss;
  if (isPointer)
  {
    ss << "P->" << std::hex << std::uppercase << address;
    return QString::fromStdString(ss.str());
  }
  ss << std::hex << std::uppercase << address;
  return QString::fromStdString(ss.str());
}

void MemWatchModel::loadRootFromJsonRecursive(const QJsonObject& json)
{
  m_rootNode->readFromJson(json);
  emit layoutChanged();
}

MemWatchModel::CTParsingErrors
MemWatchModel::importRootFromCTFile(QFile* CTFile, const bool useDolphinPointer, const u64 CEStart)
{
  CheatEngineParser parser = CheatEngineParser();
  parser.setTableStartAddress(CEStart);
  MemWatchTreeNode* importedRoot = parser.parseCTFile(CTFile, useDolphinPointer);
  if (importedRoot != nullptr)
    m_rootNode = importedRoot;

  CTParsingErrors parsingErrors;
  parsingErrors.errorStr = parser.getErrorMessages();
  parsingErrors.isCritical = parser.hasACriticalErrorOccured();
  if (!parsingErrors.isCritical)
    emit layoutChanged();

  return parsingErrors;
}

void MemWatchModel::writeRootToJsonRecursive(QJsonObject& json) const
{
  m_rootNode->writeToJson(json);
}

QString MemWatchModel::writeRootToCSVStringRecursive() const
{
  QString csv = QString(tr("Name;Address[offsets] (in hex);Type\n"));
  csv.append(m_rootNode->writeAsCSV());
  if (csv.endsWith(tr("\n")))
    csv.remove(csv.length() - 1, 1);
  return csv;
}

bool MemWatchModel::hasAnyNodes() const
{
  return m_rootNode->hasChildren();
}

MemWatchTreeNode* MemWatchModel::getRootNode() const
{
  return m_rootNode;
}

MemWatchTreeNode* MemWatchModel::getTreeNodeFromIndex(const QModelIndex& index) const
{
  return static_cast<MemWatchTreeNode*>(index.internalPointer());
}

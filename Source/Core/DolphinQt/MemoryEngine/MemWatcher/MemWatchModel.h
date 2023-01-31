#pragma once

#include <QAbstractItemModel>
#include <QFile>
#include <QJsonObject>

#include "../../../MemoryEngine/MemoryWatch/MemWatchEntry.h"
#include "../../../MemoryEngine/MemoryWatch/MemWatchTreeNode.h"

class MemWatchModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  enum
  {
    WATCH_COL_LABEL = 0,
    WATCH_COL_TYPE,
    WATCH_COL_ADDRESS,
    WATCH_COL_LOCK,
    WATCH_COL_VALUE,
    WATCH_COL_NUM
  };

  struct CTParsingErrors
  {
    QString errorStr;
    bool isCritical;
  };

  MemWatchModel(QObject* parent);
  ~MemWatchModel();

  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  Qt::DropActions supportedDropActions() const override;
  Qt::DropActions supportedDragActions() const override;
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                    const QModelIndex& parent) override;

  void changeType(const QModelIndex& index, const Common::MemType type, const size_t length);
  MemWatchEntry* getEntryFromIndex(const QModelIndex& index) const;
  void addGroup(const QString& name);
  void addEntry(MemWatchEntry* entry);
  void editEntry(MemWatchEntry* entry, const QModelIndex& index);
  void sortRecursive(int column, Qt::SortOrder order, MemWatchTreeNode* parent);
  void clearRoot();
  void removeNode(const QModelIndex& index);
  void onUpdateTimer();
  void onFreezeTimer();
  void loadRootFromJsonRecursive(const QJsonObject& json);
  CTParsingErrors importRootFromCTFile(QFile* CTFile, const bool useDolphinPointer,
                                       const u64 CEStart = 0);
  void writeRootToJsonRecursive(QJsonObject& json) const;
  QString writeRootToCSVStringRecursive() const;
  bool hasAnyNodes() const;
  MemWatchTreeNode* getRootNode() const;
  MemWatchTreeNode* getTreeNodeFromIndex(const QModelIndex& index) const;
signals:
  void writeFailed(const QModelIndex& index, Common::MemOperationReturnCode writeReturn);
  void readFailed();
  void dropSucceeded();

private:
  bool updateNodeValueRecursive(MemWatchTreeNode* node, const QModelIndex& parent = QModelIndex(),
                                const bool readSucess = true);
  bool freezeNodeValueRecursive(MemWatchTreeNode* node, const QModelIndex& parent = QModelIndex(),
                                const bool writeSucess = true);
  QString getAddressString(u32 address, bool isPointer) const;
  MemWatchTreeNode* getLeastDeepNodeFromList(const QList<MemWatchTreeNode*> nodes) const;
  int getNodeDeepness(const MemWatchTreeNode* node) const;

  MemWatchTreeNode* m_rootNode;
};

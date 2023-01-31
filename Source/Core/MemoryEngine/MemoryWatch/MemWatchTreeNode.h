#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

#include "MemWatchEntry.h"

class MemWatchTreeNode
{
public:
  MemWatchTreeNode(MemWatchEntry* entry, MemWatchTreeNode* parent = nullptr,
                   const bool isGroup = false, const QString& groupName = QString());
  MemWatchTreeNode(const MemWatchTreeNode& node);
  ~MemWatchTreeNode();

  bool isGroup() const;
  QString getGroupName() const;
  void setGroupName(const QString& groupName);
  MemWatchEntry* getEntry() const;
  void setEntry(MemWatchEntry* entry);
  QVector<MemWatchTreeNode*> getChildren() const;
  void setChildren(QVector<MemWatchTreeNode*> children);
  MemWatchTreeNode* getParent() const;
  int getRow() const;
  bool isValueEditing() const;
  bool hasChildren() const;
  int childrenCount() const;
  void setValueEditing(const bool valueEditing);

  void appendChild(MemWatchTreeNode* node);
  void insertChild(const int row, MemWatchTreeNode* node);
  void removeChild(const int row);
  void clearAllChild();

  void readFromJson(const QJsonObject& json, MemWatchTreeNode* parent = nullptr);
  void writeToJson(QJsonObject& json) const;
  QString writeAsCSV() const;

private:
  bool m_isGroup;
  bool m_isValueEditing = false;
  QString m_groupName;
  MemWatchEntry* m_entry;
  QVector<MemWatchTreeNode*> m_children;
  MemWatchTreeNode* m_parent;
};

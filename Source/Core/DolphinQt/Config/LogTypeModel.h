// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QAbstractItemModel>

#include "Common/Assert.h"
#include "Common/Logging/LogManager.h"

class LogTypeModel : public QAbstractItemModel
{
public:
  LogTypeModel(QObject* parent = nullptr) : QAbstractItemModel(parent) {}

  int columnCount(const QModelIndex& parent) const override { return 4; }
  QVariant data(const QModelIndex& index, int role) const override
  {
    if (!index.isValid())
    {
      return QVariant();
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
    {
      return QVariant();
    }
    switch (index.column())
    {
    case 0:
      return QString::fromStdString(LogTypes::INFO[index.internalId()].long_name);
    case 1:
      return LogManager::GetInstance()->GetLogLevel(LogTypes::LOG_TYPE(index.internalId()));
    case 2:
      return index.internalId();
    case 3:
      return LogTypes::INFO[index.internalId()].end;
    default:
      return QVariant();
    }
  }
  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    if (!index.isValid())
    {
      return Qt::NoItemFlags;
    }
    if (index.column() == 0)
    {
      return LogTypes::INFO[index.internalId()].end ? Qt::ItemIsEnabled :
                                                      Qt::ItemNeverHasChildren | Qt::ItemIsEnabled;
    }
    else if (index.column() == 1)
    {
      return Qt::ItemNeverHasChildren | Qt::ItemIsEnabled;
    }
    else
    {
      return Qt::ItemNeverHasChildren;
    }
  }
  bool hasChildren(const QModelIndex& parent) const override
  {
    if (!parent.isValid())
    {
      return true;
    }
    if (parent.column() != 0)
    {
      return false;
    }
    return !LogTypes::IsLeaf(LogTypes::LOG_TYPE(parent.internalId()));
  }
  QModelIndex index(int row, int column, const QModelIndex& parent) const override
  {
    if (column < 0 || column > 3)
    {
      // have only 2 columns ⇒ invalid
      return QModelIndex();
    }
    if (!parent.isValid())
    {
      if (row != 0)
      {
        return QModelIndex();
      }
      return createIndex(0, column, quintptr(0));
    }
    auto par_type = LogTypes::LOG_TYPE(parent.internalId());
    ASSERT(LogTypes::IsValid(par_type));
    auto child_type = LogTypes::FirstChild(par_type);
    for (int count = 0; count < row; ++count)
    {
      ASSERT(LogTypes::IsValid(child_type));
      child_type = LogTypes::NextSibling(child_type);
    }
    return createIndex(row, column, quintptr(child_type));
  }
  QModelIndex parent(const QModelIndex& child) const override
  {
    if (!child.isValid() || child.internalId() == 0)
    {
      return QModelIndex();
    }
    auto par_type = LogTypes::Parent(LogTypes::LOG_TYPE(child.internalId()));
    if (IsValid(par_type))
    {
      auto grandpar = LogTypes::Parent(par_type);
      if (LogTypes::IsValid(grandpar))
      {
        return createIndex(int(grandpar) - 1 - int(par_type), 0, quintptr(par_type));
      }
      else
      {
        return createIndex(0, 0, quintptr(0));
      }
    }
    return QModelIndex();
  }
  int rowCount(const QModelIndex& parent) const override
  {
    if (!parent.isValid())
    {
      return 1;
    }
    return LogTypes::CountChildren(LogTypes::LOG_TYPE(parent.internalId()));
  }
  bool setData(const QModelIndex& index, const QVariant& value, int role) override
  {
    if (role != Qt::DisplayRole && role != Qt::EditRole)
    {
      return false;
    }
    if (!index.isValid() || index.column() != 1)
    {
      return false;
    }
    bool ok;
    int val = value.toInt(&ok);
    if (!ok)
    {
      return false;
    }
    if (val < 0 || val > MAX_LOGLEVEL)
    {
      return false;
    }
    LogManager::GetInstance()->SetLogLevel(LogTypes::LOG_TYPE(index.internalId()),
                                           LogTypes::LOG_LEVELS(val));
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
  }
};
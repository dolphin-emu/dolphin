// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "ScriptsListModel.h"

int ScriptsListModel::rowCount(const QModelIndex &parent) const
{
  return static_cast<int>(m_scripts.size());
}

int ScriptsListModel::columnCount(const QModelIndex& parent) const
{
  return 2;
}

QVariant ScriptsListModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || (size_t)index.row() >= m_scripts.size())
    return QVariant();

  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    if (index.column() == 0)
      return QVariant();
    else if (index.column() == 1)
      return QVariant(QString::fromStdString(m_scripts[index.row()].filename));
  }
  return QVariant();
}

QVariant ScriptsListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role != Qt::DisplayRole)
    return QVariant();

  if (orientation == Qt::Horizontal)
  {
    if (section == 0)
      // first column should get checkboxes for pausing scripts
      return QStringLiteral("");
    if (section == 1)
      return tr("filename");
  }
  return QVariant();
}

bool ScriptsListModel::insertRows(int position, int rows, const QModelIndex& parent)
{
  beginInsertRows(QModelIndex(), position, position+rows-1);
  endInsertRows();
  return true;
}

bool ScriptsListModel::removeRows(int position, int rows, const QModelIndex& parent)
{
  beginRemoveRows(QModelIndex(), position, position+rows-1);
  endRemoveRows();
  return true;
}

void ScriptsListModel::Add(std::string filename)
{
  m_scripts.emplace_back(Script{filename, Scripting::ScriptingBackend(filename)});
  this->insertRow(this->rowCount());
}

void ScriptsListModel::Reload(int index)
{
  std::string filename = m_scripts.at(index).filename;
  // need to first explicitly erase instead of simply replacing the existing backend,
  // because otherwise there would be 2 backends for a short period of time,
  // which is not supported when running in subinterpreter-less mode.
  m_scripts.erase(m_scripts.begin() + index);
  m_scripts.insert(m_scripts.begin() + index,
                   Script{filename, Scripting::ScriptingBackend(filename)});
}

void ScriptsListModel::Remove(int index)
{
  m_scripts.erase(m_scripts.begin() + index);
  this->removeRow(index);
}

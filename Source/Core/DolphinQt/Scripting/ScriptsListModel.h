// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QAbstractListModel>

#include "Scripting/ScriptingEngine.h"

struct Script
{
  std::string filename;
  Scripting::ScriptingBackend backend;
};

class ScriptsListModel : public QAbstractListModel
{
public:
  ScriptsListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  bool insertRows(int position, int rows, const QModelIndex &index = QModelIndex()) override;
  bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex()) override;

  void Add(std::string filename);
  void Reload(int index);
  void Remove(int index);

private:
  std::vector<Script> m_scripts;
};

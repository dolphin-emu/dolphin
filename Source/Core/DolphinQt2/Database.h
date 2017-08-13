// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSqlDatabase>
#include <QSqlRecord>

class Database
{
public:
  Database(const QString& db_file);

  bool Execute(const QString& query);
  bool InsertValues(const QString& table, QStringList values);
  QSqlQuery Fetch(const QString& query);
  QSqlRecord GetRow(const QString& row);

  static constexpr int GAMELIST_CACHE_VERSION = 0x00;

private:
  QSqlDatabase m_db;

  void Init();
};

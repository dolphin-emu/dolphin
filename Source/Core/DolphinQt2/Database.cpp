// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Database.h"

#include <QDebug>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>

Database::Database(const QString& db_file)
{
  m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));

  m_db.setHostName(QStringLiteral("localhost"));
  m_db.setDatabaseName(db_file);

  if (!m_db.open())
  {
    qDebug() << QStringLiteral("Failed to open Database!");
    return;
  }

  Init();

  auto cache_version =
      GetRow(QStringLiteral("SELECT value FROM db_meta WHERE key='cache_version';"));

  if (cache_version.isEmpty())
  {
    Execute(QStringLiteral("INSERT INTO db_meta(key,value) VALUES('cache_version', %1)")
                .arg(GAMELIST_CACHE_VERSION));
  }
  else
  {
    int db_version = cache_version.value(0).toInt();
    if (db_version < GAMELIST_CACHE_VERSION)
    {
      qDebug() << QStringLiteral("This database file is outdated. Recreating the cache tables...");
      Execute(QStringLiteral("DROP TABLE IF EXISTS gamefile_cache"));
      Execute(QStringLiteral("DROP TABLE IF EXISTS gamefile_cache_multilang"));
      Execute(QStringLiteral("UPDATE db_meta SET value=%1 WHERE key='cache_version'")
                  .arg(GAMELIST_CACHE_VERSION));
      Init();
    }
  }
}

void Database::Init()
{
  // TABLE db_meta
  Execute(
      QStringLiteral("CREATE TABLE IF NOT EXISTS db_meta(key TEXT PRIMARY KEY, value INTEGER)"));

  // TABLE gamefile_cache
  Execute(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gamefile_cache(path_hash TEXT PRIMARY KEY, "
      "game_id TEXT, title_id TEXT, maker TEXT, maker_id TEXT, name_internal TEXT, "
      "revision INTEGER, disc_number INTEGER, platform INTEGER, region INTEGER, country INTEGER, "
      "blob_type INTEGER, size INTEGER, apploader_data TEXT,  banner BLOB)"));

  // TABLE gamefile_cache_multilang
  Execute(
      QStringLiteral("CREATE TABLE IF NOT EXISTS gamefile_cache_multilang(path_hash TEXT, "
                     "language_id TEXT, description TEXT, name_short TEXT, name_long TEXT, "
                     "maker_short TEXT, maker_long TEXT, PRIMARY KEY(path_hash, language_id))"));
}

QSqlQuery Database::Fetch(const QString& query)
{
  QSqlQuery sql(m_db);
  sql.exec(query);
  return sql;
}

bool Database::Execute(const QString& query)
{
  QSqlQuery sql(m_db);

  bool success = sql.exec(query);

  if (!success)
  {
    qDebug() << QStringLiteral("Failed to execute query: ") << sql.lastError();
  }

  return success;
}

bool Database::InsertValues(const QString& table, QStringList values)
{
  for (auto& item : values)
  {
    item = QStringLiteral("'%1'").arg(item.replace(QStringLiteral("'"), QStringLiteral("''")));
  }
  QString query =
      QStringLiteral("INSERT INTO %1 VALUES(%2)").arg(table, values.join(QStringLiteral(", ")));

  return Execute(query);
}

QSqlRecord Database::GetRow(const QString& query)
{
  QSqlQuery sql = Fetch(query);

  sql.last();
  bool empty = sql.at() == QSql::AfterLastRow;
  sql.first();

  if (!empty)
    return sql.record();
  else
    return QSqlRecord();
}

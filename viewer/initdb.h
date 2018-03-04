#ifndef INITDB_H
#define INITDB_H

#include <QtSql>

QSqlError initDb(const QString& path)
{
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  db.setDatabaseName(path);
  if (!db.open())
  {
    return db.lastError();
  }
  return QSqlError();
}

#endif // INITDB_H

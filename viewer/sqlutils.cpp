#include "sqlutils.h"
#include <QThread>

QStringList SqlUtils::quoteStrings(const QStringList &list)
{
  QStringList newList(list);
  for(QString& item : newList)
  {
    item.prepend("\"");
    item.append("\"");
  }
  return newList;
}

QString SqlUtils::makeSqlStringObjects(const QStringList &list)
{
  return list.join(", ");
}

QString SqlUtils::makeSqlStringFunctions(const QStringList &list)
{
  auto quotedList = quoteStrings(list);
  return quotedList.join(" or name = ");
}

QString SqlUtils::makeSqlStringFunction(const QString &functionString)
{
  auto functionStringCpy(functionString);
  functionStringCpy.prepend("\"");
  functionStringCpy.append("\"");
  return functionStringCpy;
}

QString SqlUtils::makeIncludeNullStatement(const QStringList& items)
{
  QString nullClause = "";
  if(items.contains(""))
  {
     nullClause = "or allocation_id is NULL";
  }
  return nullClause;
}

QSqlDatabase SqlUtils::getThreadDbCon()
{
    auto defaultDb = QSqlDatabase::database("QSLITE");
    auto defaultName = defaultDb.databaseName();
    auto name = defaultName + QString::number((quint64)QThread::currentThread(), 16);
    if(QSqlDatabase::contains(name))
        return QSqlDatabase::database(name);
    else {
        auto db = QSqlDatabase::addDatabase( "QSQLITE", name);
        db.open();
        // open the database, setup tables, etc.
        return db;
    }
}

QVariant SqlUtils::executeSingleResultQuery(QSqlQuery& q)
{
  q.exec();
  if(q.next())
  {
    return q.value(0);
  }
  else
  {
    qDebug() << q.lastQuery();
    qDebug() << q.lastError().text();
    return QVariant();
  }
}

QString SqlUtils::executeSingleStringQuery(QSqlQuery& q)
{
  auto r = executeSingleResultQuery(q);
  if(r.isNull())
  {
    return "";
  }
  else
  {
    return r.toString();
  }
}

float SqlUtils::executeSingleFloatQuery(QSqlQuery& q)
{
  auto r = executeSingleResultQuery(q);
  if(r.isNull())
  {
    return 0;
  }
  else
  {
    return r.toFloat();
  }
}

unsigned int SqlUtils::executeSingleUnsignedIntQuery(QSqlQuery& q)
{
  auto r = executeSingleResultQuery(q);
  if(r.isNull())
  {
    return 0;
  }
  else
  {
    return r.toUInt();
  }
}

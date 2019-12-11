#ifndef SQLUTILS_H
#define SQLUTILS_H

#include <QStringList>
#include <QtSql>

class SqlUtils
{
public:
  static QStringList quoteStrings(const QStringList& list);
  static QString makeSqlStringObjects(const QStringList& list);
  static QString makeSqlStringFunctions(const QStringList& list);
  static QString makeSqlStringFunction(const QString& functionString);
  static QString makeIncludeNullStatement(const QStringList &items);
  static QSqlDatabase getThreadDbCon();
  static unsigned int executeSingleUnsignedIntQuery(QSqlQuery &q);
  static float executeSingleFloatQuery(QSqlQuery &q);
  static QString executeSingleStringQuery(QSqlQuery &q);
  static QVariant executeSingleResultQuery(QSqlQuery &q);
};

#endif // SQLUTILS_H

#ifndef SQLUTILS_H
#define SQLUTILS_H

#include <QStringList>

class SqlUtils
{
public:
  static QStringList quoteStrings(const QStringList& list);
  static QString makeSqlStringObjects(const QStringList& list);
  static QString makeSqlStringFunctions(const QStringList& list);
  static QString makeSqlStringFunction(const QString& functionString);
  static QString makeIncludeNullStatement(const QStringList &items);
};

#endif // SQLUTILS_H

#include "sqlutils.h"

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

QString SqlUtils::makeIncludeNullStatement(const QStringList& items)
{
  QString nullClause = "";
  if(items.contains(""))
  {
     nullClause = "or allocation_id is NULL";
  }
  return nullClause;
}

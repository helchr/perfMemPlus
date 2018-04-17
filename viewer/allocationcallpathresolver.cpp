#include "allocationcallpathresolver.h"
#include <QProcess>
#include <QSqlQuery>
#include <QTreeWidgetItem>

bool AllocationCallPathResolver::columnExists(const QString& tableName, const QString& columnName) const
{
  auto db = QSqlDatabase::database();
  static QMap<QString,bool> resultCache;
  if(resultCache.contains(db.databaseName()))
  {
    return resultCache[db.databaseName()];
  }
  else
  {
    QSqlQuery tableInfo("PRAGMA table_info(" + tableName + ")");
    tableInfo.exec();
    while(tableInfo.next())
    {
      if(tableInfo.value("name") == columnName)
      {
        resultCache.insert(db.databaseName(),true);
        return true;
      }
    }
    resultCache.insert(db.databaseName(),false);
    return false;
  }
}

template <class T>
void AllocationCallPathResolver::addCallstackEntry(long long id, T* parent) const
{
  QSqlQuery getCallstackEntry;
  if(columnExists("allocation_symbols","inlinedBy"))
  {
    getCallstackEntry.prepare("select parent_id, \
    (select short_name from dsos where id = (select dso_id from allocation_symbols where id = allocation_symbol_id)) as dso, \
    (select name from allocation_symbols where id = allocation_symbol_id) as name, \
    (select printf('%x',ip) from allocation_symbols where id = allocation_symbol_id) as address, \
    (select file from allocation_symbols where id = allocation_symbol_id) as file,\
    (select line from allocation_symbols where id = allocation_symbol_id) as line,\
    (select inlinedBy from allocation_symbols where id = allocation_symbol_id) as inlinedBy\
    from allocation_call_paths where id = ?");
  }
  else
  {
    getCallstackEntry.prepare("select parent_id, \
    (select short_name from dsos where id = (select dso_id from allocation_symbols where id = allocation_symbol_id)) as dso, \
    (select name from allocation_symbols where id = allocation_symbol_id) as name, \
    (select printf('%x',ip) from allocation_symbols where id = allocation_symbol_id) as address, \
    (select file from allocation_symbols where id = allocation_symbol_id) as file,\
    (select line from allocation_symbols where id = allocation_symbol_id) as line\
    from allocation_call_paths where id = ?");
  }
  getCallstackEntry.bindValue(0,id);
  getCallstackEntry.exec();
  if(getCallstackEntry.next())
    {
      long long parentId = getCallstackEntry.value(0).toLongLong();
      QString dso = getCallstackEntry.value(1).toString();
      QString function = getCallstackEntry.value(2).toString();
      QString ip = getCallstackEntry.value(3).toString();
      QString file = getCallstackEntry.value(4).toString();
      QString line = getCallstackEntry.value(5).toString();
      QString inlinedBy = getCallstackEntry.value(6).toString();
      auto item = new QTreeWidgetItem(parent);
      item->setText(0,dso);
      item->setText(1,function);
      item->setText(2,file + line + '\n' + inlinedBy);
      item->setExpanded(true);
      if(parentId != 0)
        {
          addCallstackEntry(parentId,item);
        }
    }
  else
    {
      return;
    }
}

void AllocationCallPathResolver::writeAllocationCallpathFromCallpathId(const int callpathId, QTreeWidget* treeWidget) const
{
   addCallstackEntry(callpathId,treeWidget);
}

void AllocationCallPathResolver::writeAllocationCallpath(const int allocationId, QTreeWidget* treeWidget) const
{
  QSqlQuery getCallpathId;
  getCallpathId.prepare("select call_path_id from allocations where id = ?");
  getCallpathId.bindValue(0,allocationId);
  getCallpathId.exec();
  long long callPathId = -1;
  if(getCallpathId.next())
    {
      callPathId = getCallpathId.value(0).toLongLong();
      addCallstackEntry(callPathId,treeWidget);
  }
}

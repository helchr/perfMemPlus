#include "allocationcallpathresolver.h"
#include <QProcess>
#include <QSqlQuery>
#include <QTreeWidgetItem>


AllocationCallPathResolver::AllocationCallPathResolver(const QString &path)
{
  exePath = path;
}

void AllocationCallPathResolver::setExe(const QString& path)
{
  exePath = path;
}

QString AllocationCallPathResolver::addr2line(const QString& exe, const QString& address) const
{
  QProcess process;
  QString file = QString("sh -c \"addr2line -p -f -C -i -e  ") + exe + " " + "0x" + address + "\"";
  process.start(file);
  process.closeWriteChannel();
  if(process.waitForFinished(100))
  {
    auto result = process.readAll();
    return QString(result);
  }
  return QString("");
}

template <class T>
void AllocationCallPathResolver::addCallstackEntry(long long id, T* parent) const
{
  QSqlQuery getCallstackEntry;
  getCallstackEntry.prepare("select parent_id, \
  (select short_name from dsos where id = (select dso_id from allocation_symbols where id = allocation_symbol_id)) as dso, \
  (select name from allocation_symbols where id = allocation_symbol_id) as name, \
  (select printf('%x',ip) from allocation_symbols where id = allocation_symbol_id) as address \
  from allocation_call_paths where id = ?");
  getCallstackEntry.bindValue(0,id);
  getCallstackEntry.exec();
  if(getCallstackEntry.next())
  {
    long long parentId = getCallstackEntry.value(0).toLongLong();
    QString dso = getCallstackEntry.value(1).toString();
    QString function = getCallstackEntry.value(2).toString();
    QString ip = getCallstackEntry.value(3).toString();
    auto item = new QTreeWidgetItem(parent);
    item->setText(0,dso);
    auto addr2lineResult = addr2line(exePath,ip);
    QStringList addr2lineParts = addr2lineResult.split(" at ",QString::SkipEmptyParts);
    if(!addr2lineParts.empty() && addr2lineParts.first() != "" && addr2lineParts.first() != "?? ??:0\n")
    {
      //addr2line can get useful data
      item->setText(1,addr2lineParts.first());
      addr2lineParts.removeFirst();
      item->setText(2,addr2lineParts.join(" "));
    }
    else
    {
      //addr2line can not get any useful data
      item->setText(1,function);
    }
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


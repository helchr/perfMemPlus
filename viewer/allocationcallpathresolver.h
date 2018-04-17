#ifndef ALLOCATIONCALLPATHRESOLVER_H
#define ALLOCATIONCALLPATHRESOLVER_H

#include <QString>
#include <QTreeWidget>

class AllocationCallPathResolver
{
public:
  AllocationCallPathResolver() = default;
  void writeAllocationCallpath(const int allocationId, QTreeWidget *treeWidget) const;
  void writeAllocationCallpathFromCallpathId(const int callpathId, QTreeWidget *treeWidget) const;
private:
  template<class T>
  void addCallstackEntry(long long id, T *parent) const;
  bool columnExists(const QString &tableName, const QString &columnName) const;
};

#endif // ALLOCATIONCALLPATHRESOLVER_H

#ifndef ALLOCATIONCALLPATHRESOLVER_H
#define ALLOCATIONCALLPATHRESOLVER_H

#include <QString>
#include <QTreeWidget>

class AllocationCallPathResolver
{
public:
  AllocationCallPathResolver(const QString& path = "");

  void setExe(const QString& path);
  void writeAllocationCallpath(const int allocationId, QTreeWidget *treeWidget) const;
  void writeAllocationCallpathFromCallpathId(const int callpathId, QTreeWidget *treeWidget) const;

private:
  QString exePath = "";
  QString addr2line(const QString &exe, const QString &address) const;
  template<class T>
  void addCallstackEntry(long long id, T *parent) const;
};

#endif // ALLOCATIONCALLPATHRESOLVER_H

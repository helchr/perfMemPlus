#ifndef GUIUTILS_H
#define GUIUTILS_H

class QTableView;
class QTreeWidget;
class QTreeView;

class GuiUtils
{
private:
  static int getColumnCount(QTableView *tv);
  static int getColumnCount(QTreeWidget *tv);
  static int getColumnCount(QTreeView *tv);

public:
  GuiUtils() = default;

  template <class T>
  static void resizeColumnsToContents(T* widget, const int columnSizeLimit = 600)
  {
    for(int i = 0; i < getColumnCount(widget); i++)
    {
      widget->resizeColumnToContents(i);
      if(widget->columnWidth(i)> columnSizeLimit)
      {
        widget->setColumnWidth(i,columnSizeLimit);
      }
    }
  }

};

#endif // GUIUTILS_H

#ifndef GUIUTILS_H
#define GUIUTILS_H

class QTableView;
class QTreeWidget;

class GuiUtils
{
private:
  static int getColumnCount(QTableView *tv);
  static int getColumnCount(QTreeWidget *tv);

public:
  GuiUtils() = default;

  template <class T>
  static void resizeColumnsToContents(T* widget)
  {
    const static int COLUMN_SIZE_LIMIT = 600;

    for(int i = 0; i < getColumnCount(widget); i++)
    {
      widget->resizeColumnToContents(i);
      if(widget->columnWidth(i)> COLUMN_SIZE_LIMIT)
      {
        widget->setColumnWidth(i,COLUMN_SIZE_LIMIT);
      }
    }
  }

};

#endif // GUIUTILS_H

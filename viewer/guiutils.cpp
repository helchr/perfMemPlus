#include "guiutils.h"
#include <QTableView>
#include <QTreeWidget>

int GuiUtils::getColumnCount(QTableView* tv)
{
  return tv->model()->columnCount();
}

int GuiUtils::getColumnCount(QTreeWidget* tv)
{
  return tv->columnCount();
}

int GuiUtils::getColumnCount(QTreeView* tv)
{
  return tv->model()->columnCount();
}


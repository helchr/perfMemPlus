#include "objectaccessedbyfunctionwindow.h"
#include "ui_objectaccessedbyfunctionwindow.h"
#include "percentdelegate.h"
#include "allocationcallpathresolver.h"
#include "memorylevelwindow.h"
#include "pdfwriter.h"
#include "sqlutils.h"
#include "guiutils.h"

ObjectAccessedByFunctionWindow::ObjectAccessedByFunctionWindow(QStringList items, const QString& dbPath, QWidget *parent) :
QDialog(parent),
ui(new Ui::ObjectAccessedByFunctionWindow)
{
  this->setWindowFlags(Qt::Window);
  ui->setupUi(this);
  this->setWindowTitle(this->windowTitle() + " - " + dbPath);
  this->dbPath = dbPath;

  this->items = items;
  ui->selectedItemsListWidget->addItems(items);
  model = new QSqlQueryModel(this);
  ui->label->setText("Selected\nfunctions");
  auto sqlItems = SqlUtils::makeSqlStringFunctions(items);

  model->setQuery("select allocation_id, 100 * count (*) / \
  (select count(*) from samples where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id in (select id from symbols where name = " + sqlItems + " ) ), \
  avg(weight) as \"average latency\", \
  100 * sum(weight) / (select sum(weight) from samples where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id in (select id from symbols where name = " + sqlItems + " ) ) \
  from samples where \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id in (select id from symbols where name = " + sqlItems + " ) \
  group by allocation_id");
  if (model->lastError().isValid())
  {
    qDebug() << model->lastError();
    return;
  }
  model->setHeaderData(0,Qt::Horizontal, "Object");
  model->setHeaderData(1,Qt::Horizontal, "Samples %");
  model->setHeaderData(2,Qt::Horizontal, "Average Latency");
  model->setHeaderData(3,Qt::Horizontal, "Latency %");
  ui->objectsTableView->setModel(model);
  ui->objectsTableView->setItemDelegateForColumn(3,new PercentDelegate(this));
  ui->objectsTableView->setItemDelegateForColumn(1,new PercentDelegate(this));
  ui->objectsTableView->resizeColumnsToContents();
  ui->objectsTableView->show();
  connect(ui->objectsTableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection ,QItemSelection)),
  this, SLOT(objectsTableRowChanged()));
}

void ObjectAccessedByFunctionWindow::setExePath(const QString &path)
{
  exePath = path;
}

ObjectAccessedByFunctionWindow::~ObjectAccessedByFunctionWindow()
{
  delete ui;
}

void ObjectAccessedByFunctionWindow::objectsTableRowChanged()
{
  auto selectedRows = ui->objectsTableView->selectionModel()->selectedRows(0);
  if(selectedRows.count() == 1)
  {
    auto id = selectedRows.first().data(Qt::DisplayRole).toInt();
    AllocationCallPathResolver acpr(exePath);
    ui->callPathTreeWidget->clear();
    acpr.writeAllocationCallpath(id,ui->callPathTreeWidget);
    GuiUtils::resizeColumnsToContents(ui->callPathTreeWidget);
  }
  else
  {
    ui->callPathTreeWidget->clear();
  }
}

void ObjectAccessedByFunctionWindow::on_showCacheUsagePushButton_clicked()
{
  auto indexList = ui->objectsTableView->selectionModel()->selectedRows(0);
  if(indexList.empty() == false)
    {
      QStringList sl;
      for(auto item : indexList)
        {
          sl.push_back(item.data(Qt::DisplayRole).toString());
        }
      MemoryLevelWindow* mlw = new MemoryLevelWindow(items,sl,dbPath,this);
      mlw->show();
    }
}

void ObjectAccessedByFunctionWindow::on_exportToPdfPushButton_clicked()
{
   PdfWriter p(dbPath);
   p.writeTableToPdf(ui->objectsTableView);
}

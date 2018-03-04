#include "memorylevelwindow.h"
#include "ui_memorylevelwindow.h"
#include "percentdelegate.h"
#include "pdfwriter.h"
#include "sqlutils.h"
#include "guiutils.h"

MemoryLevelWindow::MemoryLevelWindow(QWidget *parent) :
QDialog(parent),
ui(new Ui::MemoryLevelWindow)
{
  this->setWindowFlags(Qt::Window);
  ui->setupUi(this);

  selectAll.prepare("select (select name from memory_levels where id = memory_level) as lvl, count (*) as \"count\", \
  avg(weight) as \"average latency\", \
  count(*) * 100 / (select count(*) from samples where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") ) as \"hit rate %\" \
  from samples \
  where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  group by lvl order by count desc");
  QSqlQueryModel* model = new QSqlQueryModel(this);
  model->setQuery(selectAll);
  model->setHeaderData(0,Qt::Horizontal, "Memory Level");
  model->setHeaderData(1,Qt::Horizontal, "Count");
  model->setHeaderData(2,Qt::Horizontal, "Average Latency");
  model->setHeaderData(3,Qt::Horizontal, "Hit Rate");
  ui->memoryLevelTableView->setModel(model);
  ui->memoryLevelTableView->show();

}

MemoryLevelWindow::MemoryLevelWindow(QStringList items, const MemoryLevelWindow::LimitType fo, const QString& dbPath, QWidget* parent) :
QDialog(parent),
ui(new Ui::MemoryLevelWindow)
{
  this->setWindowFlags(Qt::Window);
  ui->setupUi(this);
  this->setWindowTitle(this->windowTitle() + " - " + dbPath);

  this->functionsObjects = fo;
  ui->selectedItemsListWidget->addItems(items);
  QSqlQueryModel* model = new QSqlQueryModel;
  if(fo == Functions)
  {
    this->functionItems = items;
    ui->label->setText("Selected\nfunctions");
    auto sqlItems = SqlUtils::makeSqlStringFunctions(items);
    model->setQuery("select (select name from memory_levels where id = memory_level) as lvl, count (*) as \"count\", \
    avg(weight) as \"average latency\", \
    count(*) * 100 / (select count(*) from samples where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
    and symbol_id in (select id from symbols where name = " + sqlItems + " ) )as \"hit rate %\" \
    from samples \
    where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
    and symbol_id in (select id from symbols where name = " + sqlItems + " ) \
    group by lvl order by memory_level asc");
    printRemoteMemoryAccessFunctions(items);
    printRemoteMemoryAccessFunctionsInclCache(items);
  }
  else if(fo == Objects)
  {
    this->objectItems = items;
    ui->label->setText("Selected\nobjects");
    auto sqlItems = SqlUtils::makeSqlStringObjects(items);
    auto inclNull = SqlUtils::makeIncludeNullStatement(items);
    model->setQuery("select (select name from memory_levels where id = memory_level) as lvl, count (*) as \"count\", \
    avg(weight) as \"average latency\", \
    count(*) * 100 / (select count(*) from samples where (allocation_id in (" + sqlItems + ") " + inclNull + ") and \
    evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") ) as \"hit rate %\" from samples \
    where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
    and (allocation_id in (" + sqlItems + ") " + inclNull + ") group by lvl order by memory_level asc");

    printRemoteMemoryAccessObjects(items);
    printRemoteMemoryAccessObjectsInclCache(items);
  }
  if (model->lastError().isValid())
  {
    qDebug() << model->lastError();
  }
  model->setHeaderData(0,Qt::Horizontal, "Memory Level");
  model->setHeaderData(1,Qt::Horizontal, "Count");
  model->setHeaderData(2,Qt::Horizontal, "Average Latency");
  model->setHeaderData(3,Qt::Horizontal, "Hit Rate");
  ui->memoryLevelTableView->setModel(model);
  ui->memoryLevelTableView->setItemDelegateForColumn(3,new PercentDelegate(ui->memoryLevelTableView));
  GuiUtils::resizeColumnsToContents(ui->memoryLevelTableView);

  ui->memoryLevelTableView->show();
}

MemoryLevelWindow::MemoryLevelWindow(QStringList functionItems, const QStringList& objectItems, const QString& dbPath, QWidget *parent) :
QDialog(parent),
ui(new Ui::MemoryLevelWindow)
{
  this->setWindowFlags(Qt::Window);
  ui->setupUi(this);
  this->setWindowTitle(this->windowTitle() + " - " + dbPath);

  this->functionItems = functionItems;
  this->objectItems = objectItems;
  ui->selectedItemsListWidget->addItems(functionItems);
  ui->selectedItemsListWidget->addItems(objectItems);
  QSqlQueryModel* model = new QSqlQueryModel(this);

  ui->label->setText("Selected\nfunctions\nand\nobjects");

  auto inclNull = SqlUtils::makeIncludeNullStatement(objectItems);
  auto sqlFunctionItems = SqlUtils::makeSqlStringFunctions(functionItems);
  auto sqlObjectItems = SqlUtils::makeSqlStringObjects(objectItems);

  model->setQuery("select (select name from memory_levels where id = memory_level) as lvl, count (*) as \"count\", \
  avg(weight) as \"average latency\", \
  count(*) * 100 / (select count(*) from samples where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id in (select id from symbols where name = " + sqlFunctionItems + " ) and \
  (allocation_id in (" + sqlObjectItems + ") " + inclNull + ") )as \"hit rate %\" \
  from samples \
  where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id in (select id from symbols where name = " + sqlFunctionItems + " ) and \
  (allocation_id in (" + sqlObjectItems + ") " + inclNull + ") \
  group by lvl order by memory_level asc");

  if (model->lastError().isValid())
  {
  qDebug() << model->lastError();
  }
  model->setHeaderData(0,Qt::Horizontal, "Memory Level");
  model->setHeaderData(1,Qt::Horizontal, "Count");
  model->setHeaderData(2,Qt::Horizontal, "Average Latency");
  model->setHeaderData(3,Qt::Horizontal, "Hit Rate");
  ui->memoryLevelTableView->setModel(model);
  ui->memoryLevelTableView->setItemDelegateForColumn(3,new PercentDelegate(ui->memoryLevelTableView));
  GuiUtils::resizeColumnsToContents(ui->memoryLevelTableView);

  printRemoteMemoryAccessFunctionsObjects(functionItems,objectItems);
  printRemoteMemoryAccessFunctionsObjectsInclCache(functionItems,objectItems);

  ui->memoryLevelTableView->show();
}

MemoryLevelWindow::~MemoryLevelWindow()
{
  delete ui;
}

void MemoryLevelWindow::printRemoteMemoryAccessFunctions(const QStringList& items)
{
  auto sqlItems = SqlUtils::makeSqlStringFunctions(items);
  auto result = executeSingleResultQuery("select 100 * numRemote / (numLocal + numRemote) as remote from ( \
  select ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level = (select id from memory_levels where name like \"Local DRAM%\") and \
  symbol_id in (select id from symbols where name = " + sqlItems + " \
  ) ) as numLocal, \
  ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level = (select id from memory_levels where name like \"Remote DRAM%\") and \
  symbol_id in (select id from symbols where name = " + sqlItems + " \
  ) ) as numRemote )");
  ui->remoteAccessNumberLabel->setText(result.toString() + "%");
}

void MemoryLevelWindow::printRemoteMemoryAccessFunctionsInclCache(const QStringList& items)
{
  auto sqlItems = SqlUtils::makeSqlStringFunctions(items);
  auto result = executeSingleResultQuery("select 100 * numRemote / (numLocal + numRemote) as remote from ( \
  select ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level in (select id from memory_levels where name like \"Local DRAM%\" or name like \"L3\") and \
  symbol_id in (select id from symbols where name = " + sqlItems + " \
  ) ) as numLocal, \
  ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level in (select id from memory_levels where name like \"Remote DRAM%\" or name like \"Remote Cache%\") and \
  symbol_id in (select id from symbols where name = " + sqlItems + " \
  ) ) as numRemote )");
  ui->remoteAccessInclCacheNumberLabel->setText(result.toString() + "%");
}

void MemoryLevelWindow::printRemoteMemoryAccessObjects(QStringList items)
{
  auto sqlItems = SqlUtils::makeSqlStringObjects(items);
  auto inclNull = SqlUtils::makeIncludeNullStatement(items);
  auto result = executeSingleResultQuery("select 100 * numRemote / (numLocal + numRemote) as remote from ( \
  select ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level = (select id from memory_levels where name like \"Local DRAM%\") and \
  (allocation_id in (" + sqlItems + ") " + inclNull + ") \
  ) as numLocal, \
  ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level = (select id from memory_levels where name like \"Remote DRAM%\") and \
  (allocation_id in (" + sqlItems + ") " + inclNull + ") \
  ) as numRemote \
  )");
  ui->remoteAccessNumberLabel->setText(result.toString() + "%");
}


void MemoryLevelWindow::printRemoteMemoryAccessObjectsInclCache(QStringList items)
{
  auto sqlItems = SqlUtils::makeSqlStringObjects(items);
  auto inclNull = SqlUtils::makeIncludeNullStatement(items);
  auto result = executeSingleResultQuery("select 100 * numRemote / (numLocal + numRemote) as remote from ( \
  select ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level in (select id from memory_levels where name like \"Local DRAM%\" or name like \"L3\") and \
  (allocation_id in (" + sqlItems + ") " + inclNull + ") \
  ) as numLocal, \
  ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level in (select id from memory_levels where name like \"Remote DRAM%\" or name like \"Remote Cache%\") and \
  (allocation_id in (" + sqlItems + ") " + inclNull + ") \
  ) as numRemote \
  )");
  ui->remoteAccessInclCacheNumberLabel->setText(result.toString() + "%");
}

void MemoryLevelWindow::printRemoteMemoryAccessFunctionsObjects(const QStringList& functionItems, QStringList objectItems)
{
  auto sqlObjectItems = SqlUtils::makeSqlStringObjects(objectItems);
  auto inclNull = SqlUtils::makeIncludeNullStatement(objectItems);
  auto sqlFunctionItems = SqlUtils::makeSqlStringFunctions(functionItems);
  auto result = executeSingleResultQuery("select 100 * numRemote / (numLocal + numRemote) as remote from ( \
  select ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level = (select id from memory_levels where name like \"Local DRAM%\") and \
  (allocation_id in (" + sqlObjectItems + ") " + inclNull + ") and \
  symbol_id in (select id from symbols where name = " + sqlFunctionItems + ") \
  ) as numLocal, \
  ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level = (select id from memory_levels where name like \"Remote DRAM%\") and \
  (allocation_id in (" + sqlObjectItems + ") " + inclNull + ") and \
  symbol_id in (select id from symbols where name = " + sqlFunctionItems + ") \
  ) as numRemote \
  )");
  ui->remoteAccessNumberLabel->setText(result.toString() + "%");
}

void MemoryLevelWindow::printRemoteMemoryAccessFunctionsObjectsInclCache(const QStringList& functionItems, QStringList objectItems)
{
  auto sqlObjectItems = SqlUtils::makeSqlStringObjects(objectItems);
  auto inclNull = SqlUtils::makeIncludeNullStatement(objectItems);
  auto sqlFunctionItems = SqlUtils::makeSqlStringFunctions(functionItems);
  auto result = executeSingleResultQuery("select 100 * numRemote / (numLocal + numRemote) as remote from ( \
  select ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level = (select id from memory_levels where name like \"Local DRAM%\" or name like \"L3%\") and \
  (allocation_id in (" + sqlObjectItems + ") " + inclNull + ") and \
  symbol_id in (select id from symbols where name = " + sqlFunctionItems + ") \
  ) as numLocal, \
  ( \
  select count(*) from samples where evsel_id = (select id from selected_events where name like \"%load%\")  and \
  memory_level = (select id from memory_levels where name like \"Remote DRAM%\" or name like \"Remote Cache%\") and \
  (allocation_id in (" + sqlObjectItems + ") " + inclNull + ") and \
  symbol_id in (select id from symbols where name = " + sqlFunctionItems + ") \
  ) as numRemote \
  )");
  ui->remoteAccessInclCacheNumberLabel->setText(result.toString() + "%");
}

QVariant MemoryLevelWindow::executeSingleResultQuery(const QString& query) const
{
 QSqlQuery q(query);
 q.exec();
 if(q.lastError().isValid())
 {
   qDebug() << query << q.lastError();
 }
 if(q.next())
 {
   auto result = q.value(0);
   if(!result.isNull())
   {
     return result;
   }
 }
 return QVariant((float)0);
}

void MemoryLevelWindow::on_exportToPdfPushButton_clicked()
{
   PdfWriter p;
   p.writeTableToPdf(ui->memoryLevelTableView);
}



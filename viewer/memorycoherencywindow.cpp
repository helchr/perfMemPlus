#include "memorycoherencywindow.h"
#include "ui_memorycoherencywindow.h"
#include "pdfwriter.h"
#include "sqlutils.h"
#include "guiutils.h"

MemoryCoherencyWindow::~MemoryCoherencyWindow()
{
  delete ui;
}

MemoryCoherencyWindow::MemoryCoherencyWindow(const QString& dbPath, QWidget* parent) :
QDialog(parent),
ui(new Ui::MemoryCoherencyWindow)
{
  this->setWindowFlags(Qt::Window);
  ui->setupUi(this);
  this->setWindowTitle(this->windowTitle() + " - " + dbPath);
  model = new QSqlQueryModel(this);
}

void MemoryCoherencyWindow::setObjects(const QStringList& items)
{
  this->items = items;
  auto sqlItems = SqlUtils::makeSqlStringObjects(items);
  model->setQuery("select \
  allocation_id, \
  coalesce(\"HITM count\",0) as \"HITM count\", \
  \"average latency\", \
  \"HITM average latency\" from \
  (select allocation_id, \
  avg(weight) as \"average latency\" \
  from samples \
  where allocation_id in (" + sqlItems + " ) \
  group by allocation_id) a \
  left outer join \
  (select allocation_id, \
  count (*) as \"HITM count\", \
  avg(weight) as \"HITM average latency\" \
  from samples \
  where allocation_id in (" + sqlItems + " ) \
  and memory_snoop = (select id from memory_snoop where name = \"Snoop Hit Modified\") \
  group by allocation_id) b using(allocation_id) \
  order by \"HITM count\" desc");

  if (model->lastError().isValid())
  {
    qDebug() << model->lastError();
  }
  model->setHeaderData(0,Qt::Horizontal, "Allocation Id");
  model->setHeaderData(1,Qt::Horizontal, "HITM Count");
  model->setHeaderData(2,Qt::Horizontal, "Average Latency");
  model->setHeaderData(3,Qt::Horizontal, "HITM Average Latency");
  ui->tableView->setModel(model);
  GuiUtils::resizeColumnsToContents(ui->tableView);
  ui->tableView->show();
}

void MemoryCoherencyWindow::setFunctions(QStringList items)
{
  auto sqlItems = SqlUtils::makeSqlStringFunctions(items);

  model->setQuery("select \
  (select name from symbols where id = symbol_id) as \"function\", \
  coalesce(\"HITM count\",0) as \"HITM count\", \
  \"average latency\", \
  \"HITM average latency\" from \
  (select symbol_id, \
  avg(weight) as \"average latency\" \
  from samples \
  where symbol_id in (select id from symbols where name = " + sqlItems + " ) \
  group by symbol_id) a \
  left outer join \
  (select symbol_id, \
  count (*) as \"HITM count\", \
  avg(weight) as \"HITM average latency\" \
  from samples \
  where symbol_id in (select id from symbols where name = " + sqlItems + " ) \
  and memory_snoop = (select id from memory_snoop where name = \"Snoop Hit Modified\") \
  group by symbol_id) b using(symbol_id) \
  order by \"HITM count\" desc");

  if (model->lastError().isValid())
  {
    qDebug() << model->lastError();
  }
  model->setHeaderData(0,Qt::Horizontal, "Function");
  model->setHeaderData(1,Qt::Horizontal, "HITM Count");
  model->setHeaderData(2,Qt::Horizontal, "Average Latency");
  model->setHeaderData(3,Qt::Horizontal, "HITM Average Latency");
  ui->tableView->setModel(model);
  GuiUtils::resizeColumnsToContents(ui->tableView);
  ui->tableView->show();
}

void MemoryCoherencyWindow::on_exportToPdfPushButton_clicked()
{
   PdfWriter p;
   p.writeTableToPdf(ui->tableView);
}

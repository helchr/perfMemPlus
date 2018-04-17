#include "graphwindow.h"
#include "ui_graphwindow.h"
#include <QtSql>
#include "pdfwriter.h"
#include "sqlutils.h"

QT_CHARTS_USE_NAMESPACE

GraphWindow::GraphWindow(const QString& dbPath, QWidget *parent)  :
QDialog(parent),
ui(new Ui::GraphWindow)
{
  this->setWindowFlags(Qt::Window);
  ui->setupUi(this);
  this->setWindowTitle(this->windowTitle() + " - " + dbPath);
  chart = new QChart();
  chartView = new QChartView(chart,this);
  ui->centralLayout->addWidget(chartView);
  axisY = new QValueAxis(chartView);
  axisX = new QValueAxis(chartView);
  series = new QScatterSeries(chart);
  series->setUseOpenGL(true);
}

GraphWindow::~GraphWindow()
{
  delete ui;
}

void GraphWindow::populateComboBoxWithThreads()
{
  QSqlQuery getThreadIdsQuery;
  if(!functions.empty() && !allocations.empty())
  {
    auto sqlFunctions = SqlUtils::makeSqlStringFunctions(functions);
    auto sqlAllocations = SqlUtils::makeSqlStringObjects(allocations);
    getThreadIdsQuery.prepare("select (select tid from threads where id = thread_id) as \"tid\" from samples where \
    symbol_id in (select id from symbols where name = " + sqlFunctions + " ) and \
    allocation_id in ( " + sqlAllocations + ") order by tid asc");
  }
  else if(!functions.empty() && allocations.empty())
  {
    auto sqlFunctions = SqlUtils::makeSqlStringFunctions(functions);
    getThreadIdsQuery.prepare("select (select tid from threads where id = thread_id) as \"tid\" from samples where \
    symbol_id in (select id from symbols where name = " + sqlFunctions + " ) order by tid asc");
  }
  else if(functions.empty() && !allocations.empty())
  {
    auto sqlAllocations = SqlUtils::makeSqlStringObjects(allocations);
    getThreadIdsQuery.prepare("select distinct (select tid from threads where id = thread_id) as \"tid\" from samples where \
    allocation_id in ( " + sqlAllocations + ") order by tid asc");
  }
  else
  {
    getThreadIdsQuery.prepare("select (select tid from threads where id = thread_id) as \"tid\" from samples order by tid asc");
  }
  getThreadIdsQuery.exec();
  ui->comboBox->blockSignals(true);
  ui->comboBox->clear();
  while(getThreadIdsQuery.next())
  {
    ui->comboBox->addItem(getThreadIdsQuery.value(0).toString());
  }
  ui->comboBox->addItem("All Threads");
  ui->comboBox->setCurrentText("All Threads");
  ui->comboBox->blockSignals(false);
}

QVector<GraphWindow::PointType> GraphWindow::getTimeAddressData(const QString& threadId) const
{
  QVector<PointType> values;
  values.reserve(100000);
  QSqlQuery getTimeAddressQuery;
  QString tidQueryPart = "";
  if(threadId != "")
  {
    tidQueryPart = "thread_id = (select id from threads where tid = " + threadId + ") and";
  }
  if(!functions.empty() && !allocations.empty())
  {
    auto sqlFunctions = SqlUtils::makeSqlStringFunctions(functions);
    auto sqlAllocations = SqlUtils::makeSqlStringObjects(allocations);
    getTimeAddressQuery.prepare("select time - (select min(time) from samples where id != 0),to_ip from samples where id != 0 and \
    memory_opcode in (select id from memory_opcodes where name = \"Load\" or name = \"Store\") and \
    " + tidQueryPart + "\
    symbol_id in (select id from symbols where name = " + sqlFunctions + " ) and \
    allocation_id in ( " + sqlAllocations + ") order by time asc");
  }
  else if(!functions.empty() && allocations.empty())
  {
    auto sqlFunctions = SqlUtils::makeSqlStringObjects(functions);
    getTimeAddressQuery.prepare("select time - (select min(time) from samples where id != 0),to_ip from samples where id != 0 and \
    memory_opcode in (select id from memory_opcodes where name = \"Load\" or name = \"Store\") and \
    " + tidQueryPart + "\
    symbol_id in (select id from symbols where name = " + sqlFunctions + " ) order by time asc");
  }
  else if(functions.empty() && !allocations.empty())
  {
    auto sqlAllocations = SqlUtils::makeSqlStringObjects(allocations);
    getTimeAddressQuery.prepare("select time - (select min(time) from samples where id != 0),to_ip from samples where id != 0 and \
    memory_opcode in (select id from memory_opcodes where name = \"Load\" or name = \"Store\") and \
    " + tidQueryPart + "\
    allocation_id in ( " + sqlAllocations + ") order by time asc");
  }
  else
  {
    getTimeAddressQuery.prepare("select time - (select min(time) from samples where id != 0),to_ip from samples where id != 0 and \
    " + tidQueryPart + "\
    memory_opcode in (select id from memory_opcodes where name = \"Load\" or name = \"Store\") \
    order by time asc limit 1000");
  }
  getTimeAddressQuery.setForwardOnly(true);
  getTimeAddressQuery.exec();
  while(getTimeAddressQuery.next())
    {
      values.push_back(PointType(getTimeAddressQuery.value(0).toULongLong(),getTimeAddressQuery.value(1).toULongLong()));
    }
  return values;
}

void GraphWindow::setAxisY(const QVector<PointType>& values, QChartView *chartView)
{
  axisY->setTitleText("Address");
  auto minAddressPoint = std::min_element(values.begin(),values.end(),[](auto lhs, auto rhs){return lhs.second < rhs.second;});
  axisY->setMin(minAddressPoint->second);
  auto maxAddressPoint = std::max_element(values.begin(),values.end(),[](auto lhs, auto rhs){return lhs.second < rhs.second;});
  axisY->setMax(maxAddressPoint->second);
  axisY->setLabelFormat("%#x");
  chartView->chart()->setAxisY(axisY);
  minPoint.second = minAddressPoint->second;
  maxPoint.second = maxAddressPoint->second;
}

void GraphWindow::setAxisX(const QVector<PointType>& values, QChartView *chartView)
{
  axisX->setMin(0);
  auto maxTimePoint = std::max_element(values.begin(),values.end(),[](auto lhs, auto rhs){return lhs.first < rhs.first;});
  auto time = decodeTime(maxTimePoint->first);
  axisX->setMax(time.first);
  axisX->setTitleText("Time [" + time.second + "]");
  chartView->chart()->setAxisX(axisX);
  minPoint.first = 0;
  maxPoint.first = maxTimePoint->first;
}

void GraphWindow::setPointStyle()
{
  QPen pen;
  pen.setWidth(1);
  series->setPen(pen);
  QBrush brush;
  brush.setStyle(Qt::SolidPattern);
  brush.setColor(Qt::black);
  series->setBrush(brush);
  series->setMarkerSize(2);
}

void GraphWindow::showEvent(QShowEvent *event)
{
  if(event->spontaneous() == false)
  {
    //if show event is cuased by this application, not by OS
    draw();
  }
}

void GraphWindow::draw(const QString& threadId)
{
  chart->removeSeries(series);
  series->clear();

  if(threadId == "")
  {
    series->setName("All Threads");
  }
  else
  {
    series->setName("Thread Id " + threadId);
  }

  setPointStyle();
  auto values = getTimeAddressData(threadId);
  for(auto&& item : values)
  {
    *series << QPointF(item.first,item.second);
  }

  if(!axisSet)
  {
    chart->createDefaultAxes();
    setAxisX(values, chartView);
    setAxisY(values, chartView);
    axisSet = true;
  }

  QPen invPen;
  invPen.setWidth(0);
  series->setPen(invPen);
  *series << QPointF(minPoint.first,minPoint.second);
  *series << QPointF(maxPoint.first,maxPoint.second);

  chart->addSeries(series);
}

void GraphWindow::setFunctions(const QStringList &functions)
{
  this->functions = functions;
  populateComboBoxWithThreads();
}

void GraphWindow::setAllocations(const QStringList &allocations)
{
  this->allocations = allocations;
  populateComboBoxWithThreads();
}

QPair<unsigned long long, QString> GraphWindow::decodeTime(const unsigned long long value) const
{
  auto us = value / 1000;
  auto ms = us / 1000;
  auto s = ms / 1000;
  auto min = s / 60;
  QString text;
  unsigned long long outValue = 0;
  if(min != 0)
  {
   text = "s";
   outValue = s + s%60;
  }
  else if(s != 0)
  {
   text = "ms";
   outValue = ms + ms % 1000;
  }
  else if (ms != 0)
  {
    text = "us";
    outValue = us + us % 1000;
  }
  else if(us != 0)
  {
    text = "ns";
    outValue = value;
  }
  else
  {
    text = "ns";
    outValue = value;
  }
  return QPair<unsigned long long,QString>(outValue,text);
}

void GraphWindow::on_comboBox_currentIndexChanged(const QString &arg1)
{
  if(arg1 == "All Threads")
  {
    draw();
  }
  else
  {
    draw(arg1);
  }
}

void GraphWindow::on_exportToPdfPushButton_clicked()
{
  PdfWriter p;
  p.writeWidgetToPdf(chartView);
}

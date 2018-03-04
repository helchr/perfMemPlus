#include "timelinewindow.h"
#include "ui_timelinewindow.h"
#include <QList>
#include <QtSql>
#include <algorithm>
#include "pdfwriter.h"
#include "sqlutils.h"
#include <timelinewidget.h>
#include <timelineaxiswidget.h>


TimelineWindow::TimelineWindow(QWidget *parent) :
QDialog(parent),
ui(new Ui::TimelineWindow)
{
  this->setWindowFlags(Qt::Window);
  ui->setupUi(this);
}

TimelineWindow::TimelineWindow(const QStringList& items, const QString& dbPath, QWidget *parent) :
QDialog(parent),
ui(new Ui::TimelineWindow)
{
  this->setWindowFlags(Qt::Window);
  ui->setupUi(this);
  this->setWindowTitle(this->windowTitle() + " - " + dbPath);

  this->allItems = items;

  auto min = getFirstSampleTime();
  auto max = getLastSampleTime();
  auto range = max - min;

  for(auto item : items)
  {
    auto rList = getReadList(QStringList(item));
    trimList(rList,min);
    auto r = new TimelineWidget(this);
    r->setPoints(rList,QColor(Qt::green));
    r->setMaxValue(range);
    ui->formLayout->addRow("Object " + item + " read:",r);

    auto wList = getWriteList(QStringList(item));
    trimList(wList,min);
    auto w = new TimelineWidget(this);
    w->setPoints(wList,QColor(Qt::red));
    w->setMaxValue(range);
    ui->formLayout->addRow("Object " + item + " write:",w);

    ui->formLayout->addRow(" ",(QWidget*)nullptr); // empty widget for spacing
  }

  time = new TimelineAxisWidget(this);
  time->setAxis(0,range);
  ui->formLayout->addRow("Time:",time);
}

void TimelineWindow::trimList(QList<unsigned long long>& list, const unsigned long long min) const
{
  for(auto& item : list)
  {
    item = item - min;
  }
}

TimelineWindow::~TimelineWindow()
{
  delete ui;
}

unsigned long long TimelineWindow::getFirstSampleTime() const
{
  QSqlQuery q("select min(time) from samples where time != 0");
  q.exec();
  if(q.next())
  {
    return q.value(0).toULongLong();
  }
  else
  {
    throw(std::runtime_error("Can not find time of first sample"));
  }
}

unsigned long long TimelineWindow::getLastSampleTime() const
{
  QSqlQuery q("select max(time) from samples");
  q.exec();
  if(q.next())
  {
    return q.value(0).toULongLong();
  }
  else
  {
    throw(std::runtime_error("Can not find time of first sample"));
  }
}

QList<unsigned long long> TimelineWindow::getReadList(const QStringList& items) const
{
  auto nullClause = SqlUtils::makeIncludeNullStatement(items);
  auto sqlItems = SqlUtils::makeSqlStringObjects(items);
  QString q("select time from samples where memory_opcode = \
  (select id from memory_opcodes where name = \"Load\") and \
  (allocation_id in (" + sqlItems + ") " + nullClause + ") order by time asc");
  return getList(q);
}

QList<unsigned long long> TimelineWindow::getWriteList(const QStringList& items) const
{
  auto nullClause = SqlUtils::makeIncludeNullStatement(items);
  auto sqlItems = SqlUtils::makeSqlStringObjects(items);
  QString q("select time from samples where memory_opcode = \
  (select id from memory_opcodes where name = \"Store\") and \
  (allocation_id in (" + sqlItems + ") " + nullClause + ") order by time asc");
  return getList(q);
}

QList<unsigned long long> TimelineWindow::getList(const QString &query) const
{
  QSqlQuery q(query);
  q.exec();
  QList<unsigned long long> readList;
  while(q.next())
  {
    readList.push_back(q.value(0).toULongLong());
  }
  return readList;
}

void TimelineWindow::on_exportToPdfPushButton_clicked()
{
  PdfWriter p;
  p.writeWidgetToPdf(ui->widget);
}

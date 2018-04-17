#include "abstracttimelinewidget.h"
#include <QPainter>
#include <QPaintEvent>

AbstractTimelineWidget::AbstractTimelineWidget(QWidget *parent) : QWidget(parent)
{
  painter = new QPainter();
}

AbstractTimelineWidget::~AbstractTimelineWidget()
{
 delete painter;
}

void AbstractTimelineWidget::paintBackground(QPaintEvent *event)
{
  auto background = QBrush(QColor(Qt::white));
  painter->fillRect(event->rect(),background);
}

unsigned long long AbstractTimelineWidget::scale(const unsigned long long x)
{
  double scale = (double) this->size().width() / (double) maxValue;
  unsigned long long drawPos = std::round((double)x * scale);
  return drawPos;
}

QString AbstractTimelineWidget::decodeTime(const unsigned long long value)
{
  auto us = value / 1000;
  auto ms = us / 1000;
  auto s = ms / 1000;
  auto min = s / 60;
  QString text;
  if(min != 0)
  {
   text = QString::number(min) + "min ";
   s = s % 60;
   text += QString::number(s) + "s ";
  }
  else if(s != 0)
  {
   text = QString::number(s) + "s ";
   ms = ms % 1000;
   text += QString::number(ms) + "ms ";
  }
  else if (ms != 0)
  {
    text = QString::number(ms) + "ms ";
    us = us % 1000;
    text += QString::number(us) + "us ";
  }
  else if(us != 0)
  {
    text = QString::number(us) + "us ";
    auto ns = value % 1000;
    text += QString::number(ns) + "ns ";
  }
  else
  {
    text = value + "ns";
  }
  return text;
}

void AbstractTimelineWidget::setAxis(unsigned long long min, unsigned long long max)
{
  this->minValue = min;
  this->maxValue = max;
}

void AbstractTimelineWidget::setMaxValue(const unsigned long long value)
{
 this->maxValue = value;
}

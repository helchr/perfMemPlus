#include "timelinewidget.h"
#include <QPainter>
#include <QPaintEvent>

TimelineWidget::TimelineWidget(QWidget *parent) : AbstractTimelineWidget(parent)
{
}

void TimelineWidget::setPoints(const QList<unsigned long long>& points, const QColor& color)
{
  this->allPoints = points;
  this->pointsColor = color;
}

void TimelineWidget::paintEvent(QPaintEvent *event)
{
  painter->begin(this);
  paintBackground(event);
  drawPoints(allPoints);
  painter->end();
}

void TimelineWidget::setPointsStyle(const QColor& c)
{
  QPen pen(c,1,Qt::SolidLine);
  painter->setPen(pen);
}

void TimelineWidget::drawPoint(const int xPos)
{
  painter->drawLine(xPos,1,xPos,this->size().height());
}

void TimelineWidget::drawPoints(const QList<unsigned long long> &points)
{
  static int lastDrawPos = -1;
  setPointsStyle(pointsColor);
  for(auto point : points)
  {
    unsigned long long drawPos = scale(point);
    if(lastDrawPos != (int) drawPos)
    {
      drawPoint((int)drawPos);
      lastDrawPos = (int) drawPos;
    }
  }
}

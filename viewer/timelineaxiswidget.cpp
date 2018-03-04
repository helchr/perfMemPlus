#include "timelineaxiswidget.h"
#include <QPainter>
#include <QPen>

TimelineAxisWidget::TimelineAxisWidget(QWidget *parent) : AbstractTimelineWidget(parent)
{
}

void TimelineAxisWidget::drawAxis(unsigned long long min, unsigned long long max)
{
  this->resize(this->size().width(),30);
  setAxisStyle();
  const int lineOffsetTop = 5;
  drawStraightLine(lineOffsetTop);
  drawLeftmostAxisLabel(min,lineOffsetTop);
  drawRightmostAxisLabel(max,lineOffsetTop);
  drawTicks(min,max,lineOffsetTop);
}

void TimelineAxisWidget::drawTicks(const unsigned long long left, const unsigned long long right, const int yOffset)
{
  const int textHeight = 20;
  const int textWidth = 200;

  unsigned long long half = (right-left) / 2;
  half = left + half;
  auto drawPos = scale(half);
  auto drawPosRight = scale(right);
  if(drawPosRight - drawPos > 200)
  {
    drawLineTick(drawPos,yOffset);
    QRect label(QPoint(drawPos-textWidth,yOffset*2),QPoint(drawPos+textWidth,yOffset*2+textHeight));
    painter->drawText(label,Qt::AlignCenter,decodeTime(half));
    drawTicks(left,half,yOffset);
    drawTicks(half,right,yOffset);
  }
  else if(drawPosRight - drawPos > 100)
  {
    drawLineTick(drawPos,yOffset);
  }
}

void TimelineAxisWidget::drawStraightLine(const int yOffset)
{
  auto xEnd = this->size().width();
  painter->drawLine(1,yOffset,xEnd,yOffset);
}

void TimelineAxisWidget::setAxisStyle()
{
  QPen blackPen(QColor(Qt::black),3,Qt::SolidLine);
  QBrush blackBrush(QColor(Qt::black));
  painter->setPen(blackPen);
  painter->setBrush(blackBrush);
}

void TimelineAxisWidget::drawLeftmostAxisLabel(const unsigned long long value, const int topOffset)
{
  const int textHeight = 20;
  const int textWidth = 40;
  QRect leftLabel(QPoint(1,topOffset*2),QPoint(1+textWidth,topOffset*2+textHeight));
  painter->drawText(leftLabel,QString::number(value));
  drawLineTick(1,topOffset);
}

void TimelineAxisWidget::drawRightmostAxisLabel(const unsigned long long value, const int topOffset)
{

  const int textHeight = 20;
  const int textWidth = 200;
  const int width = this->size().width();
  QRect rightLabel(QPoint(width-textWidth,topOffset*2),QPoint(width,topOffset*2+textHeight));
  painter->drawText(rightLabel,Qt::AlignRight,decodeTime(value));
  drawLineTick(width-2,topOffset);
}

void TimelineAxisWidget::drawLineTick(const int xPos, const int yLinePos)
{
  const int tickHeight = 10;
  const int tickYBegin = yLinePos - tickHeight/2;
  const int tickYEnd = tickYBegin + tickHeight;
  painter->drawLine(xPos,tickYBegin,xPos,tickYEnd);
}

void TimelineAxisWidget::paintEvent(QPaintEvent *event)
{
  painter->begin(this);
  AbstractTimelineWidget::paintBackground(event);
  drawAxis(minValue,maxValue);
  painter->end();
}

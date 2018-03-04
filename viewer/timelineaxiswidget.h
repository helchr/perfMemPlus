#ifndef TIMELINEAXISWIDGET_H
#define TIMELINEAXISWIDGET_H

#include <abstracttimelinewidget.h>

class QPaintEvent;

class TimelineAxisWidget : public AbstractTimelineWidget
{
  Q_OBJECT
public:
  explicit TimelineAxisWidget(QWidget *parent = nullptr);

signals:

public slots:
  virtual void paintEvent(QPaintEvent *event) override;

private:
  void drawTicks(const unsigned long long left, const unsigned long long right, const int yOffset);
  void drawAxis(unsigned long long min, unsigned long long max);
  void drawStraightLine(const int yOffset);
  void setAxisStyle();
  void drawLeftmostAxisLabel(const unsigned long long value, const int topOffset);
  void drawLineTick(const int xPos, const int yLinePos);
  void drawRightmostAxisLabel(const unsigned long long value, const int topOffset);

};

#endif // TIMELINEAXISWIDGET_H

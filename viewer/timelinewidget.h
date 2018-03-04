#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include <QList>
#include "abstracttimelinewidget.h"

class TimelineWidget : public AbstractTimelineWidget
{
  Q_OBJECT
public:
  explicit TimelineWidget(QWidget *parent = nullptr);
  void setPoints(const QList<unsigned long long>& points, const QColor& color);

protected:
  virtual void paintEvent(QPaintEvent* event) override;

signals:

public slots:

private:
  QList<unsigned long long> allPoints;
  QColor pointsColor;
  void drawPoints(const QList<unsigned long long>& points);
  void drawPoint(const int xPos);
  void setPointsStyle(const QColor &c);
};

#endif // TIMELINEWIDGET_H

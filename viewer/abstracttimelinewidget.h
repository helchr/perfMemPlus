#ifndef ABSTRACTTIMELINEWIDGET_H
#define ABSTRACTTIMELINEWIDGET_H

#include <QWidget>

class QPaintEvent;

class AbstractTimelineWidget : public QWidget
{
  Q_OBJECT
public:
  explicit AbstractTimelineWidget(QWidget *parent = nullptr);
  virtual ~AbstractTimelineWidget();
  void setMaxValue(const unsigned long long value);
  void setAxis(unsigned long long min, unsigned long long max);

protected:
  unsigned long long maxValue = 1;
  unsigned long long minValue = 0;
  QPainter* painter;
  virtual void paintEvent(QPaintEvent* event) = 0;
  QString decodeTime(const unsigned long long value) const;
  unsigned long long scale(const unsigned long long x) const;
  void paintBackground(QPaintEvent *event);

signals:

public slots:
};

#endif // ABSTRACTTIMELINEWIDGET_H

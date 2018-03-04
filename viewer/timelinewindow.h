#ifndef TIMELINEVIEW_H
#define TIMELINEVIEW_H
#include <QDialog>

namespace Ui {
  class TimelineWindow;
}

class TimelineAxisWidget;

class TimelineWindow : public QDialog
{
  Q_OBJECT

public:
  explicit TimelineWindow(QWidget *parent = 0);
  explicit TimelineWindow(const QStringList &items, const QString &path, QWidget *parent = 0);
  ~TimelineWindow();

private slots:
  void on_exportToPdfPushButton_clicked();

private:
  Ui::TimelineWindow *ui;
  TimelineAxisWidget* time;
  QStringList allItems;

  unsigned long long getFirstSampleTime() const;
  unsigned long long getLastSampleTime() const;
  unsigned long long getObjectLifetimeStart() const;
  unsigned long long getObjectLifetimeEnd() const;
  QList<unsigned long long> getReadList(const QStringList &items) const;
  QList<unsigned long long> getWriteList(const QStringList &items) const;
  QList<unsigned long long> getList(const QString& query) const;
  void trimList(QList<unsigned long long> &list, const unsigned long long min) const;
};

#endif // TIMELINEVIEW_H

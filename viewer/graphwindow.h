#ifndef GRAPHWINDOW_H
#define GRAPHWINDOW_H

#include <QDialog>
#include <QChart>
#include <QPair>
#include <QString>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

namespace Ui {
  class GraphWindow;
}

class GraphWindow : public QDialog
{
  Q_OBJECT

public:
  explicit GraphWindow(const QString &dbPath, QWidget *parent = 0) ;
  ~GraphWindow();

  void setAllocations(const QStringList &allocations);
  void setFunctions(const QStringList& functions);

public slots:

private slots:
  void on_comboBox_currentIndexChanged(const QString &arg1);

  void on_exportToPdfPushButton_clicked();

private:
  Ui::GraphWindow *ui;
  typedef QPair<unsigned long long,unsigned long long> PointType;
  QStringList functions;
  QStringList allocations;
  QChart *chart;
  QChartView *chartView;
  QScatterSeries* series;
  QValueAxis* axisY;
  QValueAxis* axisX;
  PointType minPoint;
  PointType maxPoint;
  bool axisSet = false;
  QVector<PointType> getTimeAddressData(const QString& threadId = "") const;
  void setAxisY(const QVector<PointType>& values, QChartView *chartView);
  void setAxisX(const QVector<PointType>& values, QChartView *chartView);
  QPair<unsigned long long, QString> decodeTime(const unsigned long long value) const;
  void populateComboBoxWithThreads();
  void draw(const QString& threadId = "");
  void setPointStyle();
  virtual void showEvent(QShowEvent *event) override;
};

#endif // GRAPHWINDOW_H

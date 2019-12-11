#ifndef ANALYSISMAIN_H
#define ANALYSISMAIN_H

#include <QMainWindow>
#include <QtSql>
#include <QtWidgets>

namespace Ui {
  class AnalysisMain;
}

struct BandwidthResult;
class TreeItem;
class Result;


class AnalysisMain : public QMainWindow
{
  Q_OBJECT

public:
  explicit AnalysisMain(QWidget *parent = nullptr);
  ~AnalysisMain();
  void setDbPath(const QString& path, const bool headless = false);

  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);


public slots:

private slots:
  void objectsTableRowChanged();
  void objectsCallPathTableRowChanged();
  void functionsTableRowChanged();
  void on_actionOpen_Database_triggered();
  void on_functionsCachePushButton_clicked();
  void on_showCacheObjectspushButton_clicked();
  void on_showObjectsAccessedByPushButton_clicked();
  void on_exportToPdfPushButton_clicked();
  void on_exportToPdfPushButton_2_clicked();
  void on_showAccessTimelinePushButton_clicked();
  void on_checkBox_stateChanged(int arg1);
  void on_showCacheCoherencyPushButton_clicked();
  void on_showObjectCacheCoherencyPushButton_clicked();
  void on_timeAccessDiagramButton_clicked();
  void on_timeAccessObjectsDiagramPushButton_clicked();
  void on_runPushButton_clicked();
  void on_exportToPdfPushButton_3_clicked();
  void displayCallstack();

private:
  Ui::AnalysisMain *ui;
  QSqlRelationalTableModel *modelFunctions;
  QSqlRelationalTableModel *modelObjects;
  QItemSelectionModel *modelObjectsSelectionModel;
  QSqlRelationalTableModel *modelObjectsAllocationSites;
  QItemSelectionModel *modelObjectsAllocationSitesSelectionModel;
  QString dbPath;
  QFutureWatcher<TreeItem*>* queryCallstack;
  QFutureWatcher<QList<Result>>* queryAutoAnalysis;

  void showError(const QSqlError &err, const bool headless);
  void loadDatabase(const QString& path, const bool headless = false);
  void showAllocationCallpath(const int allocationId);
  int getColumnCount(QTableView *tv) const;
  int getColumnCount(QTreeWidget *tv) const;
  void createViews();
  void changeModel(QTableView *view, QAbstractItemModel *model, QItemSelectionModel *selectionModel);
  QStringList getAllocationIdsFromCallPathIds(const QStringList &allocationIds) const;
  void showAllocationCallpathFromCallPathId(const int callpathId);
  QStringList getSelectedFunctions() const;
  QStringList getSelectedObjects() const;
  QPair<QString, int> getFileAndLineOfCallpathId(int id);
  QString getShortFileAndLineOfCallpathId(int id);
  void showBandwidthResults(const BandwidthResult& r,const auto& objectItem);
  void showCallpath(const QString& functionName);
  TreeItem *printCallstack(QString fName);
  void printCallstackEntry(long long id, long long level, QVector<QString> &callStack);
  QString space(long long level) const;
  void displayAutoAnalysisResult();
  void sqlitePerformanceSettings();
};

#endif // ANALYSISMAIN_H

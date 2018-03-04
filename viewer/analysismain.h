#ifndef ANALYSISMAIN_H
#define ANALYSISMAIN_H

#include <QMainWindow>
#include <QtSql>
#include <QtWidgets>

namespace Ui {
  class AnalysisMain;
}

class AnalysisMain : public QMainWindow
{
  Q_OBJECT

public:
  explicit AnalysisMain(QWidget *parent = 0);
  ~AnalysisMain();
  void setDbPath(const QString& path);
  void setExePath(const QString& path);

  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);


public slots:

private slots:
  void objectsTableRowChanged();
  void objectsCallPathTableRowChanged();

  void on_actionOpen_Database_triggered();
  void on_actionSet_Executable_triggered();
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

private:
  Ui::AnalysisMain *ui;
  QSqlRelationalTableModel *modelFunctions;
  QSqlRelationalTableModel *modelObjects;
  QItemSelectionModel *modelObjectsSelectionModel;
  QSqlRelationalTableModel *modelObjectsAllocationSites;
  QItemSelectionModel *modelObjectsAllocationSitesSelectionModel;
  QString dbPath;
  QString exePath;

  void showError(const QSqlError &err);
  void loadDatabase(const QString& path);
  void showAllocationCallpath(const int allocationId);
  int getColumnCount(QTableView *tv) const;
  int getColumnCount(QTreeWidget *tv) const;
  void createViews();
  void changeModel(QTableView *view, QAbstractItemModel *model, QItemSelectionModel *selectionModel);
  QStringList getAllocationIdsFromCallPathIds(const QStringList &allocationIds) const;
  void showAllocationCallpathFromCallPathId(const int callpathId);
  QStringList getSelectedFunctions() const;
  QStringList getSelectedObjects() const;
};

#endif // ANALYSISMAIN_H

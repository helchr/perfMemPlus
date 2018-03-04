#ifndef OBJECTACCESSEDBYFUNCTIONWINDOW_H
#define OBJECTACCESSEDBYFUNCTIONWINDOW_H

#include <QDialog>
#include <QtSql>

namespace Ui {
  class ObjectAccessedByFunctionWindow;
}

class ObjectAccessedByFunctionWindow : public QDialog
{
  Q_OBJECT

public:
  explicit ObjectAccessedByFunctionWindow(QWidget *parent = 0);
  explicit ObjectAccessedByFunctionWindow(QStringList items, const QString &dbPath, QWidget *parent = 0);
  void setExePath(const QString& path);
  ~ObjectAccessedByFunctionWindow();

private slots:
  void objectsTableRowChanged();
  void on_showCacheUsagePushButton_clicked();
  void on_exportToPdfPushButton_clicked();

private:
  Ui::ObjectAccessedByFunctionWindow *ui;
  QStringList items;
  QString exePath;
  QString dbPath;
  QSqlQueryModel* model;
  template<class T>
  void resizeColumnsToContents(T *widget);
};

#endif // OBJECTACCESSEDBYFUNCTIONWINDOW_H

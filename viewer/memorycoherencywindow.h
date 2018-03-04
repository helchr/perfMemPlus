#ifndef MEMORYCOHERENCYWINDOW_H
#define MEMORYCOHERENCYWINDOW_H

#include <QDialog>
#include <QtSql>

namespace Ui {
  class MemoryCoherencyWindow;
}

class MemoryCoherencyWindow : public QDialog
{
  Q_OBJECT

public:
  explicit MemoryCoherencyWindow(const QString& dbPath, QWidget *parent = 0);
  ~MemoryCoherencyWindow();

  void setFunctions(QStringList items);
  void setObjects(const QStringList &items);

private slots:
  void on_exportToPdfPushButton_clicked();

private:
  Ui::MemoryCoherencyWindow *ui;
  QSqlQueryModel* model;
  QStringList items;
};

#endif // MEMORYCOHERENCYWINDOW_H

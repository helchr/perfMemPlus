#ifndef MEMORYLEVELWINDOW_H
#define MEMORYLEVELWINDOW_H

#include <QDialog>
#include <QtSql>

namespace Ui {
  class MemoryLevelWindow;
}

class MemoryLevelWindow : public QDialog
{
  Q_OBJECT

public:
  enum LimitType
  {
    Functions,
    Objects
  };
  explicit MemoryLevelWindow(QWidget *parent = 0);
  explicit MemoryLevelWindow(QStringList items, const LimitType fo, const QString& dbPath, QWidget *parent = 0 );
  explicit MemoryLevelWindow(QStringList functionItems, const QStringList& objectItems, const QString& dbPath, QWidget *parent = 0 );
  ~MemoryLevelWindow();

private slots:
  void on_exportToPdfPushButton_clicked();

private:
  Ui::MemoryLevelWindow *ui;
  QSqlQuery selectAll;
  QSqlQuery selectLimitFunction;
  QSqlQuery selectLimitObject;
  QStringList functionItems;
  QStringList objectItems;
  LimitType functionsObjects;

  void printRemoteMemoryAccessFunctions(const QStringList &items);
  void printRemoteMemoryAccessFunctionsInclCache(const QStringList &items);
  void printRemoteMemoryAccessObjects(QStringList items);
  void printRemoteMemoryAccessObjectsInclCache(QStringList items);
  void printRemoteMemoryAccessFunctionsObjects(const QStringList &functionItems, QStringList objectItems);
  void printRemoteMemoryAccessFunctionsObjectsInclCache(const QStringList &functionItems, QStringList objectItems);
  QVariant executeSingleResultQuery(const QString &query) const;
};

#endif // MEMORYLEVELWINDOW_H

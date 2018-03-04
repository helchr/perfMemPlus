#include "analysismain.h"
#include "ui_analysismain.h"
#include "initdb.h"
#include "memorylevelwindow.h"
#include "percentdelegate.h"
#include "objectaccessedbyfunctionwindow.h"
#include "allocationcallpathresolver.h"
#include "pdfwriter.h"
#include "timelinewindow.h"
#include "memorycoherencywindow.h"
#include "graphwindow.h"
#include "guiutils.h"

AnalysisMain::AnalysisMain(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::AnalysisMain)
{
  ui->setupUi(this);
}

void AnalysisMain::loadDatabase(const QString& path)
{
  QSqlError err = initDb(path);
  if (err.type() != QSqlError::NoError)
  {
    showError(err);
    return;
  }

  createViews();

  modelFunctions = new QSqlRelationalTableModel(ui->functionsTableView);
  modelFunctions->setEditStrategy(QSqlTableModel::OnManualSubmit);
  modelFunctions->setTable("\"functions all\"");

  modelFunctions->setHeaderData(modelFunctions->fieldIndex("function"), Qt::Horizontal, tr("Function"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("execution time %"), Qt::Horizontal, tr("Execution Time %"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("IPC"), Qt::Horizontal, tr("IPC"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("latency"), Qt::Horizontal, tr("Average Lateny"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("latency %"), Qt::Horizontal, tr("Lateny %"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("latency factor"), Qt::Horizontal, tr("Lateny Factor"));

  if (!modelFunctions->select())
  {
    showError(modelFunctions->lastError());
    return;
  }

  ui->functionsTableView->setModel(modelFunctions);
  ui->functionsTableView->setItemDelegateForColumn(1,new PercentDelegate(ui->functionsTableView));
  ui->functionsTableView->setItemDelegateForColumn(4,new PercentDelegate(ui->functionsTableView));
  GuiUtils::resizeColumnsToContents(ui->functionsTableView);

  modelObjects = new QSqlRelationalTableModel(ui->objectsTableView);
  modelObjects->setEditStrategy(QSqlTableModel::OnManualSubmit);
  modelObjects->setTable("\"latency of allocations\"");

  modelObjects->setHeaderData(0, Qt::Horizontal, tr("Object"));
  modelObjects->setHeaderData(1, Qt::Horizontal, tr("Size [kb]"));
  modelObjects->setHeaderData(5, Qt::Horizontal, tr("Average Latency"));
  modelObjects->setHeaderData(6, Qt::Horizontal, tr("Latency %"));
  modelObjects->setHeaderData(7, Qt::Horizontal, tr("Latency Factor"));

  if (!modelObjects->select())
  {
    showError(modelObjects->lastError());
    return;
  }

  modelObjectsAllocationSites = new QSqlRelationalTableModel(ui->objectsTableView);
  modelObjectsAllocationSites->setEditStrategy(QSqlTableModel::OnManualSubmit);
  modelObjectsAllocationSites->setTable("\"latency of allocation sites\"");

  modelObjectsAllocationSites->setHeaderData(0, Qt::Horizontal, tr("Allocation Site"));
  modelObjectsAllocationSites->setHeaderData(1, Qt::Horizontal, tr("Size [kb]"));
  modelObjectsAllocationSites->setHeaderData(5, Qt::Horizontal, tr("Average Latency"));
  modelObjectsAllocationSites->setHeaderData(6, Qt::Horizontal, tr("Latency %"));
  modelObjectsAllocationSites->setHeaderData(7, Qt::Horizontal, tr("Latency Factor"));

  if (!modelObjectsAllocationSites->select())
  {
    showError(modelObjectsAllocationSites->lastError());
    return;
  }

  ui->objectsTableView->setModel(modelObjects);
  modelObjectsSelectionModel = ui->objectsTableView->selectionModel();
  ui->objectsTableView->setColumnHidden(2,true);
  ui->objectsTableView->setColumnHidden(3,true);
  ui->objectsTableView->setColumnHidden(4,true);

  ui->objectsTableView->setItemDelegateForColumn(6,new PercentDelegate(ui->objectsTableView));
  GuiUtils::resizeColumnsToContents(ui->objectsTableView);

  ui->objectsTableView->setModel(modelObjectsAllocationSites);
  modelObjectsAllocationSitesSelectionModel = ui->objectsTableView->selectionModel();

  connect(modelObjectsSelectionModel, SIGNAL(selectionChanged(QItemSelection ,QItemSelection)),
          this, SLOT(objectsTableRowChanged()));

  connect(modelObjectsAllocationSitesSelectionModel, SIGNAL(selectionChanged(QItemSelection ,QItemSelection)),
          this, SLOT(objectsCallPathTableRowChanged()));

  changeModel(ui->objectsTableView,modelObjects,modelObjectsSelectionModel);

  QSqlQuery q("select count(*) from samples");
  q.exec();
  int numSamples = 0;
  if(q.next())
  {
    numSamples = q.value(0).toInt();
  }
  this->dbPath = path;
  ui->statusBar->showMessage("Loaded database " + dbPath + " containing " + QString::number(numSamples) + " samples");
  this->setWindowTitle("PerfMemPlus Viewer - " + dbPath);
}

void AnalysisMain::showAllocationCallpathFromCallPathId(const int callpathId)
{
  AllocationCallPathResolver acpr;
  acpr.setExe(exePath);
  ui->treeWidget->clear();
  acpr.writeAllocationCallpathFromCallpathId(callpathId,ui->treeWidget);
  GuiUtils::resizeColumnsToContents(ui->treeWidget);
}

void AnalysisMain::showAllocationCallpath(const int allocationId)
{
  AllocationCallPathResolver acpr;
  acpr.setExe(exePath);
  ui->treeWidget->clear();
  acpr.writeAllocationCallpath(allocationId,ui->treeWidget);
  GuiUtils::resizeColumnsToContents(ui->treeWidget);
}

AnalysisMain::~AnalysisMain()
{
  delete ui;
}

void AnalysisMain::setDbPath(const QString &path)
{
  dbPath = path;
  loadDatabase(path);
}

void AnalysisMain::setExePath(const QString &path)
{
  exePath = path;
  auto sel = ui->objectsTableView->selectionModel()->selection();
  if(sel.count() > 0)
  {
    objectsTableRowChanged();
  }
}

void AnalysisMain::showError(const QSqlError &err)
{
  QMessageBox::critical(this, "Unable to initialize Database",
                        "Error initializing database: " + err.text());
}

void AnalysisMain::on_actionOpen_Database_triggered()
{
  QFileDialog fd(this,"Select analysis result database file");
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::ExistingFile);
  if(fd.exec())
  {
    dbPath = fd.selectedFiles().first();
    loadDatabase(dbPath);
  }
}

void AnalysisMain::on_actionSet_Executable_triggered()
{
  QFileDialog fd(this,"Select executable file");
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::ExistingFile);
  if(fd.exec())
  {
    exePath = fd.selectedFiles().first();
  }
}

void AnalysisMain::objectsTableRowChanged()
{
  auto selectedRows = ui->objectsTableView->selectionModel()->selectedRows(0);
  if(selectedRows.count() == 1)
  {
    auto id = selectedRows.first().data(Qt::DisplayRole).toInt();
    showAllocationCallpath(id);
  }
  else
  {
    ui->treeWidget->clear();
  }
}

void AnalysisMain::objectsCallPathTableRowChanged()
{
  auto selectedRows = ui->objectsTableView->selectionModel()->selectedRows(0);
  if(selectedRows.count() == 1)
  {
    auto id = selectedRows.first().data(Qt::DisplayRole).toInt();
    showAllocationCallpathFromCallPathId(id);
  }
  else
  {
    ui->treeWidget->clear();
  }
}

void AnalysisMain::on_functionsCachePushButton_clicked()
{
  auto sl = getSelectedFunctions();
  if(!sl.empty())
  {
    MemoryLevelWindow* mlw = new MemoryLevelWindow(sl,MemoryLevelWindow::Functions, dbPath, this);
    mlw->show();
  }
}

void AnalysisMain::on_showCacheObjectspushButton_clicked()
{
  auto sl = getSelectedObjects();
  if(!sl.empty())
  {
    MemoryLevelWindow* mlw = new MemoryLevelWindow(sl,MemoryLevelWindow::Objects,dbPath, this);
    mlw->show();
    mlw->show();
  }
}

QStringList AnalysisMain::getAllocationIdsFromCallPathIds(const QStringList& callPathIds) const
{
  QStringList allocationIds;
  auto sqlCallpathIds = callPathIds.join(", ");
  QSqlQuery q(QString("select id from allocations where call_path_id in (" + sqlCallpathIds + ")"));
  q.exec();
  while(q.next())
  {
    allocationIds << q.value(0).toString();
  }
  return allocationIds;
}

void AnalysisMain::on_showObjectsAccessedByPushButton_clicked()
{
  auto sl = getSelectedFunctions();
  if(!sl.empty())
  {
    auto w = new ObjectAccessedByFunctionWindow(sl, dbPath, this);
    w->setExePath(exePath);
    w->show();
  }
}

void AnalysisMain::on_exportToPdfPushButton_clicked()
{
  PdfWriter p(dbPath);
  p.writeTableToPdf(ui->functionsTableView);
}

void AnalysisMain::on_exportToPdfPushButton_2_clicked()
{
  PdfWriter p(dbPath);
  p.writeTableToPdf(ui->objectsTableView);
}

void AnalysisMain::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasUrls())
  {
    auto urls = event->mimeData()->urls();
    if(urls.count() == 1)
    {
      auto url = urls.first().path();
      QFileInfo fi(url);
      if(fi.isFile())
      {
        event->acceptProposedAction();
      }
    }
  }
}

void AnalysisMain::dropEvent(QDropEvent *event)
{
  auto urls = event->mimeData()->urls();
  if(urls.count() == 1)
  {
    auto url = urls.first().path();
    loadDatabase(url);
    event->acceptProposedAction();
  }
}

void AnalysisMain::on_showAccessTimelinePushButton_clicked()
{
  auto sl = getSelectedObjects();
  if(!sl.empty())
  {
    TimelineWindow* tmv = new TimelineWindow(sl,dbPath,this);
    tmv->show();
  }
}

void AnalysisMain::createViews()
{
  const QString minSamplesStr = "5";
  QSqlQuery q(QString("CREATE VIEW IF NOT EXISTS `latency of allocation sites` AS \
         select allocations.call_path_id,  \
         (select sum(address_end - address_start) / 1024 from allocations inq where inq.call_path_id = allocations.call_path_id group by inq.call_path_id) as `size[kb]`, \
         (select tid from threads where id = allocations.thread_id) as tid,\
         count(*) as numSamples, \
         sum(weight) as sumWeight, \
         cast (printf('%.2f', sum(weight)/count(*)) as float) as averageWeight, \
         cast (printf('%.2f', sum(weight)  *100 / (select cast( sum(weight)  as float)  from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%'))) as float)  as `Latency %`, \
         cast (printf('%.2f', avg(weight) / (select cast(avg(weight) as float) from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%'))) as float) as `latency contribution factor` \
         from samples left outer join allocations on samples.allocation_id = allocations.id \
         where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%') \
         group by allocations.call_path_id having numSamples >= " + minSamplesStr + " order by sumWeight desc"));
  q.exec();
}

void AnalysisMain::changeModel(QTableView* view, QAbstractItemModel* model, QItemSelectionModel* selectionModel)
{
  view->setModel(model);
  auto m = view->selectionModel();
  view->setSelectionModel(selectionModel);
  delete m; //setModel creates a new selection model but we do not use it
  selectionModel->clearSelection();
  selectionModel->selectionChanged(QItemSelection(),QItemSelection());
}

void AnalysisMain::on_checkBox_stateChanged(int state)
{
  if(state == 0)
  {
    changeModel(ui->objectsTableView,modelObjects,modelObjectsSelectionModel);   }
  else if(state == 2) // check box activated
  {
    changeModel(ui->objectsTableView,modelObjectsAllocationSites,modelObjectsAllocationSitesSelectionModel);
  }
}

void AnalysisMain::on_showCacheCoherencyPushButton_clicked()
{
  auto sl = getSelectedFunctions();
  if(!sl.empty())
  {
    MemoryCoherencyWindow* mcw = new MemoryCoherencyWindow(dbPath, this);
    mcw->setFunctions(sl);
    mcw->show();
  }
}

void AnalysisMain::on_showObjectCacheCoherencyPushButton_clicked()
{
  auto sl = getSelectedObjects();
  if(!sl.empty())
  {
    MemoryCoherencyWindow* mlw = new MemoryCoherencyWindow(dbPath, this);
    mlw->setObjects(sl);
    mlw->show();
  }
}

QStringList AnalysisMain::getSelectedFunctions() const
{
  auto indexList = ui->functionsTableView->selectionModel()->selectedRows(0);
  QStringList sl;
  if(indexList.empty() == false)
  {
    for(auto item : indexList)
    {
      sl.push_back(item.data(Qt::DisplayRole).toString());
    }
  }
  return sl;
}

void AnalysisMain::on_timeAccessDiagramButton_clicked()
{
  auto sl = getSelectedFunctions();
  if(!sl.empty())
  {
    auto gw = new GraphWindow(dbPath,this);
    gw->setFunctions(sl);
    gw->show();
  }
}

QStringList AnalysisMain::getSelectedObjects() const
{
  auto indexList = ui->objectsTableView->selectionModel()->selectedRows(0);
  QStringList sl;
  if(indexList.empty() == false)
  {
    for(auto item : indexList)
    {
      sl.push_back(item.data(Qt::DisplayRole).toString());
    }
    if(ui->checkBox->isChecked())
    {
      sl = getAllocationIdsFromCallPathIds(sl);
    }
  }
  return sl;
}

void AnalysisMain::on_timeAccessObjectsDiagramPushButton_clicked()
{
  auto sl = getSelectedObjects();
  if(!sl.empty())
  {
    auto gw = new GraphWindow(dbPath, this);
    gw->setAllocations(sl);
    gw->show();
  }
}

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
#include "autoanalysis.h"
#include "treemodel.h"
#include "treeitem.h"
#include <QThread>
#include <qtconcurrentrun.h>

AnalysisMain::AnalysisMain(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::AnalysisMain)
{
  ui->setupUi(this);
  queryCallstack = new QFutureWatcher<TreeItem*>(this);
  queryAutoAnalysis = new QFutureWatcher<QList<Result>>(this);
  connect(queryCallstack,&QFutureWatcher<TreeItem*>::finished,this,&AnalysisMain::displayCallstack);
  connect(queryAutoAnalysis,&QFutureWatcher<QList<Result>*>::finished,this,&AnalysisMain::displayAutoAnalysisResult);
}


void AnalysisMain::sqlitePerformanceSettings()
{
  QSqlQuery q;
  q.exec("PRAGMA synchronous = OFF");
  q.exec("PRAGMA journal_mode = OFF");
  q.exec("PRAGMA cache_size = 500000"); // page size of 1KB = 500MB cache size
}

void AnalysisMain::loadDatabase(const QString& path, const bool headless)
{
  QSqlError err = initDb(path);
  if (err.type() != QSqlError::NoError)
  {
    showError(err,headless);
    return;
  }
  createViews();
  sqlitePerformanceSettings();
  if(headless)
  {
    return;
  }

  modelFunctions = new QSqlRelationalTableModel(ui->functionsTableView);
  modelFunctions->setEditStrategy(QSqlTableModel::OnManualSubmit);
  modelFunctions->setTable("\"functions all\"");

  modelFunctions->setHeaderData(modelFunctions->fieldIndex("function"), Qt::Horizontal, tr("Function"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("execution time %"), Qt::Horizontal, tr("Execution Time %"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("IPC"), Qt::Horizontal, tr("IPC"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("latency"), Qt::Horizontal, tr("Average Latency"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("latency %"), Qt::Horizontal, tr("Latency %"));
  modelFunctions->setHeaderData(modelFunctions->fieldIndex("latency factor"), Qt::Horizontal, tr("Latency Factor"));

  if (!modelFunctions->select())
  {
    showError(modelFunctions->lastError(),headless);
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
    showError(modelObjects->lastError(),headless);
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
    showError(modelObjectsAllocationSites->lastError(),headless);
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

  connect(ui->functionsTableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
          this, SLOT(functionsTableRowChanged()));

  ui->callerTreeView->setItemDelegateForColumn(1,new PercentDelegate(ui->callerTreeView));

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
  ui->treeWidget->clear();
  acpr.writeAllocationCallpathFromCallpathId(callpathId,ui->treeWidget);
  GuiUtils::resizeColumnsToContents(ui->treeWidget,9000);
}

void AnalysisMain::showAllocationCallpath(const int allocationId)
{
  AllocationCallPathResolver acpr;
  ui->treeWidget->clear();
  acpr.writeAllocationCallpath(allocationId,ui->treeWidget);
  GuiUtils::resizeColumnsToContents(ui->treeWidget,9000);
}

AnalysisMain::~AnalysisMain()
{
  delete ui;
}

void AnalysisMain::setDbPath(const QString &path, const bool headless)
{
  dbPath = path;
  loadDatabase(path,headless);
}

void AnalysisMain::showError(const QSqlError &err, bool headless)
{
   if(headless)
   {
     qDebug() << "Unable to initialize Database" + err.text();
   }
   else
   {
    QMessageBox::critical(this, "Unable to initialize Database",
                        "Error initializing database: " + err.text());
   }
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

void AnalysisMain::functionsTableRowChanged()
{
  auto selectedRows = ui->functionsTableView->selectionModel()->selectedRows(0);
  if(selectedRows.count() == 1)
  {
    auto id = selectedRows.first().data(Qt::DisplayRole).toString();
    showCallpath(id);
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
      if(ui->checkBox->isChecked())
    {
      sl = getAllocationIdsFromCallPathIds(sl);
    }

    MemoryLevelWindow* mlw = new MemoryLevelWindow(sl,MemoryLevelWindow::Objects,dbPath, this);
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
      if(ui->checkBox->isChecked())
    {
      sl = getAllocationIdsFromCallPathIds(sl);
    }

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
    changeModel(ui->objectsTableView,modelObjects,modelObjectsSelectionModel);
  }
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
    if(ui->checkBox->isChecked())
    {
      mlw->setCallpaths(sl);
    }
    else
    {
      mlw->setObjects(sl);
    }
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
    }
  return sl;
}

void AnalysisMain::on_timeAccessObjectsDiagramPushButton_clicked()
{
  auto sl = getSelectedObjects();
  if(!sl.empty())
  {
    if(ui->checkBox->isChecked())
    {
      sl = getAllocationIdsFromCallPathIds(sl);
    }
    auto gw = new GraphWindow(dbPath, this);
    gw->setAllocations(sl);
    gw->show();
  }
}

void AnalysisMain::on_runPushButton_clicked()
{
  if(queryAutoAnalysis->isRunning())
  {
    queryAutoAnalysis->cancel();
    queryAutoAnalysis->waitForFinished();
  }
  ui->autoAnalysisResultsTreeWidget->clear();
  AutoAnalysis aa;
  queryAutoAnalysis->setFuture(QtConcurrent::run(aa,&AutoAnalysis::run));
  ui->runPushButton->setDisabled(true);
  ui->runPushButton->setText("Analysis Running");
}

void AnalysisMain::displayAutoAnalysisResult()
{
  auto results = queryAutoAnalysis->result();
  ui->runPushButton->setText("Run Analyis");
  ui->runPushButton->setEnabled(true);
  auto considerObjects = true;
  if(considerObjects)
  {
    QMap<QString,QTreeWidgetItem*> functionMap;
    for(auto result : results)
    {
      if(result.falseSharing || result.trueSharing || result.dramBandwidth.problem || result.remoteDramBandwidth.problem)
      {
        QTreeWidgetItem* objectItem;
        if(functionMap.contains(result.function))
        {
          auto functionItem = *(functionMap.find(result.function));
          QStringList info;
          //info << "Object allocation call path id: " + QString::number(result.callPathId);
          info << "Object id "+ QString::number(result.callPathId) + " allocated at: " + getShortFileAndLineOfCallpathId(result.callPathId);
          info << "Latency contribution in function: " + QString::number(result.objectLatencyPercent,'f',1) + "%";
          objectItem = new QTreeWidgetItem(functionItem,info);
          functionItem->addChild(objectItem);
          objectItem->setExpanded(true);
        }
        else
        {
          QStringList info;
          info << "Function: " + result.function;
          info << "Execution time contribution: " + QString::number(result.functionExectimePercent,'f',1) + "% ";
          auto functionItem = new QTreeWidgetItem(ui->autoAnalysisResultsTreeWidget,info);
          ui->autoAnalysisResultsTreeWidget->addTopLevelItem(functionItem);
          QStringList objInfo;
          objInfo << "Object id " + QString::number(result.callPathId) + " alloced at: " + getShortFileAndLineOfCallpathId(result.callPathId);
          objInfo << "Latency contribution in function: " + QString::number(result.objectLatencyPercent,'f',1) + "%";
          objectItem = new QTreeWidgetItem(functionItem,objInfo);
          functionItem->addChild(objectItem);
          functionMap.insert(result.function,functionItem);
          functionItem->setExpanded(true);
          objectItem->setExpanded(true);
        }
        if(result.falseSharing)
        {
          QStringList info;
          //info << "False Sharing" << "HITM Accesses: " + QString::number(result.hitmPercent,'f',1) + "%";
          info << "False Sharing";
          auto probItem = new QTreeWidgetItem(objectItem,info);
          QStringList addresses;
          auto addrStr = addresses.join(", ");
          QStringList addrList(addrStr);
          auto probDetails = new QTreeWidgetItem(probItem,addrList);
          probItem->addChild(probDetails);
          objectItem->addChild(probItem);
        }
        if(result.trueSharing)
        {
          QStringList info;
          info << "True Sharing";
          auto probItem = new QTreeWidgetItem(objectItem,info);
          objectItem->addChild(probItem);
          QStringList addresses;
          auto addrStr = addresses.join(", ");
          QStringList addrList(addrStr);
          auto probDetails = new QTreeWidgetItem(probItem,addrList);
          probItem->addChild(probDetails);
          objectItem->addChild(probItem);
        }
        showBandwidthResults(result.dramBandwidth,objectItem);
        showBandwidthResults(result.remoteDramBandwidth,objectItem);
      }
    }
  }
  QTreeWidgetItemIterator it(ui->autoAnalysisResultsTreeWidget);
  if(*it == nullptr)
  {
    QStringList info;
    info << "No performance problems found" ;
    auto noItem = new QTreeWidgetItem(ui->autoAnalysisResultsTreeWidget,info);
    ui->autoAnalysisResultsTreeWidget->addTopLevelItem(noItem);
  }
  GuiUtils::resizeColumnsToContents(ui->autoAnalysisResultsTreeWidget);
}

void AnalysisMain::showBandwidthResults(const BandwidthResult &r, const auto& objectItem)
{
  if(r.problem)
  {
    QStringList info;
    info << r.memory + " bandwidth limit";
    //info << "Latency limit exceeded by " + QString::number(result.latencyOverLimitPercent,'f',1) + "%";
    if(r.lowSampleCount)
    {
      info << QString(" Warning: low sample count") + " Parallel readers: " + QString::number(r.parallelReaders) + " Latency: " + QString::number(r.latency);
    }
    else
    {
      info << "Parallel readers: " + QString::number(r.parallelReaders) + " Latency: " + QString::number(r.latency);
    }
    //info << "Latency savings:" + QString::number(r.latencySavings,'e',1);
    auto probItem = new QTreeWidgetItem(objectItem,info);
    objectItem->addChild(probItem);
  }
}

void AnalysisMain::displayCallstack()
{
  // called when callstack data is ready to be displayed
  auto root = queryCallstack->result();
  TreeModel* model = new TreeModel(root,ui->callerTreeView);
  delete ui->callerTreeView->model();
  ui->callerTreeView->setModel(model);
  GuiUtils::resizeColumnsToContents(ui->callerTreeView);
}

void AnalysisMain::showCallpath(const QString &functionName)
{
  if(queryCallstack->isRunning())
  {
    queryCallstack->cancel();
    queryCallstack->waitForFinished();
  }
  queryCallstack->setFuture(QtConcurrent::run(this,&AnalysisMain::printCallstack,functionName));
  auto tmpRoot = new TreeItem({"Querying Callstack ..."});
  auto tmpModel = new TreeModel(tmpRoot);
  delete ui->callerTreeView->model();
  ui->callerTreeView->setModel(tmpModel);
}

inline uint qHash(const std::vector<QString> &key, uint seed = 0)
{
    return qHashRange(key.begin(), key.end(), seed);
}

QString AnalysisMain::space(long long level) const
{
  QString s;
  for(long long i = 0; i < level; i++)
  {
    s += "  ";
  }
  return s;
}



void AnalysisMain::printCallstackEntry(long long id, long long level, QVector<QString>& callStack)
{
  QTextStream out;
  QSqlQuery getCallstackEntry;
  getCallstackEntry.prepare("select parent_id, symbol, ip from call_paths_view where id = ? ");
  getCallstackEntry.bindValue(0,id);
  getCallstackEntry.exec();
  while(getCallstackEntry.next())
    {
      auto name = getCallstackEntry.value(1).toString();
      auto parentId = getCallstackEntry.value(0).toLongLong();
      auto ip = getCallstackEntry.value(3).toString();
      //out << space(level) << name << endl;
      //callStack.append(space(level) + name);
      callStack.append(name);
      if(parentId != 0)
      {
          printCallstackEntry(parentId,level+1,callStack);
      }
  }
}

TreeItem* AnalysisMain::printCallstack(QString fName)
{
  QSqlQuery getCallpathId("select call_path_id, 100.0 * sum(weight) / (select sum (weight) from samples where symbol_id = (select id from symbols where name = ?)) as latencyPercent from samples where symbol_id = (select id from symbols where name = ?) group by call_path_id order by latencyPercent desc");
  getCallpathId.bindValue(0,fName);
  getCallpathId.bindValue(1,fName);
  getCallpathId.exec();
  QHash<QVector<QString>,float> callStacks;
  while(getCallpathId.next())
  {
    QVector<QString> callStack;
    printCallstackEntry(getCallpathId.value(0).toLongLong(),0,callStack);
    if(callStacks.find(callStack) == callStacks.end())
    {
      callStacks.insert(callStack,getCallpathId.value(1).toFloat());
    }
    else
    {
      callStacks[callStack] += getCallpathId.value(1).toFloat();
    }
  }
  QTextStream out(stdout);

  QList<QVariant> l = {"Caller","Latency %"};
  auto root = new TreeItem(l);

  QHashIterator<QVector<QString>,float> i(callStacks);
  while (i.hasNext())
  {
    i.next();
    auto callStack = i.key();
    TreeItem* curTreeItem = root;
    int j = 0;
    while(j < callStack.size())
    {
       auto callStackEntry = callStack[j];
         //find child with same name
         bool found = false;
         for(int childIt = 0; childIt < curTreeItem->childCount(); childIt++)
         {
           if(curTreeItem->child(childIt)->data(0) == callStackEntry)
           {
             curTreeItem = curTreeItem->child(childIt);
             auto val = curTreeItem->data(1).toFloat();
             val += i.value();
             curTreeItem->setData(1,val);
             j++;
             found = true;
             break;
           }
         }
         if(!found)
         {
         // no child with name found
         QList<QVariant> list;
         list << callStackEntry << i.value();
         curTreeItem->appendChild(new TreeItem(list,curTreeItem));
         curTreeItem = curTreeItem->child(0);
         j++;
       }
     }
   }
  return root;
}

void AnalysisMain::on_exportToPdfPushButton_3_clicked()
{
  PdfWriter p(dbPath);
  p.writeWidgetToPdf(ui->autoAnalysisResultsTreeWidget);
}

QPair<QString,int> AnalysisMain::getFileAndLineOfCallpathId(int id)
{
  QSqlQuery q;
  q.prepare("select file, line from allocation_symbols where id = \
  (select allocation_symbol_id from allocation_call_paths where id = ?)");
  q.bindValue(0,id);
  q.exec();
  QPair<QString,int> result("",-1);
  if(q.next())
  {
    result.first = q.value(0).toString();
    result.second = q.value(1).toInt();
  }
  return result;
}

QString AnalysisMain::getShortFileAndLineOfCallpathId(int id)
{
  auto longName = getFileAndLineOfCallpathId(id);
  QString path = longName.first;
  auto filename = path.split("/").last();
  return filename + ":" + QString::number(longName.second);
}

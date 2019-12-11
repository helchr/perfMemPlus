#include <QCoreApplication>
#include <QtSql>
#include <iostream>
#include <fstream>
#include <regex>
#include <vector>
#include <limits>
#include "address2Line.h"
#include <QStringBuilder>
#include "counterattributes.h"

struct AllocationInfoRaw
{
  unsigned long long timestamp;
  unsigned long long address;
  unsigned long long size;
  int type;
  int cpu;
  int pid;
  int tid;
  long long callpathId;
};

struct CallpathSymbolInfoRaw
{
  QString name;
  QString dso;
  long long ip;
  long long offset;
};

void sqlitePerformanceSettings(QSqlDatabase& db)
{
  db.exec("PRAGMA synchronous = OFF");
  db.exec("PRAGMA journal_mode = OFF");
  db.exec("PRAGMA cache_size = 500000"); // page size of 1KB = 500MB cache size
}

void createAllocationsTable(QSqlDatabase& db)
{
  db.exec("DROP TABLE IF EXISTS allocations");
  db.exec("CREATE TABLE allocations ( \
  id INTEGER PRIMARY KEY UNIQUE, \
  thread_id bigint, \
  cpu integer, \
  address_start bigint, \
  address_end bigint, \
  time_start bigint, \
  time_end bigint, \
  call_path_id integer)");
}

void createAllocationsCallpathTable(QSqlDatabase& db)
{
  db.exec("DROP TABLE IF EXISTS allocation_call_paths");
  db.exec("CREATE TABLE allocation_call_paths ( \
  id integer PRIMARY KEY, \
  parent_id integer, \
  allocation_symbol_id integer, \
  UNIQUE(parent_id,allocation_symbol_id))");
}

void createAllocationsSymbolsTable(QSqlDatabase& db)
{
  db.exec("DROP TABLE IF EXISTS allocation_symbols");
  db.exec("CREATE TABLE allocation_symbols ( \
  id integer PRIMARY KEY UNIQUE, \
  ip bigint, \
  dso_id integer, \
  name varchar(2048), \
  offset integer, \
  file varchar(4096), \
  line integer, \
  inlinedBy varchar(4096))");
}

void prepare(QSqlQuery& query, const QString& statement)
{
  query.setForwardOnly(true);
  query.prepare(statement);
}

long long getLastInsertedId(const QSqlDatabase& db)
{
  QSqlQuery getKey(db);
  prepare(getKey,"SELECT last_insert_rowid()");
  getKey.exec();

  if(getKey.next())
  {
    return getKey.value(0).toLongLong();
  }
  else
  {
    // error
    throw std::runtime_error("Can not get last inserted id");
  }
}

bool isInCallchain(const QString& line)
{
  if (line.length() > 0)
  {
    if(line[0] == QLatin1Char('/') || line[0] == QLatin1Char('['))
    {
      return true;
    }
  }
  return false;
}


bool isCallchainStart(const QString& line)
{
    if (line == QLatin1String("callchain"))
    {
        return true;
    }
    else
    {
        return false;
    }
    /*
  auto idx = line.indexOf(' ',0);
  if(idx != -1)
  {
    if(line.indexOf(QLatin1String("callchain"),idx+1) == idx+1)
    {
      return true;
    }
  }
  return false;
    */
}

bool isAllocationInfo(const QString& line)
{
  auto idx = line.indexOf(' ',0);
  if(idx != -1)
  {
    if(line.indexOf(QLatin1String("pid "),idx+1) == idx+1)
    {
      return true;
    }
  }
  return false;
}

bool isDeallocation(const AllocationInfoRaw a)
{
  if(a.type == 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool isAllocation(const AllocationInfoRaw a)
{
  if(a.type == 1)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void printWarningIncompleteEntry()
{
  static bool printed = false;
  if(!printed)
  {
    std::cout << "At least one allocation without matching deallocation found. Assuming deallocation at the end of program execution.\n";
    printed = true;
  }
}



void insertAllocation(const AllocationInfoRaw& ao, const long long threadId, const QSqlDatabase& db)
{
    QSqlQuery insertAllocation(db);
    prepare(insertAllocation,"INSERT INTO allocations \
    ( thread_id, cpu, address_start, address_end, \
    time_start, time_end, call_path_id) \
    VALUES (?, ?, ?, ?, ?, ?, ?)");
    insertAllocation.bindValue(0,threadId);
    insertAllocation.bindValue(1,ao.cpu);
    insertAllocation.bindValue(2,(long long) ao.address);
    insertAllocation.bindValue(3,(long long) (ao.address+ao.size));
    insertAllocation.bindValue(4,(long long) ao.timestamp);
    insertAllocation.bindValue(5,std::numeric_limits<long long>::max());
    insertAllocation.bindValue(6,ao.callpathId);
    insertAllocation.exec();
}

void processAllocationInfo(const AllocationInfoRaw& ao, const QSqlDatabase& db)
{
  if(isAllocation(ao))
  {
    QSqlQuery getThreadId(db);
    prepare(getThreadId, "SELECT id from threads where pid = ? AND tid = ?");
    getThreadId.bindValue(0, (const int) ao.pid);
    getThreadId.bindValue(1,(const int) ao.tid);
    long long threadId = -1;
    getThreadId.exec();
    if(getThreadId.next())
    {
      threadId = getThreadId.value(0).toLongLong();
      if(getThreadId.next())
      {
        //error
        //returned two results
      }
    }
    else
    {
      QSqlQuery insertNewThread(db);
      prepare(insertNewThread,"INSERT INTO threads \
      ( machine_id, process_id, pid, tid) \
      VALUES (1,1,?,?) ");
      insertNewThread.bindValue(0,ao.pid);
      insertNewThread.bindValue(1,ao.tid);
      insertNewThread.exec();
      threadId = getLastInsertedId(db);
    }
    insertAllocation(ao,threadId,db);
  }
  else if(isDeallocation(ao)) // must be deallocation
  {
    // update end time of entry
    QSqlQuery getAllocation(db);
    prepare(getAllocation,"select id from allocations where \
    address_start = ? order by time_start desc limit 1");
    getAllocation.bindValue(0,ao.address);
    getAllocation.exec();
    long long id;
    if(getAllocation.next())
    {
      id = getAllocation.value(0).toLongLong();
      QSqlQuery updateAllocation(db);
      prepare(updateAllocation,"update allocations set time_end = ? where id = ?");
      updateAllocation.bindValue(0,ao.timestamp);
      updateAllocation.bindValue(1,id);
      updateAllocation.exec();
    }
    else
    {
      printWarningIncompleteEntry();
    }
  }
}

bool readAllocationInfo(const std::string& line, AllocationInfoRaw& tmp)
{
  std::stringstream ss(line);
  std::string pidDelim;
  std::string cpuDelim;
  std::string sizeDelim;
  std::string addrDelim;
  std::string typeDelim;

  ss >> tmp.timestamp;
  if (!ss.good())
    return false;
  ss >> pidDelim;
  if (!ss.good())
    return false;
  ss >> tmp.pid;
  if (!ss.good())
    return false;
  ss >> cpuDelim;
  if (!ss.good())
    return false;
  ss >> tmp.cpu;
  if (!ss.good())
    return false;
  ss >> sizeDelim;
  if (!ss.good())
    return false;
  ss >> tmp.size;
  if (!ss.good())
    return false;
  ss >> addrDelim;
  if (!ss.good())
    return false;
  ss >> std::hex >> tmp.address;
  if (!ss.good())
    return false;
  ss >> typeDelim;
  if (!ss.good())
    return false;
  ss >> tmp.type;

  if(ss.eof())
  {
    return true;
  }
  else
  {
    return false;
  }
}

int getTid(const QString& file)
{
  static thread_local QRegExp rgx("(\\d+)(\\.allocationData)");
  rgx.indexIn(file);
  QStringList list = rgx.capturedTexts();
  if(list.length() == 3)
  {
    return list.at(1).toLongLong();
  }
  else
  {
    throw std::runtime_error("Can not find thread id");
  }
}


CallpathSymbolInfoRaw readCallchainEntry(const QString& input)
{
  CallpathSymbolInfoRaw callpathInfo;
  std::string ip;
  std::string offset;
  std::string dso;
  std::string name;
  auto trimInput = input.trimmed().toStdString();
  std::stringstream line(trimInput);
  if(line.peek() == '[')
  {
    // only address avalable
    line.ignore(3,'x');
    getline(line,ip,']');
  }
  else
  {
    std::getline(line,dso,'(');
    callpathInfo.dso = QString::fromStdString(dso);
    if(line.peek() == ')')
    {
      // only dso and address
      line.ignore(5,'x');
      getline(line,ip,']');
    }
    else if(line.peek() == '+')
    {
      // dso, offset and address
      line.ignore(1,'+');
      std::getline(line,offset,')');
      line.ignore(5,'x');
      std::getline(line,ip,']');
    }
    else
    {
      // full information available
      std::getline(line,name,'+');
      callpathInfo.name = QString::fromStdString(name);
      std::getline(line,offset,')');
      line.ignore(5,'x');
      std::getline(line,ip,']');
    }
  }
  callpathInfo.offset = strtoll(offset.c_str(),NULL,16);
  callpathInfo.ip = strtoll(ip.c_str(), NULL, 16);
  return callpathInfo;
}


template<typename Out>
void split(const std::string &s, char delim, Out result) {
  std::stringstream ss;
  ss.str(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    *(result++) = item;
  }
}


std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, std::back_inserter(elems));
  return elems;
}


QString getLongNameOfDso(int dsoId, const QSqlDatabase& db)
{
 QSqlQuery getDsoLongName(db);
 getDsoLongName.prepare("Select long_name from dsos where id = ?");
 getDsoLongName.bindValue(0,dsoId);
 getDsoLongName.exec();
 if(getDsoLongName.next())
 {
   return getDsoLongName.value(0).toString();
 }
 else
 {
   return QString("");
 }
}

long long insertCallpathSymbolInfo(const CallpathSymbolInfoRaw& callpathSymbol, const QSqlDatabase& db)
{
  long long dsoId;
  if(callpathSymbol.dso == "")
  {
    dsoId = 0;
  }
  else
  {
    //find existing dso id
    QSqlQuery getDsoId;
    prepare(getDsoId,"SELECT id from dsos WHERE short_name = ?");
    // get shortname of dso string: split at /, get the last entry
    QString shortName;
    auto list = callpathSymbol.dso.splitRef('/');
    shortName = list.last().toString();
    getDsoId.bindValue(0,shortName);
    getDsoId.exec();
    if(getDsoId.next())
    {
      dsoId = getDsoId.value(0).toLongLong();
    }
    else
    {
      // add new dso
      QSqlQuery addDso;
      prepare(addDso,"INSERT INTO dsos (machine_id,short_name,long_name,build_id) VALUES (1,?,?,'')");
      addDso.bindValue(0,shortName);
      addDso.bindValue(1,callpathSymbol.dso);
      addDso.exec();
      dsoId = getLastInsertedId(db);
    }
  }
  auto dsoName = getLongNameOfDso(dsoId,db);
  Address2Line::LineInfo lineInfo;
  if(dsoName != "")
  {
    lineInfo = Address2Line::getLineInfo(dsoName,QString::number(callpathSymbol.ip,16));
  }

  /* ip based call paths
  QSqlQuery getIdOfIp;
  prepare(getIdOfIp,"SELECT id from allocation_symbols where ip = ?");
  getIdOfIp.bindValue(0,callpathSymbol.ip);
  long long symbolId = -1;
  getIdOfIp.exec();
  if(getIdOfIp.next())
  {
    symbolId = getIdOfIp.value(0).toLongLong();
    if(getIdOfIp.next())
    {
      //error
      //returned two results
    }
  }
  */

  // line and file based call paths
  long long symbolId = -1;
  static QHash<QPair<QString,int>,long long> cache;
  auto it = cache.find(QPair<QString,int>(lineInfo.file,lineInfo.line));
  if(it != cache.end())
  {
    symbolId = it.value();
  }
  else
  {
    if(lineInfo.line != -1)
    {
      QSqlQuery insertNewAllocationSymbol;
      prepare(insertNewAllocationSymbol,"INSERT INTO allocation_symbols \
      (ip, dso_id, name, offset, file, line, inlinedBy) \
      VALUES (?,?,?,?,?,?,?) ");
      insertNewAllocationSymbol.bindValue(0,callpathSymbol.ip);
      insertNewAllocationSymbol.bindValue(1,dsoId);
      insertNewAllocationSymbol.bindValue(2,lineInfo.function);
      insertNewAllocationSymbol.bindValue(3,callpathSymbol.offset);
      insertNewAllocationSymbol.bindValue(4,lineInfo.file);
      insertNewAllocationSymbol.bindValue(5,lineInfo.line);
      insertNewAllocationSymbol.bindValue(6,lineInfo.inlinedBy);
      insertNewAllocationSymbol.exec();
    }
    else
    {
      QSqlQuery insertNewAllocationSymbol;
      prepare(insertNewAllocationSymbol,"INSERT INTO allocation_symbols \
      (ip, dso_id, name, offset) \
      VALUES (?,?,?,?) ");
      insertNewAllocationSymbol.bindValue(0,callpathSymbol.ip);
      insertNewAllocationSymbol.bindValue(1,dsoId);
      insertNewAllocationSymbol.bindValue(2,callpathSymbol.name);
      insertNewAllocationSymbol.bindValue(3,callpathSymbol.offset);
      insertNewAllocationSymbol.exec();
    }
    symbolId = getLastInsertedId(db);
    cache.insert(QPair<QString,int>(lineInfo.file,lineInfo.line),symbolId);
  }
  return symbolId;
}

long long getCallpathId(const long long symbolId, const long long parentId, const QSqlDatabase& db)
{
  QSqlQuery query(db);
  prepare(query,"SELECT id from allocation_call_paths where parent_id = ? and allocation_symbol_id = ?");
  query.bindValue(0,parentId);
  query.bindValue(1,symbolId);
  query.exec();
  if(query.next())
  {
    return query.value(0).toLongLong();
  }
  else
  {
    return -1;
  }
}

long long insertCallpathEntry(const long long symbolId, const long long parentId, const QSqlDatabase& db)
{
  // search for same parent and symbol_id
  // if not exists insert
  // Add callstack entry
  auto id = getCallpathId(symbolId,parentId,db);
  if(id == -1)
  {
    QSqlQuery addAllocationCallpath;
    prepare(addAllocationCallpath,"INSERT INTO allocation_call_paths \
    (parent_id, allocation_symbol_id) \
    VALUES (?, ?)");
    addAllocationCallpath.bindValue(0,parentId);
    addAllocationCallpath.bindValue(1,symbolId);
    addAllocationCallpath.exec();
    return getLastInsertedId(db);
  }
  else
  {
    return id;
  }
}


long long insertCallpath(const std::vector<long long>& callpathIds, const QSqlDatabase& db)
{
  long long parentId = 0;
  for (auto i = callpathIds.rbegin(); i != callpathIds.rend(); ++i ) {
    parentId = insertCallpathEntry(*i,parentId,db);
  }
  return parentId;
}




void readAllocationFile(const std::string& file, QSqlDatabase& db)
{
  db.exec("CREATE INDEX IF NOT EXISTS idx_ip on allocation_symbols(ip)");
  db.exec("CREATE INDEX IF NOT EXISTS idx_address_start on allocations(address_start)");
  //db.exec("CREATE INDEX IF NOT EXISTS idx_file_line on allocation_symbols(file,line)");
  db.commit();
  db.exec("BEGIN TRANSACTION");
  std::ifstream infile(file);
  std::string line;
  int tid = getTid(QString::fromStdString(file));
  std::vector<long long> callpathSymbolIds;

  // insert of anon object
  AllocationInfoRaw a0;
  a0.cpu = 0;
  a0.pid = 0;
  a0.tid = 0;
  a0.size = 0;
  a0.type = 0;
  a0.address = 0;
  a0.timestamp = 0;
  a0.callpathId = 0;
  insertAllocation(a0,0,db);

  while (std::getline(infile,line))
  {
    if(isCallchainStart(QString::fromStdString(line)))
    {
        continue;
    }
    else
    {
      AllocationInfoRaw tmp;
      tmp.tid = tid;
      bool readOk = readAllocationInfo(line, tmp);

      if(readOk)   {
        tmp.callpathId = insertCallpath(callpathSymbolIds,db);
        processAllocationInfo(tmp,db);
        callpathSymbolIds.clear();
      }
      else // must be a callchain entry
      {
        auto callpathInfo = readCallchainEntry(QString::fromStdString(line));
        auto symbolId = insertCallpathSymbolInfo(callpathInfo,db);
        callpathSymbolIds.push_back(symbolId);
      }
    }
  }
  db.exec("END TRANSACTION");
}

void readAllocationTrackerFiles(QString dir, QSqlDatabase& db)
{
  const QString suffix = "allocationData";
  QDirIterator it(dir);
  //QThreadPool tp;
  //tp.setMaxThreadCount(2);
  while(it.hasNext())
  {
    it.next();
    if(it.fileInfo().isFile())
    {
      if(it.fileInfo().suffix() == suffix)
      {
        auto path = it.fileInfo().filePath();
        readAllocationFile(path.toStdString(),db);
        /*
              if(it.fileInfo().size() > sizeLimit)
                {
                  QtConcurrent::run(&tp,readAllocationFileParallel,path.toStdString(),db);
                }
              else
                {
                  readAllocationFile(path.toStdString(),db);
                }
                */
      }
    }
  }
  //tp.waitForDone();
}

void modifySamplesTable()
{
  try
  {
    QSqlQuery addColumn;
    prepare(addColumn,"alter table samples add allocation_id");
    addColumn.exec();
  }
  catch(QSqlError& e)
  {
    if(e.text() == "duplicate column name: allocation_id")
    {
      return;
    }
    else
    {
      throw e;
    }
  }
}

std::string getTime()
{
  time_t t = time(0);
  char* tStr = asctime(localtime(&t));
  tStr[strlen(tStr) -1] = '\0';
  return tStr;
}


void updateRelationshipKeys(QSqlDatabase& db)
{
  db.exec("CREATE INDEX IF NOT EXISTS idx_allocations_id on allocations(id)");
  QSqlQuery getAllocationData;
  prepare(getAllocationData,"select address_start, address_end, time_start, time_end from allocations where id = ?");
  QSqlQuery getIds;
  prepare(getIds,"select id from allocations");
  QSqlQuery updateSample;
  QSqlQuery selectLoad;
  prepare(selectLoad,"select id from selected_events where name like 'cpu/mem-loads%'");
  selectLoad.exec();
  QSqlQuery selectStore;
  prepare(selectStore,"select id from selected_events where name like 'cpu/mem-stores%'");
  selectStore.exec();
  long long loadId = -1;
  long long storeId = -1;
  if(selectLoad.next())
  {
    loadId = selectLoad.value(0).toLongLong();
  }
  else
  {
      //Loads must always be in db
      throw std::runtime_error("Event id not found");
  }
  if(selectStore.next())
  {
      storeId = selectStore.value(0).toLongLong();
  }
  prepare(updateSample,"update samples set allocation_id = ? where id = ?");
  QSqlQuery createTmpTable;
  prepare(createTmpTable,"create table samplesMem as select id, time, to_ip from samples where evsel_id in (?,?)");
  createTmpTable.bindValue(0,loadId);
  createTmpTable.bindValue(1,storeId);
  createTmpTable.exec();
  db.exec("create index idx_samplesMem_toIp on samplesMem(to_ip,time,id)"); // covering index, id must be last parameter
  QSqlQuery findUpdates;
  prepare(findUpdates,"select id from samplesMem where to_ip between ? and ? and time between ? and ?");
  getIds.exec();
  QVariantList allocIds;
  QVariantList sampleIds;
  while(getIds.next())
  {
    auto id = getIds.value(0).toLongLong();
    getAllocationData.bindValue(0,id);
    getAllocationData.exec();
    if(getAllocationData.next())
    {
      long long addressStart = getAllocationData.value(0).toLongLong();
      long long addressEnd = getAllocationData.value(1).toLongLong();
      long long timeStart = getAllocationData.value(2).toLongLong();
      long long timeEnd = getAllocationData.value(3).toLongLong();

      findUpdates.bindValue(0, addressStart);
      findUpdates.bindValue(1, addressEnd);
      findUpdates.bindValue(2, timeStart);
      findUpdates.bindValue(3, timeEnd);
      findUpdates.exec();
      while(findUpdates.next())
      {
          allocIds << id;
          sampleIds << findUpdates.value(0);
      }
    }
    if(getAllocationData.next())
    {
      throw(std::runtime_error("Duplicated id"));
    }
    getAllocationData.finish();
  }
  db.exec("BEGIN TRANSACTION");
  updateSample.addBindValue(allocIds);
  updateSample.addBindValue(sampleIds);
  updateSample.execBatch();

  prepare(updateSample,"update samples set allocation_id = 1 where allocation_id is NULL and evsel_id = (select id from selected_events where name like 'cpu/mem-loads%')");
  updateSample.exec();

  db.exec("END TRANSACTION");
  db.exec("drop table samplesMem");
  getIds.finish();
  updateSample.finish();
}

void createViews(QSqlDatabase& db)
{
  const QString minSamplesStr = "1";

  db.exec(QString("CREATE VIEW `snoop of samples` AS \
  select (select name from memory_snoop where id = memory_snoop) as `snoop`, \
  count(*)  as `count` from samples \
  group by memory_snoop order by count desc"));

  db.exec(QString("CREATE VIEW `functions with locked access` AS \
  select (select name from symbols where id = symbol_id) as `function`, \
  count(*)  as `count` from samples where memory_lock = \
  (select id from memory_lock where name = 'Locked') \
  group by symbol_id having count(*) >= " % minSamplesStr % " order by count desc"));

  db.exec(QString("CREATE VIEW `latency of allocations` AS \
  select allocation_id,   (address_end - address_start) / 1024 as `size [kb]`,\
  (select tid from threads where id = allocations.thread_id) as tid, \
  count(*) as numSamples, \
  sum(weight) as sumWeight, \
  cast (printf('%.2f', sum(weight)/count(*)) as float) as averageWeight, \
  cast (printf('%.2f', sum(weight)  *100 / (select cast( sum(weight)  as float)  from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%'))) as float)  as `Latency %`, \
  cast (printf('%.2f', avg(weight) / (select cast(avg(weight) as float) from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%'))) as float) as `latency contribution factor` \
  from samples left outer join allocations on samples.allocation_id = allocations.id \
  where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%') \
  group by allocation_id having count(*) >= " % minSamplesStr % " order by sumWeight desc"));

  db.exec(QString("CREATE VIEW IF NOT EXISTS `latency of allocation sites` AS \
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
  group by allocations.call_path_id having numSamples >= " % minSamplesStr % " order by sumWeight desc"));

  db.exec(QString("CREATE VIEW `latency of allocations and functions` AS select allocation_id, \
  (select name from symbols where id = symbol_id) as function, \
  cast ( printf('%.2f',avg(weight)) as float) as `latency`, \
  count(*) as `count`, \
  cast ( printf('%.2f',sum(weight)  *100 / (select cast( sum(weight)  as float)  from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%'))) as float) as `latency %`, \
  cast ( printf('%.2f',avg(weight) / (select cast(avg(weight) as float) from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%'))) as float) as `latency factor` \
  from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%') \
  group by allocation_id,function having count(*) >= " % minSamplesStr % " order by count desc"));

  db.exec(QString("CREATE VIEW `latency of functions` AS select symbol_id, \
  (select name from symbols where id = symbol_id) as function, \
  cast ( printf('%.2f',avg(weight)) as float) as `latency`, \
  count(*) as `count`, \
  cast ( printf('%.2f',sum(weight)  *100 / (select cast( sum(weight)  as float) from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%'))) as float)  as `latency %`, \
  cast ( printf('%.2f',avg(weight) / (select cast(avg(weight) as float) from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%') )) as float) as `latency factor` \
  from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%')  \
  group by symbol_id having count(*) >= " % minSamplesStr % " order by count desc"));

  db.exec(QString("CREATE VIEW `function profile` AS \
  select symbol_id, (select name from symbols where id = symbol_id) as `function`, \
  cast ( printf('%.2f',sum(period) * 100 / (select cast(sum(period) as float) from samples where evsel_id = \
  (select id from selected_events where name like 'cpu/cpu-cycles%'))) as float) as `execution time %` \
  from samples where evsel_id = \
  (select id from selected_events where name like 'cpu/cpu-cycles%') \
  group by symbol_id having count(*) >= " % minSamplesStr % " order by `execution time %` desc"));

  db.exec(QString("CREATE VIEW `IPC of functions` AS \
  select a.symbol_id, (select name from symbols where id = a.symbol_id) as 'function', \
  cast ( printf('%.2f',instructions/cast(cycles as float)) as float) as `IPC` from \
  (select symbol_id, \
  sum(period) as cycles from samples where evsel_id = \
  (select id from selected_events where name like 'cpu/cpu-cycles%') \
  group by symbol_id having count(*) >= " % minSamplesStr % ") a \
  inner join \
  (select symbol_id, \
  sum(period) as instructions from samples where evsel_id = \
  (select id from selected_events where name like 'cpu/instructions%') \
  group by symbol_id having count(*) >= " % minSamplesStr % ") b \
  on a.symbol_id = b.symbol_id \
  order by IPC asc"));

  db.exec(QString("create view 'functions all' as \
  select (select name from symbols where id = a.symbol_id) as `function`, `execution time %`, `IPC`,`latency`,`latency %`,`latency factor` from \
  'IPC of functions' a inner join 'function profile' b using (symbol_id) inner join 'latency of functions' c using (symbol_id) \
  order by 'execution time %' desc"));

  db.exec((QString("create view 'samples load' as \
  select * from samples where evsel_id = (select id from selected_events where name like 'cpu/mem-loads%')")));

}


void createMetadataTable(QSqlDatabase& db)
{
  db.exec(QString("Create table metadata ( \
  commandline varchar(1000), \
  samplerate int, \
  min_allocation_size int)"));
}

void fillMetadataTable(QSqlDatabase& db, const QString& cmdline, const int samplerate, const int minAllocationSize)
{

  QSqlQuery q("insert into metadata (commandline, samplerate, min_allocation_size) values (?,?,?)",db);
  q.bindValue(0,cmdline);
  q.bindValue(1,samplerate);
  q.bindValue(2,minAllocationSize);
  q.exec();
}

QHash<unsigned int, QList<unsigned int>> readCpuNodeMapping()
{

    QHash<unsigned int, QList<unsigned int>> mapping;
    QProcess* process = new QProcess;
    QString file = QString("sh -c \"numactl --hardware  ");
    process->start(file);
    process->waitForFinished(100);
    auto output = process->readAllStandardOutput();
    auto text = QString(output);
    QRegularExpression regexp("node (?<node>\\d) cpus:(?<cpus>( \\d+)+)");
    auto matchIterator = regexp.globalMatch(text);
    while(matchIterator.hasNext())
    {
        auto match = matchIterator.next();
        auto node = static_cast<unsigned int>(match.captured("node").toULong());
        auto cpuStr = match.captured("cpus");
        cpuStr.remove(0,1); // remove first leading space
        auto cpuStrList = cpuStr.split(" ");
        QList<unsigned int> cpuList;
        std::transform(cpuStrList.begin(),cpuStrList.end(),std::back_inserter(cpuList),[](QString cpuAsStr){return cpuAsStr.toULong();});
        mapping.insert(node,cpuList);
    }
    return mapping;
}


QHash<unsigned int, QList<unsigned int>> arcturusCpuNodeMapping()
{
    //Fixed mapping for testing purporse
    QList<unsigned int> node0 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};//, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65};
    QList<unsigned int> node1 = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43};//, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87};
    QHash<unsigned int, QList<unsigned int>> mapping;
    mapping.insert(0,node0);
    mapping.insert(1,node1);
    return mapping;
}

void writeCpuNodeMapping(const QHash<unsigned int, QList<unsigned int>>& mapping, QSqlDatabase& db)
{
    db.exec("DROP TABLE IF EXISTS cpuNodeMapping");
    db.exec("CREATE TABLE cpuNodeMapping ( \
    cpu INTEGER PRIMARY KEY UNIQUE, \
    node INTEGER)");

    QSqlQuery q("insert into cpuNodeMapping (cpu,node) values (?,?)");
    for(auto node : mapping.keys())
    {
        for(auto cpu : mapping[node])
        {
            q.bindValue(0,cpu);
            q.bindValue(1,node);
            q.exec();
        }
    }
}

int main(int argc, char *argv[])
{
  QString dbname;
  QCoreApplication a(argc, argv);
  QCoreApplication::setApplicationName("PerfMemPlus Viewer");
  QCoreApplication::setApplicationVersion("1.0");
  QCommandLineParser parser;
  parser.setApplicationDescription("Imports allocation tracker data and prepares database for queries");
  parser.addHelpOption();
  parser.addPositionalArgument("dbPath","Path to database file");
  QCommandLineOption samplerateOpt({"c","samplerate"},"Samplerate of the profiling run","samplerate");
  parser.addOption(samplerateOpt);
  QCommandLineOption minAllocationSizeOpt("a","Min allocation size","minAllocationSize");
  parser.addOption(minAllocationSizeOpt);
  QCommandLineOption cmdlineOpt("l","Command line used for profiling","commandline");
  parser.addOption(cmdlineOpt);
  QCommandLineOption allocationDataOpt("allocData","Directory where allocation data is stored","allocData","/tmp");
  parser.addOption(allocationDataOpt);
  QCommandLineOption dramBandwidthOpt("dramBandwidth","Process data for dram bandwidth");
  parser.addOption(dramBandwidthOpt);
  QCommandLineOption l1MissLatencyOpt("l1MissLatency","Process data for l1Miss latency");
  parser.addOption(l1MissLatencyOpt);

  parser.process(a);
  auto arguments = parser.positionalArguments();
  if(arguments.size() >= 1)
  {
    dbname = arguments.first();
  }
  else
  {
    std::cout << "Error: Database argument is missing";
    return 1;
  }
  auto allocationDataDir = parser.value(allocationDataOpt);
  auto cmdline = parser.value(cmdlineOpt);
  auto samplerate = parser.value(samplerateOpt).toInt();
  auto minAllocationSize = parser.value(minAllocationSizeOpt).toInt();
  auto dramBandwidth = parser.isSet(dramBandwidthOpt);
  auto l1MissLatency = parser.isSet(l1MissLatencyOpt);

  std::cout << getTime() << " Reading allocation data..." << std::endl;
  auto db = QSqlDatabase::addDatabase("QSQLITE");
  db.setDatabaseName(dbname);
  db.open();
  sqlitePerformanceSettings(db);
  createMetadataTable(db);
  fillMetadataTable(db,cmdline,samplerate,minAllocationSize);
  createAllocationsTable(db);
  createAllocationsSymbolsTable(db);
  createAllocationsCallpathTable(db);
  readAllocationTrackerFiles(allocationDataDir,db);
  std::cout << getTime() << " Reading files complete. Updating samples table..." << std::endl;
  modifySamplesTable();
  updateRelationshipKeys(db);
  createViews(db);

  std::cout << getTime() << " Update of samples table complete. Calculating counter metrics..." << std::endl;
  auto mapping = readCpuNodeMapping();
  //auto mapping = arcturusCpuNodeMapping(); // for test
  writeCpuNodeMapping(mapping,db);

  QList<QString> events;
  if(dramBandwidth == true)
  {
    events = {"offcore_response.all_reads.llc_miss.local_dram","offcore_response.all_reads.llc_miss.remote_dram"};
  }
  if(l1MissLatency == true)
  {
    events = {"l1d_pend_miss.pending","mem_load_uops_retired.l1_miss","mem_load_uops_retired.hit_lfb"};
  }
  if(l1MissLatency == true || dramBandwidth == true)
  {
    CounterAttributes ca(db);
    ca.setNodeMapping(mapping);
    ca.updateIntervals(events);
  }
  db.close();
  std::cout << getTime() << " Insert complete" << std::endl;
  return 0;
}

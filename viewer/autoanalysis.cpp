#include "autoanalysis.h"
#include <QtSql>
#include "sqlutils.h"
#include <QSettings>
#include <QMap>
#include <QStringBuilder>


AutoAnalysis::AutoAnalysis()
{
  loadSettings();
  createIndexes();
}

AutoAnalysis::~AutoAnalysis()
{
  saveSettings();
}

void AutoAnalysis::createIndexes() const
{
  QSqlQuery q;
  q.exec("create index if not exists idx_samples_symbol_id on samples(symbol_id)");
  q.exec("create index if not exists idx_samples_evsel_id on samples(evsel_id)");
  q.exec("create index if not exists idx_samples_evsel_id on samples(memory_level)");
  q.exec("create index if not exists idx_samples_evsel_id on samples(memory_snoop)");
  q.exec("create index if not exists idx_samples_allocation_id on samples(allocation_id)");
  q.exec("create index if not exists idx_allocations_call_path_id on allocations(call_path_id)");
  q.exec("create index if not exists idx_allocations_id on allocations(id)");
}

QList<AutoAnalysis::Result> AutoAnalysis::run(bool considerObjects)
{
 auto fncts = getOffendingFunctions();
 if(considerObjects)
 {
   auto list = analyzeFunctionsWithObjects(fncts);
   return list;
 }
 else
 {
   return QList<AutoAnalysis::Result>();
 }
}

QList<int> AutoAnalysis::getOffendingFunctions() const
{
  QList<int> functions;
  float execTimeThreshhold = 99;
  float minTimeContribution = 1;
  QSqlQuery q("select symbol_id, cast ( printf('%.2f',sum(period) * 100.0 / (select cast(sum(period) as float) \
    from samples where evsel_id = (select id from selected_events where name like 'cpu/cpu-cycles%'))) as float) as `execution time %` \
    from samples where evsel_id = (select id from selected_events where name like 'cpu/cpu-cycles%') \
    group by symbol_id having count(*) >= 5 order by `execution time %` desc");
  q.exec();
  float sumExecTime = 0;
  while(q.next() && sumExecTime < execTimeThreshhold)
  {
    if(q.value(1).toFloat() < minTimeContribution)
    {
      break;
    }
    functions.append(q.value(0).toInt());
    sumExecTime += q.value(1).toFloat();
  }
  return functions;
}

QList<int> AutoAnalysis::getMostAccessedObjectsOfFunction(const int symbolId) const
{
  QList<int> objects;
  float sampleCountLimit = 90.9;
  float sampleLatencyLimit = 99.9;
  unsigned int minLatency = 32;
  float minLatencyContribution = 0.1;
  QSqlQuery q("select allocations.call_path_id, 100.0 * count (*) / \
  (select count(*) from samples where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id = ? ), \
  avg(weight) as \"average latency\", \
  100.0 * sum(weight) / (select sum(weight) from samples where evsel_id = \
  (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id = ? ) as \"latency %\" \
  from samples inner join allocations on allocations.id = samples.allocation_id where \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id = ? \
  group by allocations.call_path_id order by \"latency %\" desc");
  q.bindValue(0,symbolId);
  q.bindValue(1,symbolId);
  q.bindValue(2,symbolId);
  q.exec();

  float sumSamples = 0;
  while(q.next() && sumSamples < sampleLatencyLimit)
  {
    auto latencyPercent = q.value(3).toFloat();
    auto latency = q.value(2);
    if(latencyPercent >= minLatencyContribution)
    {
      objects.append(q.value(0).toInt());
    }
    sumSamples += latencyPercent;
  }
  return objects;
}

QVariant AutoAnalysis::executeSingleResultQuery(QSqlQuery& q) const
{
  q.exec();
  if(q.next())
  {
    return q.value(0);
  }
  else
  {
    qDebug() << q.lastQuery();
    qDebug() << q.lastError().text();
    return QVariant();
  }
}

QString AutoAnalysis::executeSingleStringQuery(QSqlQuery& q) const
{
  auto r = executeSingleResultQuery(q);
  if(r.isNull())
  {
    return "";
  }
  else
  {
    return r.toString();
  }
}

float AutoAnalysis::executeSingleFloatQuery(QSqlQuery& q) const
{
  auto r = executeSingleResultQuery(q);
  if(r.isNull())
  {
    return 0;
  }
  else
  {
    return r.toFloat();
  }
}

unsigned int AutoAnalysis::executeSingleUnsignedIntQuery(QSqlQuery& q) const
{
  auto r = executeSingleResultQuery(q);
  if(r.isNull())
  {
    return 0;
  }
  else
  {
    return r.toUInt();
  }
}


QString AutoAnalysis::symbolIdToFuctionName(const int symbolId) const
{
  QSqlQuery q("select name from symbols where id = ?");
  q.bindValue(0,symbolId);
  return executeSingleStringQuery(q);
 }

QList<AutoAnalysis::Result> AutoAnalysis::analyzeFunctionsWithObjects(const QList<int>& functions) const
{
  QList<Result> results;
  for(auto item : functions)
  {
    auto objects = getMostAccessedObjectsOfFunction(item);
    for(auto object : objects)
    {
      auto r = analyzeFunctionAndObject(item,object);
      results.append(r);
    }
  }
  return results;
}

float AutoAnalysis::getExecTimePercent(const int symbolId) const
{
  QSqlQuery q("select \"execution time %\" from \"function profile\" where function = (select name from symbols where id = ?)");
  q.bindValue(0,symbolId);
  return executeSingleFloatQuery(q);
}

float AutoAnalysis::getObjectLatencyPercent(const int symbolId, const int allocation) const
{
  QSqlQuery q("select 100.0 * sum(weight) / (select sum(weight) from samples where evsel_id = \
  (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id = ?) as \"latency %\" \
  from samples inner join allocations on allocations.id = samples.allocation_id where \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id = ?\
  and allocations.call_path_id = ?");
  q.bindValue(0,symbolId);
  q.bindValue(1,symbolId);
  q.bindValue(2,allocation);
  return executeSingleFloatQuery(q);
}

AutoAnalysis::Result AutoAnalysis::analyzeFunctionAndObject(const int symbolId, const int allocation) const
{
  Result r;
  r.function = symbolIdToFuctionName(symbolId);
  r.callPathId = allocation;
  r.functionExectimePercent = getExecTimePercent(symbolId);
  r.objectLatencyPercent = getObjectLatencyPercent(symbolId,allocation);

  // check for hitm
  auto sumHitm = getNumberOfHitmInFunction(symbolId,allocation);
  auto sumAccess = getNumberOfAccesses(symbolId,allocation);
  auto hitmPercent = 100.0 * (float)sumHitm / (float)sumAccess;
  if(hitmPercent >= hitmLimit)
  {
    r.falseSharing = true;
    r.hitmCount = sumHitm;
    r.hitmPercent = hitmPercent;
  }

  // check for latency
  auto latencyDram = getLatencyInDram(symbolId,allocation);
  auto latencyLfb = getLatencyInLfb(symbolId,allocation);
  auto dramSampleCount = getDramSampleCount(symbolId,allocation);
  if(latencyDram >= dramLatencyLimit && dramSampleCount >= dramSampleCountLimit)
  {
    r.bandwidth = true;
    //r.latencyOverLimitPercent = ((latencyDram - dramLatencyLimit) * 100) / dramLatencyLimit;
    r.latencyOverLimitPercent = latencyDram;
  }

  // check for cache
  auto l1HitRate = getL1HitRate(symbolId,allocation);
  auto l2HitRate = getL2HitRate(symbolId,allocation);
  auto l3HitRate = getL3HitRate(symbolId,allocation);
  auto l2Latency = 12; // get from db
  auto l2DefaultLatency = 12; //default for broadwell
  auto latencyMargin = 1.2;
  auto l3Latency = 70;
  auto l3DefaultLatency = 70; //default for broadwell


  if(l1HitRate < cacheHitRateLimit && (l1HitRate + l2HitRate + cacheHitRateMargin) >= 100 && l2Latency < l2DefaultLatency * latencyMargin)
  {
    //L1 limit
    r.workingSetL1 = true;
    r.l1HitRate = l1HitRate;
  }
  if(l2HitRate < cacheHitRateLimit && (l3HitRate + l2HitRate + cacheHitRateMargin) >= 100 && l3Latency < l3DefaultLatency * latencyMargin)
  {
    //L2 limit
    r.workingSetL2 = true;
    r.l2HitRate = l2HitRate;
  }

  // add calls to other analyis functions
  return r;
}

unsigned int AutoAnalysis::getDramSampleCount(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select count(*) from samples inner join allocations on samples.allocation_id = allocations.id\
   where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
   symbol_id = ?" % allocationLimitString(allocationId) % " and \
   memory_level = (select id from memory_levels where name = \"Local DRAM\") ");
  q.bindValue(0,symbolId);
  return executeSingleUnsignedIntQuery(q);
}

float AutoAnalysis::getL1HitRate(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select 100 * accessInCache / allAccess as 'Hit Rate %' from \
   (select count(*) as accessInCache from samples inner join allocations on samples.allocation_id = allocations.id\
   where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
   symbol_id = ?" % allocationLimitString(allocationId) % " and \
   memory_level = (select id from memory_levels where name like \"%L1%\") ), \
   (select  count(*) as allAccess from samples inner join allocations on samples.allocation_id = allocations.id where \
   symbol_id = ?" % allocationLimitString(allocationId) % " and evsel_id = \
   (select id from selected_events where name like \"cpu/mem-loads%\"))");
  q.bindValue(0,symbolId);
  q.bindValue(1,symbolId);
  return executeSingleFloatQuery(q);
}

float AutoAnalysis::getL2HitRate(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select 100 * accessInCache / allAccess as 'Hit Rate %' from \
   (select count(*) as accessInCache from samples inner join allocations on samples.allocation_id = allocations.id\
   where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
   symbol_id = ?" % allocationLimitString(allocationId) % " and \
   memory_level = (select id from memory_levels where name like \"%L2%\") ), \
   (select  count(*) as allAccess from samples inner join allocations on samples.allocation_id = allocations.id where \
   symbol_id = ?" % allocationLimitString(allocationId) % " and evsel_id = \
   (select id from selected_events where name like \"cpu/mem-loads%\"))");
  q.bindValue(0,symbolId);
  q.bindValue(1,symbolId);
  return executeSingleFloatQuery(q);
}

float AutoAnalysis::getL3HitRate(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select 100 * accessInCache / allAccess as 'Hit Rate %' from \
   (select count(*) as accessInCache from samples inner join allocations on samples.allocation_id = allocations.id\
   where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
   symbol_id = ? " % allocationLimitString(allocationId) % " and \
   memory_level = (select id from memory_levels where name like \"%L3%\") ), \
   (select  count(*) as allAccess from samples inner join allocations on samples.allocation_id = allocations.id where \
   symbol_id = ? " % allocationLimitString(allocationId) % " and evsel_id = \
   (select id from selected_events where name like \"cpu/mem-loads%\"))");
  q.bindValue(0,symbolId);
  q.bindValue(1,symbolId);
  return executeSingleFloatQuery(q);
}

void AutoAnalysis::loadSettings(const QString &path)
{
  QSettings settings(path,QSettings::IniFormat);
  settings.beginGroup("AutoAnalysis");
  hitmLimit = settings.value("hitmLimit",2).toUInt();
  lfbLatencyLimit = settings.value("lfbLatencyLimit",290).toUInt();
  dramLatencyLimit = settings.value("dramLatencyLimit",466).toUInt();
  cacheHitRateLimit = settings.value("cacheHitRateLimit",80).toFloat();
  cacheHitRateMargin = settings.value("cacheHitRateMargin",10).toFloat();
  dramSampleCountLimit = settings.value("dramSampleCountLimit",10).toUInt();
  settings.endGroup();
}

void AutoAnalysis::saveSettings(const QString &path)
{
  QSettings settings(path,QSettings::IniFormat);
  settings.beginGroup("AutoAnalysis");
  settings.setValue("hitmLmit",hitmLimit);
  settings.setValue("lfbLatencyLimit",lfbLatencyLimit);
  settings.setValue("dramLatencyLimit",dramLatencyLimit);
  settings.setValue("cacheHitRateLimit",(unsigned int) cacheHitRateLimit);
  settings.setValue("cacheHitRateMargin",(unsigned int) cacheHitRateMargin);
  settings.setValue("dramSampleCountLimit",(unsigned int) dramSampleCountLimit);
  settings.endGroup();
  settings.sync();
}

unsigned int AutoAnalysis::getNumberOfHitmInFunction(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select symbol_id, \
  count (*) as \"HITM count\", \
  avg(weight) as \"HITM average latency\" \
  from samples inner join allocations on samples.allocation_id = allocations.id\
  where symbol_id = ? \
  and memory_snoop = (select id from memory_snoop where name = \"Snoop Hit Modified\") \
  " % allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleUnsignedIntQuery(q);
}

unsigned int AutoAnalysis::getLatencyInLfb(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id \
    where symbol_id = ? and \
    memory_level = (select id from memory_levels where name = \"LFB\")"
    + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleUnsignedIntQuery(q);
}

unsigned int AutoAnalysis::getNumberOfAccesses(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select count(*) from samples inner join allocations on samples.allocation_id = allocations.id \
    where symbol_id = ? and \
    evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\")"
    % allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleUnsignedIntQuery(q);
}

unsigned int AutoAnalysis::getLatencyInDram(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id\
    where symbol_id = ? and \
    memory_level = (select id from memory_levels where name = \"Local DRAM\") \
    " % allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleUnsignedIntQuery(q);
}

QString AutoAnalysis::allocationLimitString(const int allocationId) const
{
 if(allocationId == -1)
 {
   return "";
 }
 else
 {
   return " and allocations.call_path_id = " % QString::number(allocationId);
 }
}


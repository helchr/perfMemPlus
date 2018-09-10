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
  q.exec("create index if not exists idx_samples_to_ip on samples(to_ip)");
  q.exec("create index if not exists idx_allocations_call_path_id on allocations(call_path_id)");
  q.exec("create index if not exists idx_allocations_id on allocations(id)");
}

QList<Result> AutoAnalysis::run(bool considerObjects)
{
 auto fncts = getOffendingFunctions();
 if(considerObjects)
 {
   auto list = analyzeFunctionsWithObjects(fncts);
   return list;
 }
 else
 {
   return QList<Result>();
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
  float sampleLatencyLimit = 99.999;
  unsigned int minLatency = 32;
  float minLatencyContribution = 0.001;
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

QList<Result> AutoAnalysis::analyzeFunctionsWithObjects(const QList<int>& functions) const
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

Result AutoAnalysis::analyzeFunctionAndObject(const int symbolId, const int allocation) const
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
  if(sumHitm >= hitmLimit)
  {
    if(isObjectWrittenByMultipleThreads(allocation))
    {
      if(areAddressesInObjectShared(allocation))
      {
        r.trueSharing = true;
      }
      else
      {
        r.falseSharing = true;
      }
    }
    else
    {
      r.modifiedCachelines = true;
    }
    r.hitmCount = sumHitm;
    r.hitmPercent = hitmPercent;
  }

  auto levels = getHitmMemoryLevels(symbolId,allocation);
  auto nonHitmLatency = getNonHimLatency(symbolId,allocation,levels);
  auto hitmLatency = getHimLatency(symbolId,allocation);
  auto samplerate = getSamplerate();
  r.falseSharingLatencySavings = sumHitm * samplerate * (hitmLatency - nonHitmLatency);

  // check for latency
  auto latencyDram = getLatencyInDram(symbolId,allocation);
  auto latencyLfb = getLatencyInLfb(symbolId,allocation);
  auto dramSampleCount = getDramSampleCount(symbolId,allocation);

  //r.l1Bandwidth = checkBandwidth("L1",symbolId,allocation);
  //r.l2Bandwidth = checkBandwidth("L2",symbolId,allocation);
  //r.l3Bandwidth = checkBandwidth("L3",symbolId,allocation);
  //r.remoteCacheBandwidth = checkBandwidth("Remote Cache (1 hops)",symbolId,allocation);
  r.dramBandwidth = checkBandwidth("Local DRAM",symbolId,allocation);
  r.remoteDramBandwidth = checkBandwidth("Remote DRAM (1 hop)",symbolId,allocation);

  // check for cache
  /*
  auto l1HitRate = getL1HitRate(symbolId,allocation);
  auto l2HitRate = getL2HitRate(symbolId,allocation);
  auto l3HitRate = getL3HitRate(symbolId,allocation);
  auto l2Latency = 12; // get from db
  auto l2DefaultLatency = 12; //default for broadwell
  auto latencyMargin = 1.2;
  auto l3Latency = 70;
  auto l3DefaultLatency = 70; //default for broadwell
  */


  r.memoryStallCycles = getMemoryStallCycles(symbolId);
/*
  if(l1HitRate < cacheHitRateLimit && (l1HitRate + l2HitRate + cacheHitRateMargin) >= 100 && l2Latency < l2DefaultLatency * latencyMargin)
  {
    //L1 limit
    r.workingSetL1 = true;
    r.l1HitRate = l1HitRate;
  }

  r.l1LatencySavings = getL2HitCount(symbolId,allocation) * samplerate * (getL2Latency(symbolId, allocation) - getL1Latency(symbolId, allocation));


  if(l2HitRate < cacheHitRateLimit && (l3HitRate + l2HitRate + cacheHitRateMargin) >= 100 && l3Latency < l3DefaultLatency * latencyMargin)
  {
    //L2 limit
    r.workingSetL2 = true;
    r.l2HitRate = l2HitRate;
  }
  r.l2LatencySavings = getL3HitCount(symbolId,allocation) * samplerate * (getL3Latency(symbolId, allocation) - getL2Latency(symbolId, allocation));
  */

  //auto totalLatency = getTotalLatency(symbolId);
  //auto memStallCycles = getMemStallCycles(symbolId);

  // add calls to other analyis functions
  return r;
}

BandwidthResult AutoAnalysis::checkBandwidth(const QString& memory, const int symbolId, const int allocationId) const
{
  BandwidthResult r;
  r.memory = memory;
  auto sampleCount = getSampleCountInMemory(symbolId,allocationId,memory);
  auto latency = getLatencyInMemory(symbolId,allocationId,memory);
  if(latency > getLatencyLimit(memory) )
  {
    r.problem = true;
    r.latency = latency;
    if(sampleCount <= sampleCountLimit)
    {
      r.lowSampleCount = true;
    }
  }
  r.latencySavings = getSamplerate() * sampleCount * (latency - getLatencyLimit(memory));
  return r;
}

float AutoAnalysis::getLatencyLimit(const QString& memory) const
{
 return latencyLimit[memory];
}

unsigned int AutoAnalysis::getSampleCountInMemory(const int symbolId, const int allocationId, const QString& memory) const
{
  QSqlQuery q("select count(*) from samples inner join allocations on samples.allocation_id = allocations.id\
   where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
   symbol_id = ?" % allocationLimitString(allocationId) % " and \
   memory_level = (select id from memory_levels where name = ?)");
  q.bindValue(0,symbolId);
  q.bindValue(1,memory);
  return executeSingleUnsignedIntQuery(q);
}

float AutoAnalysis::getLatencyInMemory(const int symbolId, const int allocationId, const QString& memory) const
{
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
  memory_level = (select id from memory_levels where name like ?)" + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  q.bindValue(1,memory);
  return executeSingleFloatQuery(q);
}

unsigned int AutoAnalysis::getSamplerate() const
{
  QSqlQuery q("select samplerate from metadata");
  return executeSingleUnsignedIntQuery(q);
}

QStringList AutoAnalysis::getHitmMemoryLevels(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select distinct(memory_level) from samples inner join allocations on samples.allocation_id = allocations.id \
  where memory_snoop = (select id from memory_snoop where name = \"Snoop Hit Modified\") and \
  symbol_id = ?" + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  q.exec();
  QStringList mems;
  while(q.next())
  {
    mems.append(q.value(0).toString());
  }
  return mems;
}

float AutoAnalysis::getL1Latency(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
  memory_level = (select id from memory_levels where name like \"%L1%\")" + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleFloatQuery(q);
}

bool AutoAnalysis::isObjectWrittenByMultipleThreads(const int allocationId) const
{
  QSqlQuery q("select count(distinct samples.thread_id)from samples inner join allocations on samples.allocation_id = allocations.id where memory_opcode = \
  (select id from memory_opcodes where name = \"Store\")" + allocationLimitString(allocationId));
  auto threads = executeSingleUnsignedIntQuery(q);
  if(threads > 1)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool AutoAnalysis::areAddressesInObjectShared(const int allocationId) const
{
  QSqlQuery q("select count(distinct samples.thread_id) as numThreads from samples where \
  memory_opcode = (select id from memory_opcodes where name = \"Store\") \
  and allocation_id in (select id from allocations where call_path_id = ? ) \
  and to_ip in \
  (select distinct to_ip from samples inner join allocations on samples.allocation_id = allocations.id where \
  memory_snoop = (select id from memory_snoop where name = \"Snoop Hit Modified\") and allocations.call_path_id = ?) \
  group by allocation_id,to_Ip \
  having count(to_ip) > 1"); //to include only addresses which have 2 or more accesses to an address
  q.bindValue(0,allocationId);
  q.bindValue(1,allocationId);
  q.exec();
  unsigned int oneThread = 0;
  unsigned int moreThreads = 0;
  while (q.next())
  {
    auto value = q.value(0).toUInt();
    if(value == 1)
    {
      oneThread++;
    }
    else
    {
      moreThreads++;
    }
  }
  if((float)oneThread / (float)(oneThread + moreThreads) > 0.5)
  {
    return false;
  }
  else
  {
    return true;
  }
}

float AutoAnalysis::getL2Latency(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
  memory_level = (select id from memory_levels where name like \"%L2%\")" + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleFloatQuery(q);
}

float AutoAnalysis::getL3Latency(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
  memory_level = (select id from memory_levels where name like \"%L3%\")" + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleFloatQuery(q);
}

unsigned int AutoAnalysis::getL2HitCount(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select count(*)from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
  memory_level = (select id from memory_levels where name like \"%L2%\") \
  and to_ip/64 in (select to_ip/64 from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = symbol_id and memory_level = (select id from memory_levels where name like \"%L1%\") \
  "+ allocationLimitString(allocationId) + ") \
  " + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleUnsignedIntQuery(q);
}

unsigned int AutoAnalysis::getL3HitCount(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select count(*) from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
  memory_level = (select id from memory_levels where name like \"%L3%\")  and to_ip/64 in (select to_ip/64 from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = symbol_id and memory_level = (select id from memory_levels where name like \"%L2%\") \
  "+ allocationLimitString(allocationId) + ") " + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleUnsignedIntQuery(q);
}

unsigned int AutoAnalysis::getDramHitCount(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select count(*) from samples inner join allocations on samples.allocation_id = allocations.id\
  where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
  symbol_id = ?" % allocationLimitString(allocationId) % " and \
  memory_level = (select id from memory_levels where name = \"Local DRAM\") ");
  q.bindValue(0,symbolId);
  return executeSingleUnsignedIntQuery(q);
}


float AutoAnalysis::getNonHimLatency(const int symbolId, const int allocationId, const QStringList &memories) const
{
  QStringList s(memories);
  auto str = s.join(",");
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and memory_snoop != \
  (select id from memory_snoop where name = \"Snoop Hit Modified\") and evsel_id = \
  (select id from selected_events where name like \"cpu/mem-loads%\") and memory_level in ("+str+")" + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleFloatQuery(q);
}

float AutoAnalysis::getHimLatency(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id\
  where symbol_id = ? and memory_snoop = \
  (select id from memory_snoop where name = \"Snoop Hit Modified\") and evsel_id = \
  (select id from selected_events where name like \"cpu/mem-loads%\")" + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return executeSingleFloatQuery(q);
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
  hitmLimit = settings.value("hitmLimit",1).toUInt();
  lfbLatencyLimit = settings.value("lfbLatencyLimit",290).toUInt();
  dramLatencyLimit = settings.value("dramLatencyLimit",466).toUInt();
  cacheHitRateLimit = settings.value("cacheHitRateLimit",80).toFloat();
  cacheHitRateMargin = settings.value("cacheHitRateMargin",10).toFloat();
  sampleCountLimit = settings.value("sampleCountLimit",10).toUInt();
  latencyLimit["L1"] = settings.value("l1LatencyLimit",0).toUInt();
  latencyLimit["L2"] = settings.value("l2LatencyLimit",0).toUInt();
  latencyLimit["L3"] = settings.value("l3LatencyLimit",0).toUInt();
  latencyLimit["Local DRAM"] = settings.value("dramLatencyLimit",0).toUInt();
  latencyLimit["Remote DRAM (1 hop)"] = settings.value("remoteDramLatencyLimit",0).toUInt();
  latencyLimit["Remote Cache (1 hops)"] = settings.value("remoteCacheLatencyLimit",0).toUInt();
  settings.endGroup();
}

void AutoAnalysis::saveSettings(const QString &path)
{
  QSettings settings(path,QSettings::IniFormat);
  settings.beginGroup("AutoAnalysis");
  settings.setValue("hitmLimit",hitmLimit);
  settings.setValue("lfbLatencyLimit",lfbLatencyLimit);
  settings.setValue("dramLatencyLimit",dramLatencyLimit);
  settings.setValue("cacheHitRateLimit",(unsigned int) cacheHitRateLimit);
  settings.setValue("cacheHitRateMargin",(unsigned int) cacheHitRateMargin);
  settings.setValue("sampleCountLimit",(unsigned int) sampleCountLimit);
  settings.setValue("l1LatencyLimit",(unsigned int) latencyLimit["L1"]);
  settings.setValue("l2LatencyLimit",(unsigned int) latencyLimit["L2"]);
  settings.setValue("l3LatencyLimit",(unsigned int) latencyLimit["L3"]);
  settings.setValue("dramLatencyLimit",(unsigned int) latencyLimit["Local DRAM"]);
  settings.setValue("remoteDramLatencyLimit",(unsigned int) latencyLimit["Remote DRAM (1 hop)"]);
  settings.setValue("remoteCacheLatencyLimit",(unsigned int) latencyLimit["Remote Cache (1 hops)"]);
  settings.endGroup();
  settings.sync();
}

unsigned int AutoAnalysis::getNumberOfHitmInFunction(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select count (*) as \"HITM count\" \
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

float AutoAnalysis::getMemoryStallCycles(const int symbolId) const
{
  QSqlQuery q("select sum(period) from samples where symbol_id = ? and evsel_id =\
  (select id from selected_events where name = \"cycle_activity_stalls_mem_any\")");
  q.bindValue(0,symbolId);
  return executeSingleFloatQuery(q);
}

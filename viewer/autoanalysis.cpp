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
}

void AutoAnalysis::createIndexes() const
{
  QSqlQuery q;
  q.exec("create index if not exists idx_allocations_call_path_id on allocations(call_path_id)");
  q.exec("create index if not exists idx_allocations_id on allocations(id)");
  q.exec("create index if not exists ids_samples_evsel_id_symbol_id on samples(evsel_id,symbol_id)");
  q.exec("create index if not exists ids_samples_evsel_id_symbol_id_cpu on samples(evsel_id,symbol_id,cpu)");
  q.exec("create index if not exists ids_samples_symbol_id_memory_snoop on samples(symbol_id,memory_snoop)");
  q.exec("create index if not exists idxLatencyQuery2 on samples(evsel_id, symbol_id,memory_level,memory_dtlb_hit_miss,memory_lock)");
  q.exec("create table if not exists samplesForFs as \
  select time / (1000*1000) as t_ms, to_ip, to_ip/64 as cl, thread_id, memory_snoop, memory_opcode, allocation_id, ip, symbol_id from samples \
  where memory_opcode in (2,4)");
  q.exec("create index if not exists idxSamplesForFs on samplesForFs(symbol_id,allocation_id,memory_snoop,cl)");
}

QList<Result> AutoAnalysis::runDetection(std::function<Result(const int, const int,const Result& r)> analyze)
{
 QList<Result> results;
 //QElapsedTimer t;
 //t.start();
 auto functions = getOffendingFunctions();
 //qDebug() << "functions" << t.elapsed();
 for(auto f : functions)
 {
    //t.restart();
    auto objects = getMostAccessedObjectsOfFunction(f);
    //qDebug() << "objects" << t.elapsed();
    for(auto o : objects)
    {
      Result rIn;
      auto r = analyze(f,o,rIn);
      results.append(r);
    }
  }
  return results;
}

QList<Result> AutoAnalysis::runFalseSharingDetection()
{
 auto f = [this](auto f, auto o, auto rIn){return detectFalseSharing(f,o,rIn);};
 return runDetection(f);
}

QList<Result> AutoAnalysis::runDramContentionDetection()
{
 auto f = [this](const int f, const int o, const Result& rIn){return detectDramContention(f,o,rIn);};
 return runDetection(f);
}

QList<Result> AutoAnalysis::run()
{
  auto f = [this](auto f, auto o, auto rIn){return detectFalseSharing(f,o,detectDramContention(f,o,rIn));};
  return runDetection(f);
}

QList<int> AutoAnalysis::getOffendingFunctions() const
{
  QList<int> functions;
  float execTimeThreshhold = 99;
  float minTimeContribution = 0.01f;
  QSqlQuery q("select symbol_id, cast( sum(period) as float) as t \
              from samples where evsel_id = (select id from selected_events where name like 'cpu/cpu-cycles%') \
              group by symbol_id having count(*) >= 5 order by t desc");
  q.exec();
  QVector<int> ids;
  QVector<float> times;
  float totalTime = 0;
  while(q.next())
  {
      ids.append(q.value(0).toInt());
      times.append(q.value(1).toFloat());
      totalTime+=q.value(1).toFloat();
  }
  for(auto&l : times)
  {
      l = l / totalTime;
  }
  float sumExecTime = 0;
  int i = 0;
  while(i < ids.size() && sumExecTime < execTimeThreshhold)
  {
    if(times[i] < minTimeContribution)
    {
       break;
    }
    functions.append(ids[i]);
    sumExecTime += times[i];
    i++;
  }
  return functions;
}

QList<int> AutoAnalysis::getMostAccessedObjectsOfFunction(const int symbolId) const
{
  QList<int> objects;
  float sampleLatencyLimit = 99.999f;
  float minLatencyContribution = 0.001f;
  QSqlQuery q("select allocations.call_path_id, \
   sum(weight) as \"latency %\" \
  from \"samples load\" inner join allocations on allocations.id = \"samples load\".allocation_id where \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") \
  and symbol_id = ? \
  group by allocations.call_path_id order by \"latency %\" desc");
  q.bindValue(0,symbolId);
  q.exec();

  QVector<int> ids;
  QVector<float> lat;
  float totalLat = 0;
  while(q.next())
  {
      ids.append(q.value(0).toInt());
      lat.append(q.value(1).toFloat());
      totalLat+=q.value(1).toFloat();
  }
  for(auto&l : lat)
  {
      l = l / totalLat;
  }
  float sumSamples = 0;
  int i = 0;
  while(i < ids.size() && sumSamples < sampleLatencyLimit)
  {
    auto latencyPercent = lat[i];
    if(latencyPercent >= minLatencyContribution)
    {
      objects.append(ids[i]);
    }
    sumSamples += latencyPercent;
    i++;
  }
  return objects;
}

QString AutoAnalysis::symbolIdToFuctionName(const int symbolId) const
{
  QSqlQuery q("select name from symbols where id = ?");
  q.bindValue(0,symbolId);
  return SqlUtils::executeSingleStringQuery(q);
 }

Result AutoAnalysis::detectFalseSharing(const int symbolId, const int allocation, const Result& rIn) const
{
  Result result = rIn;
  result.function = symbolIdToFuctionName(symbolId);
  result.callPathId = allocation;

  auto sumHitm = getNumberOfHitmInFunction(symbolId,allocation);
  if(sumHitm >= hitmLimit)
  {
      QSqlQuery q("select distinct cl from samplesForFs where \
      allocation_id in (select id from allocations where call_path_id = ?) and symbol_id = ?");
      QSqlQuery clData("select t_ms, to_ip, thread_id, memory_opcode, allocation_id, ip from samplesForFs where cl = ? \
                       and allocation_id in (select id from allocations where call_path_id = ?) and symbol_id = ?");
     q.bindValue(0,allocation);
     q.bindValue(1,symbolId);
     q.exec();
     if(q.lastError().isValid())
      {
          qDebug() << q.lastError().text();
      }
      while (q.next())
      {
          const int MAX_DETECT = 1000;
          QVector<ClAccess> writer;
          writer.reserve(MAX_DETECT);
          QVector<ClAccess> reader;
          reader.reserve(MAX_DETECT);
          auto cLine = q.value(0).toLongLong();
          clData.bindValue(0,cLine);
          clData.bindValue(1,allocation);
          clData.bindValue(2,symbolId);
          clData.exec();
          if(clData.lastError().isValid())
          {
              qDebug() << clData.lastError().text();
          }
          while(clData.next())
          {
              ClAccess c;
              c.time = clData.value(0).toLongLong();
              c.adr = clData.value(1).toLongLong();
              c.tid = clData.value(2).toInt();
              auto opcode = clData.value(3).toInt();
              c.allocationId = clData.value(4).toInt();
              c.ip = clData.value(5).toLongLong();
              if(opcode == 2)
              {
                  reader.append(c);
              }
              else if(opcode == 4)
              {
                  writer.append(c);
              }
          }
          // can be slow for many readers or writers. For speed break after one is found
          for(auto w : writer)
          {
            if(result.intraObjectTrueSharing.count() >= MAX_DETECT || result.interObjectTrueSharing.count() >= MAX_DETECT ||
               result.intraObjectFalseSharing.count() >= MAX_DETECT || result.interObjectFalseSharing.count() >= MAX_DETECT)
            {
                break;
            }
              for(auto r : reader)
              {
                  // read/write or write/read dependency
                  if (r.tid != w.tid)
                  {
                      // different threads, there is sharing
                      auto timeDiff = llabs(r.time - w.time);
                      if(timeDiff < 5)
                      {
                          auto pair = qMakePair(w,r);
                          if(r.allocationId == w.allocationId)
                          {
                            if(r.adr == w.adr)
                            {
                              if(!result.intraObjectTrueSharing.contains(pair))
                              {
                                result.intraObjectTrueSharing.append(pair);
                              }
                            }
                            else
                            {
                              if(!result.intraObjectFalseSharing.contains(pair))
                                {
                                result.intraObjectFalseSharing.append(pair);
                              }
                            }
                          }
                          else // same call_path_id but different allocationId
                          {
                            if(r.adr == w.adr)
                            {
                              if(!result.interObjectTrueSharing.contains(pair))
                              {
                                result.interObjectTrueSharing.append(pair);
                              }
                            }
                            else
                            {
                              if(!result.interObjectFalseSharing.contains(pair))
                              {
                                result.interObjectFalseSharing.append(pair);
                              }
                            }
                          }
                      }
                  }
              }
              for(auto w1 : writer)
              {
                  if (w1.tid != w.tid)
                  {
                      // different threads, there is sharing
                      auto timeDiff = llabs(w1.time - w.time);
                      if(timeDiff < 5)
                      {
                          // within short time range
                          auto pair = qMakePair(w,w1);
                          if(w1.allocationId == w.allocationId)
                          {
                          if(w1.adr == w.adr)
                          {
                              if(!result.intraObjectTrueSharing.contains(pair))
                              {
                              result.intraObjectTrueSharing.append(pair);
                              }
                          }
                          else
                          {
                              if(!result.intraObjectFalseSharing.contains(pair))
                              {
                              result.intraObjectFalseSharing.append(pair);
                              }
                          }
                          }
                          else {
                              if(w1.adr == w.adr)
                              {
                                  if(!result.interObjectTrueSharing.contains(pair))
                                  {
                                  result.interObjectTrueSharing.append(pair);
                                  }
                              }
                              {
                                  if(!result.interObjectFalseSharing.contains(pair))
                                  {
                                  result.interObjectFalseSharing.append(pair);
                                  }
                              }
                          }
                      }
                  }
              }
          }
      }

      if(result.intraObjectFalseSharing.count() > 2 || result.interObjectFalseSharing.count() > 2)
      {
        result.falseSharing = true;
      }
      if(result.intraObjectTrueSharing.count() > 2 || result.interObjectTrueSharing.count() > 2)
      {
        result.trueSharing = true;
      }
  }
  return result;
}

Result AutoAnalysis::detectDramContention(const int symbolId, const int allocation, const Result& rIn) const
{
  Result result = rIn;
  result.function = symbolIdToFuctionName(symbolId);
  result.callPathId = allocation;
  result.dramBandwidth = checkBandwidth("Local DRAM",symbolId,allocation);
  result.remoteDramBandwidth = checkBandwidth("Remote DRAM (1 hop)",symbolId,allocation);
  return result;
}

BandwidthResult AutoAnalysis::checkBandwidth(const QString& memory, const int symbolId, const int allocationId) const
{
  BandwidthResult r;
  r.memory = memory;
  //QElapsedTimer t;
  //t.start();
  auto latency = getLatencyInMemory(symbolId,allocationId,memory);
  //qDebug() << "latency" << t.elapsed();
  r.latency = latency;
  if(latency > getLatencyLimit(memory) )
  {
    auto sampleCountTotal = getSampleCount(symbolId,allocationId);
    auto sampleCountDram = getSampleCountInMemory(symbolId,allocationId,memory);
    auto sampleCountLfb = getSampleCountInMemory(symbolId,allocationId,"LFB");
    auto hitRate = (float(sampleCountLfb) + float(sampleCountDram)) / (sampleCountTotal);
    if(hitRate > 0.05f)
    {
        r.problem = true;
        r.latency = latency;
        //t.restart();
        //qDebug() << "sampleCount" << t.elapsed();
        if(sampleCountDram <= sampleCountLimit)
        {
          r.lowSampleCount = true;
        }
        r.parallelReaders = getNumberOfParallelReaders(symbolId,allocationId);
    }
  }
  return r;
}

float AutoAnalysis::getLatencyLimit(const QString& memory) const
{
 return latencyLimit[memory];
}

unsigned int AutoAnalysis::getSampleCount(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select count(*) from samples inner join allocations on samples.allocation_id = allocations.id\
   where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
   symbol_id = ?" % allocationLimitString(allocationId) % " and \
   memory_lock != 2");
  q.bindValue(0,symbolId);
  return SqlUtils::executeSingleUnsignedIntQuery(q);
}

unsigned int AutoAnalysis::getSampleCountInMemory(const int symbolId, const int allocationId, const QString& memory) const
{
  QSqlQuery q("select count(*) from samples inner join allocations on samples.allocation_id = allocations.id\
   where evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
   symbol_id = ?" % allocationLimitString(allocationId) % " and \
   memory_level = (select id from memory_levels where name = ?) and \
   memory_lock != 2");
  q.bindValue(0,symbolId);
  q.bindValue(1,memory);
  return SqlUtils::executeSingleUnsignedIntQuery(q);
}

float AutoAnalysis::getLatencyInMemory(const int symbolId, const int allocationId, const QString& memory) const
{
  QSqlQuery q("select avg(weight) from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and \
  evsel_id = (select id from selected_events where name like \"cpu/mem-loads%\") and \
  memory_level = (select id from memory_levels where name like ?) and \
  memory_dtlb_hit_miss = (select id from memory_dtlb_hit_miss where name = \"Hit\") \
  and memory_lock != 2"
  + allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  q.bindValue(1,memory);
  return SqlUtils::executeSingleFloatQuery(q);
}

void AutoAnalysis::loadSettings(const QString &path)
{
  QSettings settings(path,QSettings::IniFormat);
  settings.beginGroup("AutoAnalysis");
  hitmLimit = settings.value("hitmLimit",1).toUInt();
  sampleCountLimit = settings.value("sampleCountLimit",10).toUInt();
  latencyLimit["L1"] = settings.value("l1LatencyLimit",0).toUInt();
  latencyLimit["L2"] = settings.value("l2LatencyLimit",0).toUInt();
  latencyLimit["L3"] = settings.value("l3LatencyLimit",0).toUInt();
  latencyLimit["Local DRAM"] = settings.value("dramLatencyLimit",0).toUInt();
  latencyLimit["Remote DRAM (1 hop)"] = settings.value("remoteDramLatencyLimit",0).toUInt();
  latencyLimit["Remote Cache (1 hops)"] = settings.value("remoteCacheLatencyLimit",0).toUInt();
  settings.endGroup();
}

unsigned int AutoAnalysis::getNumberOfHitmInFunction(const int symbolId, const int allocationId) const
{
  QSqlQuery q("select count (*) as \"HITM count\" \
  from samples inner join allocations on samples.allocation_id = allocations.id\
  where symbol_id = ? \
  and memory_snoop = (select id from memory_snoop where name = \"Snoop Hit Modified\") \
  " % allocationLimitString(allocationId));
  q.bindValue(0,symbolId);
  return SqlUtils::executeSingleUnsignedIntQuery(q);
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

float AutoAnalysis::getAvgTimeBetweenMemoryReadSamples(const int symbolId, const int allocationId) const
{
    QSqlQuery qTid("select distinct samples.cpu from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and evsel_id =\
                (select id from selected_events where name like \"%cpu/mem-loads%\") "
                % allocationLimitString(allocationId));
    qTid.bindValue(0,symbolId);
    qTid.exec();
    QVector<int> cpus;
    while(qTid.next())
    {
        cpus << qTid.value(0).toInt();
    }
    if(cpus.empty())
    {
        qDebug() << qTid.lastError();
        qDebug() << qTid.lastQuery();
    }
    QSqlQuery qTime("select min(time),max(time),count(*) from samples inner join allocations on samples.allocation_id = allocations.id where symbol_id = ? and samples.cpu = ? and evsel_id = \
                   (select id from selected_events where name like \"%cpu/mem-loads%\") "
                    % allocationLimitString(allocationId));
    QVector<float> avgDiffs;
    for(auto t : cpus)
    {
        qTime.bindValue(0,symbolId);
        qTime.bindValue(1,t);
        qTime.exec();
        if(qTime.next())
        {
            auto diff = qTime.value(1).toULongLong() - qTime.value(0).toULongLong();
            avgDiffs << diff / qTime.value(2).toFloat();
        }
    }
    auto sum = std::accumulate(avgDiffs.begin(),avgDiffs.end(),0.0f);
    return sum / avgDiffs.count();
}

float AutoAnalysis::getNumberOfParallelReaders(const int symbolId, const int allocationId) const
{
    /* Context is selected based on function and object.
     * Correct would be to use the time frame in which this funktion and object is active and consider all concurrently happening actions. */
    auto timeUnit = getAvgTimeBetweenMemoryReadSamples(symbolId,allocationId);
    QSqlQuery q("select avg(c) from ( \
                select time / " % QString::number(timeUnit,'f',0)  % " as t, count(distinct samples.cpu) as c from samples inner join allocations on samples.allocation_id = allocations.id \
                where symbol_id = ? and evsel_id = \
                (select id from selected_events where name like \"%cpu/mem-loads%\") "
                 % allocationLimitString(allocationId) % " group by t)");
   q.bindValue(0,symbolId);
   q.exec();
   if(q.next())
   {
       return q.value(0).toFloat();
   }
   else
   {
       qDebug() << q.lastError();
       return 0;
   }
}

#include "counterattributes.h"
#include "iostream"
#include <QtConcurrent/QtConcurrentRun>

CounterAttributes::CounterAttributes(QSqlDatabase &db)
{
    this->db = db;
}

void CounterAttributes::setNodeMapping(const QHash<unsigned int, QList<unsigned int> > &mapping)
{
   this->nodeToCpus = mapping;
}

QStringList CounterAttributes::createFlatEventList(const EventList& eventGroupList) const
{
    QStringList l;
    {
        for(auto eventPair : eventGroupList)
        {
            l.append(eventPair);
        }
    }
    return l;
}

CounterAttributes::EventAttribute CounterAttributes::getEventAttributes(const QString &event) const
{
    EventAttribute eAttr;
    if(event.startsWith("offcore",Qt::CaseInsensitive))
    {
        eAttr.offcore = true;
    }
    return eAttr;
}

CounterAttributes::EventAttribute CounterAttributes::getEventAttributes(const unsigned long long event) const
{
   auto name = eventIdToName[event];
   return getEventAttributes(name);
}

CounterAttributes::EventAttribute CounterAttributes::getEventAttributes(const QList<unsigned long long>& eventList ) const
{
    auto eAttrPrev = getEventAttributes(eventList.first());
    for(auto e : eventList)
    {
        auto eAttr = getEventAttributes(e);
        if(eAttr != eAttrPrev)
        {
            throw "Unmatching event attributes in group";
        }
        eAttrPrev = eAttr;
    }
    return eAttrPrev;
}
CounterAttributes::EventAttribute CounterAttributes::getEventAttributes(const QList<QPair<QString,bool>>& eventList ) const
{
    auto eAttrPrev = getEventAttributes(eventList.first().first);
    for(auto e : eventList)
    {
        auto eAttr = getEventAttributes(e.first);
        if(eAttr != eAttrPrev)
        {
            throw "Unmatching event attributes in group";
        }
        eAttrPrev = eAttr;
    }
    return eAttrPrev;
}

void CounterAttributes::createEventIdMap(const EventList& consideredEvents)
{
    QSqlQuery q("select id from selected_events where name like ? ");
    auto l = createFlatEventList(consideredEvents);
    for(auto eventName : l)
    {

        q.bindValue(0,"cpu/"+eventName+"%");
        if(q.exec() && q.next())
        {
            auto id = q.value(0).toULongLong();
            eventIdToName.insert(id,eventName);
            eventNameToId.insert(eventName,id);
        }
    }
}

void CounterAttributes::createTable(EventList events)
{
    for(auto& event : events)
    {
        event.prepend("'");
        event.append("'");
    }
    events += {"l1HitRate","lfbHitRate","l2HitRate","l3HitRate","localDramHitRate"};
    auto columnStr = events.join(" REAL, ");
    QString createStatement = "create table counterSamples ( \
            id INTEGER PRIMARY KEY UNIQUE, ";
    createStatement += columnStr + ")";

    db.exec("drop table if exists counterSamples");
    if(db.lastError().isValid())
    {
        throw(db.lastError().text());
    }
    db.exec(createStatement);
    if(db.lastError().isValid())
    {
        throw(db.lastError().text() + createStatement);
    }
}

void CounterAttributes::createMetricViews(const EventList& list)
{
   db.exec("drop view if exists counterMetrics");
   QString createStatement = "create view counterMetrics as ";
   bool execute = false;
   if(list.contains("l1d_pend_miss.pending") && list.contains("mem_load_uops_retired.l1_miss") && list.contains("mem_load_uops_retired.hit_lfb"))
   {
       createStatement+= "select id, \"l1d_pend_miss.pending\" / (\"mem_load_uops_retired.l1_miss\" + \"mem_load_uops_retired.hit_lfb\") as 'loadMissRealLatency',";
       createStatement+= " ((\"l1d_pend_miss.pending\" / (\"mem_load_uops_retired.l1_miss\" + \"mem_load_uops_retired.hit_lfb\")) - l2HitRate * 12 - l3HitRate * 33) / (localDramHitRate) as 'loadMissRealDramLatency'";
       createStatement+= " from counterSamples";
       execute = true;
   }
   else if(list.contains("l1d_pend_miss.pending") && list.contains("mem_load_uops_retired.l1_miss") && !list.contains("mem_load_uops_retired.hit_lfb"))
   {
       createStatement+= "select id, \"l1d_pend_miss.pending\" / \"mem_load_uops_retired.l1_miss\" as 'l1MissLatency' from counterSamples";
       execute = true;
   }
   else if(list.contains("offcore_response.all_reads.llc_miss.local_dram") && list.contains("offcore_response.all_reads.llc_miss.remote_dram"))
   {
      createStatement+= "select id, \"offcore_response.all_reads.llc_miss.local_dram\" * 64 / 1000 as 'localDramBandwidth', \"offcore_response.all_reads.llc_miss.remote_dram\" * 64 / 1000 as 'remoteDramBandwidth' from counterSamples";
      execute = true;
   }
   if(execute)
   {
     db.exec(createStatement);
     if(db.lastError().isValid())
     {
       throw(db.lastError().text());
     }
   }
}

template <class T>
QString CounterAttributes::listToString(const QList<T>& list) const
{
    QStringList stringList;
    std::transform(list.begin(),list.end(),std::back_inserter(stringList),[](unsigned long long i){return QString::number(i);});
    auto str = stringList.join(",");
    return str;
}

void CounterAttributes::createIndexes()
{
   db.exec("create index if not exists samples_time_cpu_threadId_evselId on samples(time,cpu,thread_id,evsel_id,period)");
   db.commit();
}

void CounterAttributes::writeSampleIds()
{
    db.transaction();
    QSqlQuery q("insert into counterSamples (id) \
        select id from samples where evsel_id = \
        (select id from selected_events where name like '%mem-loads%') ",db);
    q.exec();
    db.commit();
}

void CounterAttributes::updateCounterSamples(unsigned long long eventId, const QList<unsigned int>& cpu, unsigned long long startTime, unsigned long long endTime, double value)
{
    auto eventName = eventIdToName[eventId];
    auto cpuStr = listToString(cpu);
    QSqlQuery q(db);
    q.prepare("update counterSamples set '" + eventName + "' = ? where \
                id in (select id from samples where time > ? and time <= ? and cpu in (" + cpuStr + "))");
    q.bindValue(0,value);
    q.bindValue(1,startTime);
    q.bindValue(2,endTime);
    q.exec();
    if(q.lastError().isValid())
    {
        throw(q.lastError().text() + q.lastQuery());
    }
}

void CounterAttributes::updateCounterSamples(unsigned long long eventId, const unsigned long long cpu, const unsigned long long threadId, unsigned long long startTime, unsigned long long endTime, double value, QHash<QString,double> hitRates)
{
    auto eventName = eventIdToName[eventId];
    BufferEntry& b = buffer[eventName];
    b.values << value;
    b.startTimes << startTime;
    b.endTimes << endTime;
    b.cpus << cpu;
    b.threads << threadId;
    b.l1HitRates << hitRates["L1"];
    b.lfbHitRates << hitRates["LFB"];
    b.l2HitRates << hitRates["L2"];
    b.l3HitRates << hitRates["L3"];
    b.localDramHitRates << hitRates["Local DRAM"];
}

void CounterAttributes::writeCounterSamples()
{
    for(auto eventName : buffer.keys() )
    {
    QSqlQuery q(db);
    q.prepare("update counterSamples set '" + eventName + "' = ?, \
              l1HitRate = ?, lfbHitRate = ?, l2HitRate = ?, l3HitRate = ?, localDramHitRate = ? \
              where id in (select id from samples where time > ? and time <= ? and cpu = ? and thread_id = ?)");
    auto& b = buffer[eventName];
    q.addBindValue(b.values);
    q.addBindValue(b.l1HitRates);
    q.addBindValue(b.lfbHitRates);
    q.addBindValue(b.l2HitRates);
    q.addBindValue(b.l3HitRates);
    q.addBindValue(b.localDramHitRates);
    q.addBindValue(b.startTimes);
    q.addBindValue(b.endTimes);
    q.addBindValue(b.cpus);
    q.addBindValue(b.threads);
    if(!q.execBatch())
    {
        throw(q.lastError().text() + q.lastQuery());
    }
    }
    buffer.clear();
}

unsigned long long CounterAttributes::getFirstTimestamp(const unsigned long long cpu, const unsigned long long threadId) const
{
    static QMap<QPair<unsigned long long,unsigned long long>,unsigned long long> cache;
    if(cache.contains({cpu,threadId}))
    {
        return cache.value({cpu,threadId});
    }
    else
    {
    QSqlQuery q;
    q.setForwardOnly(true);
    // Limit the minimum search to the first 20 entires to speed up the query
    // Entires are usually sorted by time but it is not guaranteed
    q.prepare("select min(time) from (select time from samples where time != 0 and cpu = ? and thread_id = ? limit 20)");
    q.bindValue(0,cpu);
    q.bindValue(1,threadId);
    q.exec();
    if(q.next())
    {
        auto val = q.value(0).toULongLong();
        cache.insert({cpu,threadId},val);
        return val;
    }
    else
    {
        throw(q.lastError().text() + q.lastQuery());
    }
    }
}

unsigned long long CounterAttributes::getFirstTimestamp(QList<unsigned int> cpus) const
{
    auto cpuStr = listToString(cpus);
    QSqlQuery q(db);
    q.prepare("select min(time) from (select time from samples where time != 0 and cpu in (" + cpuStr + ") limit 20)" );
    q.exec();
    if(q.next())
    {
        return q.value(0).toULongLong();
    }
    else
    {
        throw(q.lastError().text() + q.lastQuery());
    }
}

QList<QPair<unsigned long long, unsigned long long>> CounterAttributes::getUsedCpuThreadPairs() const
{
    QSqlQuery q("select cpu, thread_id from samples group by thread_id,cpu having count(*) > 10 and count(distinct evsel_id) > ?",db);
    q.bindValue(0,eventIdToName.keys().length());
    q.exec();
    QList<QPair<unsigned long long,unsigned long long>> pairs;
    while (q.next())
    {
        pairs.append({q.value(0).toULongLong(),q.value(1).toULongLong()});
    }
    if(pairs.empty())
    {
        throw (q.lastError().text() + q.lastQuery());
    }
    return pairs;
}

double CounterAttributes::calculateRatePerMs(unsigned long long counterValue, unsigned long long timeInterval)
{
    auto rate = static_cast<double>(counterValue) * pow(10,6) / static_cast<double>(timeInterval);
    return rate;
}

void CounterAttributes::updateIntervalsForCpus(const QList<unsigned int>& cpus, const unsigned long long eventId)
{
    auto cpuStr = listToString(cpus);
    QSqlQuery q("select time,period from samples where evsel_id = ? and cpu in (" + cpuStr + ")");
    q.setForwardOnly(true);
        q.bindValue(0,eventId);
        q.exec();
        unsigned int counter = 0;
        unsigned long long sum = 0;
        unsigned long long timeBegin = getFirstTimestamp(cpus);
        auto time = timeBegin;
        while(q.next())
        {
            auto period = q.value(1).toULongLong();
            time = q.value(0).toULongLong();
            sum+=period;
            counter++;
            if(counter == 1000)
            {
                auto rate = calculateRatePerMs(sum,time-timeBegin);
                updateCounterSamples(eventId,cpus,timeBegin,time,rate);
                counter = 0;
                sum = 0;
                timeBegin = time;
            }
        }
        auto rate = calculateRatePerMs(sum,time-timeBegin);
        updateCounterSamples(eventId,cpus,timeBegin,time,rate);
}

void CounterAttributes::updateIntervalsForCpuAndThread(const unsigned long long cpu, const unsigned long long thread, const unsigned long long eventId)
{
    static QSqlQuery q("select time,period from samples where evsel_id = ? and cpu = ? and thread_id = ?",db);
    q.setForwardOnly(true);
    q.bindValue(1,cpu);
    q.bindValue(2,thread);
        q.bindValue(0,eventId);
        q.exec();
        unsigned int counter = 0;
        unsigned long long sum = 0;
        unsigned long long timeBegin = getFirstTimestamp(cpu,thread);
        auto time = timeBegin;

        while(q.next())
        {
            auto period = q.value(1).toULongLong();
            time = q.value(0).toULongLong();
            sum+=period;
            counter++;
            if(counter == 1000)
            {
                auto rate = calculateRatePerMs(sum,time-timeBegin);
                auto hitRates = getCacheHitRates(cpu,thread,timeBegin,time);
                updateCounterSamples(eventId,cpu,thread,timeBegin,time,rate,hitRates);
                counter = 0;
                sum = 0;
                timeBegin = time;
            }
        }
        auto hitRates = getCacheHitRates(cpu,thread,timeBegin,time);
        auto rate = calculateRatePerMs(sum,time-timeBegin);
        updateCounterSamples(eventId,cpu,thread,timeBegin,time,rate,hitRates);
}

QHash<QString,double> CounterAttributes::getCacheHitRates(const unsigned long long cpu, const unsigned long long thread, const unsigned long long timeBegin,const unsigned long long timeEnd) const
{
    QString sAllAccess(R"(select count(*) from samples where evsel_id =
    (select id from selected_events where name like '%mem-loads%') and cpu = ? and thread_id = ? and time > ? and time <= ? and memory_level != 1 )");
    QString sCacheAccess(sAllAccess + " and memory_level = (select id from memory_levels where name = ? )");
    QSqlQuery qAllAcces(sAllAccess);
    QSqlQuery qCacheAccess(sCacheAccess);
    QSqlQuery qL2OrL3ByLfb(sAllAccess + "and memory_level = 2 and weight <= 275"); // (select min(weight) * 1.1 from samples where memory_level = 16)");
    QSqlQuery qLocalDramByLfb(sAllAccess + "and memory_level = 2 and weight > 275"); // (select min(weight) * 1.1 from samples where memory_level = 16)");
    static QVector<QString> memories = {"LFB", "L2", "L3", "Local DRAM"};
    qAllAcces.bindValue(0,cpu);
    qAllAcces.bindValue(1,thread);
    qAllAcces.bindValue(2,timeBegin);
    qAllAcces.bindValue(3,timeEnd);
    qAllAcces.exec();
    double numAccess = 0.0;
    QHash<QString,double> result;
    for(auto s : memories)
    {
        result.insert(s,0.0);
    }
    if(qAllAcces.next())
    {
        numAccess = qAllAcces.value(0).toDouble();
        if(numAccess == 0.0)
        {
            return result;
        }
        else
        {
            for(auto s : memories)
            {
                qCacheAccess.bindValue(0,cpu);
                qCacheAccess.bindValue(1,thread);
                qCacheAccess.bindValue(2,timeBegin);
                qCacheAccess.bindValue(3,timeEnd);
                qCacheAccess.bindValue(4,s);
                qCacheAccess.exec();
                if(qCacheAccess.next())
                {
                    auto v = qCacheAccess.value(0).toDouble();
                    if(s == "L2")
                    {
                        qL2OrL3ByLfb.bindValue(0,cpu);
                        qL2OrL3ByLfb.bindValue(1,thread);
                        qL2OrL3ByLfb.bindValue(2,timeBegin);
                        qL2OrL3ByLfb.bindValue(3,timeEnd);
                        qL2OrL3ByLfb.exec();
                        if(qL2OrL3ByLfb.next())
                        {
                            v += qL2OrL3ByLfb.value(0).toDouble();
                        }
                        else
                        {
                            throw(qL2OrL3ByLfb.lastError().text() + qL2OrL3ByLfb.lastQuery());
                        }

                    }
                    else if(s == "Local DRAM")
                    {
                        qLocalDramByLfb.bindValue(0,cpu);
                        qLocalDramByLfb.bindValue(1,thread);
                        qLocalDramByLfb.bindValue(2,timeBegin);
                        qLocalDramByLfb.bindValue(3,timeEnd);
                        qLocalDramByLfb.exec();
                        if(qLocalDramByLfb.next())
                        {
                            v += qLocalDramByLfb.value(0).toDouble();
                        }
                        else
                        {
                            throw(qLocalDramByLfb.lastError().text() + qLocalDramByLfb.lastQuery());
                        }
                    }
                    v = v / numAccess;
                    result[s] = v;
                }
                else
                {
                   throw(qCacheAccess.lastError().text() + qCacheAccess.lastQuery());
                }
            }
        }
    }
    else
    {
        throw(qAllAcces.lastError().text() + qAllAcces.lastQuery());
    }
    return result;
}

void CounterAttributes::updateIntervals(const EventList& consideredEvents)
{
    this->eventList = consideredEvents;
    try {
        createTable(consideredEvents);
        createEventIdMap(consideredEvents);
        createIndexes();
        writeSampleIds();
        auto cpuThreadPairs = getUsedCpuThreadPairs();
        for(auto event : eventNameToId.keys())
        {
            auto eventId = eventNameToId.value(event);
            auto eAttr = getEventAttributes(event);
            if(eAttr.offcore == true)
            {
                for(auto node : nodeToCpus.keys())
                {
                    auto cpus = nodeToCpus[node];
                    updateIntervalsForCpus(cpus,eventId);
                }
            }
            else
            {
                for(auto p : cpuThreadPairs)
                {
                        updateIntervalsForCpuAndThread(p.first,p.second,eventId);
                }
            }
         }

    db.transaction();
    writeCounterSamples();
    db.commit();
    createMetricViews(eventList);
    }
    catch (QString& s)
    {
        std::cout << "Error in CounterAttributes: " << s.toStdString();
    }
}

bool CounterAttributes::EventAttribute::operator !=(const CounterAttributes::EventAttribute &other) const
{
    return !(*this == other);
}

bool CounterAttributes::EventAttribute::operator==(const CounterAttributes::EventAttribute &other) const
{
   return this->offcore == other.offcore;
}

#ifndef COUNTERATTRIBUTES_H
#define COUNTERATTRIBUTES_H

#include <QtSql>
#include <QHash>
#include <QVector>

class CounterAttributes
{
public:

    typedef QList<QString> EventList;

    CounterAttributes(QSqlDatabase& db);
    void setNodeMapping(const QHash<unsigned int, QList<unsigned int>>& cpus);
    void updateIntervals(const EventList &consideredEvents);


private:
    struct EventAttribute
    {
        bool offcore = false;
        bool operator!=(const EventAttribute& other) const;
        bool operator==(const EventAttribute& other) const;
    };

    struct BufferEntry
    {
    QVariantList values;
    QVariantList startTimes;
    QVariantList endTimes;
    QVariantList cpus;
    QVariantList threads;
    QVariantList l1HitRates;
    QVariantList lfbHitRates;
    QVariantList l2HitRates;
    QVariantList l3HitRates;
    QVariantList localDramHitRates;
    };

    QSqlDatabase db;
    QHash<unsigned long long,QString> eventIdToName;
    QHash<QString,unsigned long long> eventNameToId;
    EventList eventList;
    QHash<unsigned int, unsigned int> cpuToNode;
    QHash<unsigned int, QList<unsigned int>> nodeToCpus;
    QHash<QString,BufferEntry> buffer;

    void createEventIdMap(const EventList& consideredEvents);
    void createTable(EventList events);
    QHash<unsigned long long,unsigned long long> calcTimeSumInterval(unsigned long long startTime, QList<unsigned long long> consdiredEvents, const unsigned long long cpu, const unsigned long long threadId, unsigned long long& maxEndTime);
    bool containsAllKeys(QHash<unsigned long long,unsigned long long> map, QList<unsigned long long> keys);
    void writeSampleIds();
    void updateCounterSamples(unsigned long long eventId, const unsigned long long cpu, const unsigned long long threadId, unsigned long long startTime, unsigned long long endTime, double value, QHash<QString,double> hitRates);
    unsigned long long getRawCounterValue(unsigned long long eventId, const unsigned long long cpu, const unsigned long long threadId, unsigned long long startTime, unsigned long long endTime);
    template <class T>
    QString listToString(const QList<T> &list) const;
    bool isLast(const unsigned long long startTime, const QList<unsigned long long> &consideredEvents) const;
    unsigned long long getFirstTimestamp(const unsigned long long cpu, const unsigned long long threadId) const;
    unsigned long long getFirstTimestamp(QList<unsigned int> cpus) const;
    unsigned long long adjustCounterValue(unsigned long long raw, unsigned long long activeTime, unsigned long long totalTime) const;
    void createIndexes();
    bool allAtLeast(QHash<unsigned long long, unsigned long> map, unsigned long value);
    QList<unsigned long long> getUsedCpus() const;
    QList<unsigned long long> getUsedThreadIds() const;
    unsigned long long nextIntervalBegin(const unsigned long long cpu, const unsigned long long threadId, const QList<unsigned long long> &eventIds, unsigned long long endTime) const;
    double calculateRatePerMs(unsigned long long counterValue, unsigned long long timeInterval);
    QStringList createFlatEventList(const EventList &eventList) const;
    EventAttribute getEventAttributes(const QString& event) const;
    EventAttribute getEventAttributes(const unsigned long long event) const;


    CounterAttributes::EventAttribute getEventAttributes(const QList<unsigned long long> &eventList) const;
    CounterAttributes::EventAttribute getEventAttributes(const QList<QPair<QString, bool> > &eventList) const;
    QHash<unsigned long long, unsigned long long> calcTimeSumInterval(unsigned long long startTime, QSqlQuery &q, QList<unsigned long long> consideredEvents, unsigned long long &maxEndTime);
    unsigned long long nextIntervalBegin(const QList<unsigned int> &cpu, const QList<unsigned long long> &eventIds, unsigned long long endTime) const;
    QHash<unsigned long long, unsigned long long> calcTimeSumInterval(unsigned long long startTime, QList<unsigned long long> consideredEvents, const QList<unsigned int> &cpu, unsigned long long &maxEndTime);
    QHash<unsigned long long,unsigned long long> calcTimeSumInterval(unsigned long long startTime,  QList<unsigned long long> consideredEvents, QSqlQuery& orderedEventsQuery, unsigned long long& maxEndTime);
    unsigned long long getRawCounterValue(unsigned long long eventId, const QList<unsigned int> &cpu, unsigned long long startTime, unsigned long long endTime);
    void updateCounterSamples(unsigned long long eventId, const QList<unsigned int> &cpu, unsigned long long startTime, unsigned long long endTime, double value);
    void updateIntervalsForCpus(const QList<unsigned int> &cpus, const unsigned long long eventId);
    void updateIntervalsForCpuAndThread(const unsigned long long cpu, const unsigned long long thread, const unsigned long long eventId);
    QList<unsigned long long> getEventIds() const;
    QList<QPair<unsigned long long, unsigned long long> > getUsedCpuThreadPairs() const;
    void writeCounterSamples();
    void createMetricViews(const EventList &list);
    QHash<QString, double> getCacheHitRates(const unsigned long long cpu, const unsigned long long thread, const unsigned long long timeBegin, const unsigned long long timeEnd) const;
};

#endif // COUNTERATTRIBUTES_H
